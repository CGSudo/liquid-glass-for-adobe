# Liquid Glass for After Effects

Liquid Glass for Adobe 0.8.0 ships **Liquid Glass** and **Liquid Glass Brightness** for After Effects 2026 on Windows. The owner tested the main effect in After Effects on Windows. Premiere Pro support is coming and is not released. Both effects share the Rust engine and Glass Studio controls.

## Using it
**After Effects, directly on a layer:** apply Liquid Glass to a shape, text, footage, image, solid or pre-comp layer. The effect uses its input alpha as the outline and ignores its colour. Everything below becomes the backdrop through an automatically created empty adjustment shape layer named **Liquid Glass backdrop**, directly below the glass layer. Creation happens on the next AE idle callback; until ready, the input passes through. Keep the helper directly below its glass layer. Cameras, lights, nulls and sources without video pass through without creating a helper.

**Text:** **Lens > Liquid letters** defaults off, so letters retain their separate outlines. Enable it to let **Merge** fuse nearby letters. The checkbox has no effect on other layer types: shapes and raster layers retain merging. It is hidden in the native parameter list for non-text glass layers; Glass Studio may still show it.

**Liquid Glass Brightness (After Effects only):** apply it directly to a raster layer or pre-comp. Its mask is Rec.709 luma (`0.2126 R + 0.7152 G + 0.0722 B`) multiplied by input alpha. **Brightness shape > Threshold**, **Softness** and **Invert** control the luma coverage before alpha multiplication. Threshold 0.5 and Softness 1 preserve luma directly; Softness 0 makes a hard threshold. Invert reverses the coverage but keeps transparent pixels transparent. Channels are normalized for 8-, 16- and 32-bit input, and HDR luma is clamped to 0–1. Backdrop, looks, tint and Glass Studio are shared with the main effect.

**Tint:** choose **Colour > Tint colour** and increase **Tint amount** (default 0). AE colour parameters are read through its floating-point colour suite, including in 16- and 32-bit projects; the renderer remains 8-bit inside the glass.

**Look** applies one of the 9 built-in looks. It keeps Edge width and Merge, as in Photoshop, then snaps back so you can pick the same look again. It sets the values at the current time, so a look can be keyframed by picking it at another time.

## Premiere Pro 2026: existing graphics and alpha clips (not released)

This section documents experimental development, not a released workflow. **The new alpha workflows have not been run in Premiere.** The owner previously confirmed the adjustment-layer input path; the SDK-free tests prove the compositor, not Premiere's track checkout or background delivery.

### What the APIs actually provide

- `PF_ADD_LAYER` provides a Premiere track selector. `PF_CHECKOUT_PARAM` reads the selected source; it is not an API for the composite of every track underneath. Adobe engineering describes the clip boundary and recommends precomposing for effects on a selected source ([Adobe SDK discussion](https://community.adobe.com/questions-729/questions-about-effects-plugin-sdk-for-premiere-pro-checking-out-layers-and-ui-1355997)). Nest transformed/effected graphics when source-space alpha does not match the sequence. Native graphics and nested-sequence checkout still need the real-app test.
- An adjustment layer receives the composition below it, consistent with [Adobe's adjustment-layer workflow](https://helpx.adobe.com/sg/premiere/desktop/add-video-effects/apply-video-effects/create-adjustment-layers.html) and the owner's earlier test. This does **not** remove an opaque source graphic already included in that composition.
- The installed `PrSDKAESupport.h` declares `PF_BackgroundFrameSuite1`: register supported transfer modes, then request a background frame. The header explicitly allows no frame to be returned. The new direct-clip path probes this optional suite and composites Normal/source-over when it supplies a compatible frame. **A declaration is not evidence that Premiere 2026 supplies the frame for this effect.** Direct mode remains experimental until the verifier passes. No standalone Premiere SDK was found under `E:\AI\SDK`; no SDK files were copied.
- Checkout pixels default to ARGB8 unless an effect opts into the render format. We now call `EffectWantsCheckedOutFramesToMatchRenderPixelFormat` during setup and validate each frame's format before use ([Adobe SDK pixel-format contract](https://ae-plugins.docsforadobe.dev/ppro/bigger-differences/)). A mismatch passes the input through instead of interpreting ARGB as BGRA/float.
- Premiere supports PF custom-control events ([Adobe SDK host differences](https://ae-plugins.docsforadobe.dev/ppro/basic-host-differences/)). Glass Studio now probes its PF App, custom UI and Drawbot suites before registering the same control used in AE, and avoids AEGP status calls in Premiere. Missing suites leave the native Look popup and keyframeable controls. Suite availability alone does not prove drawing, colour picking or interaction; those need Premiere.
- UXP can [create an effect component](https://developer.adobe.com/premiere-pro/uxp/ppro-reference/classes/videofilterfactory/) and [append it to a clip](https://developer.adobe.com/premiere-pro/uxp/ppro-reference/classes/videocomponentchain/). The documented [SequenceEditor API](https://developer.adobe.com/premiere-pro/uxp/ppro-reference/classes/sequenceeditor/) can insert/clone existing media; no adjustment-layer creation API was found. CEP/ExtendScript can import existing assets, but no supported adjustment-layer factory was established. No one-click panel is shipped on the strength of undocumented QE calls. The disposable verifier uses QE only for applying the installed test effect.

### Setup and fallback

1. **Direct clip, experimental:** apply Liquid Glass to an existing alpha graphic, raster clip or nest. New Premiere instances enable **Use clip alpha as shape**. If the host supplies a compatible background, the input alpha is the outline, its RGB is ignored, and all supplied background pixels remain underneath. Empty/opaque clips and unavailable backgrounds pass through. The optional suite's composite coverage, coordinate alignment and cache invalidation must be checked in the app; this is not a promise of automatic access to other tracks.
2. **Adjustment layer with existing alpha:** place an adjustment layer over all backdrop tracks. Put the source graphic on its own video track, disable that track's visible output, and choose that track in **Glass from** (Premiere supplies the Video track N menu). Leave the graphic's opacity at 100%. The effect uses only its alpha; adjustment input supplies the backdrop. An empty selected track passes through. This depends on checkout retaining alpha while track output is disabled, which the verifier probes separately. It does not assume Track Matte Key's private suppression behaviour. Never leave the original coloured graphic visible below the adjustment layer: those pixels cannot be recovered from the composite.
3. **If isolated-track checkout fails:** use the already supported adjustment-layer built-in shapes, with **Glass from: None** and **Use clip alpha as shape** off. Existing editable text/graphic conversion is then unavailable in this build; use the AE workflow instead. No required backdrop picker is added, and the old Backdrop layer parameter stays at its original index but is hidden/ignored in Premiere.

Merge calls the shared `FuseParts` path for separate alpha islands. Text alpha and graphic alpha have no reliable type information here: with the current table, use **Merge: 0** to keep letters apart. If a checkbox labelled **Liquid letters** is added by the parallel worker, the Premiere path finds it by label and conservatively disables alpha fusion while it is off. It does not hard-code a new parameter index. Tint remains available; new Premiere instances start with zero Dispersion and zero Tint amount so a grey gradient has no default rainbow cast. Explicit looks and existing saved settings retain their chosen values. BGRA8 and BGRA32f remain supported; the shared 8-bit glass-engine limit still applies.

### Run the host gate when Premiere is closed

The manager must separately install the reviewed `build.ps1 -Test` build as **Liquid Glass (test)**. The script does not build, install, kill an app or delete previous runs. With Premiere closed:

```powershell
powershell -ExecutionPolicy Bypass -File aftereffects/test/verify_pr.ps1 -Out E:\AI\lg-pr-results
```

It generates a grey gradient, two coloured alpha shapes, a second lower-track stripe and empty/grey controls; creates disposable sequences; applies the test effect; exports H.264; decodes frames with an existing FFmpeg; prints numerical PASS/FAIL checks and exits nonzero on failure. `-FFmpeg` selects another existing binary; `-EffectName` selects a separately installed production build. No dependencies are installed. Every run gets its own directory containing fixtures, JSX, project, log and exports. `-PrepareOnly` generates fixtures/JSX without launching Premiere; `-MeasureOnly -Out <run-directory>` repeats measurement only.

The direct test checks ignored source colour, changed glass pixels, a second lower track contributing, neutrality, empty/opaque passthrough and no black frame. The isolated-track test deliberately uses a clean opaque input clip to separate checkout from adjustment creation. **It does not automatically create an adjustment layer:** that still requires a manual app check, together with native text/graphics, nested alpha, transformations, scrubbing/cache refresh, Glass Studio and maximum-depth exports. A successful 8-bit fixture export must not be reported as proof of those cases. If scripting or checkout is unavailable, the command fails with the saved project/log available for inspection.

## Why this design
- **Direct AE layers separate outline from backdrop.** `AEGP_NewFromUpstreamOfEffect` and `AEGP_RenderSuite5` read through a Slider Control on the helper. A receipt GUID and the sampling transform are mixed into SmartFX dependencies so backdrop and transform edits invalidate cached glass.
- **Raster coordinates stay in layer space.** SmartFX buffer origins are respected, including negative origins and small source buffers. `AEGP_GetLayerToWorldXform` supplies position, anchor, scale, rotation and parenting at composition time; bilinear sampling pulls the comp backdrop into that coordinate system. AE subsequently transforms the effect output. Shape/text and continuously rasterized vector footage already have transforms applied before effects, so they use identity sampling. Pre-comps use their checked-out rendered input alpha and the layer transform, including when Collapse Transformations is enabled. This follows [Adobe's render-order documentation](https://helpx.adobe.com/after-effects/desktop/work-with-compositions/precomposing-and-nesting/precomposing-nesting-pre-rendering.html); collapsed pre-comps especially need an AE render to confirm alignment.
- **3D raster layers** use the active camera pose and zoom, or AE's default camera distance, to project the backdrop onto the layer plane. Camera parenting is included. The current projection assumes square pixels.
- **The glass replaces only its own area.** Outside the shape and its shadow, the input passes through unchanged at its full bit depth. The effect therefore keeps the full frame as its output, and the speed saving comes from running the engine only on the shape's bounds plus lens, frost, shadow and dim reach (`Margin`).
- **Two shapes merge analytically** with the same smooth minimum as the engine's `smooth_union`. That avoids four distance transforms per frame.
- **The two apps render differently.** After Effects uses SmartFX and Multi-Frame Rendering. Premiere uses `PF_Cmd_RENDER` with BGRA 8-bit and 32-bit float, declared through `PF_PixelFormatSuite`.

## Limits
- **8-bit engine:** in 16-bit and 32-bit projects, the area inside the glass is computed at 8 bits and HDR values are clipped there. Everything outside the glass stays at full depth. Marked `shortcut:` in `Source/GlassHost.h`.
- **Colours:** the engine treats values as display-referred, so in a linear working space the glass looks a little different.
- **Not confirmed yet in Premiere:** the Look popup and the layer pickers.
- **Windows only.**
- **Transform verification pending:** small/moved/scaled/rotated/parented footage, cropped SmartFX buffers, ordinary and collapsed pre-comps, and 3D camera projection require real AE checks. Non-square-pixel camera projection is not implemented. Failed layer/transform/backdrop queries pass the source through.
- **No `.ffx` presets:** After Effects has no scripting call to save one, so `make_test_project.jsx` shows the setups as a script instead.

## Build and install
Needs the VS C++ Build Tools, Rust, and the [After Effects SDK](https://developer.adobe.com/console), which is never committed because its licence forbids redistribution.
```bash
powershell -ExecutionPolicy Bypass -File aftereffects/build.ps1
```
This builds both binaries from the same source; `LG_BRIGHTNESS` selects the Brightness variant with its own effect name and match name. For release, both effects install only into `C:\Program Files\Adobe\Adobe After Effects 2026\Support Files\Plug-ins\CGSudo\`, so Premiere does not discover them. Override that folder with `-AEPluginDir <folder>` for another AE installation. Copying into Program Files prompts for UAC; close the apps first if they have the effects loaded.

Add `-NoInstall` to only build, and `-SDK <Examples folder>` if the SDK is elsewhere. `-Test` never installs: it builds **Liquid Glass (test)** and **Liquid Glass Brightness (test)**, then runs the Brightness host self-check. Output filenames remain `LiquidGlass.aex` and `LiquidGlassBrightness.aex` under `aftereffects/build/`.

Saved parameter IDs and the 42-value core Params layout are unchanged. Liquid letters has a newly appended saved ID and a row inside Lens; later runtime indices move by one and are always obtained through `IndexOf`. Brightness-only rows append after all shared rows, preserving shared indices between binaries. Parameter tests cover both layouts.

**About** shows the effect name, version, by CGSudo, and these text links: https://www.linkedin.com/in/cgsudo and https://github.com/CGSudo/liquid-glass-for-adobe.

## Tests
- `npm test` includes `aftereffects/params.test.js`: same defaults, looks, labels and ranges as the Photoshop panel.
- `test/run_test.ps1`: host logic without either app (parameter layout, text separation, luma masks and tint at every AE depth, affine/camera mapping, buffer origins, shapes, every pixel format, 1080p timing).
- `test/verify_ae.ps1`: real After Effects. Builds comps by script in a separate instance, renders them with `aerender`, and times them. A project open in another After Effects is left alone.
- `test/verify_pr.ps1`: real Premiere Pro, which must be closed first. Generates alpha/backdrop fixtures, exports disposable sequences and prints numerical PASS/FAIL checks. See the Premiere section for setup, fallback and remaining manual checks. Pass `-Out` to keep exports off a full C: drive.
