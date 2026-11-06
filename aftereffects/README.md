# Liquid Glass for After Effects and Premiere Pro

One native effect (`LiquidGlass.aex`, Windows) that runs in both apps: **Effects > CGSudo > Liquid Glass**. It uses the same Rust engine as the Photoshop and Illustrator plug-ins, and every setting can be keyframed. Tested on Windows, in After Effects 2026 and Premiere Pro 2026.

## Using it
**Adjustment layer (the normal way, both apps):** add an adjustment layer, apply Liquid Glass, and set the shape in **Glass shape**. Everything under the adjustment layer is what the glass bends. Animate Center, Width, Height, Corner radius and Rotation like any other property.

**Look** applies one of the 9 built-in looks. It keeps Edge width and Merge, as in Photoshop, then snaps back so you can pick the same look again. It sets the values at the current time, so a look can be keyframed by picking it at another time.

**Second shape (liquid merge):** turn on **Add second shape**, then animate the two centres toward each other. The shapes fuse within the **Merge** distance.

**Animation** adds **Wobble** and **Wobble speed** (the outline jiggles over time). **Light > Light follows motion** turns the light toward the direction the glass is moving, and eases back to **Direction** when it stops.

**Text or a graphic as the glass shape:**
- **Shape from layer** (adjustment-layer setup) uses another layer's alpha, for example a hidden text layer.
- **Use clip alpha as shape** applies the effect directly on the text or graphic. An effect can't see what is under its own layer, so also choose a **Backdrop layer**. Without one, the glass refracts the clip's own colours.

Layer pickers give you a layer's source, with position and scale ignored, like any After Effects effect with a layer picker. Effects on the picked layer only count if you switch the picker's menu to **Effects & Masks**. To follow an animated or transformed layer, precompose it.

## Why this design
- **The effect's input is the backdrop, and the shape is parameters.** This works the same in both apps and keyframes natively. Layer pickers are only an extra for After Effects, because Premiere's support for them is weak.
- **The glass replaces only its own area.** Outside the shape and its shadow, the input passes through unchanged at its full bit depth. The effect therefore keeps the full frame as its output, and the speed saving comes from running the engine only on the shape's bounds plus lens, frost, shadow and dim reach (`Margin`).
- **Two shapes merge analytically** with the same smooth minimum as the engine's `smooth_union`. That avoids four distance transforms per frame.
- **The two apps render differently.** After Effects uses SmartFX and Multi-Frame Rendering. Premiere uses `PF_Cmd_RENDER` with BGRA 8-bit and 32-bit float, declared through `PF_PixelFormatSuite`.

## Limits
- **8-bit engine:** in 16-bit and 32-bit projects, the area inside the glass is computed at 8 bits and HDR values are clipped there. Everything outside the glass stays at full depth. Marked `shortcut:` in `Source/GlassHost.h`.
- **Colours:** the engine treats values as display-referred, so in a linear working space the glass looks a little different.
- **Not confirmed yet in Premiere:** the Look popup and the layer pickers.
- **Windows only.**
- **No `.ffx` presets:** After Effects has no scripting call to save one, so `make_test_project.jsx` shows the setups as a script instead.

## Build and install
Needs the VS C++ Build Tools, Rust, and the [After Effects SDK](https://developer.adobe.com/console), which is never committed because its licence forbids redistribution.
```bash
powershell -ExecutionPolicy Bypass -File aftereffects/build.ps1
```
This installs into `C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\CGSudo\` through a UAC prompt, where both apps load it. Close both apps first if they have the effect loaded. Add `-NoInstall` to only build, and `-SDK <Examples folder>` if the SDK is elsewhere.

## Tests
- `npm test` includes `aftereffects/params.test.js`: same defaults, looks, labels and ranges as the Photoshop panel.
- `test/run_test.ps1`: host logic without either app (mapping, shapes, every pixel format, buffer origins, 1080p timing).
- `test/verify_ae.ps1`: real After Effects. Builds comps by script in a separate instance, renders them with `aerender`, and times them. A project open in another After Effects is left alone.
- `test/verify_pr.ps1`: real Premiere Pro, which must be closed first. Applies the effect to a clip, keyframes it and exports it plain, keyframed, with Wobble and with Light follows motion. Compare the frames by eye. Pass `-Out` to keep the exports off a full C: drive.
