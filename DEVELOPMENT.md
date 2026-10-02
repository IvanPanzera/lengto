# Build lengto

The application is C17 with a native Win32 interface. End users need only `lengto.exe`; these files are for development.

## Requirements

- Windows x64, MSVC Build Tools with C/C++ tools and the Windows SDK.
- The PDFium DLL identified by [its version](third_party/pdfium-version.json) and [SHA-256](third_party/pdfium-sha256.txt). See [provenance and notices](third_party/README.txt).
- Python 3.8+ for integration tests. No extra packages are needed unless regenerating fixtures with Pillow and ReportLab.

## Build and test

```powershell
.\build.ps1 -PdfiumPath 'C:\dependencies\pdfium.dll' -Test
python tests/test_scripts.py
```

If `build/pdfium.dll` already exists, omit `-PdfiumPath`. The build checks its hash, compresses and verifies embedded resources, and produces **`portable/lengto-single/lengto.exe`**. Publish that file as a release asset. `LENGTO_TEST_EXE` can select a different executable for Python tests.

## Source map

| Files under `src/` | Purpose |
|---|---|
| `lengto.c` | Menus, dialogs, input and page navigation |
| `model.*`, `render.*` | Calibration, measurements, labels and drawing |
| `document.*`, `portable.*` | Image/PDF decoding, clipboard input, export and embedded resources |
| `automation.*`, `recorder.*` | Shared command engine and Python recording |
| `script_runner.*`, `script_json.*`, `python_api.py` | Script execution, protocol and Python API |

`tools/pack_resource.c` compresses resources with Windows LZMS. `tools/export-icons.ps1` regenerates icons from `src/assets/lengto-symbol.png`.

## Implementation notes

- One document calibration; pixel centers at `index + 0.5`; double-precision calculations. Rounding changes labels only.
- PDF input: 144 dpi. Preview: at most 24 Mi pixels and 32,768 pixels per side. Exports use preview resolution; source sampling retains original raster indices.
- PDFium is extracted to a private temporary folder, loaded from that location and removed on normal exit. No persistent cache or installed service.
- GUI and headless scripts share a bounded, line-delimited JSON protocol. Esc terminates Python and its children; completed native operations remain applied.
- Clipboard images stay in memory and are recorded as embedded PNG data. File-based recordings keep paths. Macro text is limited to 64 MiB.
- Version 0.5 macros retain compatibility through their embedded API. New macros use English names.

The suite covers formats, Unicode paths, calibration, export failures, clipboard decoding, recording, replay and cancellation. UI preview modes exercise navigation without opening desktop windows:

```powershell
.\lengto.exe --preview tests\fixtures\drawing.pdf build\preview.png
```

Use absolute paths when invoking diagnostics from another directory. Component licenses are kept in [third_party](third_party) and embedded in the executable.
