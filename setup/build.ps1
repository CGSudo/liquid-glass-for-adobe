# Builds the one-click Windows installer: dist\LiquidGlass-for-Adobe-<ver>-Setup.exe.
# It rebuilds and packs the Photoshop, Illustrator and both After Effects plug-ins with the live engine tour.
#   powershell -ExecutionPolicy Bypass -File setup\build.ps1 [-WebView2 <unpacked Microsoft.Web.WebView2 NuGet>]
param([string]$WebView2 = "E:\AI\SDK\webview2\pkg")
$ErrorActionPreference = "Stop"
$here = $PSScriptRoot
$root = Split-Path $here
$ver = (Get-Content (Join-Path $root "photoshop\manifest.json") -Raw | ConvertFrom-Json).version
& node (Join-Path $here 'ui.test.js')
if ($LASTEXITCODE) { throw 'Setup UI self-check failed' }

& powershell -ExecutionPolicy Bypass -File (Join-Path $root "install.ps1") -NoInstall
if ($LASTEXITCODE) { throw "Photoshop packaging failed" }
& powershell -ExecutionPolicy Bypass -File (Join-Path $root "illustrator\build.ps1") -NoInstall
if ($LASTEXITCODE) { throw "Illustrator build failed" }
Copy-Item (Join-Path $root "illustrator\build\LiquidGlass.aip") (Join-Path $root "dist") -Force
& powershell -ExecutionPolicy Bypass -File (Join-Path $root "aftereffects\build.ps1") -NoInstall
if ($LASTEXITCODE) { throw "After Effects build failed" }

$obj = Join-Path $here "obj"
New-Item -ItemType Directory -Force $obj | Out-Null
# Icon: the logo PNG inside an .ico container (Windows reads PNG icons).
$png = [IO.File]::ReadAllBytes((Join-Path $root "photoshop\logo.png"))
$ico = New-Object IO.MemoryStream
$bw = New-Object IO.BinaryWriter $ico
$bw.Write([uint16]0); $bw.Write([uint16]1); $bw.Write([uint16]1)
$bw.Write([byte]0); $bw.Write([byte]0); $bw.Write([byte]0); $bw.Write([byte]0) # 0 = 256 or the PNG's own size
$bw.Write([uint16]1); $bw.Write([uint16]32); $bw.Write([uint32]$png.Length); $bw.Write([uint32]22); $bw.Write($png)
[IO.File]::WriteAllBytes((Join-Path $obj "icon.ico"), $ico.ToArray())

# Payload: RCDATA 101, 102, … in the order of the file list in setup.cpp's Extract().
$files = "setup\setup.html", "setup\glass-worker.js", "setup\stage.js", "photoshop\looks.js", "photoshop\engine.js", "photoshop\glass_core.wasm",
    "photoshop\logo.png", "dist\LiquidGlass-for-Adobe.ccx", "dist\LiquidGlass.aip",
    "aftereffects\build\LiquidGlass.aex", "aftereffects\build\LiquidGlassBrightness.aex", "photoshop\index.js"
$rc = "1 ICON `"$((Join-Path $obj 'icon.ico') -replace '\\', '\\')`"`n"
$id = 101
foreach ($f in $files) { $rc += "$id RCDATA `"$((Join-Path $root $f) -replace '\\', '\\')`"`n"; $id++ }
$rc += @"
1 VERSIONINFO
FILEVERSION $($ver -replace '\.', ','),0
PRODUCTVERSION $($ver -replace '\.', ','),0
BEGIN
  BLOCK "StringFileInfo"
  BEGIN
    BLOCK "040904B0"
    BEGIN
      VALUE "CompanyName", "CGSudo"
      VALUE "FileDescription", "Liquid Glass for Adobe Setup"
      VALUE "ProductName", "Liquid Glass for Adobe"
      VALUE "ProductVersion", "$ver"
      VALUE "FileVersion", "$ver"
      VALUE "LegalCopyright", "MIT License, CGSudo"
    END
  END
  BLOCK "VarFileInfo"
  BEGIN
    VALUE "Translation", 0x409, 1200
  END
END
"@
[IO.File]::WriteAllText((Join-Path $obj "setup.rc"), $rc, [Text.Encoding]::ASCII)

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vcvars = Join-Path (& $vswhere -latest -products * -property installationPath) "VC\Auxiliary\Build\vcvars64.bat"
$out = Join-Path $root "dist\LiquidGlass-for-Adobe-$ver-Setup.exe"
# Static C runtime (/MT): runs on a clean PC with no Visual C++ redistributable.
$cmd = "`"$vcvars`" >nul && cd /d `"$obj`" && rc /nologo /fo setup.res setup.rc && " +
    "cl /nologo /O2 /MT /EHsc /std:c++17 /W3 /utf-8 /DUNICODE /D_UNICODE /DLG_VERSION=L\`"$ver\`" /I`"$WebView2\build\native\include`" " +
    "`"$here\setup.cpp`" setup.res /Fe`"$out`" /link /SUBSYSTEM:WINDOWS `"$WebView2\build\native\x64\WebView2LoaderStatic.lib`" " +
    "user32.lib gdi32.lib shell32.lib ole32.lib dwmapi.lib advapi32.lib version.lib /MANIFESTUAC:`"level='asInvoker' uiAccess='false'`""
cmd /c $cmd
if ($LASTEXITCODE -or -not (Test-Path $out)) { throw "Setup build failed" }
& node (Join-Path $here 'payload.test.js')
if ($LASTEXITCODE) { throw 'Setup payload self-check failed' }
Write-Host "Built $out"
