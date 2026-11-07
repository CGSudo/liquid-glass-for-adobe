// Help is keyed by saved ID so duplicate labels and appended parameters stay unambiguous.
#pragma once
#include "GlassHost.h"

namespace studio {
inline const char* GlassFromHelp()
{
	return "Uses a hidden source track's outline on an adjustment layer; choose None for built-in shapes.";
}
inline const char* RowHelp(int id, bool premiere = false)
{
	switch (id) {
	case P_FROST: return "How frosted the glass is: blurs everything behind it, in pixels. 0 = crystal clear.";
	case P_LENS: return "How much the glass bends the picture behind it, like a magnifying lens. 0 = flat window.";
	case P_EDGE: return "How wide the rounded rim is, in pixels. Wider = rounder glass, but a smaller clear middle.";
	case P_SHINE: return "How bright the shiny highlights on the edge are.";
	case P_SOFT: return "Low = a crisp, sharp rim. High = a soft, bubble-like rim.";
	case P_DISP: return "Light dispersion: splits light into a thin rainbow where the edge bends it, like a prism.";
	case P_MERGE: return "Shapes closer than this many pixels melt together like drops of liquid.";
	case P_ANGLE: return "Where the light comes from. Drag the dial; 0\xB0 is the top.";
	case P_BRIGHT: return "Makes the glass darker (left) or lighter (right).";
	case P_VIVID: return "How colourful things look through the glass. 1 = unchanged.";
	case P_TINT: return "Colours the glass, like tinted sunglasses.";
	case P_TINT_AMT: return "How strongly the tint colour shows. 0 = no tint.";
	case P_SH_DARK: return "How dark the shadow under the glass is. 0 = no shadow.";
	case P_SH_SOFT: return "How blurry the shadow's edge is, in pixels.";
	case P_SH_DROP: return "How far the shadow falls below the glass, in pixels.";
	case P_DIM: return "How much the background behind the glass is darkened.";
	case P_DIM_SPREAD: return "How far the darkening reaches outside the glass, in pixels.";
	case P_GLOW: return "How strong the glow is.";
	case P_GLOW_SIZE: return "How far the glow reaches in from the edge, in pixels.";
	case P_GLOW_COLOR: return "The colour of the glow.";
	case P_GLOW_BLEND: return "How the glow mixes with the glass. Normal paints the colour; Screen and Add only brighten; Overlay and Soft light add contrast; Multiply darkens.";
	case P_ST_WIDTH: return "How thick the stroke is, in pixels. It sits just inside the edge.";
	case P_ST_FILL: return "Solid: one colour. Gradient: blends from the colour to the end colour across the shape.";
	case P_ST_COLOR: return "The stroke colour (where a gradient starts).";
	case P_ST_OPACITY: return "How solid the stroke is (where a gradient starts).";
	case P_ST_COLOR2: return "Gradient only: the colour the stroke blends to.";
	case P_ST_OPACITY2: return "Gradient only: how solid the stroke is where the gradient ends. Low = a line that fades away.";
	case P_ST_ANGLE: return "Gradient only: which way the gradient runs. 0 puts the start colour at the top.";
	case P_GRAIN: return "How visible the grain is.";
	case P_GRAIN_SIZE: return "Size of each grain speck, in pixels. Set by the film format; fine-tune here.";
	case P_EMBOSS: return "How raised the edge looks: the side facing the light gets brighter, the far side darker.";
	case P_RIM: return "Width of the thin bright line that runs around the edge, in pixels.";
	case P_IOR: return "How strongly the material bends light: 1.0 = air (no bend), 1.5 = glass, 2.4 = diamond.";
	case P_SHEEN: return "Soft, broad shine across the curved edge. 1 = default, 0 = none.";
	case P_LIQUID_LETTERS: return "Lets Merge melt nearby letters together; leave off to keep text easy to read.";
	case P_THRESHOLD: return "Chooses which bright areas become glass; raise it to keep only highlights.";
	case P_MASK_SOFTNESS: return "Softens the brightness cutoff; use 0 for hard edges or 1 for smooth shading.";
	case P_INVERT: return "Makes dark areas glass instead of bright ones; transparent pixels stay clear.";
	case P_SHAPE_LAYER: return premiere ? GlassFromHelp() : "Uses another layer's outline; precompose it first to include its movement and scale.";
	case P_FOLLOW: return "Turns the light toward the glass's movement; enable for moving built-in shapes.";
	default: return "";
	}
}
inline const char* TabHelp(const char* label)
{
	if (!std::strcmp(label, "Looks")) return "Pick a material, then fine-tune it in Quick adjust.";
	if (!std::strcmp(label, "Quick adjust")) return "Start with Frost and Lens to set how clearly the background shows.";
	if (!std::strcmp(label, "Lens")) return "Tune the rim and rainbow edges, then use Merge to join nearby outlines.";
	if (!std::strcmp(label, "Light")) return "Turn Direction to place the highlight on the side facing your scene's light.";
	if (!std::strcmp(label, "Colour")) return "Choose a tint and raise Tint amount to colour the glass.";
	if (!std::strcmp(label, "Drop shadow")) return "A soft shadow under the glass, so it seems to float.";
	if (!std::strcmp(label, "Dim background")) return "Darkens what's behind and around the glass so it stands out on busy images.";
	if (!std::strcmp(label, "Inner glow")) return "A soft light just inside the edge, like light caught inside the glass.";
	if (!std::strcmp(label, "Stroke")) return "A line just inside the edge of the glass, in one colour or a gradient.";
	if (!std::strcmp(label, "Film grain")) return "Adds a fine film-photo texture to the glass.";
	if (!std::strcmp(label, "Advanced")) return "Fine-tune the raised edge and glass type after choosing a look.";
	if (!std::strcmp(label, "Brightness shape")) return "Use Threshold and Softness to turn bright areas into a glass outline.";
	if (!std::strcmp(label, "About")) return "Find CGSudo, source code and updates using the links below.";
	return "";
}
}
