# Python examples

[Quick guide](USER_GUIDE.md) · Python 3.8+ · No extra packages

Record a workflow with **Script → Start recording**, including an open, calibration and export. Save it as `macro.py`. It contains an editable `run()` function and its own support code.

## Run on another image

Keep `lengto.exe` beside the macro:

```powershell
python macro.py --input "drawing.tif" --output "measured.pdf"
```

Use `--lengto "C:\tools\lengto.exe"` if the EXE is elsewhere. Input/output options replace the **first** recorded open/export. They do not add an export operation.

## Process a folder

Save `batch.py` beside the macro:

```python
from pathlib import Path
from macro import Lengto, COORDINATES, run

with Lengto(r"C:\tools\lengto.exe", COORDINATES) as app:
    for source in sorted(Path(r"C:\drawings").glob("*.tif")):
        output = source.with_name(source.stem + "_measured.pdf")
        run(app, input_file=str(source), output_file=str(output))
```

Absolute mode repeats pixel positions. Relative mode adapts to resized images with the same framing; it does not align different layouts. Opened files are referenced by path; pasted images are embedded in the macro.

## Customize measurements

Inside `run()`, after opening the [sample drawing](images/engineering-example.png):

```python
app.calibrate((100, 130), (600, 130), value=5, unit="m")
app.settings(unit="mm", precision=-3)  # Round labels to 1 mm.
m = app.measure((100, 180), (100, 420), name="Depth")
print(m.metres)  # 2.4
app.export(output_file or "measured.pdf")
```

Tuples use absolute, zero-based pixel indices. `Point` also stores relative fractions and source RGB. `m.metres` is unrounded; returned values do not update after recalibration.

For color-based workflows, `app.find_pixels(rgb=(220, 40, 40), tolerance=15)` yields matching source pixels. It does not group objects or relocate recorded endpoints automatically. Keep the document/page unchanged during a search.

See [the Python API](../src/python_api.py) for `info`, `page`, `pixel`, `edit_measurement`, `delete_measurement` and `open_image`.
