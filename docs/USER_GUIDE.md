# Quick guide

[Download lengto.exe](https://github.com/IvanPanzera/lengto/releases/latest/download/lengto.exe) · [Overview](../README.md)

**Windows, Linux and macOS. [Choose your portable package](INSTALLATION.md).**

On macOS, use **Command** in place of **Ctrl**; menus appear in the system menu bar.

## Measure your first drawing

1. **Open:** drag in a TIFF, PNG, JPEG or PDF; use **Ctrl+O** to browse or **Ctrl+V** to paste an image.
2. **Calibrate:** press **C**, enter a known length and its unit, confirm, then click the two reference endpoints.
3. **Measure:** press **M** and click endpoint pairs. Every pair creates a measurement; **Esc** ends the tool.
4. **Export:** press **Ctrl+E**, choose the format and destination. TIFF/PDF can include all pages; PNG/JPEG save the current page.

Try the [sample drawing](images/engineering-example.png): its marked reference is **5 m**.

## Everyday controls

| Action | Control |
|---|---|
| Pan / zoom | Left-drag / mouse wheel |
| Fit the page | **F** |
| Constrain a measurement | Hold **Shift** for the second point |
| Edit / delete a measurement | **Esc**, then right-click to edit; click and **Delete** to remove |
| Units and rounding | **Measurements → Settings** |
| Previous / next page | Bottom arrows or **Page Up / Page Down** |
| Jump to a page | Click the number in **1 / 18**, type, press **Enter** |
| Save / save as | **Ctrl+S** / **Ctrl+Shift+S** |

Reopen **Calibrate** to change the scale; **Segment** replaces its endpoints. Existing measurements update automatically. Settings change displayed units and rounding, not the original calculation.

## Repeat with a macro

Choose **Script → Start recording**, select absolute or relative coordinates, then work normally. Include an export if you want an output file. **Stop recording** saves a `.py`; **File → Run script** replays it. **Esc** stops execution, keeping completed operations.

Running macros needs **Python 3.8+**; recording does not. Absolute coordinates repeat pixel positions. Relative coordinates suit resized images with matching framing. Opened files must remain available; pasted images are embedded in recordings.

[Batch processing and Python examples →](PYTHON_EXAMPLES.md)

## Keep in mind

- One calibration applies to **all pages**. Use a known reference on the same plane; perspective and scan distortion are not corrected.
- Exports contain **flattened annotations**, not editable measurement objects. Keep the original and a macro to reproduce a session.
- PDF input uses **144 dpi**. Large images may have a reduced preview; exports use that resolution. More decimal places do not improve accuracy.

**Trouble?** Check the calibration unit, choose a writable output folder, or check the Python interpreter. Paste accepts image data or a copied supported file, not plain text. [Report persistent problems](https://github.com/IvanPanzera/lengto/issues).
