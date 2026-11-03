// Live glass stage, shared by the Setup window and the website: a moving colourful scene with two shapes that melt
// together, rendered with the chosen look by the real engine in a worker (glass-worker.js). Drag on the canvas to move
// the glass. Needs looks.js loaded first.
//   const stage = createStage(canvas, { width, height });  stage.start() / stop() / setLook(i);
//   stage.frame(t, pos) -> Promise: one render at time t (recording).
function createStage(canvas, { width: W, height: H, interactive = true }) {
  const { LOOKS, KEEP } = window.SudoLooks;
  canvas.width = W;
  canvas.height = H;
  const out = canvas.getContext("2d");
  const scene = new OffscreenCanvas(W, H), sx = scene.getContext("2d", { willReadFrequently: true });
  const shape = new OffscreenCanvas(W, H), mx = shape.getContext("2d", { willReadFrequently: true });
  const worker = new Worker("glass-worker.js");
  const s = Math.min(W, H) / 470; // the scene was designed at 470 px
  let look = 0, waiting = null, pointer = null, glide = null;

  function drawScene(t) {
    const g = sx.createLinearGradient(0, 0, W, H);
    g.addColorStop(0, "#0b1a2e");
    g.addColorStop(1, "#1a0f2e");
    sx.fillStyle = g;
    sx.fillRect(0, 0, W, H);
    for (const [c, ph, r] of [["#ff5f6d", 0.0, 150], ["#ffc371", 1.7, 120], ["#36d1dc", 3.1, 140], ["#7b5cff", 4.6, 160]]) {
      const x = W / 2 + Math.cos(t * 0.35 + ph) * W * 0.32, y = H / 2 + Math.sin(t * 0.27 + ph * 1.3) * H * 0.32;
      const rg = sx.createRadialGradient(x, y, 0, x, y, r * s);
      rg.addColorStop(0, c);
      rg.addColorStop(1, "transparent");
      sx.fillStyle = rg;
      sx.fillRect(0, 0, W, H);
    }
    sx.strokeStyle = "rgba(255,255,255,0.10)";
    sx.lineWidth = 1;
    for (let x = 0.5; x < W; x += 24 * s) { sx.beginPath(); sx.moveTo(x, 0); sx.lineTo(x, H); sx.stroke(); }
    sx.fillStyle = "rgba(255,255,255,0.88)";
    sx.font = `800 ${Math.round(64 * s)}px 'Segoe UI', system-ui, sans-serif`;
    sx.textAlign = "center";
    sx.fillText("Liquid", W / 2, H / 2 - 18 * s);
    sx.fillText("Glass", W / 2, H / 2 + 50 * s);
  }

  // The two shapes: a pill (follows `pos` when given) and a dot circling it.
  function shapes(t, pos) {
    const w = 300 * s, h = 150 * s;
    const cx = pos ? pos[0] : W / 2 + Math.sin(t * 0.5) * 30 * s, cy = pos ? pos[1] : H / 2 - 20 * s + Math.cos(t * 0.4) * 24 * s;
    const a = t * 0.6;
    return [(c) => c.roundRect(cx - w / 2, cy - h / 2, w, h, h / 2), (c) => c.arc(cx + Math.cos(a) * 150 * s, cy + 115 * s + Math.sin(a) * 40 * s, 52 * s, 0, Math.PI * 2)];
  }
  function masks(list) {
    const all = new Uint8Array(list.length * W * H);
    list.forEach((draw, k) => {
      mx.clearRect(0, 0, W, H);
      mx.fillStyle = "#fff";
      mx.beginPath();
      draw(mx);
      mx.fill();
      const px = mx.getImageData(0, 0, W, H).data;
      for (let i = 0; i < W * H; i++) all[k * W * H + i] = px[i * 4 + 3];
    });
    return all;
  }

  worker.onmessage = ({ data }) => {
    out.putImageData(new ImageData(data.rgba, W, H), 0, 0);
    const done = waiting;
    waiting = null;
    done && done();
  };
  // One render of time `t` (seconds), the glass centred on `pos` ([x, y]) or drifting on its own.
  function frame(t, pos) {
    return new Promise((resolve) => {
      waiting = resolve;
      drawScene(t);
      const backdrop = new Uint8Array(sx.getImageData(0, 0, W, H).data.buffer);
      const m = masks(shapes(t, pos));
      const params = LOOKS[look].values.slice();
      params[KEEP[0]] = Math.round(34 * s); // edge width sized for these shapes, as Make Glass does
      worker.postMessage({ params, backdrop, masks: m, w: W, h: H }, [backdrop.buffer, m.buffer]);
    });
  }

  // Dragging moves the glass; letting go glides it back to its drift.
  if (interactive) {
    const at = (e) => {
      const r = canvas.getBoundingClientRect();
      return [((e.clientX - r.left) / r.width) * W, ((e.clientY - r.top) / r.height) * H];
    };
    canvas.addEventListener("pointerdown", (e) => { canvas.setPointerCapture(e.pointerId); pointer = at(e); glide = pointer; });
    canvas.addEventListener("pointermove", (e) => pointer && (pointer = at(e)));
    canvas.addEventListener("pointerup", () => (pointer = null));
    canvas.style.cursor = "grab";
  }
  const reduced = matchMedia("(prefers-reduced-motion: reduce)").matches;
  let running = false;
  function loop() {
    running = true;
    const t = reduced ? 0 : performance.now() / 1000;
    // Ease towards the finger while dragging; afterwards drift again.
    if (pointer) glide = glide ? [glide[0] + (pointer[0] - glide[0]) * 0.5, glide[1] + (pointer[1] - glide[1]) * 0.5] : pointer;
    else glide = null;
    frame(t, glide).then(() => (reduced || paused ? (running = false) : requestAnimationFrame(loop)));
  }
  let paused = false;
  return {
    get look() { return look; },
    setLook(i) { look = (i + LOOKS.length) % LOOKS.length; if (reduced && !running) loop(); },
    start() { paused = false; if (!running) loop(); },
    stop() { paused = true; }, // the current render finishes, then the loop rests
    frame, scene, shapes,
  };
}
