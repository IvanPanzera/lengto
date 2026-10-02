# Presentation assets

The README is the public product page; keep download and quick-guide links prominent.

- `images/lengto-logo.png`: original project logo, losslessly recompressed.
- `images/lengto-overview.png`: offscreen application render, not a desktop screenshot.
- `images/lengto-empty.png`: empty-window render.
- The engineering sample and workflow diagrams are original synthetic illustrations. SVG files are their editable sources.

Regenerate the application view:

```powershell
.\lengto.exe --preview docs\images\engineering-example.png build\product-overview.png
```

Copy the main output to `docs/images/lengto-overview.png`. The sample reference spans 500 pixels for 5 metres. It contains no client drawing or project data.

Publish the standalone EXE under Releases; source and documentation are not runtime dependencies.
