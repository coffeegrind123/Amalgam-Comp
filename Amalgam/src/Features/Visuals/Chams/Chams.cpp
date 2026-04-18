#include "Chams.h"

#include "../Groups/Groups.h"
#include "../Materials/Materials.h"
#include "../FakeAngle/FakeAngle.h"
#include "../../Backtrack/Backtrack.h"
<<<<<<< HEAD
#include "../../Players/PlayerUtils.h"
#include "../../Simulation/ProjectileSimulation/ProjectileSimulation.h"
#include "../StickyESP/StickyESP.h"
#include "../SentryESP/SentryESP.h"
#include "../StickyCam/StickyCam.h"
#include "../FocusFire/FocusFire.h"
=======
>>>>>>> upstream/master

void CChams::Begin()
{
	m_tOriginalColor = I::RenderView->GetColorModulation();
	m_flOriginalBlend = I::RenderView->GetBlend();
	I::ModelRender->GetMaterialOverride(&m_pOriginalMaterial, &m_iOriginalOverride);
}
void CChams::End()
{
	I::RenderView->SetColorModulation(m_tOriginalColor);
	I::RenderView->SetBlend(m_flOriginalBlend);
	I::ModelRender->ForcedMaterialOverride(m_pOriginalMaterial, m_iOriginalOverride);
}

void CChams::DrawModel(CBaseEntity* pEntity, Chams_t& tChams, IMatRenderContext* pRenderContext, bool bTwoModels)
{
	const auto& vVisibleMaterials = !tChams.Visible.empty() ? tChams.Visible : std::vector<std::pair<std::string, Color_t>> { { "None", {} } };
	const auto& vOccludedMaterials = !tChams.Occluded.empty() ? tChams.Occluded : std::vector<std::pair<std::string, Color_t>> { { "None", {} } };

	Begin();
	if (bTwoModels)
	{
<<<<<<< HEAD
	// player chams
	case ETFClassID::CTFPlayer:
	{
		// Check for StickyCam chams first (players in sticky radius)
		if (Vars::Competitive::StickyCam::ShowChams.Value && 
			!F::StickyCam.m_mEntities.empty() &&
			F::StickyCam.m_mEntities.count(pEntity->entindex()))
		{
			// Use red chams for players in sticky damage radius
			auto visibleMaterial = std::vector<std::pair<std::string, Color_t>>{ { "Flat", Color_t(255, 0, 0, 255) } };
			auto occludedMaterial = std::vector<std::pair<std::string, Color_t>>{ { "Flat", Color_t(255, 0, 0, 150) } };
			*pChams = Chams_t(visibleMaterial, occludedMaterial);
			return true;
		}
		
		// Check for FocusFire chams
		if (Vars::Competitive::FocusFire::EnableChams.Value && 
			!F::FocusFire.m_mEntities.empty() &&
			F::FocusFire.m_mEntities.count(pEntity->entindex()))
		{
			// Use proper visible and occluded materials for through-wall visibility
			auto visibleMaterial = std::vector<std::pair<std::string, Color_t>>{ { "Flat", Vars::Competitive::FocusFire::Color.Value } };
			auto occludedMaterial = std::vector<std::pair<std::string, Color_t>>{ { "Flat", Vars::Competitive::FocusFire::Color.Value } };
			*pChams = Chams_t(visibleMaterial, occludedMaterial);
			return true;
		}
		
		return GetPlayerChams(pEntity, pEntity, pLocal, pChams, Vars::Chams::Enemy::Players.Value, Vars::Chams::Team::Players.Value);
	}
	// building chams
	case ETFClassID::CObjectSentrygun:
	{
		// Special chams for sentries targeting the local player (following StickyESP pattern)
		if (Vars::Competitive::SentryESP::ShowChams.Value && 
			!F::SentryESP.m_mEntities.empty() &&
			F::SentryESP.m_mEntities.count(pEntity->entindex()))
		{
			// Use proper visible and occluded materials for through-wall visibility
			auto visibleMaterial = std::vector<std::pair<std::string, Color_t>>{ { "Flat", Color_t(255, 0, 0, 255) } };
			auto occludedMaterial = std::vector<std::pair<std::string, Color_t>>{ { "Flat", Color_t(255, 0, 0, 150) } };
			*pChams = Chams_t(visibleMaterial, occludedMaterial);
			return true;
		}
		
		auto pOwner = pEntity->As<CBaseObject>()->m_hBuilder().Get();
		if (!pOwner) pOwner = pEntity;
		return GetPlayerChams(pOwner, pEntity, pLocal, pChams, Vars::Chams::Enemy::Buildings.Value, Vars::Chams::Team::Buildings.Value);
	}
	case ETFClassID::CObjectDispenser:
	case ETFClassID::CObjectTeleporter:
	{
		auto pOwner = pEntity->As<CBaseObject>()->m_hBuilder().Get();
		if (!pOwner) pOwner = pEntity;
		return GetPlayerChams(pOwner, pEntity, pLocal, pChams, Vars::Chams::Enemy::Buildings.Value, Vars::Chams::Team::Buildings.Value);
	}
	// projectile chams
	case ETFClassID::CBaseGrenade:
	case ETFClassID::CTFWeaponBaseGrenadeProj:
	case ETFClassID::CTFWeaponBaseMerasmusGrenade:
	case ETFClassID::CTFGrenadePipebombProjectile:
	case ETFClassID::CTFStunBall:
	case ETFClassID::CTFBall_Ornament:
	case ETFClassID::CTFProjectile_Jar:
	case ETFClassID::CTFProjectile_Cleaver:
	case ETFClassID::CTFProjectile_JarGas:
	case ETFClassID::CTFProjectile_JarMilk:
	case ETFClassID::CTFProjectile_SpellBats:
	case ETFClassID::CTFProjectile_SpellKartBats:
	case ETFClassID::CTFProjectile_SpellMeteorShower:
	case ETFClassID::CTFProjectile_SpellMirv:
	case ETFClassID::CTFProjectile_SpellPumpkin:
	case ETFClassID::CTFProjectile_SpellSpawnBoss:
	case ETFClassID::CTFProjectile_SpellSpawnHorde:
	case ETFClassID::CTFProjectile_SpellSpawnZombie:
	case ETFClassID::CTFProjectile_SpellTransposeTeleport:
	case ETFClassID::CTFProjectile_Throwable:
	case ETFClassID::CTFProjectile_ThrowableBreadMonster:
	case ETFClassID::CTFProjectile_ThrowableBrick:
	case ETFClassID::CTFProjectile_ThrowableRepel:
	case ETFClassID::CTFBaseRocket:
	case ETFClassID::CTFFlameRocket:
	case ETFClassID::CTFProjectile_Arrow:
	case ETFClassID::CTFProjectile_GrapplingHook:
	case ETFClassID::CTFProjectile_HealingBolt:
	case ETFClassID::CTFProjectile_Rocket:
	case ETFClassID::CTFProjectile_BallOfFire:
	case ETFClassID::CTFProjectile_MechanicalArmOrb:
	case ETFClassID::CTFProjectile_SentryRocket:
	case ETFClassID::CTFProjectile_SpellFireball:
	case ETFClassID::CTFProjectile_SpellLightningOrb:
	case ETFClassID::CTFProjectile_SpellKartOrb:
	case ETFClassID::CTFProjectile_EnergyBall:
	case ETFClassID::CTFProjectile_Flare:
	case ETFClassID::CTFBaseProjectile:
	case ETFClassID::CTFProjectile_EnergyRing:
	//case ETFClassID::CTFProjectile_Syringe:
	{
		// Check for StickyESP chams first for stickybombs
		if (pEntity->GetClassID() == ETFClassID::CTFGrenadePipebombProjectile &&
			Vars::Competitive::StickyESP::EnableChams.Value && 
			!F::StickyESP.m_mEntities.empty() &&
			F::StickyESP.m_mEntities.count(pEntity->entindex()))
		{
			// Determine team and use appropriate colors
			bool isEnemy = pEntity->m_iTeamNum() != pLocal->m_iTeamNum();
			Color_t visibleColor = isEnemy ? Vars::Competitive::StickyESP::EnemyVisibleColor.Value : Vars::Competitive::StickyESP::TeamVisibleColor.Value;
			Color_t invisibleColor = isEnemy ? Vars::Competitive::StickyESP::EnemyInvisibleColor.Value : Vars::Competitive::StickyESP::TeamInvisibleColor.Value;
			
			auto visibleMaterial = std::vector<std::pair<std::string, Color_t>>{ { "Flat", visibleColor } };
			auto occludedMaterial = std::vector<std::pair<std::string, Color_t>>{ { "Flat", invisibleColor } };
			*pChams = Chams_t(visibleMaterial, occludedMaterial);
			return true;
		}
		
		auto pOwner = F::ProjSim.GetEntities(pEntity).second->As<CBaseEntity>();
		if (!pOwner) pOwner = pEntity;
		return GetPlayerChams(pOwner, pEntity, pLocal, pChams, Vars::Chams::Enemy::Projectiles.Value, Vars::Chams::Team::Projectiles.Value);
	}
	// ragdoll chams
	case ETFClassID::CTFRagdoll:
	case ETFClassID::CRagdollProp:
	case ETFClassID::CRagdollPropAttached:
	{
		auto pOwner = pEntity->As<CTFRagdoll>()->m_hPlayer().Get();
		if (!pOwner) pOwner = pEntity;
		return GetPlayerChams(pOwner, pEntity, pLocal, pChams, Vars::Chams::Enemy::Ragdolls.Value, Vars::Chams::Team::Ragdolls.Value);
	}
	// objective chams
	case ETFClassID::CCaptureFlag:
		*pChams = Chams_t(Vars::Chams::World::Visible.Value, Vars::Chams::World::Occluded.Value);
		return Vars::Chams::World::Objective.Value;
	// npc chams
	case ETFClassID::CEyeballBoss:
	case ETFClassID::CHeadlessHatman:
	case ETFClassID::CMerasmus:
	case ETFClassID::CTFBaseBoss:
	case ETFClassID::CTFTankBoss:
	case ETFClassID::CZombie:
		*pChams = Chams_t(Vars::Chams::World::Visible.Value, Vars::Chams::World::Occluded.Value);
		return Vars::Chams::World::NPCs.Value;
	// pickup chams
	case ETFClassID::CTFAmmoPack:
	case ETFClassID::CCurrencyPack:
	case ETFClassID::CHalloweenGiftPickup:
		*pChams = Chams_t(Vars::Chams::World::Visible.Value, Vars::Chams::World::Occluded.Value);
		return Vars::Chams::World::Pickups.Value;
	case ETFClassID::CBaseAnimating:
	{
		if (H::Entities.IsAmmo(H::Entities.GetModel(pEntity->entindex())) || H::Entities.IsHealth(H::Entities.GetModel(pEntity->entindex())))
		{
			*pChams = Chams_t(Vars::Chams::World::Visible.Value, Vars::Chams::World::Occluded.Value);
			return Vars::Chams::World::Pickups.Value;
		}
		else if (H::Entities.IsPowerup(H::Entities.GetModel(pEntity->entindex())))
		{
			*pChams = Chams_t(Vars::Chams::World::Visible.Value, Vars::Chams::World::Occluded.Value);
			return Vars::Chams::World::Powerups.Value;
		}
		else if (H::Entities.IsSpellbook(H::Entities.GetModel(pEntity->entindex())))
		{
			*pChams = Chams_t(Vars::Chams::World::Visible.Value, Vars::Chams::World::Occluded.Value);
			return Vars::Chams::World::Halloween.Value;
		}
		break;
	}
	// bomb chams
	case ETFClassID::CTFPumpkinBomb:
	case ETFClassID::CTFGenericBomb:
		*pChams = Chams_t(Vars::Chams::World::Visible.Value, Vars::Chams::World::Occluded.Value);
		return Vars::Chams::World::Bombs.Value;
	case ETFClassID::CTFMedigunShield:
		return false;
	}


	// player chams
	auto pOwner = pEntity->m_hOwnerEntity().Get();
	if (pOwner && pOwner->IsPlayer())
		return GetPlayerChams(pOwner, pOwner, pLocal, pChams, Vars::Chams::Enemy::Players.Value, Vars::Chams::Team::Players.Value);

	return false;
}

void CChams::StencilBegin(IMatRenderContext* pRenderContext, bool bTwoModels)
{
	if (!bTwoModels)
		return;
	
	pRenderContext->SetStencilEnable(true);
}
void CChams::StencilVisible(IMatRenderContext* pRenderContext, bool bTwoModels)
{
	if (!bTwoModels)
		return;

	pRenderContext->ClearBuffers(false, false, false);
	pRenderContext->SetStencilCompareFunction(STENCILCOMPARISONFUNCTION_ALWAYS);
	pRenderContext->SetStencilPassOperation(STENCILOPERATION_REPLACE);
	pRenderContext->SetStencilFailOperation(STENCILOPERATION_KEEP);
	pRenderContext->SetStencilZFailOperation(STENCILOPERATION_KEEP);
	pRenderContext->SetStencilReferenceValue(1);
	pRenderContext->SetStencilWriteMask(0xFF);
	pRenderContext->SetStencilTestMask(0x0);
}
void CChams::StencilOccluded(IMatRenderContext* pRenderContext)
{
	pRenderContext->ClearBuffers(false, false, false);
	pRenderContext->SetStencilCompareFunction(STENCILCOMPARISONFUNCTION_EQUAL);
	pRenderContext->SetStencilPassOperation(STENCILOPERATION_KEEP);
	pRenderContext->SetStencilFailOperation(STENCILOPERATION_KEEP);
	pRenderContext->SetStencilZFailOperation(STENCILOPERATION_KEEP);
	pRenderContext->SetStencilReferenceValue(0);
	pRenderContext->SetStencilWriteMask(0x0);
	pRenderContext->SetStencilTestMask(0xFF);
	pRenderContext->DepthRange(0.f, 0.2f);
}
void CChams::StencilEnd(IMatRenderContext* pRenderContext, bool bTwoModels)
{
	if (!bTwoModels)
		return;

	pRenderContext->SetStencilEnable(false);
	pRenderContext->DepthRange(0.f, 1.f);
}

void CChams::DrawModel(CBaseEntity* pEntity, Chams_t& tChams, IMatRenderContext* pRenderContext, bool bExtra)
{
	auto vVisibleMaterials = tChams.Visible.size() ? tChams.Visible : std::vector<std::pair<std::string, Color_t>> { { "None", {} } };
	auto vOccludedMaterials = tChams.Occluded.size() ? tChams.Occluded : std::vector<std::pair<std::string, Color_t>> { { "None", {} } };

	StencilBegin(pRenderContext, !bExtra);

	StencilVisible(pRenderContext, !bExtra);
=======
		pRenderContext->SetStencilEnable(true);

		pRenderContext->ClearBuffers(false, false, false);
		pRenderContext->SetStencilCompareFunction(STENCILCOMPARISONFUNCTION_ALWAYS);
		pRenderContext->SetStencilPassOperation(STENCILOPERATION_REPLACE);
		pRenderContext->SetStencilFailOperation(STENCILOPERATION_KEEP);
		pRenderContext->SetStencilZFailOperation(STENCILOPERATION_KEEP);
		pRenderContext->SetStencilReferenceValue(1);
		pRenderContext->SetStencilWriteMask(0xFF);
		pRenderContext->SetStencilTestMask(0x0);
	}
>>>>>>> upstream/master
	for (auto& [sName, tColor] : vVisibleMaterials)
	{
		auto pMaterial = F::Materials.GetMaterial(FNV1A::Hash32(sName.c_str()));

		F::Materials.SetColor(pMaterial, tColor);
		I::ModelRender->ForcedMaterialOverride(pMaterial ? pMaterial->m_pMaterial : nullptr);
		if (pMaterial && pMaterial->m_bInvertCull)
			pRenderContext->CullMode(MATERIAL_CULLMODE_CW);

		m_bRendering = true;
		pEntity->DrawModel(STUDIO_RENDER);
		m_bRendering = false;

		if (pMaterial && pMaterial->m_bInvertCull)
			pRenderContext->CullMode(MATERIAL_CULLMODE_CCW);
	}
	if (bTwoModels)
	{
		pRenderContext->ClearBuffers(false, false, false);
		pRenderContext->SetStencilCompareFunction(STENCILCOMPARISONFUNCTION_EQUAL);
		pRenderContext->SetStencilPassOperation(STENCILOPERATION_KEEP);
		pRenderContext->SetStencilFailOperation(STENCILOPERATION_KEEP);
		pRenderContext->SetStencilZFailOperation(STENCILOPERATION_KEEP);
		pRenderContext->SetStencilReferenceValue(0);
		pRenderContext->SetStencilWriteMask(0x0);
		pRenderContext->SetStencilTestMask(0xFF);
		pRenderContext->DepthRange(0.f, 0.2f);

		for (auto& [sName, tColor] : vOccludedMaterials)
		{
			auto pMaterial = F::Materials.GetMaterial(FNV1A::Hash32(sName.c_str()));

			F::Materials.SetColor(pMaterial, tColor);
			I::ModelRender->ForcedMaterialOverride(pMaterial ? pMaterial->m_pMaterial : nullptr);
			if (pMaterial && pMaterial->m_bInvertCull)
				pRenderContext->CullMode(MATERIAL_CULLMODE_CW);

			m_bRendering = true;
			pEntity->DrawModel(STUDIO_RENDER);
			m_bRendering = false;

			if (pMaterial && pMaterial->m_bInvertCull)
				pRenderContext->CullMode(MATERIAL_CULLMODE_CCW);
		}

		pRenderContext->SetStencilEnable(false);
		pRenderContext->DepthRange(0.f, 1.f);

		m_mEntities[pEntity->entindex()];
	}
	End();
}



void CChams::Store(CTFPlayer* pLocal)
{
	m_vEntities.clear();
	if (!pLocal || !F::Groups.GroupsActive())
		return;

	for (auto& [pEntity, pGroup] : F::Groups.GetGroup())
	{
		if (pEntity->IsDormant() || !pEntity->ShouldDraw())
			continue;

		if (pGroup->m_tChams()
			&& SDK::IsOnScreen(pEntity, pEntity->IsBaseCombatWeapon() || pEntity->IsWearable()))
			m_vEntities.emplace_back(pEntity, pGroup->m_tChams);

		if (pEntity->IsPlayer() && pEntity != pLocal && pGroup->m_iBacktrack & BacktrackEnum::Enabled && !pGroup->m_vBacktrackChams.empty()
			&& (F::Backtrack.GetFakeLatency() || F::Backtrack.GetFakeInterp() > G::Lerp || F::Backtrack.GetWindow()))
		{	// backtrack
			auto pWeapon = H::Entities.GetWeapon();
			if (pWeapon && (pGroup->m_iBacktrack & BacktrackEnum::Always || G::PrimaryWeaponType != EWeaponType::PROJECTILE))
			{
				bool bShowFriendly = false, bShowEnemy = true;
				if (G::PrimaryWeaponType == EWeaponType::MELEE && SDK::AttribHookValue(0, "speed_buff_ally", pWeapon) > 0)
					bShowFriendly = true;
				else if (pWeapon->GetWeaponID() == TF_WEAPON_MEDIGUN)
					bShowFriendly = true, bShowEnemy = false;

				if (bShowEnemy && pEntity->m_iTeamNum() != pLocal->m_iTeamNum() || bShowFriendly && pEntity->m_iTeamNum() == pLocal->m_iTeamNum())
					m_vEntities.emplace_back(pEntity, Chams_t(pGroup->m_vBacktrackChams, {}), pGroup->m_iBacktrack);
			}
		}
	}

	Group_t* pGroup = nullptr;
	if (F::FakeAngle.bDrawChams && F::FakeAngle.bBonesSetup
		&& F::Groups.GetGroup(TargetsEnum::FakeAngle, pGroup) && pGroup->m_tChams(true))
	{	// fakeangle
		m_vEntities.emplace_back(pLocal, pGroup->m_tChams, 1);
	}
}

void CChams::RenderMain()
{
	auto pRenderContext = I::MaterialSystem->GetRenderContext();
	if (!pRenderContext)
		return;

	for (auto& tInfo : m_vEntities)
	{
		if (!tInfo.m_iFlags)
			DrawModel(tInfo.m_pEntity, tInfo.m_tChams, pRenderContext);
		else
		{
			m_iFlags = tInfo.m_iFlags;

			auto pPlayer = tInfo.m_pEntity->As<CTFPlayer>();
			const float flOldInvisibility = pPlayer->m_flInvisibility();
			pPlayer->m_flInvisibility() = 0.f;
			DrawModel(tInfo.m_pEntity, tInfo.m_tChams, pRenderContext, false);
			pPlayer->m_flInvisibility() = flOldInvisibility;

			m_iFlags = false;
		}
	}
}

void CChams::RenderBacktrack(const DrawModelState_t& pState, const ModelRenderInfo_t& pInfo)
{
	auto pRenderContext = I::MaterialSystem->GetRenderContext();
	if (!pRenderContext)
		return;

	auto pEntity = I::ClientEntityList->GetClientEntity(pInfo.entity_index)->As<CTFPlayer>();
	if (!pEntity || !pEntity->IsPlayer())
		return;

	std::vector<TickRecord*> vRecords = {};
	if (!F::Backtrack.GetRecords(pEntity, vRecords))
		return;
	vRecords = F::Backtrack.GetValidRecords(vRecords);
	if (!vRecords.size())
		return;

	bool bDrawLast = m_iFlags & BacktrackEnum::Last;
	bool bDrawFirst = m_iFlags & BacktrackEnum::First;

	pRenderContext->DepthRange(0.f, m_iFlags & BacktrackEnum::IgnoreZ ? 0.2f : 1.f);

	auto drawModel = [&](Vec3& vOrigin, const DrawModelState_t& pState, const ModelRenderInfo_t& pInfo, matrix3x4* pBoneToWorld, float flBlend)
		{
			if (!SDK::IsOnScreen(pEntity, vOrigin))
				return;

			float flOriginalBlend = I::RenderView->GetBlend();
			I::RenderView->SetBlend(flBlend * flOriginalBlend);
			static auto IVModelRender_DrawModelExecute = U::Hooks.m_mHooks["IVModelRender_DrawModelExecute"];
			IVModelRender_DrawModelExecute->Call<void>(I::ModelRender, pState, pInfo, pBoneToWorld);
			I::RenderView->SetBlend(flOriginalBlend);
		};
	if (!bDrawLast && !bDrawFirst)
	{
		for (auto pRecord : vRecords)
		{
			if (float flBlend = Math::RemapVal(pEntity->GetAbsOrigin().DistTo(pRecord->m_vOrigin), 1.f, 24.f, 0.f, 1.f))
				drawModel(pRecord->m_vOrigin, pState, pInfo, pRecord->m_aBones, flBlend);
		}
	}
	else
	{
		if (bDrawLast)
		{
			auto pRecord = vRecords.back();
			if (float flBlend = Math::RemapVal(pEntity->GetAbsOrigin().DistTo(pRecord->m_vOrigin), 1.f, 24.f, 0.f, 1.f))
				drawModel(pRecord->m_vOrigin, pState, pInfo, pRecord->m_aBones, flBlend);
		}
		if (bDrawFirst)
		{
			auto pRecord = vRecords.front();
			if (float flBlend = Math::RemapVal(pEntity->GetAbsOrigin().DistTo(pRecord->m_vOrigin), 1.f, 24.f, 0.f, 1.f))
				drawModel(pRecord->m_vOrigin, pState, pInfo, pRecord->m_aBones, flBlend);
		}
	}

	pRenderContext->DepthRange(0.f, 1.f);
}
void CChams::RenderFakeAngle(const DrawModelState_t& pState, const ModelRenderInfo_t& pInfo)
{
	//auto pRenderContext = I::MaterialSystem->GetRenderContext();
	//if (!pRenderContext)
	//	return;

	//pRenderContext->DepthRange(0.f, Vars::Chams::FakeAngle::IgnoreZ.Value ? 0.2f : 1.f);

	static auto IVModelRender_DrawModelExecute = U::Hooks.m_mHooks["IVModelRender_DrawModelExecute"];
	IVModelRender_DrawModelExecute->Call<void>(I::ModelRender, pState, pInfo, F::FakeAngle.aBones);

	//pRenderContext->DepthRange(0.f, 1.f);
}
void CChams::RenderHandler(const DrawModelState_t& pState, const ModelRenderInfo_t& pInfo, matrix3x4* pBoneToWorld)
{
	if (!m_iFlags)
	{
		static auto IVModelRender_DrawModelExecute = U::Hooks.m_mHooks["IVModelRender_DrawModelExecute"];
		IVModelRender_DrawModelExecute->Call<void>(I::ModelRender, pState, pInfo, pBoneToWorld);
	}
	else
	{
		if (pInfo.entity_index != I::EngineClient->GetLocalPlayer())
			RenderBacktrack(pState, pInfo);
		else
			RenderFakeAngle(pState, pInfo);
	}
}

bool CChams::RenderViewmodel(void* ecx, int flags, int* iReturn)
{
	if (!F::Groups.GroupsActive())
		return false;

	auto pRenderContext = I::MaterialSystem->GetRenderContext();
	if (!pRenderContext)
		return false;

	Group_t* pGroup = nullptr;
	if (!F::Groups.GetGroup(TargetsEnum::ViewmodelWeapon, pGroup) || !pGroup->m_tChams(true))
		return false;

	Begin();
	for (auto& [sName, tColor] : pGroup->m_tChams.Visible)
	{
		auto pMaterial = F::Materials.GetMaterial(FNV1A::Hash32(sName.c_str()));

		F::Materials.SetColor(pMaterial, tColor);
		I::ModelRender->ForcedMaterialOverride(pMaterial ? pMaterial->m_pMaterial : nullptr);

		if (pMaterial && pMaterial->m_bInvertCull)
			pRenderContext->CullMode(G::FlipViewmodels ? MATERIAL_CULLMODE_CCW : MATERIAL_CULLMODE_CW);

		static auto CBaseAnimating_InternalDrawModel = U::Hooks.m_mHooks["CBaseAnimating_InternalDrawModel"];
		*iReturn = CBaseAnimating_InternalDrawModel->Call<int>(ecx, flags);

		if (pMaterial && pMaterial->m_bInvertCull)
			pRenderContext->CullMode(G::FlipViewmodels ? MATERIAL_CULLMODE_CW : MATERIAL_CULLMODE_CCW);
	}
	End();

	return true;
}
bool CChams::RenderViewmodel(const DrawModelState_t& pState, const ModelRenderInfo_t& pInfo, matrix3x4* pBoneToWorld)
{
	if (!F::Groups.GroupsActive())
		return false;

	auto pRenderContext = I::MaterialSystem->GetRenderContext();
	if (!pRenderContext)
		return false;

	Group_t* pGroup = nullptr;
	if (!F::Groups.GetGroup(TargetsEnum::ViewmodelHands, pGroup) || !pGroup->m_tChams(true))
		return false;

	Begin();
	for (auto& [sName, tColor] : pGroup->m_tChams.Visible)
	{
		auto pMaterial = F::Materials.GetMaterial(FNV1A::Hash32(sName.c_str()));

		F::Materials.SetColor(pMaterial, tColor);
		I::ModelRender->ForcedMaterialOverride(pMaterial ? pMaterial->m_pMaterial : nullptr);

		if (pMaterial && pMaterial->m_bInvertCull)
			pRenderContext->CullMode(G::FlipViewmodels ? MATERIAL_CULLMODE_CCW : MATERIAL_CULLMODE_CW);

		static auto IVModelRender_DrawModelExecute = U::Hooks.m_mHooks["IVModelRender_DrawModelExecute"];
		IVModelRender_DrawModelExecute->Call<void>(I::ModelRender, pState, pInfo, pBoneToWorld);

		if (pMaterial && pMaterial->m_bInvertCull)
			pRenderContext->CullMode(G::FlipViewmodels ? MATERIAL_CULLMODE_CW : MATERIAL_CULLMODE_CCW);
	}
	End();

	return true;
}