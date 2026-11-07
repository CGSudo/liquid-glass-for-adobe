// Premiere routing stays SDK-free so both BGRA paths can be checked without opening the host.
#pragma once
#include "GlassHost.h"

template <class P> inline bool PremiereHasOutline(const World& w)
{
	if (!w.data || w.w <= 0 || w.h <= 0) return false;
	bool visible = false, transparent = false;
	for (int y = w.oy; y < w.oy + w.h; y++) {
		const auto* row = PixelAt<P>(w, w.ox, y);
		for (int x = 0; x < w.w; x++) {
			float a = float(row[x * 4 + P::A]) / P::Max;
			visible |= a > 0;
			transparent |= a < 1;
			if (visible && transparent) return true;
		}
	}
	return false;
}

inline int PremiereRowByLabel(const char* label)
{
	for (int i = 0; i < kRowCount; i++)
		if (kRows[i].label && !std::strcmp(kRows[i].label, label)) return i + 1;
	return -1;
}

template <class P> inline void PremiereComposite(const World& output, const World& background)
{
	const Rect r = Clip(output.rect(), background.rect());
	for (int y = r.t; y < r.b; y++) for (int x = r.l; x < r.r; x++) {
		auto* o = PixelAt<P>(output, x, y);
		const auto* b = PixelAt<P>(background, x, y);
		const float a = std::clamp(float(o[P::A]) / P::Max, 0.f, 1.f);
		if (a == 1) continue;
		if (a == 0) { std::memcpy(o, b, 4 * sizeof(typename P::Chan)); continue; }
		const float under = std::clamp(float(b[P::A]) / P::Max, 0.f, 1.f) * (1 - a), total = a + under;
		for (int c : { P::R, P::G, P::B }) o[c] = FromUnit<P>((o[c] * a + b[c] * under) / (P::Max * total));
		o[P::A] = FromUnit<P>(total);
	}
}

inline int RenderPremiereFrame(PixFmt fmt, Settings s, const World& input, const World& output,
	const World* trackAlpha, const World* background, bool fromAlpha, Scratch& scratch)
{
	const World* backdrop = &input;
	const World* alpha = trackAlpha;
	if (!s.noGlass && !alpha && fromAlpha && background && background->data &&
		background->w == input.w && background->h == input.h &&
		background->ox == input.ox && background->oy == input.oy &&
		(fmt == FMT_BGRA8 ? PremiereHasOutline<PxBGRA8>(input) : PremiereHasOutline<PxBGRAF>(input))) {
		backdrop = background;
		alpha = &input;
	}
	// Missing track/background must pass through, never refract the source's colour or synthesize a black frame.
	s.noGlass = s.noGlass || (fromAlpha && !alpha);
	s.clipAlpha = false;
	s.ownLayer = alpha != nullptr;
	const int err = RenderFrame(fmt, s, *backdrop, output, alpha, nullptr, scratch);
	// Registering background transfer support makes this effect responsible for ordinary source-over too.
	if (!err && backdrop == &input && background && background->data) {
		if (fmt == FMT_BGRA8) PremiereComposite<PxBGRA8>(output, *background);
		else if (fmt == FMT_BGRAF) PremiereComposite<PxBGRAF>(output, *background);
	}
	return err;
}
