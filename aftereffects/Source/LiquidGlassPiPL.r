#include "AEConfig.h"
#include "AE_EffectVers.h"
#include "LiquidGlassNames.h"

resource 'PiPL' (16000) {
	{
		Kind { AEEffect },
		Name { LG_NAME },
		Category { "CGSudo" },
		CodeWin64X86 { "EffectMain" },
		AE_PiPL_Version { 2, 0 },
		AE_Effect_Spec_Version { PF_PLUG_IN_VERSION, PF_PLUG_IN_SUBVERS },
		/* PF_VERSION(0, 8, 0, PF_Stage_RELEASE, 0), must match GlobalSetup */
		AE_Effect_Version { 263680 },
		AE_Effect_Info_Flags { 0 },
		/* DEEP_COLOR_AWARE | I_EXPAND_BUFFER | CUSTOM_UI, must match GlobalSetup */
		AE_Effect_Global_OutFlags { 0x02008200 },
		/* PARAM_GROUP_START_COLLAPSED | SMART_RENDER | FLOAT_COLOR_AWARE | I_MIX_GUID_DEPENDENCIES | THREADED_RENDERING | I_USE_3D_CAMERA */
		AE_Effect_Global_OutFlags_2 { 0x0820140A },
		AE_Effect_Match_Name { LG_MATCH },
		AE_Reserved_Info { 8 },
		AE_Effect_Support_URL { "https://github.com/CGSudo/liquid-glass-for-adobe" }
	}
};
