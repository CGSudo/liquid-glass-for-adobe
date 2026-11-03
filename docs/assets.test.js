// The website (GitHub Pages serves only docs/) runs the same live demo as Setup, so it carries copies of the engine and
// stage files. This test fails when a copy is stale; `node docs/assets.test.js --sync` refreshes them.
const assert = require("assert");
const fs = require("fs");
const path = require("path");
const root = path.join(__dirname, "..");
const SOURCES = ["photoshop/looks.js", "photoshop/engine.js", "photoshop/glass_core.wasm", "setup/glass-worker.js", "setup/stage.js"];
for (const src of SOURCES) {
  const from = path.join(root, src), to = path.join(__dirname, path.basename(src));
  if (process.argv.includes("--sync")) fs.copyFileSync(from, to);
  assert(fs.existsSync(to) && fs.readFileSync(from).equals(fs.readFileSync(to)), `docs/${path.basename(src)} is stale: run node docs/assets.test.js --sync`);
}
console.log("docs assets ok");
