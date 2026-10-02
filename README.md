<p align="center">
  <img src="docs/images/lengto-logo.png" alt="lengto" width="440">
</p>

<h1 align="center">Turn pixels into measurements.</h1>

<p align="center">
  Calibrate an image. Measure what matters. Turn repeated work into Python macros.
</p>

<p align="center">
  <strong>Windows 10/11 x64 · Native C · One portable EXE · About 3.5 MB</strong>
</p>

<p align="center">
  <a href="https://github.com/IvanPanzera/lengto/releases/latest"><strong>Download for Windows</strong></a> ·
  <a href="docs/USER_GUIDE.md"><strong>User guide</strong></a> ·
  <a href="docs/PYTHON_EXAMPLES.md">Python examples</a> ·
  <a href="DEVELOPMENT.md">Build from source</a>
</p>

**lengto** is a lightweight image viewer for measuring real distances on engineering scans, technical photographs and microscope images with a known scale. Open a file, mark a reference distance and start measuring. When the work becomes repetitive, record your actions as an editable Python script.

![lengto displaying an engineering plan with a 5 m calibration and measurements of 5.000 m and 2.400 m](docs/images/lengto-overview.png)

*Application view rendered by lengto 0.6 using a synthetic engineering drawing. The interface, guide and Python API are in English.*

## A small tool for a clear job

- **Open familiar formats.** Read TIFF, PNG, JPEG and PDF, including multipage TIFF and PDF documents.
- **Paste an image.** Use **Ctrl+V** or **File → Paste** to bring a copied image straight into lengto.
- **Move through pages directly.** Click the arrows for the previous/next page, or click the page number to type a destination in place.
- **Set one reference.** Define a known distance and its unit. Updating the calibration updates the measurements throughout the document.
- **Measure continuously.** Every pair of clicks creates a measurement. Endpoints snap to pixel centers; press **Esc** to leave the tool.
- **Inspect the details.** Drag to pan and use the wheel to zoom, up to a single source pixel spanning the viewport on full-resolution previews.
- **Save annotated drawings.** Export to TIFF, JPEG, PNG or PDF. TIFF and PDF can include all pages.
- **Automate repeated work.** Record Python macros, change their parameters and run them in the application or in batches without its window.

![Four steps: open a document, calibrate a known distance, measure between pixels, and automate with Python](docs/images/measurement-workflow.png)

## Where lengto fits

| Your work | A practical use |
|---|---|
| Engineering and technical archives | Recover dimensions from scanned plans when the original CAD file is unavailable. |
| Scan quality checks | Compare known spans and horizontal/vertical proportions on a reference target. |
| Materials and microscopy | Measure coating thickness, fiber widths or visible crack openings on calibrated images. |
| Electronics and component inspection | Check track widths, contact spacing and dimensions in images captured at a consistent scale. |
| Repeated inspections | Apply a recorded procedure to a series of comparable images and collect results with Python. |

The best fit is **planar distance measurement with a known, uniform scale**. A reference on the same plane as the feature being measured makes the result meaningful.

## From manual measurements to Python

Start recording as soon as lengto opens—even before loading a document. Work normally, then stop recording and choose where to save the `.py` file.

Each recorded endpoint keeps three pieces of information:

- Its **absolute pixel coordinates**.
- Its **relative coordinates**, stored as exact fractions of the image width and height.
- Its **RGB color**, sampled from the source image without lengto's measurement overlays.

![A selected pixel represented as a Python Point with absolute coordinates, relative fractions and RGB values](docs/images/pixel-to-python.png)

Choose absolute coordinates to repeat exact pixel positions, or relative coordinates for equivalent images at different resolutions. Relative coordinates assume matching framing and proportions; they do not align shifted, cropped or rotated images. Stored RGB values are available to your script and do not automatically relocate points.

The generated file contains its own Python support code. Edit its `run()` function to add loops, conditions or color searches. For example, **after opening and calibrating an image**, this loop measures from a reference pixel to pixels close to a chosen red:

```python
origin = app.pixel(0, 0)

for point in app.find_pixels(rgb=(220, 40, 40), tolerance=15, limit=100):
    if point.absolute != origin.absolute:
        measurement = app.measure(origin, point, name="Red marker")
        print(point.absolute, measurement.metres)

app.export(output_file or "measured.pdf")
```

Color searches can also use custom conditions, such as `predicate=lambda r, g, b: max(r, g, b) < 100`. Searches return matching **pixels**, so grouping them into objects or selecting a particular edge is part of your script.

Replay a recorded workflow on another file:

```powershell
python macro.py --input "next-scan.tif" --output "next-scan-measured.pdf"
```

Keep `lengto.exe` beside the macro, or provide `--lengto "C:\tools\lengto.exe"`. The input and output options replace the first recorded open and export operations; record an export if you want the macro to write an annotated file. For a complete folder loop, see the [Python examples](docs/PYTHON_EXAMPLES.md).

## Get started

Download **lengto.exe** or **lengto-portable-x64.zip** from the [latest release](https://github.com/IvanPanzera/lengto/releases/latest). The ZIP contains the single executable; GitHub's source-code archives are for developers. For step-by-step instructions, shortcuts and troubleshooting, read the **[complete user guide](docs/USER_GUIDE.md)**.

1. Extract the portable package and run **`lengto.exe`**. No installer is needed.
2. Open or drag in an image or PDF, or paste a copied image with **Ctrl+V**.
3. Choose **Measurements → Calibrate**, enter a known length and its unit, then mark its two endpoints.
4. Choose **Measurements → Measure** and keep picking endpoint pairs. Press **Esc** when finished.
5. Use **File → Export** to save the annotated drawing.

To record a macro, use **Script → Start recording** and **Script → Stop recording**. To run one, choose **File → Run script**. Press **Esc** to interrupt a running script; completed operations remain applied.

| Requirement | Manual use and recording | Running Python macros |
|---|---|---|
| Windows 10/11 x64 | Required | Required for lengto |
| Python 3.8 or later | Not required | Required; a portable interpreter works |
| Extra Python packages | None | None for the included API |
| Files beside `lengto.exe` | None required | Your macro and input documents |

The PDF engine, guide, Python support and component notices are compressed inside the executable and expanded only when needed. PDF operations unpack the engine into a private temporary folder, removed on normal exit. There are no application profiles, registry settings or background services to install.

## Measurement scope

lengto 0.6 uses **one uniform calibration per document**, including all its pages, with up to **4,096 measurements**. It measures straight distances in the image plane; perspective, lens distortion and scan deformation are not corrected.

Display units range from ångströms to light-years. Rounding steps range from **10⁻¹⁰ to 10¹⁶ metres**. Display precision does not increase the accuracy of the image or its calibration.

PDFs use a **144 dpi** reference raster. Very large images may use a reduced preview, and exports use the preview resolution. Pixel coordinates and RGB sampling still refer to the original raster grid, or the 144 dpi grid for PDFs.

Exports contain flattened annotations. There is no custom project format: use a recorded macro and the original input file to reproduce a measurement session. Astronomy-specific FITS/WCS calibration, area measurement and automatic object segmentation are outside the current feature set.

## Project information

- **Version:** 0.6, Windows x64.
- **Implementation:** C17, native Win32 interface, Windows Imaging Component and embedded PDFium.
- **Validation:** 315 C checks and 9 Python integration tests passed for this version, covering image formats, clipboard image decoding, measurements, recording, replay, pixel sampling and PDF export.
- **Documentation:** [Complete user guide](docs/USER_GUIDE.md), [Plain-text guide](GUIDE.txt), [Build notes](DEVELOPMENT.md), [Python examples](docs/PYTHON_EXAMPLES.md).
- **Project website:** [github.com/IvanPanzera/lengto](https://github.com/IvanPanzera/lengto), also available through the Guide's **GitHub** button and **? → GitHub**.
- **Component notices:** available in **? → About** and in [third_party](third_party).

*Small enough to carry with your drawings. Scriptable enough to repeat the work.*
