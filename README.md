<p align="center">
  <img src="docs/images/lengto-logo.png" alt="lengto" width="440">
</p>

<h1 align="center">Turn pixels into measurements.</h1>

<p align="center">Open an image. Set the scale. Measure.</p>

<p align="center">
  <a href="https://github.com/IvanPanzera/lengto/releases/latest/download/lengto.exe"><strong>Download lengto.exe</strong></a> ·
  <a href="docs/USER_GUIDE.md">Quick guide</a> ·
  <a href="docs/PYTHON_EXAMPLES.md">Python examples</a>
</p>

<p align="center"><strong>Windows 10/11 x64 · About 3.5 MB · No installation</strong></p>

**lengto** measures real distances on scanned plans, technical photographs and microscope images. All you need is a known reference length and a single executable.

![lengto measuring a calibrated engineering drawing](docs/images/lengto-overview.png)

*Application render using a synthetic drawing.*

## Small tool. Useful capabilities.

- **Open or paste.** TIFF, PNG, JPEG and PDF, including multipage documents. Paste images with **Ctrl+V**.
- **Calibrate and measure.** Pixel snapping, flexible units and editable measurement labels.
- **Save the result.** Export annotated images or PDFs; include all pages in TIFF/PDF.
- **Repeat the work.** Record editable Python macros and apply them to other images or whole folders.

## Start in three steps

1. [Download and run lengto.exe](https://github.com/IvanPanzera/lengto/releases/latest/download/lengto.exe). Open, drop or paste an image.
2. Choose **Measurements → Calibrate**, enter a known length and click its endpoints.
3. Choose **Measure**, click endpoint pairs, then **File → Export** to save. **Esc** ends the tool.

[Quick guide →](docs/USER_GUIDE.md)

Python 3.8+ is needed only to **run macros**. Measuring and recording need nothing extra.

**Know the limits:** one uniform scale per document; no perspective correction. Exports flatten annotations and use preview resolution. Keep the source and a macro to reproduce your work.

[Build from source](DEVELOPMENT.md) · [Releases](https://github.com/IvanPanzera/lengto/releases) · [Report a problem](https://github.com/IvanPanzera/lengto/issues)

[MIT license](LICENSE) · © 2026 Ivan Panzera. Bundled components retain their [own licenses](third_party).
