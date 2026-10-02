# lengto user guide

**Version 0.6 · Windows 10/11 x64**

[Product overview](../README.md) · [Download](https://github.com/IvanPanzera/lengto/releases/latest) · [Python examples](PYTHON_EXAMPLES.md) · [Build instructions](../DEVELOPMENT.md)

lengto turns distances in an image into real measurements using a reference length you provide. This guide covers manual measurement, multipage documents, exports and Python macros.

## Contents

- [Download and start](#download-and-start)
- [Your first measurement](#your-first-measurement)
- [Open a document](#open-a-document)
- [Calibrate the scale](#calibrate-the-scale)
- [Measure and edit](#measure-and-edit)
- [Pan, zoom and change pages](#pan-zoom-and-change-pages)
- [Units and precision](#units-and-precision)
- [Save and export](#save-and-export)
- [Record a Python macro](#record-a-python-macro)
- [Run and adapt a macro](#run-and-adapt-a-macro)
- [Coordinates and source colors](#coordinates-and-source-colors)
- [Keyboard shortcuts](#keyboard-shortcuts)
- [Troubleshooting](#troubleshooting)
- [Limits and accuracy](#limits-and-accuracy)

## Download and start

1. Open the [latest release](https://github.com/IvanPanzera/lengto/releases/latest).
2. Download `lengto-portable-x64.zip` from **Assets** and extract it, or download `lengto.exe` directly. GitHub's **Source code** archives contain the development files, not the ready-to-run application.
3. Run `lengto.exe` on Windows 10 or 11, 64-bit.

The application is portable: no installer, companion DLL, .NET, Java or Visual C++ Redistributable is required. Python is optional; you need Python 3.8 or later only to **run** macros. Manual measurement and macro recording work without Python.

The PDF engine and component notices are embedded in the executable. PDF operations extract the engine into a private temporary folder and remove it on normal exit. Your Windows temporary folder must be writable.

## Your first measurement

![lengto showing a calibrated engineering example](images/lengto-overview.png)

*Application render using the synthetic example below.*

Use the [sample drawing](images/engineering-example.png) to follow these steps. Open the image on GitHub and download its original file.

1. Choose **File → Open** and select the drawing.
2. Choose **Measurements → Calibrate** (or press **C**).
3. Enter **5** and select **m**. Confirm, then click the two ends of the reference marked **5 m**.
4. Choose **Measurements → Measure** (or press **M**).
5. Click two endpoints of a feature. Each completed pair creates a measurement. Keep clicking pairs to add more.
6. Press **Esc** to leave the measurement tool.
7. Choose **File → Export** (or **Ctrl+E**) to save an annotated PNG, TIFF, JPEG or PDF.

On this sample, the reference runs from pixel `(100, 130)` to `(600, 130)`. A segment from `(100, 180)` to `(100, 420)` measures **2.400 m**. Zoom in to place the endpoints accurately.

## Open a document

Choose **File → Open**, press **Ctrl+O**, or drag a supported file into the window. Supported input formats are **TIFF, PNG, JPEG and PDF**, including multipage TIFF and PDF.

To use a screenshot or another copied image, choose **File → Paste** or press **Ctrl+V**. You can also paste a supported file copied in File Explorer. Pasted images use the same calibration, measurement and export tools as opened files. Save an annotated output when you want to keep your work.

Only one document is open at a time. Opening another document or closing the current one offers to save unsaved annotations. The input file remains the source for the session; save annotated results to a separate destination.

## Calibrate the scale

Calibration tells lengto how much real distance corresponds to a segment in your image.

1. Identify a known length on the same plane as the features to measure.
2. Open **Measurements → Calibrate**.
3. Enter its positive length and unit. A decimal point or decimal comma is accepted.
4. Confirm and click the two reference endpoints.

There is **one uniform calibration for the entire document**, including every page. Use multipage documents only when their pages share the same scale. The reference segment is drawn on the page where it was defined.

To change the known value or its unit, reopen **Calibrate**. To replace the reference endpoints, choose **Segment** in that dialog and select a new pair, including on another page. **Esc** cancels a replacement. Changing the calibration recalculates all existing measurements.

Calibration assumes a uniform scale. It does not correct perspective, lens distortion or deformation in a scan. Prefer a long, clearly defined reference: uncertainty of a few pixels matters more on a short reference.

## Measure and edit

Choose **Measurements → Measure** and click endpoint pairs. The tool stays active until **Esc**, including after zooming, panning, changing settings, saving or changing pages.

- Endpoints snap to the **centers of source pixels**.
- Two clicks on the same pixel do not create a zero-length measurement.
- Hold **Shift** while placing the second endpoint to constrain the segment horizontally or vertically.
- Drag with the left mouse button to pan without placing an endpoint.
- Changing pages cancels an unfinished segment and keeps completed measurements.

To edit a measurement, press **Esc** to leave the tool, then:

| Action | Result |
|---|---|
| Click a measurement | Select it. |
| Press **Delete** | Remove the selected measurement. |
| Right-click a measurement | Open its editing dialog to change its name or replace its segment. |

Names support up to 79 characters. A document can contain up to 4,096 measurements across its pages.

## Pan, zoom and change pages

- **Pan:** hold the left mouse button and drag.
- **Zoom:** turn the mouse wheel; zoom is centered around the pointer.
- **Fit:** click **Fit** or press **F** to show the whole page.
- **Previous/next page:** click the arrows beside the page counter, or use **Page Up / Page Down**. These arrows navigate directly.
- **Jump to a page:** click the current number in the **1 / 18** indicator, type a page number and press **Enter**. **Esc** cancels; moving focus elsewhere applies the value. Values outside the document range select the nearest valid page.

The first/last-page arrows are disabled at the corresponding boundary. Page controls are disabled while a script is running. Calibration and completed measurements survive page changes.

## Units and precision

Open **Measurements → Settings** to choose the display unit and rounding step. Display units are independent of the unit used to enter the calibration.

Available units: **Å, nm, µm, mm, cm, dm, m, dam, hm, km, Mm, Gm, AU, Tm, Pm and ly**.

Precision is a rounding step measured in metres, from **10⁻¹⁰ m** to **10¹⁶ m** in integer powers of ten. The default is **10⁻³ m**, or **1 mm**. Changing the display unit keeps the same physical rounding step.

Only displayed and exported labels are rounded. Source coordinates and calculated lengths stay unchanged. More decimal places do not make the underlying image or calibration more accurate.

## Save and export

| Command | What it does |
|---|---|
| **File → Save** | Asks for an output destination the first time, then updates that destination. |
| **File → Save as** | Chooses another destination and format. |
| **File → Export** | Chooses an output format and, for TIFF/PDF, whether to export the current page or all pages. |

| Output format | Pages | Compression |
|---|---|---|
| PNG | Current page | Lossless |
| JPEG | Current page | Quality 95 |
| TIFF | Current page or all pages | Lossless |
| PDF | Current page or all pages | Lossless raster images |

Exports include the complete page with annotations, regardless of your pan and zoom. The last successful output becomes the next **Save** destination. If other pages still have unsaved annotations, the document stays marked as modified.

**Exports flatten the annotations into the image.** Measurements remain editable while the source document stays open, but reopening an exported file does not restore them as editable objects. There is no separate project format. To reproduce a session later, keep the original source file and record a macro.

Repeated exports in the same session redraw from the source, so annotations do not accumulate. File writes use a temporary file and atomic replacement; a failed export preserves an existing destination.

Exports use the preview resolution. PDF input is rasterized at **144 dpi**; very large image inputs may use a reduced preview. A PDF export is therefore an annotated raster document, not an editable vector/CAD document.

## Record a Python macro

1. Choose **Script → Start recording**, even before opening a document.
2. Select **Relative** or **Absolute** coordinates.
3. Open a document, calibrate, measure and edit as usual. Include a save/export operation if you want the macro to produce an output file.
4. Choose **Script → Stop recording**, then save the `.py` file.

**REC** in the title/status bar indicates active recording. Canceling the script save dialog or a failed write keeps the recording available. Closing lengto during recording prompts you to save the macro.

The recorder captures completed open/close operations, page changes, settings, calibration, measurements, edits, deletions and successful saves/exports. It ignores pan, zoom and incomplete segments. Recording with a document already open also captures its initial calibration, measurements and current page.

For opened files, the macro refers to input paths on disk: keep those files available or supply a new input when replaying. For pasted images, the recorder embeds a PNG snapshot in the macro, so replay works after the clipboard changes. These recordings can be larger because they include image data.

Each generated macro includes an editable `run()` function, a `COORDINATES` setting and its own Python support code. No additional Python packages are needed for the included API. See the [Python examples](PYTHON_EXAMPLES.md) for folder processing, color searches and CSV output.

## Run and adapt a macro

**In the application:** choose **File → Run script** and select a `.py` file. Operations appear in the current window. You can save current edits before execution. Press **Esc** to stop Python and its child processes; completed operations remain applied. A native operation already underway finishes before cancellation is handled.

The application finds `py.exe` or `python.exe`, or asks you to choose an interpreter. A portable Python interpreter works. The selection lasts for the session; `LENGTO_PYTHON` can specify the interpreter path. lengto does not install Python.

**Without the application window:** open a terminal and run:

```powershell
python macro.py --input "next-scan.tif" --output "next-scan-measured.pdf"
```

Place `lengto.exe` beside the macro, add it to `PATH`, set `LENGTO_EXE`, or specify it explicitly:

```powershell
python macro.py --lengto "C:\tools\lengto.exe" --coordinates absolute
```

`--input` replaces the **first** recorded open operation. `--output` replaces the **first** recorded export; it does not create an export operation. With `--input` alone, the first output uses the input stem plus `_measured` and the recorded extension. Later recorded paths stay literal unless you edit the script.

Scripts are ordinary Python programs and can access files with your user permissions. The GUI displays errors and `print()` output at the end, retaining the last 8 KiB. Previously returned Python measurement values are snapshots; they do not change if you later recalibrate the document.

## Coordinates and source colors

Every recorded endpoint includes absolute coordinates, exact relative fractions and its source RGB color:

```python
Point(
    absolute=(345, 120),
    relative=(Fraction(345, 65634), Fraction(120, 40000)),
    rgb=(28, 31, 35),
)
```

Indices start at zero in the top-left corner. X increases to the right and Y downward. In this example, the image is 65,634 × 40,000 pixels.

| Coordinate mode | Appropriate use |
|---|---|
| Absolute | Repeat the same pixel indices on matching images. |
| Relative | Adapt to images with the same framing and proportions at a different resolution. |

Relative fractions use `index / dimension`. Replay selects `floor(fraction × current_dimension)`, then measures between pixel centers. Relative coordinates do not align shifted, cropped or rotated images. A plain Python `(x, y)` pair always uses absolute indices.

RGB values are sampled from the source raster, even with a reduced preview. PDF sampling uses its 144 dpi raster; transparency is composited over white. lengto's overlays are excluded, but annotations already present in the input are part of the source.

Stored RGB values **do not automatically relocate endpoints**. Use `app.find_pixels()` to search explicitly by color or a custom condition. Results are individual matching pixels; object grouping and edge selection belong in your script.

## Keyboard shortcuts

| Shortcut | Action |
|---|---|
| **Ctrl+O** | Open a document |
| **Ctrl+V** | Paste an image or a copied supported file |
| **Ctrl+S** | Save annotated output |
| **Ctrl+Shift+S** | Save as |
| **Ctrl+E** | Export |
| **Ctrl+W** | Close the document |
| **C** | Calibrate |
| **M** | Measure |
| **F** | Fit the page |
| **F1** | Open the guide |
| **Esc** | Cancel the current tool or stop a running script |
| **Delete** | Delete the selected measurement |
| **Page Up / Page Down** | Previous / next page |
| **Shift** while placing the second endpoint | Constrain horizontally or vertically |

In dialogs, **Confirm** applies changes; **Esc** or the close button cancels them. In the inline page field, **Enter** applies the number and **Esc** cancels.

## Troubleshooting

| Symptom | What to check |
|---|---|
| Measurements have an unexpected scale | Recheck the reference endpoints, entered value and calibration unit. Every page shares one scale. |
| I cannot select a measurement | Press **Esc** to leave Measure, then click the segment. |
| Labels show unexpected units or rounding | Open **Measurements → Settings**. Display units and the physical rounding step are separate settings. |
| An exported drawing has fewer pixels than the input | Very large inputs use a reduced preview; exports use that resolution. PDF input uses 144 dpi. |
| I cannot edit measurements after reopening an export | Exported annotations are flattened. Reopen the source and replay a recorded macro. |
| A macro cannot find lengto | Keep the EXE beside the macro or provide `--lengto`, `LENGTO_EXE` or `PATH`. |
| A macro runs but produces no output | Include a recorded export or add `app.export(...)` in `run()`. `--output` only replaces an existing export path. |
| A macro uses the wrong points on another image | Check absolute/relative mode, dimensions, framing and page number. Relative mode does not perform image alignment. |
| Python is not found | Install or select Python 3.8+, including a portable interpreter, or set `LENGTO_PYTHON`. |
| Paste does not open an image | Copy image data or a supported file, then try **Ctrl+V** again. A copied web address or plain text is not an image. |
| PDF import/export fails | Check that the PDF is not password-protected, the Windows temporary folder is writable, and the destination is writable and not locked by another application. |
| Save leaves the document marked as modified | Annotations on other pages may still be unsaved; export all pages to TIFF or PDF. |

If a problem persists, [open an issue](https://github.com/IvanPanzera/lengto/issues) with the lengto version, Windows version, file format, error message and steps to reproduce it. A small non-confidential sample is preferable to a private client drawing.

## Limits and accuracy

- One uniform scale per document; straight distances in the image plane only.
- Up to 4,096 measurements; no area measurement, CAD recognition, object segmentation or FITS/WCS calibration.
- No perspective, lens distortion, scan deformation or EXIF orientation correction.
- PDF source coordinates use a 144 dpi raster. Password-protected PDFs are unsupported.
- Preview limit: 24 Mi pixels and 32,768 pixels per side. Exports use the preview resolution; original raster indices are retained for pixel snapping and source sampling.
- Accuracy depends on source resolution, endpoint placement, the known reference and image geometry. Display precision is not a statement of measurement uncertainty.

For API examples, continue to [Python workflows](PYTHON_EXAMPLES.md). The [plain-text guide](../GUIDE.txt) is also embedded in the executable; component notices are available in **? → About** and [third_party](../third_party).
