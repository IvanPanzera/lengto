# Build lengto

For the Linux/macOS preview, download and extract the [native source archive](https://github.com/IvanPanzera/lengto/releases/download/v0.7.0-preview.1/lengto-native-source.tar.gz) before following the commands below. The preview source snapshot is also kept on [codex/native-build](https://github.com/IvanPanzera/lengto/tree/codex/native-build).

The measurement model, recorder and automation protocol are C17 and shared across
Windows, Linux and macOS. Interfaces are native: Win32, GTK 3 and AppKit. A small
Objective-C adapter connects the macOS system frameworks. End users receive a
portable executable or app bundle; no Python runtime is shipped.

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
| `platform.*` | Strict UTF-8 conversion and portable path/atomic-file operations |
| `unix/app.c`, `unix/script.c` | Shared Linux/macOS interaction and asynchronous Python host |
| `unix/linux_*` | GTK interface, Cairo drawing, native image libraries and PDFium |
| `unix/mac_native.m` | AppKit, CoreGraphics, ImageIO and CoreText adapter |

`tools/pack_resource.c` compresses resources with Windows LZMS. `tools/export-icons.ps1` regenerates icons from `src/assets/lengto-symbol.png`.

## Linux build

Use an x86_64 Linux development environment. Release builds use Ubuntu 22.04 to
avoid requiring a newer C runtime on users' machines.

```sh
sudo apt-get install cmake ninja-build pkg-config libgtk-3-dev libpng-dev libjpeg-dev libtiff-dev python3 patchelf xvfb xauth
python3 tools/fetch_pdfium.py
cmake -S . -B build/linux -G Ninja
cmake --build build/linux --parallel
ctest --test-dir build/linux --output-on-failure
xvfb-run -a build/linux/lengto --preview tests/fixtures/drawing.pdf build/linux-preview.png
python3 tools/package_linux.py build/linux
```

The packager produces an AppImage and a `.tar.xz` directory in `portable/`.
Both include shared runtime libraries and their notices. The host supplies libc,
display services and fonts. Tool downloads and the PDFium SDK are SHA-256 pinned;
if an upstream continuous packaging asset changes, review it before updating the
pin. Never bypass the checksum failure. `cmake --install build/linux --prefix ...`
is also available for a traditional Linux installation with system GTK/codecs.

## macOS build

On a Mac with Apple Command Line Tools and Python 3.8+:

```sh
sh tools/package_macos.sh
```

This builds and tests a universal Intel/Apple Silicon app for macOS 11+. CMake is
used if available; otherwise the script uses Apple's compiler directly. No
Homebrew runtime libraries are linked. The ZIP in `portable/` contains one
`lengto.app`, including its icon and compressed guide, licenses and Python API.

The script verifies runtime dependencies and applies an ad-hoc signature.
Developer ID signing/notarization is a separate distribution step requiring the
maintainer's Apple credentials. Do not describe ad-hoc builds as notarized.

`.github/workflows/native.yml` builds Linux on Ubuntu 22.04 and the universal Mac
app on both Apple Silicon and Intel runners. The workflow tests native exports,
recording/replay, Unicode paths and controller behavior before uploading packages.

## Implementation notes

- One document calibration; pixel centers at `index + 0.5`; double-precision calculations. Rounding changes labels only.
- PDF input: 144 dpi. Preview: at most 24 Mi pixels and 32,768 pixels per side on Windows (32,767 on Linux/macOS). Exports use preview resolution; source sampling retains original raster indices.
- PDFium is extracted to a private temporary folder, loaded from that location and removed on normal exit. No persistent cache or installed service.
- GUI and headless scripts share a bounded, line-delimited JSON protocol. Esc terminates Python and its children; completed native operations remain applied.
- Clipboard images stay in memory and are recorded as embedded PNG data. File-based recordings keep paths. Macro text is limited to 64 MiB.
- Version 0.5 macros retain compatibility through their embedded API. New macros use English names.
- Linux/macOS keep the current source raster for exact RGB sampling, with a
  268-million-pixel allocation limit. A reduced preview is allocated only when
  needed. macOS multipage TIFF export spools pages to temporary files rather than
  retaining every decoded page in memory.

The suite covers formats, Unicode paths, calibration, export failures, clipboard decoding, recording, replay and cancellation. UI preview modes exercise page navigation and render a window snapshot. Use Xvfb on Linux for a virtual display:

```powershell
.\lengto.exe --preview tests\fixtures\drawing.pdf build\preview.png
```

Use absolute paths when invoking diagnostics from another directory. Component licenses are kept in [third_party](third_party) and embedded in the executable.
