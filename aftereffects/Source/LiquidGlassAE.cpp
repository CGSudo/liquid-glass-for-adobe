// Liquid Glass as one effect plug-in for After Effects and Premiere Pro (Effects > CGSudo > Liquid Glass).
//
// The effect's own input is the backdrop, so it goes on an adjustment layer (or directly on a clip); the glass shape is
// keyframeable parameters, which work the same in both apps. In After Effects a layer's alpha can replace the shape.
// After Effects renders through SmartFX, Premiere through PF_Cmd_RENDER. No globals, one scratch per thread: safe for MFR.
#include "AEConfig.h"
#include "AE_EffectUI.h"
#include <adobesdk/DrawbotSuite.h>
#include "entry.h"
#include "AEFX_SuiteHelper.h"
#include "PrSDKAESupport.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_EffectCBSuites.h"
#include "AE_Macros.h"
#include "Param_Utils.h"
#include "AE_GeneralPlug.h"
#include "GlassHost.h"
#include "GlassStudioModel.h"
#include "LiquidGlassNames.h"
#include "GlassPremiereSDK.h"
#include <cstdio>
#include <cstdlib>

enum { MAJOR = 1, MINOR = 0, BUG = 0, BUILD = 0 };
static AEGP_PluginID gPlugin = 0; // both written once in GLOBAL_SETUP, before any render thread exists
static SPBasicSuite* gBasic = nullptr;

// TEMP spike log, removed before commit.
#define LG_LOG(...) do { if (const char* p_ = std::getenv("LG_TEST_LOG")) { if (FILE* f_ = std::fopen(p_, "a")) { std::fprintf(f_, __VA_ARGS__); std::fprintf(f_, "\n"); std::fclose(f_); } } } while (0)
// Checkout ids of the extra layers in a smart render.
enum { CHECK_INPUT = 0, CHECK_SHAPE = 1, CHECK_BACK = 2 };

static PF_Err About(PF_InData* in_data, PF_OutData* out_data)
{
	PF_SPRINTF(out_data->return_msg, "Liquid Glass %d.%d by CGSudo\rRefracting glass, keyframeable. Put it on an adjustment layer.", MAJOR, MINOR);
	return PF_Err_NONE;
}

static A_Err OnIdle(AEGP_GlobalRefcon, AEGP_IdleRefcon, A_long*);
static void RegisterIdle(PF_InData* in_data, PF_OutData* out_data)
{
	AEFX_SuiteScoper<AEGP_RegisterSuite5> reg(in_data, kAEGPRegisterSuite, kAEGPRegisterSuiteVersion5, out_data);
	reg->AEGP_RegisterIdleHook(gPlugin, OnIdle, nullptr);
}

static PF_Err GlobalSetup(PF_InData* in_data, PF_OutData* out_data)
{
	if (lg_param_count() != kCoreCount) return PF_Err_INTERNAL_STRUCT_DAMAGED; // engine and host out of step
	out_data->my_version = PF_VERSION(MAJOR, MINOR, BUG, PF_Stage_RELEASE, BUILD);
	out_data->out_flags = PF_OutFlag_DEEP_COLOR_AWARE | PF_OutFlag_I_EXPAND_BUFFER | PF_OutFlag_CUSTOM_UI;
	out_data->out_flags2 = PF_OutFlag2_PARAM_GROUP_START_COLLAPSED_FLAG | PF_OutFlag2_SUPPORTS_SMART_RENDER | PF_OutFlag2_FLOAT_COLOR_AWARE |
		PF_OutFlag2_I_MIX_GUID_DEPENDENCIES | PF_OutFlag2_SUPPORTS_THREADED_RENDERING;
	if (in_data->appl_id == kAppID_Premiere) {
		PremiereSetup(in_data, out_data);
		// Premiere has no GUID mixing, and Wobble changes with time alone.
		out_data->out_flags |= PF_OutFlag_NON_PARAM_VARY;
		AEFX_SuiteScoper<PF_PixelFormatSuite1> pf(in_data, kPFPixelFormatSuite, kPFPixelFormatSuiteVersion1, out_data);
		pf->ClearSupportedPixelFormats(in_data->effect_ref);
		pf->AddSupportedPixelFormat(in_data->effect_ref, PrPixelFormat_BGRA_4444_32f);
		pf->AddSupportedPixelFormat(in_data->effect_ref, PrPixelFormat_BGRA_4444_8u);
	} else {
		AEFX_SuiteScoper<AEGP_UtilitySuite6> util(in_data, kAEGPUtilitySuite, kAEGPUtilitySuiteVersion6, out_data);
		util->AEGP_RegisterWithAEGP(nullptr, "Liquid Glass", &gPlugin);
		gBasic = in_data->pica_basicP;
		RegisterIdle(in_data, out_data);
	}
	return PF_Err_NONE;
}

static float Ratio(const PF_RationalScale& r) { return r.den ? (float)r.num / r.den : 1.f; }

// A shape or text layer can't see what's under it, but an effect on an adjustment layer can. So applying the effect also puts a
// small adjustment layer right below (an empty shape layer, so no Solid lands in the project), and the render reads through it.
#define LG_BACKDROP_LAYER "Liquid Glass backdrop"

// True if this is the backdrop layer. Wide name compare, no conversions: the name is ours and plain ASCII.
static bool IsBackdropLayer(const AEGP_LayerSuite9* ls, const AEGP_MemorySuite1* mem, AEGP_LayerH l)
{
	AEGP_MemHandle name = nullptr;
	if (ls->AEGP_GetLayerName(gPlugin, l, &name, nullptr) || !name) return false;
	const A_UTF16Char* p = nullptr;
	mem->AEGP_LockMemHandle(name, (void**)&p);
	const char* want = LG_BACKDROP_LAYER;
	bool same = p != nullptr;
	for (int i = 0; same; i++) {
		if (p[i] != (A_UTF16Char)want[i]) same = false;
		if (!want[i]) break;
	}
	mem->AEGP_UnlockMemHandle(name);
	mem->AEGP_FreeMemHandle(name);
	return same;
}

// Right below the layer.
static void MakeBackdropLayer(const PF_InData* in_data, PF_OutData* out_data, const AEGP_LayerSuite9* ls, AEGP_CompH comp, AEGP_LayerH layer)
{
	AEFX_SuiteScoper<AEGP_CompSuite13> cs(in_data, kAEGPCompSuite, kAEGPCompSuiteVersion13, out_data);
	AEFX_SuiteScoper<AEGP_EffectSuite5> fx(in_data, kAEGPEffectSuite, kAEGPEffectSuiteVersion5, out_data);
	AEFX_SuiteScoper<AEGP_UtilitySuite6> util(in_data, kAEGPUtilitySuite, kAEGPUtilitySuiteVersion6, out_data);
	util->AEGP_StartUndoGroup("Liquid Glass");
	// A new layer takes the selection, which would swap the Effect Controls panel away from the layer the user is working on.
	AEFX_SuiteScoper<AEGP_CollectionSuite2> coll(in_data, kAEGPCollectionSuite, kAEGPCollectionSuiteVersion2, out_data);
	AEGP_Collection2H selected = nullptr;
	cs->AEGP_GetNewCollectionFromCompSelection(gPlugin, comp, &selected);
	AEGP_LayerH helper = nullptr;
	A_Err e = cs->AEGP_CreateVectorLayerInComp(comp, &helper);
	LG_LOG("create helper err=%d", (int)e);
	if (!e && helper) {
		const char* name = LG_BACKDROP_LAYER;
		A_UTF16Char wide[64] = {};
		for (int i = 0; name[i]; i++) wide[i] = (A_UTF16Char)name[i];
		ls->AEGP_SetLayerName(helper, wide);
		ls->AEGP_SetLayerFlag(helper, AEGP_LayerFlag_ADJUSTMENT_LAYER, TRUE);
		ls->AEGP_SetLayerFlag(helper, AEGP_LayerFlag_SHY, TRUE);
		AEGP_InstalledEffectKey key = AEGP_InstalledEffectKey_NONE;
		while (!fx->AEGP_GetNextInstalledEffect(key, &key) && key != AEGP_InstalledEffectKey_NONE) {
			A_char match[AEGP_MAX_EFFECT_MATCH_NAME_SIZE + 1] = {};
			if (!fx->AEGP_GetEffectMatchName(key, match) && !std::strcmp(match, "ADBE Slider Control")) {
				AEGP_EffectRefH slider = nullptr;
				e = fx->AEGP_ApplyEffect(gPlugin, helper, key, &slider);
				if (slider) fx->AEGP_DisposeEffect(slider);
				LG_LOG("slider err=%d", (int)e);
				break;
			}
		}
		// The new layer went on top, pushing this one down one.
		A_long at = 0;
		ls->AEGP_GetLayerIndex(layer, &at);
		e = ls->AEGP_ReorderLayer(helper, at);
		LG_LOG("reorder to %d err=%d", (int)at, (int)e);
	}
	if (selected) {
		cs->AEGP_SetSelection(comp, selected);
		coll->AEGP_DisposeCollection(selected);
	}
	util->AEGP_EndUndoGroup();
}

// Layers the effect was just applied to, waiting for the next idle. UI thread only: SEQUENCE_SETUP and the idle hook both run there.
struct NewGlass {
	A_long compItem;
	AEGP_LayerIDVal layer;
};
static std::vector<NewGlass> gNewGlass;

// Hides the built-in shape controls of this effect on the layer: they mean nothing when the layer itself is the shape.
static void HideShapeControls(const PF_InData* in_data, AEGP_LayerH layer)
{
	AEFX_SuiteScoper<AEGP_EffectSuite5> fx(in_data, kAEGPEffectSuite, kAEGPEffectSuiteVersion5, nullptr);
	AEFX_SuiteScoper<AEGP_StreamSuite7> ss(in_data, kAEGPStreamSuite, kAEGPStreamSuiteVersion7, nullptr);
	AEFX_SuiteScoper<AEGP_DynamicStreamSuite4> ds(in_data, kAEGPDynamicStreamSuite, kAEGPDynamicStreamSuiteVersion4, nullptr);
	A_long effects = 0;
	if (fx->AEGP_GetLayerNumEffects(layer, &effects)) return;
	for (A_long i = 0; i < effects; i++) {
		AEGP_EffectRefH ref = nullptr;
		if (fx->AEGP_GetLayerEffectByIndex(gPlugin, layer, i, &ref) || !ref) continue;
		AEGP_InstalledEffectKey key = AEGP_InstalledEffectKey_NONE;
		A_char match[AEGP_MAX_EFFECT_MATCH_NAME_SIZE + 1] = {};
		if (!fx->AEGP_GetInstalledKeyFromLayerEffect(ref, &key) && !fx->AEGP_GetEffectMatchName(key, match) && !std::strcmp(match, LG_MATCH)) {
			const auto hide = [&](int from, int to) {
				for (int id = from; id <= to; id++) {
					AEGP_StreamRefH st = nullptr;
					if (ss->AEGP_GetNewEffectStreamByIndex(gPlugin, ref, IndexOf(id), &st) || !st) continue;
					A_Err h = ds->AEGP_SetDynamicStreamFlag(st, AEGP_DynStreamFlag_HIDDEN, FALSE, TRUE);
					if (h) LG_LOG("hide %d err=%d", id, (int)h);
					ss->AEGP_DisposeStream(st);
				}
			};
			hide(P_T_SHAPE, P_E_SHAPE2);
			hide(P_FOLLOW, P_FOLLOW);
			hide(P_T_ANIM, P_E_ANIM);
		}
		fx->AEGP_DisposeEffect(ref);
	}
}

// Nothing else is running here, so changing the project is safe. Ids, not handles: the layer may be gone by now (undo, project closed).
static A_Err OnIdle(AEGP_GlobalRefcon, AEGP_IdleRefcon, A_long*)
{
	if (gNewGlass.empty() || !gBasic) return A_Err_NONE;
	LG_LOG("idle: %d to do", (int)gNewGlass.size());
	const std::vector<NewGlass> todo = std::move(gNewGlass);
	gNewGlass.clear();
	PF_InData host = {}; // the suite helpers only read the suite pointer from it
	host.pica_basicP = gBasic;
	AEFX_SuiteScoper<AEGP_ProjSuite6> ps(&host, kAEGPProjSuite, kAEGPProjSuiteVersion6, nullptr);
	AEFX_SuiteScoper<AEGP_ItemSuite9> is(&host, kAEGPItemSuite, kAEGPItemSuiteVersion9, nullptr);
	AEFX_SuiteScoper<AEGP_CompSuite13> cs(&host, kAEGPCompSuite, kAEGPCompSuiteVersion13, nullptr);
	AEFX_SuiteScoper<AEGP_LayerSuite9> ls(&host, kAEGPLayerSuite, kAEGPLayerSuiteVersion9, nullptr);
	AEFX_SuiteScoper<AEGP_MemorySuite1> mem(&host, kAEGPMemorySuite, kAEGPMemorySuiteVersion1, nullptr);
	AEGP_ProjectH project = nullptr;
	if (ps->AEGP_GetProjectByIndex(0, &project) || !project) return A_Err_NONE;
	for (const NewGlass& g : todo) {
		AEGP_ItemH item = nullptr;
		is->AEGP_GetFirstProjItem(project, &item);
		for (; item; ) {
			A_long id = 0;
			if (!is->AEGP_GetItemID(item, &id) && id == g.compItem) break;
			AEGP_ItemH next = nullptr;
			if (is->AEGP_GetNextProjItem(project, item, &next)) next = nullptr;
			item = next;
		}
		AEGP_CompH comp = nullptr;
		AEGP_LayerH layer = nullptr;
		if (!item || cs->AEGP_GetCompFromItem(item, &comp) || !comp || ls->AEGP_GetLayerFromLayerID(comp, g.layer, &layer) || !layer) { LG_LOG("idle: layer gone"); continue; }
		LG_LOG("idle: found layer");
		HideShapeControls(&host, layer);
		LG_LOG("idle: hidden");
		A_long at = 0, count = 0;
		AEGP_LayerH below = nullptr;
		if (ls->AEGP_GetLayerIndex(layer, &at) || ls->AEGP_GetCompNumLayers(comp, &count)) continue;
		if (at + 1 < count && !ls->AEGP_GetCompLayerByIndex(comp, at + 1, &below) && below && IsBackdropLayer(ls.get(), mem.get(), below)) continue;
		MakeBackdropLayer(&host, nullptr, ls.get(), comp, layer);
	}
	return A_Err_NONE;
}

// Only notes the layer. From inside this call a script locks up the script engine, touching the effect's streams stalls, and a
// new layer never reaches the render threads' copy of the project.
static PF_Err SequenceSetup(PF_InData* in_data, PF_OutData* out_data)
{
	if (in_data->appl_id == kAppID_Premiere || !in_data->effect_ref || !gPlugin) return PF_Err_NONE;
	AEFX_SuiteScoper<AEGP_PFInterfaceSuite1> pfi(in_data, kAEGPPFInterfaceSuite, kAEGPPFInterfaceSuiteVersion1, out_data);
	AEFX_SuiteScoper<AEGP_LayerSuite9> ls(in_data, kAEGPLayerSuite, kAEGPLayerSuiteVersion9, out_data);
	AEFX_SuiteScoper<AEGP_CompSuite13> cs(in_data, kAEGPCompSuite, kAEGPCompSuiteVersion13, out_data);
	AEFX_SuiteScoper<AEGP_ItemSuite9> is(in_data, kAEGPItemSuite, kAEGPItemSuiteVersion9, out_data);
	AEGP_LayerH layer = nullptr;
	AEGP_ObjectType type = AEGP_ObjectType_NONE;
	if (pfi->AEGP_GetEffectLayer(in_data->effect_ref, &layer) || !layer || ls->AEGP_GetLayerObjectType(layer, &type)) return PF_Err_NONE;
	if (type != AEGP_ObjectType_VECTOR && type != AEGP_ObjectType_TEXT) return PF_Err_NONE;
	AEGP_CompH comp = nullptr;
	AEGP_ItemH item = nullptr;
	NewGlass g = {};
	if (ls->AEGP_GetLayerParentComp(layer, &comp) || cs->AEGP_GetItemFromComp(comp, &item) || is->AEGP_GetItemID(item, &g.compItem) || ls->AEGP_GetLayerID(layer, &g.layer)) return PF_Err_NONE;
	gNewGlass.push_back(g);
	LG_LOG("setup: noted layer %d in comp item %d", (int)g.layer, (int)g.compItem);
	return PF_Err_NONE;
}

// What's under the glass: the input of the effect on the nearest backdrop layer below, as 8-bit ARGB in comp pixels.
// Held by a frame receipt until this goes out of scope. ok = false for any other layer type, or with no backdrop layer.
struct UnderFrame {
	PF_InData* id;
	PF_OutData* od;
	AEGP_FrameReceiptH receipt = nullptr;
	World world;
	bool ok = false;
	bool shapeOrText = false; // and not switched to Adjustment by hand

	UnderFrame(PF_InData* in, PF_OutData* out) : id(in), od(out)
	{
		if (!gPlugin || id->appl_id == kAppID_Premiere) return;
		AEFX_SuiteScoper<AEGP_PFInterfaceSuite1> pfi(id, kAEGPPFInterfaceSuite, kAEGPPFInterfaceSuiteVersion1, od);
		AEFX_SuiteScoper<AEGP_LayerSuite9> ls(id, kAEGPLayerSuite, kAEGPLayerSuiteVersion9, od);
		AEFX_SuiteScoper<AEGP_MemorySuite1> mem(id, kAEGPMemorySuite, kAEGPMemorySuiteVersion1, od);
		AEGP_LayerH layer = nullptr;
		AEGP_ObjectType type = AEGP_ObjectType_NONE;
		AEGP_LayerFlags flags = 0;
		if (pfi->AEGP_GetEffectLayer(id->effect_ref, &layer) || !layer || ls->AEGP_GetLayerObjectType(layer, &type) || ls->AEGP_GetLayerFlags(layer, &flags)) return;
		if ((type != AEGP_ObjectType_VECTOR && type != AEGP_ObjectType_TEXT) || (flags & AEGP_LayerFlag_ADJUSTMENT_LAYER)) return;
		shapeOrText = true;

		AEGP_CompH comp = nullptr;
		A_long at = 0, count = 0;
		if (ls->AEGP_GetLayerParentComp(layer, &comp) || ls->AEGP_GetLayerIndex(layer, &at) || ls->AEGP_GetCompNumLayers(comp, &count)) return;
		AEGP_LayerH helper = nullptr;
		for (A_long i = at + 1; i < count && !helper; i++) {
			AEGP_LayerH l = nullptr;
			if (!ls->AEGP_GetCompLayerByIndex(comp, i, &l) && l && IsBackdropLayer(ls.get(), mem.get(), l)) helper = l;
		}
		LG_LOG("layer index %d of %d helper=%p", (int)at, (int)count, (void*)helper);
		if (!helper) return;

		AEFX_SuiteScoper<AEGP_EffectSuite5> fx(id, kAEGPEffectSuite, kAEGPEffectSuiteVersion5, od);
		AEFX_SuiteScoper<AEGP_LayerRenderOptionsSuite2> lro(id, kAEGPLayerRenderOptionsSuite, kAEGPLayerRenderOptionsSuiteVersion2, od);
		AEFX_SuiteScoper<AEGP_RenderSuite5> rs(id, kAEGPRenderSuite, kAEGPRenderSuiteVersion5, od);
		AEFX_SuiteScoper<AEGP_WorldSuite3> ws(id, kAEGPWorldSuite, kAEGPWorldSuiteVersion3, od);
		AEGP_EffectRefH tap = nullptr;
		AEGP_LayerRenderOptionsH opt = nullptr;
		A_Err e = fx->AEGP_GetLayerEffectByIndex(gPlugin, helper, 0, &tap);
		if (!e) e = lro->AEGP_NewFromUpstreamOfEffect(gPlugin, tap, &opt);
		if (!e) {
			A_Time t = {}, st = { id->time_step, (A_u_long)id->time_scale };
			pfi->AEGP_ConvertEffectToCompTime(id->effect_ref, id->current_time, id->time_scale, &t);
			lro->AEGP_SetTime(opt, t);
			lro->AEGP_SetTimeStep(opt, st);
			lro->AEGP_SetWorldType(opt, AEGP_WorldType_8);
			lro->AEGP_SetDownsampleFactor(opt, (A_short)(1 / Ratio(id->downsample_x) + 0.5f), (A_short)(1 / Ratio(id->downsample_y) + 0.5f));
			e = rs->AEGP_RenderAndCheckoutLayerFrame(opt, nullptr, nullptr, &receipt);
		}
		if (opt) lro->AEGP_Dispose(opt);
		if (tap) fx->AEGP_DisposeEffect(tap);
		if (e || !receipt) { LG_LOG("under render err=%d", (int)e); return; }
		AEGP_WorldH wh = nullptr;
		A_long w = 0, h = 0;
		A_u_long rb = 0;
		PF_Pixel8* base = nullptr;
		if (rs->AEGP_GetReceiptWorld(receipt, &wh) || ws->AEGP_GetSize(wh, &w, &h) || ws->AEGP_GetRowBytes(wh, &rb) || ws->AEGP_GetBaseAddr8(wh, &base)) return;
		world.data = reinterpret_cast<unsigned char*>(base);
		world.rowbytes = (int)rb;
		world.w = w;
		world.h = h;
		ok = true;
		LG_LOG("under %dx%d", w, h);
	}
	// AE doesn't know the glass depends on this frame: without this, changes below would leave stale glass in the cache.
	void MixGuid(PF_PreRenderExtra* extra)
	{
		if (!receipt || !extra->cb->GuidMixInPtr) return;
		AEFX_SuiteScoper<AEGP_RenderSuite5> rs(id, kAEGPRenderSuite, kAEGPRenderSuiteVersion5, od);
		AEFX_SuiteScoper<AEGP_MemorySuite1> mem(id, kAEGPMemorySuite, kAEGPMemorySuiteVersion1, od);
		AEGP_MemHandle guid = nullptr;
		if (rs->AEGP_GetReceiptGuid(receipt, &guid) || !guid) return;
		void* p = nullptr;
		AEGP_MemSize n = 0;
		mem->AEGP_LockMemHandle(guid, &p);
		mem->AEGP_GetMemHandleSize(guid, &n);
		if (p && n) extra->cb->GuidMixInPtr(id->effect_ref, (A_u_long)n, p);
		mem->AEGP_UnlockMemHandle(guid);
		mem->AEGP_FreeMemHandle(guid);
	}
	~UnderFrame()
	{
		if (receipt) {
			AEFX_SuiteScoper<AEGP_RenderSuite5> rs(id, kAEGPRenderSuite, kAEGPRenderSuiteVersion5, od);
			rs->AEGP_CheckinFrame(receipt);
		}
	}
};

static PF_Err ParamsSetup(PF_InData* in_data, PF_OutData* out_data)
{
	PF_ParamDef def;
	const bool premiere = in_data->appl_id == kAppID_Premiere;
	const bool studio = !premiere || PremiereStudioAvailable(in_data);
	for (const Row& r : kRows) {
		AEFX_CLR_STRUCT(def);
		if (premiere && r.id == P_BACK_LAYER) def.ui_flags = PF_PUI_INVISIBLE;
		switch (r.kind) {
		case K_TOPIC:
			if (in_data->appl_id != kAppID_Premiere) def.flags = PF_ParamFlag_START_COLLAPSED;
			PF_ADD_TOPIC(r.label, r.id); break;
		case K_TOPIC_CLOSED:
			def.flags = PF_ParamFlag_START_COLLAPSED;
			PF_ADD_TOPIC(r.label, r.id);
			break;
		case K_END: PF_END_TOPIC(r.id); break;
		case K_FLOAT:
			PF_ADD_FLOAT_SLIDERX(r.label, r.min, r.max, r.min, r.max, premiere && r.id == P_DISP ? 0 : r.dflt, r.prec, 0, 0, r.id);
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
			// Reuse the first parameter's custom control so saved projects and Premiere keep every index.
			if (studio) {
				def.ui_flags = PF_PUI_CONTROL | PF_PUI_DONT_ERASE_CONTROL;
				def.ui_width = studio::kWidth;
				def.ui_height = studio::kHeight;
			}
			PF_ADD_POPUP(studio ? "Glass Studio" : r.label, kLookCount + 1, 1, r.items, r.id);
			break;
		}
		case K_CHECK:
			PF_ADD_CHECKBOXX(r.label, premiere && r.id == P_CLIP_ALPHA, 0, r.id);
			break;
		case K_POINT: PF_ADD_POINT(r.label, r.max, r.dflt, FALSE, r.id); break;
		case K_LAYER: PF_ADD_LAYER(premiere && r.id == P_SHAPE_LAYER ? "Glass from" : r.label, PF_LayerDefault_NONE, r.id); break;
		}
	}
	out_data->num_params = kRowCount + 1;
	if (studio) {
		PF_CustomUIInfo ci = {};
		ci.events = PF_CustomEFlag_EFFECT;
		return in_data->inter.register_ui(in_data->effect_ref, &ci);
	}
	return PF_Err_NONE;
}

// Both the popup and Studio write the same native streams at the current time.
static PF_Err ApplyLookToParams(PF_ParamDef* params[], int n)
{
	PF_ParamDef* look = params[IndexOf(P_LOOK)];
	if (n < 0 || n >= kLookCount) return PF_Err_BAD_CALLBACK_PARAM;
	float core[kCoreCount];
	core[3] = 0;
	core[20] = 0;
	ApplyLook(n, core);
	for (int i = 0; i < kRowCount; i++) {
		const Row& r = kRows[i];
		if (r.core < 0 || r.core == 3 || r.core == 20) continue;
		PF_ParamDef* d = params[i + 1];
		switch (r.kind) {
		case K_FLOAT:
			if (d->u.fs_d.value == core[r.core]) continue;
			d->u.fs_d.value = core[r.core]; break;
		case K_ANGLE:
			if (d->u.ad.value == FLOAT2FIX(core[r.core])) continue;
			d->u.ad.value = FLOAT2FIX(core[r.core]); break;
		case K_POPUP:
			if (d->u.pd.value == (A_long)core[r.core] + 1) continue;
			d->u.pd.value = (A_long)core[r.core] + 1; break;
		case K_COLOR: {
			const A_u_char red = (A_u_char)(core[r.core] * 255 + 0.5f), green = (A_u_char)(core[r.core + 1] * 255 + 0.5f), blue = (A_u_char)(core[r.core + 2] * 255 + 0.5f);
			if (d->u.cd.value.red == red && d->u.cd.value.green == green && d->u.cd.value.blue == blue) continue;
			d->u.cd.value.red = red;
			d->u.cd.value.green = green;
			d->u.cd.value.blue = blue;
			break;
		}
		default: continue;
		}
		d->uu.change_flags |= PF_ChangeFlag_CHANGED_VALUE;
	}
	if (look->u.pd.value != 1) {
		look->u.pd.value = 1;
		look->uu.change_flags |= PF_ChangeFlag_CHANGED_VALUE;
	}
	return PF_Err_NONE;
}

// Look popup: write the look into the parameters, then snap the popup back.
static PF_Err UserChangedParam(PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[], const PF_UserChangedParamExtra* extra)
{
	PF_ParamDef* look = params[IndexOf(P_LOOK)];
	if (extra->param_index != IndexOf(P_LOOK) || look->u.pd.value < 2) return PF_Err_NONE;
	out_data->out_flags |= PF_OutFlag_REFRESH_UI;
	return ApplyLookToParams(params, look->u.pd.value - 2);
}

#include "GlassStudioUI.h"

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

	UnderFrame under(in_data, out_data);
	if (under.ok) {
		s.ownLayer = true;
		s.clipAlpha = true; // the layer is the outline; its colour is never used
		under.MixGuid(extra);
	}
	s.noGlass = under.shapeOrText && !under.ok;
	const Rect request = FromLRect(extra->input->output_request.rect);
	const int margin = Margin(s.core, Ratio(in_data->downsample_x)) + (s.ownLayer ? (int)std::ceil(s.core[20] * 0.5f) : 0);
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
	const Rect glassMax = s.noGlass ? Rect{} : !shapeMax.empty() ? Grow(shapeMax, margin) : s.clipAlpha ? Grow(FromLRect(in.max_result_rect), margin) : Grow(BuiltInBounds(s), margin);
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

static PF_Err Draw(PF_InData* in_data, PF_OutData* out_data, const Settings& s, PF_EffectWorld* in, PF_EffectWorld* out, PF_EffectWorld* shape, PF_EffectWorld* back, bool smart, const World* under = nullptr)
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
	LG_LOG("draw in %d,%d %dx%d out %d,%d %dx%d", wIn.ox, wIn.oy, wIn.w, wIn.h, out->origin_x, out->origin_y, out->width, out->height);
	int e = RenderFrame(fmt, s, wIn, ToWorld(*out, smart), hasShape ? &wShape : nullptr, hasBack ? &wBack : nullptr, sc, under);
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
	if (!err && out) {
		UnderFrame under(in_data, out_data);
		if (s.ownLayer && !under.ok) return PF_Err_INTERNAL_STRUCT_DAMAGED; // was there in pre-render
		err = Draw(in_data, out_data, s, in, out, shape, back, true, under.ok ? &under.world : nullptr);
	}
	return err;
}

// Premiere: the frame arrives whole; layer parameters are checked out like any other.
static PF_Err RenderPremiere(PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[], PF_LayerDef* output)
{
	Settings s;
	PF_Err err = ReadSettings(in_data, params, s);
	if (err) return err;
	PixFmt fmt, inputFmt;
	if ((err = WorldFormat(in_data, out_data, output, fmt))) return err;
	if ((err = WorldFormat(in_data, out_data, &params[0]->u.ld, inputFmt))) return err;
	if (fmt != inputFmt) return PF_Err_BAD_CALLBACK_PARAM;
	struct Checkout {
		PF_InData* in;
		PF_ParamDef param = {};
		bool held = false;
		~Checkout() { if (held) PF_CHECKIN_PARAM(in, &param); }
	} shape{ in_data };
	shape.held = !PF_CHECKOUT_PARAM(in_data, IndexOf(P_SHAPE_LAYER), in_data->current_time,
		in_data->time_step, in_data->time_scale, &shape.param);
	World input = ToWorld(params[0]->u.ld, false), out = ToWorld(*output, false), mask, backdrop;
	const World* alpha = nullptr;
	const World* back = nullptr;
	if (shape.held && shape.param.u.ld.data) {
		PixFmt shapeFmt;
		if (!WorldFormat(in_data, out_data, &shape.param.u.ld, shapeFmt) && shapeFmt == fmt) {
			mask = ToWorld(shape.param.u.ld, false);
			alpha = &mask;
		} else s.noGlass = true; // never read an ARGB checkout as BGRA or float
	}
	PremiereBackground background(in_data);
	if (!alpha && background.frame && background.frame->data) {
		PixFmt backFmt;
		if (!WorldFormat(in_data, out_data, background.frame, backFmt) && backFmt == fmt) {
			backdrop = ToWorld(*background.frame, false);
			back = &backdrop;
		}
	}
	if (!shape.held) s.noGlass = true;
	const int letters = PremiereRowByLabel("Liquid letters");
	// shortcut: alpha carries no text/graphic type; keep fusion conservative until Premiere exposes source metadata.
	if (letters > 0 && kRows[letters - 1].kind == K_CHECK && !params[letters]->u.bd.value) s.core[20] = 0;
	if (s.noGlass || (s.clipAlpha && !alpha && !back))
		PF_SPRINTF(out_data->return_msg, "Liquid Glass: use an adjustment layer, hide the source track output and select it in Glass from. No backdrop picker is needed.");
	Scratch& scratch = ThreadScratch();
	int result = RenderPremiereFrame(fmt, s, input, out, alpha, back, s.clipAlpha, scratch);
	TrimScratch(scratch);
	return result ? PF_Err_OUT_OF_MEMORY : PF_Err_NONE;
}

static PF_Err Render(PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[], PF_LayerDef* output)
{
	if (in_data->appl_id == kAppID_Premiere) return RenderPremiere(in_data, out_data, params, output);
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
	result = PF_REGISTER_EFFECT_EXT2(inPtr, inPluginDataCallBackPtr, LG_NAME, LG_MATCH, "CGSudo", AE_RESERVED_INFO, "EffectMain",
		"https://github.com/CGSudo/liquid-glass-for-adobe");
	return result;
}

extern "C" DllExport PF_Err EffectMain(PF_Cmd cmd, PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[], PF_LayerDef* output, void* extra)
{
	try {
		switch (cmd) {
		case PF_Cmd_ABOUT: return About(in_data, out_data);
		case PF_Cmd_GLOBAL_SETUP: return GlobalSetup(in_data, out_data);
		case PF_Cmd_SEQUENCE_SETUP: return SequenceSetup(in_data, out_data);
		case PF_Cmd_PARAMS_SETUP: return ParamsSetup(in_data, out_data);
		case PF_Cmd_USER_CHANGED_PARAM: return UserChangedParam(in_data, out_data, params, (const PF_UserChangedParamExtra*)extra);
		case PF_Cmd_EVENT: return studio_ui::Event(in_data, out_data, params, (PF_EventExtra*)extra);
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
