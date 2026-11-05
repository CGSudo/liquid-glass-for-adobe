// Builds the verification project: real comps using the effect, queued to PNG, saved as .aepx for aerender.
// Run through verify_ae.ps1 only: it refuses to do anything unless it's in the instance started with LG_TEST_INSTANCE=1,
// so a user's open After Effects can never be touched. Scripts may not write files by default, so results go into a
// text layer of the saved project ("LOG" comp).
(function () {
	if ($.getenv("LG_TEST_INSTANCE") !== "1") return;
	var dir = $.getenv("LG_TEST_DIR");
	app.beginSuppressDialogs();
	app.project.close(CloseOptions.DO_NOT_SAVE_CHANGES);
	var log = "ver=" + app.version + "\n";

	var ID = { LOOK: 1, TYPE: 3, CENTER: 4, WIDTH: 5, HEIGHT: 6, RADIUS: 7, ROTATION: 8, SHAPE_LAYER: 9, CLIP: 10, BACK: 11, SECOND: 14, TYPE2: 15, CENTER2: 16, W2: 17, H2: 18,
		FROST: 23, LENS: 24, EDGE: 25, SHINE: 26, DISP: 30, MERGE: 31, ANGLE: 34, FOLLOW: 35, BRIGHT: 38, TINT: 40, TINT_AMT: 41, DARK: 44, SHSOFT: 45, DROP: 46, DIM: 49,
		GLOW: 53, GLOW_COLOR: 55, ST_WIDTH: 59, ST_FILL: 60, ST_COLOR2: 63, GRAIN: 68, WOBBLE: 78, SPEED: 79 };
	function P(fx, id) { var s = "000" + id; return fx.property("CGSudo Liquid Glass-" + s.substr(s.length - 4)); }

	function backdrop(c) {
		var bg = c.layers.addSolid([0.1, 0.2, 0.5], "bg", c.width, c.height, 1);
		var fxs = bg.property("ADBE Effect Parade");
		var ramp = fxs.addProperty("ADBE Ramp");
		ramp.property("ADBE Ramp-0002").setValue([1, 0.45, 0.15]);
		ramp.property("ADBE Ramp-0004").setValue([0.1, 0.25, 0.8]);
		ramp.property("ADBE Ramp-0001").setValue([0, 0]);
		ramp.property("ADBE Ramp-0003").setValue([c.width, c.height]);
		var grid = c.layers.addSolid([1, 1, 1], "grid", c.width, c.height, 1);
		grid.property("ADBE Effect Parade").addProperty("ADBE Grid"); // on its own layer: it makes its layer transparent
		grid.moveBefore(bg);
		var t = c.layers.addText("LIQUID GLASS");
		var d = t.property("Source Text").value;
		d.fontSize = 180;
		d.fillColor = [1, 1, 1];
		d.applyFill = true;
		d.justification = ParagraphJustification.CENTER_JUSTIFY;
		t.property("Source Text").setValue(d);
		t.property("Position").setValue([c.width / 2, c.height / 2 + 60]);
		return bg;
	}
	function glass(c) {
		var a = c.layers.addSolid([1, 1, 1], "glass", c.width, c.height, 1);
		a.adjustmentLayer = true;
		return a.property("ADBE Effect Parade").addProperty("CGSudo Liquid Glass");
	}
	function comp(name, seconds) { return app.project.items.addComp(name, 1920, 1080, 1, seconds || 2, 30); }
	function queue(c, name, frame, n, half) {
		var rq = app.project.renderQueue.items.add(c);
		rq.timeSpanStart = frame / c.frameRate;
		rq.timeSpanDuration = n / c.frameRate;
		if (half) rq.setSettings({ "Resolution": [2, 2] });
		var om = rq.outputModule(1);
		om.applyTemplate("TIFF Sequence with Alpha");
		om.file = new File(dir + "/" + name + "_f" + frame + "_[#####].tif"); // this version has no PNG template; verify_ae.ps1 converts
	}

	try {
		// Keyframes: position and rotation.
		var c = comp("basic"); backdrop(c);
		var fx = glass(c);
		P(fx, ID.CENTER).setValueAtTime(0, [300, 540]);
		P(fx, ID.CENTER).setValueAtTime(2, [1620, 540]);
		P(fx, ID.ROTATION).setValueAtTime(0, 0);
		P(fx, ID.ROTATION).setValueAtTime(2, 30);
		P(fx, ID.EDGE).setValue(40);
		queue(c, "basic", 0, 1); queue(c, "basic", 30, 1); queue(c, "basic_half", 30, 1, true);

		// Look popup: does setting it fire the supervised change?
		var c2 = comp("look"); backdrop(c2);
		var fx2 = glass(c2);
		P(fx2, ID.EDGE).setValue(33);
		P(fx2, ID.LOOK).setValue(3); // popup item 3 = Clear
		log += "afterLook: lens=" + P(fx2, ID.LENS).value + " frost=" + P(fx2, ID.FROST).value + " edge=" + P(fx2, ID.EDGE).value + " dark=" + P(fx2, ID.DARK).value + " look=" + P(fx2, ID.LOOK).value + "\n";
		queue(c2, "look", 0, 1);

		// Everything on at once, plus the two-shape liquid merge.
		var c3 = comp("merge"); backdrop(c3);
		var fx3 = glass(c3);
		P(fx3, ID.TYPE).setValue(2);
		P(fx3, ID.WIDTH).setValue(420); P(fx3, ID.HEIGHT).setValue(420);
		P(fx3, ID.CENTER).setValue([700, 540]);
		P(fx3, ID.SECOND).setValue(1);
		P(fx3, ID.TYPE2).setValue(2);
		P(fx3, ID.W2).setValue(300); P(fx3, ID.H2).setValue(300);
		P(fx3, ID.CENTER2).setValueAtTime(0, [1500, 540]);
		P(fx3, ID.CENTER2).setValueAtTime(2, [950, 540]);
		P(fx3, ID.MERGE).setValue(160);
		P(fx3, ID.EDGE).setValue(60);
		P(fx3, ID.DARK).setValue(0.4);
		P(fx3, ID.GLOW).setValue(0.4); P(fx3, ID.GLOW_COLOR).setValue([0.6, 0.9, 1]);
		P(fx3, ID.ST_WIDTH).setValue(4); P(fx3, ID.ST_FILL).setValue(2); P(fx3, ID.ST_COLOR2).setValue([1, 0.3, 0.7]);
		P(fx3, ID.TINT).setValue([0.4, 0.8, 1]); P(fx3, ID.TINT_AMT).setValue(0.15);
		P(fx3, ID.GRAIN).setValue(0.2);
		queue(c3, "merge", 0, 1); queue(c3, "merge", 59, 1);

		// Shape from another layer's alpha (text), which stays hidden.
		var c4 = comp("shape_layer"); backdrop(c4);
		var src = c4.layers.addText("GLASS");
		var d4 = src.property("Source Text").value;
		d4.fontSize = 400; d4.fillColor = [1, 1, 1]; d4.applyFill = true; d4.justification = ParagraphJustification.CENTER_JUSTIFY;
		src.property("Source Text").setValue(d4);
		src.property("Position").setValue([960, 680]);
		src.name = "glass text";
		var fx4 = glass(c4);
		P(fx4, ID.SHAPE_LAYER).setValue(c4.layer("glass text").index);
		c4.layer("glass text").enabled = false;
		P(fx4, ID.EDGE).setValue(24); P(fx4, ID.DARK).setValue(0.35);
		queue(c4, "shape_layer", 0, 1);

		// Clip alpha on a text layer, with an explicit backdrop layer, over the comp.
		var c5 = comp("clip_alpha");
		var pre5 = comp("clip_backdrop"); backdrop(pre5);
		var bg5 = c5.layers.add(pre5);
		var t5 = c5.layers.addText("CLIP");
		var d5 = t5.property("Source Text").value;
		d5.fontSize = 500; d5.fillColor = [1, 1, 1]; d5.applyFill = true; d5.justification = ParagraphJustification.CENTER_JUSTIFY;
		t5.property("Source Text").setValue(d5);
		t5.property("Position").setValue([960, 720]);
		var fx5 = t5.property("ADBE Effect Parade").addProperty("CGSudo Liquid Glass");
		P(fx5, ID.CLIP).setValue(1);
		P(fx5, ID.BACK).setValue(bg5.index);
		P(fx5, ID.EDGE).setValue(24); P(fx5, ID.DARK).setValue(0.4);
		queue(c5, "clip_alpha", 0, 1);

		// Time-driven wobble, and the light turning toward the direction of travel.
		var c6 = comp("wobble"); backdrop(c6);
		var fx6 = glass(c6);
		P(fx6, ID.WOBBLE).setValue(30); P(fx6, ID.SPEED).setValue(1.5); P(fx6, ID.EDGE).setValue(40);
		P(fx6, ID.CENTER).setValueAtTime(0, [300, 300]);
		P(fx6, ID.CENTER).setValueAtTime(2, [1600, 800]);
		P(fx6, ID.FOLLOW).setValue(1);
		queue(c6, "wobble", 10, 1); queue(c6, "wobble", 25, 1);

		// Timing: same comp with the effect on and off, 120 frames each. Queued further down, one project per comp.
		var c7 = comp("time_on", 4); backdrop(c7);
		var fx7 = glass(c7);
		P(fx7, ID.CENTER).setValueAtTime(0, [300, 540]); P(fx7, ID.CENTER).setValueAtTime(4, [1620, 540]);
		var c8 = comp("time_off", 4); backdrop(c8);
		var c9 = comp("time_full", 4); backdrop(c9);
		var fx9 = glass(c9);
		P(fx9, ID.WIDTH).setValue(1900); P(fx9, ID.HEIGHT).setValue(1060);
		P(fx9, ID.CENTER).setValueAtTime(0, [960, 540]); P(fx9, ID.CENTER).setValueAtTime(4, [961, 540]); // every frame differs
		P(fx9, ID.FROST).setValue(24);
		P(fx9, ID.DARK).setValue(0.3);
	} catch (e) {
		log += "ERROR line " + e.line + ": " + e.toString() + "\n";
	}

	var lc = app.project.items.addComp("LOG", 100, 100, 1, 1, 30);
	lc.layers.addText("LOGSTART " + log + " LOGEND");
	app.project.save(new File(dir + "/lg_8bit.aepx"));

	// Deep colour: same project, only the keyframes comp queued.
	function requeue(bits, name, compName, frame, n) {
		app.project.bitsPerChannel = bits;
		var q = app.project.renderQueue;
		for (var i = q.numItems; i >= 1; i--) q.item(i).remove();
		for (var j = 1; j <= app.project.numItems; j++) {
			var it = app.project.item(j);
			if (it instanceof CompItem && it.name === compName) queue(it, name, frame, n);
		}
		app.project.save(new File(dir + "/" + name + ".aepx"));
	}
	try {
		requeue(8, "t_on", "time_on", 0, 120);
		requeue(8, "t_off", "time_off", 0, 120);
		requeue(8, "t_full", "time_full", 0, 120);
		requeue(16, "lg_16bit", "basic", 30, 1);
		requeue(32, "lg_32bit", "basic", 30, 1);
	} catch (e2) {
		app.project.save(new File(dir + "/lg_32bit.aepx")); // ends the wait in verify_ae.ps1
	}

	app.project.close(CloseOptions.DO_NOT_SAVE_CHANGES);
	app.endSuppressDialogs(false);
	app.scheduleTask('app.executeCommand(app.findMenuCommandId("Exit"))', 500, false); // app.quit() does nothing in a -r launch
})();
