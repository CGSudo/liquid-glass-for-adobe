// Host logic without After Effects or Premiere: parameter mapping, looks, shapes, pixel formats, plus a 1080p timing.
#include "../Source/GlassHost.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>

static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

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

int main()
{
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
			if (r.kind == K_FLOAT) CHECK(r.dflt == kDefaults[r.core]);
			if (r.kind == K_FLOAT) CHECK(r.dflt >= r.min && r.dflt <= r.max);
			if (r.kind == K_POPUP) CHECK((int)r.dflt == (int)kDefaults[r.core] + 1);
		}
	// Tint amount (12) and the colours are rows; nothing is left without a control.
	for (int i = 0; i < kCoreCount; i++) CHECK(driven[i] == 1);

	// Ids are unique and every topic closes.
	int depth = 0;
	for (int i = 0; i < kRowCount; i++) {
		for (int j = i + 1; j < kRowCount; j++) CHECK(kRows[i].id != kRows[j].id);
		if (kRows[i].kind == K_TOPIC || kRows[i].kind == K_TOPIC_CLOSED) depth++;
		if (kRows[i].kind == K_END) depth--;
		CHECK(depth == 0 || depth == 1);
	}
	CHECK(depth == 0);
	CHECK(IndexOf(P_LOOK) == 1);

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
