#include "../SDK/SDK.h"

#include "../Features/Visuals/Chams/Chams.h"
#include "../Features/Visuals/Glow/Glow.h"
#include "../Features/Visuals/Materials/Materials.h"
#include "../Features/Visuals/CameraWindow/CameraWindow.h"
#include "../Features/Misc/SpectateAll/SpectateAll.h"

MAKE_SIGNATURE(CBaseAnimating_DrawModel, "client.dll", "4C 8B DC 49 89 5B ? 89 54 24 ? 55 56 57 41 54 41 55 41 56 41 57 48 83 EC ? 48 8B 05 ? ? ? ? 48 8D 3D", 0x0);
MAKE_SIGNATURE(CEconEntity_DrawOverriddenViewmodel_DrawModel_Call, "client.dll", "8B 97 ? ? ? ? 85 D2 74 ? 83 FA ? 74 ? 0F B7 EA 4C 8B 05 ? ? ? ? 8B CD 49 83 C0 ? 48 C1 E1 ? 4C 03 C1 74 ? C1 EA ? 41 39 50 ? 75 ? 49 8B 18 BA", 0x0);
MAKE_SIGNATURE(CBaseAnimating_InternalDrawModel, "client.dll", "48 8B C4 55 56 48 8D 6C 24 ? 48 81 EC ? ? ? ? 44 8B 81", 0x0);

MAKE_HOOK(IVModelRender_DrawModelExecute, U::Memory.GetVirtual(I::ModelRender, 19), void,
	void* rcx, const DrawModelState_t& pState, const ModelRenderInfo_t& pInfo, matrix3x4* pBoneToWorld)
{
	DEBUG_RETURN(IVModelRender_DrawModelExecute, rcx, pState, pInfo, pBoneToWorld);

	if (I::EngineVGui->IsGameUIVisible() || SDK::CleanScreenshot()
		|| F::CameraWindow.m_bDrawing || !F::Materials.m_bLoaded || G::Unload)
		return CALL_ORIGINAL(rcx, pState, pInfo, pBoneToWorld);

	if (F::Chams.m_bRendering)
		return F::Chams.RenderHandler(pState, pInfo, pBoneToWorld);
	if (F::Glow.m_bRendering)
		return F::Glow.RenderHandler(pState, pInfo, pBoneToWorld);

	if (F::Chams.m_mEntities.contains(pInfo.entity_index))
		return;

	auto pEntity = I::ClientEntityList->GetClientEntity(pInfo.entity_index);
	
	// Check if this entity should be hidden for SpectateAll
	if (pEntity)
	{
		// Check if it's the spectated player
		if (pEntity->GetClassID() == ETFClassID::CTFPlayer)
		{
			auto pPlayer = pEntity->As<CTFPlayer>();
			if (pPlayer && F::SpectateAll.ShouldHidePlayer(pPlayer))
				return; // Don't render the spectated player
		}
		// Check if it's an entity belonging to the spectated player (weapons, cosmetics)
		else if (F::SpectateAll.ShouldHideEntity(pEntity->As<CBaseEntity>()))
		{
			return; // Don't render this entity
		}
		
		// NoHats feature - hide all cosmetics/wearables
		if (Vars::Competitive::Features::NoHats.Value)
		{
			auto classID = pEntity->GetClassID();
			if (classID == ETFClassID::CTFWearable || classID == ETFClassID::CTFWearableDemoShield)
			{
				return; // Don't render wearables/cosmetics
			}
		}
	}
	
	auto pRenderContext = I::MaterialSystem->GetRenderContext();
	if (pEntity && pRenderContext && pEntity->GetClassID() == ETFClassID::CTFViewModel)
	{
		F::Glow.RenderViewmodel(pState, pInfo, pBoneToWorld);
		if (F::Chams.RenderViewmodel(pState, pInfo, pBoneToWorld))
			return;
	}

	CALL_ORIGINAL(rcx, pState, pInfo, pBoneToWorld);
}

static bool s_bDrawingViewmodel = false;

MAKE_HOOK(CBaseAnimating_DrawModel, S::CBaseAnimating_DrawModel(), int,
	void* rcx, int flags)
{
	DEBUG_RETURN(CBaseAnimating_DrawModel, rcx, flags);

	const auto dwRetAddr = uintptr_t(_ReturnAddress());
	const auto dwDesired = S::CEconEntity_DrawOverriddenViewmodel_DrawModel_Call();

	if (dwRetAddr != dwDesired || I::EngineVGui->IsGameUIVisible() || SDK::CleanScreenshot()
		|| F::CameraWindow.m_bDrawing || !F::Materials.m_bLoaded || G::Unload)
		return CALL_ORIGINAL(rcx, flags);

	s_bDrawingViewmodel = true;
	int iReturn = CALL_ORIGINAL(rcx, flags);
	s_bDrawingViewmodel = false;
	return iReturn;
}

MAKE_HOOK(CBaseAnimating_InternalDrawModel, S::CBaseAnimating_InternalDrawModel(), int,
	void* rcx, int flags)
{
	DEBUG_RETURN(CBaseAnimating_InternalDrawModel, rcx, flags);

	if (!s_bDrawingViewmodel || !(flags & STUDIO_RENDER))
		return CALL_ORIGINAL(rcx, flags);

	auto pRenderContext = I::MaterialSystem->GetRenderContext();
	if (!pRenderContext)
		return CALL_ORIGINAL(rcx, flags);

	int iReturn;
	F::Glow.RenderViewmodel(rcx, flags);
	if (F::Chams.RenderViewmodel(rcx, flags, &iReturn))
		return iReturn;

	return CALL_ORIGINAL(rcx, 1);
}