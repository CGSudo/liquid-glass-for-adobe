// SDK-free Studio layout and interaction rules keep drawing and hit-testing in agreement.
#pragma once
#include "GlassHost.h"
#include "GlassStudioHelp.h"
#include <string>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#pragma comment(lib, "gdi32.lib")
#endif

namespace studio {
const int kWidth = 420, kHeight = 640;
const float kLabelSize = 12, kValueSize = 12, kTitleSize = 20;
struct Box {
	float x, y, w, h;
	bool contains(float px, float py) const { return px >= x && py >= y && px < x + w && py < y + h; }
};
struct Tab { const char* label; std::vector<int> rows; };
enum Target { None, Blob, LookCard, TabButton, Control, AboutLink };
struct Hit {
	Target type = None;
	int index = -1;
	bool operator==(const Hit& b) const { return type == b.type && index == b.index; }
};
inline std::vector<Tab> Tabs()
{
	std::vector<Tab> tabs = { { "Looks", {} } };
	bool include = false;
	for (int i = 0; i < kRowCount; i++) {
		const Row& r = kRows[i];
		if (r.kind == K_TOPIC || r.kind == K_TOPIC_CLOSED) {
			include = r.id != P_T_SHAPE && r.id != P_T_SHAPE2 && r.id != P_T_ANIM;
			if (include) tabs.push_back({ r.label, {} });
		} else if (r.kind == K_END) include = false;
		else if (include) tabs.back().rows.push_back(i);
	}
	// Both views reference the original rows, so tint keeps native animation and undo.
	for (auto& tab : tabs) if (!std::strcmp(tab.label, "Quick adjust"))
		for (int i = 0; i < kRowCount; i++) if (kRows[i].id == P_TINT || kRows[i].id == P_TINT_AMT)
			if (std::find(tab.rows.begin(), tab.rows.end(), i) == tab.rows.end()) tab.rows.push_back(i);
	tabs.push_back({ "About", {} });
	return tabs;
}
inline int RowByLabel(const char* label)
{
	for (int i = 0; i < kRowCount; i++) if (kRows[i].label && !std::strcmp(kRows[i].label, label)) return i;
	return -1;
}
inline float Clamp(float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); }
inline float PanelWidth(float width) { return Clamp(width, 280, 800); }
inline float Fraction(const Row& r, float v) { return r.max > r.min ? Clamp((v - r.min) / (r.max - r.min), 0, 1) : 0; }
inline float ValueAt(const Row& r, float x, const Box& b)
{
	return r.min + Clamp(b.w > 0 ? (x - b.x) / b.w : 0, 0, 1) * (r.max - r.min);
}
inline float DragValue(const Row& r, float value, float dx, float width, bool fine)
{
	return Clamp(value + dx / std::max(1.f, width) * (r.max - r.min) * (fine ? 0.1f : 1.f), r.min, r.max);
}
inline float AngleAt(float x, float y, float cx, float cy) { return std::atan2(x - cx, cy - y) * 180.f / 3.14159265f; }
inline float AngleDelta(float a, float b) { return std::remainder(a - b, 360.f); }
inline std::vector<std::string> Items(const char* text)
{
	std::vector<std::string> result;
	if (text) {
		const char* start = text;
		for (const char* p = text; ; p++) if (*p == '|' || !*p) {
			result.emplace_back(start, p);
			if (!*p) break;
			start = p + 1;
		}
	}
	return result;
}
inline Box Header(float w) { return { 8, 4, w - 16, 40 }; }
inline Box Hero(float w) { return { 12, 52, w - 24, 90 }; }
inline Box HelpBox(float w) { return { 12, 148, w - 24, 34 }; }
inline Box HeroRenderSize(float w)
{
	const auto hero = Hero(w);
	const float width = std::min(420.f, hero.w);
	return { 0, 0, width, std::round(hero.h * width / hero.w) };
}
inline float TabsTop() { return 206; }
inline int TabColumns(float) { return ((int)Tabs().size() + 1) / 2; }
inline Box TabBox(int i, float w)
{
	const int cols = TabColumns(w);
	const float cell = std::floor((w - 24 + 4) / cols);
	return { 12 + (i % cols) * cell, TabsTop() + (i / cols) * 36, cell - 4, 32 };
}
inline bool TabLabels(float w) { return TabBox(0, w).w >= 92; }
inline const char* TabLabel(const char* label)
{
	if (!std::strcmp(label, "Quick adjust")) return "Quick";
	if (!std::strcmp(label, "Drop shadow")) return "Shadow";
	if (!std::strcmp(label, "Dim background")) return "Dim";
	if (!std::strcmp(label, "Inner glow")) return "Glow";
	if (!std::strcmp(label, "Film grain")) return "Grain";
	if (!std::strcmp(label, "Brightness shape")) return "Luma";
	return label;
}
inline float BodyTop(float w = kWidth) { return TabsTop() + ((Tabs().size() + TabColumns(w) - 1) / TabColumns(w)) * 36 + 8; }
inline Box StatusBox(float w) { return { 12, kHeight - 48.f, w - 24, 48 }; }
inline int LookColumns(float w) { return w < 400 ? 2 : 3; }
inline Box LookBox(int i, float w)
{
	const int cols = LookColumns(w);
	const float cell = std::floor((w - 24 + 8) / cols);
	return { 12 + (i % cols) * cell, BodyTop(w) + (i / cols) * 58, cell - 8, 54 };
}
inline Box Thumbnail(const Box& card) { return { card.x + 4, card.y + 4, card.w - 8, 30 }; }
inline float RowHeight(const Row& r) { return r.kind == K_POPUP ? 24.f + std::ceil(Items(r.items).size() / 2.f) * 28 : 40.f; }
inline Box RowBox(const Tab& tab, int n, float w)
{
	float y = BodyTop(w);
	for (int i = 0; i < n; i++) y += RowHeight(kRows[tab.rows[i]]);
	return { 24, y, w - 48, RowHeight(kRows[tab.rows[n]]) - 4 };
}
inline Box RowSurface(const Box& row) { return { row.x - 12, row.y, row.w + 24, row.h + 4 }; }
inline Box RowLabel(const Box& row, const Row& r) { return { row.x, row.y + 4, row.w - (r.kind == K_POPUP ? 0 : r.kind == K_ANGLE ? 108 : 76), 16 }; }
inline Box Track(const Box& row) { return { row.x + 8, row.y + 26, row.w - 16, 6 }; }
inline bool NearTrack(const Box& track, float x, float y)
{
	return Box{ track.x - 6, track.y - 6, track.w + 12, track.h + 12 }.contains(x, y);
}
inline float SliderValue(const Row& r, float current, const Box& track, float x, float y, float dx, bool first, bool fine, bool reset)
{
	if (reset) return r.dflt;
	if (first && !fine && NearTrack(track, x, y)) return ValueAt(r, x, track);
	return dx == 0 ? current : DragValue(r, current, dx, track.w, fine);
}
inline Box DialBox(const Box& row) { return { row.x + row.w - 32, row.y + 2, 32, 32 }; }
inline Box ToggleBox(const Box& row) { return { row.x + row.w - 48, row.y + 4, 48, 28 }; }
inline Box Segment(const Box& row, int i, int count)
{
	const int cols = std::min(2, count);
	return { row.x + (i % cols) * std::floor((row.w + 8) / cols), row.y + 20 + (i / cols) * 28.f,
		std::floor((row.w + 8) / cols) - 8, 24 };
}
inline Box LinkBox(int i, float w) { return { 24, BodyTop(w) + 164 + i * 48.f, w - 48, 40 }; }
inline float BlobRadius(const Box& hero) { return hero.h * 0.27f; }
inline int BlobHit(float x, float y, const float* bx, const float* by, const Box& hero)
{
	int best = -1;
	float distance = BlobRadius(hero) * BlobRadius(hero);
	for (int i = 0; i < 2; i++) {
		float dx = x - hero.x - bx[i] * hero.w, dy = y - hero.y - by[i] * hero.h;
		float d = dx * dx + dy * dy;
		if (hero.contains(x, y) && d <= distance) { best = i; distance = d; }
	}
	return best;
}
inline Hit HitTest(float x, float y, float w, const std::vector<Tab>& tabs, int active, const float* bx, const float* by)
{
	int blob = BlobHit(x, y, bx, by, Hero(w));
	if (blob >= 0) return { Blob, blob };
	if (active == 0) for (int i = 0; i < kLookCount; i++) if (LookBox(i, w).contains(x, y)) return { LookCard, i };
	for (int i = 0; i < (int)tabs.size(); i++) if (TabBox(i, w).contains(x, y)) return { TabButton, i };
	if (active == (int)tabs.size() - 1) for (int i = 0; i < 2; i++) if (LinkBox(i, w).contains(x, y)) return { AboutLink, i };
	if (active >= 0 && active < (int)tabs.size())
		for (int i = 0; i < (int)tabs[active].rows.size(); i++) if (RowSurface(RowBox(tabs[active], i, w)).contains(x, y)) return { Control, i };
	return {};
}
inline const char* BackdropStatus(bool helper, bool queued)
{
	return helper ? "Live glass - edit your layer to change its outline" : queued ? "Setting up..." : "Backdrop layer missing - re-apply the effect";
}
inline std::string HelpText(const std::vector<Tab>& tabs, int active, Hit hover, bool textLayer, bool adjustment, bool premiere)
{
	if (hover.type == Control && hover.index >= 0 && hover.index < (int)tabs[active].rows.size()) {
		const Row& row = kRows[tabs[active].rows[hover.index]];
		return std::string(row.label) + ": " + RowHelp(row.id, premiere);
	}
	if (hover.type == TabButton && hover.index >= 0 && hover.index < (int)tabs.size())
		return std::string(tabs[hover.index].label) + ": " + TabHelp(tabs[hover.index].label);
	if (hover.type == LookCard && hover.index >= 0 && hover.index < kLookCount) {
		const char* help[] = {
			"Rounded, shiny glass for a liquid look.", "Clear glass for a sharp view of the background.",
			"Soft blur for hiding busy details behind the glass.", "A soft panel with a thin edge for readable overlays.",
			"Cool tint and grain for an icy surface.", "Dark glass for a subdued background.",
			"Strong bending and rainbow edges for a cut-glass look.", "Soft rounded edges for a floating bubble.",
			"Frost and visible grain for a film texture."
		};
		return std::string(kLooks[hover.index].name) + ": " + help[hover.index];
	}
	if (hover.type == Blob) return "Material preview: drag the drops together to try Merge without moving your layer.";
	if (hover.type == AboutLink) return hover.index ? "GitHub: open the source code and check for updates." : "LinkedIn: visit CGSudo's profile.";
	const char* tip = TabHelp(tabs[active].label);
	if (textLayer && !std::strcmp(tabs[active].label, "Lens")) tip = RowHelp(P_LIQUID_LETTERS);
	if (premiere && active == 0) tip = GlassFromHelp();
	return std::string(tabs[active].label) + (textLayer ? " / Text: " : adjustment ? " / Adjustment: " : " / Layer outline: ") + tip;
}
struct HelpLines { std::string first, second; int size = 12; };
template<class Measure> inline HelpLines WrapHelp(const std::string& text, float width, Measure measure)
{
	HelpLines result;
	for (int size = 12; size >= 8; size--) {
		result = { text, "", size };
		if (measure(text.c_str(), size) <= width) return result;
		size_t split = text.find_last_of(' ');
		while (split != std::string::npos && measure(text.substr(0, split).c_str(), size) > width)
			split = split ? text.find_last_of(' ', split - 1) : std::string::npos;
		if (split == std::string::npos) continue;
		result.first = text.substr(0, split); result.second = text.substr(split + 1);
		if (measure(result.second.c_str(), size) <= width) return result;
	}
	// The strip promises two lines; retain the complete lead sentence instead of cutting a word.
	const size_t sentence = text.find(". ");
	if (sentence != std::string::npos) return WrapHelp(text.substr(0, sentence + 1), width, measure);
	return result;
}
inline bool MatchesLook(int n, const float* core)
{
	float expected[kCoreCount];
	std::copy(core, core + kCoreCount, expected);
	ApplyLook(n, expected);
	for (int i = 0; i < kCoreCount; i++) if (std::fabs(expected[i] - core[i]) > 0.5f / 255.f) return false;
	return true;
}

inline void Scene(int w, int h, std::vector<unsigned char>& rgba, bool panel = false)
{
	rgba.resize(size_t(w) * h * 4);
	for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
		const size_t at = size_t(y) * w + x;
		float u = float(x) / w, v = float(y) / h;
		const float mint = std::exp(-7 * ((u - 0.14f) * (u - 0.14f) + (v - 0.3f) * (v - 0.3f)));
		const float violet = std::exp(-9 * ((u - 0.85f) * (u - 0.85f) + (v - 0.65f) * (v - 0.65f)));
		const float gold = std::exp(-18 * ((u - 0.67f) * (u - 0.67f) + (v - 0.08f) * (v - 0.08f)));
		// A single soft ribbon gives refraction an edge to bend without a checkerboard.
		const float ribbonAt = v - 0.7f + 0.3f * u;
		const float ribbon = panel ? 0 : std::exp(-180 * ribbonAt * ribbonAt);
		const float gain = panel ? 0.23f : 1.f;
		const float rgb[] = { 14 + gain * (30 * mint + 118 * violet + 166 * gold + 48 * ribbon),
			20 + gain * (140 * mint + 35 * violet + 106 * gold + 55 * ribbon),
			24 + gain * (136 * mint + 160 * violet + 38 * gold + 50 * ribbon) };
		for (int c = 0; c < 3; c++) rgba[at * 4 + c] = (unsigned char)Clamp(rgb[c], 0, 255);
		rgba[at * 4 + 3] = 255;
	}
}
inline float RoundMask(float x, float y, float w, float h, float radius)
{
	const float dx = std::fabs(x - w / 2) - (w / 2 - radius), dy = std::fabs(y - h / 2) - (h / 2 - radius);
	return Clamp(radius - std::sqrt(std::max(0.f, dx) * std::max(0.f, dx) + std::max(0.f, dy) * std::max(0.f, dy)) - std::min(0.f, std::max(dx, dy)) + 0.5f, 0, 1);
}
inline bool LetterMasks(int w, int h, std::vector<unsigned char>& masks)
{
#ifdef _WIN32
	HDC dc = CreateCompatibleDC(nullptr);
	if (!dc) return false;
	BITMAPINFO info = {};
	info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	info.bmiHeader.biWidth = w; info.bmiHeader.biHeight = -h;
	info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32; info.bmiHeader.biCompression = BI_RGB;
	void* bits = nullptr;
	HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
	HFONT font = CreateFontW(-int(h * 0.86f), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
	if (!bitmap || !font) { if (bitmap) DeleteObject(bitmap); if (font) DeleteObject(font); DeleteDC(dc); return false; }
	HGDIOBJ oldBitmap = SelectObject(dc, bitmap), oldFont = SelectObject(dc, font);
	SetTextColor(dc, RGB(255, 255, 255)); SetBkColor(dc, RGB(0, 0, 0));
	SIZE sizes[2] = {};
	GetTextExtentPoint32W(dc, L"A", 1, &sizes[0]); GetTextExtentPoint32W(dc, L"a", 1, &sizes[1]);
	const int gap = std::max(4, h / 20), left = (w - sizes[0].cx - sizes[1].cx - gap) / 2;
	masks.assign(size_t(w) * h * 2, 0);
	bool ok = true;
	for (int i = 0; i < 2; i++) {
		std::memset(bits, 0, size_t(w) * h * 4);
		ok = TextOutW(dc, left + (i ? sizes[0].cx + gap : 0), (h - sizes[i].cy) / 2, i ? L"a" : L"A", 1) && ok;
		GdiFlush();
		for (size_t at = 0; at < size_t(w) * h; at++) masks[size_t(i) * w * h + at] = ((unsigned char*)bits)[at * 4];
	}
	SelectObject(dc, oldFont); SelectObject(dc, oldBitmap);
	DeleteObject(font); DeleteObject(bitmap); DeleteDC(dc);
	return ok;
#else
	return false;
#endif
}
inline void ScaleImage(const std::vector<unsigned char>& src, int sw, int sh, int dw, int dh, std::vector<unsigned char>& dst)
{
	dst.resize(size_t(dw) * dh * 4);
	for (int y = 0; y < dh; y++) for (int x = 0; x < dw; x++) {
		const float sx = Clamp((x + 0.5f) * sw / dw - 0.5f, 0, float(sw - 1));
		const float sy = Clamp((y + 0.5f) * sh / dh - 0.5f, 0, float(sh - 1));
		const int x0 = (int)sx, y0 = (int)sy, x1 = std::min(x0 + 1, sw - 1), y1 = std::min(y0 + 1, sh - 1);
		const float fx = sx - x0, fy = sy - y0;
		for (int c = 0; c < 4; c++) {
			const float top = src[(size_t(y0) * sw + x0) * 4 + c] * (1 - fx) + src[(size_t(y0) * sw + x1) * 4 + c] * fx;
			const float bottom = src[(size_t(y1) * sw + x0) * 4 + c] * (1 - fx) + src[(size_t(y1) * sw + x1) * 4 + c] * fx;
			dst[(size_t(y) * dw + x) * 4 + c] = (unsigned char)(top * (1 - fy) + bottom * fy + 0.5f);
		}
	}
}
// An opaque test scene makes engine RGBA safe to upload as a Drawbot BGRA image.
inline int Preview(const float* core, int w, int h, const float* bx, const float* by, std::vector<unsigned char>& bgra,
	const std::vector<unsigned char>* letters = nullptr, bool liquidLetters = false, bool thumbnail = false, int lookIndex = -1)
{
	std::vector<unsigned char> scene, masks(size_t(w) * h * 2), out(size_t(w) * h * 4);
	Scene(w, h, scene);
	for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
		const size_t at = size_t(y) * w + x;
		for (int i = 0; i < 2; i++) {
			float dx = x + 0.5f - bx[i] * w, dy = y + 0.5f - by[i] * h;
			masks[size_t(i) * w * h + at] = (unsigned char)(Clamp(h * 0.27f - std::sqrt(dx * dx + dy * dy) + 0.5f, 0, 1) * 255);
		}
		if (thumbnail) {
			// One fine ribbon makes clear and frosted glass visibly different on the same calm scene.
			const float ribbon = Clamp(1.5f - std::fabs(y - h * 0.56f + (x - w * 0.5f) * 0.1f), 0, 1);
			for (int c = 0; c < 3; c++) scene[at * 4 + c] = (unsigned char)(scene[at * 4 + c] * (1 - ribbon * 0.65f) + 224 * ribbon * 0.65f);
			float mask = RoundMask(x + 0.5f - w * 0.14f, y + 0.5f - h * 0.16f, w * 0.72f, h * 0.68f, h * (lookIndex == 2 ? 0.3f : 0.18f));
			if (lookIndex == 6) mask = Clamp((1 - std::fabs(x - w * 0.5f) / (w * 0.32f) - std::fabs(y - h * 0.5f) / (h * 0.4f)) * h * 0.4f + 0.5f, 0, 1);
			if (lookIndex == 7) mask = Clamp(h * 0.36f - std::hypot(x - w * 0.5f, y - h * 0.5f) + 0.5f, 0, 1);
			if (lookIndex != 0) masks[at] = (unsigned char)(255 * mask);
		}
	}
	float p[kCoreCount];
	std::copy(core, core + kCoreCount, p);
	for (int k : kLengths) p[k] *= h / 240.f;
	if (letters && !liquidLetters) p[20] = 0;
	if (thumbnail) { p[3] = h * 0.12f; p[22] = std::max(0.7f, p[22]); }
	int err = lg_render(p, scene.data(), letters ? letters->data() : masks.data(), thumbnail && lookIndex != 0 ? 1 : 2, w, h, out.data(), nullptr);
	if (err) return err;
	bgra.resize(out.size());
	for (size_t at = 0; at < out.size(); at += 4) {
		for (int c = 0; c < 3; c++) bgra[at + 2 - c] = (unsigned char)((out[at + c] * out[at + 3] + scene[at + c] * (255 - out[at + 3]) + 127) / 255);
		bgra[at + 3] = 255;
	}
	return 0;
}

struct Surface {
	Box box;
	bool active;
	std::vector<unsigned char> pixels;
};
struct Skin {
	int width = 0, bakes = 0;
	std::vector<unsigned char> background;
	std::vector<Surface> surfaces;
	const Surface* Find(Box b, bool active = false) const
	{
		for (const auto& s : surfaces) if (s.active == active && s.box.x == b.x && s.box.y == b.y && s.box.w == b.w && s.box.h == b.h) return &s;
		return nullptr;
	}
	int Add(const std::vector<unsigned char>& scene, Box b, float radius, bool active = false)
	{
		if (Find(b, active)) return 0;
		const int pad = 8, w = (int)std::ceil(b.w) + pad * 2, h = (int)std::ceil(b.h) + pad * 2;
		std::vector<unsigned char> local(size_t(w) * h * 4), mask(size_t(w) * h), out(local.size());
		for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
			const size_t at = size_t(y) * w + x;
			int sx = (int)Clamp(b.x + x - pad, 0, float(width - 1)), sy = (int)Clamp(b.y + y - pad, 0, float(kHeight - 1));
			std::memcpy(&local[at * 4], &scene[(size_t(sy) * width + sx) * 4], 4);
			mask[at] = (unsigned char)(255 * RoundMask(x - pad + 0.5f, y - pad + 0.5f, b.w, b.h, radius));
		}
		float p[kCoreCount]; std::copy(kDefaults, kDefaults + kCoreCount, p);
		p[2] = 18; p[3] = std::min(6.f, b.h / 4); p[4] = 3; p[5] = 8; p[7] = active ? 0.06f : 0.025f;
		p[9] = active ? 0.03f : 0.12f; p[10] = active ? 0.3f : 0.16f; p[11] = 0.17f;
		p[12] = active ? 0.65f : 0.18f; p[20] = 0; p[22] = 0.7f;
		int err = lg_render(p, local.data(), mask.data(), 1, w, h, out.data(), nullptr);
		if (err) return err;
		Surface s{ b, active, std::vector<unsigned char>(size_t((int)std::ceil(b.w)) * (int)std::ceil(b.h) * 4) };
		for (int y = 0; y < (int)std::ceil(b.h); y++) for (int x = 0; x < (int)std::ceil(b.w); x++) {
			const size_t src = (size_t(y + pad) * w + x + pad) * 4, dst = (size_t(y) * (int)std::ceil(b.w) + x) * 4;
			for (int c = 0; c < 3; c++) s.pixels[dst + 2 - c] = out[src + c];
			s.pixels[dst + 3] = out[src + 3];
		}
		surfaces.push_back(std::move(s));
		return 0;
	}
	int Ensure(int w)
	{
		if (width == w && !background.empty()) return 0;
		width = w; surfaces.clear(); background.clear();
		std::vector<unsigned char> scene; Scene(w, kHeight, scene, true);
		int err = 0;
		auto add = [&](Box b, float radius, bool active = false) { if (!err) err = Add(scene, b, radius, active); };
		add(Header((float)w), 16, true);
		const auto tabs = Tabs();
		for (int t = 0; t < (int)tabs.size(); t++) {
			add(TabBox(t, (float)w), 16); add(TabBox(t, (float)w), 16, true);
			for (int n = 0; n < (int)tabs[t].rows.size(); n++) {
				const Row& r = kRows[tabs[t].rows[n]];
				const Box b = RowBox(tabs[t], n, (float)w);
				add(RowSurface(b), 12);
				if (r.kind == K_FLOAT) add(Track(b), 3);
				if (r.kind == K_ANGLE) add(DialBox(b), 16);
				if (r.kind == K_CHECK) { add(ToggleBox(b), 14); add(ToggleBox(b), 14, true); }
				if (r.kind == K_POPUP) for (int i = 0, count = (int)Items(r.items).size(); i < count; i++) {
					add(Segment(b, i, count), 10); add(Segment(b, i, count), 10, true);
				}
			}
		}
		for (int i = 0; i < kLookCount; i++) add(LookBox(i, (float)w), 12);
		for (int i = 0; i < 2; i++) add(LinkBox(i, (float)w), 12, true);
		// Moving thumbs reuse one glass sprite; dragging never re-bakes the skin.
		add({ 0, 0, 20, 20 }, 10, true);
		if (err) { width = 0; return err; }
		background = std::move(scene);
		for (size_t i = 0; i < background.size(); i += 4) std::swap(background[i], background[i + 2]);
		bakes++;
		return 0;
	}
};
}
