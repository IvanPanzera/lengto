# Publishing the product presentation

The repository root `README.md` is the English product presentation for [IvanPanzera/lengto](https://github.com/IvanPanzera/lengto). It uses relative links so its images travel with the repository. The README links to the latest release and the complete user guide.

## Include these files

- `README.md`
- `docs/USER_GUIDE.md`
- `docs/PYTHON_EXAMPLES.md`
- `docs/images/lengto-logo.png`
- `docs/images/lengto-overview.png`
- `docs/images/measurement-workflow.png`
- `docs/images/pixel-to-python.png`
- `docs/images/engineering-example.png`
- The existing `GUIDE.txt`, `DEVELOPMENT.md`, `third_party/`, source and test files referenced by the presentation.

The SVG files in `docs/images/` are editable originals for the diagrams and sample drawing. They are optional for displaying the README but useful for maintaining it.

The current `.gitignore` excludes `.exe` and `.zip` files. Distribute `lengto-portable-x64.zip` or `lengto.exe` as GitHub Release attachments. Once a real release exists, add its download link to the README's Get started section. The app itself still needs only `lengto.exe`; documentation images are repository assets, not runtime dependencies.

## Suggested repository description

Portable image measurement in native C. Calibrate scans, measure real distances, and record Python macros for batch workflows.

## Suggested topics

`image-measurement`, `engineering`, `microscopy`, `calibration`, `python-automation`, `portable`, `windows`, `c`

## Image provenance

- **Logo:** copied from the existing project asset `output/logo/lengto-logo-minimale.png` and losslessly recompressed. Decoded pixels are identical to the original.
- **Application view:** produced by the existing lengto 0.6 offscreen preview command using the synthetic `engineering-example.png`. It shows the native drawing renderer and English interface labels, with a simplified menu strip drawn by that preview mode. It is an application render, not a desktop screenshot.
- **Workflow and Python point illustrations:** original vector diagrams, supplied as editable SVG and rendered PNG.
- **Engineering example:** original synthetic SVG, rendered to PNG. Its reference span is 500 pixels for 5 metres. It is a demonstration drawing, not a client document or construction plan.

The presentation does not imply that the current app supports multiple calibrations, processes FITS/WCS data, measures areas or automatically recognizes objects.

## Regenerate the application view

After rendering `engineering-example.svg` to `engineering-example.png`, use the existing diagnostic preview command:

```powershell
lengto.exe --preview docs\images\engineering-example.png build\product-overview.png
```

Copy `build/product-overview.png` to `docs/images/lengto-overview.png`. The command also creates diagnostic images and an annotated PDF under `build`; those are not needed for the README.

The publication includes source, synthetic fixtures, component notices, documentation and presentation images. Build products and local working files are excluded. Release assets contain the standalone Windows executable, its ZIP package and SHA-256 checksums.
