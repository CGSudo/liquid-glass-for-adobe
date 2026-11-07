// Optional Premiere capabilities: a missing suite leaves the native controls and pass-through available.
#pragma once
#include "GlassPremiere.h"

inline bool PremiereHasSuite(PF_InData* in, const char* name, int version)
{
	const void* suite = nullptr;
	if (!in->pica_basicP || in->pica_basicP->AcquireSuite(name, version, &suite)) return false;
	in->pica_basicP->ReleaseSuite(name, version);
	return suite != nullptr;
}

inline bool PremiereStudioAvailable(PF_InData* in)
{
	return PremiereHasSuite(in, kPFEffectCustomUISuite, kPFEffectCustomUISuiteVersion1) &&
		PremiereHasSuite(in, kPFAppSuite, kPFAppSuiteVersion4) &&
		PremiereHasSuite(in, kDRAWBOT_DrawSuite, kDRAWBOT_DrawSuite_VersionCurrent) &&
		PremiereHasSuite(in, kDRAWBOT_SupplierSuite, kDRAWBOT_SupplierSuite_VersionCurrent) &&
		PremiereHasSuite(in, kDRAWBOT_SurfaceSuite, kDRAWBOT_SurfaceSuite_VersionCurrent) &&
		PremiereHasSuite(in, kDRAWBOT_PathSuite, kDRAWBOT_PathSuite_VersionCurrent);
}

inline void PremiereSetup(PF_InData* in, PF_OutData* out)
{
	if (!PremiereStudioAvailable(in)) out->out_flags &= ~PF_OutFlag_CUSTOM_UI;
	const PF_UtilitySuite4* util = nullptr;
	if (!in->pica_basicP->AcquireSuite(kPFUtilitySuite, kPFUtilitySuiteVersion4, (const void**)&util)) {
		if (util) util->EffectWantsCheckedOutFramesToMatchRenderPixelFormat(in->effect_ref);
		in->pica_basicP->ReleaseSuite(kPFUtilitySuite, kPFUtilitySuiteVersion4);
	}
	const PF_BackgroundFrameSuite1* bg = nullptr;
	if (!in->pica_basicP->AcquireSuite(kPFBackgroundFrameSuite, kPFBackgroundFrameSuiteVersion1, (const void**)&bg)) {
		if (bg) {
			bg->AddSupportedBackgroundTransferMode(in->effect_ref, PF_Xfer_IN_FRONT, PrPixelFormat_BGRA_4444_8u);
			bg->AddSupportedBackgroundTransferMode(in->effect_ref, PF_Xfer_IN_FRONT, PrPixelFormat_BGRA_4444_32f);
		}
		in->pica_basicP->ReleaseSuite(kPFBackgroundFrameSuite, kPFBackgroundFrameSuiteVersion1);
	}
}

struct PremiereBackground {
	PF_InData* in;
	const PF_BackgroundFrameSuite1* suite = nullptr;
	PF_EffectWorld* frame = nullptr;
	bool acquired = false;
	explicit PremiereBackground(PF_InData* input) : in(input)
	{
		acquired = !in->pica_basicP->AcquireSuite(kPFBackgroundFrameSuite, kPFBackgroundFrameSuiteVersion1, (const void**)&suite);
		PF_TransferMode mode = PF_Xfer_IN_FRONT;
		// shortcut: background delivery depends on Premiere's render pipeline; verify_pr.ps1 gates the direct-clip workflow.
		if (!acquired || !suite || suite->GetBackgroundFrame(in->effect_ref, &frame, &mode) || mode != PF_Xfer_IN_FRONT) frame = nullptr;
	}
	~PremiereBackground()
	{
		if (acquired) in->pica_basicP->ReleaseSuite(kPFBackgroundFrameSuite, kPFBackgroundFrameSuiteVersion1);
	}
};
