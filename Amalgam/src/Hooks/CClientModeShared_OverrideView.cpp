#include "../SDK/SDK.h"

#include "../Features/Visuals/Visuals.h"
#include "../Features/Misc/SpectateAll/SpectateAll.h"

MAKE_HOOK(CClientModeShared_OverrideView, U::Memory.GetVirtual(I::ClientModeShared, 16), void,
	void* rcx, CViewSetup* pView)
{
	DEBUG_RETURN(CClientModeShared_OverrideView, rcx, pView);

	CALL_ORIGINAL(rcx, pView);

	if (auto pLocal = H::Entities.GetLocal(); pLocal && pView)
	{
		F::SpectateAll.OverrideView(pView);
		F::Visuals.FOV(pLocal, pView);
		F::Visuals.ThirdPerson(pLocal, pView);
	}
}