# Builds the shared LiquidGlass.aex and the AE-only LiquidGlassBrightness.aex from the same source.
#   powershell -ExecutionPolicy Bypass -File build.ps1 [-SDK <unpacked AE SDK Examples folder>] [-NoInstall]
param(
    [string]$SDK = "E:\AI\SDK\AE_SDK\AfterEffectsSDK\Examples",
    [switch]$NoInstall,
    [switch]$Test, # builds separate test identities and runs the brightness self-check; never installs
    [string[]]$AEPluginDir = @(),
    [switch]$CopyOnly
)
if ($Test) { $NoInstall = $true }
$ErrorActionPreference = "Stop"
$here = $PSScriptRoot
$out = Join-Path $here "build"
if ($CopyOnly) {
    if ($Test -or $NoInstall) { throw 'CopyOnly cannot be combined with Test or NoInstall.' }
    if (-not $AEPluginDir.Count) { throw 'CopyOnly needs at least one After Effects destination.' }
    $ErrorActionPreference = 'Stop'
    if (Get-Process AfterFX,aerender,'Adobe Premiere Pro' -ErrorAction SilentlyContinue) { throw 'Close After Effects, background renders and Premiere Pro. Nothing was changed.' }
    $copies = @()
    foreach ($dest in $AEPluginDir) {
        $full = [IO.Path]::GetFullPath($dest).TrimEnd('\')
        if ($full -notmatch '\\Support Files\\Plug-ins\\CGSudo$') { throw 'Destination must be an After Effects Support Files\Plug-ins\CGSudo folder.' }
        foreach ($name in @('LiquidGlass.aex', 'LiquidGlassBrightness.aex')) {
            $copies += [pscustomobject]@{ Source = Join-Path $out $name; Target = Join-Path $full $name }
        }
    }
    $old = Join-Path $env:ProgramFiles 'Adobe\Common\Plug-ins\7.0\MediaCore\CGSudo\LiquidGlass.aex'
    $targets = @($copies | ForEach-Object { $_.Target }) + @($old)
    foreach ($path in $targets) {
        for ($dir = Split-Path $path; $dir.Length -gt 3; $dir = Split-Path $dir) {
            if ((Test-Path -LiteralPath $dir) -and ((Get-Item -LiteralPath $dir).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw "Redirected plug-in folder: $dir. Nothing was changed." }
        }
        if (Test-Path -LiteralPath $path) {
            $attrs = (Get-Item -LiteralPath $path).Attributes
            if ($attrs -band ([IO.FileAttributes]::ReparsePoint -bor [IO.FileAttributes]::ReadOnly -bor [IO.FileAttributes]::Directory)) { throw "Unsafe or read-only plug-in path: $path. Nothing was changed." }
            $stream = [IO.File]::Open($path, 'Open', 'ReadWrite', 'None')
            $stream.Dispose()
        }
    }
    $suffix = '.lg-setup-' + [guid]::NewGuid().ToString('N')
    $staged = @(); $saved = @(); $installed = @()
    try {
        foreach ($copy in $copies) {
            New-Item -ItemType Directory -Force (Split-Path $copy.Target) | Out-Null
            $staged += $copy.Target + $suffix + '.new'
            Copy-Item -LiteralPath $copy.Source -Destination ($copy.Target + $suffix + '.new')
        }
        if (Get-Process AfterFX,aerender,'Adobe Premiere Pro' -ErrorAction SilentlyContinue) { throw 'Close Adobe video apps and retry.' }
        foreach ($path in $targets) {
            if (Test-Path -LiteralPath $path) {
                Move-Item -LiteralPath $path -Destination ($path + $suffix)
                $saved += $path
            }
        }
        foreach ($copy in $copies) {
            Move-Item -LiteralPath ($copy.Target + $suffix + '.new') -Destination $copy.Target
            $installed += $copy.Target
            if ((Get-FileHash -LiteralPath $copy.Source).Hash -ne (Get-FileHash -LiteralPath $copy.Target).Hash) { throw "File verification failed: $($copy.Target)" }
        }
    } catch {
        foreach ($path in $installed) { Remove-Item -LiteralPath $path -Force }
        foreach ($path in $saved) { Move-Item -LiteralPath ($path + $suffix) -Destination $path }
        throw
    } finally {
        foreach ($path in $staged) { if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Force } }
    }
    foreach ($path in $saved) { Remove-Item -LiteralPath ($path + $suffix) -Force }
    if (Test-Path -LiteralPath (Split-Path $old)) {
        if (-not (Get-ChildItem -LiteralPath (Split-Path $old) -Force)) { [IO.Directory]::Delete((Split-Path $old)) }
    }
    return
}
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
        "`"$here\test\param_test.cpp`" `"$engine`" kernel32.lib user32.lib gdi32.lib ntdll.lib userenv.lib ws2_32.lib dbghelp.lib bcrypt.lib advapi32.lib"
    cmd /c $check
    if ($LASTEXITCODE) { throw "brightness self-check build failed" }
    & (Join-Path $out "brightness_param_test.exe")
    if ($LASTEXITCODE) { throw "brightness self-check failed" }
}
if ($NoInstall) { return }

# Both release effects stay in AE's own folders; Premiere installation is not verified.
if (-not $AEPluginDir.Count) {
    $apps = @(Get-ChildItem -Path "$env:ProgramFiles\Adobe\Adobe After Effects *" -Directory -ErrorAction SilentlyContinue | Where-Object {
        $_.Name -match '^Adobe After Effects (20\d{2})(?: |$)' -and [int]$Matches[1] -ge 2024
    } | ForEach-Object { $_.FullName })
    foreach ($hive in @('HKLM:', 'HKCU:')) {
        foreach ($key in Get-ChildItem "$hive\SOFTWARE\Adobe\After Effects" -ErrorAction SilentlyContinue) {
            if ($key.PSChildName -match '^(\d{2})(?:\.|$)' -and [int]$Matches[1] -ge 24) {
                $app = (Get-ItemProperty -LiteralPath $key.PSPath -Name InstallPath -ErrorAction SilentlyContinue).InstallPath
                if ($app) { $apps += $app.TrimEnd('\') -replace '\\Support Files$', '' }
            }
        }
    }
    $AEPluginDir = @($apps | Sort-Object -Unique | Where-Object { Test-Path -LiteralPath (Join-Path $_ 'Support Files\AfterFX.exe') } | ForEach-Object { Join-Path $_ 'Support Files\Plug-ins\CGSudo' })
}
if (-not $AEPluginDir.Count) { throw 'After Effects 2024+ was not found. Built files were not installed.' }
if (Get-Process AfterFX,aerender,'Adobe Premiere Pro' -ErrorAction SilentlyContinue) { throw 'Close Adobe video apps before installing. Nothing was changed.' }
function Quote($value) { "'" + $value.Replace("'", "''") + "'" }
$command = "& $(Quote $PSCommandPath) -CopyOnly -AEPluginDir @(" + (($AEPluginDir | ForEach-Object { Quote $_ }) -join ',') + ')'
$encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($command))
$copyProcess = Start-Process powershell -WindowStyle Hidden -Verb RunAs -Wait -PassThru -ArgumentList "-NoProfile -ExecutionPolicy Bypass -EncodedCommand $encoded"
if ($copyProcess.ExitCode) { throw 'Install failed: close Adobe apps and check destination permissions, then retry.' }
foreach ($dest in $AEPluginDir) {
    foreach ($name in @('LiquidGlass.aex', 'LiquidGlassBrightness.aex')) {
        if ((Get-FileHash -LiteralPath (Join-Path $out $name)).Hash -ne (Get-FileHash -LiteralPath (Join-Path $dest $name)).Hash) { throw "Install verification failed: $dest\$name" }
    }
    Write-Host "Installed -> $dest"
}
