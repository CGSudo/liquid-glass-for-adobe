// Premiere Pro check, started by verify_pr.ps1: is the effect listed, and does it render on a real clip? Writes results to LG_TEST_DIR.
// Needs a new project of its own, so it's only ever run with Premiere closed (verify_pr.ps1 refuses otherwise).
(function () {
	var dir = $.getenv("LG_TEST_DIR");
	var f = new File(dir + "/pr.log");
	f.encoding = "UTF-8";
	f.open("a");
	function log(s) { f.writeln(s); f.close(); f.open("a"); }
	try {
		log("version " + app.version);
		app.enableQE();
		var names = qe.project.getVideoEffectList();
		var hit = [];
		for (var i = 0; i < names.length; i++) if (/liquid glass/i.test(names[i])) hit.push(names[i]);
		log("effects total " + names.length + ", matching: " + hit.join(", "));

		var proj = new File(dir + "/lg_pr_test.prproj");
		app.newProject(proj.fsName);
		$.sleep(3000);
		var ok = app.project.importFiles([dir + "/bg.png"], true, app.project.getInsertionBin(), false);
		log("import " + ok + " items " + app.project.rootItem.children.numItems);
		var clip = app.project.rootItem.children[0];
		var plain = app.project.createNewSequenceFromClips("plain", [clip], app.project.getInsertionBin());
		$.sleep(2000);
		var seq = app.project.createNewSequenceFromClips("lg", [clip], app.project.getInsertionBin());
		$.sleep(2000);
		app.project.activeSequence = seq;
		var qs = qe.project.getActiveSequence();
		var item = qs.getVideoTrackAt(0).getItemAt(0);
		var fxe = qe.project.getVideoEffectByName("Liquid Glass");
		log("clip " + (item ? item.name : "none") + ", effect object " + (fxe ? "found" : "none"));
		item.addVideoEffect(fxe);
		$.sleep(1000);

		var track = seq.videoTracks[0].clips[0];
		var comps = track.components;
		for (var c = 0; c < comps.numItems; c++) {
			var cp = comps[c];
			if (cp.displayName !== "Liquid Glass") continue;
			log("component " + cp.displayName + " props " + cp.properties.numItems);
			var listed = [];
			for (var p = 0; p < cp.properties.numItems; p++) listed.push(cp.properties[p].displayName);
			log(listed.join(" | "));
			for (var q = 0; q < cp.properties.numItems; q++) {
				var pr = cp.properties[q];
				if (pr.displayName === "Edge width") pr.setValue(40, true);
				if (pr.displayName === "Darkness") pr.setValue(0.35, true);
			}
		}
		// Keyframed centre (Premiere's points are 0..1 of the frame); two frames should differ.
		var lg = null;
		for (var k = 0; k < comps.numItems; k++) if (comps[k].displayName === "Liquid Glass") lg = comps[k];
		for (var m = 0; m < lg.properties.numItems; m++) {
			var ce = lg.properties[m];
			if (ce.displayName !== "Center") continue;
			ce.setTimeVarying(true);
			var t0 = new Time(), t4 = new Time();
			t0.seconds = 0;
			t4.seconds = 4;
			ce.addKey(t0);
			ce.addKey(t4);
			ce.setValueAtKey(t0, [0.25, 0.3], 1);
			ce.setValueAtKey(t4, [0.75, 0.7], 1);
			var keys = ce.getKeys();
			log("center keys: " + keys.length + ", at 0 s " + ce.getValueAtKey(t0) + ", at 4 s " + ce.getValueAtKey(t4));
			break;
		}
		// The QE frame export throws in this version, so render real files; timing them against the plain sequence gives ms per frame.
		var preset = "C:/Program Files/Adobe/Adobe Premiere Pro 2026/MediaIO/systempresets/3F3F3F3F_4D6F6F56/H264 Match Source - High bitrate.epr";
		function render(s, name) {
			var t0 = new Date().getTime();
			var r = s.exportAsMediaDirect(new File(dir + "/" + name).fsName, new File(preset).fsName, 0);
			log(name + ": '" + r + "' in " + (new Date().getTime() - t0) + " ms");
		}
		render(plain, "pr_plain.mp4");
		render(seq, "pr_glass.mp4");
		app.project.save();
		log("done");
		app.quit();
	} catch (e) {
		log("ERROR line " + e.line + ": " + e.toString());
	}
	f.close();
})();
