# Builds every release download into dist\release (run after committing: the source zips come from HEAD).
#   powershell -ExecutionPolicy Bypass -File release.ps1
# 1. All-in-one: Setup.exe (Windows, three apps) and an AIO zip for Windows + Mac.
# 2. Each plugin on its own: Photoshop .ccx, Illustrator .aip and the two After Effects .aex files.
# 3. The source of each plugin on its own (each with the shared engine, so it builds by itself).
$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
$ver = (Get-Content (Join-Path $root "photoshop\manifest.json") -Raw | ConvertFrom-Json).version
$name = "LiquidGlass-for-Adobe"
$out = Join-Path $root "dist\release"

Push-Location $root
try {
    npm.cmd test
    if ($LASTEXITCODE) { throw "tests failed" }
    & powershell -ExecutionPolicy Bypass -File (Join-Path $root "setup\build.ps1")
    if ($LASTEXITCODE) { throw "Setup build failed" }

    if ([IO.Path]::GetFullPath($out) -ne [IO.Path]::GetFullPath((Join-Path $root 'dist\release'))) { throw 'Unexpected release output path' }
    if (Test-Path -LiteralPath $out) { Remove-Item -LiteralPath $out -Recurse -Force }
    New-Item -ItemType Directory $out | Out-Null
    $stage = Join-Path $env:TEMP ("lg-release-" + [guid]::NewGuid().ToString('N'))
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
Live Liquid Glass for Photoshop, Illustrator and After Effects. Free and open source (MIT).

WINDOWS (Photoshop + Illustrator + After Effects 2024+)
  Close Photoshop, Illustrator, After Effects and background renders, run Windows\$setup and click Install.
  Setup installs only into apps it finds. After Effects receives Liquid Glass and Liquid Glass Brightness.
  Windows asks once for permission to update native plug-in folders.
  Older copies of our plug-ins are replaced. Presets, logs and saved looks are kept.
  Close Premiere Pro too if an older shared Liquid Glass effect is installed: Setup removes that old copy.
  This release does not install into Premiere Pro or MediaCore. Tested on Windows.
  If SmartScreen says "Windows protected your PC", click More info > Run anyway (Setup isn't code-signed yet).

MAC (Photoshop)
  Double-click Mac\$ccx. The Creative Cloud desktop app installs it.
  Illustrator for Mac isn't available yet.
  After Effects for Mac isn't available yet.

THEN
  Photoshop: Plugins > Liquid Glass for Adobe, select shape or text layers, click Make Glass.
  Illustrator: select a shape and choose Effect > Stylize > Liquid Glass...
  After Effects: Effects > CGSudo > Liquid Glass, on a shape or text layer.
  Liquid Glass Brightness is the second After Effects effect, also under CGSudo.

Website: https://cgsudo.github.io/liquid-glass-for-adobe/
Source and help: https://github.com/CGSudo/liquid-glass-for-adobe
Not affiliated with Adobe or Apple. Adobe, Photoshop, Illustrator and After Effects are trademarks of Adobe.
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

    # After Effects on its own; these were rebuilt without LG_TEST by setup/build.ps1.
    $ae = Join-Path $stage 'ae'
    New-Item -ItemType Directory -Force $ae | Out-Null
    Copy-Item -LiteralPath (Join-Path $root 'aftereffects\build\LiquidGlass.aex'), (Join-Path $root 'aftereffects\build\LiquidGlassBrightness.aex') -Destination $ae
    Text (Join-Path $ae 'READ ME.txt') @"
Liquid Glass for Adobe ${ver}: After Effects effects (Windows, After Effects 2024+)

Easiest: use $setup; it finds installed After Effects versions and safely replaces older copies.

By hand:
  1. Close After Effects and background renders. Close Premiere Pro if it has an older shared copy loaded.
  2. Remove only the old LiquidGlass.aex from Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\CGSudo, if present.
  3. Copy both .aex files into each After Effects 2024+ installation's Support Files\Plug-ins\CGSudo folder.
  4. Start After Effects. Find Liquid Glass and Liquid Glass Brightness under Effects > CGSudo.
  Apply Liquid Glass directly to a shape or text layer; its alpha becomes the glass outline.
  Liquid Glass Brightness uses a raster or pre-comp layer's brightness and alpha.

Do not copy into MediaCore or CGSudoTest. This release does not install into Premiere Pro.
Presets, logs and saved looks in %APPDATA%\Sudo Liquid Glass must be kept. Tested on Windows.

https://github.com/CGSudo/liquid-glass-for-adobe
"@
    Zip (Join-Path $out "$name-AfterEffects-$ver-Windows.zip") $ae

    # Source of each plugin carries the engine and workspace; explicit tracked paths exclude Adobe SDKs.
    $common = "core", "preview", "Cargo.toml", "Cargo.lock", "LICENSE", "README.md", "AGENTS.md"
    git archive --format=zip --prefix="$name-Photoshop-$ver-source/" -o (Join-Path $out "$name-Photoshop-$ver-source.zip") HEAD `
        @common photoshop install.ps1 install.sh package.json package-lock.json
    if ($LASTEXITCODE) { throw "Photoshop source archive failed" }
    git archive --format=zip --prefix="$name-Illustrator-$ver-source/" -o (Join-Path $out "$name-Illustrator-$ver-source.zip") HEAD `
        @common illustrator photoshop/looks.js photoshop/logo.png
    if ($LASTEXITCODE) { throw "git archive failed" }
    git archive --format=zip --prefix="$name-AfterEffects-$ver-source/" -o (Join-Path $out "$name-AfterEffects-$ver-source.zip") HEAD `
        @common aftereffects photoshop/looks.js photoshop/logo.png setup/InstallPaths.h
    if ($LASTEXITCODE) { throw "After Effects source archive failed" }

    Get-ChildItem $out | Select-Object Name, Length | Format-Table -AutoSize
} finally {
    if ($stage -and (Test-Path -LiteralPath $stage)) {
        $tempRoot = [IO.Path]::GetFullPath($env:TEMP).TrimEnd('\') + '\'
        if (-not [IO.Path]::GetFullPath($stage).StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unexpected temporary path' }
        Remove-Item -LiteralPath $stage -Recurse -Force
    }
    Pop-Location
}
