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

console.log("aftereffects params: ok");
