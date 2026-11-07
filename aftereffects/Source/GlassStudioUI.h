// Included after the host's look action; only PF_Cmd_EVENT accesses this UI state.
#pragma once
#include "GlassStudioModel.h"
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")

namespace studio_ui {
using studio::Box;
using studio::Hit;
using studio::Target;
using studio::None;
using studio::Blob;
using studio::LookCard;
using studio::TabButton;
using studio::Control;
using studio::AboutLink;
struct State {
	int tab = 0;
	Hit hover;
	Hit pressed;
	studio::Skin skin;
	bool textLayer = false, savedText = false, savedLiquid = false;
	int letterWidth = 0, letterHeight = 0;
	std::vector<unsigned char> letters;
	float bx[2] = { 0.39f, 0.61f }, by[2] = { 0.5f, 0.5f };
	float savedCore[kCoreCount] = {}, savedX[2] = {}, savedY[2] = {};
	int savedWidth = 0;
	int thumbWidth[kLookCount] = {}, thumbHeight[kLookCount] = {};
	std::vector<unsigned char> hero, thumbs[kLookCount];
	std::string status;
};
// shortcut: tabs, hover and demo positions are shared between effect windows, use per-context state if independent windows are needed.
static State state;
static const std::vector<studio::Tab> tabs = studio::Tabs();

static DRAWBOT_ColorRGBA Colour(unsigned hex)
{
	return { ((hex >> 16) & 255) / 255.f, ((hex >> 8) & 255) / 255.f, (hex & 255) / 255.f, 1 };
}
const unsigned Background = 0x141716, Green = 0x45c891,
	Text = 0xf2f7f4, Muted = 0xc0d0c8, Border = 0x5b766c;

struct Canvas {
	AEFX_DrawbotSuitesScoper scope;
	DRAWBOT_Suites* s;
	DRAWBOT_SupplierRef supplier = nullptr;
	DRAWBOT_SurfaceRef surface = nullptr;
	float ox, oy;
	Canvas(PF_InData* in, PF_OutData* out, PF_EventExtra* e) : scope(in, out), s(scope.Get()),
		ox((float)e->effect_win.current_frame.left), oy((float)e->effect_win.current_frame.top)
	{
		AEFX_SuiteScoper<PF_EffectCustomUISuite1> ui(in, kPFEffectCustomUISuite, kPFEffectCustomUISuiteVersion1, out);
		DRAWBOT_DrawRef ref = nullptr;
		PF_Err err = ui->PF_GetDrawingReference(e->contextH, &ref);
		if (err) throw err;
		DRAWBOT_ErrorToException(s->drawbot_suiteP->GetSupplier(ref, &supplier));
		DRAWBOT_ErrorToException(s->drawbot_suiteP->GetSurface(ref, &surface));
	}
	void Rect(Box b, unsigned hex)
	{
		DRAWBOT_RectF32 r = { ox + b.x, oy + b.y, b.w, b.h };
		auto c = Colour(hex);
		DRAWBOT_ErrorToException(s->surface_suiteP->PaintRect(surface, &c, &r));
	}
	void Round(Box b, float radius, unsigned hex, bool outline = false)
	{
		DRAWBOT_PathP path(s->supplier_suiteP, supplier);
		const float x = ox + b.x, y = oy + b.y, r = std::min(radius, std::min(b.w, b.h) / 2);
		DRAWBOT_PointF32 centers[] = { {x + b.w - r, y + r}, {x + b.w - r, y + b.h - r}, {x + r, y + b.h - r}, {x + r, y + r} };
		DRAWBOT_ErrorToException(s->path_suiteP->MoveTo(path, x + r, y));
		for (int i = 0; i < 4; i++) {
			const float angle = float(i * 90 - 90);
			const float rad = angle * 3.14159265f / 180;
			DRAWBOT_ErrorToException(s->path_suiteP->LineTo(path, centers[i].x + r * std::cos(rad), centers[i].y + r * std::sin(rad)));
			DRAWBOT_ErrorToException(s->path_suiteP->AddArc(path, &centers[i], r, angle, 90));
		}
		DRAWBOT_ErrorToException(s->path_suiteP->Close(path));
		auto c = Colour(hex);
		if (outline) {
			DRAWBOT_PenP pen(s->supplier_suiteP, supplier, &c, 1);
			DRAWBOT_ErrorToException(s->surface_suiteP->StrokePath(surface, pen, path));
		} else {
			DRAWBOT_BrushP brush(s->supplier_suiteP, supplier, &c);
			DRAWBOT_ErrorToException(s->surface_suiteP->FillPath(surface, brush, path, kDRAWBOT_FillType_Default));
		}
	}
	void Label(const char* text, float x, float y, unsigned hex = Text, float size = studio::kLabelSize, float width = 380)
	{
		std::vector<DRAWBOT_UTF16Char> wide;
		for (const unsigned char* p = (const unsigned char*)text; *p; p++) wide.push_back(*p);
		wide.push_back(0);
		auto c = Colour(hex);
		DRAWBOT_BrushP brush(s->supplier_suiteP, supplier, &c);
		DRAWBOT_FontP font(s->supplier_suiteP, supplier, size);
		DRAWBOT_PointF32 origin = { ox + x, oy + y };
		DRAWBOT_ErrorToException(s->surface_suiteP->DrawString(surface, brush, font, wide.data(), &origin,
			kDRAWBOT_TextAlignment_Left, kDRAWBOT_TextTruncation_EndEllipsis, width));
	}
	void Image(const std::vector<unsigned char>& pixels, int w, int h, float x, float y)
	{
		DRAWBOT_ImageRef image = nullptr;
		DRAWBOT_ErrorToException(s->supplier_suiteP->NewImageFromBuffer(supplier, w, h, w * 4,
			kDRAWBOT_PixelLayout_32BGRA_Straight, pixels.data(), &image));
		SharedRefImpl<DRAWBOT_ImageRef> owned(s->supplier_suiteP);
		owned.Reset(image, false);
		DRAWBOT_PointF32 origin = { ox + x, oy + y };
		DRAWBOT_ErrorToException(s->surface_suiteP->DrawImage(surface, image, &origin, 1));
	}
	void Glass(Box b, bool active = false)
	{
		const auto* skin = state.skin.Find(b, active);
		if (skin) Image(skin->pixels, (int)std::ceil(b.w), (int)std::ceil(b.h), b.x, b.y);
	}
	void Thumb(float x, float y)
	{
		const auto* skin = state.skin.Find({ 0, 0, 20, 20 }, true);
		if (skin) Image(skin->pixels, 20, 20, x - 10, y - 10);
		Round({ x - 9, y - 9, 18, 18 }, 9, 0xc1f9df, true);
	}
	void Line(std::initializer_list<DRAWBOT_PointF32> points, float x, float y, unsigned hex = Muted)
	{
		DRAWBOT_PathP path(s->supplier_suiteP, supplier);
		bool first = true;
		for (auto p : points) {
			if (first) { DRAWBOT_ErrorToException(s->path_suiteP->MoveTo(path, ox + x + p.x, oy + y + p.y)); }
			else { DRAWBOT_ErrorToException(s->path_suiteP->LineTo(path, ox + x + p.x, oy + y + p.y)); }
			first = false;
		}
		auto colour = Colour(hex);
		DRAWBOT_PenP pen(s->supplier_suiteP, supplier, &colour, 1.35f);
		DRAWBOT_ErrorToException(s->surface_suiteP->StrokePath(surface, pen, path));
	}
	// Coordinates follow the shared 16 px line icons in photoshop/looks.js.
	void Icon(const char* name, float x, float y, unsigned colour = Muted)
	{
		auto is = [&](const char* n) { return !std::strcmp(name, n); };
		auto line = [&](std::initializer_list<DRAWBOT_PointF32> p) { Line(p, x, y, colour); };
		auto circle = [&](float cx, float cy, float r) { Round({ x + cx - r, y + cy - r, r * 2, r * 2 }, r, colour, true); };
		auto rect = [&](float a, float b, float w, float h, float r) { Round({ x + a, y + b, w, h }, r, colour, true); };
		if (is("presets")) { for (int j : {2, 9}) for (int i : {2, 9}) rect((float)i, (float)j, 5, 5, 1.2f); }
		else if (is("advanced")) {
			line({{2,4},{9,4}}); line({{12,4},{14,4}}); line({{2,8},{4,8}}); line({{7,8},{14,8}}); line({{2,12},{11,12}});
			circle(10.5f,4,1.5f); circle(5.5f,8,1.5f); circle(12.5f,12,1.5f);
		} else if (is("lens")) { circle(6.8f,6.8f,4.6f); line({{10.2f,10.2f},{14,14}}); line({{4.6f,6.2f},{5.2f,5},{6.4f,4.4f}}); }
		else if (is("sun")) {
			circle(8,8,2.6f); line({{8,1.5f},{8,3.1f}}); line({{8,12.9f},{8,14.5f}}); line({{1.5f,8},{3.1f,8}}); line({{12.9f,8},{14.5f,8}});
			line({{3.4f,3.4f},{4.5f,4.5f}}); line({{11.5f,11.5f},{12.6f,12.6f}}); line({{3.4f,12.6f},{4.5f,11.5f}}); line({{11.5f,4.5f},{12.6f,3.4f}});
		} else if (is("frost")) {
			line({{8,1.8f},{8,14.2f}}); line({{2.6f,4.9f},{13.4f,11.1f}}); line({{2.6f,11.1f},{13.4f,4.9f}});
			line({{6.4f,2.6f},{8,4.2f},{9.6f,2.6f}}); line({{6.4f,13.4f},{8,11.8f},{9.6f,13.4f}});
		} else if (is("diamond")) {
			line({{4.5f,2.5f},{11.5f,2.5f},{14.5f,6.1f},{8,14},{1.5f,6.1f},{4.5f,2.5f}});
			line({{1.5f,6.1f},{14.5f,6.1f}}); line({{6,2.5f},{5,6.1f},{8,14},{11,6.1f},{10,2.5f}});
		} else if (is("shadow")) { rect(2,2,8.5f,8.5f,2); line({{12.5f,5.5f},{12.5f,12},{12,13},{11,13.5f},{5.5f,13.5f}}); }
		else if (is("glow") || is("stroke")) { rect(2,2,12,12,3.5f); rect(4.5f,4.5f,7,7,1.8f); }
		else if (is("film")) {
			rect(3,1.8f,10,12.4f,1.2f); line({{5.5f,1.8f},{5.5f,14.2f}}); line({{10.5f,1.8f},{10.5f,14.2f}});
			for (float v : {4.5f,8.f,11.5f}) { line({{3,v},{5.5f,v}}); line({{10.5f,v},{13,v}}); }
		} else if (is("info")) { circle(8,8,6.2f); line({{8,7.2f},{8,11.2f}}); circle(8,4.9f,0.35f); }
		else if (is("blend")) { circle(6,8,4.2f); circle(10,8,4.2f); }
		else if (is("palette")) {
			// The palette outline is sampled from the shared SVG's circular bowl and paint well.
			line({{8,2},{5,2.8f},{2.8f,5},{2,8},{2.8f,11},{5,13.2f},{8,14},{8.8f,13.4f},{8.8f,12.4f},{8.5f,11.5f},{9.7f,10.7f},{11,10.7f},{13.1f,9.8f},{14,7.7f},{13.2f,5},{11,2.8f},{8,2}});
			circle(5,7.5f,0.6f); circle(7.4f,4.8f,0.6f); circle(10.6f,5.4f,0.6f);
		} else if (is("moon")) { line({{13,10.2f},{11,11},{8.5f,10.8f},{6.5f,9.2f},{5.4f,6.5f},{5.8f,3},{3.4f,4.4f},{2.4f,7.2f},{3,10.2f},{5,12.4f},{8,13.2f},{11,12.4f},{13,10.2f}}); }
		else { rect(2,3,12,10,3.5f); line({{4.5f,6.5f},{5,5.2f},{6.5f,4.5f}}); }
	}
	void Dial(Box b, float angle, bool hover)
	{
		float radius = b.w / 2, cx = b.x + radius, cy = b.y + radius;
		Glass(b);
		Round(b, radius, hover ? Green : Border, true);
		const float a = angle * 3.14159265f / 180;
		Icon("sun", cx - 8, cy - 8, 0xffda91);
		float x = cx + std::sin(a) * (radius - 3), y = cy - std::cos(a) * (radius - 3);
		Round({ x - 4, y - 4, 8, 8 }, 4, 0xffe9bd);
	}
};

static float Value(const Row& r, const PF_ParamDef& p)
{
	switch (r.kind) {
	case K_FLOAT: return (float)p.u.fs_d.value;
	case K_ANGLE: return (float)FIX_2_FLOAT(p.u.ad.value);
	case K_POPUP: return (float)p.u.pd.value;
	case K_CHECK: return p.u.bd.value ? 1.f : 0.f;
	default: return 0;
	}
}
static bool SetValue(const Row& r, PF_ParamDef& p, float v)
{
	if (v == Value(r, p)) return false;
	switch (r.kind) {
	case K_FLOAT:
		v = studio::Clamp(v, r.min, r.max);
		if (v == Value(r, p)) return false;
		p.u.fs_d.value = v; break;
	case K_ANGLE: {
		PF_Fixed angle = FLOAT2FIX(std::remainder(v, 360.f));
		if (angle == p.u.ad.value) return false;
		p.u.ad.value = angle; break;
	}
	case K_POPUP:
		if ((A_long)v == p.u.pd.value) return false;
		p.u.pd.value = (A_long)v; break;
	case K_CHECK:
		if ((v != 0) == (p.u.bd.value != 0)) return false;
		p.u.bd.value = v != 0; break;
	default: return false;
	}
	p.uu.change_flags |= PF_ChangeFlag_CHANGED_VALUE;
	return true;
}
static void Core(PF_ParamDef* params[], float* core)
{
	std::copy(kDefaults, kDefaults + kCoreCount, core);
	for (int i = 0; i < kRowCount; i++) {
		const Row& r = kRows[i];
		if (r.core < 0) continue;
		const PF_ParamDef& p = *params[i + 1];
		if (r.kind == K_COLOR) {
			core[r.core] = p.u.cd.value.red / 255.f;
			core[r.core + 1] = p.u.cd.value.green / 255.f;
			core[r.core + 2] = p.u.cd.value.blue / 255.f;
		} else core[r.core] = Value(r, p) - (r.kind == K_POPUP ? 1.f : 0.f);
	}
}

// Metadata only: drawing must never acquire a comp frame or enter the renderer.
static std::string Status(PF_InData* in, PF_OutData* out)
{
	state.textLayer = false;
	if (in->appl_id == kAppID_Premiere) return "Glass from needs an adjustment layer and hidden source track";
	AEFX_SuiteScoper<AEGP_PFInterfaceSuite1> pfi(in, kAEGPPFInterfaceSuite, kAEGPPFInterfaceSuiteVersion1, out);
	AEFX_SuiteScoper<AEGP_LayerSuite9> ls(in, kAEGPLayerSuite, kAEGPLayerSuiteVersion9, out);
	AEFX_SuiteScoper<AEGP_MemorySuite1> mem(in, kAEGPMemorySuite, kAEGPMemorySuiteVersion1, out);
	AEGP_LayerH layer = nullptr;
	AEGP_ObjectType type = AEGP_ObjectType_NONE;
	AEGP_LayerFlags flags = 0;
	if (pfi->AEGP_GetEffectLayer(in->effect_ref, &layer) || !layer || ls->AEGP_GetLayerObjectType(layer, &type) || ls->AEGP_GetLayerFlags(layer, &flags)) return "Layer status unavailable";
	state.textLayer = type == AEGP_ObjectType_TEXT;
	if (flags & AEGP_LayerFlag_ADJUSTMENT_LAYER) return "Adjustment layer - uses input backdrop and Glass shape";
	if (!OwnLayerType(type, flags) || !HasVideo(in, ls.get(), layer, type)) return "Use a shape, text, footage, solid or pre-comp layer";
	AEGP_CompH comp = nullptr;
	A_long at = 0, count = 0;
	if (ls->AEGP_GetLayerParentComp(layer, &comp) || ls->AEGP_GetLayerIndex(layer, &at) || ls->AEGP_GetCompNumLayers(comp, &count)) return "Layer status unavailable";
	for (A_long i = at + 1; i < count; i++) {
		AEGP_LayerH below = nullptr;
		if (!ls->AEGP_GetCompLayerByIndex(comp, i, &below) && below && IsBackdropLayer(ls.get(), mem.get(), below)) return studio::BackdropStatus(true, false);
	}
	if (!gNewGlass.empty()) {
		AEFX_SuiteScoper<AEGP_CompSuite13> cs(in, kAEGPCompSuite, kAEGPCompSuiteVersion13, out);
		AEFX_SuiteScoper<AEGP_ItemSuite9> is(in, kAEGPItemSuite, kAEGPItemSuiteVersion9, out);
		AEGP_ItemH item = nullptr;
		AEGP_LayerIDVal id = 0;
		A_long compID = 0;
		if (!ls->AEGP_GetLayerID(layer, &id) && !cs->AEGP_GetItemFromComp(comp, &item) && !is->AEGP_GetItemID(item, &compID))
			for (const NewGlass& g : gNewGlass) if (g.compItem == compID && g.layer == id) return studio::BackdropStatus(false, true);
	}
	return studio::BackdropStatus(false, false);
}
static Hit HitTest(float x, float y, float w)
{
	Hit hit = studio::HitTest(x, y, w, tabs, state.tab, state.bx, state.by);
	return state.textLayer && hit.type == Blob ? Hit{} : hit;
}
static PF_Err Refresh(PF_InData* in, PF_OutData* out, PF_EventExtra* e, bool changed)
{
	AEFX_SuiteScoper<PFAppSuite4> app(in, kPFAppSuite, kPFAppSuiteVersion4, out);
	PF_Rect rect = e->effect_win.current_frame;
	e->evt_out_flags |= PF_EO_HANDLED_EVENT | PF_EO_UPDATE_NOW;
	if (changed) out->out_flags |= PF_OutFlag_REFRESH_UI | PF_OutFlag_FORCE_RERENDER;
	else e->evt_out_flags |= PF_EO_NEVER_UPDATE;
	return app->PF_InvalidateRect(e->contextH, &rect);
}

static const char* TabIcon(const char* label)
{
	const char* labels[] = { "Looks", "Quick adjust", "Lens", "Light", "Colour", "Drop shadow", "Dim background", "Inner glow", "Stroke", "Film grain", "Advanced", "About" };
	const char* icons[] = { "presets", "advanced", "lens", "sun", "palette", "shadow", "moon", "glow", "stroke", "film", "advanced", "info" };
	for (int i = 0; i < 12; i++) if (!std::strcmp(label, labels[i])) return icons[i];
	return "advanced";
}
static PF_Err Draw(PF_InData* in, PF_OutData* out, PF_ParamDef* params[], PF_EventExtra* e, float w)
{
	Canvas c(in, out, e);
	DRAWBOT_SaveAndRestoreStateStack restore(c.s->surface_suiteP, c.surface);
	DRAWBOT_Rect32 clip = { (int)c.ox, (int)c.oy, (int)w, studio::kHeight };
	DRAWBOT_ErrorToException(c.s->surface_suiteP->Clip(c.surface, c.supplier, &clip));
	if (state.skin.Ensure((int)w)) return PF_Err_INTERNAL_STRUCT_DAMAGED;
	c.Image(state.skin.background, (int)w, studio::kHeight, 0, 0);
	c.Glass(studio::Header(w), true);
	c.Label("SUDO", 20, 29, 0xb7e6d1, 12, 44);
	c.Label("Glass Studio", 76, 30, Text, studio::kTitleSize, w - 92);

	float core[kCoreCount];
	Core(params, core);
	state.status = Status(in, out);
	const Box hero = studio::Hero(w);
	const Box renderSize = studio::HeroRenderSize(w);
	const int hw = (int)renderSize.w, hh = (int)renderSize.h;
	const int liquidRow = studio::RowByLabel("Liquid letters");
	const bool liquid = liquidRow >= 0 && Value(kRows[liquidRow], *params[liquidRow + 1]) != 0;
	if (state.textLayer && (state.letterWidth != hw || state.letterHeight != hh)) {
		if (!studio::LetterMasks(hw, hh, state.letters)) return PF_Err_INTERNAL_STRUCT_DAMAGED;
		state.letterWidth = hw; state.letterHeight = hh;
	}
	if (state.hero.empty() || state.savedWidth != (int)w || state.savedText != state.textLayer || state.savedLiquid != liquid ||
		std::memcmp(core, state.savedCore, sizeof core) || std::memcmp(state.bx, state.savedX, sizeof state.bx) || std::memcmp(state.by, state.savedY, sizeof state.by)) {
		std::vector<unsigned char> rendered;
		int err = studio::Preview(core, hw, hh, state.bx, state.by, rendered, state.textLayer ? &state.letters : nullptr, liquid);
		if (err) return PF_Err_INTERNAL_STRUCT_DAMAGED;
		if (hw == (int)hero.w && hh == (int)hero.h) state.hero = std::move(rendered);
		else studio::ScaleImage(rendered, hw, hh, (int)hero.w, (int)hero.h, state.hero);
		std::copy(core, core + kCoreCount, state.savedCore);
		std::copy(state.bx, state.bx + 2, state.savedX);
		std::copy(state.by, state.by + 2, state.savedY);
		state.savedWidth = (int)w; state.savedText = state.textLayer; state.savedLiquid = liquid;
	}
	c.Image(state.hero, (int)hero.w, (int)hero.h, hero.x, hero.y);
	c.Round(hero, 0, Border, true);
	if (!state.textLayer && state.hover.type == Blob) {
		int i = state.hover.index;
		float radius = studio::BlobRadius(hero);
		c.Round({ hero.x + state.bx[i] * hero.w - radius, hero.y + state.by[i] * hero.h - radius, radius * 2, radius * 2 }, radius,
			state.pressed.type == Blob ? Text : Green, true);
	}
	c.Label(state.textLayer ? "Aa / Material preview" : "Drag the glass / Material preview", 12, 198, Muted, 12, w - 24);
	if (state.tab == 0) for (int i = 0; i < kLookCount; i++) {
		Box b = studio::LookBox(i, w);
		bool active = studio::MatchesLook(i, core), hover = state.hover == Hit{ LookCard, i };
		c.Glass(b);
		if (active || hover) c.Round(b, 12, state.pressed == Hit{ LookCard, i } ? Text : active ? Green : Text, true);
		const Box thumb = studio::Thumbnail(b);
		const int tw = (int)thumb.w, th = (int)thumb.h;
		if (state.thumbs[i].empty() || state.thumbWidth[i] != tw || state.thumbHeight[i] != th) {
			float look[kCoreCount];
			std::copy(kDefaults, kDefaults + kCoreCount, look);
			ApplyLook(i, look);
			const float bx[] = { 0.4f, 0.6f }, by[] = { 0.5f, 0.5f };
			if (studio::Preview(look, tw, th, bx, by, state.thumbs[i], nullptr, false, true, i)) return PF_Err_INTERNAL_STRUCT_DAMAGED;
			state.thumbWidth[i] = tw; state.thumbHeight[i] = th;
		}
		c.Image(state.thumbs[i], tw, th, thumb.x, thumb.y);
		const char* icons[] = { "glass", "lens", "frost", "glow", "frost", "moon", "diamond", "blend", "film" };
		c.Round({ b.x + 8, b.y + 8, 24, 24 }, 8, Background);
		c.Icon(icons[i], b.x + 12, b.y + 12, Text);
		c.Label(kLooks[i].name, b.x + 8, b.y + 48, Text, 12, b.w - 16);
	}
	for (int i = 0; i < (int)tabs.size(); i++) {
		Box b = studio::TabBox(i, w);
		const bool active = state.tab == i, hover = state.hover == Hit{ TabButton, i };
		c.Glass(b, active);
		if (active || hover) c.Round(b, 16, state.pressed == Hit{ TabButton, i } ? Text : active ? Green : Text, true);
		c.Icon(TabIcon(tabs[i].label), b.x + (studio::TabLabels(w) ? 6 : (b.w - 16) / 2), b.y + 8, active ? 0xb1ffdc : Muted);
		if (studio::TabLabels(w)) c.Label(studio::TabLabel(tabs[i].label), b.x + 28, b.y + 21, active ? Text : Muted, 12, b.w - 32);
	}
	const auto& tab = tabs[state.tab];
	for (int n = 0; n < (int)tab.rows.size(); n++) {
		const int rowIndex = tab.rows[n];
		const Row& r = kRows[rowIndex];
		const PF_ParamDef& p = *params[rowIndex + 1];
		Box b = studio::RowBox(tab, n, w);
		bool hover = state.hover == Hit{ Control, n }, pressed = state.pressed == Hit{ Control, n };
		c.Glass(studio::RowSurface(b));
		if (hover || pressed) c.Round(studio::RowSurface(b), 12, pressed ? Text : Green, true);
		const Box label = studio::RowLabel(b, r);
		c.Label(r.label, label.x, label.y + 12, Text, 12, label.w);
		float v = Value(r, p);
		char value[48];
		std::snprintf(value, sizeof value, "%.*f%s", r.kind == K_ANGLE ? 0 : r.prec, v, r.kind == K_ANGLE ? " deg" : "");
		if (r.kind == K_FLOAT) {
			c.Label(value, b.x + b.w - 64, b.y + 16, 0xb1ffdc, 12, 64);
			Box track = studio::Track(b);
			c.Glass(track);
			float fill = studio::Fraction(r, v) * track.w;
			if (fill > 0) c.Round({ track.x, track.y + 1, std::max(1.f, fill), 4 }, 2, Green);
			c.Thumb(track.x + fill, track.y + 3);
		} else if (r.kind == K_ANGLE) {
			c.Label(value, b.x + b.w - 100, b.y + 16, 0xb1ffdc, 12, 64);
			c.Dial(studio::DialBox(b), v, hover);
		} else if (r.kind == K_POPUP) {
			const auto items = studio::Items(r.items);
			for (int i = 0; i < (int)items.size(); i++) {
				Box segment = studio::Segment(b, i, (int)items.size());
				bool selected = (int)v == i + 1;
				c.Glass(segment, selected);
				if (selected) c.Round(segment, 10, Green, true);
				c.Label(items[i].c_str(), segment.x + 8, segment.y + 17, selected ? Text : Muted, 12, segment.w - 16);
			}
		} else if (r.kind == K_COLOR) {
			unsigned rgb = (p.u.cd.value.red << 16) | (p.u.cd.value.green << 8) | p.u.cd.value.blue;
			c.Round({ b.x + b.w - 64, b.y + 6, 64, 24 }, 10, rgb);
			c.Round({ b.x + b.w - 64, b.y + 6, 64, 24 }, 10, hover ? Green : Text, true);
		} else if (r.kind == K_CHECK) {
			c.Glass(studio::ToggleBox(b), v != 0);
			c.Round(studio::ToggleBox(b), 14, v ? Green : Border, true);
			c.Thumb(b.x + b.w - (v ? 14 : 34), b.y + 18);
			c.Label(v ? "On" : "Off", b.x + b.w - 76, b.y + 20, v ? Green : Muted, 12, 24);
		}
	}
	if (state.tab == (int)tabs.size() - 1) {
		float y = studio::BodyTop(w);
		c.Label("SUDO", 24, y + 38, Green, 32, w - 48);
		c.Label("Liquid Glass for Adobe", 24, y + 78, Text, 18, w - 48);
		char version[64]; std::snprintf(version, sizeof version, "%d.%d.%d / by CGSudo", MAJOR, MINOR, BUG);
		c.Label(version, 24, y + 106, Muted, 12, w - 48);
		c.Label("Live glass. Your layers stay editable.", 24, y + 130, Text, 12, w - 48);
		c.Label("Made with the Liquid Glass engine.", 24, y + 150, Muted, 12, w - 48);
		for (int i = 0; i < 2; i++) {
			Box b = studio::LinkBox(i, w);
			c.Glass(b, true);
			if (state.hover == Hit{ AboutLink, i }) c.Round(b, 12, Green, true);
			c.Label(i ? "GitHub / Source & updates" : "LinkedIn / CGSudo", b.x + 12, b.y + 28, Text, 12, b.w - 40);
			c.Line({{0,8},{8,0},{2,0},{8,0},{8,6}}, b.x + b.w - 24, b.y + 18, Green);
		}
	}
	const Box status = studio::StatusBox(w);
	c.Rect({ status.x, status.y, status.w, 1 }, Border);
	const bool live = state.status == studio::BackdropStatus(true, false);
	c.Round({ status.x, status.y + 13, 6, 6 }, 3, live ? Green : 0xffc76a);
	c.Label(live ? "Live glass / Shape & text stay editable" : state.status.c_str(), status.x + 14, status.y + 20, Muted, 12, status.w - 14);
	c.Label(state.hover.type == TabButton ? tabs[state.hover.index].label : state.hover.type == Control ?
		"Shift: fine control / Double-click: reset" : tabs[state.tab].label, status.x, status.y + 40, Text, 12, status.w);
	e->evt_out_flags |= PF_EO_HANDLED_EVENT;
	return PF_Err_NONE;
}

static PF_Err Event(PF_InData* in, PF_OutData* out, PF_ParamDef* params[], PF_EventExtra* e)
{
	// Premiere uses this control only when all required PF and Drawbot suites are available.
	if ((in->appl_id == kAppID_Premiere && !PremiereStudioAvailable(in)) || !e || !e->contextH || (*e->contextH)->w_type != PF_Window_EFFECT ||
		e->effect_win.index != IndexOf(P_LOOK) || e->effect_win.area != PF_EA_CONTROL) return PF_Err_NONE;
	const float w = studio::PanelWidth((float)e->effect_win.current_frame.right - e->effect_win.current_frame.left);
	if (e->e_type == PF_Event_DRAW) return Draw(in, out, params, e, w);
	if (e->e_type == PF_Event_IDLE) {
		if (Status(in, out) != state.status) return Refresh(in, out, e, false);
		return PF_Err_NONE;
	}
	if (e->e_type == PF_Event_ADJUST_CURSOR) {
		const auto& point = e->u.adjust_cursor.screen_point;
		Hit h = HitTest(point.h - (float)e->effect_win.current_frame.left, point.v - (float)e->effect_win.current_frame.top, w);
		e->u.adjust_cursor.set_cursor = h.type == Blob ? PF_Cursor_PAN : h.type == None ? PF_Cursor_ARROW : PF_Cursor_FINGER_POINTER;
		if (h.type == Control) {
			Kind kind = kRows[tabs[state.tab].rows[h.index]].kind;
			if (kind == K_FLOAT) e->u.adjust_cursor.set_cursor = PF_Cursor_RESIZE_HORIZONTAL;
			if (kind == K_ANGLE) e->u.adjust_cursor.set_cursor = PF_Cursor_CROSS_ROTATE;
		}
		state.pressed = {};
		if (!(h == state.hover)) { state.hover = h; return Refresh(in, out, e, false); }
		e->evt_out_flags |= PF_EO_HANDLED_EVENT;
		return PF_Err_NONE;
	}
	if (e->e_type != PF_Event_DO_CLICK && e->e_type != PF_Event_DRAG) return PF_Err_NONE;
	auto& click = e->u.do_click;
	float x = click.screen_point.h - (float)e->effect_win.current_frame.left;
	float y = click.screen_point.v - (float)e->effect_win.current_frame.top;
	// AE can deliver a drag as a continued DO_CLICK, as in the SDK's CCU sample.
	const bool first = e->e_type == PF_Event_DO_CLICK && !click.send_drag;
	Hit hit = first ? HitTest(x, y, w) : Hit{ (Target)click.continue_refcon[0], (int)click.continue_refcon[1] };
	state.hover = hit;
	state.pressed = click.last_time ? Hit{} : hit;
	if (first) {
		click.continue_refcon[0] = hit.type;
		click.continue_refcon[1] = hit.index;
		click.continue_refcon[2] = (A_intptr_t)(x * 65536);
		click.continue_refcon[3] = (A_intptr_t)(y * 65536);
	}
	if (hit.type == None) return PF_Err_NONE;
	if (hit.type == AboutLink) {
		if (first) {
			static const wchar_t* const urls[] = { L"https://www.linkedin.com/in/cgsudo", L"https://github.com/CGSudo/liquid-glass-for-adobe" };
			if (hit.index >= 0 && hit.index < 2) ShellExecuteW(nullptr, L"open", urls[hit.index], nullptr, nullptr, SW_SHOWNORMAL);
		}
		return Refresh(in, out, e, false);
	}
	if (hit.type == TabButton) { state.tab = hit.index; state.hover = {}; return Refresh(in, out, e, false); }
	if (hit.type == LookCard) {
		float core[kCoreCount];
		Core(params, core);
		if (studio::MatchesLook(hit.index, core)) return Refresh(in, out, e, false);
		PF_Err err = ApplyLookToParams(params, hit.index);
		return err ? err : Refresh(in, out, e, true);
	}
	float lastX = click.continue_refcon[2] / 65536.f, lastY = click.continue_refcon[3] / 65536.f;
	click.continue_refcon[2] = (A_intptr_t)(x * 65536);
	click.continue_refcon[3] = (A_intptr_t)(y * 65536);
	if (hit.type == Blob) {
		const Box hero = studio::Hero(w);
		int i = hit.index;
		float radius = studio::BlobRadius(hero) + 1;
		state.bx[i] = studio::Clamp(state.bx[i] + (x - lastX) / hero.w, radius / hero.w, 1 - radius / hero.w);
		state.by[i] = studio::Clamp(state.by[i] + (y - lastY) / hero.h, radius / hero.h, 1 - radius / hero.h);
		click.send_drag = !click.last_time;
		return Refresh(in, out, e, false);
	}
	const auto& tab = tabs[state.tab];
	const int rowIndex = tab.rows[hit.index];
	const Row& r = kRows[rowIndex];
	PF_ParamDef& p = *params[rowIndex + 1];
	Box b = studio::RowBox(tab, hit.index, w);
	bool changed = false, fine = (click.modifiers & PF_Mod_SHIFT_KEY) != 0;
	if (r.kind == K_FLOAT) {
		Box track = studio::Track(b);
		float value = studio::SliderValue(r, Value(r, p), track, x, y, x - lastX, first, fine, first && click.num_clicks == 2);
		changed = SetValue(r, p, value);
		click.send_drag = !click.last_time && click.num_clicks != 2;
	} else if (r.kind == K_ANGLE) {
		const Box dial = studio::DialBox(b);
		float cx = dial.x + dial.w / 2, cy = dial.y + dial.h / 2;
		float angle = studio::AngleAt(x, y, cx, cy);
		float delta = studio::AngleDelta(angle, studio::AngleAt(lastX, lastY, cx, cy));
		changed = first && click.num_clicks == 2 ? SetValue(r, p, r.dflt) :
			!first && delta != 0 && SetValue(r, p, Value(r, p) + delta * (fine ? 0.1f : 1.f));
		click.send_drag = !click.last_time;
	} else if (first && r.kind == K_CHECK) {
		changed = SetValue(r, p, !p.u.bd.value);
	} else if (first && r.kind == K_POPUP) {
		int count = (int)studio::Items(r.items).size();
		for (int i = 0; i < count; i++) if (studio::Segment(b, i, count).contains(x, y)) changed = SetValue(r, p, float(i + 1));
	} else if (first && r.kind == K_COLOR) {
		AEFX_SuiteScoper<PFAppSuite4> app(in, kPFAppSuite, kPFAppSuiteVersion4, out);
		PF_PixelFloat before = { 1, p.u.cd.value.red / 255.f, p.u.cd.value.green / 255.f, p.u.cd.value.blue / 255.f }, after = before;
		PF_Err err = app->PF_AppColorPickerDialog(r.label, &before, TRUE, &after);
		if (err == PF_Interrupt_CANCEL) return Refresh(in, out, e, false);
		if (err) return err;
		PF_Pixel colour = p.u.cd.value;
		colour.red = (A_u_char)(studio::Clamp(after.red, 0, 1) * 255 + 0.5f);
		colour.green = (A_u_char)(studio::Clamp(after.green, 0, 1) * 255 + 0.5f);
		colour.blue = (A_u_char)(studio::Clamp(after.blue, 0, 1) * 255 + 0.5f);
		changed = colour.red != p.u.cd.value.red || colour.green != p.u.cd.value.green || colour.blue != p.u.cd.value.blue;
		if (changed) {
			p.u.cd.value = colour;
			p.uu.change_flags |= PF_ChangeFlag_CHANGED_VALUE;
		}
	}
	return Refresh(in, out, e, changed);
}
}
