param(
    [string]$PdfiumPath,
    [switch]$Test
)
$ErrorActionPreference = 'Stop'
$taskRoot = $PSScriptRoot
$taskBuild = Join-Path $taskRoot 'build'
$taskDist = Join-Path $taskRoot 'portable\lengto-single'
New-Item -ItemType Directory -Force -Path $taskBuild, $taskDist | Out-Null

# Import a local MSVC x64 toolchain without changing machine/user settings.
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    $taskVsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $taskVsWhere)) { throw 'Visual Studio Build Tools with C/C++ tools is required to build the application.' }
    $taskVs = & $taskVsWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $taskVs) { throw 'MSVC C compiler not found.' }
    $taskVcVars = Join-Path $taskVs 'VC\Auxiliary\Build\vcvars64.bat'
    $taskEnvCmd = Join-Path $taskBuild 'compiler-env.cmd'
    @("@call `"$taskVcVars`" >nul", '@set') | Set-Content -LiteralPath $taskEnvCmd -Encoding ASCII
    $taskEnv = & $env:ComSpec /d /c $taskEnvCmd
    if ($LASTEXITCODE -ne 0) { throw 'Cannot initialize MSVC.' }
    foreach ($taskLine in $taskEnv) {
        if ($taskLine -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process') }
    }
    # Some hosts provide both PATH and Path; retain the compiler's expanded PATH.
    $taskCompilerPath = $taskEnv | Where-Object { $_ -cmatch '^PATH=' } | Select-Object -First 1
    if ($taskCompilerPath) { $env:Path = $taskCompilerPath.Substring(5) }
}

$taskPdfium = if ($PdfiumPath) { (Resolve-Path -LiteralPath $PdfiumPath).Path } else { Join-Path $taskBuild 'pdfium.dll' }
if (-not (Test-Path -LiteralPath $taskPdfium)) { $taskPdfium = Join-Path $taskRoot 'portable\lengto\pdfium.dll' }
if (-not (Test-Path -LiteralPath $taskPdfium)) { throw 'Pass -PdfiumPath with the PDFium x64 DLL to embed.' }
$taskExpectedHash = (Get-Content -LiteralPath (Join-Path $taskRoot 'third_party\pdfium-sha256.txt') -Raw).Trim()
if ((Get-FileHash -LiteralPath $taskPdfium -Algorithm SHA256).Hash -ne $taskExpectedHash) { throw 'The PDFium DLL does not match the bundled version and licenses.' }
$taskLicenseParts = @('lengto - Included component licenses and provenance')
$taskProjectLicense = Join-Path $taskRoot 'LICENSE'
if (Test-Path -LiteralPath $taskProjectLicense) {
    $taskLicenseParts += "`r`n----- lengto license -----`r`n" + (Get-Content -LiteralPath $taskProjectLicense -Raw -Encoding UTF8)
}
foreach ($taskLicense in (Get-ChildItem -LiteralPath (Join-Path $taskRoot 'third_party') -File -Recurse | Sort-Object FullName)) {
    $taskLicenseParts += "`r`n----- $($taskLicense.Name) -----`r`n" + (Get-Content -LiteralPath $taskLicense.FullName -Raw -Encoding UTF8)
}
$taskLicenseText = ($taskLicenseParts -join "`r`n").Replace("`r`n", "`n").Replace("`n", "`r`n")
$taskLicenses = Join-Path $taskBuild 'embedded-licenses.txt'
[System.IO.File]::WriteAllText($taskLicenses, $taskLicenseText, [System.Text.UTF8Encoding]::new($false))
$taskGuide = Join-Path $taskBuild 'embedded-guide.txt'
# Native multiline edit controls require CRLF to display paragraph breaks.
$taskGuideText = (Get-Content -LiteralPath (Join-Path $taskRoot 'GUIDE.txt') -Raw -Encoding UTF8).Replace("`r`n", "`n").Replace("`n", "`r`n")
[System.IO.File]::WriteAllText($taskGuide, $taskGuideText, [System.Text.UTF8Encoding]::new($false))
$taskPythonApi = Join-Path $taskRoot 'src\python_api.py'
$taskAppIcon = Join-Path $taskRoot 'src\assets\lengto.ico'
$taskMenuIcon = Join-Path $taskRoot 'src\assets\squadretta.ico'
$taskResource = Join-Path $taskBuild 'lengto.rc'
$taskPackSource = Join-Path $taskRoot 'tools\pack_resource.c'
$taskPacker = Join-Path $taskBuild 'pack-resource.exe'
& cl.exe /nologo /TC /std:c17 /O1 /MT /W4 /utf-8 /DUNICODE /D_UNICODE "/Fo:$taskBuild\pack-resource.obj" $taskPackSource "/Fe:$taskPacker" /link cabinet.lib
if ($LASTEXITCODE -ne 0) { throw 'Cannot build the resource packer.' }
$taskPackedResources = @('1 ICON "' + $taskAppIcon.Replace('\','\\') + '"', '2 ICON "' + $taskMenuIcon.Replace('\','\\') + '"')
$taskResourceId = 101
foreach ($taskInputResource in @($taskPdfium, $taskLicenses, $taskGuide, $taskPythonApi)) {
    $taskPackedResource = Join-Path $taskBuild ("resource-$taskResourceId.lzms")
    & $taskPacker 5 $taskInputResource $taskPackedResource
    if ($LASTEXITCODE -ne 0) { throw 'Resource compression or verification failed.' }
    $taskPackedResources += "$taskResourceId RCDATA `"" + $taskPackedResource.Replace('\','\\') + '"'
    $taskResourceId++
}
$taskPackedResources | Set-Content -LiteralPath $taskResource -Encoding UTF8
$taskSource = @('src\lengto.c','src\model.c','src\document.c','src\render.c','src\portable.c','src\script_json.c','src\automation.c','src\recorder.c','src\script_runner.c') | ForEach-Object { Join-Path $taskRoot $_ }
$taskFlags = @('/nologo','/TC','/std:c17','/O1','/GL','/MT','/W4','/utf-8','/DUNICODE','/D_UNICODE','/DWIN32_LEAN_AND_MEAN','/D_WIN32_WINNT=0x0A00')
$taskLibraries = @('user32.lib','gdi32.lib','comctl32.lib','comdlg32.lib','shell32.lib','ole32.lib','oleaut32.lib','windowscodecs.lib','uuid.lib','cabinet.lib')
Push-Location $taskBuild
try {
    & rc.exe /nologo /c65001 /fo lengto.res $taskResource
    if ($LASTEXITCODE -ne 0) { throw 'Cannot embed resources.' }
    & cl.exe @taskFlags @taskSource "/Fe:$taskDist\lengto.exe" /link lengto.res /LTCG /OPT:REF /OPT:ICF /MANIFEST:EMBED /SUBSYSTEM:WINDOWS @taskLibraries
    if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
    if ($Test) {
        $taskTestSources = @('tests\test.c','src\model.c','src\document.c','src\render.c','src\portable.c','src\script_json.c','src\automation.c','src\recorder.c','src\script_runner.c') | ForEach-Object { Join-Path $taskRoot $_ }
        & cl.exe @taskFlags @taskTestSources "/Fe:$taskBuild\tests.exe" /link lengto.res /LTCG /OPT:REF /OPT:ICF /SUBSYSTEM:CONSOLE @taskLibraries
        if ($LASTEXITCODE -ne 0) { throw 'Test build failed.' }
        & (Join-Path $taskBuild 'tests.exe') (Join-Path $taskRoot 'tests\fixtures') $taskBuild
        if ($LASTEXITCODE -ne 0) { throw 'Tests failed.' }
    }
} finally { Pop-Location }
Write-Host "Portable application ready: $taskDist"
