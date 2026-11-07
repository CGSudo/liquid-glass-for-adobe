# Builds the shared LiquidGlass.aex and the AE-only LiquidGlassBrightness.aex from the same source.
#   powershell -ExecutionPolicy Bypass -File build.ps1 [-SDK <unpacked AE SDK Examples folder>] [-NoInstall]
param(
    [string]$SDK = "E:\AI\SDK\AE_SDK\AfterEffectsSDK\Examples",
    [switch]$NoInstall,
    [switch]$Test, # builds separate test identities and runs the brightness self-check; never installs
    [string]$AEPluginDir = "C:\Program Files\Adobe\Adobe After Effects 2026\Support Files\Plug-ins\CGSudo"
)
if ($Test) { $NoInstall = $true }
$ErrorActionPreference = "Stop"
$here = $PSScriptRoot
$out = Join-Path $here "build"
New-Item -ItemType Directory -Force $out | Out-Null

# Rust engine as a static lib (MSVC, /MD).
cargo build -p glass_core --release --manifest-path (Join-Path $here "..\Cargo.toml")
if ($LASTEXITCODE) { throw "cargo build failed" }

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -property installationPath
$inc = "/I`"$SDK\Headers`" /I`"$SDK\Headers\SP`" /I`"$SDK\Headers\Win`" /I`"$SDK\Resources`" /I`"$SDK\Util`" /I`"$here\Source`""
if ($Test) { $inc += " /DLG_TEST" }
$engine = Join-Path $here "..\target\release\glass_core.lib"
foreach ($name in @("LiquidGlass", "LiquidGlassBrightness")) {
    $variantInc = $inc
    if ($name -eq "LiquidGlassBrightness") { $variantInc += " /DLG_BRIGHTNESS" }
    # PiPL steps are the SDK's own: preprocess the .r, PiPLTool turns it into a resource script.
    $cmd = "`"$vs\VC\Auxiliary\Build\vcvars64.bat`" >nul && cd /d `"$out`" && " +
        "cl /nologo $variantInc /EP `"$here\Source\LiquidGlassPiPL.r`" > LiquidGlassPiPL.rr && " +
        "`"$SDK\Resources\PiPLTool.exe`" LiquidGlassPiPL.rr LiquidGlassPiPL.rrc && " +
        "cl /nologo /D MSWindows /EP LiquidGlassPiPL.rrc > LiquidGlassPiPL.rc && " +
        "rc /nologo /fo LiquidGlassPiPL.res LiquidGlassPiPL.rc && " +
        "cl /nologo /std:c++17 /EHsc /O2 /MD /W3 /DMSWindows /DWIN32 /D_WINDOWS /DNOMINMAX $variantInc /Fe:$name.aex " +
        "`"$here\Source\LiquidGlassAE.cpp`" `"$SDK\Util\AEFX_SuiteHelper.c`" `"$SDK\Util\MissingSuiteError.cpp`" LiquidGlassPiPL.res " +
        "`"$engine`" /link /DLL /NOLOGO kernel32.lib user32.lib ntdll.lib userenv.lib ws2_32.lib dbghelp.lib bcrypt.lib advapi32.lib"
    cmd /c $cmd
    if ($LASTEXITCODE) { throw "build failed" }
    Write-Host "Built $(Join-Path $out "$name.aex")"
}
if ($Test) {
    $check = "`"$vs\VC\Auxiliary\Build\vcvars64.bat`" >nul && cd /d `"$out`" && " +
        "cl /nologo /std:c++17 /EHsc /O2 /MD /DNOMINMAX /DLG_BRIGHTNESS /Fe:brightness_param_test.exe " +
        "`"$here\test\param_test.cpp`" `"$engine`" kernel32.lib user32.lib ntdll.lib userenv.lib ws2_32.lib dbghelp.lib bcrypt.lib advapi32.lib"
    cmd /c $check
    if ($LASTEXITCODE) { throw "brightness self-check build failed" }
    & (Join-Path $out "brightness_param_test.exe")
    if ($LASTEXITCODE) { throw "brightness self-check failed" }
}
if ($NoInstall) { return }

# Keep Brightness out of MediaCore so Premiere never discovers it.
foreach ($name in @("LiquidGlass", "LiquidGlassBrightness")) {
    $aex = Join-Path $out "$name.aex"
    $dest = if ($name -eq "LiquidGlass") { "C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\CGSudo" } else { $AEPluginDir }
    Start-Process powershell -WindowStyle Hidden -Verb RunAs -Wait -ArgumentList "-NoProfile -Command New-Item -ItemType Directory -Force '$dest' | Out-Null; Copy-Item -LiteralPath '$aex' -Destination '$dest' -Force"
    if ((Get-FileHash $aex).Hash -ne (Get-FileHash (Join-Path $dest "$name.aex")).Hash) { throw "install failed: close the apps, then run again" }
    Write-Host "Installed -> $dest"
}
