// Inspect the shipped executable as data; never run the installer during a build check.
const assert = require("assert");
const fs = require("fs");
const path = require("path");
const root = path.join(__dirname, "..");
const version = JSON.parse(fs.readFileSync(path.join(root, "photoshop/manifest.json"))).version;
const exe = fs.readFileSync(path.join(root, `dist/LiquidGlass-for-Adobe-${version}-Setup.exe`));
for (const file of ["dist/LiquidGlass-for-Adobe.ccx", "dist/LiquidGlass.aip", "aftereffects/build/LiquidGlass.aex", "aftereffects/build/LiquidGlassBrightness.aex"]) {
  const bytes = fs.readFileSync(path.join(root, file));
  assert(bytes.length && exe.includes(bytes), `Setup payload differs from ${file}`);
  if (file.endsWith(".aex")) {
    const name = file.includes("Brightness") ? "CGSudo Liquid Glass Brightness" : "CGSudo Liquid Glass";
    assert(bytes.includes(Buffer.from(name + "\0")), `${file} is missing its release identity`);
    assert(!bytes.includes(Buffer.from(name + " Test")), `${file} was built with LG_TEST`);
  }
}
const manifest = fs.readFileSync(path.join(root, "photoshop/manifest.json"));
assert(!(manifest[0] === 0xef && manifest[1] === 0xbb && manifest[2] === 0xbf), "UXP manifest must have no BOM");
console.log("setup payload: all four plug-ins match; both AE identities are release builds; manifest has no BOM");
