PDFium 153.0.7999.0, windows_x64, build without V8/XFA.
Origin: pdfium-binaries, bundled by pypdfium2 5.13.0.
https://github.com/bblanchon/pdfium-binaries
https://pdfium.googlesource.com/pdfium/

The distributed pdfium.dll was copied from the existing local dependency
runtime. No Python components are used or distributed with the application.
The exact DLL version is recorded in pdfium-version.json; its SHA-256 is
recorded in pdfium-sha256.txt. Licenses supplied with that build are included
in pdfium-licenses. The complete notices and this provenance are embedded in lengto.exe and can be read under ? > About.

PDFium is embedded as a losslessly compressed resource and loaded through its C API from a private temporary directory. That directory is removed on normal exit. No adjacent DLL is required or loaded.
Imports of the included x64 build: KERNEL32, ADVAPI32, GDI32 and USER32.
