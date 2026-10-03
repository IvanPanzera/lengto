# Run lengto

lengto is portable. Measuring, viewing and recording macros do not require Python,
an installer, administrator access or an account.

| Platform | Package | Start |
|---|---|---|
| Windows 10/11, x64 | [lengto.exe](https://github.com/IvanPanzera/lengto/releases/latest/download/lengto.exe) | Double-click the EXE. |
| Linux, x86_64 | [AppImage](https://github.com/IvanPanzera/lengto/releases/download/v0.7.0-preview.1/lengto-linux-x86_64.AppImage) | Allow execution, then double-click. |
| macOS 11+, Intel or Apple Silicon | [Universal app ZIP](https://github.com/IvanPanzera/lengto/releases/download/v0.7.0-preview.1/lengto-macos-universal.zip) | Extract and open `lengto.app`. |

Use the package for your operating system. A Windows EXE cannot run natively on
Linux or macOS. Linux and macOS are preview builds. Packages and checksums are available on the
[release page](https://github.com/IvanPanzera/lengto/releases/tag/v0.7.0-preview.1).

## Linux

In the file manager, open the AppImage's properties and allow it to run as a
program. Alternatively:

```sh
chmod +x lengto-linux-x86_64.AppImage
./lengto-linux-x86_64.AppImage
```

If your system does not provide FUSE, run:

```sh
./lengto-linux-x86_64.AppImage --appimage-extract-and-run
```

The [`.tar.xz` package](https://github.com/IvanPanzera/lengto/releases/download/v0.7.0-preview.1/lengto-linux-x86_64.tar.xz) provides the same application without an AppImage runtime.
Extract it and run `lengto/AppRun`; keep that directory together. GTK and the
image/PDF libraries are included. The application uses the host's display server,
fonts and C runtime. Official Linux packages are built on Ubuntu 22.04 x86_64;
Linux ARM builds are not provided by this release.

## macOS

Extract the ZIP in Finder and open `lengto.app`. You may keep it in any folder,
or drag it into Applications. The same universal application contains both
Apple Silicon and Intel code. Its supporting files are inside the app bundle.
No Homebrew packages or Python installation are needed for measurement.

Development builds are ad-hoc signed, without Apple Developer ID notarization.
macOS may require **System Settings → Privacy & Security → Open Anyway** for a
download you trust. There is no need to disable Gatekeeper. Developer ID signing
and notarization require the maintainer's Apple Developer credentials.

On macOS, use **Command** in place of **Ctrl** for Open, Paste, Save, Export and
Close. The application menus appear in the macOS menu bar.

## Optional Python scripts

Install Python 3.8+ only if you want to run macros. The application and generated
scripts use the Python standard library. No packages need to be installed.
On Linux/macOS, `python3` must be available; `LENGTO_PYTHON` can select a specific
interpreter when starting the app from a terminal.

Examples:

```sh
python3 macro.py --lengto ./lengto-linux-x86_64.AppImage --input drawing.tif --output measured.pdf
python3 macro.py --lengto /Applications/lengto.app --input drawing.tif --output measured.pdf
```

New macros can discover `lengto` beside the script, on PATH, or inside a nearby
`lengto.app`. On macOS they also check Applications. Older self-contained macros
can use `--lengto /path/to/lengto.app/Contents/MacOS/lengto`.

Updating means replacing the EXE, AppImage or app bundle while lengto is closed.
Remove that item to uninstall. Keep your drawings, exports and macros separately.
