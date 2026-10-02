# Python workflows for lengto

These examples use the API embedded in a macro recorded by lengto 0.6. The API, comments and examples are in English. Python 3.8 or later is required, with no additional packages for these examples.

## Record a reusable starting point

1. Choose **Script → Start recording**. A document does not need to be open yet.
2. Select relative or absolute coordinates.
3. Open a document, calibrate it, add measurements and export the result.
4. Choose **Script → Stop recording** and save the file as `macro.py`.

The file starts with `COORDINATES` and a function named `run()`. Edit that function and leave the embedded support code below it in place. Opened documents are referenced on disk. Images pasted from the clipboard are embedded as lossless PNG data and replay through `app.open_image(png_bytes)`, so the clipboard is not needed later. The first pasted image also accepts the normal `--input` override.

## Define endpoints explicitly

Inside `run()`, after opening the document:

```python
# Two endpoints in absolute, zero-based pixel coordinates.
app.calibrate((100, 130), (600, 130), value=5, unit="m")
app.settings(unit="mm", precision=-3)

measurement = app.measure((100, 180), (100, 420), name="Depth")
print(measurement.metres)  # 2.4 metres on this example's scale
print(measurement.value)  # 2400 millimetres

app.export(output_file or "measured.pdf")
```

These coordinates match the [synthetic engineering example](images/engineering-example.png). A plain `(x, y)` pair always means absolute pixel indices, even if the session uses relative coordinates.

A recorded point also includes relative fractions and color:

```python
point = Point(
    absolute=(345, 120),
    relative=(Fraction(345, 65634), Fraction(120, 40000)),
    rgb=(28, 31, 35),
)
```

This describes a pixel in a 65,634 × 40,000 image. Fractions use `index / dimension`, with the origin at the top left. During relative replay, lengto selects the pixel at `floor(fraction × current_dimension)` and measures between pixel centers. Use `COORDINATES = "absolute"` for absolute replay, or `"relative"` for relative replay. The same choices are available through `--coordinates` on the command line.

## Measure between dark edges on selected rows

Add this after your recorded open and calibration commands:

```python
width = app.info()["width"]

for y in (200, 300, 400):
    points = list(app.find_pixels(
        predicate=lambda r, g, b: max(r, g, b) < 100,
        region=(0, y, width, y + 1),
    ))

    if len(points) >= 2:
        left = min(points, key=lambda p: p.absolute[0])
        right = max(points, key=lambda p: p.absolute[0])
        measurement = app.measure(left, right, name=f"Row {y}")
        print(y, measurement.metres)

app.export(output_file or "edge-measurements.pdf")
```

Adapt the row positions, search region and threshold to the drawing. This measures between the leftmost and rightmost dark pixels on each selected row. Text and other lines in that region can become endpoints too.

`region=(left, top, right, bottom)` uses absolute pixels; right and bottom are excluded. `step=2` samples every second pixel along each axis. `limit=100` stops after 100 matches; omitting it returns all matching pixels. Keep the same document and page open while iterating over a search.

RGB searches use the maximum allowed difference in each channel:

```python
points = app.find_pixels(rgb=(220, 40, 40), tolerance=15)
```

Color sampling excludes annotations drawn by lengto. Matching pixels are not automatically grouped into separate objects. There is a limit of 4,096 drawn measurements per document, even though a script can inspect more pixels.

## Run the same recording on a folder

Save this as `batch.py` beside your recorded `macro.py`:

```python
from pathlib import Path
from macro import Lengto, COORDINATES, run

source_dir = Path(r"C:\drawings")
output_dir = source_dir / "measured"
output_dir.mkdir(exist_ok=True)

with Lengto(r"C:\tools\lengto.exe", COORDINATES) as app:
    for source in sorted(source_dir.glob("*.tif")):
        output = output_dir / f"{source.stem}.pdf"
        run(app, input_file=str(source), output_file=str(output))
```

The recording must include an open and an export operation. Its first open and first export accept the overrides; later recorded paths remain as written unless you parameterize them too. Each new document starts without measurements. Relative replay works for corresponding images with the same framing and proportions, not arbitrary layouts.

Run a single file from the terminal:

```powershell
python macro.py --lengto "C:\tools\lengto.exe" --input "drawing.tif" --output "measured.pdf"
```

If `lengto.exe` is beside the macro, `--lengto` can be omitted. `LENGTO_EXE` or a PATH entry can also identify the executable. These commands run lengto without opening its window.

## Collect numeric results

Python's standard `csv` module can save measurements while the macro runs:

```python
import csv

rows = []
for y in (200, 300, 400):
    measurement = app.measure((100, y), (600, y), name=f"Span at {y}")
    rows.append((y, measurement.metres))

with open("measurements.csv", "w", newline="", encoding="utf-8") as file:
    writer = csv.writer(file)
    writer.writerow(("row_px", "length_m"))
    writer.writerows(rows)
```

Run this after opening and calibrating an image containing those coordinates. CSV output is written by your script; it is not an additional format in lengto's Export menu.

`measurement.metres` is the unrounded distance in metres. `value` is the unrounded distance in the current display unit; `rounded` is its rounded counterpart. Returned Python values describe the measurement when the call completed and do not update if you later change calibration.
