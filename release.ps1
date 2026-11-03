# Builds every release download into dist\release (run after committing: the source zips come from HEAD).
#   powershell -ExecutionPolicy Bypass -File release.ps1
# 1. All-in-one: Setup.exe (Windows, both apps) and an AIO zip for Windows + Mac.
# 2. Each plugin on its own: the Photoshop .ccx and the Illustrator .aip.
# 3. The source of each plugin on its own (each with the shared engine, so it builds by itself).
$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
$ver = (Get-Content (Join-Path $root "photoshop\manifest.json") -Raw | ConvertFrom-Json).version
$name = "LiquidGlass-for-Adobe"
$out = Join-Path $root "dist\release"

Push-Location $root
try {
    npm test
    if ($LASTEXITCODE) { throw "tests failed" }
    & powershell -ExecutionPolicy Bypass -File (Join-Path $root "setup\build.ps1")
    if ($LASTEXITCODE) { throw "Setup build failed" }

    Remove-Item $out -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory $out | Out-Null
    $stage = Join-Path $env:TEMP "lg-release"
    Remove-Item $stage -Recurse -Force -ErrorAction SilentlyContinue
    # .NET writes text without the BOM and line-ending surprises of PowerShell 5.1's text cmdlets.
    function Text($path, $s) { New-Item -ItemType Directory -Force (Split-Path $path) | Out-Null; [IO.File]::WriteAllText($path, ($s -replace "`r?`n", "`r`n")) }
    # Zips with forward-slash paths (tar = bsdtar on Windows 10+), so they unpack cleanly on a Mac too.
    function Zip($zip, $dir) { tar -a -c -f $zip -C $dir *; if ($LASTEXITCODE) { throw "zip failed: $zip" } }

    $setup = "$name-$ver-Setup.exe"
    $ccx = "$name-Photoshop-$ver.ccx"
    Copy-Item (Join-Path $root "dist\$setup") (Join-Path $out $setup)
    Copy-Item (Join-Path $root "dist\$name.ccx") (Join-Path $out $ccx)

    # All-in-one zip: Windows Setup + Mac Photoshop plugin.
    $aio = Join-Path $stage "aio"
    New-Item -ItemType Directory -Force (Join-Path $aio "Windows"), (Join-Path $aio "Mac") | Out-Null
    Copy-Item (Join-Path $out $setup) (Join-Path $aio "Windows")
    Copy-Item (Join-Path $out $ccx) (Join-Path $aio "Mac")
    Text (Join-Path $aio "READ ME.txt") @"
Liquid Glass for Adobe $ver by CGSudo
Apple's Liquid Glass effect for Adobe Photoshop and Illustrator. Free and open source (MIT).

WINDOWS (Photoshop + Illustrator)
  Close Photoshop and Illustrator, run Windows\$setup and click Install.
  Windows asks once for permission (Illustrator's Plug-ins folder needs it).
  If SmartScreen says "Windows protected your PC", click More info > Run anyway (Setup isn't code-signed yet).

MAC (Photoshop)
  Double-click Mac\$ccx. The Creative Cloud desktop app installs it.
  Illustrator for Mac isn't available yet.

THEN
  Photoshop: Plugins > Liquid Glass for Adobe, select shape or text layers, click Make Glass.
  Illustrator: select a shape and choose Effect > Stylize > Liquid Glass...

Website: https://cgsudo.github.io/liquid-glass-for-adobe/
Source and help: https://github.com/CGSudo/liquid-glass-for-adobe
Not affiliated with Adobe or Apple. Adobe, Photoshop and Illustrator are trademarks of Adobe.
"@
    Zip (Join-Path $out "$name-$ver-AIO-Windows-Mac.zip") $aio

    # Illustrator on its own.
    $ai = Join-Path $stage "ai"
    New-Item -ItemType Directory -Force $ai | Out-Null
    Copy-Item (Join-Path $root "dist\LiquidGlass.aip") $ai
    Text (Join-Path $ai "READ ME.txt") @"
Liquid Glass for Adobe ${ver}: the Illustrator plug-in (Windows, Illustrator 2026)

Easiest: use $setup instead; it installs this for you.

By hand:
  1. Close Illustrator.
  2. Create the folder  C:\Program Files\Adobe\Adobe Illustrator 2026\Plug-ins\Liquid Glass
  3. Copy LiquidGlass.aip into it (Windows asks for permission).
  4. Start Illustrator, select a shape and choose Effect > Stylize > Liquid Glass...

https://github.com/CGSudo/liquid-glass-for-adobe
"@
    Zip (Join-Path $out "$name-Illustrator-$ver-Windows.zip") $ai

    # Source of each plugin; both carry the engine (core) and the workspace so they build on their own.
    $common = "core", "preview", "Cargo.toml", "Cargo.lock", "LICENSE", "README.md", "AGENTS.md"
    git archive --format=zip --prefix="$name-Photoshop-$ver-source/" -o (Join-Path $out "$name-Photoshop-$ver-source.zip") HEAD `
        @common photoshop install.ps1 install.sh package.json package-lock.json
    git archive --format=zip --prefix="$name-Illustrator-$ver-source/" -o (Join-Path $out "$name-Illustrator-$ver-source.zip") HEAD `
        @common illustrator photoshop/looks.js photoshop/logo.png
    if ($LASTEXITCODE) { throw "git archive failed" }

    Get-ChildItem $out | Select-Object Name, Length | Format-Table -AutoSize
} finally {
    Pop-Location
}
