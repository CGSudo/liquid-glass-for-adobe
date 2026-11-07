// Included after the host's look action; only PF_Cmd_EVENT accesses this UI state.
#pragma once
#include "GlassStudioModel.h"

namespace studio_ui {
using studio::Box;
using studio::Hit;
using studio::Target;
using studio::None;
using studio::Blob;
using studio::LookCard;
using studio::TabButton;
using studio::Control;
struct State {
	int tab = 0;
	Hit hover;
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
const unsigned Background = 0x1b1d1c, Card = 0x242726, Hover = 0x2e3230, Green = 0x2fae7c,
	Text = 0xe9efec, Muted = 0x9aa6a0, Border = 0x3c4240;

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
	void Label(const char* text, float x, float y, unsigned hex = Text, float size = 11, float width = 380)
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
	void Dial(Box b, float angle, bool hover)
	{
		float radius = b.w / 2, cx = b.x + radius, cy = b.y + radius;
		Round(b, radius, hover ? Hover : Background);
		Round(b, radius, hover ? Green : Border, true);
		const float a = angle * 3.14159265f / 180;
		float x = cx + std::sin(a) * (radius - 5), y = cy - std::cos(a) * (radius - 5);
		Round({ x - 3, y - 3, 6, 6 }, 3, Green);
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
	AEFX_SuiteScoper<AEGP_PFInterfaceSuite1> pfi(in, kAEGPPFInterfaceSuite, kAEGPPFInterfaceSuiteVersion1, out);
	AEFX_SuiteScoper<AEGP_LayerSuite9> ls(in, kAEGPLayerSuite, kAEGPLayerSuiteVersion9, out);
	AEFX_SuiteScoper<AEGP_MemorySuite1> mem(in, kAEGPMemorySuite, kAEGPMemorySuiteVersion1, out);
	AEGP_LayerH layer = nullptr;
	AEGP_ObjectType type = AEGP_ObjectType_NONE;
	AEGP_LayerFlags flags = 0;
	if (pfi->AEGP_GetEffectLayer(in->effect_ref, &layer) || !layer || ls->AEGP_GetLayerObjectType(layer, &type) || ls->AEGP_GetLayerFlags(layer, &flags)) return "Layer status unavailable";
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
	return studio::HitTest(x, y, w, tabs, state.tab, state.bx, state.by);
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

static PF_Err Draw(PF_InData* in, PF_OutData* out, PF_ParamDef* params[], PF_EventExtra* e, float w)
{
	Canvas c(in, out, e);
	DRAWBOT_SaveAndRestoreStateStack restore(c.s->surface_suiteP, c.surface);
	DRAWBOT_Rect32 clip = { (int)c.ox, (int)c.oy, (int)w, studio::kHeight };
	DRAWBOT_ErrorToException(c.s->surface_suiteP->Clip(c.surface, c.supplier, &clip));
	c.Rect({ 0, 0, w, studio::kHeight }, Background);
	const Box header = studio::Header(w);
	c.Round(header, 8, 0x0d4431);
	c.Label("SUDO", 14, header.h / 2 + 4, 0xb7e6d1, 11, 42);
	c.Label("Glass Studio", 68, header.h / 2 + 5, Text, 16, w - 124);
	c.Label("LIVE", w - 47, header.h / 2 + 4, 0x45c891, 10, 40);

	float core[kCoreCount];
	Core(params, core);
	const Box hero = studio::Hero(w);
	const int hw = (int)hero.w, hh = (int)hero.h;
	if (state.hero.empty() || state.savedWidth != hw || std::memcmp(core, state.savedCore, sizeof core) ||
		std::memcmp(state.bx, state.savedX, sizeof state.bx) || std::memcmp(state.by, state.savedY, sizeof state.by)) {
		int err = studio::Preview(core, hw, hh, state.bx, state.by, state.hero);
		if (err) return PF_Err_INTERNAL_STRUCT_DAMAGED;
		std::copy(core, core + kCoreCount, state.savedCore);
		std::copy(state.bx, state.bx + 2, state.savedX);
		std::copy(state.by, state.by + 2, state.savedY);
		state.savedWidth = hw;
	}
	c.Image(state.hero, hw, hh, hero.x, hero.y);
	if (state.hover.type == Blob) {
		int i = state.hover.index;
		float radius = studio::BlobRadius(hero);
		c.Round({ hero.x + state.bx[i] * hero.w - radius, hero.y + state.by[i] * hero.h - radius, radius * 2, radius * 2 }, radius, Green, true);
	}
	if (state.tab == 0) for (int i = 0; i < kLookCount; i++) {
		Box b = studio::LookBox(i, w);
		bool active = studio::MatchesLook(i, core), hover = state.hover == Hit{ LookCard, i };
		c.Round(b, 7, hover ? Hover : Card);
		c.Round(b, 7, active || hover ? Green : Border, true);
		const Box thumb = studio::Thumbnail(b);
		const int tw = (int)thumb.w, th = (int)thumb.h;
		if (state.thumbs[i].empty() || state.thumbWidth[i] != tw || state.thumbHeight[i] != th) {
			float look[kCoreCount];
			std::copy(kDefaults, kDefaults + kCoreCount, look);
			ApplyLook(i, look);
			const float bx[] = { 0.4f, 0.6f }, by[] = { 0.5f, 0.5f };
			if (studio::Preview(look, tw, th, bx, by, state.thumbs[i])) return PF_Err_INTERNAL_STRUCT_DAMAGED;
			state.thumbWidth[i] = tw;
			state.thumbHeight[i] = th;
		}
		c.Image(state.thumbs[i], tw, th, thumb.x, thumb.y);
		c.Label(kLooks[i].name, b.x + 7, b.y + b.h - 10, active ? Text : Muted, 10, b.w - 14);
		if (active) c.Round({ b.x + b.w - 10, b.y + b.h - 18, 4, 4 }, 2, Green);
	}
	for (int i = 0; i < (int)tabs.size(); i++) {
		Box b = studio::TabBox(i, w);
		c.Round(b, 5, state.tab == i ? 0x0d4431 : state.hover == Hit{ TabButton, i } ? Hover : Card);
		if (state.tab == i) c.Round(b, 5, Green, true);
		c.Label(tabs[i].label, b.x + 4, b.y + 14, state.tab == i ? Text : Muted, 9, b.w - 8);
	}
	const auto& tab = tabs[state.tab];
	for (int n = 0; n < (int)tab.rows.size(); n++) {
		const int rowIndex = tab.rows[n];
		const Row& r = kRows[rowIndex];
		const PF_ParamDef& p = *params[rowIndex + 1];
		Box b = studio::RowBox(tab, n, w);
		bool hover = state.hover == Hit{ Control, n };
		c.Round({ b.x - 8, b.y - 3, b.w + 16, b.h + 2 }, 6, hover ? Hover : Card);
		c.Label(r.label, b.x, b.y + 13, hover ? Text : Muted, 11, b.w - 100);
		float v = Value(r, p);
		char value[48];
		std::snprintf(value, sizeof value, "%.*f%s", r.kind == K_ANGLE ? 0 : r.prec, v, r.kind == K_ANGLE ? " deg" : "");
		if (r.kind == K_FLOAT) {
			c.Label(value, b.x + b.w - 64, b.y + 13, Text, 11, 64);
			Box track = studio::Track(b);
			c.Round(track, 3.5f, Background);
			float fill = studio::Fraction(r, v) * track.w;
			if (fill > 0) c.Round({ track.x, track.y, std::max(1.f, fill), track.h }, 3.5f, Green);
			c.Round({ track.x + fill - 4, track.y - 1, 8, 9 }, 4, hover ? 0xffffff : Text);
		} else if (r.kind == K_ANGLE) {
			c.Label(value, b.x + b.w - 98, b.y + 13, Text, 11, 64);
			c.Dial(studio::DialBox(b), v, hover);
		} else if (r.kind == K_POPUP) {
			const auto items = studio::Items(r.items);
			for (int i = 0; i < (int)items.size(); i++) {
				Box segment = studio::Segment(b, i, (int)items.size());
				bool selected = (int)v == i + 1;
				c.Round(segment, 4, selected ? 0x0d4431 : Background);
				if (selected || hover) c.Round(segment, 4, selected ? Green : Border, true);
				c.Label(items[i].c_str(), segment.x + 5, segment.y + 13, selected ? Text : Muted, 10, segment.w - 10);
			}
		} else if (r.kind == K_COLOR) {
			unsigned rgb = (p.u.cd.value.red << 16) | (p.u.cd.value.green << 8) | p.u.cd.value.blue;
			c.Round({ b.x + b.w - 74, b.y + 3, 74, 29 }, 6, rgb);
			c.Round({ b.x + b.w - 74, b.y + 3, 74, 29 }, 6, hover ? Green : Border, true);
		} else if (r.kind == K_CHECK) {
			c.Round({ b.x + b.w - 42, b.y + 6, 42, 23 }, 11, v ? Green : Background);
			c.Round({ b.x + b.w - (v ? 20 : 39), b.y + 9, 17, 17 }, 8.5f, Text);
			c.Label(v ? "On" : "Off", b.x + b.w - 74, b.y + 17, Text, 10, 30);
		}
	}
	const Box status = studio::StatusBox(w);
	c.Rect({ status.x, status.y, status.w, 1 }, Border);
	state.status = Status(in, out);
	c.Label(state.status.c_str(), status.x, status.y + 17, Muted, 10, status.w);
	e->evt_out_flags |= PF_EO_HANDLED_EVENT;
	return PF_Err_NONE;
}

static PF_Err Event(PF_InData* in, PF_OutData* out, PF_ParamDef* params[], PF_EventExtra* e)
{
	// Premiere has no registered custom control and must never acquire AE-only UI suites.
	if (in->appl_id == kAppID_Premiere || !e || !e->contextH || (*e->contextH)->w_type != PF_Window_EFFECT ||
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
	if (first) {
		click.continue_refcon[0] = hit.type;
		click.continue_refcon[1] = hit.index;
		click.continue_refcon[2] = (A_intptr_t)(x * 65536);
		click.continue_refcon[3] = (A_intptr_t)(y * 65536);
	}
	if (hit.type == None) return PF_Err_NONE;
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
		changed = !first && delta != 0 && SetValue(r, p, Value(r, p) + delta * (fine ? 0.1f : 1.f));
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
