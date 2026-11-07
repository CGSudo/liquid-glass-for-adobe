// Host logic without After Effects or Premiere: parameter mapping, looks, shapes, pixel formats, plus a 1080p timing.
#include "../Source/GlassHost.h"
#include "../Source/GlassStudioModel.h"
#include "../Source/GlassPremiere.h"
#include "../../setup/InstallPaths.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>

static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

static float TextWidth(const char* text, int size = 12)
{
	HDC dc = CreateCompatibleDC(nullptr);
	HFONT font = CreateFontW(-size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
	CHECK(dc && font);
	if (!dc || !font) { if (dc) DeleteDC(dc); if (font) DeleteObject(font); return 10000; }
	HGDIOBJ old = SelectObject(dc, font);
	SIZE extent = {};
	CHECK(GetTextExtentPoint32A(dc, text, (int)std::strlen(text), &extent));
	SelectObject(dc, old); DeleteObject(font); DeleteDC(dc);
	return (float)extent.cx;
}
static bool Overlap(const studio::Box& a, const studio::Box& b)
{
	return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

static Settings Base(int w, int h)
{
	Settings s;
	std::copy(kDefaults, kDefaults + kCoreCount, s.core);
	s.shape[0].cx = w * 0.5f;
	s.shape[0].cy = h * 0.5f;
	s.shape[0].w = w * 0.5f;
	s.shape[0].h = h * 0.5f;
	s.shape[0].radius = 20;
	return s;
}

template <class T> struct Frame {
	std::vector<T> px;
	World world;
	Frame(int w, int h, int ox = 0, int oy = 0) : px(size_t(w) * h * 4)
	{
		world.data = reinterpret_cast<unsigned char*>(px.data());
		world.rowbytes = w * 4 * sizeof(T);
		world.w = w;
		world.h = h;
		world.ox = ox;
		world.oy = oy;
	}
};

template <class P> static void Gradient(Frame<typename P::Chan>& f)
{
	for (int y = 0; y < f.world.h; y++)
		for (int x = 0; x < f.world.w; x++) {
			typename P::Chan* o = PixelAt<P>(f.world, f.world.ox + x, f.world.oy + y);
			o[P::R] = (typename P::Chan)(P::Max * x / f.world.w);
			o[P::G] = (typename P::Chan)(P::Max * y / f.world.h);
			o[P::B] = (typename P::Chan)(P::Max * 0.5f);
			o[P::A] = (typename P::Chan)P::Max;
		}
}

// Same picture through every pixel format: passes through outside, changes inside, agrees with 8-bit ARGB.
template <class P> static void FormatCase(PixFmt fmt, const Frame<uint8_t>& ref)
{
	const int w = ref.world.w, h = ref.world.h;
	Frame<typename P::Chan> in(w, h), out(w, h);
	Gradient<P>(in);
	Scratch sc;
	Settings s = Base(w, h);
	CHECK(RenderFrame(fmt, s, in.world, out.world, nullptr, nullptr, sc) == 0);
	const typename P::Chan* corner = PixelAt<P>(out.world, 1, 1), * cornerIn = PixelAt<P>(in.world, 1, 1);
	CHECK(corner[P::R] == cornerIn[P::R] && corner[P::G] == cornerIn[P::G] && corner[P::A] == cornerIn[P::A]);
	const typename P::Chan* mid = PixelAt<P>(out.world, w / 2, h / 2);
	const uint8_t* mid8 = PixelAt<PxARGB8>(ref.world, w / 2, h / 2);
	CHECK(std::fabs(mid[P::R] / P::Max - mid8[PxARGB8::R] / 255.f) < 0.02f);
	CHECK(std::fabs(mid[P::G] / P::Max - mid8[PxARGB8::G] / 255.f) < 0.02f);
	CHECK(std::fabs(mid[P::B] / P::Max - mid8[PxARGB8::B] / 255.f) < 0.02f);
	CHECK(mid[P::A] == (typename P::Chan)P::Max);
}

template <class P> static void MaskAndTintCase(PixFmt fmt)
{
	Settings s = Base(40, 40);
	s.brightnessMask = true;
	typename P::Chan pixel[4] = {};
	pixel[P::A] = FromUnit<P>(0.5f);
	pixel[P::R] = FromUnit<P>(1);
	CHECK(std::abs(int(LayerMask<P>(pixel, s)) - 27) <= 1);
	pixel[P::R] = 0; pixel[P::G] = FromUnit<P>(1);
	CHECK(std::abs(int(LayerMask<P>(pixel, s)) - 91) <= 1);
	pixel[P::G] = 0; pixel[P::B] = FromUnit<P>(1);
	CHECK(std::abs(int(LayerMask<P>(pixel, s)) - 9) <= 1);
	pixel[P::R] = pixel[P::G] = FromUnit<P>(1);
	CHECK(std::abs(int(LayerMask<P>(pixel, s)) - 128) <= 1);
	s.invertMask = true;
	CHECK(LayerMask<P>(pixel, s) == 0);
	pixel[P::R] = pixel[P::G] = pixel[P::B] = 0;
	CHECK(std::abs(int(LayerMask<P>(pixel, s)) - 128) <= 1);
	pixel[P::A] = 0;
	CHECK(LayerMask<P>(pixel, s) == 0);
	pixel[P::A] = FromUnit<P>(1); pixel[P::R] = FromUnit<P>(1);
	s.invertMask = false; s.maskSoftness = 0;
	CHECK(LayerMask<P>(pixel, s) == 0);
	s.threshold = 0.2f;
	CHECK(LayerMask<P>(pixel, s) == 255);
	s.maskSoftness = 0.2f;
	CHECK(std::abs(int(LayerMask<P>(pixel, s)) - 144) <= 1);

	Frame<typename P::Chan> in(40, 40, -8, 13), clear(40, 40, -8, 13), tint(40, 40, -8, 13);
	Frame<uint8_t> backdrop(60, 60);
	Gradient<P>(in); Gradient<PxARGB8>(backdrop);
	s = Base(40, 40);
	s.ownLayer = s.clipAlpha = true;
	s.core[20] = 0;
	Scratch sc;
	CHECK(RenderFrame(fmt, s, in.world, clear.world, nullptr, nullptr, sc, &backdrop.world) == 0);
	s.core[9] = 1; s.core[10] = s.core[11] = 0; s.core[12] = 1;
	CHECK(RenderFrame(fmt, s, in.world, tint.world, nullptr, nullptr, sc, &backdrop.world) == 0);
	const auto* c = PixelAt<P>(clear.world, 12, 33);
	const auto* t = PixelAt<P>(tint.world, 12, 33);
	CHECK(t[P::R] > c[P::R] && t[P::G] < c[P::G] && t[P::B] < c[P::B]);
	// Input RGB never supplies the material in the alpha path.
	for (size_t i = 0; i < in.px.size(); i += 4) in.px[i + P::R] = in.px[i + P::G] = in.px[i + P::B] = 0;
	CHECK(RenderFrame(fmt, s, in.world, clear.world, nullptr, nullptr, sc, &backdrop.world) == 0);
	CHECK(clear.px == tint.px);
	s.brightnessMask = true;
	CHECK(RenderFrame(fmt, s, in.world, clear.world, nullptr, nullptr, sc, &backdrop.world) == 0);
	CHECK(PixelAt<P>(clear.world, 12, 33)[P::A] == 0);
	s.invertMask = true;
	CHECK(RenderFrame(fmt, s, in.world, clear.world, nullptr, nullptr, sc, &backdrop.world) == 0);
	CHECK(PixelAt<P>(clear.world, 12, 33)[P::A] == typename P::Chan(P::Max));
}

template <class P> static void PremiereCase(PixFmt fmt)
{
	Frame<typename P::Chan> bg(96, 64), source(96, 64), out(96, 64), other(96, 64);
	for (int y = 0; y < 64; y++) for (int x = 0; x < 96; x++) {
		auto* b = PixelAt<P>(bg.world, x, y);
		b[P::R] = b[P::G] = b[P::B] = FromUnit<P>(0.2f + 0.5f * x / 96);
		b[P::A] = FromUnit<P>(1);
		auto* p = PixelAt<P>(source.world, x, y);
		p[P::B] = FromUnit<P>(1);
		p[P::A] = FromUnit<P>(y > 16 && y < 48 && ((x > 16 && x < 44) || (x > 48 && x < 78)) ? 1.f : 0.f);
	}
	Scratch sc;
	Settings s = Base(96, 64);
	s.core[4] = 0; // dispersion can colour a gradient even when the backdrop is neutral
	CHECK(PremiereHasOutline<P>(source.world));
	CHECK(!PremiereHasOutline<P>(bg.world));
	CHECK(RenderPremiereFrame(fmt, s, source.world, out.world, nullptr, nullptr, true, sc) == 0);
	CHECK(out.px == source.px); // no background must never manufacture black glass
	CHECK(RenderPremiereFrame(fmt, s, source.world, out.world, nullptr, &bg.world, true, sc) == 0);
	CHECK(out.px != bg.px && out.px != source.px);
	for (int y = 0; y < 64; y++) for (int x = 0; x < 96; x++) {
		const auto* p = PixelAt<P>(out.world, x, y);
		CHECK(std::abs(float(p[P::R]) - float(p[P::B])) <= P::Max / 255 + 0.001f);
		CHECK(p[P::A] == FromUnit<P>(1));
		PixelAt<P>(source.world, x, y)[P::R] = FromUnit<P>(1);
	}
	CHECK(RenderPremiereFrame(fmt, s, source.world, other.world, nullptr, &bg.world, true, sc) == 0);
	CHECK(out.px == other.px); // source RGB cannot tint the glass
	CHECK(RenderPremiereFrame(fmt, s, bg.world, other.world, &source.world, nullptr, true, sc) == 0);
	CHECK(out.px == other.px); // adjustment input and direct background share the renderer
	Frame<typename P::Chan> empty(96, 64);
	CHECK(RenderPremiereFrame(fmt, s, empty.world, out.world, nullptr, &bg.world, true, sc) == 0);
	CHECK(out.px == bg.px); // the background suite delegates compositing even for an empty source
	CHECK(RenderPremiereFrame(fmt, s, bg.world, out.world, &empty.world, nullptr, true, sc) == 0);
	CHECK(out.px == bg.px);
	CHECK(RenderPremiereFrame(fmt, s, bg.world, out.world, nullptr, nullptr, true, sc) == 0);
	CHECK(out.px == bg.px); // plain opaque clips pass through
	CHECK(PremiereRowByLabel("Tint amount") == IndexOf(P_TINT_AMT));
	CHECK(PremiereRowByLabel("Liquid letters") == IndexOf(P_LIQUID_LETTERS));
	CHECK(PremiereRowByLabel("No such parameter") == -1);
	CHECK(RenderPremiereFrame(fmt, s, bg.world, out.world, &source.world, nullptr, true, sc) == 0);
	s.core[20] = 0;
	CHECK(RenderPremiereFrame(fmt, s, bg.world, other.world, &source.world, nullptr, true, sc) == 0);
	CHECK(out.px != other.px); // separate alpha islands respond to Merge
	CHECK(std::memcmp(PixelAt<P>(other.world, 46, 32), PixelAt<P>(bg.world, 46, 32), 4 * sizeof(typename P::Chan)) == 0);
	s.core[12] = 0.8f; s.core[9] = 1; s.core[10] = s.core[11] = 0;
	CHECK(RenderPremiereFrame(fmt, s, bg.world, out.world, &source.world, nullptr, true, sc) == 0);
	const auto* tinted = PixelAt<P>(out.world, 30, 32);
	CHECK(tinted[P::R] > tinted[P::B]);
	s.noGlass = true;
	auto* translucent = PixelAt<P>(source.world, 30, 32);
	translucent[P::A] = FromUnit<P>(0.5f);
	CHECK(RenderPremiereFrame(fmt, s, source.world, out.world, nullptr, &bg.world, true, sc) == 0);
	const auto* mixed = PixelAt<P>(out.world, 30, 32);
	const auto* behind = PixelAt<P>(bg.world, 30, 32);
	CHECK(std::abs(float(mixed[P::R]) / P::Max - (1.f + float(behind[P::R]) / P::Max) * 0.5f) < 0.01f);
	CHECK(mixed[P::A] == FromUnit<P>(1));
}

int main()
{
	CHECK(setup::SupportedAE(L"24.0") && setup::SupportedAE(L"2026"));
	CHECK(!setup::SupportedAE(L"23.6") && !setup::SupportedAE(L"2023") && !setup::SupportedAE(L"Beta"));
	CHECK(!setup::SupportedAE(L"999999999999") && !setup::SupportedAE(L"2026other"));
	const auto install = setup::NativePlan({ L"X:\\AI" }, { L"X:\\AE" }, L"X:\\Program Files");
	CHECK(install.copies.size() == 3 && install.obsolete.size() == 7);
	CHECK(install.obsolete.back() == L"X:\\Program Files\\Adobe\\Common\\Plug-ins\\7.0\\MediaCore\\CGSudo\\LiquidGlass.aex");
	CHECK(install.copies[1].destination == L"X:\\AE\\Support Files\\Plug-ins\\CGSudo\\LiquidGlass.aex");
	CHECK(install.copies[2].file == L"LiquidGlassBrightness.aex");
	for (const auto& path : install.obsolete) {
		CHECK(path.find(L'*') == std::wstring::npos && path.find(L"CGSudoTest") == std::wstring::npos);
		CHECK(path.find(L"APPDATA") == std::wstring::npos && path.find(L".a") == path.size() - 4);
	}
	PremiereCase<PxBGRA8>(FMT_BGRA8);
	PremiereCase<PxBGRAF>(FMT_BGRAF);
	// Studio covers every material row, with shared layout edges and clamped interaction math.
	const auto tabs = studio::Tabs();
	int topics = 2;
	for (const Row& r : kRows) if ((r.kind == K_TOPIC || r.kind == K_TOPIC_CLOSED) && r.id != P_T_SHAPE && r.id != P_T_SHAPE2 && r.id != P_T_ANIM) topics++;
	CHECK(tabs.size() == topics);
	CHECK(studio::kHeight <= 640);
	CHECK(std::strcmp(tabs[0].label, "Looks") == 0 && tabs[0].rows.empty());
	CHECK(!std::strcmp(tabs.back().label, "About"));
	CHECK(studio::kLabelSize >= 12 && studio::kValueSize >= 12);
	CHECK(studio::PanelWidth(100) == 280 && studio::PanelWidth(1000) == 800);
	for (float width : { 280.f, 420.f, 800.f }) {
		const float bx[] = { 0.39f, 0.61f }, by[] = { 0.5f, 0.5f };
		CHECK(studio::PanelWidth(width) == width);
		const auto status = studio::StatusBox(width);
		CHECK(status.x >= 0 && status.x + status.w <= width && status.y + status.h == studio::kHeight);
		const auto inside = [&](const studio::Box& b) {
			CHECK(b.x >= 0 && b.y >= 0 && b.w > 0 && b.h > 0);
			CHECK(b.x + b.w <= width && b.y + b.h < status.y);
		};
		inside(studio::Header(width));
		inside(studio::Hero(width));
		const auto help = studio::HelpBox(width);
		inside(help);
		CHECK(studio::kHeight <= 640 && !Overlap(help, studio::Hero(width)));
		CHECK(help.y + help.h <= studio::TabsTop());
		const auto checkHelp = [&](const std::string& text) {
			CHECK(!text.empty());
			const auto lines = studio::WrapHelp(text, help.w, TextWidth);
			CHECK(lines.size >= 8 && lines.size <= 12);
			CHECK(TextWidth(lines.first.c_str(), lines.size) <= help.w);
			if (TextWidth(lines.second.c_str(), lines.size) > help.w) printf("Help overflow at %.0fpx: %s\n", width, text.c_str());
			CHECK(TextWidth(lines.second.c_str(), lines.size) <= help.w);
			const std::string shown = lines.first + (lines.second.empty() ? "" : " " + lines.second);
			CHECK(shown == text || shown == text.substr(0, text.find(". ") + 1));
			CHECK(28 <= help.h);
		};
		CHECK(!Overlap(studio::Header(width), studio::Hero(width)));
		CHECK((tabs.size() + studio::TabColumns(width) - 1) / studio::TabColumns(width) <= 2);
		CHECK(studio::TabLabels(width) == (width == 800));
		for (int t = 0; t < (int)tabs.size(); t++) {
			CHECK(*studio::TabHelp(tabs[t].label));
			checkHelp(studio::HelpText(tabs, t, { studio::TabButton, t }, false, false, false));
			for (bool text : { false, true }) for (bool adjustment : { false, true }) for (bool premiere : { false, true })
				checkHelp(studio::HelpText(tabs, t, {}, text, adjustment, premiere));
			const auto button = studio::TabBox(t, width);
			inside(button);
			CHECK(button.w >= 28 && button.h >= 32);
			if (studio::TabLabels(width)) CHECK(TextWidth(studio::TabLabel(tabs[t].label)) <= button.w - 32);
			CHECK(button.y >= studio::Hero(width).y + studio::Hero(width).h + 24);
			CHECK(button.y + button.h <= studio::BodyTop(width) - 8);
			for (int j = 0; j < t; j++) CHECK(!Overlap(button, studio::TabBox(j, width)));
			CHECK((studio::HitTest(button.x + 2, button.y + 2, width, tabs, t, bx, by) == studio::Hit{ studio::TabButton, t }));
			if (t == 0) for (int i = 0; i < kLookCount; i++) {
				checkHelp(studio::HelpText(tabs, t, { studio::LookCard, i }, false, false, false));
				const auto card = studio::LookBox(i, width), thumb = studio::Thumbnail(card);
				inside(card); inside(thumb);
				CHECK(TextWidth(kLooks[i].name) <= card.w - 16);
				for (int j = 0; j < i; j++) CHECK(!Overlap(card, studio::LookBox(j, width)));
				CHECK((studio::HitTest(thumb.x + 2, thumb.y + 2, width, tabs, t, bx, by) == studio::Hit{ studio::LookCard, i }));
				CHECK(thumb.x >= card.x && thumb.x + thumb.w <= card.x + card.w);
				CHECK(thumb.y >= card.y && thumb.y + thumb.h <= card.y + card.h);
			}
			for (int n = 0; n < (int)tabs[t].rows.size(); n++) {
				const auto b = studio::RowBox(tabs[t], n, width);
				const Row& row = kRows[tabs[t].rows[n]];
				CHECK(*studio::RowHelp(row.id));
				checkHelp(studio::HelpText(tabs, t, { studio::Control, n }, false, false, false));
				inside(b);
				CHECK((studio::HitTest(b.x + 2, b.y + 2, width, tabs, t, bx, by) == studio::Hit{ studio::Control, n }));
				inside(studio::RowSurface(b));
				const auto label = studio::RowLabel(b, row);
				inside(label);
				CHECK(TextWidth(row.label) <= label.w);
				CHECK(b.y >= studio::BodyTop(width));
				for (int j = 0; j < n; j++) CHECK(!Overlap(studio::RowSurface(b), studio::RowSurface(studio::RowBox(tabs[t], j, width))));
				if (row.kind == K_FLOAT) {
					const auto track = studio::Track(b);
					inside(track);
					CHECK(track.y + track.h <= b.y + b.h);
					const float current = row.min + (row.max - row.min) * 0.25f;
					CHECK(!studio::NearTrack(track, b.x, b.y + 10));
					CHECK(studio::SliderValue(row, current, track, b.x, b.y + 10, 0, true, false, false) == current);
					CHECK(studio::SliderValue(row, current, track, b.x, b.y + 10, 0, false, false, false) == current);
					CHECK(studio::SliderValue(row, current, track, b.x, b.y + 10, 3, false, false, false) > current);
					CHECK(studio::SliderValue(row, current, track, track.x + track.w, track.y - 5, 0, true, false, false) == row.max);
					CHECK(studio::SliderValue(row, current, track, track.x + track.w, track.y - 7, 0, true, false, false) == current);
					CHECK(studio::SliderValue(row, current, track, track.x, track.y, 0, true, true, false) == current);
					CHECK(studio::SliderValue(row, current, track, b.x, b.y, 0, true, false, true) == row.dflt);
				} else if (row.kind == K_ANGLE) {
					const auto dial = studio::DialBox(b);
					inside(dial);
					CHECK(dial.y + dial.h <= b.y + b.h);
				} else if (row.kind == K_CHECK) {
					inside(studio::ToggleBox(b));
					CHECK(studio::ToggleBox(b).y + studio::ToggleBox(b).h <= b.y + b.h);
				} else if (row.kind == K_POPUP) {
					const int count = (int)studio::Items(row.items).size();
					for (int i = 0; i < count; i++) {
						const auto segment = studio::Segment(b, i, count);
						inside(segment);
						CHECK(TextWidth(studio::Items(row.items)[i].c_str()) <= segment.w - 16);
						CHECK(!Overlap(label, segment));
						for (int j = 0; j < i; j++) CHECK(!Overlap(segment, studio::Segment(b, j, count)));
						CHECK(segment.y + segment.h <= b.y + b.h);
					}
				}
			}
			if (t == (int)tabs.size() - 1) for (int i = 0; i < 2; i++) {
				checkHelp(studio::HelpText(tabs, t, { studio::AboutLink, i }, false, false, false));
				const auto link = studio::LinkBox(i, width);
				inside(link);
				CHECK(TextWidth(i ? "GitHub / Source & updates" : "LinkedIn / CGSudo") <= link.w - 40);
				CHECK((studio::HitTest(link.x + 2, link.y + 2, width, tabs, t, bx, by) == studio::Hit{ studio::AboutLink, i }));
			}
		}
		checkHelp(studio::HelpText(tabs, 0, { studio::Blob, 0 }, false, false, false));
	}
	CHECK(!*studio::RowHelp(-1));
	CHECK(*studio::RowHelp(P_THRESHOLD) && *studio::RowHelp(P_MASK_SOFTNESS) && *studio::RowHelp(P_INVERT));
	CHECK(*studio::GlassFromHelp());
	CHECK(std::strcmp(studio::RowHelp(P_SHAPE_LAYER, true), studio::GlassFromHelp()) == 0);
	CHECK(std::strcmp(studio::RowHelp(P_SHAPE_LAYER), studio::RowHelp(P_SHAPE_LAYER, true)) != 0);
	CHECK(std::strcmp(studio::BackdropStatus(false, true), "Setting up...") == 0);
	CHECK(std::strcmp(studio::BackdropStatus(false, false), "Backdrop layer missing - re-apply the effect") == 0);
	CHECK(std::strcmp(studio::BackdropStatus(true, true), studio::BackdropStatus(true, false)) == 0);
	int covered[kCoreCount] = {};
	for (const auto& tab : tabs) {
		for (int n = 0; n < (int)tab.rows.size(); n++) {
			const Row& r = kRows[tab.rows[n]];
			CHECK(IndexOf(r.id) == tab.rows[n] + 1);
			CHECK(r.kind == K_FLOAT || r.kind == K_ANGLE || r.kind == K_POPUP || r.kind == K_COLOR || r.kind == K_CHECK);
			if (r.core >= 0) for (int c = 0; c < (r.kind == K_COLOR ? 3 : 1); c++) covered[r.core + c]++;
			const auto box = studio::RowBox(tab, n, studio::kWidth);
			CHECK(box.y + box.h < studio::kHeight - 40);
			CHECK(box.contains(box.x, box.y) && !box.contains(box.x + box.w, box.y));
			if (r.kind == K_FLOAT) {
				const auto track = studio::Track(box);
				CHECK(studio::ValueAt(r, track.x - 100, track) == r.min);
				CHECK(studio::ValueAt(r, track.x + track.w + 100, track) == r.max);
				CHECK(std::fabs(studio::Fraction(r, studio::ValueAt(r, track.x + track.w / 2, track)) - 0.5f) < 0.001f);
				CHECK(std::fabs(studio::DragValue(r, r.min, track.w, track.w, true) - (r.min + (r.max - r.min) * 0.1f)) < 0.001f);
			}
			if (r.kind == K_POPUP) {
				const auto items = studio::Items(r.items);
				for (int i = 0; i < (int)items.size(); i++) {
					const auto segment = studio::Segment(box, i, (int)items.size());
					CHECK(segment.y + segment.h <= box.y + box.h);
				}
			}
		}
	}
	for (int i = 0; i < kCoreCount; i++) CHECK(covered[i] == (i >= 9 && i <= 12 ? 2 : 1));
	int quickTint = -1, colourTint = -2;
	for (const auto& tab : tabs) for (int n : tab.rows) if (kRows[n].id == P_TINT) {
		if (!std::strcmp(tab.label, "Quick adjust")) quickTint = n;
		if (!std::strcmp(tab.label, "Colour")) colourTint = n;
	}
	CHECK(quickTint >= 0 && quickTint == colourTint);
	CHECK(studio::RowByLabel("Tint colour") == quickTint);
	CHECK(studio::RowByLabel("Nonexistent setting") == -1);
	const auto inTab = [&](const char* label, int id) {
		for (const auto& tab : tabs) if (!std::strcmp(tab.label, label))
			for (int row : tab.rows) if (kRows[row].id == id) return true;
		return false;
	};
	CHECK(inTab("Lens", P_LIQUID_LETTERS));
	CHECK(kRows[IndexOf(P_SHAPE_LAYER) - 1].kind == K_LAYER);
#ifdef LG_BRIGHTNESS
	CHECK(inTab("Brightness shape", P_THRESHOLD));
	CHECK(inTab("Brightness shape", P_MASK_SOFTNESS));
	CHECK(inTab("Brightness shape", P_INVERT));
#endif
	CHECK(std::fabs(studio::AngleAt(1, 0, 0, 0) - 90) < 0.001f);
	CHECK(studio::AngleDelta(-179, 179) == 2);
	float bx[] = { 0.38f, 0.62f }, by[] = { 0.5f, 0.5f };
	const auto hero = studio::Hero(studio::kWidth);
	CHECK(studio::BlobHit(hero.x + bx[0] * hero.w, hero.y + by[0] * hero.h, bx, by, hero) == 0);
	CHECK(studio::BlobHit(-1, -1, bx, by, hero) == -1);
	CHECK(studio::HitTest(-1, -1, studio::kWidth, tabs, 0, bx, by).type == studio::None);
	for (int i = 0; i < kLookCount; i++) {
		const auto b = studio::LookBox(i, studio::kWidth);
		CHECK((studio::HitTest(b.x + 2, b.y + 2, studio::kWidth, tabs, 0, bx, by) == studio::Hit{ studio::LookCard, i }));
	}
	for (int t = 0; t < (int)tabs.size(); t++) {
		const auto b = studio::TabBox(t, studio::kWidth);
		CHECK((studio::HitTest(b.x + 2, b.y + 2, studio::kWidth, tabs, 0, bx, by) == studio::Hit{ studio::TabButton, t }));
		for (int n = 0; n < (int)tabs[t].rows.size(); n++) {
			const auto r = studio::RowBox(tabs[t], n, studio::kWidth);
			CHECK((studio::HitTest(r.x + 2, r.y + 2, studio::kWidth, tabs, t, bx, by) == studio::Hit{ studio::Control, n }));
		}
		if (t != 0) {
			const auto look = studio::LookBox(0, studio::kWidth);
			CHECK(studio::HitTest(look.x + 2, look.y + 2, studio::kWidth, tabs, t, bx, by).type != studio::LookCard);
		}
	}
	for (int i = 0; i < kLookCount; i++) {
		float p[kCoreCount];
		std::copy(kDefaults, kDefaults + kCoreCount, p);
		ApplyLook(i, p);
		CHECK(studio::MatchesLook(i, p));
		p[3] += 17; p[20] += 13;
		CHECK(studio::MatchesLook(i, p));
		p[2] += 1;
		CHECK(!studio::MatchesLook(i, p));
		std::vector<unsigned char> thumb;
		ApplyLook(i, p);
		CHECK(studio::Preview(p, 112, 32, bx, by, thumb) == 0);
		CHECK(thumb.size() == 112 * 32 * 4);
	}
	std::vector<unsigned char> preview, moved;
	CHECK(studio::Preview(kDefaults, 160, 80, bx, by, preview) == 0);
	bx[0] += 0.1f;
	CHECK(studio::Preview(kDefaults, 160, 80, bx, by, moved) == 0);
	CHECK(preview.size() == 160 * 80 * 4 && preview != moved);
	bool opaque = true;
	for (size_t i = 3; i < preview.size(); i += 4) opaque = opaque && preview[i] == 255;
	CHECK(opaque);
	// Text keeps separate glyphs even when the material's merge distance is large.
	std::vector<unsigned char> letters;
	CHECK(studio::LetterMasks(256, 184, letters));
	CHECK(letters.size() == 256 * 184 * 2);
	int lit[2] = {}, rightA = -1, leftA = 256;
	for (int y = 0; y < 184; y++) for (int x = 0; x < 256; x++) {
		const size_t at = size_t(y) * 256 + x;
		CHECK(!(letters[at] && letters[256 * 184 + at]));
		if (letters[at]) { lit[0]++; rightA = std::max(rightA, x); }
		if (letters[256 * 184 + at]) { lit[1]++; leftA = std::min(leftA, x); }
	}
	CHECK(lit[0] > 100 && lit[1] > 100 && leftA - rightA >= 4);
	float letterCore[kCoreCount]; std::copy(kDefaults, kDefaults + kCoreCount, letterCore);
	std::vector<unsigned char> separate, merged, zeroMerge;
	letterCore[20] = 200;
	CHECK(studio::Preview(letterCore, 256, 184, bx, by, separate, &letters) == 0);
	CHECK(studio::Preview(letterCore, 256, 184, bx, by, merged, &letters, true) == 0);
	letterCore[20] = 0;
	CHECK(studio::Preview(letterCore, 256, 184, bx, by, zeroMerge, &letters) == 0);
	CHECK(separate == zeroMerge && separate != merged);
	std::vector<unsigned char> scaled;
	const std::vector<unsigned char> swatch = { 0, 0, 0, 255, 100, 100, 100, 255, 200, 200, 200, 255, 100, 100, 100, 255 };
	studio::ScaleImage(swatch, 2, 2, 3, 3, scaled);
	CHECK(scaled.size() == 36 && scaled[0] == 0 && scaled[16] == 100 && scaled[32] == 100 && scaled[19] == 255);
	std::vector<std::vector<unsigned char>> thumbnails(kLookCount);
	for (int i = 0; i < kLookCount; i++) {
		float look[kCoreCount]; std::copy(kDefaults, kDefaults + kCoreCount, look); ApplyLook(i, look);
		CHECK(studio::Preview(look, 112, 52, bx, by, thumbnails[i], nullptr, false, true, i) == 0);
		for (int j = 0; j < i; j++) {
			int difference = 0;
			for (size_t at = 0; at < thumbnails[i].size(); at++) difference += std::abs(int(thumbnails[i][at]) - int(thumbnails[j][at]));
			CHECK(difference > 1000);
		}
	}
	for (int width : { 280, 420, 800 }) {
		studio::Skin skin;
		const auto t0 = std::chrono::steady_clock::now();
		CHECK(skin.Ensure(width) == 0);
		const double bake = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
		CHECK(skin.background.size() == size_t(width) * studio::kHeight * 4);
		const auto* cached = skin.background.data();
		const auto t1 = std::chrono::steady_clock::now();
		for (int i = 0; i < 100; i++) {
			CHECK(skin.Ensure(width) == 0);
			CHECK(skin.Find(studio::Header((float)width), true));
		}
		const double reuse = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t1).count() / 100;
		CHECK(skin.bakes == 1 && skin.background.data() == cached);
		const auto hero = studio::Hero((float)width), renderSize = studio::HeroRenderSize((float)width);
		CHECK(renderSize.w <= 420 && renderSize.h <= hero.h);
		const auto t2 = std::chrono::steady_clock::now();
		CHECK(studio::Preview(kDefaults, (int)renderSize.w, (int)renderSize.h, bx, by, preview) == 0);
		if (renderSize.w != hero.w) studio::ScaleImage(preview, (int)renderSize.w, (int)renderSize.h, (int)hero.w, (int)hero.h, scaled);
		const double render = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t2).count();
		printf("Studio %dpx bake %.1f ms, cached skin %.4f ms, hero %.1f ms (Drawbot upload needs app)\n", width, bake, reuse, render);
		CHECK(skin.Ensure(width == 280 ? 420 : 280) == 0 && skin.bakes == 2);
	}

	// Every engine value is driven by exactly one row.
	CHECK(lg_param_count() == kCoreCount);
	int driven[kCoreCount] = {};
	for (const Row& r : kRows)
		if (r.core >= 0) {
			driven[r.core]++;
			if (r.kind == K_COLOR) {
				driven[r.core + 1]++;
				driven[r.core + 2]++;
			}
			if (r.kind == K_FLOAT) CHECK(r.dflt == (r.core == 3 ? 30.f : r.core == 13 ? 0.25f : kDefaults[r.core]));
			if (r.kind == K_FLOAT) CHECK(r.dflt >= r.min && r.dflt <= r.max);
			if (r.kind == K_POPUP) CHECK((int)r.dflt == (int)kDefaults[r.core] + 1);
		}
	// Tint amount (12) and the colours are rows; nothing is left without a control.
	for (int i = 0; i < kCoreCount; i++) CHECK(driven[i] == 1);

	// Ids are unique and every topic closes.
	int depth = 0;
	for (int i = 0; i < kRowCount; i++) {
		CHECK(IndexOf(kRows[i].id) == i + 1);
		for (int j = i + 1; j < kRowCount; j++) CHECK(kRows[i].id != kRows[j].id);
		if (kRows[i].kind == K_TOPIC || kRows[i].kind == K_TOPIC_CLOSED) depth++;
		if (kRows[i].kind == K_END) depth--;
		CHECK(depth == 0 || depth == 1);
	}
	CHECK(depth == 0);
	CHECK(IndexOf(P_LOOK) == 1);
	for (int id = P_LOOK; id <= P_E_ANIM; id++) CHECK(IndexOf(id) == id + (id > P_MERGE ? 1 : 0));
	CHECK(IndexOf(P_LIQUID_LETTERS) == IndexOf(P_MERGE) + 1);
	CHECK(kRows[IndexOf(P_LIQUID_LETTERS) - 1].dflt == 0);
#ifdef LG_BRIGHTNESS
	CHECK(IndexOf(P_T_LUMA) == IndexOf(P_E_ANIM) + 1);
	CHECK(IndexOf(P_E_LUMA) == kRowCount);
#else
	CHECK(IndexOf(P_THRESHOLD) == -1 && IndexOf(P_MASK_SOFTNESS) == -1 && IndexOf(P_INVERT) == -1);
#endif

	MaskAndTintCase<PxARGB8>(FMT_ARGB8);
	MaskAndTintCase<PxARGB16>(FMT_ARGB16);
	MaskAndTintCase<PxARGBF>(FMT_ARGBF);
	{
		Settings s = Base(60, 40);
		s.ownLayer = s.clipAlpha = s.textLayer = true;
		s.core[20] = 12;
		Frame<uint8_t> in(60, 40), out(60, 40), back(60, 40);
		Gradient<PxARGB8>(back);
		for (int y = 10; y < 30; y++) for (int x = 10; x < 45; x++)
			if (x < 25 || x >= 30) PixelAt<PxARGB8>(in.world, x, y)[0] = 255;
		Scratch sc;
		CHECK(!MergeParts(s));
		CHECK(RenderFrame(FMT_ARGB8, s, in.world, out.world, nullptr, nullptr, sc, &back.world) == 0);
		CHECK(PixelAt<PxARGB8>(out.world, 27, 20)[0] == 0);
		s.liquidLetters = true;
		CHECK(MergeParts(s));
		CHECK(RenderFrame(FMT_ARGB8, s, in.world, out.world, nullptr, nullptr, sc, &back.world) == 0);
		CHECK(PixelAt<PxARGB8>(out.world, 27, 20)[0] > 0);
		s.textLayer = false; s.liquidLetters = false;
		CHECK(MergeParts(s));
		s.noGlass = true;
		CHECK(RenderFrame(FMT_ARGB8, s, in.world, out.world, nullptr, nullptr, sc, &back.world) == 0);
		CHECK(out.px == in.px);
	}
	{
		// Combined anchor/rotation/nonuniform scale/parent translation, with unequal downsample and buffer origins.
		const double matrix[4][4] = { {0, 2, 0, 0}, {-3, 0, 0, 0}, {0, 0, 1, 0}, {130, 40, 0, 1} };
		Settings s = Base(1, 1);
		BackdropMap(matrix, 0.5, 0.25, s.backdropMap);
		double u = 0, v = 0;
		CHECK(MapBackdrop(s.backdropMap, 5, 5, u, v));
		CHECK(std::fabs(u - 35) < 1e-6 && std::fabs(v - 15) < 1e-6);
		Frame<uint8_t> back(100, 60, -10, -10);
		Gradient<PxARGB8>(back);
		Scratch a, b;
		a.backdrop.resize(4); b.backdrop.resize(4);
		FillMappedBackdrop(back.world, { 5, 5, 6, 6 }, s, a);
		CHECK(MapBackdrop(s.backdropMap, 5.5, 5.5, u, v));
		CHECK(std::abs(int(a.backdrop[0]) - int((u - 0.5 + 10) * 255 / 100)) <= 2);
		CHECK(std::abs(int(a.backdrop[1]) - int((v - 0.5 + 10) * 255 / 60)) <= 2);
		s = Base(1, 1);
		FillMappedBackdrop(back.world, { -3, 17, -2, 18 }, s, a);
		FillBackdrop<PxARGB8>(back.world, { -3, 17, -2, 18 }, b);
		CHECK(a.backdrop == b.backdrop);
		const double layer[4][4] = { {1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1} };
		double camera[4][4] = { {1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {100, 50, -200, 1} };
		CHECK(CameraBackdropMap(layer, camera, 200, 200, 100, 0.5, 0.25, s.backdropMap));
		CHECK(MapBackdrop(s.backdropMap, 5, 5, u, v) && std::fabs(u - 5) < 1e-6 && std::fabs(v - 5) < 1e-6);
		camera[3][2] = -400;
		CHECK(CameraBackdropMap(layer, camera, 200, 200, 100, 1, 1, s.backdropMap));
		CHECK(MapBackdrop(s.backdropMap, 0, 0, u, v) && std::fabs(u - 50) < 1e-6 && std::fabs(v - 25) < 1e-6);
		camera[3][2] = 200;
		CHECK(CameraBackdropMap(layer, camera, 200, 200, 100, 1, 1, s.backdropMap));
		CHECK(!MapBackdrop(s.backdropMap, 5, 5, u, v));
		camera[2][2] = 0;
		CHECK(!CameraBackdropMap(layer, camera, 200, 200, 100, 1, 1, s.backdropMap));
	}

	// Looks keep edge width and merge.
	float core[kCoreCount];
	core[3] = 123;
	core[20] = 45;
	ApplyLook(1, core);
	CHECK(core[3] == 123 && core[20] == 45);
	CHECK(core[2] == 90 && core[5] == 0 && core[6] == 30);
	ApplyLook(0, core);
	CHECK(core[2] == 80 && core[3] == 123);

	// Half resolution halves lengths, not strengths.
	Settings half = Base(100, 100);
	ScaleToBuffer(half, 0.5f);
	CHECK(half.core[3] == 8 && half.core[5] == 1.5f && half.core[14] == 12 && half.core[2] == 80 && half.core[1] == 0.75f);
	CHECK(half.shape[0].w == 25);

	// Light turns toward the direction of travel; a still glass keeps its own angle.
	CHECK(FollowLight(-45, 0, 0, 1) == -45);
	CHECK(std::fabs(FollowLight(-45, 50, 0, 1) - 90) < 0.01f);
	CHECK(std::fabs(FollowLight(-45, 0, -50, 1)) < 0.01f);

	// Shapes: sign and rough edge position.
	Shape sh;
	sh.type = SHAPE_ELLIPSE;
	sh.w = 100;
	sh.h = 50;
	CHECK(ShapeDistance(sh, 1, 0, 0, 0) < 0 && ShapeDistance(sh, 1, 0, 60, 0) > 0 && std::fabs(ShapeDistance(sh, 1, 0, 50, 0)) < 1);
	sh.type = SHAPE_PILL;
	CHECK(ShapeDistance(sh, 1, 0, 49, 24) > 0); // corner rounded away
	sh.type = SHAPE_ROUNDED;
	sh.radius = 0;
	CHECK(ShapeDistance(sh, 1, 0, 49, 24) < 0);

	// Two shapes closer than `merge` grow a bridge; far apart they don't.
	Settings m = Base(400, 200);
	m.second = true;
	m.shape[0].cx = 100;
	m.shape[0].cy = 100;
	m.shape[0].w = m.shape[0].h = 100;
	m.shape[0].type = SHAPE_ELLIPSE;
	m.shape[1] = m.shape[0];
	m.shape[1].cx = 220;
	m.core[20] = 60;
	Scratch sc;
	const Rect all = { 0, 0, 400, 200 };
	BuildMask(m, all, sc);
	CHECK(sc.mask[size_t(100) * 400 + 160] > 128);
	m.core[20] = 0;
	BuildMask(m, all, sc);
	CHECK(sc.mask[size_t(100) * 400 + 160] == 0);

	// Wobble moves the edge over time, and stays inside its bounds.
	Settings wb = Base(400, 200);
	wb.wobble = 12;
	wb.seconds = 0;
	BuildMask(wb, all, sc);
	const std::vector<unsigned char> m0 = sc.mask;
	wb.seconds = 0.4f;
	BuildMask(wb, all, sc);
	CHECK(m0 != sc.mask);

	// Every pixel format.
	const int W = 200, H = 120;
	Frame<uint8_t> in8(W, H), ref(W, H);
	Gradient<PxARGB8>(in8);
	{
		Scratch s8;
		Settings s = Base(W, H);
		CHECK(RenderFrame(FMT_ARGB8, s, in8.world, ref.world, nullptr, nullptr, s8) == 0);
	}
	FormatCase<PxARGB16>(FMT_ARGB16, ref);
	FormatCase<PxARGBF>(FMT_ARGBF, ref);
	FormatCase<PxBGRA8>(FMT_BGRA8, ref);
	FormatCase<PxBGRAF>(FMT_BGRAF, ref);

	// Origins and a bottom-up buffer: the same pixels come out however the host lays the buffers out.
	{
		Frame<uint8_t> inFlip(W, H), outFlip(W, H);
		for (Frame<uint8_t>* f : { &inFlip, &outFlip }) {
			f->world.data += size_t(H - 1) * f->world.rowbytes;
			f->world.rowbytes = -f->world.rowbytes;
		}
		Gradient<PxARGB8>(inFlip);
		Scratch s2;
		Settings s = Base(W, H);
		CHECK(RenderFrame(FMT_ARGB8, s, inFlip.world, outFlip.world, nullptr, nullptr, s2) == 0);
		Frame<uint8_t> inShift(W, H, 500, 300), outShift(W, H, 500, 300);
		Gradient<PxARGB8>(inShift);
		Settings ss = Base(W, H);
		ss.shape[0].cx += 500;
		ss.shape[0].cy += 300;
		CHECK(RenderFrame(FMT_ARGB8, ss, inShift.world, outShift.world, nullptr, nullptr, s2) == 0);
		bool same = true;
		for (int y = 0; y < H; y++) same = same && !memcmp(PixelAt<PxARGB8>(outFlip.world, 0, y), PixelAt<PxARGB8>(outShift.world, 500, 300 + y), size_t(W) * 4);
		CHECK(same);
	}

	// Clip-alpha mode: the glass replaces the clip, transparent stays transparent, and the output is exactly its shape.
	{
		Frame<uint8_t> in(W, H), out(W, H);
		for (int y = 40; y < 80; y++)
			for (int x = 50; x < 150; x++) {
				uint8_t* o = PixelAt<PxARGB8>(in.world, x, y);
				o[0] = 255;
				o[1] = o[2] = o[3] = 200;
			}
		Scratch s3;
		Settings s = Base(W, H);
		s.clipAlpha = true;
		CHECK(RenderFrame(FMT_ARGB8, s, in.world, out.world, nullptr, nullptr, s3) == 0);
		CHECK(PixelAt<PxARGB8>(out.world, 100, 60)[0] == 255);
		CHECK(PixelAt<PxARGB8>(out.world, 5, 5)[0] == 0);
		// A separate shape layer drives the same mask while the input passes through.
		Frame<uint8_t> out2(W, H), bg(W, H);
		Gradient<PxARGB8>(bg);
		Settings sl = Base(W, H);
		CHECK(RenderFrame(FMT_ARGB8, sl, bg.world, out2.world, &in.world, nullptr, s3) == 0);
		CHECK(PixelAt<PxARGB8>(out2.world, 5, 5)[1] == PixelAt<PxARGB8>(bg.world, 5, 5)[1]);
		CHECK(out2.px != bg.px);
		// No shape at all: pure pass-through.
		Frame<uint8_t> out3(W, H), empty(W, H);
		CHECK(RenderFrame(FMT_ARGB8, sl, bg.world, out3.world, &empty.world, nullptr, s3) == 0);
		CHECK(out3.px == bg.px);
	}

	// A shape cut by the frame edge has no false bevel there: its pixels match the same shape with the frame further out.
	{
		Frame<uint8_t> small(W, H), big(W + 200, H), outS(W, H), outB(W + 200, H);
		Gradient<PxARGB8>(big);
		for (int y = 0; y < H; y++) memcpy(PixelAt<PxARGB8>(small.world, 0, y), PixelAt<PxARGB8>(big.world, 0, y), size_t(W) * 4);
		Settings s = Base(W, H);
		s.shape[0].cx = W;
		s.shape[0].w = 200;
		s.core[2] = 0; // lens off: only the edge-distance shading is compared
		s.core[4] = 0;
		Scratch s4;
		CHECK(RenderFrame(FMT_ARGB8, s, small.world, outS.world, nullptr, nullptr, s4) == 0);
		CHECK(RenderFrame(FMT_ARGB8, s, big.world, outB.world, nullptr, nullptr, s4) == 0);
		const uint8_t* a = PixelAt<PxARGB8>(outS.world, W - 1, H / 2), * b = PixelAt<PxARGB8>(outB.world, W - 1, H / 2);
		CHECK(std::abs(a[1] - b[1]) <= 6 && std::abs(a[2] - b[2]) <= 6);
	}

	// 1080p timing, 8-bit, single call on this thread.
	if (!fails) {
		const int FW = 1920, FH = 1080;
		Frame<uint8_t> in(FW, FH), out(FW, FH);
		Gradient<PxARGB8>(in);
		Scratch st;
		struct Case { const char* name; float w, h, frost; bool shadow; } cases[] = {
			{ "default 800x400", 800, 400, 3, false },
			{ "frosted 800x400 + shadow", 800, 400, 24, true },
			{ "full frame", 1900, 1060, 3, false },
			{ "full frame frosted + shadow", 1900, 1060, 24, true },
		};
		for (const Case& c : cases) {
			Settings s = Base(FW, FH);
			s.shape[0].w = c.w;
			s.shape[0].h = c.h;
			s.core[5] = c.frost;
			if (c.shadow) s.core[13] = 0.3f;
			double best = 1e9;
			for (int i = 0; i < 5; i++) {
				const auto t0 = std::chrono::steady_clock::now();
				CHECK(RenderFrame(FMT_ARGB8, s, in.world, out.world, nullptr, nullptr, st) == 0);
				best = std::min(best, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
			}
			printf("1080p %-30s %7.1f ms\n", c.name, best);
		}
	}

	printf(fails ? "%d FAILED\n" : "all passed\n", fails);
	return fails ? 1 : 0;
}
