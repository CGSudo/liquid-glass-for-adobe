# Builds LiquidGlass.aex (one effect for After Effects and Premiere Pro) and installs it where both apps load it.
#   powershell -ExecutionPolicy Bypass -File build.ps1 [-SDK <unpacked AE SDK Examples folder>] [-NoInstall]
param(
    [string]$SDK = "E:\AI\SDK\AE_SDK\AfterEffectsSDK\Examples",
    [switch]$NoInstall,
    [switch]$Test # builds the separate "Liquid Glass (test)" effect; never installs
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
# PiPL steps are the SDK's own: preprocess the .r, PiPLTool turns it into a resource script.
$cmd = "`"$vs\VC\Auxiliary\Build\vcvars64.bat`" >nul && cd /d `"$out`" && " +
    "cl /nologo $inc /EP `"$here\Source\LiquidGlassPiPL.r`" > LiquidGlassPiPL.rr && " +
    "`"$SDK\Resources\PiPLTool.exe`" LiquidGlassPiPL.rr LiquidGlassPiPL.rrc && " +
    "cl /nologo /D MSWindows /EP LiquidGlassPiPL.rrc > LiquidGlassPiPL.rc && " +
    "rc /nologo /fo LiquidGlassPiPL.res LiquidGlassPiPL.rc && " +
    "cl /nologo /std:c++17 /EHsc /O2 /MD /W3 /DMSWindows /DWIN32 /D_WINDOWS /DNOMINMAX $inc /Fe:LiquidGlass.aex " +
    "`"$here\Source\LiquidGlassAE.cpp`" `"$SDK\Util\AEFX_SuiteHelper.c`" `"$SDK\Util\MissingSuiteError.cpp`" LiquidGlassPiPL.res " +
    "`"$engine`" /link /DLL /NOLOGO kernel32.lib user32.lib ntdll.lib userenv.lib ws2_32.lib dbghelp.lib bcrypt.lib advapi32.lib"
cmd /c $cmd
if ($LASTEXITCODE) { throw "build failed" }
$aex = Join-Path $out "LiquidGlass.aex"
Write-Host "Built $aex"
if ($NoInstall) { return }

# Both apps load MediaCore. Program Files needs admin: copy through one elevated process (UAC prompt).
$dest = "C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\CGSudo"
Start-Process powershell -Verb RunAs -Wait -ArgumentList "-NoProfile -Command New-Item -ItemType Directory -Force '$dest' | Out-Null; Copy-Item -LiteralPath '$aex' -Destination '$dest' -Force"
if ((Get-FileHash $aex).Hash -ne (Get-FileHash (Join-Path $dest "LiquidGlass.aex")).Hash) { throw "install failed: close After Effects and Premiere Pro, then run again" }
Write-Host "Installed -> $dest"
