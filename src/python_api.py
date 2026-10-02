# Embedded lengto support. Requires only Python 3.8+.
import argparse
import base64
import json
import os
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from fractions import Fraction
from pathlib import Path


@dataclass(frozen=True)
class Point:
    absolute: tuple
    relative: tuple = ()
    rgb: tuple = ()


@dataclass(frozen=True)
class Measurement:
    id: int
    metres: float
    value: float
    rounded: float
    unit: str


class Lengto:
    def __init__(self, executable=None, coordinates="relative"):
        if coordinates not in ("relative", "absolute"):
            raise ValueError("Coordinates: choose 'relative' or 'absolute'.")
        self.coordinates = coordinates
        self._generation = 0
        self._info = None
        self._process = None
        self._old_stdout = None
        self._closed = False
        if os.environ.get("LENGTO_HOSTED") == "1":
            self._input = sys.stdin.buffer
            self._output = sys.stdout.buffer
            self._old_stdout = sys.stdout
            sys.stdout = sys.stderr  # Keep print() output separate from commands.
        else:
            exe = executable or os.environ.get("LENGTO_EXE")
            if not exe:
                beside = Path(__file__).resolve().with_name("lengto.exe")
                exe = str(beside) if beside.is_file() else shutil.which("lengto.exe")
            if not exe:
                raise FileNotFoundError("Specify lengto.exe with --lengto or place it beside the macro.")
            self._process = subprocess.Popen(
                [str(exe), "--automation"], stdin=subprocess.PIPE,
                stdout=subprocess.PIPE, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0)
            )
            self._input = self._process.stdout
            self._output = self._process.stdin

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.shutdown()

    def shutdown(self):
        if self._closed:
            return
        self._closed = True
        if self._old_stdout is not None:
            sys.stdout = self._old_stdout
        if self._process:
            self._output.close()
            try:
                self._process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                self._process.kill()
                self._process.wait()
            self._input.close()

    def _rpc(self, op, **params):
        if self._closed:
            raise RuntimeError("Session ended.")
        request = json.dumps(dict(op=op, **params), ensure_ascii=False, allow_nan=False)
        self._output.write((request + "\n").encode("utf-8"))
        self._output.flush()
        line = self._input.readline()
        if not line:
            raise RuntimeError("Lengto closed the connection.")
        response = json.loads(line.decode("utf-8"))
        if not response.get("ok"):
            raise RuntimeError(response.get("error", "Command failed."))
        return response.get("result")

    def info(self):
        self._info = self._rpc("info")
        return self._info.copy()

    def open(self, file):
        self._rpc("open", path=str(Path(file).resolve()))
        self._generation += 1
        return self.info()

    def close(self):
        self._rpc("close")
        self._generation += 1
        self._info = None

    def open_image(self, png_bytes):
        """Open PNG image bytes without requiring a persistent source file."""
        with tempfile.TemporaryDirectory(prefix="lengto-image-") as folder:
            path = Path(folder) / "image.png"
            path.write_bytes(png_bytes)
            # The engine retains its own memory copy before the file is removed.
            self._rpc("open_image", path=str(path))
        self._generation += 1
        return self.info()

    def page(self, number):
        self._rpc("page", page=number)
        self._generation += 1
        return self.info()

    def settings(self, unit="m", precision=-3):
        self._rpc("settings", unit=unit, precision=precision)

    def _dimensions(self):
        info = self._info or self.info()
        if not info["loaded"]:
            raise RuntimeError("Open a document first.")
        return info["width"], info["height"]

    def _resolve(self, point, coordinates=None):
        width, height = self._dimensions()
        mode = coordinates or self.coordinates
        if mode not in ("relative", "absolute"):
            raise ValueError("Invalid coordinates.")
        if isinstance(point, Point):
            if mode == "relative" and point.relative:
                rx, ry = map(Fraction, point.relative)
                if not (0 <= rx < 1 and 0 <= ry < 1):
                    raise ValueError("Relative coordinates must be at least 0 and less than 1.")
                # Exact fractions avoid one-pixel rounding errors during replay.
                x, y = (rx * width).__floor__(), (ry * height).__floor__()
            else:
                x, y = point.absolute
        else:
            x, y = point  # A plain pair always uses absolute pixel indices.
        if int(x) != x or int(y) != y or not (0 <= x < width and 0 <= y < height):
            raise ValueError("Pixel outside the image or non-integer index.")
        return int(x), int(y)

    def calibrate(self, start, end, value, unit="m", coordinates=None):
        ax, ay = self._resolve(start, coordinates)
        bx, by = self._resolve(end, coordinates)
        self._rpc("calibrate", ax=ax, ay=ay, bx=bx, by=by, value=value, unit=unit)

    @staticmethod
    def _measurement(result):
        return Measurement(result["id"], result["metres"], result["value"], result["rounded"], result["unit"])

    def measure(self, start, end, name="Measurement", coordinates=None):
        ax, ay = self._resolve(start, coordinates)
        bx, by = self._resolve(end, coordinates)
        return self._measurement(self._rpc("measure", ax=ax, ay=ay, bx=bx, by=by, name=name))

    def edit_measurement(self, measure, start, end, name="Measurement", coordinates=None):
        ax, ay = self._resolve(start, coordinates)
        bx, by = self._resolve(end, coordinates)
        return self._measurement(self._rpc("edit_measure", id=getattr(measure, "id", measure), ax=ax, ay=ay, bx=bx, by=by, name=name))

    def delete_measurement(self, measure):
        self._rpc("delete", id=getattr(measure, "id", measure))

    def export(self, file, all_pages=False):
        path = Path(file).resolve()
        formats = {".tif": 0, ".tiff": 0, ".jpg": 1, ".jpeg": 1, ".png": 2, ".pdf": 3}
        try:
            fmt = formats[path.suffix.lower()]
        except KeyError:
            raise ValueError("Output extension: TIFF, JPEG, PNG or PDF.") from None
        self._rpc("export", path=str(path), format=fmt, all_pages=int(bool(all_pages)))

    def pixel(self, x, y):
        x, y = self._resolve((x, y))
        width, height = self._dimensions()
        rgb = tuple(bytes.fromhex(self._rpc("pixels", x=x, y=y, width=1, height=1)["rgb"]))
        return Point((x, y), (Fraction(x, width), Fraction(y, height)), rgb)

    def find_pixels(self, rgb=None, tolerance=0, region=None, step=1, predicate=None, limit=None):
        """Yield points from the source raster; region=(left, top, right, bottom).
        Right and bottom are excluded. RGB is 0..255; tolerance is per channel.
        predicate receives (r, g, b). Annotations do not affect source colors.
        """
        width, height = self._dimensions()
        left, top, right, bottom = region or (0, 0, width, height)
        if any(int(v) != v for v in (left, top, right, bottom, step)) or not (0 <= left < right <= width and 0 <= top < bottom <= height) or step < 1:
            raise ValueError("Invalid search region or step.")
        if not 0 <= tolerance <= 255 or (limit is not None and (limit < 1 or int(limit) != limit)):
            raise ValueError("Invalid tolerance or limit.")
        if rgb is not None and (len(rgb) != 3 or any(int(v) != v or not 0 <= v <= 255 for v in rgb)):
            raise ValueError("RGB must contain three integers between 0 and 255.")
        if rgb is None and predicate is None:
            raise ValueError("Specify rgb or predicate.")
        left, top, right, bottom, step = map(int, (left, top, right, bottom, step))
        generation, found = self._generation, 0
        for y0 in range(top, bottom, 16):
            h = min(16, bottom - y0)
            for x0 in range(left, right, 4096):
                if generation != self._generation:
                    raise RuntimeError("The page changed during the search.")
                w = min(4096, right - x0)
                data = bytes.fromhex(self._rpc("pixels", x=x0, y=y0, width=w, height=h)["rgb"])
                for y in range(y0, y0 + h):
                    if (y - top) % step:
                        continue
                    for x in range(x0, x0 + w):
                        if (x - left) % step:
                            continue
                        i = ((y - y0) * w + x - x0) * 3
                        color = tuple(data[i:i + 3])
                        if rgb is not None and any(abs(a - b) > tolerance for a, b in zip(color, rgb)):
                            continue
                        if predicate is not None and not predicate(*color):
                            continue
                        yield Point((x, y), (Fraction(x, width), Fraction(y, height)), color)
                        if generation != self._generation:
                            raise RuntimeError("The page changed during the search.")
                        found += 1
                        if limit is not None and found >= limit:
                            return


def _output_path(override, input_file, original):
    if override:
        return override
    if input_file:
        source = Path(input_file)
        return source.with_name(source.stem + "_measured" + Path(original).suffix)
    return original


def _main(function):
    parser = argparse.ArgumentParser(description="lengto macro: customize run() to change its behavior.")
    parser.add_argument("--lengto", help="Path to lengto.exe")
    parser.add_argument("--input", help="Override the first recorded input document")
    parser.add_argument("--output", help="Override the first recorded output file")
    parser.add_argument("--coordinates", choices=("relative", "absolute"), default=globals().get("COORDINATES", "relative"))
    args = parser.parse_args()
    with Lengto(args.lengto, args.coordinates) as app:
        function(app, args.input, args.output)
