// Shared by the PiPL and the entry point. build.ps1 -Test makes a separate effect, so it can sit beside the installed one.
#pragma once
#if defined(LG_BRIGHTNESS) && defined(LG_TEST)
#define LG_NAME "Liquid Glass Brightness (test)"
#define LG_MATCH "CGSudo Glass Brightness Test" // After Effects cuts match names at 31 characters
#elif defined(LG_BRIGHTNESS)
#define LG_NAME "Liquid Glass Brightness"
#define LG_MATCH "CGSudo Liquid Glass Brightness"
#elif defined(LG_TEST)
#define LG_NAME "Liquid Glass (test)"
#define LG_MATCH "CGSudo Liquid Glass Test"
#else
#define LG_NAME "Liquid Glass"
#define LG_MATCH "CGSudo Liquid Glass"
#endif
