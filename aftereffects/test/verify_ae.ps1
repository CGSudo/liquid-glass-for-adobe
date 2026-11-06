# Verifies the installed effect in the real After Effects: builds comps by script in a separate instance (-m), renders them with aerender.
#   powershell -ExecutionPolicy Bypass -File verify_ae.ps1 [-Out <folder>] [-Version 2026]
# A running After Effects with someone's project is left alone: the script checks LG_TEST_INSTANCE, which only this launch has.
param([string]$Out = "$env:TEMP\lgtest\ae", [string]$Version = "2026")
$ErrorActionPreference = "Stop"
$sf = "C:\Program Files\Adobe\Adobe After Effects $Version\Support Files"
New-Item -ItemType Directory -Force $Out | Out-Null
Get-ChildItem $Out -File | Remove-Item -Force
$env:LG_TEST_INSTANCE = "1"
$env:LG_TEST_DIR = $Out

$p = Start-Process "$sf\AfterFX.exe" -ArgumentList "-m", "-r", "$PSScriptRoot\make_test_project.jsx" -PassThru
$last = Join-Path $Out "lg_32bit.aepx"
$sw = [Diagnostics.Stopwatch]::StartNew()
while (-not (Test-Path $last) -and $sw.Elapsed.TotalSeconds -lt 180 -and -not $p.HasExited) { Start-Sleep 2 }
if (-not (Test-Path $last)) { throw "test project not written" }
# app.quit() and the Exit command do nothing after a -r launch, and WM_CLOSE is ignored; WM_QUIT to its UI thread ends it. Killing it would
# put up the Crash Repair prompt at the next launch.
Add-Type -Namespace LG -Name Win -MemberDefinition '[DllImport("user32.dll")] public static extern bool PostThreadMessage(uint t, uint m, IntPtr w, IntPtr l);
[DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, IntPtr p);'
$p.Refresh()
[LG.Win]::PostThreadMessage([LG.Win]::GetWindowThreadProcessId($p.MainWindowHandle, [IntPtr]::Zero), 0x12, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
if (-not $p.WaitForExit(30000)) { Write-Host "test instance $($p.Id) did not quit" }

foreach ($name in "lg_8bit", "lg_16bit", "lg_32bit") {
    $t = [Diagnostics.Stopwatch]::StartNew()
    & "$sf\aerender.exe" -project (Join-Path $Out "$name.aepx") -v ERRORS_AND_PROGRESS -log (Join-Path $Out "$name.log")
    Write-Host "$name exit $LASTEXITCODE in $([int]$t.Elapsed.TotalSeconds) s"
}
Start-Sleep 3 # aerender can return just before its last frame lands on disk
Add-Type -AssemblyName System.Drawing
$n = 0
foreach ($t in Get-ChildItem $Out -Filter *.tif) {
    try {
        $img = [Drawing.Image]::FromFile($t.FullName)
        $img.Save([IO.Path]::ChangeExtension($t.FullName, ".png"), [Drawing.Imaging.ImageFormat]::Png)
        $img.Dispose()
        $n++
    } catch { Write-Host "can't convert $($t.Name): $_" }
}
Write-Host "$n frames in $Out"

# Timing: wall clock of aerender on 120 frames at 1080p, effect on versus off. App start and file writing cancel out.
# shortcut: only the first mode measured is real, the second reads the first's disk cache and shows about +0 ms; to time both, render each with different settings.
foreach ($mfr in "ON", "OFF") {
    $sec = @{}
    foreach ($name in "t_off", "t_on", "t_full") {
        $t = [Diagnostics.Stopwatch]::StartNew()
        & "$sf\aerender.exe" -project (Join-Path $Out "$name.aepx") -mfr $mfr 100 -v ERRORS -log (Join-Path $Out "$name.log") | Out-Null
        $sec[$name] = $t.Elapsed.TotalSeconds
        Get-ChildItem $Out -Filter "$name*.tif" | Remove-Item -Force
    }
    Write-Host ("MFR {0}: off {1:n1} s, 800x400 glass {2:n1} s (+{3:n0} ms/frame), full-frame frosted with shadow {4:n1} s (+{5:n0} ms/frame)" -f $mfr, $sec.t_off, $sec.t_on, (($sec.t_on - $sec.t_off) / 120 * 1000), $sec.t_full, (($sec.t_full - $sec.t_off) / 120 * 1000))
}
