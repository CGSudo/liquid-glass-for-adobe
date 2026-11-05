# Starts Premiere Pro with verify_pr.jsx: effect listed, applied to a clip, keyframed, rendered to .mp4 and timed. See <Out>\pr.log.
#   powershell -ExecutionPolicy Bypass -File verify_pr.ps1 -Background <any PNG> [-Out <folder>]
param([Parameter(Mandatory)][string]$Background, [string]$Out = "$env:TEMP\lgtest\pr", [int]$TimeoutSec = 240)
$ErrorActionPreference = "Stop"
if (Get-Process "Adobe Premiere Pro" -ErrorAction SilentlyContinue) { throw "Premiere Pro is open: close it first, this test needs its own project" }
New-Item -ItemType Directory -Force $Out | Out-Null
Get-ChildItem $Out -File | Remove-Item -Force
Copy-Item $Background (Join-Path $Out "bg.png")
$env:LG_TEST_DIR = $Out
$p = Start-Process "C:\Program Files\Adobe\Adobe Premiere Pro 2026\Adobe Premiere Pro.exe" -ArgumentList "/C", "es.processFile", "$PSScriptRoot\verify_pr.jsx" -PassThru
$log = Join-Path $Out "pr.log"
$sw = [Diagnostics.Stopwatch]::StartNew()
while ($sw.Elapsed.TotalSeconds -lt $TimeoutSec -and -not $p.HasExited) {
    if ((Test-Path $log) -and (Select-String -LiteralPath $log -Pattern '^(done|ERROR)' -Quiet)) { break }
    Start-Sleep 3
}
Write-Host "pid $($p.Id), $([int]$sw.Elapsed.TotalSeconds) s"
if (Test-Path $log) { Get-Content $log -Encoding UTF8 } else { Write-Host "no log: the script did not run" }
