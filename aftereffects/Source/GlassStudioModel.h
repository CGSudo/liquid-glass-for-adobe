// SDK-free Studio layout and interaction rules keep drawing and hit-testing in agreement.
#pragma once
#include "GlassHost.h"
#include <string>

namespace studio {
const int kWidth = 420, kHeight = 520;
struct Box {
	float x, y, w, h;
	bool contains(float px, float py) const { return px >= x && py >= y && px < x + w && py < y + h; }
};
struct Tab { const char* label; std::vector<int> rows; };
enum Target { None, Blob, LookCard, TabButton, Control };
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
	return tabs;
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
inline Box Header(float w) { return { 0, 0, w, 30 }; }
inline Box Hero(float w) { return { 14, Header(w).h + 6, w - 28, 88 }; }
inline float TabsTop() { const auto h = Hero(kWidth); return h.y + h.h + 8; }
inline Box TabBox(int i, float w) { return { 14 + (i % 4) * ((w - 28) / 4), TabsTop() + (i / 4) * 24, (w - 28) / 4 - 4, 21 }; }
inline float BodyTop() { static const float top = TabsTop() + ((Tabs().size() + 3) / 4) * 24 + 8; return top; }
inline Box StatusBox(float w) { return { 14, kHeight - 28.f, w - 28, 28 }; }
inline Box LookBox(int i, float w) { return { 14 + (i % 3) * ((w - 28) / 3), BodyTop() + (i / 3) * 76, (w - 28) / 3 - 6, 72 }; }
inline Box Thumbnail(const Box& card) { return { card.x + 6, card.y + 4, std::floor(card.w - 12), 44 }; }
inline float RowHeight(const Row& r) { return r.kind == K_POPUP && Items(r.items).size() > 3 ? 58.f : 38.f; }
inline Box RowBox(const Tab& tab, int n, float w)
{
	float y = BodyTop();
	for (int i = 0; i < n; i++) y += RowHeight(kRows[tab.rows[i]]);
	return { 22, y, w - 44, RowHeight(kRows[tab.rows[n]]) - 4 };
}
inline Box Track(const Box& row) { return { row.x, row.y + 23, row.w, 6 }; }
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
inline Box DialBox(const Box& row) { return { row.x + row.w - 30, row.y + 2, 28, 28 }; }
inline Box Segment(const Box& row, int i, int count)
{
	const int cols = std::min(3, count);
	return { row.x + (i % cols) * row.w / cols, row.y + 16 + (i / cols) * 19.f, row.w / cols - 3, 17 };
}
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
	if (active >= 0 && active < (int)tabs.size())
		for (int i = 0; i < (int)tabs[active].rows.size(); i++) if (RowBox(tabs[active], i, w).contains(x, y)) return { Control, i };
	return {};
}
inline const char* BackdropStatus(bool helper, bool queued)
{
	return helper ? "Live glass - edit your layer to change its outline" : queued ? "Setting up..." : "Backdrop layer missing - re-apply the effect";
}
inline bool MatchesLook(int n, const float* core)
{
	float expected[kCoreCount];
	std::copy(core, core + kCoreCount, expected);
	ApplyLook(n, expected);
	for (int i = 0; i < kCoreCount; i++) if (std::fabs(expected[i] - core[i]) > 0.5f / 255.f) return false;
	return true;
}

// An opaque test scene makes engine RGBA safe to upload as a Drawbot BGRA image.
inline int Preview(const float* core, int w, int h, const float* bx, const float* by, std::vector<unsigned char>& bgra)
{
	std::vector<unsigned char> scene(size_t(w) * h * 4), masks(size_t(w) * h * 2), out(scene.size());
	for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
		const size_t at = size_t(y) * w + x;
		float u = float(x) / w, v = float(y) / h;
		float wave = 0.5f + 0.5f * std::sin(u * 12 + v * 6);
		scene[at * 4] = (unsigned char)(35 + 195 * u);
		scene[at * 4 + 1] = (unsigned char)(45 + 155 * wave);
		scene[at * 4 + 2] = (unsigned char)(210 - 130 * v);
		if ((x / 12 + y / 12) % 2 == 0) for (int c = 0; c < 3; c++) scene[at * 4 + c] = (unsigned char)(scene[at * 4 + c] * 0.82f);
		scene[at * 4 + 3] = 255;
		for (int i = 0; i < 2; i++) {
			float dx = x + 0.5f - bx[i] * w, dy = y + 0.5f - by[i] * h;
			masks[size_t(i) * w * h + at] = (unsigned char)(Clamp(h * 0.27f - std::sqrt(dx * dx + dy * dy) + 0.5f, 0, 1) * 255);
		}
	}
	float p[kCoreCount];
	std::copy(core, core + kCoreCount, p);
	for (int k : kLengths) p[k] *= h / 240.f;
	int err = lg_render(p, scene.data(), masks.data(), 2, w, h, out.data(), nullptr);
	if (err) return err;
	bgra.resize(out.size());
	for (size_t at = 0; at < out.size(); at += 4) {
		for (int c = 0; c < 3; c++) bgra[at + 2 - c] = (unsigned char)((out[at + c] * out[at + 3] + scene[at + c] * (255 - out[at + 3]) + 127) / 255);
		bgra[at + 3] = 255;
	}
	return 0;
}
}
