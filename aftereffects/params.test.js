// The After Effects / Premiere table must offer what the Photoshop panel offers: same defaults, looks, labels and ranges.
const fs = require("fs");
const path = require("path");
const assert = require("assert");
const { DEFAULTS, LOOKS } = require("../photoshop/looks.js");

const header = fs.readFileSync(path.join(__dirname, "Source/GlassHost.h"), "utf8");
const panel = fs.readFileSync(path.join(__dirname, "../photoshop/index.js"), "utf8");
const num = (s) => parseFloat(s);

// Defaults
const defaults = header.match(/kDefaults\[kCoreCount\] = \{([^}]+)\}/)[1].split(",").map(num);
assert.deepStrictEqual(defaults, DEFAULTS, "kDefaults differs from looks.js");

// Looks, name by name, every value
const looks = [...header.matchAll(/\{ "([^"]+)", \{((?:\s*\{\d+, [-\d.]+f?\},?)*)\s*\} \}/g)].map((m) => {
  const values = DEFAULTS.slice();
  for (const v of m[2].matchAll(/\{(\d+), ([-\d.]+)f?\}/g)) values[+v[1]] = num(v[2]);
  return { name: m[1], values };
});
assert.deepStrictEqual(looks.map((l) => l.name), LOOKS.map((l) => l.name), "look names differ");
looks.forEach((l, i) => assert.deepStrictEqual(l.values, LOOKS[i].values, `look ${l.name} differs`));
assert.ok(header.includes(`"Choose a look...|${LOOKS.map((l) => l.name).join("|")}"`), "Look popup items differ");

// Slider rows: [index, "Label", min, max, ...] in the panel, K_FLOAT rows here. Frost and Lens etc. appear once each.
const sliders = [...panel.matchAll(/\[(\d+), "([^"]+)", (-?[\d.]+), (-?[\d.]+), [\d.]+, "/g)].map((m) => ({ core: +m[1], label: m[2], min: num(m[3]), max: num(m[4]) }));
const rows = new Map([...header.matchAll(/K_FLOAT, (\d+), "([^"]+)", (-?[\d.]+)f?, (-?[\d.]+)f?, (-?[\d.]+)f?,/g)].map((m) => [+m[1], { label: m[2], min: num(m[3]), max: num(m[4]), dflt: num(m[5]) }]));
assert.ok(sliders.length >= 25, "panel sliders not found");
for (const s of sliders) {
  const r = rows.get(s.core);
  if (header.includes(`K_ANGLE, ${s.core}, "${s.label}"`)) continue; // an angle dial covers the panel's -180..180 slider
  assert.ok(r, `no row for param ${s.core} (${s.label})`);
  assert.deepStrictEqual({ label: r.label, min: r.min, max: r.max }, { label: s.label, min: s.min, max: s.max }, `param ${s.core}`);
  const hostDefaults = { 3: 30, 13: 0.25 }; // this host starts with a stronger edge and a shadow; looks stay the same
  assert.strictEqual(r.dflt, hostDefaults[s.core] ?? DEFAULTS[s.core], `default of param ${s.core}`);
}

// Every one of the 42 values has a control (sliders + dials + colours + popups).
const driven = new Set(rows.keys());
for (const m of header.matchAll(/K_(ANGLE|POPUP|COLOR), (\d+),/g)) {
  driven.add(+m[2]);
  if (m[1] === "COLOR") [1, 2].forEach((d) => driven.add(+m[2] + d));
}
assert.strictEqual(driven.size, DEFAULTS.length, "a param has no control");

// Saved IDs stay append-only; the Lens checkbox moves later runtime indices by exactly one.
const ids = header.match(/enum ParamId \{([\s\S]*?)\};/)[1].match(/P_\w+/g);
assert.strictEqual(ids.indexOf("P_E_ANIM") + 1, 80);
assert.strictEqual(ids.indexOf("P_LIQUID_LETTERS") + 1, 81);
assert.deepStrictEqual(ids.slice(81), ["P_T_LUMA", "P_THRESHOLD", "P_MASK_SOFTNESS", "P_INVERT", "P_E_LUMA"]);
const table = header.match(/const Row kRows\[\] = \{([\s\S]*?)\n\};/)[1];
const mainTable = table.replace(/#ifdef LG_BRIGHTNESS[\s\S]*?#endif/g, "");
const rowIds = (text) => [...text.matchAll(/\{ (P_\w+), K_/g)].map((m) => m[1]);
const mainIds = rowIds(mainTable), brightnessIds = rowIds(table);
assert.deepStrictEqual(mainIds.filter((id) => id !== "P_LIQUID_LETTERS"), ids.slice(0, 80));
assert.strictEqual(mainIds.indexOf("P_LIQUID_LETTERS"), mainIds.indexOf("P_MERGE") + 1);
assert.deepStrictEqual(brightnessIds.slice(0, mainIds.length), mainIds);
assert.deepStrictEqual(brightnessIds.slice(mainIds.length), ids.slice(81));
assert.match(mainTable, /P_LIQUID_LETTERS, K_CHECK, -1, "Liquid letters", 0, 1, 0,/);
assert.match(table, /P_THRESHOLD, K_FLOAT, -1, "Threshold", 0, 1, 0.5f,/);
assert.match(table, /P_MASK_SOFTNESS, K_FLOAT, -1, "Softness", 0, 1, 1,/);

const names = fs.readFileSync(path.join(__dirname, "Source/LiquidGlassNames.h"), "utf8");
for (const name of ["Liquid Glass", "Liquid Glass (test)", "Liquid Glass Brightness", "Liquid Glass Brightness (test)"]) {
  assert.ok(names.includes(`LG_NAME "${name}"`), `missing identity: ${name}`);
}
assert.strictEqual(new Set([...names.matchAll(/#define LG_MATCH "([^"]+)"/g)].map((m) => m[1])).size, 4);

console.log("aftereffects params: ok");
