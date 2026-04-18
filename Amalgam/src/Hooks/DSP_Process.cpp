#include "../SDK/SDK.h"

MAKE_SIGNATURE(DSP_Process, "engine.dll", "48 89 5C 24 ? 55 41 54 41 57 48 83 EC ? 48 63 D9", 0x0);

MAKE_HOOK(DSP_Process, S::DSP_Process(), void,
	unsigned int idsp, int* pbfront, int* pbrear, int* pbcenter, int sampleCount)
{
	DEBUG_RETURN(DSP_Process, idsp, pbfront, pbrear, pbcenter, sampleCount);

	// Always process audio normally to prevent sound disappearing issues
	// The RemoveDSP feature was causing audio loss and has been disabled
	CALL_ORIGINAL(idsp, pbfront, pbrear, pbcenter, sampleCount);
}