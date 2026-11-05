# Builds and runs param_test.exe (needs VS C++ Build Tools; no Adobe SDK or app). Run a cargo release build first, or build.ps1.
$ErrorActionPreference = "Stop"
cargo build -p glass_core --release --manifest-path "$PSScriptRoot\..\..\Cargo.toml"
if ($LASTEXITCODE) { throw "cargo build failed" }
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -property installationPath
$out = "$PSScriptRoot\..\build\test"
New-Item -ItemType Directory -Force $out | Out-Null
$cmd = "`"$vs\VC\Auxiliary\Build\vcvars64.bat`" >nul && cd /d `"$PSScriptRoot`" && " +
  "cl /nologo /std:c++17 /EHsc /O2 /MD /DNOMINMAX /Fo`"$out/`" /Fe`"$out\param_test.exe`" param_test.cpp " +
  "`"$PSScriptRoot\..\..\target\release\glass_core.lib`" kernel32.lib user32.lib ntdll.lib userenv.lib ws2_32.lib dbghelp.lib bcrypt.lib advapi32.lib"
cmd /c $cmd
if ($LASTEXITCODE) { throw "build failed" }
& "$out\param_test.exe"
exit $LASTEXITCODE
