// Liquid Glass as one effect plug-in for After Effects and Premiere Pro (Effects > CGSudo > Liquid Glass).
//
// The effect's own input is the backdrop, so it goes on an adjustment layer (or directly on a clip); the glass shape is
// keyframeable parameters, which work the same in both apps. In After Effects a layer's alpha can replace the shape.
// After Effects renders through SmartFX, Premiere through PF_Cmd_RENDER. No globals, one scratch per thread: safe for MFR.
#include "AEConfig.h"
#include "entry.h"
#include "AEFX_SuiteHelper.h"
#include "PrSDKAESupport.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_EffectCBSuites.h"
#include "AE_Macros.h"
#include "Param_Utils.h"
#include "GlassHost.h"

enum { MAJOR = 1, MINOR = 0, BUG = 0, BUILD = 0 };
// Checkout ids of the extra layers in a smart render.
enum { CHECK_INPUT = 0, CHECK_SHAPE = 1, CHECK_BACK = 2 };

static PF_Err About(PF_InData* in_data, PF_OutData* out_data)
{
	PF_SPRINTF(out_data->return_msg, "Liquid Glass %d.%d by CGSudo\rRefracting glass, keyframeable. Put it on an adjustment layer.", MAJOR, MINOR);
	return PF_Err_NONE;
}

static PF_Err GlobalSetup(PF_InData* in_data, PF_OutData* out_data)
{
	if (lg_param_count() != kCoreCount) return PF_Err_INTERNAL_STRUCT_DAMAGED; // engine and host out of step
	out_data->my_version = PF_VERSION(MAJOR, MINOR, BUG, PF_Stage_RELEASE, BUILD);
	out_data->out_flags = PF_OutFlag_DEEP_COLOR_AWARE | PF_OutFlag_I_EXPAND_BUFFER;
	out_data->out_flags2 = PF_OutFlag2_PARAM_GROUP_START_COLLAPSED_FLAG | PF_OutFlag2_SUPPORTS_SMART_RENDER | PF_OutFlag2_FLOAT_COLOR_AWARE |
		PF_OutFlag2_I_MIX_GUID_DEPENDENCIES | PF_OutFlag2_SUPPORTS_THREADED_RENDERING;
	if (in_data->appl_id == kAppID_Premiere) {
		// Premiere has no GUID mixing, and Wobble changes with time alone.
		out_data->out_flags |= PF_OutFlag_NON_PARAM_VARY;
		AEFX_SuiteScoper<PF_PixelFormatSuite1> pf(in_data, kPFPixelFormatSuite, kPFPixelFormatSuiteVersion1, out_data);
		pf->ClearSupportedPixelFormats(in_data->effect_ref);
		pf->AddSupportedPixelFormat(in_data->effect_ref, PrPixelFormat_BGRA_4444_32f);
		pf->AddSupportedPixelFormat(in_data->effect_ref, PrPixelFormat_BGRA_4444_8u);
	}
	return PF_Err_NONE;
}

static PF_Err ParamsSetup(PF_InData* in_data, PF_OutData* out_data)
{
	PF_ParamDef def;
	for (const Row& r : kRows) {
		AEFX_CLR_STRUCT(def);
		switch (r.kind) {
		case K_TOPIC: PF_ADD_TOPIC(r.label, r.id); break;
		case K_TOPIC_CLOSED:
			def.flags = PF_ParamFlag_START_COLLAPSED;
			PF_ADD_TOPIC(r.label, r.id);
			break;
		case K_END: PF_END_TOPIC(r.id); break;
		case K_FLOAT:
			PF_ADD_FLOAT_SLIDERX(r.label, r.min, r.max, r.min, r.max, r.dflt, r.prec, 0, 0, r.id);
			break;
		case K_ANGLE: PF_ADD_ANGLE(r.label, r.dflt, r.id); break;
		case K_COLOR: {
			// All colour defaults are white today; a coloured default would be read from kDefaults here.
			const auto c = [&](int i) { return (A_u_char)(kDefaults[r.core + i] * 255 + 0.5f); };
			PF_ADD_COLOR(r.label, c(0), c(1), c(2), r.id);
			break;
		}
		case K_POPUP: {
			int n = 1;
			for (const char* p = r.items; *p; p++) n += *p == '|';
			PF_ADD_POPUP(r.label, n, (int)r.dflt, r.items, r.id);
			break;
		}
		case K_LOOK: {
			def.flags = PF_ParamFlag_SUPERVISE | PF_ParamFlag_CANNOT_TIME_VARY;
			PF_ADD_POPUP(r.label, kLookCount + 1, 1, r.items, r.id);
			break;
		}
		case K_CHECK:
			PF_ADD_CHECKBOXX(r.label, FALSE, 0, r.id);
			break;
		case K_POINT: PF_ADD_POINT(r.label, r.max, r.dflt, FALSE, r.id); break;
		case K_LAYER: PF_ADD_LAYER(r.label, PF_LayerDefault_NONE, r.id); break;
		}
	}
	out_data->num_params = kRowCount + 1;
	return PF_Err_NONE;
}

// Look popup: write the look into the parameters, then snap the popup back.
static PF_Err UserChangedParam(PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[], const PF_UserChangedParamExtra* extra)
{
	PF_ParamDef* look = params[IndexOf(P_LOOK)];
	if (extra->param_index != IndexOf(P_LOOK) || look->u.pd.value < 2) return PF_Err_NONE;

	float core[kCoreCount];
	core[3] = 0;
	core[20] = 0;
	ApplyLook(look->u.pd.value - 2, core);
	for (int i = 0; i < kRowCount; i++) {
		const Row& r = kRows[i];
		if (r.core < 0 || r.core == 3 || r.core == 20) continue;
		PF_ParamDef* d = params[i + 1];
		switch (r.kind) {
		case K_FLOAT: d->u.fs_d.value = core[r.core]; break;
		case K_ANGLE: d->u.ad.value = FLOAT2FIX(core[r.core]); break;
		case K_POPUP: d->u.pd.value = (A_long)core[r.core] + 1; break;
		case K_COLOR:
			d->u.cd.value.red = (A_u_char)(core[r.core] * 255 + 0.5f);
			d->u.cd.value.green = (A_u_char)(core[r.core + 1] * 255 + 0.5f);
			d->u.cd.value.blue = (A_u_char)(core[r.core + 2] * 255 + 0.5f);
			break;
		default: continue;
		}
		d->uu.change_flags = PF_ChangeFlag_CHANGED_VALUE;
	}
	look->u.pd.value = 1;
	look->uu.change_flags = PF_ChangeFlag_CHANGED_VALUE;
	return PF_Err_NONE;
}

static float Ratio(const PF_RationalScale& r) { return r.den ? (float)r.num / r.den : 1.f; }

// All values at the current time, in this render's pixels. `params`: the render call's own parameters (Premiere), so no checkouts;
// null in a smart render, which has none.
static PF_Err ReadSettings(PF_InData* in_data, PF_ParamDef* params[], Settings& s)
{
	PF_Err err = PF_Err_NONE, err2 = PF_Err_NONE;
	std::copy(kDefaults, kDefaults + kCoreCount, s.core);
	const float ds = Ratio(in_data->downsample_x);
	for (int i = 0; i < kRowCount && !err; i++) {
		const Row& r = kRows[i];
		if (r.kind == K_TOPIC || r.kind == K_TOPIC_CLOSED || r.kind == K_END || r.kind == K_LAYER || r.kind == K_LOOK) continue;
		PF_ParamDef held;
		AEFX_CLR_STRUCT(held);
		if (!params) ERR(PF_CHECKOUT_PARAM(in_data, i + 1, in_data->current_time, in_data->time_step, in_data->time_scale, &held));
		if (err) break;
		const PF_ParamDef& d = params ? *params[i + 1] : held;
		float v = 0, x = 0, y = 0;
		switch (r.kind) {
		case K_FLOAT: v = (float)d.u.fs_d.value; break;
		case K_ANGLE: v = (float)FIX_2_FLOAT(d.u.ad.value); break;
		case K_POPUP: v = (float)d.u.pd.value; break;
		case K_CHECK: v = d.u.bd.value ? 1.f : 0.f; break;
		case K_POINT:
			x = (float)FIX_2_FLOAT(d.u.td.x_value);
			y = (float)FIX_2_FLOAT(d.u.td.y_value);
			break;
		case K_COLOR:
			s.core[r.core] = d.u.cd.value.red / 255.f;
			s.core[r.core + 1] = d.u.cd.value.green / 255.f;
			s.core[r.core + 2] = d.u.cd.value.blue / 255.f;
			break;
		default: break;
		}
		if (r.core >= 0 && r.kind != K_COLOR) s.core[r.core] = r.kind == K_POPUP ? v - 1 : v;
		switch (r.id) {
		case P_TYPE: s.shape[0].type = (int)v; break;
		case P_CENTER: s.shape[0].cx = x; s.shape[0].cy = y; break;
		case P_WIDTH: s.shape[0].w = v; break;
		case P_HEIGHT: s.shape[0].h = v; break;
		case P_RADIUS: s.shape[0].radius = v; break;
		case P_ROTATION: s.shape[0].rotation = v; break;
		case P_CLIP_ALPHA: s.clipAlpha = v != 0; break;
		case P_SECOND: s.second = v != 0; break;
		case P_TYPE2: s.shape[1].type = (int)v; break;
		case P_CENTER2: s.shape[1].cx = x; s.shape[1].cy = y; break;
		case P_WIDTH2: s.shape[1].w = v; break;
		case P_HEIGHT2: s.shape[1].h = v; break;
		case P_RADIUS2: s.shape[1].radius = v; break;
		case P_ROTATION2: s.shape[1].rotation = v; break;
		case P_FOLLOW: s.follow = v != 0; break;
		case P_WOBBLE: s.wobble = v; break;
		case P_WOBBLE_SPEED: s.wobbleSpeed = v; break;
		default: break;
		}
		if (!params) ERR2(PF_CHECKIN_PARAM(in_data, &held));
	}
	if (err) return err;
	s.seconds = in_data->time_scale ? (float)in_data->current_time / in_data->time_scale : 0;
	ScaleToBuffer(s, ds);

	if (s.follow) { // where the centre was one frame ago
		PF_ParamDef d;
		AEFX_CLR_STRUCT(d);
		ERR(PF_CHECKOUT_PARAM(in_data, IndexOf(P_CENTER), in_data->current_time - in_data->time_step, in_data->time_step, in_data->time_scale, &d));
		if (!err) {
			const float dx = s.shape[0].cx - (float)FIX_2_FLOAT(d.u.td.x_value), dy = s.shape[0].cy - (float)FIX_2_FLOAT(d.u.td.y_value);
			s.core[0] = FollowLight(s.core[0], dx, dy, ds);
			ERR2(PF_CHECKIN_PARAM(in_data, &d));
		}
	}
	return err;
}

static World ToWorld(const PF_EffectWorld& w, bool smart)
{
	World o;
	o.data = reinterpret_cast<unsigned char*>(w.data);
	o.rowbytes = w.rowbytes;
	o.w = w.width;
	o.h = w.height;
	o.ox = smart ? w.origin_x : 0; // only smart checkouts fill the origin
	o.oy = smart ? w.origin_y : 0;
	return o;
}

// Keeps the scratch for the next frame, unless it's big enough that holding on to it per thread costs real memory.
static Scratch& ThreadScratch()
{
	thread_local Scratch sc;
	return sc;
}
static void TrimScratch(Scratch& sc)
{
	if (sc.backdrop.capacity() > 48u << 20) sc = Scratch();
}

static PF_LRect ToLRect(const Rect& r)
{
	PF_LRect o;
	o.left = r.l;
	o.top = r.t;
	o.right = r.r;
	o.bottom = r.b;
	return o;
}
static Rect FromLRect(const PF_LRect& r) { return { r.left, r.top, r.right, r.bottom }; }

static void DeleteSettings(void* p) { delete static_cast<Settings*>(p); }

static PF_Err SmartPreRender(PF_InData* in_data, PF_OutData* out_data, PF_PreRenderExtra* extra)
{
	PF_Err err = PF_Err_NONE;
	Settings* held = new Settings; // handed to SmartRender, so a frame checks out its 60-odd parameters once
	extra->output->pre_render_data = held;
	extra->output->delete_pre_render_data_func = DeleteSettings;
	Settings& s = *held;
	ERR(ReadSettings(in_data, nullptr, s));
	if (err) return err;

	const Rect request = FromLRect(extra->input->output_request.rect);
	const int margin = Margin(s.core, Ratio(in_data->downsample_x));
	PF_RenderRequest req = extra->input->output_request;
	req.channel_mask = PF_ChannelMask_ARGB;
	req.rect = ToLRect(Grow(request, margin)); // the lens and the frost read past the requested area
	req.preserve_rgb_of_zero_alpha = FALSE;

	PF_CheckoutResult in, shape, back;
	AEFX_CLR_STRUCT(in);
	AEFX_CLR_STRUCT(shape);
	AEFX_CLR_STRUCT(back);
	ERR(extra->cb->checkout_layer(in_data->effect_ref, 0, CHECK_INPUT, &req, in_data->current_time, in_data->time_step, in_data->time_scale, &in));
	ERR(extra->cb->checkout_layer(in_data->effect_ref, IndexOf(P_SHAPE_LAYER), CHECK_SHAPE, &req, in_data->current_time, in_data->time_step, in_data->time_scale, &shape));
	ERR(extra->cb->checkout_layer(in_data->effect_ref, IndexOf(P_BACK_LAYER), CHECK_BACK, &req, in_data->current_time, in_data->time_step, in_data->time_scale, &back));
	if (err) return err;

	// Glass may reach past the input (a shadow off the edge of a small layer). Outside it, the input just passes through.
	const Rect shapeMax = FromLRect(shape.max_result_rect);
	const Rect glassMax = !shapeMax.empty() ? Grow(shapeMax, margin) : s.clipAlpha ? Grow(FromLRect(in.max_result_rect), margin) : Grow(BuiltInBounds(s), margin);
	Rect inResult = FromLRect(in.result_rect);
	if (s.clipAlpha) inResult = {}; // the input isn't passed through in this mode
	extra->output->result_rect = ToLRect(Clip(Join(inResult, glassMax), request));
	extra->output->max_result_rect = ToLRect(Join(s.clipAlpha ? Rect{} : FromLRect(in.max_result_rect), glassMax));
	extra->output->solid = FALSE;
	extra->output->flags = 0;

	// What the parameters at this time don't show: time for the Wobble, the follow-motion angle.
	if (extra->cb->GuidMixInPtr) {
		const float mix[2] = { s.wobble > 0 ? s.seconds * s.wobbleSpeed : 0, s.core[0] };
		extra->cb->GuidMixInPtr(in_data->effect_ref, sizeof mix, mix);
	}
	return err;
}

static PF_Err WorldFormat(PF_InData* in_data, PF_OutData* out_data, PF_EffectWorld* world, PixFmt& fmt)
{
	if (in_data->appl_id == kAppID_Premiere) {
		AEFX_SuiteScoper<PF_PixelFormatSuite1> pf(in_data, kPFPixelFormatSuite, kPFPixelFormatSuiteVersion1, out_data);
		PrPixelFormat f = PrPixelFormat_Invalid;
		pf->GetPixelFormat(world, &f);
		if (f == PrPixelFormat_BGRA_4444_8u) fmt = FMT_BGRA8;
		else if (f == PrPixelFormat_BGRA_4444_32f) fmt = FMT_BGRAF;
		else return PF_Err_INVALID_CALLBACK;
		return PF_Err_NONE;
	}
	AEFX_SuiteScoper<PF_WorldSuite2> ws(in_data, kPFWorldSuite, kPFWorldSuiteVersion2, out_data);
	PF_PixelFormat f = PF_PixelFormat_INVALID;
	PF_Err err = ws->PF_GetPixelFormat(world, &f);
	if (err) return err;
	if (f == PF_PixelFormat_ARGB32) fmt = FMT_ARGB8;
	else if (f == PF_PixelFormat_ARGB64) fmt = FMT_ARGB16;
	else if (f == PF_PixelFormat_ARGB128) fmt = FMT_ARGBF;
	else return PF_Err_INVALID_CALLBACK;
	return PF_Err_NONE;
}

static PF_Err Draw(PF_InData* in_data, PF_OutData* out_data, const Settings& s, PF_EffectWorld* in, PF_EffectWorld* out, PF_EffectWorld* shape, PF_EffectWorld* back, bool smart)
{
	PixFmt fmt = FMT_ARGB8;
	PF_Err err = WorldFormat(in_data, out_data, out, fmt);
	if (err) return err;
	World wIn, wShape, wBack;
	if (in && in->data) wIn = ToWorld(*in, smart);
	const bool hasShape = shape && shape->data, hasBack = back && back->data;
	if (hasShape) wShape = ToWorld(*shape, smart);
	if (hasBack) wBack = ToWorld(*back, smart);
	Scratch& sc = ThreadScratch();
	int e = RenderFrame(fmt, s, wIn, ToWorld(*out, smart), hasShape ? &wShape : nullptr, hasBack ? &wBack : nullptr, sc);
	TrimScratch(sc);
	return e ? PF_Err_OUT_OF_MEMORY : PF_Err_NONE;
}

static PF_Err SmartRender(PF_InData* in_data, PF_OutData* out_data, PF_SmartRenderExtra* extra)
{
	PF_Err err = PF_Err_NONE;
	const Settings& s = *static_cast<const Settings*>(extra->input->pre_render_data);
	PF_EffectWorld* in = nullptr, * out = nullptr, * shape = nullptr, * back = nullptr;
	ERR(extra->cb->checkout_layer_pixels(in_data->effect_ref, CHECK_INPUT, &in));
	ERR(extra->cb->checkout_layer_pixels(in_data->effect_ref, CHECK_SHAPE, &shape));
	ERR(extra->cb->checkout_layer_pixels(in_data->effect_ref, CHECK_BACK, &back));
	ERR(extra->cb->checkout_output(in_data->effect_ref, &out));
	if (!err && out) err = Draw(in_data, out_data, s, in, out, shape, back, true);
	return err;
}

// Premiere: the frame arrives whole; layer parameters are checked out like any other.
static PF_Err Render(PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[], PF_LayerDef* output)
{
	PF_Err err = PF_Err_NONE, err2 = PF_Err_NONE;
	Settings s;
	ERR(ReadSettings(in_data, params, s));
	if (err) return err;
	PF_ParamDef shape, back;
	AEFX_CLR_STRUCT(shape);
	AEFX_CLR_STRUCT(back);
	ERR(PF_CHECKOUT_PARAM(in_data, IndexOf(P_SHAPE_LAYER), in_data->current_time, in_data->time_step, in_data->time_scale, &shape));
	ERR(PF_CHECKOUT_PARAM(in_data, IndexOf(P_BACK_LAYER), in_data->current_time, in_data->time_step, in_data->time_scale, &back));
	if (!err) err = Draw(in_data, out_data, s, &params[0]->u.ld, output, &shape.u.ld, &back.u.ld, false);
	ERR2(PF_CHECKIN_PARAM(in_data, &shape));
	ERR2(PF_CHECKIN_PARAM(in_data, &back));
	return err;
}

extern "C" DllExport PF_Err PluginDataEntryFunction2(PF_PluginDataPtr inPtr, PF_PluginDataCB2 inPluginDataCallBackPtr, SPBasicSuite*, const char*, const char*)
{
	PF_Err result = PF_Err_INVALID_CALLBACK;
	result = PF_REGISTER_EFFECT_EXT2(inPtr, inPluginDataCallBackPtr, "Liquid Glass", "CGSudo Liquid Glass", "CGSudo", AE_RESERVED_INFO, "EffectMain",
		"https://github.com/CGSudo/liquid-glass-for-adobe");
	return result;
}

extern "C" DllExport PF_Err EffectMain(PF_Cmd cmd, PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[], PF_LayerDef* output, void* extra)
{
	try {
		switch (cmd) {
		case PF_Cmd_ABOUT: return About(in_data, out_data);
		case PF_Cmd_GLOBAL_SETUP: return GlobalSetup(in_data, out_data);
		case PF_Cmd_PARAMS_SETUP: return ParamsSetup(in_data, out_data);
		case PF_Cmd_USER_CHANGED_PARAM: return UserChangedParam(in_data, out_data, params, (const PF_UserChangedParamExtra*)extra);
		case PF_Cmd_SMART_PRE_RENDER: return SmartPreRender(in_data, out_data, (PF_PreRenderExtra*)extra);
		case PF_Cmd_SMART_RENDER: return SmartRender(in_data, out_data, (PF_SmartRenderExtra*)extra);
		case PF_Cmd_RENDER: return Render(in_data, out_data, params, output);
		default: return PF_Err_NONE;
		}
	} catch (PF_Err& e) {
		return e;
	} catch (...) {
		return PF_Err_OUT_OF_MEMORY; // std::bad_alloc from the scratch buffers
	}
}
