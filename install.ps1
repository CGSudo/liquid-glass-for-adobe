# Packages the Photoshop plugin as dist\LiquidGlass.ccx and installs it with Adobe's installer (UPIA).
#   powershell -ExecutionPolicy Bypass -File install.ps1 [-NoInstall]
# Illustrator is a separate native plug-in: see illustrator-native\build.ps1.
param([switch]$NoInstall)
$ErrorActionPreference = "Stop"
$root = $PSScriptRoot

# Photoshop: package the UXP plugin (a .ccx is a zip of the plugin folder).
$stage = Join-Path $env:TEMP ("lg-ccx-" + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $stage | Out-Null
Copy-Item (Join-Path $root "photoshop\*") $stage -Exclude *.test.js
New-Item -ItemType Directory -Force (Join-Path $root "dist") | Out-Null
$zip = Join-Path $root "dist\LiquidGlass-for-Adobe.zip"
Compress-Archive -Path "$stage\*" -DestinationPath $zip -Force
$ccx = [IO.Path]::ChangeExtension($zip, ".ccx")
Move-Item $zip $ccx -Force
if (-not [IO.Path]::GetFullPath($stage).StartsWith([IO.Path]::GetFullPath($env:TEMP).TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Unexpected staging path' }
Remove-Item -LiteralPath $stage -Recurse -Force
Write-Host "Packaged $ccx"
if ($NoInstall) { return }
$upia = "C:\Program Files\Common Files\Adobe\Adobe Desktop Common\RemoteComponents\UPI\UnifiedPluginInstallerAgent\UnifiedPluginInstallerAgent.exe"
if (Get-Process Photoshop -ErrorAction SilentlyContinue) { throw "Close Photoshop before installing. Nothing was changed." }
if (-not (Test-Path -LiteralPath $upia)) { throw "Creative Cloud desktop is required to install the Photoshop plug-in." }
# UPIA otherwise keeps stale files when reinstalling the same version; user presets live elsewhere.
& $upia /remove "Sudo Liquid Glass"
& $upia /remove "Liquid Glass for Adobe"
$external = Join-Path $env:APPDATA 'Adobe\UXP\Plugins\External'
if (Get-ChildItem -LiteralPath $external -Directory -ErrorAction SilentlyContinue | Where-Object { $_.Name -like 'com.liquidglass.photoshop_*' }) {
    throw "Creative Cloud could not remove the previous plug-in. Close Photoshop, restart Creative Cloud and retry."
}
& $upia /install $ccx
if ($LASTEXITCODE) { throw "Creative Cloud install failed ($LASTEXITCODE)." }

