#include "AimbotHitscan.h"

#include "../Aimbot.h"
#include "../../Resolver/Resolver.h"
#include "../../Ticks/Ticks.h"
#include "../../Visuals/Visuals.h"
#include "../../Simulation/MovementSimulation/MovementSimulation.h"
#include "WarpPrediction/WarpPrediction.h"
#include "../../../Utils/Math/SIMDMath.h"

std::vector<Target_t> CAimbotHitscan::GetTargets(CTFPlayer* pLocal, CTFWeaponBase* pWeapon)
{
	std::vector<Target_t> vTargets;
	vTargets.reserve(64);  // Pre-allocate to prevent reallocations during aimbot execution
	const auto iSort = Vars::Aimbot::General::TargetSelection.Value;

	// CRITICAL FIX: Use pLocal->GetShootPos() directly instead of cached Ticks position
	// F::Ticks.GetShootPos() may return stale/incorrect position causing aimbot to aim from wrong location
	Vec3 vLocalPos = pLocal->GetShootPos();
	Vec3 vLocalAngles = I::EngineClient->GetViewAngles();

	{
		auto eGroupType = EGroupType::GROUP_INVALID;
		if (Vars::Aimbot::General::Target.Value & Vars::Aimbot::General::TargetEnum::Players)
			eGroupType = EGroupType::PLAYERS_ENEMIES;
		if (SDK::AttribHookValue(0, "jarate_duration", pWeapon) > 0 && Vars::Aimbot::Hitscan::Modifiers.Value & Vars::Aimbot::Hitscan::ModifiersEnum::ExtinguishTeam)
			eGroupType = EGroupType::PLAYERS_ALL;
		if (pWeapon->GetWeaponID() == TF_WEAPON_MEDIGUN)
			eGroupType = Vars::Aimbot::Healing::AutoHeal.Value ? EGroupType::PLAYERS_TEAMMATES : EGroupType::GROUP_INVALID;

			for (auto pEntity : H::Entities.GetGroup(eGroupType))
		{
			if (!pEntity || pEntity->IsDormant())
				continue;

			bool bTeammate = pEntity->m_iTeamNum() == pLocal->m_iTeamNum();
			if (F::AimbotGlobal.ShouldIgnore(pEntity, pLocal, pWeapon))
				continue;

			if (bTeammate)
			{
				if (SDK::AttribHookValue(0, "jarate_duration", pWeapon) > 0)
				{
					if (!pEntity->As<CTFPlayer>()->InCond(TF_COND_BURNING))
						continue;
				}
				else if (pWeapon->GetWeaponID() == TF_WEAPON_MEDIGUN)
				{
					if (pEntity->As<CTFPlayer>()->InCond(TF_COND_STEALTHED)
						|| Vars::Aimbot::Healing::FriendsOnly.Value && !H::Entities.IsFriend(pEntity->entindex()) && !H::Entities.InParty(pEntity->entindex()))
						continue;
				}
			}

			// PERFORMANCE: Quick FOV check using entity center BEFORE expensive SetupBones call
			// This filters out ~70-80% of targets without calling SetupBones
			Vec3 vEntityCenter = pEntity->GetCenter();
			Vec3 vEntityAngleTo = Math::CalcAngle(vLocalPos, vEntityCenter);
			float flEntityFOV = Math::CalcFov(vLocalAngles, vEntityAngleTo);

			bool AllowAnyFOV = Vars::Aimbot::General::AimFOV.Value >= 180.0f;
			if (!AllowAnyFOV && flEntityFOV > Vars::Aimbot::General::AimFOV.Value)
				continue;  // Skip expensive SetupBones for this target

			float flFOVTo; Vec3 vPos, vAngleTo;
			if (!F::AimbotGlobal.PlayerBoneInFOV(pEntity->As<CTFPlayer>(), vLocalPos, vLocalAngles, flFOVTo, vPos, vAngleTo, Vars::Aimbot::Hitscan::Hitboxes.Value))
				continue;

			float flDistTo = iSort == Vars::Aimbot::General::TargetSelectionEnum::Distance ? CSIMDMath::FastDistance(vLocalPos, vPos) : 0.f;
			vTargets.emplace_back(pEntity, TargetEnum::Player, vPos, vAngleTo, flFOVTo, flDistTo, bTeammate ? 0 : F::AimbotGlobal.GetPriority(pEntity->entindex()));
		}

		if (pWeapon->GetWeaponID() == TF_WEAPON_MEDIGUN)
			return vTargets;
	}

	if (Vars::Aimbot::General::Target.Value)
	{
		for (auto pEntity : H::Entities.GetGroup(EGroupType::BUILDINGS_ENEMIES))
		{
			if (!pEntity || pEntity->IsDormant())
				continue;
			if (F::AimbotGlobal.ShouldIgnore(pEntity, pLocal, pWeapon))
				continue;

			Vec3 vPos = pEntity->GetCenter();
			Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vPos);
			// Mutiny-style: AllowAnyFOV check for FOV >= 180
			bool AllowAnyFOV = Vars::Aimbot::General::AimFOV.Value >= 180.0f;
			float flFOVTo = Math::CalcFov(vLocalAngles, vAngleTo);
			if (!AllowAnyFOV && flFOVTo > Vars::Aimbot::General::AimFOV.Value)
				continue;

			float flDistTo = iSort == Vars::Aimbot::General::TargetSelectionEnum::Distance ? CSIMDMath::FastDistance(vLocalPos, vPos) : 0.f;
			vTargets.emplace_back(pEntity, pEntity->IsSentrygun() ? TargetEnum::Sentry : pEntity->IsDispenser() ? TargetEnum::Dispenser : TargetEnum::Teleporter, vPos, vAngleTo, flFOVTo, flDistTo);
		}
	}

	if (Vars::Aimbot::General::Target.Value & Vars::Aimbot::General::TargetEnum::Stickies)
	{
		for (auto pEntity : H::Entities.GetGroup(EGroupType::WORLD_PROJECTILES))
		{
			if (!pEntity || pEntity->IsDormant())
				continue;
			if (F::AimbotGlobal.ShouldIgnore(pEntity, pLocal, pWeapon))
				continue;

			Vec3 vPos = pEntity->m_vecOrigin();
			Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vPos);
			float flFOVTo = Math::CalcFovScaled(vLocalPos, vPos, vLocalAngles);
			if (flFOVTo > Vars::Aimbot::General::AimFOV.Value)
				continue;

			float flDistTo = iSort == Vars::Aimbot::General::TargetSelectionEnum::Distance ? CSIMDMath::FastDistance(vLocalPos, vPos) : 0.f;
			vTargets.emplace_back(pEntity, TargetEnum::Sticky, vPos, vAngleTo, flFOVTo, flDistTo);
		}
	}

	if (Vars::Aimbot::General::Target.Value & Vars::Aimbot::General::TargetEnum::NPCs)
	{
		for (auto pEntity : H::Entities.GetGroup(EGroupType::WORLD_NPC))
		{
			if (!pEntity || pEntity->IsDormant())
				continue;
			if (F::AimbotGlobal.ShouldIgnore(pEntity, pLocal, pWeapon))
				continue;

			Vec3 vPos = pEntity->GetCenter();
			Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vPos);
			// Mutiny-style: AllowAnyFOV check for FOV >= 180
			bool AllowAnyFOV = Vars::Aimbot::General::AimFOV.Value >= 180.0f;
			float flFOVTo = Math::CalcFov(vLocalAngles, vAngleTo);
			if (!AllowAnyFOV && flFOVTo > Vars::Aimbot::General::AimFOV.Value)
				continue;

			float flDistTo = iSort == Vars::Aimbot::General::TargetSelectionEnum::Distance ? CSIMDMath::FastDistance(vLocalPos, vPos) : 0.f;
			vTargets.emplace_back(pEntity, TargetEnum::NPC, vPos, vAngleTo, flFOVTo, flDistTo);
		}
	}

	if (Vars::Aimbot::General::Target.Value & Vars::Aimbot::General::TargetEnum::Bombs)
	{
		for (auto pEntity : H::Entities.GetGroup(EGroupType::WORLD_BOMBS))
		{
			if (!pEntity || pEntity->IsDormant())
				continue;
			Vec3 vPos = pEntity->GetCenter();
			Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vPos);
			// Mutiny-style: AllowAnyFOV check for FOV >= 180
			bool AllowAnyFOV = Vars::Aimbot::General::AimFOV.Value >= 180.0f;
			float flFOVTo = Math::CalcFov(vLocalAngles, vAngleTo);
			if (!AllowAnyFOV && flFOVTo > Vars::Aimbot::General::AimFOV.Value)
				continue;

			if (F::AimbotGlobal.ShouldIgnore(pEntity, pLocal, pWeapon))
				continue;

			float flDistTo = iSort == Vars::Aimbot::General::TargetSelectionEnum::Distance ? CSIMDMath::FastDistance(vLocalPos, vPos) : 0.f;
			vTargets.emplace_back(pEntity, TargetEnum::Bomb, vPos, vAngleTo, flFOVTo, flDistTo);
		}
	}

	return vTargets;
}

std::vector<Target_t> CAimbotHitscan::SortTargets(CTFPlayer* pLocal, CTFWeaponBase* pWeapon)
{
	auto vTargets = GetTargets(pLocal, pWeapon);

	F::AimbotGlobal.SortTargets(vTargets, Vars::Aimbot::General::TargetSelection.Value);
	vTargets.resize(std::min(size_t(Vars::Aimbot::General::MaxTargets.Value), vTargets.size()));
	F::AimbotGlobal.SortPriority(vTargets);
	return vTargets;
}



int CAimbotHitscan::GetHitboxPriority(int nHitbox, CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CBaseEntity* pTarget)
{
	bool bHeadshot = false;
	if (pTarget->IsPlayer())
	{
		switch (pWeapon->GetWeaponID())
		{
		case TF_WEAPON_SNIPERRIFLE:
		case TF_WEAPON_SNIPERRIFLE_DECAP:
		case TF_WEAPON_SNIPERRIFLE_CLASSIC:
		{
			auto pSniperRifle = pWeapon->As<CTFSniperRifle>();

			if (G::CanHeadshot || pLocal->InCond(TF_COND_AIMING) && (
					pSniperRifle->GetRifleType() == RIFLE_JARATE && SDK::AttribHookValue(0, "jarate_duration", pWeapon) > 0
					|| Vars::Aimbot::Hitscan::Modifiers.Value & Vars::Aimbot::Hitscan::ModifiersEnum::WaitForHeadshot
				))
				bHeadshot = true;
			break;
		}
		case TF_WEAPON_REVOLVER:
		{
			if (SDK::AttribHookValue(0, "set_weapon_mode", pWeapon) == 1
				&& (pWeapon->AmbassadorCanHeadshot() || Vars::Aimbot::Hitscan::Modifiers.Value & Vars::Aimbot::Hitscan::ModifiersEnum::WaitForHeadshot))
				bHeadshot = true;
		}
		}

		if (Vars::Aimbot::Hitscan::Hitboxes.Value & Vars::Aimbot::Hitscan::HitboxesEnum::BodyaimIfLethal && bHeadshot)
		{
			auto pPlayer = pTarget->As<CTFPlayer>();

			switch (pWeapon->GetWeaponID())
			{
			case TF_WEAPON_SNIPERRIFLE:
			case TF_WEAPON_SNIPERRIFLE_DECAP:
			case TF_WEAPON_SNIPERRIFLE_CLASSIC:
			{
				auto pSniperRifle = pWeapon->As<CTFSniperRifle>();

				int iDamage = std::ceil(std::max(pSniperRifle->m_flChargedDamage(), 50.f) * pSniperRifle->GetBodyshotMult(pPlayer));
				if (pPlayer->m_iHealth() <= iDamage)
					bHeadshot = false;
				break;
			}
			case TF_WEAPON_REVOLVER:
			{
				if (SDK::AttribHookValue(0, "set_weapon_mode", pWeapon) == 1)
				{
					float flDistTo = CSIMDMath::FastDistance(pTarget->m_vecOrigin(), pLocal->m_vecOrigin());

					float flMult = SDK::AttribHookValue(1, "mult_dmg", pWeapon);
					int iDamage = std::ceil(Math::RemapVal(flDistTo, 90.f, 900.f, 60.f, 21.f) * flMult);
					if (pPlayer->m_iHealth() <= iDamage)
						bHeadshot = false;
				}
			}
			}
		}
	}

	int iHeadPriority = bHeadshot ? 0 : 2;
	int iBodyPriority = bHeadshot ? 1 : 0;
	int iLimbPriority = 2;

	switch (pTarget->GetHitboxToBase(nHitbox))
	{
	case HITBOX_HEAD: return iHeadPriority;
	case HITBOX_SPINE0:
	case HITBOX_SPINE1:
	case HITBOX_SPINE2:
	case HITBOX_SPINE3: return iBodyPriority;
	case HITBOX_PELVIS: return iLimbPriority;
	}

	return iLimbPriority;
};

int CAimbotHitscan::CanHit(Target_t& tTarget, CTFPlayer* pLocal, CTFWeaponBase* pWeapon)
{
	if (Vars::Aimbot::General::Ignore.Value & Vars::Aimbot::General::IgnoreEnum::Unsimulated && H::Entities.GetChoke(tTarget.m_pEntity->entindex()) > Vars::Aimbot::General::TickTolerance.Value)
		return false;

	Vec3 vEyePos = pLocal->GetShootPos();
	const float flMaxRange = powf(pWeapon->GetRange(), 2.f);

	auto pModel = tTarget.m_pEntity->GetModel();
	if (!pModel) return false;
	auto pHDR = I::ModelInfoClient->GetStudiomodel(pModel);
	if (!pHDR) return false;
	auto pSet = pHDR->pHitboxSet(tTarget.m_pEntity->As<CBaseAnimating>()->m_nHitboxSet());
	if (!pSet) return false;

	std::vector<TickRecord*> vRecords = {};
	vRecords.reserve(24);  // Pre-allocate for backtrack records
	if (F::Backtrack.GetRecords(tTarget.m_pEntity, vRecords))
	{
		vRecords = F::Backtrack.GetValidRecords(vRecords, pLocal);
		if (vRecords.empty())
			return false;
	}
	else
	{
		matrix3x4 aBones[MAXSTUDIOBONES];
		if (!tTarget.m_pEntity->SetupBones(aBones, MAXSTUDIOBONES, BONE_USED_BY_ANYTHING, tTarget.m_pEntity->m_flSimulationTime()))
			return false;

		F::Backtrack.m_tRecord = { tTarget.m_pEntity->m_flSimulationTime(), tTarget.m_pEntity->m_vecOrigin(), Vec3(), Vec3(), *reinterpret_cast<BoneMatrix*>(&aBones) };
		vRecords = { &F::Backtrack.m_tRecord };
	}

	bool bPeekCheck = false;
	if (Vars::Aimbot::Hitscan::PeekAmount.Value && pWeapon->GetWeaponSpread())
	{
		switch (Vars::Aimbot::Hitscan::PeekCheck.Value)
		{
		case Vars::Aimbot::Hitscan::PeekCheckEnum::Off: break;
		case Vars::Aimbot::Hitscan::PeekCheckEnum::DoubletapOnly: bPeekCheck = F::Ticks.GetTicks(pWeapon); break;
		case Vars::Aimbot::Hitscan::PeekCheckEnum::Always: bPeekCheck = true; break;
		}
	}
	Vec3 vPeekPos = bPeekCheck ? vEyePos + pLocal->m_vecVelocity() * TICKS_TO_TIME(-Vars::Aimbot::Hitscan::PeekAmount.Value) : Vec3();

	// if we're doubletapping, we can't change viewangles so work around that
	static int iTargetBone = 0;
	Vec3* pDoubletapAngle = F::Ticks.GetShootAngle();
	if (pDoubletapAngle && tTarget.m_iTargetType == TargetEnum::Player)
	{
		std::sort(vRecords.begin(), vRecords.end(), [&](const TickRecord* a, const TickRecord* b) -> bool
			{
				Vec3 vPosA = { a->m_BoneMatrix.m_aBones[iTargetBone][0][3], a->m_BoneMatrix.m_aBones[iTargetBone][1][3], a->m_BoneMatrix.m_aBones[iTargetBone][2][3] };
				Vec3 vPosB = { a->m_BoneMatrix.m_aBones[iTargetBone][0][3], a->m_BoneMatrix.m_aBones[iTargetBone][1][3], a->m_BoneMatrix.m_aBones[iTargetBone][2][3] };
				Vec3 vAnglesA = Math::CalcAngle(vEyePos, vPosA);
				Vec3 vAnglesB = Math::CalcAngle(vEyePos, vPosB);
				return CSIMDMath::FastLength2D(pDoubletapAngle->DeltaAngle(vAnglesA)) < CSIMDMath::FastLength2D(pDoubletapAngle->DeltaAngle(vAnglesB));
			});
	}

	int iReturn = false;
	for (auto pRecord : vRecords)
	{
		bool bRunPeekCheck = bPeekCheck;

		if (pWeapon->GetWeaponID() == TF_WEAPON_LASER_POINTER)
		{
			tTarget.m_vPos = tTarget.m_pEntity->m_vecOrigin();

			// not lag compensated (i assume) so run movesim based on ping
			MoveStorage tStorage;
			F::MoveSim.Initialize(tTarget.m_pEntity, tStorage);
			if (!tStorage.m_bFailed)
			{
				for (int i = 1 - TIME_TO_TICKS(F::Backtrack.GetReal()); i <= 0; i++)
				{
					F::MoveSim.RunTick(tStorage);
					tTarget.m_vPos = tStorage.m_vPredictedOrigin;
				}
			}
			F::MoveSim.Restore(tStorage);

			float flBoneScale = std::max(Vars::Aimbot::Hitscan::BoneSizeMinimumScale.Value, Vars::Aimbot::Hitscan::PointScale.Value / 100.f);
			float flBoneSubtract = Vars::Aimbot::Hitscan::BoneSizeSubtract.Value;

			Vec3 vMins = tTarget.m_pEntity->m_vecMins();
			Vec3 vMaxs = tTarget.m_pEntity->m_vecMaxs();
			Vec3 vCheckMins = (vMins + flBoneSubtract) * flBoneScale;
			Vec3 vCheckMaxs = (vMaxs - flBoneSubtract) * flBoneScale;

			const matrix3x4 mTransform = { { 1, 0, 0, tTarget.m_vPos.x }, { 0, 1, 0, tTarget.m_vPos.y }, { 0, 0, 1, tTarget.m_vPos.z } };

			tTarget.m_vPos += tTarget.m_pEntity->GetOffset() / 2;
			if (vEyePos.DistToSqr(tTarget.m_vPos) > flMaxRange)
				break;

			if (SDK::VisPosWorld(pLocal, tTarget.m_pEntity, vEyePos, tTarget.m_vPos))
			{
				Vec3 vAngles; bool bChanged = Aim(G::CurrentUserCmd->viewangles, Math::CalcAngle(vEyePos, tTarget.m_vPos), vAngles);
				Vec3 vForward; Math::AngleVectors(vAngles, &vForward);
				float flDist = CSIMDMath::FastDistance(vEyePos, tTarget.m_vPos);

				// Mutiny-style: extend trace beyond target for better visibility
				const Vec3 vExtendedPos = vEyePos + vForward * (flDist + 40.0f);
				if (!bChanged || Math::RayToOBB(vEyePos, vForward, vCheckMins, vCheckMaxs, mTransform) && SDK::VisPos(pLocal, tTarget.m_pEntity, vEyePos, vExtendedPos))
				{
					tTarget.m_vAngleTo = vAngles;
					tTarget.m_pRecord = pRecord;
					return true;
				}
				else if (iReturn == 2 ? vAngles.DeltaAngle(G::CurrentUserCmd->viewangles).Length2D() < tTarget.m_vAngleTo.DeltaAngle(G::CurrentUserCmd->viewangles).Length2D() : true)
					tTarget.m_vAngleTo = vAngles;
				iReturn = 2;
			}

			break;
		}

		if (tTarget.m_iTargetType == TargetEnum::Player)
		{
			auto aBones = pRecord->m_BoneMatrix.m_aBones;
			if (!aBones)
				continue;

			std::vector<std::tuple<const mstudiobbox_t*, int, int>> vHitboxes;
			for (int i = 0; i < pSet->numhitboxes; i++)
			{
				if (!F::AimbotGlobal.IsHitboxValid(tTarget.m_pEntity, i, Vars::Aimbot::Hitscan::Hitboxes.Value))
					continue;

				auto pBox = pSet->pHitbox(i);
				if (!pBox) continue;

				int iPriority = GetHitboxPriority(i, pLocal, pWeapon, tTarget.m_pEntity);
				vHitboxes.emplace_back(pBox, i, iPriority);
			}
			std::sort(vHitboxes.begin(), vHitboxes.end(), [&](const auto& a, const auto& b) -> bool
				{
					return std::get<2>(a) < std::get<2>(b);
				});

			float flModelScale = tTarget.m_pEntity->As<CBaseAnimating>()->m_flModelScale();
			float flBoneScale = std::max(Vars::Aimbot::Hitscan::BoneSizeMinimumScale.Value, Vars::Aimbot::Hitscan::PointScale.Value / 100.f);
			float flBoneSubtract = Vars::Aimbot::Hitscan::BoneSizeSubtract.Value;

			auto pGameRules = I::TFGameRules();
			auto pViewVectors = pGameRules ? pGameRules->GetViewVectors() : nullptr;
			Vec3 vHullMins = (pViewVectors ? pViewVectors->m_vHullMin : Vec3(-24, -24, 0)) * flModelScale;
			Vec3 vHullMaxs = (pViewVectors ? pViewVectors->m_vHullMax : Vec3(24, 24, 82)) * flModelScale;

			const matrix3x4 mTransform = { { 1, 0, 0, pRecord->m_vOrigin.x }, { 0, 1, 0, pRecord->m_vOrigin.y }, { 0, 0, 1, pRecord->m_vOrigin.z } };

			for (auto& [pBox, iHitbox, _] : vHitboxes)
			{
				Vec3 vMins = pBox->bbmin;
				Vec3 vMaxs = pBox->bbmax;
				Vec3 vCheckMins = (vMins + flBoneSubtract / flModelScale) * flBoneScale;
				Vec3 vCheckMaxs = (vMaxs - flBoneSubtract / flModelScale) * flBoneScale;
				Vec3 vAngle; Math::MatrixAngles(aBones[pBox->bone], vAngle);

				Vec3 vOffset;
				{
					Vec3 vOrigin, vCenter;
					Math::VectorTransform({}, aBones[pBox->bone], vOrigin);
					Math::VectorTransform((vMins + vMaxs) / 2, aBones[pBox->bone], vCenter);
					vOffset = vCenter - vOrigin;
				}

				std::vector<Vec3> vPoints = { Vec3() };
				if (Vars::Aimbot::Hitscan::PointScale.Value > 0.f)
				{
					bool bTriggerbot = (Vars::Aimbot::General::AimType.Value == Vars::Aimbot::General::AimTypeEnum::Smooth
						&& !Vars::Aimbot::General::SmoothStrength.Value);

					if (!bTriggerbot)
					{
						float flScale = Vars::Aimbot::Hitscan::PointScale.Value / 100;
						Vec3 vMinsS = (vMins - vMaxs) / 2 * flScale;
						Vec3 vMaxsS = (vMaxs - vMins) / 2 * flScale;

						vPoints = {
							Vec3(),
							Vec3(vMinsS.x, vMinsS.y, vMaxsS.z),
							Vec3(vMaxsS.x, vMinsS.y, vMaxsS.z),
							Vec3(vMinsS.x, vMaxsS.y, vMaxsS.z),
							Vec3(vMaxsS.x, vMaxsS.y, vMaxsS.z),
							Vec3(vMinsS.x, vMinsS.y, vMinsS.z),
							Vec3(vMaxsS.x, vMinsS.y, vMinsS.z),
							Vec3(vMinsS.x, vMaxsS.y, vMinsS.z),
							Vec3(vMaxsS.x, vMaxsS.y, vMinsS.z)
						};
					}
				}

				for (auto& vPoint : vPoints)
				{
					Vec3 vOrigin; Math::VectorTransform(vPoint, aBones[pBox->bone], vOrigin); vOrigin += vOffset;

					if (vEyePos.DistToSqr(vOrigin) > flMaxRange)
						continue;

					if (bRunPeekCheck)
					{
						bRunPeekCheck = false;
						if (!SDK::VisPos(pLocal, tTarget.m_pEntity, vPeekPos, vOrigin))
							goto nextTick; // if we can't hit our primary hitbox, don't bother
					}

					Vec3 vAngles; bool bChanged = Aim(G::CurrentUserCmd->viewangles, Math::CalcAngle(vEyePos, vOrigin), vAngles);
					Vec3 vForward; Math::AngleVectors(vAngles, &vForward);
					float flDist = vEyePos.DistTo(vOrigin);

					if (bChanged || SDK::VisPos(pLocal, tTarget.m_pEntity, vEyePos, vOrigin))
					{
						// for the time being, no vischecks against other hitboxes
						// Mutiny-style: extend trace 40 units beyond target position for better visibility (Aimbot.cpp:112)
						const Vec3 vExtendedPos = vEyePos + vForward * (flDist + 40.0f);
						if ((!bChanged || Math::RayToOBB(vEyePos, vForward, vCheckMins, vCheckMaxs, aBones[pBox->bone], flModelScale) && SDK::VisPos(pLocal, tTarget.m_pEntity, vEyePos, vExtendedPos))
							&& Math::RayToOBB(vEyePos, vForward, vHullMins, vHullMaxs, mTransform))
						{
							iTargetBone = pBox->bone;

							tTarget.m_vAngleTo = vAngles;
							tTarget.m_pRecord = pRecord;
							tTarget.m_vPos = vOrigin;
							tTarget.m_nAimedHitbox = iHitbox;
							tTarget.m_bBacktrack = true;
							return true;
						}
						else if (bChanged && SDK::VisPos(pLocal, tTarget.m_pEntity, vEyePos, vOrigin))
						{
							if (iReturn != 2 || vAngles.DeltaAngle(G::CurrentUserCmd->viewangles).Length2D() < tTarget.m_vAngleTo.DeltaAngle(G::CurrentUserCmd->viewangles).Length2D())
								tTarget.m_vAngleTo = vAngles;
							iReturn = 2;
						}
					}
				}
			}
		}
		else
		{
			float flBoneScale = std::max(Vars::Aimbot::Hitscan::BoneSizeMinimumScale.Value, Vars::Aimbot::Hitscan::PointScale.Value / 100.f);
			float flBoneSubtract = Vars::Aimbot::Hitscan::BoneSizeSubtract.Value;

			Vec3 vMins = tTarget.m_pEntity->m_vecMins();
			Vec3 vMaxs = tTarget.m_pEntity->m_vecMaxs();
			Vec3 vCheckMins = (vMins + flBoneSubtract) * flBoneScale;
			Vec3 vCheckMaxs = (vMaxs - flBoneSubtract) * flBoneScale;

			const matrix3x4& mTransform = tTarget.m_pEntity->m_Collision()->CollisionToWorldTransform();

			std::vector<Vec3> vPoints = { Vec3() };
			//if (Vars::Aimbot::Hitscan::MultipointScale.Value > 0.f)
			{
				bool bTriggerbot = (Vars::Aimbot::General::AimType.Value == Vars::Aimbot::General::AimTypeEnum::Smooth
					&& !Vars::Aimbot::General::SmoothStrength.Value);

				if (!bTriggerbot)
				{
					float flScale = 0.5f; //Vars::Aimbot::Hitscan::MultipointScale.Value / 100;
					Vec3 vMinsS = (vMins - vMaxs) / 2 * flScale;
					Vec3 vMaxsS = (vMaxs - vMins) / 2 * flScale;

					vPoints = {
						Vec3(),
						Vec3(vMinsS.x, vMinsS.y, vMaxsS.z),
						Vec3(vMaxsS.x, vMinsS.y, vMaxsS.z),
						Vec3(vMinsS.x, vMaxsS.y, vMaxsS.z),
						Vec3(vMaxsS.x, vMaxsS.y, vMaxsS.z),
						Vec3(vMinsS.x, vMinsS.y, vMinsS.z),
						Vec3(vMaxsS.x, vMinsS.y, vMinsS.z),
						Vec3(vMinsS.x, vMaxsS.y, vMinsS.z),
						Vec3(vMaxsS.x, vMaxsS.y, vMinsS.z)
					};
				}
			}

			for (auto& vPoint : vPoints)
			{
				Vec3 vOrigin = tTarget.m_pEntity->GetCenter() + vPoint;

				if (vEyePos.DistToSqr(vOrigin) > flMaxRange)
					continue;

				Vec3 vAngles; bool bChanged = Aim(G::CurrentUserCmd->viewangles, Math::CalcAngle(vEyePos, vOrigin), vAngles);
				Vec3 vForward; Math::AngleVectors(vAngles, &vForward);
				float flDist = vEyePos.DistTo(vOrigin);

				if (bChanged || SDK::VisPos(pLocal, tTarget.m_pEntity, vEyePos, vOrigin))
				{
					// Mutiny-style: extend trace beyond target for better visibility
					const Vec3 vExtendedPos = vEyePos + vForward * (flDist + 40.0f);
					if (!bChanged || Math::RayToOBB(vEyePos, vForward, vCheckMins, vCheckMaxs, mTransform) && SDK::VisPos(pLocal, tTarget.m_pEntity, vEyePos, vExtendedPos))
					{
						tTarget.m_vAngleTo = vAngles;
						tTarget.m_pRecord = pRecord;
						tTarget.m_vPos = vOrigin;
						return true;
					}
					else if (bChanged && SDK::VisPos(pLocal, tTarget.m_pEntity, vEyePos, vOrigin))
					{
						if (iReturn != 2 || vAngles.DeltaAngle(G::CurrentUserCmd->viewangles).Length2D() < tTarget.m_vAngleTo.DeltaAngle(G::CurrentUserCmd->viewangles).Length2D())
							tTarget.m_vAngleTo = vAngles;
						iReturn = 2;
					}
				}
			}
		}

		nextTick:
		continue;
	}

	return iReturn;
}



/* Returns whether AutoShoot should fire */
bool CAimbotHitscan::ShouldFire(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd, const Target_t& tTarget)
{
	if (!Vars::Aimbot::General::AutoShoot.Value) return false;

	if (Vars::Aimbot::Hitscan::Modifiers.Value & Vars::Aimbot::Hitscan::ModifiersEnum::WaitForHeadshot)
	{
		switch (pWeapon->GetWeaponID())
		{
		case TF_WEAPON_SNIPERRIFLE:
		case TF_WEAPON_SNIPERRIFLE_DECAP:
			if (!G::CanHeadshot && pLocal->InCond(TF_COND_AIMING) && pWeapon->As<CTFSniperRifle>()->GetRifleType() != RIFLE_JARATE)
				return false;
			break;
		case TF_WEAPON_SNIPERRIFLE_CLASSIC:
			if (!G::CanHeadshot)
				return false;
			break;
		case TF_WEAPON_REVOLVER:
			if (SDK::AttribHookValue(0, "set_weapon_mode", pWeapon) == 1 && !pWeapon->AmbassadorCanHeadshot())
				return false;
		}
	}

	if (Vars::Aimbot::Hitscan::Modifiers.Value & Vars::Aimbot::Hitscan::ModifiersEnum::WaitForCharge)
	{
		switch (pWeapon->GetWeaponID())
		{
		case TF_WEAPON_SNIPERRIFLE:
		case TF_WEAPON_SNIPERRIFLE_DECAP:
		case TF_WEAPON_SNIPERRIFLE_CLASSIC:
		{
			auto pPlayer = tTarget.m_pEntity->As<CTFPlayer>();
			auto pSniperRifle = pWeapon->As<CTFSniperRifle>();

			if (!pLocal->InCond(TF_COND_AIMING) || pSniperRifle->m_flChargedDamage() == 150.f)
				break;

			if (tTarget.m_nAimedHitbox == HITBOX_HEAD && (pWeapon->GetWeaponID() != TF_WEAPON_SNIPERRIFLE_CLASSIC ? true : pSniperRifle->m_flChargedDamage() == 150.f))
			{
				int iHeadDamage = std::ceil(std::max(pSniperRifle->m_flChargedDamage(), 50.f) * pSniperRifle->GetHeadshotMult(pPlayer));
				if (pPlayer->m_iHealth() <= iHeadDamage && (G::CanHeadshot || pLocal->IsCritBoosted()))
					break;
			}
			else
			{
				int iBodyDamage = std::ceil(std::max(pSniperRifle->m_flChargedDamage(), 50.f) * pSniperRifle->GetBodyshotMult(pPlayer));
				if (pPlayer->m_iHealth() <= iBodyDamage)
					break;
			}

			return false;
		}
		}
	}

	return true;
}

void CAimbotHitscan::ClearLegitAimStepVars()
{
	m_bReachedLegitAimStepTarget = false;
	m_flLegitAimStepIncTimeOverShoot = 0.0f;
	m_flCurAimTime = 0.0f;
	m_bInitializedLegitAimStepDirection = false;
	m_nLegitAimCurveType = Vars::Aimbot::General::CurveType.Value;
}

static void SmoothAngle(const Vec3& vFrom, Vec3& vTo, float flPercent)
{
	Vec3 vDelta = vFrom - vTo;
	Math::NormalizeAngle(vDelta.x);
	Math::NormalizeAngle(vDelta.y);
	vDelta.x *= flPercent;
	vDelta.y *= flPercent;
	vTo = vFrom - vDelta;
	Math::ClampAngles(vTo);
}

static inline float RandFloatRange(float flMin, float flMax)
{
	return ((float)rand() / (float)RAND_MAX) * (flMax - flMin) + flMin;
}

bool CAimbotHitscan::Aim(Vec3 vCurAngle, Vec3 vToAngle, Vec3& vOut, int iMethod)
{
	auto pLocal = H::Entities.GetLocal();
	Vec3 vPunch = pLocal ? pLocal->m_vecPunchAngle() : Vec3();

	if (Vec3* pDoubletapAngle = F::Ticks.GetShootAngle())
	{
		vOut = *pDoubletapAngle - vPunch;
		return true;
	}

	bool bReturn = false;
	vToAngle -= vPunch;
	switch (iMethod)
	{
	case Vars::Aimbot::General::AimTypeEnum::Plain:
	case Vars::Aimbot::General::AimTypeEnum::Silent:
		vOut = vToAngle;
		break;
	case Vars::Aimbot::General::AimTypeEnum::Smooth:
	{
		// MutinyFixed-style smooth aimbot with curves and humanization
		Vec3 vOldAngles = vCurAngle;

		// Calculate delta for initialization
		Vec3 vDelta = vToAngle - vCurAngle;
		Math::NormalizeAngle(vDelta.x);
		Math::NormalizeAngle(vDelta.y);
		Math::ClampAngles(vDelta);

		// Initialize direction if not set
		if (!m_bInitializedLegitAimStepDirection)
		{
			m_LegitAimStartDirection = vDelta.y > 0 ? LEFT : RIGHT;
			m_vLegitAimStepInitialDelta = vDelta;
			m_bInitializedLegitAimStepDirection = true;
		}

		// Check if we've reached the target
		if (m_bReachedLegitAimStepTarget)
		{
			if (fabsf(vDelta.y) > 15.0f || fabsf(vDelta.x) > 15.0f)
			{
				ClearLegitAimStepVars();
			}
		}
		// Reset smooth time when target changes significantly (5° threshold)
		else if (m_vLegitAimStepInitialDelta.Length2D() > 0.1f)
		{
			Vec3 vDeltaChange = vDelta - m_vLegitAimStepInitialDelta;
			float flAngleChange = fabsf(Math::NormalizeAngle(vDeltaChange.y));
			if (flAngleChange > 5.0f)
			{
				// Full reset when target changes - must reset ALL state including m_bReachedLegitAimStepTarget
				m_flCurAimTime = 0.0f;
				m_bInitializedLegitAimStepDirection = false;
				m_bReachedLegitAimStepTarget = false;  // CRITICAL: Fix camera flicking issue
				m_vLegitAimStepInitialDelta = vDelta;
			}
		}

		// Calculate frame time
		static float flLastAimStepTime = 0.0f;
		if (flLastAimStepTime == 0.0f)
			flLastAimStepTime = I::GlobalVars->curtime;

		float flFrameTime = std::min(I::GlobalVars->curtime - flLastAimStepTime, 0.1f);
		flLastAimStepTime = I::GlobalVars->curtime;

		m_flCurAimTime += flFrameTime;

		// Calculate FOV to target
		float flFOV = std::max(0.001f, Math::CalcFov(vOldAngles, vToAngle));

		// PERFORMANCE: Pre-calculate reciprocal to avoid repeated division in hot path
		const float flFOVReciprocal = 1.0f / flFOV;

		// Calculate smooth time based on speed setting
		float flSmoothScale = std::max(0.1f, Vars::Aimbot::General::SmoothStrength.Value);
		float flSmoothTime = m_flCurAimTime * flSmoothScale + (m_bReachedLegitAimStepTarget ? 0.1f * flFOVReciprocal : 0.33f * flFOVReciprocal);

		// Apply velocity-based randomization when target reached
		if (m_bReachedLegitAimStepTarget)
		{
			auto pLocal = H::Entities.GetLocal();
			if (pLocal)
			{
				float flLocalVel = pLocal->m_vecVelocity().Length();
				if (flLocalVel > 0.0f)
				{
					flSmoothTime *= RandFloatRange(0.35f, 0.45f);
				}
			}
		}

		// Clamp smooth time to prevent instant flicks (max 90% interpolation)
		if (flSmoothTime > 0.90f)
			flSmoothTime = 0.90f;

		// Detect mouse input and reduce smoothing
		if (G::CurrentUserCmd && (abs(G::CurrentUserCmd->mousedx) > 2 || abs(G::CurrentUserCmd->mousedy) > 2))
		{
			flSmoothTime *= 0.25f;
			m_flCurAimTime = std::max(0.0f, m_flCurAimTime - (flFrameTime * 2.0f));
		}

		// Apply smoothing
		vOut = vToAngle;
		SmoothAngle(vOldAngles, vOut, flSmoothTime);

		// Apply curve-specific pitch adjustments
		Vec3 vDeltaAngle = (vOut - vOldAngles);
		Math::NormalizeAngle(vDeltaAngle.x);
		Math::NormalizeAngle(vDeltaAngle.y);

		// PERFORMANCE: Pre-calculate min value for all curve cases to avoid repeated std::min/division
		const float flFOVMult = std::min(0.0f, flFOVReciprocal);

		switch (m_nLegitAimCurveType)
		{
		case 0:
			if (vDeltaAngle.y > 0.4f)
			{
				float flInc = RandFloatRange(0.265f, 0.291f) + flFOVMult - flSmoothTime;
				if (m_vLegitAimStepInitialDelta.x < 0)
					vOut.x -= flInc;
				else
					vOut.x += flInc;
				vOut.x = std::clamp(Math::NormalizeAngle(vOut.x), -89.f, 89.f);
			}
			else if (vDeltaAngle.y < -0.4f)
			{
				float flInc = RandFloatRange(0.252f, 0.294f) + flFOVMult - flSmoothTime;
				if (m_vLegitAimStepInitialDelta.x < 0)
					vOut.x -= flInc;
				else
					vOut.x += flInc;
				vOut.x = std::clamp(Math::NormalizeAngle(vOut.x), -89.f, 89.f);
			}
			else if (vDeltaAngle.y > 0.2f)
			{
				float flInc = RandFloatRange(-0.002f, 0.0385f) + flFOVMult - flSmoothTime;
				if (m_vLegitAimStepInitialDelta.x < 0)
					vOut.x -= flInc;
				else
					vOut.x += flInc;
				vOut.x = std::clamp(Math::NormalizeAngle(vOut.x), -89.f, 89.f);
			}
			else if (vDeltaAngle.y < -0.2f)
			{
				float flInc = RandFloatRange(-0.00212f, 0.032f) + flFOVMult - flSmoothTime;
				if (m_vLegitAimStepInitialDelta.x < 0)
					vOut.x -= flInc;
				else
					vOut.x += flInc;
				vOut.x = std::clamp(Math::NormalizeAngle(vOut.x), -89.f, 89.f);
			}
			else
			{
				m_bReachedLegitAimStepTarget = true;
			}
			break;
		case 1:
			if (vDeltaAngle.y > 0.4f)
			{
				float flInc = RandFloatRange(0.265f, 0.331f) + flFOVMult - flSmoothTime;
				if (m_vLegitAimStepInitialDelta.x < 0)
					vOut.x -= flInc;
				else
					vOut.x += flInc;
				vOut.x = std::clamp(Math::NormalizeAngle(vOut.x), -89.f, 89.f);
			}
			else if (vDeltaAngle.y < -0.4f)
			{
				float flInc = RandFloatRange(0.252f, 0.324f) + flFOVMult - flSmoothTime;
				if (m_vLegitAimStepInitialDelta.x < 0)
					vOut.x -= flInc;
				else
					vOut.x += flInc;
				vOut.x = std::clamp(Math::NormalizeAngle(vOut.x), -89.f, 89.f);
			}
			else if (vDeltaAngle.y > 0.2f)
			{
				float flInc = RandFloatRange(-0.002f, 0.0385f) + flFOVMult - flSmoothTime;
				if (m_vLegitAimStepInitialDelta.x < 0)
					vOut.x -= flInc;
				else
					vOut.x += flInc;
				vOut.x = std::clamp(Math::NormalizeAngle(vOut.x), -89.f, 89.f);
			}
			else if (vDeltaAngle.y < -0.2f)
			{
				float flInc = RandFloatRange(-0.00212f, 0.032f) + flFOVMult - flSmoothTime;
				if (m_vLegitAimStepInitialDelta.x < 0)
					vOut.x -= flInc;
				else
					vOut.x += flInc;
				vOut.x = std::clamp(Math::NormalizeAngle(vOut.x), -89.f, 89.f);
			}
			else
			{
				m_bReachedLegitAimStepTarget = true;
			}
			break;
		case 2:
			if (vDeltaAngle.y > 0.4f)
			{
				float flInc = RandFloatRange(0.2f, 0.25f) + flFOVMult - flSmoothTime;
				if (m_vLegitAimStepInitialDelta.x < 0)
					vOut.x -= flInc;
				else
					vOut.x += flInc;
				vOut.x = std::clamp(Math::NormalizeAngle(vOut.x), -89.f, 89.f);
			}
			else if (vDeltaAngle.y < -0.4f)
			{
				float flInc = RandFloatRange(0.15f, 0.20f) + flFOVMult - flSmoothTime;
				if (m_vLegitAimStepInitialDelta.x < 0)
					vOut.x -= flInc;
				else
					vOut.x += flInc;
				vOut.x = std::clamp(Math::NormalizeAngle(vOut.x), -89.f, 89.f);
			}
			else if (vDeltaAngle.y > 0.2f)
			{
				float flInc = RandFloatRange(-0.005f, 0.018f) + flFOVMult - flSmoothTime;
				if (m_vLegitAimStepInitialDelta.x < 0)
					vOut.x -= flInc;
				else
					vOut.x += flInc;
				vOut.x = std::clamp(Math::NormalizeAngle(vOut.x), -89.f, 89.f);
			}
			else if (vDeltaAngle.y < -0.2f)
			{
				float flInc = RandFloatRange(-0.001f, 0.04f) + flFOVMult - flSmoothTime;
				if (m_vLegitAimStepInitialDelta.x < 0)
					vOut.x -= flInc;
				else
					vOut.x += flInc;
				vOut.x = std::clamp(Math::NormalizeAngle(vOut.x), -89.f, 89.f);
			}
			else
			{
				m_bReachedLegitAimStepTarget = true;
			}
			break;
		case 3:
			{
				// PERFORMANCE: NaturalHuman smooth aimbot optimization
				// Pre-calculate expensive operations to avoid them in hot path

				// Peak velocity at 15-25% of duration, exponential decay
				const float flTotalDist = vDeltaAngle.Length2D();
				const float flFOVFactor = flFOV * 0.1f;  // Pre-calculate for division
				const float flProgress = std::min(1.0f, flSmoothTime / std::max(0.001f, flFOVFactor));
				const float flPeakTime = 0.2f;
				float flVelocityFactor;

				if (flProgress < flPeakTime)
				{
					// Acceleration phase: exponential (t/t_peak)^1.5
					const float flProgressRatio = flProgress / flPeakTime;
					flVelocityFactor = flProgressRatio * flProgressRatio * std::sqrt(flProgressRatio);  // Fast approximation of pow(x, 1.5)
				}
				else
				{
					// Deceleration phase: exponential decay exp(-2.5 * (t - t_peak) / t_remaining)
					const float flRemaining = 1.0f - flPeakTime;
					const float flDecayProgress = (flProgress - flPeakTime) / flRemaining;
					// Fast linear approximation of exp() for small values
					const float flDecayFactor = -2.5f * flDecayProgress;
					flVelocityFactor = (flDecayFactor < -1.0f) ? 0.0f : std::max(0.0f, 1.0f + flDecayFactor * (1.0f + flDecayFactor * 0.5f));
				}

				// Apply velocity with humanization jitter
				const float flJitterPercent = Vars::Aimbot::General::HumanizationJitter.Value / 100.0f;
				const float flOvershootPercent = Vars::Aimbot::General::MicroOvershootAmount.Value / 100.0f;
				const float flJitter = RandFloatRange(-flJitterPercent, flJitterPercent) * flTotalDist;
				const float flMicroOvershoot = flProgress > 0.7f ? RandFloatRange(-flOvershootPercent, flOvershootPercent) * flTotalDist : 0.0f;
				float flPitchAdjustment = flVelocityFactor * flTotalDist * 0.5f + flJitter + flMicroOvershoot;

				if (abs(vDeltaAngle.y) > 0.01f)
				{
					vOut.x += (m_vLegitAimStepInitialDelta.x < 0 ? -1.0f : 1.0f) * flPitchAdjustment;
					vOut.x = std::clamp(Math::NormalizeAngle(vOut.x), -89.f, 89.f);
				}

				if (abs(vDeltaAngle.y) < 0.1f)
				{
					m_bReachedLegitAimStepTarget = true;
				}
				break;
			}
		}

		bReturn = true;
		break;
	}
	}

	Math::ClampAngles(vOut);
	return bReturn;
}

// assume angle calculated outside with other overload
void CAimbotHitscan::Aim(CUserCmd* pCmd, Vec3& vAngle, int iMethod)
{
	bool bUnsure = F::Ticks.IsTimingUnsure();
	switch (iMethod)
	{
	case Vars::Aimbot::General::AimTypeEnum::Plain:
		if (G::Attacking != 1 && !bUnsure)
			break;
		[[fallthrough]];
	case Vars::Aimbot::General::AimTypeEnum::Smooth:
		pCmd->viewangles = vAngle;
		I::EngineClient->SetViewAngles(vAngle);
		break;
	case Vars::Aimbot::General::AimTypeEnum::Silent:
		if (G::Attacking == 1 || bUnsure)
		{
			SDK::FixMovement(pCmd, vAngle);
			pCmd->viewangles = vAngle;
			G::SilentAngles = true;
		}
		break;
	}
}

static inline void DrawVisuals(CTFPlayer* pLocal, Target_t& tTarget, int nWeaponID)
{
	if (G::Attacking == 1 && nWeaponID != TF_WEAPON_LASER_POINTER)
	{
		bool bLine = Vars::Visuals::Line::Enabled.Value;
		bool bBoxes = Vars::Visuals::Hitbox::BonesEnabled.Value & Vars::Visuals::Hitbox::BonesEnabledEnum::OnShot;
		if (G::CanPrimaryAttack && (bLine || bBoxes))
		{
			G::LineStorage.clear();
			G::BoxStorage.clear();
			G::PathStorage.clear();

			if (bLine)
			{
				Vec3 vEyePos = pLocal->GetShootPos();
				float flDist = CSIMDMath::FastDistance(vEyePos, tTarget.m_vPos);
				Vec3 vForward; Math::AngleVectors(tTarget.m_vAngleTo + pLocal->m_vecPunchAngle(), &vForward);

				if (Vars::Colors::LineIgnoreZ.Value.a)
					G::LineStorage.emplace_back(std::pair<Vec3, Vec3>(vEyePos, vEyePos + vForward * flDist), I::GlobalVars->curtime + Vars::Visuals::Line::DrawDuration.Value, Vars::Colors::LineIgnoreZ.Value);
				if (Vars::Colors::Line.Value.a)
					G::LineStorage.emplace_back(std::pair<Vec3, Vec3>(vEyePos, vEyePos + vForward * flDist), I::GlobalVars->curtime + Vars::Visuals::Line::DrawDuration.Value, Vars::Colors::Line.Value, true);
			}
			if (bBoxes)
			{
				auto vBoxes = F::Visuals.GetHitboxes(tTarget.m_pRecord->m_BoneMatrix.m_aBones, tTarget.m_pEntity->As<CBaseAnimating>(), {}, tTarget.m_nAimedHitbox);
				G::BoxStorage.insert(G::BoxStorage.end(), vBoxes.begin(), vBoxes.end());
			}
		}
	}
}

void CAimbotHitscan::Run(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd)
{
	const int nWeaponID = pWeapon->GetWeaponID();

	static int iStaticAimType = Vars::Aimbot::General::AimType.Value;
	const int iLastAimType = iStaticAimType;
	const int iRealAimType = Vars::Aimbot::General::AimType.Value;

	switch (nWeaponID)
	{
	case TF_WEAPON_SNIPERRIFLE_CLASSIC:
		if (G::Attacking && !iRealAimType && iLastAimType)
			Vars::Aimbot::General::AimType.Value = iLastAimType;
	}
	iStaticAimType = Vars::Aimbot::General::AimType.Value;

	if (F::AimbotGlobal.ShouldHoldAttack(pWeapon))
		pCmd->buttons |= IN_ATTACK;
	if (!Vars::Aimbot::General::AimType.Value
		|| !F::AimbotGlobal.ShouldAim() && (nWeaponID != TF_WEAPON_MINIGUN || pWeapon->As<CTFMinigun>()->m_iWeaponState() == AC_STATE_FIRING || pWeapon->As<CTFMinigun>()->m_iWeaponState() == AC_STATE_SPINNING))
	{
		// Debug visualization: Still run even when aimbot is disabled
		if (Vars::Debug::AimbotDrawTargets.Value || Vars::Debug::AimbotDrawFOV.Value)
		{
			// Use pLocal directly instead of Ticks to get correct position
			if (!pLocal || !pLocal->IsAlive())
				return;

			auto vDebugTargets = SortTargets(pLocal, pWeapon);
			Vec3 vLocalPos = pLocal->GetShootPos();
			Vec3 vLocalAngles = I::EngineClient->GetViewAngles();

			if (Vars::Debug::AimbotDrawTargets.Value)
			{
				const auto& fFont = H::Fonts.GetFont(FONT_INDICATORS);

				for (const auto& tTarget : vDebugTargets)
				{
					Vec3 vScreenPos;
					if (SDK::W2S(tTarget.m_vPos, vScreenPos))
					{
						const Color_t green = { 0, 255, 0, 255 };
						const Color_t yellow = { 255, 255, 0, 255 };
						float flFOVDeg = tTarget.m_flFOVTo;
						Color_t cColor = flFOVDeg < Vars::Aimbot::General::AimFOV.Value ? green : yellow;

						const char* pszType = "Unknown";
						switch (tTarget.m_iTargetType)
						{
						case TargetEnum::Player: pszType = "Player"; break;
						case TargetEnum::Sentry: pszType = "Sentry"; break;
						case TargetEnum::Dispenser: pszType = "Dispenser"; break;
						case TargetEnum::Teleporter: pszType = "Teleporter"; break;
						case TargetEnum::NPC: pszType = "NPC"; break;
						case TargetEnum::Sticky: pszType = "Sticky"; break;
						case TargetEnum::Bomb: pszType = "Bomb"; break;
						}

						char szDebug[256];
						sprintf_s(szDebug, "%s [FOV: %.1f°] [Dist: %.0fu]", pszType, flFOVDeg, sqrtf(tTarget.m_flDistTo));
						H::Draw.String(fFont, vScreenPos.x, vScreenPos.y, cColor, ALIGN_CENTER, szDebug);

						Vec3 vLocalScreen;
						if (SDK::W2S(vLocalPos, vLocalScreen))
						{
							G::LineStorage.emplace_back(std::pair<Vec3, Vec3>(vLocalPos, tTarget.m_vPos), I::GlobalVars->curtime + 0.03f, cColor, true);
						}
					}
				}
			}

			if (Vars::Debug::AimbotDrawFOV.Value)
			{
				const float flFOV = Vars::Aimbot::General::AimFOV.Value;
				const Color_t fovColor = { 100, 100, 255, 150 };

				for (float flDist = 100.f; flDist <= 500.f; flDist += 100.f)
				{
					const int iSegments = 32;
					Vec3 vPrevWorld;

					for (int i = 0; i <= iSegments; i++)
					{
						const float flAngle = (float)i / (float)iSegments * flFOV - flFOV / 2.f;
						Vec3 vAngle = vLocalAngles;
						vAngle.y += flAngle;

						Vec3 vForward;
						Math::AngleVectors(vAngle, &vForward);
						Vec3 vWorldPos = vLocalPos + vForward * flDist;

						if (i > 0)
						{
							G::LineStorage.emplace_back(std::pair<Vec3, Vec3>(vPrevWorld, vWorldPos), I::GlobalVars->curtime + 0.03f, fovColor, false);
						}
						vPrevWorld = vWorldPos;
					}
				}
			}
		}
		return;
	}

	switch (nWeaponID)
	{
	case TF_WEAPON_MINIGUN:
		if (Vars::Aimbot::Hitscan::Modifiers.Value & Vars::Aimbot::Hitscan::ModifiersEnum::AutoRev)
			pCmd->buttons |= IN_ATTACK2;
		if (pWeapon->As<CTFMinigun>()->m_iWeaponState() != AC_STATE_FIRING && pWeapon->As<CTFMinigun>()->m_iWeaponState() != AC_STATE_SPINNING)
			return;
		break;
	}

	// PERFORMANCE: Target persistence - re-validate last target before full scan
	static Target_t* pLastTarget = nullptr;
	static int nLastTargetFrame = 0;
	static int nLastTargetIndex = 0;

	// Declare vTargets outside conditional to avoid goto issues
	std::vector<Target_t> vTargets;

	// Fast path: Re-validate existing target from previous frame
	if (pLastTarget && nLastTargetFrame == I::GlobalVars->framecount - 1)
	{
		// Check if last target is still valid
		if (pLastTarget->m_pEntity && !F::AimbotGlobal.ShouldIgnore(pLastTarget->m_pEntity, pLocal, pWeapon))
		{
			auto pPlayer = pLastTarget->m_pEntity->As<CTFPlayer>();
			if (pPlayer && pPlayer->IsAlive() && !pPlayer->IsAGhost())
			{
				// Target still valid - use cached target
				vTargets.reserve(1);
				vTargets.push_back(*pLastTarget);

				// Update FOV/Angle calculations for current frame
				Vec3 vLocalPos = pLocal->GetShootPos();
				Vec3 vLocalAngles = I::EngineClient->GetViewAngles();
				Vec3 vEntityCenter = pLastTarget->m_pEntity->GetCenter();
				vTargets[0].m_vAngleTo = Math::CalcAngle(vLocalPos, vEntityCenter);
				vTargets[0].m_flFOVTo = Math::CalcFov(vLocalAngles, vTargets[0].m_vAngleTo);

				// Don't update cache pointer since we're using cached data
				// Proceed to target processing
			}
			else
			{
				// Target invalid, do full scan
				vTargets = SortTargets(pLocal, pWeapon);
				if (vTargets.empty())
					return;

				// Update cache for next frame
				nLastTargetFrame = I::GlobalVars->framecount;
				nLastTargetIndex = vTargets[0].m_pEntity->entindex();
				pLastTarget = &vTargets[0];
			}
		}
		else
		{
			// Slow path: Full target scan
			vTargets = SortTargets(pLocal, pWeapon);
			if (vTargets.empty())
				return;

			// Update cache for next frame
			nLastTargetFrame = I::GlobalVars->framecount;
			nLastTargetIndex = vTargets[0].m_pEntity->entindex();
			pLastTarget = &vTargets[0];
		}
	}
	else
	{
		// Slow path: Full target scan
		vTargets = SortTargets(pLocal, pWeapon);
		if (vTargets.empty())
			return;

		// Update cache for next frame
		nLastTargetFrame = I::GlobalVars->framecount;
		nLastTargetIndex = vTargets[0].m_pEntity->entindex();
		pLastTarget = &vTargets[0];
	}

	// Process targets
	switch (nWeaponID)
	{
	case TF_WEAPON_SNIPERRIFLE:
	case TF_WEAPON_SNIPERRIFLE_DECAP:
	{
		bool bScoped = pLocal->InCond(TF_COND_ZOOMED);
		if (Vars::Aimbot::Hitscan::Modifiers.Value & Vars::Aimbot::Hitscan::ModifiersEnum::AutoScope && !bScoped)
		{
			pCmd->buttons |= IN_ATTACK2;
			return;
		}
		else if (Vars::Aimbot::Hitscan::Modifiers.Value & Vars::Aimbot::Hitscan::ModifiersEnum::ScopedOnly && !bScoped)
			return;
		else if (!bScoped && SDK::AttribHookValue(0, "sniper_only_fire_zoomed", pWeapon))
			return;
		break;
	}
	case TF_WEAPON_SNIPERRIFLE_CLASSIC:
		if (iRealAimType)
			pCmd->buttons |= IN_ATTACK;
	}

	// Target switch delay logic
	bool bCanSwitchTarget = true;
	float flCurrentTime = I::EngineClient->Time();

	if (G::AimTarget.m_iEntIndex != 0 && vTargets.front().m_pEntity->entindex() != G::AimTarget.m_iEntIndex)
	{
		float flTimeSinceLastAcquire = (flCurrentTime - G::AimTarget.m_flLastAcquiredTime) * 1000.0f; // Convert to milliseconds
		if (flTimeSinceLastAcquire < Vars::Aimbot::General::SwitchDelay.Value)
		{
			bCanSwitchTarget = false;
		}
		else
		{
			// Reset target - set new acquisition time
			G::AimTarget = { vTargets.front().m_pEntity->entindex(), I::GlobalVars->tickcount, 32, flCurrentTime };
		}
	}
	else if (!G::AimTarget.m_iEntIndex)
	{
		// No current target - set new acquisition time
		G::AimTarget = { vTargets.front().m_pEntity->entindex(), I::GlobalVars->tickcount, 32, flCurrentTime };
	}

	// If we can't switch targets, use the current target if it's still valid
	if (!bCanSwitchTarget && G::AimTarget.m_iEntIndex)
	{
		// Check if current target is still in our target list
		bool bCurrentTargetStillValid = false;
		for (auto& tTarget : vTargets)
		{
			if (tTarget.m_pEntity->entindex() == G::AimTarget.m_iEntIndex)
			{
				bCurrentTargetStillValid = true;
				break;
			}
		}

		if (!bCurrentTargetStillValid)
		{
			// Current target is no longer valid, we must switch
			bCanSwitchTarget = true;
			G::AimTarget = { vTargets.front().m_pEntity->entindex(), I::GlobalVars->tickcount, 32, flCurrentTime };
		}
	}

	for (auto& tTarget : vTargets)
	{
		// Skip targets that don't match our current target if switch delay is active
		if (!bCanSwitchTarget && tTarget.m_pEntity->entindex() != G::AimTarget.m_iEntIndex)
			continue;
		if (nWeaponID == TF_WEAPON_MEDIGUN && pWeapon->As<CWeaponMedigun>()->m_hHealingTarget().Get() == tTarget.m_pEntity)
		{
			if (G::LastUserCmd->buttons & IN_ATTACK)
				pCmd->buttons |= IN_ATTACK;
			return;
		}

		if (tTarget.m_pEntity->IsPlayer())
		{
			auto pT = tTarget.m_pEntity->As<CTFPlayer>();
			if (WarpPrediction::ShouldPredict(pT))
			{
				Vec3 vPred = WarpPrediction::PredictPos(pT, m_vEyePos, tTarget.m_vPos);
				tTarget.m_vPos = vPred;
				tTarget.m_vAngleTo = Math::CalcAngle(m_vEyePos, vPred);
			}
		}

		const auto iResult = CanHit(tTarget, pLocal, pWeapon);
		if (!iResult) continue;
		if (iResult == 2)
		{
			// Only switch target if we can (delay expired)
			if (bCanSwitchTarget)
			{
				G::AimTarget = { tTarget.m_pEntity->entindex(), I::GlobalVars->tickcount, 32, flCurrentTime };
			}
			Aim(pCmd, tTarget.m_vAngleTo);
			break;
		}

		// Only switch target if we can (delay expired)
		if (bCanSwitchTarget || tTarget.m_pEntity->entindex() == G::AimTarget.m_iEntIndex)
		{
			G::AimTarget = { tTarget.m_pEntity->entindex(), I::GlobalVars->tickcount, 32, flCurrentTime };
			G::AimPoint = { tTarget.m_vPos, I::GlobalVars->tickcount };
		}

		if (ShouldFire(pLocal, pWeapon, pCmd, tTarget))
		{
			switch (nWeaponID)
			{
			case TF_WEAPON_MEDIGUN:
				if (!(G::LastUserCmd->buttons & IN_ATTACK))
					pCmd->buttons |= IN_ATTACK;
				break;
			case TF_WEAPON_SNIPERRIFLE_CLASSIC:
				if (pWeapon->As<CTFSniperRifle>()->m_flChargedDamage() && pLocal->m_hGroundEntity())
					pCmd->buttons &= ~IN_ATTACK;
				break;
			case TF_WEAPON_LASER_POINTER:
				pCmd->buttons |= IN_ATTACK | IN_ATTACK2;
				break;
			default:
				pCmd->buttons |= IN_ATTACK;
			}

			if (Vars::Aimbot::Hitscan::Modifiers.Value & Vars::Aimbot::Hitscan::ModifiersEnum::Tapfire && pWeapon->GetWeaponSpread() != 0.f && !pLocal->InCond(TF_COND_RUNE_PRECISION)
				&& pLocal->GetShootPos().DistTo(tTarget.m_vPos) > Vars::Aimbot::Hitscan::TapFireDist.Value)
			{
				const float flTimeSinceLastShot = (pLocal->m_nTickBase() * TICK_INTERVAL) - pWeapon->m_flLastFireTime();
				if (flTimeSinceLastShot <= (pWeapon->GetBulletsPerShot() > 1 ? 0.25f : 1.25f))
					pCmd->buttons &= ~IN_ATTACK;
			}
		}

		G::Attacking = SDK::IsAttacking(pLocal, pWeapon, pCmd, true);
		if (G::Attacking == 1 && nWeaponID != TF_WEAPON_LASER_POINTER)
		{
			if (tTarget.m_pEntity->IsPlayer())
				F::Resolver.HitscanRan(pLocal, tTarget.m_pEntity->As<CTFPlayer>(), pWeapon, tTarget.m_nAimedHitbox);

			if (tTarget.m_bBacktrack)
				pCmd->tick_count = TIME_TO_TICKS(tTarget.m_pRecord->m_flSimTime) + TIME_TO_TICKS(F::Backtrack.GetFakeInterp());
		}
		DrawVisuals(pLocal, tTarget, nWeaponID);

		Aim(pCmd, tTarget.m_vAngleTo);
		if (G::SilentAngles)
		{
			switch (nWeaponID)
			{
			case TF_WEAPON_MEDIGUN:
			//case TF_WEAPON_LASER_POINTER: // we can psilent with the wrangler though probably with some hacks
				G::SilentAngles = false, G::PSilentAngles = true;
			}
		}
		break;
	}
}