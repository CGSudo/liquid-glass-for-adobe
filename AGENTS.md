# AGENTS.md: guide for AI coding agents

**Liquid Glass for Adobe** (by CGSudo; called Sudo Liquid Glass before 0.7.2) recreates Apple's Liquid Glass material inside Adobe apps. It is live and non-destructive: the glass reads what is behind it and re-renders, and it never merges or flattens layers.

- **Today (v0.7.3):**
  - **Photoshop:** a UXP plugin, Windows and macOS (wasm engine, no platform code; tested on Windows only).
  - **Illustrator:** a native Live Effect (`.aip`), **Windows only**.
- **Built, not released yet:** one native effect (`aftereffects/`, `.aex`, Windows) for **After Effects and Premiere Pro**. Tested on Windows in both 2026 apps. It isn't in any release, Setup.exe or website copy: say so until the owner ships it.
- **Goal:** Illustrator on Mac. This is a plan: never describe it as available.
- **Packaging:** Photoshop `.ccx` (UXP; ZXP is for CEP extensions and doesn't apply). Illustrator `.aip`. Windows users get one `Setup.exe` (`setup/`) that installs both; Mac users double-click the `.ccx`.
- **Name:** never put "Adobe" first in the product name (Adobe's trademark rules); "… for Adobe" is the allowed form. Internal ids stay as they were: UXP id `com.liquidglass.photoshop`, localStorage key `sudoLiquidGlass.looks`, `%APPDATA%\Sudo Liquid Glass` (saved presets and log), so upgrades keep users' data.

**New here with zero context?** Read [docs/PROJECT_HISTORY.md](docs/PROJECT_HISTORY.md) first. It explains:
- the mission and every decision;
- every problem the owner reported and how it was fixed;
- the tools installed on the dev machine.

Then read this file, then the top-of-file comments in `core/src/lib.rs` and `photoshop/index.js`.

## Layout
```
core/src/lib.rs          Rust renderer, glass_core: render(), fill_paths(), smooth_union(), C ABI
                         (lg_render, lg_fill, lg_param_count, lg_alloc, lg_free)
core/Cargo.toml          crate types: rlib (preview), cdylib (wasm for Photoshop), staticlib (native hosts)
photoshop/manifest.json  UXP manifest v5 (MUST be saved without a UTF-8 BOM, or Adobe's installer fails with -267)
photoshop/index.html     panel markup and CSS: same look as the Illustrator window (fixed dark palette, SUDO green)
photoshop/index.js       panel UI, glass detection, history-suspension reads, live poller, Make Glass
photoshop/engine.js      loads glass_core.wasm (or a native addon if one exists); render() and fill()
photoshop/paths.js       Photoshop vector path (batchPlay pathComponents) → fill_paths stream
photoshop/looks.js       SHARED with Illustrator: DEFAULTS, the built-in looks, pad()/applyLook()/lookOf(), line icons
photoshop/logo.png       SUDO logo shown in the panel header and About page
photoshop/*.test.js      engine.test.js / paths.test.js run the real wasm; ui.test.js loads the panel in jsdom
                         with a fake Photoshop (layer lists are Proxies that behave like Photoshop's)
preview/src/main.rs      renders preview*.png from a synthetic backdrop (engine test renders, not screenshots)
illustrator/             native Live Effect: Source/*.cpp (render, notifier, menus, checks), ui/panel.html (settings
                         window, WebView2), build.ps1, test/run_test.ps1 (settings window without Illustrator)
aftereffects/            one native effect for After Effects and Premiere Pro: Source/GlassHost.h (parameter table, looks, shapes, frame compositor; no SDK),
                         Source/LiquidGlassAE.cpp (SDK glue), build.ps1, params.test.js (in npm test), test/ (no-app test, verify_ae.ps1, verify_pr.ps1)
setup/                   Windows Setup.exe: setup.cpp (Win32 + WebView2 on an acrylic window; installs via UPIA and an elevated
                         --copy-aip of itself; --silent for IT), setup.html (looks tour), build.ps1;
                         stage.js + glass-worker.js = the live engine demo, shared with the website
docs/                    GitHub Pages site (index.html: live demo, SEO meta + JSON-LD; sitemap, robots, llms.txt), explainer.webp
                         (README animation), PROJECT_HISTORY.md. Pages serves only docs/, so it holds copies of the engine and
                         stage files: docs/assets.test.js (in npm test) fails when they're stale; --sync refreshes them
release.ps1              builds every release download into dist/release (AIO, per-app, per-app source)
install.ps1 / install.sh package photoshop/ → dist/LiquidGlass-for-Adobe.ccx and install via Adobe UPIA (Windows / macOS)
package.json             dev tooling only: `npm install` (jsdom), then `npm test` runs every test
```

## The one contract every host shares: `Params`
`glass_core::Params` is `repr(C)`, 42 × f32, in this order. JS passes it as a `Float32Array`; C/C++ passes `const float*`.
- **Count check:** hosts check the count with `lg_param_count()`, and the wasm bridge throws on a mismatch.
- **Units:** all lengths are in **pixels** at render time; hosts that work in points convert first.

| # | field | default | # | field | default |
|---|---|---|---|---|---|
| 0 | light_angle (deg, 0 = top) | -45 | 13 | shadow_opacity (0 = off) | 0 |
| 1 | light_intensity ("Shine") | 0.75 | 14 | shadow_blur (px) | 24 |
| 2 | refraction ("Lens strength") | 80 | 15 | shadow_offset_y (px) | 8 |
| 3 | depth ("Edge width", px) | 16 | 16 | grain (0 = off) | 0 |
| 4 | dispersion ("Dispersion", rainbow) | 20 | 17 | grain_size (px) | 1.5 |
| 5 | frost ("Frost", px; blur radius = half; Make Glass scales it per 200 px of shape) | 3 | 18 | dim (0 = off) | 0 |
| 6 | splay ("Edge softness") | 30 | 19 | dim_spread (px) | 40 |
| 7 | brightness | 0.06 | 20 | merge (px) | 24 |
| 8 | saturation ("Vividness") | 1.2 | 21 | bevel ("Emboss", advanced) | 0.32 |
| 9 | tint r | 1 | 22 | rim_width ("Rim line", px, advanced) | 1.2 |
| 10 | tint g | 1 | 23 | ior ("Glass type", advanced) | 1.5 |
| 11 | tint b | 1 | 24 | sheen (advanced) | 1 |
| 12 | tint amount | 0 | 25 | inner_glow (0 = off) | 0 |
| | | | 26 | inner_glow_size (px) | 12 |
| | | | 27–29 | inner_glow_color r, g, b | 1, 1, 1 |
| | | | 30 | inner_glow_blend (0 normal, 1 screen, 2 overlay, 3 soft light, 4 add, 5 multiply) | 0 |
| | | | 31 | stroke_width (px, inside the edge, 0 = off) | 0 |
| | | | 32–33 | stroke_opacity start, end | 1, 0.2 |
| | | | 34 | stroke_type (0 solid, 1 linear gradient) | 0 |
| | | | 35 | stroke_angle (deg, 0 = start colour at top) | -45 |
| | | | 36–38 | stroke_color r, g, b (solid / gradient start) | 1, 1, 1 |
| | | | 39–41 | stroke_color2 r, g, b (gradient end) | 1, 1, 1 |

To add a parameter:
1. Append it to the end of the list. Never reorder.
2. `PARAM_COUNT` follows from the struct size, so leave it alone.
3. Update `DEFAULTS` in `photoshop/looks.js` (both apps read it).
4. Update `kParamCount` in `illustrator/Source/LiquidGlassDialog.h`, `kDefaults` in `illustrator/Source/LiquidGlassPlugin.cpp` and `illustrator/test/dialog_test.cpp`, and `kLengths` if the new value is a length (pt → px). For After Effects / Premiere: `kDefaults`, a row in `kRows` and `kLengths` in `aftereffects/Source/GlassHost.h`, and `kCoreCount`.
5. Update the `param_layout_matches_js` test and the param arrays in `photoshop/engine.test.js`.

`decodeName` pads older saved glasses (16, 21, 25 or 30 values) with the defaults, so existing documents keep working.

**Render call:** `lg_render(params, backdrop RGBA8, masks A8 × n, n, w, h, out RGBA8, under RGBA8 | null)`.
- **Masks:** several masks fuse into one liquid body (smooth union, `merge` px).
- **With `under`:** `out` is the glass alone, with alpha exactly equal to the shape, and `under` holds the shadow and dim halo.
- **Alpha:** straight, not premultiplied.

## How the Photoshop plugin works
- **Glass structure:** a glass is a layer group, `Liquid Glass`:
  - **Sources:** the user's shape and/or text layers, sitting directly in the group with fill opacity 0. These are what the user moves, edits and styles; Layer Styles on them draw on top of the glass.
  - **`◆ glass render (automatic)`:** a folder at the bottom of the group holding the engine's two layers:
    - `◆glass[json params]`: the glass pixels. Its settings live in this layer name.
    - `◆shadow`: shadow and dim-halo pixels.
  - **Older layouts:** glass from 0.5.0 and earlier kept the engine layers loose in the group; they are packed into the folder on the next render.
  - **Not locked:** the folder is deliberately unlocked. Locked layers make `putPixels` fail with "target sheet is protected". 0.5.1–0.5.4 locked it, and the plugin unlocks such folders.
- **Selection hand-off:** clicking the glass on the canvas lands on an engine layer, so `syncFromSelection` re-selects the glass's sources instead.
- **Peek** (the read step):
  1. `suspendHistory`.
  2. Set each source's fill opacity back to 100. At fill 0, Photoshop reports the layer as empty: no bounds, no pixels. That is the cause of "couldn't read the outline".
  3. Measure the region from the source bounds.
  4. Read the outlines:
     - shapes: batchPlay `get` on `path: vectorMask`;
     - text: `make` a workPath from the text layer, then `get` it;
     - fallback: pixels, then a transparency selection;
     - plain pixel layers are refused, because reading them crashed Photoshop.
  5. Hide the glass group and every layer above it, and read the composite with `imaging.getPixels`.
  6. `resumeHistory(…, false)` rolls everything back, so the document is never modified by a read.
- **Write:** a second suspension does `putPixels` into both engine layers, as one undo step.
- **Live updates:** `poll()` runs every 300 ms. It watches `doc.activeHistoryState`, skips while stepped back in undo, and re-renders every glass from bottom to top.
- **Look thumbnails:** each look card is an engine render of a test scene (`previewPixels` + `imaging.encodeImageData`). The live preview of the selected glass was removed in 0.7.1 at the owner's request.
- **Edge effects:** the "Add strokes, glows & shadows…" button opens the Layer Style dialog on a source shape (fill 0 keeps effects visible, and the stroke traces the real vector).

## UXP / Photoshop gotchas (each one cost a bug)
- **Layer collections:** Photoshop's collections (`group.layers`) are not real arrays. `concat` and spread on them throw "Cannot convert a Symbol value to a string". Copy them with `list()`, which copies by index.
- **`require` of a missing native addon** returns `{}` instead of throwing, so check `typeof fn === "function"`.
- **No selection means "everything selected":** a failed transparency load would otherwise fill the whole box, which is where the old "always a rectangle" bug came from.
- **Locked layers** reject `putPixels`.
- **Fill 0 layers** report empty bounds and pixels (see Peek above).
- **Files written from PowerShell 5.1** with `Set-Content`/`Out-File` get re-encoded: README mojibake, and "◆" turned into "?". Edit text files with the editor tools or Node, never PowerShell.
- **SVG icons:** UXP doesn't pass `fill`/`stroke` from `<svg>` down to its shapes (they drew filled black). `icon()` in `looks.js` puts the paint on every shape.
- **Help text:** the panel shows plain-language help on hover. Every slider row needs a help string (`ui.test.js` checks it).

## Commands (run all before you commit)
```bash
npm install
```
```bash
npm test
```
After any change in `core/`, rebuild the wasm and copy it in:
```bash
cargo build -p glass_core --release --target wasm32-unknown-unknown
```
then copy `target/wasm32-unknown-unknown/release/glass_core.wasm` to `photoshop/`. Package without installing:
```bash
powershell -ExecutionPolicy Bypass -File install.ps1 -NoInstall
```

## Conventions
- Small, direct code; no frameworks. Comments explain *why*, in one line.
- A known limitation is marked in code as `// shortcut: <limit>, <when to upgrade>`. Grep for `shortcut:` to find the debt.
- UXP is not a browser. Prefer `setAttribute` and `appendChild`, and keep CSS to flexbox, radius, rgba and host variables.
- Release:
  1. Bump `version` in `photoshop/manifest.json` (no BOM).
  2. Run `npm test`.
  3. Package with `install.ps1 -NoInstall`.
  4. Build the Illustrator plug-in: `powershell -ExecutionPolicy Bypass -File illustrator/build.ps1 -NoInstall`, and copy `illustrator/build/LiquidGlass.aip` to `dist/`.
  5. Bump `kVersion` (illustrator/Source/LiquidGlassDialog.h), the version in `illustrator/ui/panel.html` and `photoshop/ui.test.js`; update `softwareVersion` in `docs/index.html`.
  6. Commit, then `powershell -ExecutionPolicy Bypass -File release.ps1`. It runs the tests and writes into `dist/release`:
     - all-in-one: `LiquidGlass-for-Adobe-<ver>-Setup.exe` (Windows, both apps) and `…-AIO-Windows-Mac.zip`;
     - each plugin: `LiquidGlass-for-Adobe-Photoshop-<ver>.ccx`, `LiquidGlass-for-Adobe-Illustrator-<ver>-Windows.zip`;
     - each plugin's source: `…-Photoshop-<ver>-source.zip`, `…-Illustrator-<ver>-source.zip`.
  7. Push, then attach every file in `dist/release` to the GitHub release (the owner wants all three groups).
- Be honest in public text: say "tested on Windows", and label engine renders as test renders, not Photoshop screenshots.

## Roadmap for other hosts (all reuse `lg_render`)
- **Illustrator (Windows, shipped):** see [illustrator/README.md](illustrator/README.md).
  - Rasterizes the art below the shape (`srcArt`) and the shape, calls `lg_render` (glass_core as a staticlib), returns the rasters.
  - A document-changed notifier re-runs a glass when the art below it changes (newest edit time + item count of that art).
  - Settings window = `illustrator/ui/panel.html` in WebView2; `build.ps1` inlines `photoshop/looks.js` and the logo.
  - Lengths are stored in **pt** and converted to px (2 px per pt, capped at 4 MP) at render: `kLengths` lists them.
  - Tried and removed: a toolbar tool and Alt + double-click (did not work for the owner in Illustrator 2026).
  - **Mac:** needs an Xcode build on a Mac, the Mac SDK and a WKWebView settings window. Not started.
- **After Effects and Premiere Pro (Windows, built, not released):** see [aftereffects/README.md](aftereffects/README.md).
  - One `.aex` installed in `Common\Plug-ins\7.0\MediaCore\CGSudo`, which both apps load.
  - Backdrop: the effect's input, usually on an adjustment layer. Mask: keyframeable shape parameters, because Premiere's layer pickers are weak; in After Effects, optionally a layer's alpha.
  - Render: `lg_render` on the shape's bounds plus `Margin`; the rest of the frame passes through at full bit depth. After Effects uses SmartFX and MFR, Premiere `PF_Cmd_RENDER` with BGRA 8-bit and 32-bit float.
  - Testing next to someone's open After Effects: `AfterFX.exe -m -r` starts a second instance that may reopen their last project, so the test script checks an environment variable only that launch has. Never force-kill a test instance: the next launch shows the Crash Repair prompt.
- **Engine work that helps every host:**
  - linear-light blur and refraction;
  - a GPU port of the pipeline;
  - light/dark adaptive tint.

Adobe SDKs are not in this repo: their licence forbids redistribution. Download them from developer.adobe.com/console.
