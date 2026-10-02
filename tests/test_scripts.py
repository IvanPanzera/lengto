"""Integration tests: native C engine, recorded Python and GUI hosting. Stdlib only."""
import importlib.util
import json
import math
import os
from pathlib import Path
import struct
import subprocess
import sys
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
EXE = Path(os.environ.get("LENGTO_TEST_EXE", ROOT / "portable/lengto-single/lengto.exe"))
FIXTURES = ROOT / "tests/fixtures"


def load_module(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


api = load_module(ROOT / "src/python_api.py", "lengto_api_test")


def png(path, width, height, color, channels=3):
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    raw = b"".join(b"\0" + b"".join(bytes(color(x, y)) for x in range(width)) for y in range(height))
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6 if channels == 4 else 2, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))


class Scripts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        BUILD.mkdir(exist_ok=True)
        cls.small = BUILD / "script colors é Ω.png"
        cls.large = BUILD / "script colors double.png"
        cls.alpha = BUILD / "script alpha.png"
        cls.wide = BUILD / "script wide.png"
        color = lambda x, y: (10, 20, 30) if (x, y) in ((1, 1), (5, 1), (3, 4)) else (11, 22, 33) if (x, y) == (6, 3) else (255, 255, 255)
        png(cls.small, 8, 6, color)
        png(cls.large, 16, 12, lambda x, y: color(x // 2, y // 2))
        png(cls.alpha, 2, 1, lambda x, y: (255, 0, 0, 128) if x else (3, 4, 5, 0), channels=4)
        png(cls.wide, 70003, 2, lambda x, y: (0, 5, 10) if x == 345 else (255, 255, 255))

    def setUp(self):
        self.app = api.Lengto(EXE)

    def tearDown(self):
        self.app.shutdown()
        self.assertEqual(self.app._process.returncode, 0)

    def test_coordinates_and_scale(self):
        app = self.app
        self.assertFalse(app.info()["loaded"])
        app.settings("mm", -10)
        app.open(self.small)
        a, b = app.pixel(1, 1), app.pixel(5, 1)
        self.assertEqual(a.absolute, (1, 1))
        self.assertEqual(a.relative, (api.Fraction(1, 8), api.Fraction(1, 6)))
        self.assertEqual(a.rgb, (10, 20, 30))
        app.calibrate(a, b, 2, "m")
        self.assertEqual(app.measure(a, b, 'Dimension "é" Ω').value, 2000)
        app.open(self.large)
        self.assertEqual(app._resolve(a), (2, 2))
        self.assertEqual(app._resolve(a, "absolute"), (1, 1))
        app.calibrate(a, b, 2)
        self.assertEqual(app.measure(a, b).metres, 2)
        self.assertEqual(app.measure(a, b, coordinates="absolute").metres, 1)
        app.calibrate(a, b, 4)
        self.assertTrue(app.info()["calibrated"])
        self.assertEqual(app.measure(a, b).metres, 4)

    def test_source_pixels_and_alpha(self):
        app = self.app
        app.open(self.small)
        before = app.pixel(1, 1)
        app.calibrate((0, 1), (7, 1), 7)
        app.measure((0, 1), (7, 1))
        self.assertEqual(app.pixel(1, 1), before)
        app.open(self.alpha)
        self.assertEqual(app.pixel(0, 0).rgb, (255, 255, 255))
        self.assertEqual(app.pixel(1, 0).rgb, (255, 127, 127))
        app.open(self.wide)
        self.assertEqual(app.info()["width"], 70003)
        point = app.pixel(345, 1)
        self.assertEqual(point.rgb, (0, 5, 10))
        self.assertEqual(point.relative[0], api.Fraction(345, 70003))
        self.assertEqual(app._resolve(point), (345, 1))
        self.assertEqual(app.pixel(344, 1).rgb, (255, 255, 255))

    def test_color_loops(self):
        app = self.app
        app.open(self.small)
        app.calibrate((1, 1), (5, 1), 4)
        points = list(app.find_pixels(rgb=(10, 20, 30)))
        self.assertEqual([p.absolute for p in points], [(1, 1), (5, 1), (3, 4)])
        for point in points:
            app.measure((0, 0), point)
        self.assertEqual(app.info()["measurements"], 3)
        self.assertEqual(len(list(app.find_pixels(rgb=(10, 20, 30), tolerance=3))), 4)
        self.assertEqual(len(list(app.find_pixels(predicate=lambda r, g, b: r < 20 and b > 25))), 4)
        self.assertEqual([p.absolute for p in app.find_pixels(rgb=(10, 20, 30), region=(0., 0., 8., 3.))], [(1, 1), (5, 1)])
        self.assertEqual(len(list(app.find_pixels(rgb=(255, 255, 255), step=2))), 12)
        self.assertEqual(len(list(app.find_pixels(rgb=(255, 255, 255), limit=2))), 2)
        self.assertEqual(list(app.find_pixels(rgb=(0, 0, 0))), [])
        generator = app.find_pixels(rgb=(255, 255, 255))
        next(generator)
        app.open(self.large)
        with self.assertRaisesRegex(RuntimeError, "page"):
            next(generator)

    def test_multi_page_colors(self):
        for filename in ("drawing-multipage.tif", "drawing.pdf"):
            self.app.open(FIXTURES / filename)
            self.assertEqual(self.app.info()["pages"], 2)
            self.app.calibrate((100, 130), (600, 130), 5)
            self.assertEqual(self.app.pixel(1000, 500).rgb, (255, 255, 255))
            self.app.page(2)
            self.assertEqual(self.app.info()["width"], 640)
            self.assertEqual(self.app.pixel(0, 0).rgb, (30, 120, 200))
            self.assertEqual(self.app.measure((10, 10), (110, 10)).metres, 1)
            target = BUILD / (filename + ".script.pdf")
            self.app.export(target, all_pages=True)
            self.assertEqual(self.app.info()["page"], 2)
            self.app.open(target)
            self.assertEqual(self.app.info()["pages"], 2)

    def test_edit_delete_units_and_errors(self):
        app = self.app
        app.open(self.small)
        with self.assertRaisesRegex(RuntimeError, "calibration"):
            app.measure((0, 0), (2, 0))
        app.calibrate((0, 0), (4, 0), 4)
        m = app.measure((0, 0), (2, 0))
        m = app.edit_measurement(m, (0, 0), (3, 0), name="Renamed")
        self.assertEqual(m.metres, 3)
        for unit in ("Å", "nm", "µm", "mm", "cm", "dm", "m", "dam", "hm", "km", "Mm", "Gm", "AU", "Tm", "Pm", "ly"):
            app.settings(unit, -10)
            self.assertTrue(math.isfinite(app.measure((0, 0), (2, 0)).value))
        # Older self-contained macros retain the previous protocol spelling.
        app.settings("UA", -10)
        self.assertEqual(app.measure((0, 0), (2, 0)).unit, "AU")
        app.settings("m", 16)
        self.assertEqual(app.measure((0, 0), (2, 0)).rounded, 0)
        app.delete_measurement(m)
        with self.assertRaises(RuntimeError):
            app.delete_measurement(m)
        for point in ((-1, 0), (8, 0), (0, 6), (0.5, 0)):
            with self.assertRaises(ValueError):
                app.pixel(*point)
        for operation in (lambda: app.calibrate((0, 0), (0, 0), 1), lambda: app.calibrate((0, 0), (1, 0), -1), lambda: app.settings("m", 17), lambda: app.page(2)):
            with self.assertRaises(RuntimeError):
                operation()
        before = self.small.read_bytes()
        with self.assertRaisesRegex(RuntimeError, "original"):
            app.export(self.small)
        self.assertEqual(self.small.read_bytes(), before)
        with self.assertRaises(RuntimeError):
            app.open(BUILD / "missing-input.tif")
        self.assertEqual(app.info()["width"], 8)

    def test_protocol_validation(self):
        process = self.app._process
        requests = ['{}', '{', '{"op":"info",}', '{"op":"info","op":"close"}', '{"op":"info"} trailing', '{"op":"info","n":01}', '{"op":"info","n":1e999}', '{"op":"\\ud800"}', '{"op":"\\u0000"}', '{"op":[1]}', '{"op":"unterminated']
        for request in requests:
            process.stdin.write((request + "\n").encode())
            process.stdin.flush()
            self.assertFalse(json.loads(process.stdout.readline())["ok"], request)
        self.assertFalse(self.app.info()["loaded"])
        self.app.open(self.small)
        with self.assertRaises(RuntimeError):
            self.app._rpc("pixels", x=7, y=5, width=2, height=1)

    def test_memory_image_export_and_failure(self):
        app = self.app
        app.open_image(self.small.read_bytes())
        self.assertEqual(app.info()["width"], 8)
        self.assertEqual(app.pixel(1, 1).rgb, (10, 20, 30))
        app.calibrate((1, 1), (5, 1), 4)
        self.assertEqual(app.measure((1, 1), (5, 1)).metres, 4)
        with self.assertRaises(RuntimeError):
            app.open_image(b"not an image")
        self.assertEqual(app.info()["measurements"], 1)
        self.assertEqual(app.pixel(1, 1).rgb, (10, 20, 30))
        app.export(BUILD / "clipboard-export.pdf")
        app.close()
        app.open_image(self.alpha.read_bytes())
        self.assertEqual(app.pixel(0, 0).rgb, (255, 255, 255))
        self.assertEqual(app.pixel(1, 0).rgb, (255, 127, 127))

    def test_gui_recording_hosting_and_cancel(self):
        recorded = BUILD / "test recorded é Ω.py"
        result = subprocess.run([str(EXE), "--script-test", str(FIXTURES / "drawing-multipage.tif"), str(recorded), sys.executable], capture_output=True, timeout=45)
        self.assertEqual(result.returncode, 0, result.stderr.decode(errors="replace"))
        text = recorded.read_text(encoding="utf-8")
        self.assertIn("Fraction(100, 1200)", text)
        self.assertIn("rgb=(", text)
        self.assertIn("app.delete_measurement(m_1)", text)
        self.assertIn("app.edit_measurement(m_2", text)
        self.assertIn('COORDINATES = "relative"', text)
        # The .py contains its own API: run it from a different directory.
        output = BUILD / "test replay standalone.pdf"
        result = subprocess.run([sys.executable, str(recorded), "--lengto", str(EXE), "--output", str(output)], cwd=BUILD, capture_output=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stderr.decode(errors="replace"))
        self.app.open(output)
        self.assertEqual(self.app.info()["pages"], 2)
        snapshot = Path(str(recorded) + ".snapshot.py")
        module = load_module(snapshot, "snapshot_api_test")
        self.assertEqual(module.COORDINATES, "absolute")
        with module.Lengto(EXE, module.COORDINATES) as app:
            module.run(app)
            self.assertEqual(app.info()["page"], 2)
            self.assertEqual(app.info()["measurements"], 2)
        pasted = Path(str(recorded) + ".clipboard.py")
        self.assertIn("app.open_image(base64.b64decode(", pasted.read_text(encoding="utf-8"))
        self.assertNotIn("drawing-multipage.tif", pasted.read_text(encoding="utf-8"))
        module = load_module(pasted, "clipboard_macro_test")
        with module.Lengto(EXE, module.COORDINATES) as app:
            module.run(app, output_file=str(BUILD / "clipboard-replay.pdf"))
            self.assertEqual(app.info()["measurements"], 1)
            self.assertEqual(app.info()["width"], 1200)
            module.run(app, input_file=str(FIXTURES / "drawing.png"), output_file=str(BUILD / "clipboard-override.pdf"))
            self.assertEqual(app.info()["measurements"], 1)

    def test_recorded_color_batch_customization(self):
        # Use exactly the embedded API emitted by the native recorder.
        recorded = BUILD / "test recorded é Ω.py"
        if not recorded.exists():
            self.test_gui_recording_hosting_and_cancel()
        module = load_module(recorded, "color_macro_test")
        values = []
        for source in (self.small, self.large):
            with module.Lengto(EXE) as app:
                app.open(source)
                a = module.Point((1, 1), (module.Fraction(1, 8), module.Fraction(1, 6)), (10, 20, 30))
                b = module.Point((5, 1), (module.Fraction(5, 8), module.Fraction(1, 6)), (10, 20, 30))
                app.calibrate(a, b, 4)
                values.append(app.measure(a, b).metres)
                for point in app.find_pixels(rgb=(10, 20, 30), limit=3):
                    app.measure((0, 0), point, name="From color")
                app.export(BUILD / (source.stem + "_batch.pdf"))
                self.assertEqual(app.info()["measurements"], 4)
        self.assertEqual(values, [4, 4])


if __name__ == "__main__":
    unittest.main(verbosity=2)
