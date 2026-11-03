// Renders the live look previews (Setup and website) with the real glass engine, off the page's thread (Chromium only compiles
// big wasm modules synchronously inside workers). In: {id, params, backdrop, masks, w, h}. Out: {id, rgba} composited
// like the panels' thumbnails: backdrop, then shadow/dim, then glass.
self.module = { exports: {} };
importScripts("engine.js");
let engine = null;
const ready = fetch("glass_core.wasm")
  .then((r) => r.arrayBuffer())
  .then((b) => (engine = self.module.exports.createWasmEngine(new Uint8Array(b))));

self.onmessage = async ({ data: { id, params, backdrop, masks, w, h } }) => {
  await ready;
  const out = engine.render(new Float32Array(params), backdrop, masks, w, h);
  const rgba = new Uint8ClampedArray(w * h * 4);
  for (let i = 0; i < rgba.length; i += 4) {
    const ua = out.under[i + 3] / 255, ga = out.glass[i + 3] / 255;
    for (let c = 0; c < 3; c++) {
      const v = backdrop[i + c] * (1 - ua) + out.under[i + c] * ua;
      rgba[i + c] = v * (1 - ga) + out.glass[i + c] * ga;
    }
    rgba[i + 3] = 255;
  }
  self.postMessage({ id, rgba }, [rgba.buffer]);
};
