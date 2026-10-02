$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$iconRoot = Split-Path -Parent $PSScriptRoot
$iconAssets = Join-Path $iconRoot 'src\assets'
New-Item -ItemType Directory -Path $iconAssets -Force | Out-Null

function Export-WindowsIcon([string]$Source, [string]$Destination, [int[]]$Widths, [int]$Aspect, [switch]$FitSymbol) {
    $sourceImage = [System.Drawing.Image]::FromFile($Source)
    $frames = [System.Collections.Generic.List[object]]::new()
    try {
        $sourceRect = [System.Drawing.Rectangle]::new(0, 0, $sourceImage.Width, $sourceImage.Height)
        if ($FitSymbol) {
            # Fit the existing transparent mark into a square ICO without stretching it.
            $left = $sourceImage.Width; $top = $sourceImage.Height; $right = -1; $bottom = -1
            for ($y = 0; $y -lt $sourceImage.Height; $y++) {
                for ($x = 0; $x -lt $sourceImage.Width; $x++) {
                    if ($sourceImage.GetPixel($x, $y).A -gt 0) {
                        $left = [Math]::Min($left, $x); $right = [Math]::Max($right, $x)
                        $top = [Math]::Min($top, $y); $bottom = [Math]::Max($bottom, $y)
                    }
                }
            }
            if ($right -lt $left) { throw 'The icon source is empty.' }
            $sourceRect = [System.Drawing.Rectangle]::new($left, $top, $right - $left + 1, $bottom - $top + 1)
        }
        foreach ($width in $Widths) {
            $height = [int]($width / $Aspect)
            $frame = [System.Drawing.Bitmap]::new($width, $height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
            $graphics = [System.Drawing.Graphics]::FromImage($frame)
            $stream = [System.IO.MemoryStream]::new()
            try {
                $graphics.Clear([System.Drawing.Color]::Transparent)
                $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
                $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
                if ($FitSymbol) {
                    $padding = [Math]::Max(1, [int][Math]::Round($width / 32.0))
                    $scale = [Math]::Min(($width - 2 * $padding) / $sourceRect.Width, ($height - 2 * $padding) / $sourceRect.Height)
                    $drawWidth = [int][Math]::Round($sourceRect.Width * $scale)
                    $drawHeight = [int][Math]::Round($sourceRect.Height * $scale)
                    $targetRect = [System.Drawing.Rectangle]::new([int][Math]::Floor(($width - $drawWidth) / 2.0), [int][Math]::Floor(($height - $drawHeight) / 2.0), $drawWidth, $drawHeight)
                    $graphics.DrawImage($sourceImage, $targetRect, $sourceRect, [System.Drawing.GraphicsUnit]::Pixel)
                } else {
                    $graphics.DrawImage($sourceImage, [System.Drawing.Rectangle]::new(0, 0, $width, $height))
                }
                $frame.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
                $frames.Add(@{ Width = $width; Height = $height; Bytes = $stream.ToArray() })
            } finally { $stream.Dispose(); $graphics.Dispose(); $frame.Dispose() }
        }
        $file = [System.IO.File]::Create($Destination)
        $writer = [System.IO.BinaryWriter]::new($file)
        try {
            $writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write([uint16]$frames.Count)
            $offset = 6 + 16 * $frames.Count
            foreach ($frame in $frames) {
                $writer.Write([byte]($frame.Width % 256)); $writer.Write([byte]($frame.Height % 256))
                $writer.Write([byte]0); $writer.Write([byte]0)
                $writer.Write([uint16]1); $writer.Write([uint16]32)
                $writer.Write([uint32]$frame.Bytes.Length); $writer.Write([uint32]$offset)
                $offset += $frame.Bytes.Length
            }
            foreach ($frame in $frames) { $writer.Write([byte[]]$frame.Bytes) }
        } finally { $writer.Dispose(); $file.Dispose() }
    } finally { $sourceImage.Dispose() }
}

# Explorer, the title bar and the taskbar use the enlarged mark without text or a white tile.
Export-WindowsIcon (Join-Path $iconRoot 'src\assets\lengto-symbol.png') (Join-Path $iconAssets 'lengto.ico') @(16,20,24,32,40,48,64,96,128,256) 1 -FitSymbol
# The menu emblem uses only the set square and retains its generated alpha.
Export-WindowsIcon (Join-Path $iconRoot 'src\assets\lengto-symbol.png') (Join-Path $iconAssets 'squadretta.ico') @(16,24,32,40,48,64,96,128,256) 2
