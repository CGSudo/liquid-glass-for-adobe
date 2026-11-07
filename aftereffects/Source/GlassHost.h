// Everything of the After Effects / Premiere Pro host that doesn't need the Adobe SDK: the parameter table, the looks,
// the built-in shapes and the frame compositor. Kept SDK-free so test/param_test.cpp can run it without either app.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

extern "C" {
int lg_render(const float* p, const unsigned char* backdrop, const unsigned char* masks, unsigned n_masks, unsigned w,
	unsigned h, unsigned char* out, unsigned char* under);
unsigned lg_param_count();
}

const int kCoreCount = 42; // glass_core::Params

// Order = glass_core::Params. Same numbers as photoshop/looks.js DEFAULTS (aftereffects/params.test.js checks it).
const float kDefaults[kCoreCount] = { -45, 0.75f, 80, 16, 20, 3, 30, 0.06f, 1.2f, 1, 1, 1, 0, 0, 24, 8, 0, 1.5f, 0, 40, 24, 0.32f, 1.2f, 1.5f, 1, 0, 12, 1, 1, 1,
	0, 0, 1, 0.2f, 0, -45, 1, 1, 1, 1, 1, 1 };
// Lengths: authored in full-resolution px, scaled by the host's downsample at render.
const int kLengths[] = { 3, 5, 14, 15, 17, 19, 20, 22, 26, 31 };
// Looks keep the size-dependent settings (looks.js KEEP): edge width and merge.
const int kKeep[] = { 3, 20 };

struct LookValue {
	int i;
	float v;
};
struct Look {
	const char* name;
	LookValue values[20]; // terminated by a zero-filled entry (index 0 is never overridden by a look)
};
const Look kLooks[] = {
	{ "Liquid Glass", {} },
	{ "Clear", { {1, 0.7f}, {2, 90}, {4, 25}, {5, 0}, {7, -0.08f}, {8, 1.05f}, {13, 0.15f} } },
	{ "Frosted", { {1, 0.5f}, {2, 30}, {4, 5}, {5, 24}, {7, 0.18f}, {8, 1.05f}, {25, 0.25f}, {26, 10} } },
	{ "Glassmorphism", { {1, 0.4f}, {2, 10}, {4, 0}, {5, 20}, {6, 60}, {7, 0.25f}, {8, 1.3f}, {13, 0.18f}, {14, 30}, {15, 12}, {21, 0.1f}, {22, 1},
		{31, 1}, {32, 0.7f}, {33, 0.1f}, {34, 1}, {35, -45} } },
	{ "Ice", { {4, 30}, {5, 16}, {7, 0.12f}, {9, 0.75f}, {10, 0.9f}, {11, 1}, {12, 0.35f}, {16, 0.12f}, {17, 1}, {25, 0.4f}, {26, 14}, {27, 0.8f}, {28, 0.95f}, {29, 1}, {30, 1} } },
	{ "Smoked", { {1, 0.7f}, {5, 18}, {7, -0.45f}, {8, 0.9f}, {9, 0.1f}, {10, 0.1f}, {11, 0.12f}, {12, 0.3f}, {13, 0.35f} } },
	{ "Crystal", { {1, 0.8f}, {2, 140}, {4, 60}, {5, 0}, {6, 15}, {21, 0.45f}, {22, 0.8f}, {23, 1.9f}, {24, 1.4f} } },
	{ "Bubble", { {1, 0.7f}, {2, 110}, {4, 80}, {5, 0}, {6, 100}, {7, 0.02f}, {21, 0.15f}, {24, 1.8f}, {25, 0.25f}, {26, 20}, {27, 0.85f}, {28, 0.9f}, {29, 1}, {30, 1} } },
	{ "Film frost", { {2, 40}, {5, 20}, {7, 0.1f}, {16, 0.3f}, {17, 1.2f} } },
};
const int kLookCount = sizeof kLooks / sizeof kLooks[0];
// Popup value 1 = "Choose a look…": the popup is an action, it snaps back so the same look can be picked again.
const char kLookItems[] = "Choose a look...|Liquid Glass|Clear|Frosted|Glassmorphism|Ice|Smoked|Crystal|Bubble|Film frost";

// Sets `core` to look `n`, leaving the size-dependent values as they are.
inline void ApplyLook(int n, float* core)
{
	float next[kCoreCount];
	std::copy(kDefaults, kDefaults + kCoreCount, next);
	for (const LookValue& o : kLooks[n].values)
		if (o.i) next[o.i] = o.v;
	for (int k : kKeep) next[k] = core[k];
	std::copy(next, next + kCoreCount, core);
}

// ---- Effect parameters. Param ids are written to saved projects: append new ones, never renumber. ----

enum ParamId {
	P_LOOK = 1,
	P_T_SHAPE, P_TYPE, P_CENTER, P_WIDTH, P_HEIGHT, P_RADIUS, P_ROTATION, P_SHAPE_LAYER, P_CLIP_ALPHA, P_BACK_LAYER, P_E_SHAPE,
	P_T_SHAPE2, P_SECOND, P_TYPE2, P_CENTER2, P_WIDTH2, P_HEIGHT2, P_RADIUS2, P_ROTATION2, P_E_SHAPE2,
	P_T_QUICK, P_FROST, P_LENS, P_EDGE, P_SHINE, P_E_QUICK,
	P_T_LENS, P_SOFT, P_DISP, P_MERGE, P_E_LENS,
	P_T_LIGHT, P_ANGLE, P_FOLLOW, P_E_LIGHT,
	P_T_COLOUR, P_BRIGHT, P_VIVID, P_TINT, P_TINT_AMT, P_E_COLOUR,
	P_T_SHADOW, P_SH_DARK, P_SH_SOFT, P_SH_DROP, P_E_SHADOW,
	P_T_DIM, P_DIM, P_DIM_SPREAD, P_E_DIM,
	P_T_GLOW, P_GLOW, P_GLOW_SIZE, P_GLOW_COLOR, P_GLOW_BLEND, P_E_GLOW,
	P_T_STROKE, P_ST_WIDTH, P_ST_FILL, P_ST_COLOR, P_ST_OPACITY, P_ST_COLOR2, P_ST_OPACITY2, P_ST_ANGLE, P_E_STROKE,
	P_T_GRAIN, P_GRAIN, P_GRAIN_SIZE, P_E_GRAIN,
	P_T_ADV, P_EMBOSS, P_RIM, P_IOR, P_SHEEN, P_E_ADV,
	P_T_ANIM, P_WOBBLE, P_WOBBLE_SPEED, P_E_ANIM,
	P_LIQUID_LETTERS,
	P_T_LUMA, P_THRESHOLD, P_MASK_SOFTNESS, P_INVERT, P_E_LUMA,
};

enum Kind { K_TOPIC, K_TOPIC_CLOSED, K_END, K_FLOAT, K_ANGLE, K_COLOR, K_POPUP, K_CHECK, K_POINT, K_LAYER, K_LOOK };

struct Row {
	int id;
	Kind kind;
	int core; // glass_core::Params index (colours: the first of three), -1 = host only
	const char* label;
	float min, max, dflt; // K_POINT: dflt/max = default x/y in percent of the layer
	int prec;
	const char* items;
};

const char kShapeItems[] = "Rounded rectangle|Ellipse|Pill";

// Same labels and ranges as the Photoshop panel (photoshop/index.js SECTIONS); params.test.js compares them.
// Effect-control order. The index a host uses is the row's position + 1 (param 0 is the layer itself).
const Row kRows[] = {
	{ P_LOOK, K_LOOK, -1, "Look", 0, 0, 0, 0, kLookItems },

	{ P_T_SHAPE, K_TOPIC, -1, "Glass shape", 0, 0, 0, 0, nullptr },
	{ P_TYPE, K_POPUP, -1, "Shape", 0, 0, 1, 0, kShapeItems },
	{ P_CENTER, K_POINT, -1, "Center", 0, 50, 50, 0, nullptr },
	{ P_WIDTH, K_FLOAT, -1, "Width", 0, 8000, 800, 0, nullptr },
	{ P_HEIGHT, K_FLOAT, -1, "Height", 0, 8000, 400, 0, nullptr },
	{ P_RADIUS, K_FLOAT, -1, "Corner radius", 0, 2000, 80, 0, nullptr },
	{ P_ROTATION, K_ANGLE, -1, "Rotation", 0, 0, 0, 0, nullptr },
	{ P_SHAPE_LAYER, K_LAYER, -1, "Shape from layer", 0, 0, 0, 0, nullptr },
	{ P_CLIP_ALPHA, K_CHECK, -1, "Use clip alpha as shape", 0, 0, 0, 0, nullptr },
	{ P_BACK_LAYER, K_LAYER, -1, "Backdrop layer", 0, 0, 0, 0, nullptr },
	{ P_E_SHAPE, K_END, -1, nullptr, 0, 0, 0, 0, nullptr },

	{ P_T_SHAPE2, K_TOPIC_CLOSED, -1, "Second shape (liquid merge)", 0, 0, 0, 0, nullptr },
	{ P_SECOND, K_CHECK, -1, "Add second shape", 0, 0, 0, 0, nullptr },
	{ P_TYPE2, K_POPUP, -1, "Shape", 0, 0, 2, 0, kShapeItems },
	{ P_CENTER2, K_POINT, -1, "Center", 0, 50, 75, 0, nullptr },
	{ P_WIDTH2, K_FLOAT, -1, "Width", 0, 8000, 300, 0, nullptr },
	{ P_HEIGHT2, K_FLOAT, -1, "Height", 0, 8000, 300, 0, nullptr },
	{ P_RADIUS2, K_FLOAT, -1, "Corner radius", 0, 2000, 80, 0, nullptr },
	{ P_ROTATION2, K_ANGLE, -1, "Rotation", 0, 0, 0, 0, nullptr },
	{ P_E_SHAPE2, K_END, -1, nullptr, 0, 0, 0, 0, nullptr },

	{ P_T_QUICK, K_TOPIC, -1, "Quick adjust", 0, 0, 0, 0, nullptr },
	{ P_FROST, K_FLOAT, 5, "Frost", 0, 200, 3, 0, nullptr },
	{ P_LENS, K_FLOAT, 2, "Lens", 0, 200, 80, 0, nullptr },
	{ P_EDGE, K_FLOAT, 3, "Edge width", 1, 300, 30, 0, nullptr }, // starts stronger than the panel (params.test.js): on a video frame 16 barely shows
	{ P_SHINE, K_FLOAT, 1, "Shine", 0, 1, 0.75f, 2, nullptr },
	{ P_E_QUICK, K_END, -1, nullptr, 0, 0, 0, 0, nullptr },

	{ P_T_LENS, K_TOPIC, -1, "Lens", 0, 0, 0, 0, nullptr },
	{ P_SOFT, K_FLOAT, 6, "Edge softness", 0, 100, 30, 0, nullptr },
	{ P_DISP, K_FLOAT, 4, "Dispersion", 0, 100, 20, 0, nullptr },
	{ P_MERGE, K_FLOAT, 20, "Merge", 0, 200, 24, 0, nullptr },
	{ P_LIQUID_LETTERS, K_CHECK, -1, "Liquid letters", 0, 1, 0, 0, nullptr },
	{ P_E_LENS, K_END, -1, nullptr, 0, 0, 0, 0, nullptr },

	{ P_T_LIGHT, K_TOPIC, -1, "Light", 0, 0, 0, 0, nullptr },
	{ P_ANGLE, K_ANGLE, 0, "Direction", 0, 0, -45, 0, nullptr },
	{ P_FOLLOW, K_CHECK, -1, "Light follows motion", 0, 0, 0, 0, nullptr },
	{ P_E_LIGHT, K_END, -1, nullptr, 0, 0, 0, 0, nullptr },

	{ P_T_COLOUR, K_TOPIC, -1, "Colour", 0, 0, 0, 0, nullptr },
	{ P_BRIGHT, K_FLOAT, 7, "Brightness", -1, 1, 0.06f, 2, nullptr },
	{ P_VIVID, K_FLOAT, 8, "Vividness", 0, 2, 1.2f, 2, nullptr },
	{ P_TINT, K_COLOR, 9, "Tint colour", 0, 0, 0, 0, nullptr },
	{ P_TINT_AMT, K_FLOAT, 12, "Tint amount", 0, 1, 0, 2, nullptr },
	{ P_E_COLOUR, K_END, -1, nullptr, 0, 0, 0, 0, nullptr },

	{ P_T_SHADOW, K_TOPIC, -1, "Drop shadow", 0, 0, 0, 0, nullptr },
	{ P_SH_DARK, K_FLOAT, 13, "Darkness", 0, 1, 0.25f, 2, nullptr }, // likewise
	{ P_SH_SOFT, K_FLOAT, 14, "Softness", 0, 100, 24, 0, nullptr },
	{ P_SH_DROP, K_FLOAT, 15, "Drop", -50, 50, 8, 0, nullptr },
	{ P_E_SHADOW, K_END, -1, nullptr, 0, 0, 0, 0, nullptr },

	{ P_T_DIM, K_TOPIC, -1, "Dim background", 0, 0, 0, 0, nullptr },
	{ P_DIM, K_FLOAT, 18, "Amount", 0, 1, 0, 2, nullptr },
	{ P_DIM_SPREAD, K_FLOAT, 19, "Spread", 0, 200, 40, 0, nullptr },
	{ P_E_DIM, K_END, -1, nullptr, 0, 0, 0, 0, nullptr },

	{ P_T_GLOW, K_TOPIC, -1, "Inner glow", 0, 0, 0, 0, nullptr },
	{ P_GLOW, K_FLOAT, 25, "Amount", 0, 1, 0, 2, nullptr },
	{ P_GLOW_SIZE, K_FLOAT, 26, "Size", 1, 100, 12, 0, nullptr },
	{ P_GLOW_COLOR, K_COLOR, 27, "Glow colour", 0, 0, 0, 0, nullptr },
	{ P_GLOW_BLEND, K_POPUP, 30, "Blend mode", 0, 0, 1, 0, "Normal|Screen|Overlay|Soft light|Add|Multiply" },
	{ P_E_GLOW, K_END, -1, nullptr, 0, 0, 0, 0, nullptr },

	{ P_T_STROKE, K_TOPIC, -1, "Stroke", 0, 0, 0, 0, nullptr },
	{ P_ST_WIDTH, K_FLOAT, 31, "Width", 0, 50, 0, 1, nullptr },
	{ P_ST_FILL, K_POPUP, 34, "Fill", 0, 0, 1, 0, "Solid colour|Gradient" },
	{ P_ST_COLOR, K_COLOR, 36, "Colour", 0, 0, 0, 0, nullptr },
	{ P_ST_OPACITY, K_FLOAT, 32, "Opacity", 0, 1, 1, 2, nullptr },
	{ P_ST_COLOR2, K_COLOR, 39, "End colour", 0, 0, 0, 0, nullptr },
	{ P_ST_OPACITY2, K_FLOAT, 33, "End opacity", 0, 1, 0.2f, 2, nullptr },
	{ P_ST_ANGLE, K_ANGLE, 35, "Angle", 0, 0, -45, 0, nullptr },
	{ P_E_STROKE, K_END, -1, nullptr, 0, 0, 0, 0, nullptr },

	{ P_T_GRAIN, K_TOPIC, -1, "Film grain", 0, 0, 0, 0, nullptr },
	{ P_GRAIN, K_FLOAT, 16, "Amount", 0, 1, 0, 2, nullptr },
	{ P_GRAIN_SIZE, K_FLOAT, 17, "Size", 0.5f, 12, 1.5f, 1, nullptr },
	{ P_E_GRAIN, K_END, -1, nullptr, 0, 0, 0, 0, nullptr },

	{ P_T_ADV, K_TOPIC_CLOSED, -1, "Advanced", 0, 0, 0, 0, nullptr },
	{ P_EMBOSS, K_FLOAT, 21, "Emboss", 0, 1, 0.32f, 2, nullptr },
	{ P_RIM, K_FLOAT, 22, "Rim line", 0.2f, 8, 1.2f, 1, nullptr },
	{ P_IOR, K_FLOAT, 23, "Glass type", 1, 2.4f, 1.5f, 2, nullptr },
	{ P_SHEEN, K_FLOAT, 24, "Sheen", 0, 2, 1, 2, nullptr },
	{ P_E_ADV, K_END, -1, nullptr, 0, 0, 0, 0, nullptr },

	{ P_T_ANIM, K_TOPIC, -1, "Animation", 0, 0, 0, 0, nullptr },
	{ P_WOBBLE, K_FLOAT, -1, "Wobble", 0, 100, 0, 1, nullptr },
	{ P_WOBBLE_SPEED, K_FLOAT, -1, "Wobble speed", 0, 10, 1, 2, nullptr },
	{ P_E_ANIM, K_END, -1, nullptr, 0, 0, 0, 0, nullptr },
#ifdef LG_BRIGHTNESS
	{ P_T_LUMA, K_TOPIC, -1, "Brightness shape", 0, 0, 0, 0, nullptr },
	{ P_THRESHOLD, K_FLOAT, -1, "Threshold", 0, 1, 0.5f, 2, nullptr },
	{ P_MASK_SOFTNESS, K_FLOAT, -1, "Softness", 0, 1, 1, 2, nullptr },
	{ P_INVERT, K_CHECK, -1, "Invert", 0, 1, 0, 0, nullptr },
	{ P_E_LUMA, K_END, -1, nullptr, 0, 0, 0, 0, nullptr },
#endif
};
const int kRowCount = sizeof kRows / sizeof kRows[0];

// Host param index of a ParamId (position in kRows + 1).
inline int IndexOf(int id)
{
	for (int i = 0; i < kRowCount; i++)
		if (kRows[i].id == id) return i + 1;
	return -1;
}

// ---- Settings: one frame's values, in pixels of the buffer being rendered ----

enum ShapeType { SHAPE_ROUNDED = 1, SHAPE_ELLIPSE, SHAPE_PILL };

struct Shape {
	int type = SHAPE_ROUNDED;
	float cx = 0, cy = 0, w = 0, h = 0, radius = 0, rotation = 0;
};

struct Settings {
	float core[kCoreCount];
	Shape shape[2];
	bool second = false, clipAlpha = false, follow = false;
	bool noGlass = false; // the input passes through untouched
	bool ownLayer = false; // the user's layer supplies the outline, not the backdrop
	bool textLayer = false, liquidLetters = false, brightnessMask = false, invertMask = false;
	float threshold = 0.5f, maskSoftness = 1;
	// Row-vector homography from render pixels to backdrop pixels; identity for already transformed inputs.
	double backdropMap[3][3] = { {1, 0, 0}, {0, 1, 0}, {0, 0, 1} };
	float wobble = 0, wobbleSpeed = 1, seconds = 0;
};

inline bool MergeParts(const Settings& s) { return s.ownLayer && (!s.textLayer || s.liquidLetters); }

// SDK matrices use row vectors; the translation is in the last row, and includes the parent chain.
inline void BackdropMap(const double layer[4][4], double dx, double dy, double map[3][3])
{
	const int axis[] = { 0, 1, 3 };
	const double scale[] = { dx, dy, 1 };
	for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++)
		map[i][j] = layer[axis[i]][axis[j]] * scale[j] / scale[i];
}

// Invert the camera pose before projecting the layer plane, including parented cameras.
inline bool CameraBackdropMap(const double layer[4][4], const double camera[4][4], double zoom, double width, double height,
	double dx, double dy, double map[3][3])
{
	double a[4][8] = {};
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) a[i][j] = camera[i][j];
		a[i][i + 4] = 1;
	}
	for (int c = 0; c < 4; c++) {
		int pivot = c;
		for (int r = c + 1; r < 4; r++) if (std::fabs(a[r][c]) > std::fabs(a[pivot][c])) pivot = r;
		if (!std::isfinite(a[pivot][c]) || std::fabs(a[pivot][c]) < 1e-12) return false;
		for (int j = 0; j < 8; j++) std::swap(a[c][j], a[pivot][j]);
		const double divisor = a[c][c];
		for (double& v : a[c]) v /= divisor;
		for (int r = 0; r < 4; r++) if (r != c) {
			const double factor = a[r][c];
			for (int j = 0; j < 8; j++) a[r][j] -= factor * a[c][j];
		}
	}
	double projected[4][4] = {};
	for (int i = 0; i < 4; i++) {
		double view[3] = {};
		for (int j = 0; j < 3; j++) for (int k = 0; k < 4; k++) view[j] += layer[i][k] * a[k][j + 4];
		projected[i][0] = view[0] * zoom + view[2] * width * 0.5;
		projected[i][1] = view[1] * zoom + view[2] * height * 0.5;
		projected[i][3] = view[2];
	}
	BackdropMap(projected, dx, dy, map);
	return true;
}

inline bool MapBackdrop(const double map[3][3], double x, double y, double& u, double& v)
{
	const double z = x * map[0][2] + y * map[1][2] + map[2][2];
	if (!std::isfinite(z) || z <= 1e-9) return false;
	u = (x * map[0][0] + y * map[1][0] + map[2][0]) / z;
	v = (x * map[0][1] + y * map[1][1] + map[2][1]) / z;
	return std::isfinite(u) && std::isfinite(v);
}

// Full-resolution lengths → this render's pixels. Centres arrive already scaled from the host.
inline void ScaleToBuffer(Settings& s, float ds)
{
	for (int k : kLengths) s.core[k] *= ds;
	for (Shape& sh : s.shape) {
		sh.w *= ds;
		sh.h *= ds;
		sh.radius *= ds;
	}
	s.wobble *= ds;
}

// Light angle turned toward the direction of travel; (dx, dy) = px moved this frame. Eases in with speed, so the
// light doesn't snap when the glass starts or stops.
inline float FollowLight(float angle, float dx, float dy, float ds)
{
	const float rad = 3.14159265f / 180;
	float speed = std::sqrt(dx * dx + dy * dy);
	if (speed < 1e-3f) return angle;
	float t = std::min(1.f, speed / (4 * std::max(ds, 0.01f)));
	float lx = std::sin(angle * rad) * (1 - t) + dx / speed * t, ly = -std::cos(angle * rad) * (1 - t) + dy / speed * t;
	return std::atan2(lx, -ly) / rad;
}

struct Rect {
	int l = 0, t = 0, r = 0, b = 0; // half-open
	bool empty() const { return r <= l || b <= t; }
	int w() const { return r - l; }
	int h() const { return b - t; }
};
inline Rect Grow(Rect a, int m) { return { a.l - m, a.t - m, a.r + m, a.b + m }; }
inline Rect Clip(Rect a, Rect b) { return { std::max(a.l, b.l), std::max(a.t, b.t), std::min(a.r, b.r), std::min(a.b, b.b) }; }
inline Rect Join(Rect a, Rect b)
{
	if (a.empty()) return b;
	if (b.empty()) return a;
	return { std::min(a.l, b.l), std::min(a.t, b.t), std::max(a.r, b.r), std::max(a.b, b.b) };
}

// How far outside the shape the engine reads or writes: lens reach (glass_core::lens_reach; ×2 as in Illustrator), shadow,
// dim halo and frost, in this render's pixels. Lens capped because the engine clamps samples anyway.
inline int Margin(const float* p, float ds)
{
	float depth = std::max(p[3], 1.f), base = std::min(depth, 24 * ds) + std::max(0.f, depth - 24 * ds) * 0.25f;
	float lens = std::min(150 * ds, base * std::max(p[2], 0.f) / 100 * 2);
	float shadow = p[13] > 0 ? p[14] * 1.5f + std::fabs(p[15]) : 0, dim = p[18] > 0 ? p[19] * 1.5f : 0;
	return (int)std::ceil(std::max({ depth, lens, shadow, dim }) + p[5] * 2) + 4;
}

// Settings already contain buffer-scaled lengths in both pre-render and render.
inline int RenderMargin(const Settings& s)
{
	return Margin(s.core, 1) + (MergeParts(s) ? (int)std::ceil(s.core[20] * 0.5f) : 0);
}

inline Rect ShapeBounds(const Shape& s, float grow)
{
	if (s.w <= 0 || s.h <= 0) return {};
	const float a = s.rotation * 3.14159265f / 180, c = std::fabs(std::cos(a)), sn = std::fabs(std::sin(a));
	float ex = s.w * 0.5f * c + s.h * 0.5f * sn + grow + 1, ey = s.w * 0.5f * sn + s.h * 0.5f * c + grow + 1;
	return { (int)std::floor(s.cx - ex), (int)std::floor(s.cy - ey), (int)std::ceil(s.cx + ex), (int)std::ceil(s.cy + ey) };
}

// Bounds of the built-in shape(s), including the bridge a smooth union can grow and the wobble.
inline Rect BuiltInBounds(const Settings& s)
{
	float merge = s.second ? std::max(s.core[20], 0.f) * 0.25f : 0;
	Rect b = ShapeBounds(s.shape[0], merge + s.wobble);
	return s.second ? Join(b, ShapeBounds(s.shape[1], merge + s.wobble)) : b;
}

// Signed distance to a shape (negative inside). `c`, `sn`: cos/sin of its rotation.
inline float ShapeDistance(const Shape& s, float c, float sn, float x, float y)
{
	const float dx = x - s.cx, dy = y - s.cy, px = dx * c + dy * sn, py = -dx * sn + dy * c, hw = s.w * 0.5f, hh = s.h * 0.5f;
	if (hw <= 0 || hh <= 0) return 1e9f;
	if (s.type == SHAPE_ELLIPSE) {
		float k0 = std::sqrt(px * px / (hw * hw) + py * py / (hh * hh));
		float k1 = std::sqrt(px * px / (hw * hw * hw * hw) + py * py / (hh * hh * hh * hh));
		return k1 < 1e-6f ? -std::min(hw, hh) : k0 * (k0 - 1) / k1;
	}
	const float r = s.type == SHAPE_PILL ? std::min(hw, hh) : std::min(std::max(s.radius, 0.f), std::min(hw, hh));
	const float qx = std::fabs(px) - hw + r, qy = std::fabs(py) - hh + r, ox = std::max(qx, 0.f), oy = std::max(qy, 0.f);
	return std::sqrt(ox * ox + oy * oy) + std::min(std::max(qx, qy), 0.f) - r;
}

// Scratch of one render thread; reused between frames so a sequence doesn't reallocate.
struct Scratch {
	std::vector<unsigned char> backdrop, mask, glass;
	std::vector<float> warpX, warpY, dist, edtZ, edtD;
	std::vector<int> edtV;
};

// Anti-aliased coverage of the built-in shape(s) over `roi`. Two shapes join with the same polynomial smooth minimum as
// glass_core::smooth_union, analytically: no distance transforms, so a merge costs almost nothing per frame.
inline void BuildMask(const Settings& s, const Rect& roi, Scratch& sc)
{
	const int w = roi.w(), h = roi.h();
	sc.mask.assign(size_t(w) * h, 0);
	const float rad = 3.14159265f / 180, k = std::max(s.core[20], 0.f);
	const float c0 = std::cos(s.shape[0].rotation * rad), s0 = std::sin(s.shape[0].rotation * rad);
	const float c1 = std::cos(s.shape[1].rotation * rad), s1 = std::sin(s.shape[1].rotation * rad);
	const Rect b = Clip(BuiltInBounds(s), roi);
	const bool wob = s.wobble > 0;
	if (wob) { // separable, so the sines come from two small tables instead of two calls per pixel
		const float f = 2 * 3.14159265f * 1.5f / std::max(1.f, std::max(s.shape[0].w, s.shape[0].h));
		const float ph = s.seconds * s.wobbleSpeed * 2 * 3.14159265f;
		sc.warpX.resize(w);
		sc.warpY.resize(h);
		for (int x = 0; x < w; x++) sc.warpX[x] = s.wobble * std::sin((x + roi.l) * f * 0.9f + ph * 1.3f + 1.7f);
		for (int y = 0; y < h; y++) sc.warpY[y] = s.wobble * std::sin((y + roi.t) * f + ph);
	}
	for (int y = b.t; y < b.b; y++) {
		unsigned char* row = sc.mask.data() + size_t(y - roi.t) * w - roi.l;
		for (int x = b.l; x < b.r; x++) {
			const float fx = x + 0.5f + (wob ? sc.warpY[y - roi.t] : 0), fy = y + 0.5f + (wob ? sc.warpX[x - roi.l] : 0);
			float d = ShapeDistance(s.shape[0], c0, s0, fx, fy);
			if (s.second) {
				const float d2 = ShapeDistance(s.shape[1], c1, s1, fx, fy);
				if (k > 0) {
					const float hh = std::max(k - std::fabs(d - d2), 0.f) / k;
					d = std::min(d, d2) - hh * hh * k * 0.25f;
				} else {
					d = std::min(d, d2);
				}
			}
			row[x] = (unsigned char)(std::min(std::max(0.5f - d, 0.f), 1.f) * 255 + 0.5f);
		}
	}
}

// ---- Pixels ----

struct World {
	unsigned char* data = nullptr;
	int rowbytes = 0, w = 0, h = 0, ox = 0, oy = 0; // ox, oy: buffer's top-left in layer coordinates
	Rect rect() const { return { ox, oy, ox + w, oy + h }; }
};

enum PixFmt { FMT_ARGB8, FMT_ARGB16, FMT_ARGBF, FMT_BGRA8, FMT_BGRAF };

// Channel order and white level: After Effects hands out ARGB (16-bit white = 32768), Premiere BGRA. Both straight alpha.
template <class T, int MAXV, int A_, int R_, int G_, int B_> struct Px {
	using Chan = T;
	static constexpr float Max = (float)MAXV;
	static constexpr int A = A_, R = R_, G = G_, B = B_;
};
using PxARGB8 = Px<uint8_t, 255, 0, 1, 2, 3>;
using PxARGB16 = Px<uint16_t, 32768, 0, 1, 2, 3>;
using PxARGBF = Px<float, 1, 0, 1, 2, 3>;
using PxBGRA8 = Px<uint8_t, 255, 3, 2, 1, 0>;
using PxBGRAF = Px<float, 1, 3, 2, 1, 0>;

template <class P> inline typename P::Chan* PixelAt(const World& w, int x, int y) // x, y in layer coordinates
{
	return reinterpret_cast<typename P::Chan*>(w.data + std::ptrdiff_t(y - w.oy) * w.rowbytes) + size_t(x - w.ox) * 4;
}

inline unsigned char ToByte(float v) { return (unsigned char)(std::min(std::max(v, 0.f), 1.f) * 255 + 0.5f); }

template <class P> inline unsigned char LayerMask(const typename P::Chan* px, const Settings& s)
{
	const float alpha = std::clamp(float(px[P::A]) / P::Max, 0.f, 1.f);
	if (!std::isfinite(alpha)) return 0;
	if (!s.brightnessMask) return ToByte(alpha);
	float luma = (0.2126f * px[P::R] + 0.7152f * px[P::G] + 0.0722f * px[P::B]) / P::Max;
	if (!std::isfinite(luma) || !std::isfinite(alpha)) return 0;
	luma = std::clamp(luma, 0.f, 1.f);
	const float coverage = s.maskSoftness > 0 ? std::clamp((luma - s.threshold) / s.maskSoftness + 0.5f, 0.f, 1.f) : (luma >= s.threshold ? 1.f : 0.f);
	return ToByte((s.invertMask ? 1.f - coverage : coverage) * alpha);
}

template <class P> inline typename P::Chan FromUnit(float v)
{
	if (P::Max == 1.f) return (typename P::Chan)v; // float keeps its range
	return (typename P::Chan)(std::min(std::max(v, 0.f), 1.f) * P::Max + 0.5f);
}

// Alpha plane of `src` over `roi` into the mask; returns the tight bounds of its non-zero pixels when asked.
template <class P> Rect AlphaBounds(const World& src)
{
	Rect b;
	b.l = src.ox + src.w;
	b.t = src.oy + src.h;
	b.r = src.ox;
	b.b = src.oy;
	for (int y = src.oy; y < src.oy + src.h; y++) {
		const typename P::Chan* row = PixelAt<P>(src, src.ox, y);
		for (int x = 0; x < src.w; x++)
			if (row[x * 4 + P::A] > 0) {
				b.l = std::min(b.l, src.ox + x);
				b.r = std::max(b.r, src.ox + x + 1);
				b.t = std::min(b.t, y);
				b.b = std::max(b.b, y + 1);
			}
	}
	return b;
}

// Backdrop as RGBA8 over `roi`, premultiplied so transparent areas read as black, edge-clamped past its bounds.
template <class P> void FillBackdrop(const World& back, const Rect& roi, Scratch& sc)
{
	using T = typename P::Chan;
	const int w = roi.w();
	const Rect br = back.rect();
	if (br.empty()) {
		std::fill(sc.backdrop.begin(), sc.backdrop.end(), (unsigned char)0);
		return;
	}
	for (int y = roi.t; y < roi.b; y++) {
		const T* row = PixelAt<P>(back, br.l, std::min(std::max(y, br.t), br.b - 1));
		unsigned char* d = sc.backdrop.data() + size_t(y - roi.t) * w * 4;
		for (int x = roi.l; x < roi.r; x++, d += 4) {
			const T* px = row + size_t(std::min(std::max(x, br.l), br.r - 1) - br.l) * 4;
			if (sizeof(T) == 1 && (float)px[P::A] == P::Max) {
				d[0] = (unsigned char)px[P::R];
				d[1] = (unsigned char)px[P::G];
				d[2] = (unsigned char)px[P::B];
			} else {
				const float k = (float)px[P::A] / (P::Max * P::Max);
				d[0] = ToByte(px[P::R] * k);
				d[1] = ToByte(px[P::G] * k);
				d[2] = ToByte(px[P::B] * k);
			}
			d[3] = 255;
		}
	}
}

// Pull comp pixels into layer space; AE applies the layer transform to our output after the effect.
inline void FillMappedBackdrop(const World& back, const Rect& roi, const Settings& s, Scratch& sc)
{
	const Rect br = back.rect();
	for (int y = roi.t; y < roi.b; y++) for (int x = roi.l; x < roi.r; x++) {
		unsigned char* d = sc.backdrop.data() + (size_t(y - roi.t) * roi.w() + x - roi.l) * 4;
		double u = 0, v = 0;
		if (!back.data || br.empty() || !MapBackdrop(s.backdropMap, x + 0.5, y + 0.5, u, v)) {
			d[0] = d[1] = d[2] = 0; d[3] = 255;
			continue;
		}
		u = std::clamp(u - 0.5, double(br.l), double(br.r - 1));
		v = std::clamp(v - 0.5, double(br.t), double(br.b - 1));
		const int ix = int(std::floor(u)), iy = int(std::floor(v));
		const float fx = float(u - ix), fy = float(v - iy);
		float rgb[3] = {};
		for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++) {
			const auto* p = PixelAt<PxARGB8>(back, std::min(ix + i, br.r - 1), std::min(iy + j, br.b - 1));
			const float weight = (i ? fx : 1 - fx) * (j ? fy : 1 - fy) * p[0] / (255.f * 255.f);
			for (int c = 0; c < 3; c++) rgb[c] += p[c + 1] * weight;
		}
		for (int c = 0; c < 3; c++) d[c] = ToByte(rgb[c]);
		d[3] = 255;
	}
}

// Squared distance transform in place (Felzenszwalb and Huttenlocher), rows then columns.
inline void SquaredDistance(std::vector<float>& g, int w, int h, Scratch& sc)
{
	const int longest = std::max(w, h);
	sc.edtD.resize(longest);
	sc.edtV.resize(longest);
	sc.edtZ.resize(longest + 1);
	const auto line = [&](float* f, int n, int step) {
		int* v = sc.edtV.data();
		float* z = sc.edtZ.data(), * d = sc.edtD.data();
		int k = 0;
		v[0] = 0;
		z[0] = -1e20f;
		z[1] = 1e20f;
		for (int q = 1; q < n; q++) {
			const float fq = f[q * step] + float(q) * q;
			float s;
			for (;;) {
				const int p = v[k];
				s = (fq - f[p * step] - float(p) * p) / (2.f * (q - p));
				if (s > z[k] || k == 0) break;
				k--;
			}
			k++;
			v[k] = q;
			z[k] = s;
			z[k + 1] = 1e20f;
		}
		k = 0;
		for (int q = 0; q < n; q++) {
			while (z[k + 1] < q) k++;
			const int p = v[k];
			d[q] = f[p * step] + float(q - p) * (q - p);
		}
		for (int q = 0; q < n; q++) f[q * step] = d[q];
	};
	for (int y = 0; y < h; y++) line(g.data() + size_t(y) * w, w, 1);
	for (int x = 0; x < w; x++) line(g.data() + x, h, w);
}

// Separate parts of one layer fuse into one liquid body: closing by r px (grow, then shrink back), so parts within 2r grow a
// neck and inner corners round off, however many parts there are. Uses the edge pixels' coverage for sub-pixel distance.
inline void FuseParts(unsigned char* mask, int w, int h, float r, Scratch& sc)
{
	if (r < 0.5f) return;
	const size_t n = size_t(w) * h;
	const float far_ = 1e20f;
	sc.dist.resize(n);
	for (size_t i = 0; i < n; i++) { // distance to the shape
		const float a = mask[i] / 255.f, e = std::max(0.f, 0.5f - a);
		sc.dist[i] = mask[i] == 255 ? 0.f : mask[i] == 0 ? far_ : e * e;
	}
	SquaredDistance(sc.dist, w, h, sc);
	for (size_t i = 0; i < n; i++) { // grown coverage, then distance to outside it
		const float a = std::min(std::max(r - std::sqrt(sc.dist[i]) + 0.5f, 0.f), 1.f), e = std::max(0.f, a - 0.5f);
		sc.dist[i] = a >= 1.f ? far_ : a <= 0.f ? 0.f : e * e;
	}
	SquaredDistance(sc.dist, w, h, sc);
	for (size_t i = 0; i < n; i++) { // shrunk back; the original edge stays exactly as drawn
		const unsigned char c = ToByte(std::sqrt(sc.dist[i]) - r + 0.5f);
		if (c > mask[i]) mask[i] = c;
	}
}

// The whole frame. `in` passes through untouched outside the glass at its full bit depth; only the shape's bounds plus
// `Margin` go through the 8-bit engine. shapeLayer: alpha source instead of the built-in shape. backLayer: backdrop instead of `in`.
// shortcut: the engine is 8-bit, so inside the glass 16/32-bit footage is quantized and HDR clipped; add an f32 path to glass_core when banding shows up.
// under8: what's under the layer, 8-bit ARGB whatever the project depth; wins over `in` and backLayer as the backdrop.
template <class P> int RenderFrameT(const Settings& s, const World& in, const World& out, const World* shapeLayer, const World* backLayer, const World* under8, Scratch& sc)
{
	using T = typename P::Chan;
	const Rect outR = out.rect();
	const size_t pxBytes = 4 * sizeof(T);

	// Pass-through. Clip-alpha mode outputs the glass alone: the layer below shows around it.
	const Rect both = Clip(outR, s.clipAlpha && !s.noGlass ? Rect{} : in.rect());
	for (int y = outR.t; y < outR.b; y++) {
		T* o = PixelAt<P>(out, outR.l, y);
		if (y < both.t || y >= both.b || both.empty()) {
			memset(o, 0, size_t(out.w) * pxBytes);
			continue;
		}
		if (both.l > outR.l) memset(o, 0, size_t(both.l - outR.l) * pxBytes);
		memcpy(PixelAt<P>(out, both.l, y), PixelAt<P>(in, both.l, y), size_t(both.w()) * pxBytes);
		if (both.r < outR.r) memset(PixelAt<P>(out, both.r, y), 0, size_t(outR.r - both.r) * pxBytes);
	}

	if (s.noGlass) return 0;
	const World* alpha = shapeLayer ? shapeLayer : (s.ownLayer || s.clipAlpha) ? &in : nullptr;
	const Rect bounds = alpha ? AlphaBounds<P>(*alpha) : BuiltInBounds(s);
	if (bounds.empty()) return 0;
	const int margin = RenderMargin(s);
	// Buffer reaches `margin` past the frame so a shape cut by the frame edge has no false edge there.
	const Rect roi = Clip(Grow(bounds, margin), Grow(outR, margin));
	if (roi.empty() || Clip(roi, outR).empty()) return 0;
	const int w = roi.w(), h = roi.h();
	const size_t n = size_t(w) * h;

	if (alpha) {
		sc.mask.assign(n, 0);
		const Rect a = Clip(alpha->rect(), roi);
		for (int y = a.t; y < a.b; y++) {
			const T* row = PixelAt<P>(*alpha, a.l, y);
			unsigned char* m = sc.mask.data() + size_t(y - roi.t) * w + (a.l - roi.l);
			for (int x = 0; x < a.w(); x++) m[x] = LayerMask<P>(row + x * 4, s);
		}
	} else {
		BuildMask(s, roi, sc);
	}
	if (MergeParts(s)) FuseParts(sc.mask.data(), w, h, s.core[20] * 0.5f, sc);

	// Backdrop to RGBA8.
	sc.backdrop.resize(n * 4);
	if (under8) FillMappedBackdrop(*under8, roi, s, sc);
	else FillBackdrop<P>(backLayer ? *backLayer : in, roi, sc);

	sc.glass.resize(n * 4);
	int e = lg_render(s.core, sc.backdrop.data(), sc.mask.data(), 1, (unsigned)w, (unsigned)h, sc.glass.data(), nullptr);
	if (e) return e;

	// Straight-alpha "over": glass and shadow on what the output already holds.
	const Rect draw = Clip(roi, outR);
	for (int y = draw.t; y < draw.b; y++) {
		const unsigned char* g = sc.glass.data() + (size_t(y - roi.t) * w + (draw.l - roi.l)) * 4;
		T* o = PixelAt<P>(out, draw.l, y);
		for (int x = 0; x < draw.w(); x++, g += 4, o += 4) {
			if (!g[3]) continue;
			if (g[3] == 255) {
				o[P::R] = FromUnit<P>(g[0] / 255.f);
				o[P::G] = FromUnit<P>(g[1] / 255.f);
				o[P::B] = FromUnit<P>(g[2] / 255.f);
				o[P::A] = (T)P::Max;
				continue;
			}
			const float ga = g[3] / 255.f, ba = (float)o[P::A] / P::Max, under = ba * (1 - ga), a = ga + under;
			const float kg = ga / a / 255.f, kb = under / a;
			o[P::R] = FromUnit<P>(g[0] * kg + o[P::R] / P::Max * kb);
			o[P::G] = FromUnit<P>(g[1] * kg + o[P::G] / P::Max * kb);
			o[P::B] = FromUnit<P>(g[2] * kg + o[P::B] / P::Max * kb);
			o[P::A] = FromUnit<P>(a);
		}
	}
	return 0;
}

inline int RenderFrame(PixFmt f, const Settings& s, const World& in, const World& out, const World* shapeLayer, const World* backLayer, Scratch& sc, const World* under8 = nullptr)
{
	switch (f) {
	case FMT_ARGB8: return RenderFrameT<PxARGB8>(s, in, out, shapeLayer, backLayer, under8, sc);
	case FMT_ARGB16: return RenderFrameT<PxARGB16>(s, in, out, shapeLayer, backLayer, under8, sc);
	case FMT_ARGBF: return RenderFrameT<PxARGBF>(s, in, out, shapeLayer, backLayer, under8, sc);
	case FMT_BGRA8: return RenderFrameT<PxBGRA8>(s, in, out, shapeLayer, backLayer, under8, sc);
	case FMT_BGRAF: return RenderFrameT<PxBGRAF>(s, in, out, shapeLayer, backLayer, under8, sc);
	}
	return -1;
}
