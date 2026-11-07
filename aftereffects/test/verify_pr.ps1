# Generates fixtures, builds a disposable Premiere sequence and measures exports; never installs an effect.
param(
    [string]$Out = "$env:TEMP\lgtest\pr",
    [string]$Premiere = "C:\Program Files\Adobe\Adobe Premiere Pro 2026\Adobe Premiere Pro.exe",
    [string]$FFmpeg = "C:\Program Files\Shutter Encoder\app\Library\ffmpeg.exe",
    [string]$EffectName = "Liquid Glass (test)",
    [int]$TimeoutSec = 480,
    [switch]$PrepareOnly,
    [switch]$MeasureOnly
)
$ErrorActionPreference = "Stop"
if (!$PrepareOnly -and !$MeasureOnly -and (Get-Process "Adobe Premiere Pro" -ErrorAction SilentlyContinue)) {
    throw "Premiere Pro is open: close it first. This test creates its own project."
}
if (!$PrepareOnly -and !(Test-Path -LiteralPath $FFmpeg)) { throw "Supply -FFmpeg with an existing ffmpeg.exe; this test installs nothing." }
if (!$MeasureOnly) {
    $Out = Join-Path $Out ("run-" + [DateTime]::Now.ToString("yyyyMMdd-HHmmss-fff"))
    New-Item -ItemType Directory -Path $Out -Force | Out-Null
    $Out = (Resolve-Path -LiteralPath $Out).Path
    Add-Type -AssemblyName System.Drawing
    foreach ($name in @("gradient", "grey", "blue", "red", "empty", "stripe")) {
        $bmp = New-Object Drawing.Bitmap(320, 180)
        for ($y = 0; $y -lt 180; $y++) { for ($x = 0; $x -lt 320; $x++) {
            $v = [int](55 + 145 * $x / 319)
            $colour = [Drawing.Color]::Transparent
            if ($name -eq "gradient") { $colour = [Drawing.Color]::FromArgb(255, $v, $v, $v) }
            if ($name -eq "grey") { $colour = [Drawing.Color]::FromArgb(255, 128, 128, 128) }
            $inside = $y -ge 45 -and $y -lt 135 -and (($x -ge 65 -and $x -lt 155) -or ($x -ge 165 -and $x -lt 255))
            if ($inside -and $name -eq "blue") { $colour = [Drawing.Color]::FromArgb(255, 0, 0, 255) }
            if ($inside -and $name -eq "red") { $colour = [Drawing.Color]::FromArgb(255, 255, 0, 0) }
            if ($name -eq "stripe" -and $x -ge 110 -and $x -lt 140) { $colour = [Drawing.Color]::FromArgb(255, 220, 220, 220) }
            $bmp.SetPixel($x, $y, $colour)
        } }
        $bmp.Save((Join-Path $Out "$name.png"), [Drawing.Imaging.ImageFormat]::Png)
        $bmp.Dispose()
    }
    # Keep the script beside fixtures so a failed run stays inspectable and previous exports survive.
    $jsx = @'
(function () {
    var dir = $.getenv("LG_TEST_DIR"), effectName = $.getenv("LG_TEST_EFFECT");
    function log(s) { var f = new File(dir + "/pr.log"); f.encoding = "UTF-8"; f.open("a"); f.writeln(s); f.close(); }
    function need(ok, msg) { if (!ok) throw new Error(msg); }
    try {
        app.enableQE();
        var fx = qe.project.getVideoEffectByName(effectName);
        need(fx, "Install the reviewed test build separately before running; missing " + effectName);
        app.newProject(new File(dir + "/glass-alpha.prproj").fsName);
        var names = ["gradient", "grey", "blue", "red", "empty", "stripe"], media = {};
        for (var i = 0; i < names.length; i++) {
            need(app.project.importFiles([dir + "/" + names[i] + ".png"], true, app.project.rootItem, false), "import " + names[i]);
            for (var j = 0; j < app.project.rootItem.children.numItems; j++) {
                var p = app.project.rootItem.children[j];
                if (p.name === names[i] + ".png") media[names[i]] = p;
            }
            need(media[names[i]], "missing fixture " + names[i]);
        }
        var preset = "C:/Program Files/Adobe/Adobe Premiere Pro 2026/MediaIO/systempresets/3F3F3F3F_4D6F6F56/H264 Match Source - High bitrate.epr";
        need(new File(preset).exists, "Missing export preset: " + preset);
        function effect(seq, trackIndex) {
            app.project.activeSequence = seq;
            var qs = qe.project.getActiveSequence(), qt = qs.getVideoTrackAt(trackIndex), item = null;
            for (var i = 0; i < qt.numItems; i++) { var q = qt.getItemAt(i); if (q && q.type === "Clip") { item = q; break; } }
            need(item, "QE clip missing on V" + (trackIndex + 1));
            item.addVideoEffect(fx);
            var cc = seq.videoTracks[trackIndex].clips[0].components;
            for (var c = 0; c < cc.numItems; c++) if (cc[c].displayName === effectName) return cc[c];
            throw new Error("Effect not applied");
        }
        function set(comp, label, value, required) {
            for (var i = 0; i < comp.properties.numItems; i++) {
                if (comp.properties[i].displayName === label) { comp.properties[i].setValue(value, true); return true; }
            }
            need(!required, "Missing control: " + label); return false;
        }
        function render(name, source, glass, grey, stripe, trackMask) {
            var seq = app.project.createNewSequenceFromClips(name, [media[grey ? "grey" : "gradient"]], app.project.rootItem);
            need(seq && seq.videoTracks.numTracks >= 3, "Need three video tracks in the sequence preset");
            app.project.activeSequence = seq;
            if (stripe) seq.videoTracks[1].overwriteClip(media.stripe, "0");
            if (source) seq.videoTracks[2].overwriteClip(media[source], "0");
            if (glass) {
                var comp = effect(seq, trackMask || !source ? 0 : 2);
                set(comp, "Dispersion", 0, true);
                set(comp, "Merge", 24, true);
                set(comp, "Liquid letters", false, false);
                if (trackMask) {
                    set(comp, "Glass from", 3, true);
                    seq.videoTracks[2].setMute(1);
                }
            }
            seq.setInPoint(0); seq.setOutPoint(1);
            var result = seq.exportAsMediaDirect(new File(dir + "/" + name + ".mp4").fsName, preset, 1);
            need(new File(dir + "/" + name + ".mp4").exists, "Export failed " + name + ": " + result);
            log("export " + name + " " + result);
        }
        render("plain", null, false, false, false, false);
        render("source", "blue", false, false, false, false);
        render("glass", "blue", true, false, false, false);
        render("red-glass", "red", true, false, false, false);
        render("stripe-glass", "blue", true, false, true, false);
        render("grey-glass", "blue", true, true, false, false);
        render("empty", "empty", true, false, false, false);
        render("opaque", null, true, false, false, false);
        // This isolates track checkout from adjustment creation using a clean opaque input clip.
        render("track-glass", "blue", true, false, false, true);
        render("track-red", "red", true, false, false, true);
        render("track-empty", "empty", true, false, false, true);
        app.project.save();
        log("done");
        app.quit();
    } catch (e) { log("ERROR line " + e.line + ": " + e.toString()); }
})();
'@
    [IO.File]::WriteAllText((Join-Path $Out "verify.jsx"), $jsx, (New-Object Text.UTF8Encoding($false)))
    Write-Host "Fixtures and script: $Out"
    if ($PrepareOnly) { return }
    $env:LG_TEST_DIR = $Out
    $env:LG_TEST_EFFECT = $EffectName
    $p = Start-Process -FilePath $Premiere -ArgumentList @('/C', 'es.processFile', ('"' + (Join-Path $Out 'verify.jsx') + '"')) -WorkingDirectory $Out -WindowStyle Hidden -PassThru
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $log = Join-Path $Out "pr.log"
    while ($sw.Elapsed.TotalSeconds -lt $TimeoutSec -and !$p.HasExited) {
        if ((Test-Path -LiteralPath $log) -and (Select-String -LiteralPath $log -Pattern '^(done|ERROR)' -Quiet)) { break }
        Start-Sleep -Seconds 2
    }
    if (Test-Path -LiteralPath $log) { Get-Content -LiteralPath $log -Encoding UTF8 }
    if (!(Test-Path -LiteralPath $log) -or !(Select-String -LiteralPath $log -Pattern '^done$' -Quiet)) {
        throw "FAIL: Premiere setup/export did not finish. Inspect $Out. No process was killed."
    }
}
function Read-Frame([string]$Name) {
    $video = Join-Path $Out "$Name.mp4"
    $raw = Join-Path $Out "$Name.rgba"
    & $FFmpeg -v error -y -ss 0.5 -i $video -frames:v 1 -f rawvideo -pix_fmt rgba $raw
    if ($LASTEXITCODE -ne 0) { throw "FAIL: could not decode $video" }
    $bytes = [IO.File]::ReadAllBytes($raw)
    if ($bytes.Length -ne 320 * 180 * 4) { throw "FAIL: expected a 320x180 frame: $Name" }
    return ,$bytes
}
$frames = @{}
foreach ($name in @("plain", "source", "glass", "red-glass", "stripe-glass", "grey-glass", "empty", "opaque", "track-glass", "track-red", "track-empty")) { $frames[$name] = Read-Frame $name }
function Difference([byte[]]$A, [byte[]]$B, [int]$Left = 0, [int]$Right = 320) {
    $sum = 0.0; $n = 0
    for ($y = 60; $y -lt 120; $y++) { for ($x = $Left; $x -lt $Right; $x++) {
        $i = 4 * (320 * $y + $x)
        for ($c = 0; $c -lt 3; $c++) { $sum += [Math]::Abs([int]$A[$i + $c] - [int]$B[$i + $c]); $n++ }
    } }
    return $sum / $n
}
$checks = [ordered]@{}
$checks["glass differs from source colour"] = (Difference $frames.glass $frames.source 80 140) -gt 15
$checks["source RGB ignored"] = (Difference $frames.glass $frames.'red-glass' 80 140) -lt 5
$checks["glass actually renders"] = (Difference $frames.glass $frames.plain 65 90) -gt 2
$checks["another lower track reaches glass"] = (Difference $frames.glass $frames.'stripe-glass' 100 150) -gt 5
$checks["empty source passes through"] = (Difference $frames.empty $frames.plain) -lt 3
$checks["opaque clip passes through"] = (Difference $frames.opaque $frames.plain) -lt 3
$checks["isolated track checkout renders"] = (Difference $frames.'track-glass' $frames.plain 65 90) -gt 2
$checks["track source RGB ignored"] = (Difference $frames.'track-glass' $frames.'track-red' 65 255) -lt 5
$checks["empty track passes through"] = (Difference $frames.'track-empty' $frames.plain) -lt 3
$neutral = 0.0; $level = 0.0; $n = 0
for ($y = 60; $y -lt 120; $y++) { for ($x = 80; $x -lt 140; $x++) {
    $i = 4 * (320 * $y + $x); $p = $frames.'grey-glass'
    $neutral += [Math]::Max([Math]::Abs([int]$p[$i] - [int]$p[$i + 1]), [Math]::Abs([int]$p[$i] - [int]$p[$i + 2]))
    $level += ([int]$p[$i] + [int]$p[$i + 1] + [int]$p[$i + 2]) / 3; $n++
} }
$checks["neutral grey stays neutral"] = $neutral / $n -lt 6
$checks["no black glass"] = $level / $n -gt 40
$checks["no black frame outside glass"] = (Difference $frames.glass $frames.plain 0 30) -lt 3
$failed = 0
foreach ($key in $checks.Keys) { if ($checks[$key]) { Write-Host "PASS $key" } else { Write-Host "FAIL $key"; $failed++ } }
Write-Host "Frames and project: $Out"
if ($failed) { throw "FAIL: $failed checks. Direct background or isolated-track checkout may be unavailable. Use the documented adjustment-layer fallback; direct mode is unverified." }
Write-Host "PASS: measured alpha/backdrop checks. Native graphics, adjustment input, transforms, UI and 32-bit export still need host review."
