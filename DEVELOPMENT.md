# Developing lengto 0.6

lengto is a C17 application for Windows 10/11 x64. The UI, documentation,
generated macros, Python API and application messages are in English.

## Build

Install MSVC Build Tools with C/C++ tools and the Windows SDK on the build
machine. No compiler or additional runtime is required on the user's machine.

```powershell
.\build.ps1 -PdfiumPath 'C:\dependencies\pdfium.dll' -Test
```

The build validates the DLL's SHA-256 against `third_party/pdfium-sha256.txt`.
If `build/pdfium.dll` is already present, `-PdfiumPath` can be omitted.
Flags include `/TC /std:c17 /O1 /GL /MT`, with link-time optimization and
embedded resources. The process environment is changed only for that build.

The output is `portable/lengto-single/lengto.exe`. A release ZIP contains only
that executable. Publish it as a GitHub Release asset; source control excludes
generated binaries and archives.

## Size and resources

`tools/pack_resource.c` uses the Windows Compression API to compress PDFium,
component notices, the user guide and Python support with LZMS in buffer mode.
The build verifies every packed resource by decompressing and comparing every
byte before writing it. Runtime decompression uses the Windows `Cabinet.dll`,
with a 64 MiB upper bound on the expanded resource size.

Resources are expanded only when needed. PDFium is extracted to a private
temporary directory with a GUID name and loaded from its explicit path;
dependency lookup is restricted to that directory and Windows System32.
The file is held immutable until unloading, then removed on normal exit.
No executable packer, network fetch or external compression library is used.
The embedded PDF engine is the same verified DLL, with all notices retained.

The executable is about 3.46 MB (decimal), down from 7.70 MB in version 0.5,
including the embedded icons. The release contains no companion DLL or guide
file. Development assets, original logos, tests and the build-time PDFium DLL
are not required when distributing the application.

Reference: https://learn.microsoft.com/en-us/windows/win32/cmpapi/using-the-compression-api

## Source map

| File | Purpose |
|---|---|
| `src/lengto.c` | Native menus, dialogs, interaction, inline page entry, guide and GitHub link |
| `src/model.c/.h` | Global calibration, measurements, units and rounding |
| `src/document.c/.h` | WIC/PDFium decoding, clipboard images, source pixel sampling and atomic output |
| `src/render.c/.h` | Viewport rendering, annotations and hit testing |
| `src/portable.c/.h` | Compressed resources and temporary PDF engine lifecycle |
| `src/automation.c/.h` | Shared command engine for GUI and headless macros |
| `src/recorder.c/.h` | Semantic recording and atomic Python script generation |
| `src/script_runner.c/.h` | External Python, pipes, worker threads and cancellation |
| `src/script_json.c/.h` | Bounded UTF-8 JSON objects with scalar values |
| `src/python_api.py` | English Python API embedded into every recorded macro |
| `tools/pack_resource.c` | Build-only lossless resource packer |

## Verification

```powershell
.\build.ps1 -Test
python tests/test_scripts.py
```

Python integration tests need only its standard library. `LENGTO_TEST_EXE`
can point to a packaged executable outside the build directory. The fixture
generator uses Pillow and ReportLab only when regenerating image fixtures;
normal C tests use the committed fixtures without Python dependencies.

The C checks cover calibration, conversions, rounding, image formats, alpha,
large images, exports, preservation of existing files after failure, Unicode
paths and rendering at extreme zoom. Python tests cover recording and replay,
relative mapping, source colors, searches, edits, malformed requests, PDF/TIFF
pages, errors and cancellation. Native preview tests also exercise direct
arrow clicks and inline page entry, including Enter, Esc and page limits.

Internal preview commands do not show desktop windows:

```powershell
lengto.exe --preview tests\fixtures\drawing.pdf build\preview.png
lengto.exe --script-test tests\fixtures\drawing-multipage.tif build\recorded.py C:\Python\python.exe
```

Use absolute paths for reliable test invocation. These modes use the bundled
synthetic drawing, emit offscreen renders and return zero on success. Native
controls may omit combo text during WM_PRINT; selected values are also checked
through their control APIs. Offscreen images are not desktop screenshots.

## Measurement model

One calibration applies to all document pages; its page determines where the
reference is drawn. Coordinates use pixel centers (`index + 0.5`), with original
raster dimensions retained when the preview is reduced. PDF coordinates use
144 dpi. Shift preserves horizontal/vertical alignment on the same grid.

Calculations use double precision. Rounding in metres happens before conversion
to the selected display unit and does not change coordinates. One light-year
uses 365.25 days: 9,460,730,472,580,800 m. AU is the displayed astronomical unit;
the protocol also accepts its legacy spelling for old self-contained macros.

The preview is limited to 24 Mi pixels and 32768 pixels per side. Exports use
that preview resolution; sampling reads the full original frame. No perspective,
deformation or EXIF orientation correction is performed. At extreme zoom the
renderer draws only visible pixel cells, avoiding huge offscreen blits.

## Script architecture

GUI scripts and `--automation` use the same native command engine. The private
protocol is UTF-8 JSON with one request/response per line, requests up to 1 MiB
and pixel blocks up to 65536 pixels. A GUI worker forwards requests to the main
thread; another drains stderr. Python `print()` output goes to stderr while
hosted to avoid interfering with commands. There is no network listener or
simulated mouse automation.

The script and its descendants belong to a Windows job. Esc terminates that
job while the UI continues processing messages. A native operation already in
progress completes before cancellation is handled; completed work is retained.

New macros expose `Point`, `Measurement`, `run()`, `COORDINATES`, `open()`,
`calibrate()`, `measure()`, `find_pixels()` and other English names. Version 0.5
macros remain compatible through their own embedded support and the unchanged
protocol; generated examples and current documentation use only the new API.

## Page navigation and language

Arrow rendering and hit testing share the same rectangles. Clicks are checked
against both X and Y. Clicking the page counter activates a small native edit
control in place: Enter applies, Esc cancels, focus loss applies, and values
are clamped to the document range. No modal page dialog exists.

The main thread requests English Windows UI resources without changing the
user's system language. Text owned by lengto is English. Shell-owned controls
can fall back to installed Windows language resources.

The Guide and Help menu open exactly https://github.com/IvanPanzera/lengto
through the default browser. No content is published or uploaded by lengto.

## Clipboard images

Paste reads PNG, CF_DIBV5, CF_DIB and CF_BITMAP, with CF_HDROP for copied
image/PDF files. It never clears or replaces clipboard contents. DIB dimensions,
palette sizes, strides and buffer lengths are checked before WIC decoding.
Memory-backed streams retain the original raster independently of the clipboard;
the 24 Mi-pixel preview limit still applies. PNG/V5 alpha is composited over white.

The recorder embeds a pasted source as lossless PNG in base64. The Python API's
`open_image()` transfers it through a short-lived temporary file; the native
engine copies it to memory before responding, so the file can be removed even
while the GUI continues displaying it. First-input batch overrides still work.
Large pasted images increase macro size; the recorder's total text limit is
64 MiB. Manual pasting does not require Python or add runtime dependencies.

Native and Python tests cover row orientation, padding, palettes, opaque 32-bit
DIBs, V5 alpha, truncated/oversized headers, memory-source sampling, PDF export,
failed-open preservation and clipboard-independent recording/replay. Tests
inject image data without modifying the user's actual clipboard.

Clipboard layout reference: https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapv5header
