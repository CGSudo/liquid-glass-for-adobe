// Shared by the PiPL and the entry point. build.ps1 -Test makes a separate effect, so it can sit beside the installed one.
#pragma once
#ifdef LG_TEST
#define LG_NAME "Liquid Glass (test)"
#define LG_MATCH "CGSudo Liquid Glass Test"
#else
#define LG_NAME "Liquid Glass"
#define LG_MATCH "CGSudo Liquid Glass"
#endif
