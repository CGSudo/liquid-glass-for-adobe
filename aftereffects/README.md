# Liquid Glass for After Effects and Premiere Pro

Liquid Glass for Adobe, by CGSudo, has a shared native effect (`LiquidGlass.aex`, Windows) for both apps: **Effects > CGSudo > Liquid Glass**. A second binary, `LiquidGlassBrightness.aex`, provides **Effects > CGSudo > Liquid Glass Brightness** in After Effects only. Both use the same Rust engine and Glass Studio controls. These effects are built but not released or included in Setup.exe. The earlier shared effect was tested on Windows in both 2026 apps; the new AV-layer, text and Brightness behaviour below still needs real-app verification.

## Using it
**After Effects, directly on a layer:** apply Liquid Glass to a shape, text, footage, image, solid or pre-comp layer. The effect uses its input alpha as the outline and ignores its colour. Everything below becomes the backdrop through an automatically created empty adjustment shape layer named **Liquid Glass backdrop**, directly below the glass layer. Creation happens on the next AE idle callback; until ready, the input passes through. Keep the helper directly below its glass layer. Cameras, lights, nulls and sources without video pass through without creating a helper.

**Text:** **Lens > Liquid letters** defaults off, so letters retain their separate outlines. Enable it to let **Merge** fuse nearby letters. The checkbox has no effect on other layer types: shapes and raster layers retain merging. It is hidden in the native parameter list for non-text glass layers; Glass Studio may still show it.

**Adjustment layers (both apps):** the existing behaviour is unchanged. Apply Liquid Glass and set the shape in **Glass shape**. Everything under the adjustment layer is what the glass bends. Animate Center, Width, Height, Corner radius and Rotation like any other property. Brightness also retains this adjustment-layer behaviour; put it directly on a raster or pre-comp layer to use its brightness mask.

**Liquid Glass Brightness (After Effects only):** apply it directly to a raster layer or pre-comp. Its mask is Rec.709 luma (`0.2126 R + 0.7152 G + 0.0722 B`) multiplied by input alpha. **Brightness shape > Threshold**, **Softness** and **Invert** control the luma coverage before alpha multiplication. Threshold 0.5 and Softness 1 preserve luma directly; Softness 0 makes a hard threshold. Invert reverses the coverage but keeps transparent pixels transparent. Channels are normalized for 8-, 16- and 32-bit input, and HDR luma is clamped to 0–1. Backdrop, looks, tint and Glass Studio are shared with the main effect.

**Tint:** choose **Colour > Tint colour** and increase **Tint amount** (default 0). AE colour parameters are read through its floating-point colour suite, including in 16- and 32-bit projects; the renderer remains 8-bit inside the glass.

**Look** applies one of the 9 built-in looks. It keeps Edge width and Merge, as in Photoshop, then snaps back so you can pick the same look again. It sets the values at the current time, so a look can be keyframed by picking it at another time.

**Second shape (liquid merge):** turn on **Add second shape**, then animate the two centres toward each other. The shapes fuse within the **Merge** distance.

**Animation** adds **Wobble** and **Wobble speed** (the outline jiggles over time). **Light > Light follows motion** turns the light toward the direction the glass is moving, and eases back to **Direction** when it stops.

**Optional layer pickers, for the adjustment-layer setup:**
- **Shape from layer** (adjustment-layer setup) uses another layer's alpha, for example a hidden text layer.
- **Use clip alpha as shape** and **Backdrop layer** remain available for the older manual setup. Direct AE glass layers use their own input outline and the automatic backdrop instead of these pickers. Premiere retains its existing clip-alpha behaviour.

Layer pickers give you a layer's source, with position and scale ignored, like any After Effects effect with a layer picker. Effects on the picked layer only count if you switch the picker's menu to **Effects & Masks**. To follow an animated or transformed layer, precompose it.

## Why this design
- **Direct AE layers separate outline from backdrop.** `AEGP_NewFromUpstreamOfEffect` and `AEGP_RenderSuite5` read through a Slider Control on the helper. A receipt GUID and the sampling transform are mixed into SmartFX dependencies so backdrop and transform edits invalidate cached glass.
- **Raster coordinates stay in layer space.** SmartFX buffer origins are respected, including negative origins and small source buffers. `AEGP_GetLayerToWorldXform` supplies position, anchor, scale, rotation and parenting at composition time; bilinear sampling pulls the comp backdrop into that coordinate system. AE subsequently transforms the effect output. Shape/text and continuously rasterized vector footage already have transforms applied before effects, so they use identity sampling. Pre-comps use their checked-out rendered input alpha and the layer transform, including when Collapse Transformations is enabled. This follows [Adobe's render-order documentation](https://helpx.adobe.com/after-effects/desktop/work-with-compositions/precomposing-and-nesting/precomposing-nesting-pre-rendering.html); collapsed pre-comps especially need an AE render to confirm alignment.
- **3D raster layers** use the active camera pose and zoom, or AE's default camera distance, to project the backdrop onto the layer plane. Camera parenting is included. The current projection assumes square pixels.
- **Adjustment-layer input remains the backdrop.** The shape is parameters; this works in both apps and keyframes natively. Layer pickers are only an extra for After Effects, because Premiere's support for them is weak.
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
This builds both binaries from the same source; `LG_BRIGHTNESS` selects the Brightness variant with its own effect name and match name. The shared `LiquidGlass.aex` installs into `C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\CGSudo\`. `LiquidGlassBrightness.aex` installs only into `C:\Program Files\Adobe\Adobe After Effects 2026\Support Files\Plug-ins\CGSudo\`, so Premiere does not discover it. Override that folder with `-AEPluginDir <folder>` for another AE installation. Copying into Program Files prompts for UAC; close the apps first if they have the effects loaded.

Add `-NoInstall` to only build, and `-SDK <Examples folder>` if the SDK is elsewhere. `-Test` never installs: it builds **Liquid Glass (test)** and **Liquid Glass Brightness (test)**, then runs the Brightness host self-check. Output filenames remain `LiquidGlass.aex` and `LiquidGlassBrightness.aex` under `aftereffects/build/`.

Saved parameter IDs and the 42-value core Params layout are unchanged. Liquid letters has a newly appended saved ID and a row inside Lens; later runtime indices move by one and are always obtained through `IndexOf`. Brightness-only rows append after all shared rows, preserving shared indices between binaries. Parameter tests cover both layouts.

**About** shows the effect name, version, by CGSudo, and these text links: https://www.linkedin.com/in/cgsudo and https://github.com/CGSudo/liquid-glass-for-adobe.

## Tests
- `npm test` includes `aftereffects/params.test.js`: same defaults, looks, labels and ranges as the Photoshop panel.
- `test/run_test.ps1`: host logic without either app (parameter layout, text separation, luma masks and tint at every AE depth, affine/camera mapping, buffer origins, shapes, every pixel format, 1080p timing).
- `test/verify_ae.ps1`: real After Effects. Builds comps by script in a separate instance, renders them with `aerender`, and times them. A project open in another After Effects is left alone.
- `test/verify_pr.ps1`: real Premiere Pro, which must be closed first. Applies the effect to a clip, keyframes it and exports it plain, keyframed, with Wobble and with Light follows motion. Compare the frames by eye. Pass `-Out` to keep the exports off a full C: drive.
