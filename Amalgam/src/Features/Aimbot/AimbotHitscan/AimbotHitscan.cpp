#include "AimbotHitscan.h"

#include "../Aimbot.h"
#include "../../Resolver/Resolver.h"
#include "../../Ticks/Ticks.h"
#include "../../Visuals/Visuals.h"
#include "../../Simulation/MovementSimulation/MovementSimulation.h"
#include "../../../Utils/Math/SIMDMath.h"

// Linux-internals inspired reliable visibility check
bool IsPlayerVisibleReliable(CTFPlayer* pLocal, CTFPlayer* pTarget, int nBone)
{
	Vec3 vTargetPos = pTarget->GetBonePosition(nBone);
	Vec3 vLocalPos = pLocal->GetShootPos();

	CGameTrace trace = {};
	CTraceFilterHitscan filter = {};
	filter.pSkip = pLocal;

	SDK::TraceHull(vLocalPos, vTargetPos, Vec3(-3, -3, -3), Vec3(3, 3, 3), MASK_SHOT, &filter, &trace);

	return (trace.entity == pTarget || trace.fraction > 0.97f);
}

// Simplified hitbox selection based on Linux-internals approach
int GetOptimalBone(CTFPlayer* pLocal, CTFPlayer* pTarget, CTFWeaponBase* pWeapon)
{
	// Use Linux-internals logic but expanded for more weapons
	const int iBodyBone = 5;  // Body bone index
	const int iHeadBone = pTarget->GetHeadBone();

	// Head targeting logic similar to Linux-internals
	if (pLocal->m_iClass() == TF_CLASS_SNIPER)
	{
		if (pLocal->IsScoped() && pTarget->m_iHealth() > 50)
			return iHeadBone;
	}
	else if (pLocal->m_iClass() == TF_CLASS_SPY)
	{
		if (pWeapon->IsHeadshotWeapon())
			return iHeadBone;
	}

	// Default to body for reliability
	return iBodyBone;
}

// Simplified FOV calculation like Linux-internals
float CalculateFOVToTarget(Vec3 vLocalAngles, Vec3 vLocalPos, Vec3 vTargetPos)
{
	Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vTargetPos);
	return Math::CalcFov(vLocalAngles, vAngleTo);
}

std::vector<Target_t> CAimbotHitscan::GetTargets(CTFPlayer* pLocal, CTFWeaponBase* pWeapon)
{
	std::vector<Target_t> vTargets;
	const auto iSort = Vars::Aimbot::General::TargetSelection.Value;

	Vec3 vLocalPos = F::Ticks.GetShootPos();
	Vec3 vLocalAngles = I::EngineClient->GetViewAngles();

	// Enhanced targeting with Linux-internals reliability
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

			// Use Linux-internals inspired simplified targeting
			int nOptimalBone = GetOptimalBone(pLocal, pEntity->As<CTFPlayer>(), pWeapon);
			Vec3 vTargetPos = pEntity->As<CTFPlayer>()->GetBonePosition(nOptimalBone);

			float flFOVTo = CalculateFOVToTarget(vLocalAngles, vLocalPos, vTargetPos);
			if (flFOVTo > Vars::Aimbot::General::AimFOV.Value)
				continue;

			// Simple reliable visibility check
			if (!IsPlayerVisibleReliable(pLocal, pEntity->As<CTFPlayer>(), nOptimalBone))
				continue;

			Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vTargetPos);
			float flDistTo = iSort == Vars::Aimbot::General::TargetSelectionEnum::Distance ? CSIMDMath::FastDistance(vLocalPos, vTargetPos) : 0.f;
			vTargets.emplace_back(pEntity, TargetEnum::Player, vTargetPos, vAngleTo, flFOVTo, flDistTo, bTeammate ? 0 : F::AimbotGlobal.GetPriority(pEntity->entindex()));
		}

		if (pWeapon->GetWeaponID() == TF_WEAPON_MEDIGUN)
			return vTargets;
	}

	// Preserve original advanced targeting for buildings, NPCs, etc.
	if (Vars::Aimbot::General::Target.Value)
	{
		for (auto pEntity : H::Entities.GetGroup(EGroupType::BUILDINGS_ENEMIES))
		{
			if (F::AimbotGlobal.ShouldIgnore(pEntity, pLocal, pWeapon))
				continue;

			Vec3 vPos = pEntity->GetCenter();
			Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vPos);
			float flFOVTo = Math::CalcFov(vLocalAngles, vAngleTo);
			if (flFOVTo > Vars::Aimbot::General::AimFOV.Value)
				continue;

			float flDistTo = iSort == Vars::Aimbot::General::TargetSelectionEnum::Distance ? CSIMDMath::FastDistance(vLocalPos, vPos) : 0.f;
			vTargets.emplace_back(pEntity, pEntity->IsSentrygun() ? TargetEnum::Sentry : pEntity->IsDispenser() ? TargetEnum::Dispenser : TargetEnum::Teleporter, vPos, vAngleTo, flFOVTo, flDistTo);
		}
	}

	if (Vars::Aimbot::General::Target.Value & Vars::Aimbot::General::TargetEnum::Stickies)
	{
		for (auto pEntity : H::Entities.GetGroup(EGroupType::WORLD_PROJECTILES))
		{
			if (F::AimbotGlobal.ShouldIgnore(pEntity, pLocal, pWeapon))
				continue;

			Vec3 vPos = pEntity->m_vecOrigin();
			Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vPos);
			float flFOVTo = Math::CalcFov(vLocalAngles, vAngleTo);
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
			if (F::AimbotGlobal.ShouldIgnore(pEntity, pLocal, pWeapon))
				continue;

			Vec3 vPos = pEntity->GetCenter();
			Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vPos);
			float flFOVTo = Math::CalcFov(vLocalAngles, vAngleTo);
			if (flFOVTo > Vars::Aimbot::General::AimFOV.Value)
				continue;

			float flDistTo = iSort == Vars::Aimbot::General::TargetSelectionEnum::Distance ? CSIMDMath::FastDistance(vLocalPos, vPos) : 0.f;
			vTargets.emplace_back(pEntity, TargetEnum::NPC, vPos, vAngleTo, flFOVTo, flDistTo);
		}
	}

	return vTargets;
}

std::vector<Target_t> CAimbotHitscan::SortTargets(CTFPlayer* pLocal, CTFWeaponBase* pWeapon)
{
	auto vTargets = GetTargets(pLocal, pWeapon);
	if (vTargets.empty())
		return vTargets;

	// Apply Linux-internals style target selection logic
	float flSmallestFOV = __FLT_MAX__;
	float flSmallestDistance = __FLT_MAX__;
	int nSmallestHealth = INT_MAX;
	int nLargestHealth = INT_MIN;

	Target_t* pBestTarget = nullptr;

	// Use Linux-internals target selection approach
	for (auto& tTarget : vTargets)
	{
		if (tTarget.m_pEntity->IsPlayer())
		{
			int nHealth = tTarget.m_pEntity->As<CTFPlayer>()->m_iHealth();

			switch (Vars::Aimbot::General::TargetSelection.Value)
			{
			case Vars::Aimbot::General::TargetSelectionEnum::FOV:
				if (tTarget.m_flFOVTo < flSmallestFOV)
				{
					flSmallestFOV = tTarget.m_flFOVTo;
					pBestTarget = &tTarget;
				}
				break;

			case Vars::Aimbot::General::TargetSelectionEnum::Distance:
				if (tTarget.m_flDistTo < flSmallestDistance)
				{
					flSmallestDistance = tTarget.m_flDistTo;
					pBestTarget = &tTarget;
				}
				break;

			case Vars::Aimbot::General::TargetSelectionEnum::LEAST_HEALTH:
				if (nHealth < nSmallestHealth)
				{
					nSmallestHealth = nHealth;
					pBestTarget = &tTarget;
				}
				break;

			case Vars::Aimbot::General::TargetSelectionEnum::MOST_HEALTH:
				if (nHealth > nLargestHealth)
				{
					nLargestHealth = nHealth;
					pBestTarget = &tTarget;
				}
				break;
			}
		}
		else
		{
			// For non-players, use FOV-based selection
			if (tTarget.m_flFOVTo < flSmallestFOV)
			{
				flSmallestFOV = tTarget.m_flFOVTo;
				pBestTarget = &tTarget;
			}
		}
	}

	if (pBestTarget)
		return { *pBestTarget };

	return vTargets;
}

int CAimbotHitscan::GetHitboxPriority(int nHitbox, CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CBaseEntity* pTarget)
{
	// Preserve original hitbox priority system for advanced users
	auto pPlayer = pTarget->As<CTFPlayer>();
	if (!pPlayer)
		return 0;

	bool bHeadshot = false;
	if (pWeapon->GetWeaponID() == TF_WEAPON_SNIPER_RIFLE || pWeapon->GetWeaponID() == TF_WEAPON_SNIPIFIER)
	{
		if (!pLocal->IsScoped() && Vars::Aimbot::Hitscan::WaitForHeadshot.Value)
			return 3;

		bHeadshot = true;
		if (Vars::Aimbot::Hitscan::WaitForHeadshot.Value && pLocal->IsScoped())
		{
			if (pWeapon->GetWeaponID() != TF_WEAPON_SNIPIFIER)
			{
				float flChargeTime = I::GlobalVars->curtime - pLocal->m_flSniperChargeTime();
				if (flChargeTime < 1.0f)
					bHeadshot = false;
			}
		}

		if (bHeadshot)
		{
			int iDamage = static_cast<int>(pWeapon->GetDamage() * pWeapon->GetDamageBonus());
			if (pLocal->IsScoped())
				iDamage *= 3.0f;

			iDamage = static_cast<int>(static_cast<float>(iDamage) * pPlayer->GetDamageMultiplier());

			if (pPlayer->m_iHealth() <= iDamage)
				bHeadshot = false;
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
}

int CAimbotHitscan::CanHit(Target_t& tTarget, CTFPlayer* pLocal, CTFWeaponBase* pWeapon)
{
	// Simplified hit detection inspired by Linux-internals
	if (Vars::Aimbot::General::Ignore.Value & Vars::Aimbot::General::IgnoreEnum::Unsimulated && H::Entities.GetChoke(tTarget.m_pEntity->entindex()) > Vars::Aimbot::General::TickTolerance.Value)
		return false;

	Vec3 vEyePos = pLocal->GetShootPos();
	const float flMaxRange = powf(pWeapon->GetRange(), 2.f);

	// Simple current-frame targeting for reliability
	if (tTarget.m_pEntity->IsPlayer())
	{
		int nOptimalBone = GetOptimalBone(pLocal, tTarget.m_pEntity->As<CTFPlayer>(), pWeapon);
		Vec3 vTargetPos = tTarget.m_pEntity->As<CTFPlayer>()->GetBonePosition(nOptimalBone);

		if (vEyePos.DistToSqr(vTargetPos) > flMaxRange)
			return false;

		// Simple visibility check like Linux-internals
		if (!IsPlayerVisibleReliable(pLocal, tTarget.m_pEntity->As<CTFPlayer>(), nOptimalBone))
			return false;

		tTarget.m_vPos = vTargetPos;
		tTarget.m_vAngleTo = Math::CalcAngle(vEyePos, vTargetPos);
		tTarget.m_nAimedHitbox = nOptimalBone;
		tTarget.m_bBacktrack = false;
		return true;
	}

	// Preserve advanced functionality for buildings and other entities
	auto pModel = tTarget.m_pEntity->GetModel();
	if (!pModel) return false;
	auto pHDR = I::ModelInfoClient->GetStudiomodel(pModel);
	if (!pHDR) return false;
	auto pSet = pHDR->pHitboxSet(tTarget.m_pEntity->As<CBaseAnimating>()->m_nHitboxSet());
	if (!pSet) return false;

	std::vector<TickRecord*> vRecords = {};
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

	// Original advanced multipoint logic for entities
	Vec3 vPeekPos = vEyePos;
	if (Vars::Aimbot::Hitscan::PeekAmount.Value && pWeapon->GetWeaponSpread())
	{
		bool bPeekCheck = false;
		switch (Vars::Aimbot::Hitscan::PeekCheck.Value)
		{
		case Vars::Aimbot::Hitscan::PeekCheckEnum::Off: break;
		case Vars::Aimbot::Hitscan::PeekCheckEnum::DoubletapOnly: bPeekCheck = F::Ticks.GetTicks(pWeapon); break;
		case Vars::Aimbot::Hitscan::PeekCheckEnum::Always: bPeekCheck = true; break;
		}
		vPeekPos = bPeekCheck ? vEyePos + pLocal->m_vecVelocity() * TICKS_TO_TIME(-Vars::Aimbot::Hitscan::PeekAmount.Value) : vEyePos;
	}

	for (auto pRecord : vRecords)
	{
		matrix3x4 aBones[MAXSTUDIOBONES];
		if (!pRecord->m_bHasBones || !pRecord->m_BoneMatrix.ToMatrix(aBones))
			continue;

		std::vector<std::tuple<mstudiobbox_t*, int, int>> vHitboxes;
		for (int nHitbox = 0; nHitbox < pSet->numhitboxes; nHitbox++)
		{
			auto pBox = pSet->pHitbox(nHitbox);
			if (!pBox) continue;

			int iPriority = GetHitboxPriority(nHitbox, pLocal, pWeapon, tTarget.m_pEntity);
			vHitboxes.emplace_back(pBox, nHitbox, iPriority);
		}
		std::sort(vHitboxes.begin(), vHitboxes.end(), [&](const auto& a, const auto& b) -> bool
			{
				return std::get<2>(a) < std::get<2>(b);
			});

		float flModelScale = tTarget.m_pEntity->As<CBaseAnimating>()->m_flModelScale();
		float flBoneScale = std::max(Vars::Aimbot::Hitscan::BoneSizeMinimumScale.Value, Vars::Aimbot::Hitscan::PointScale.Value / 100.f);
		float flBoneSubtract = Vars::Aimbot::Hitscan::BoneSizeSubtract.Value;

		for (auto& [pBox, iHitbox, _] : vHitboxes)
		{
			Vec3 vMins = pBox->bbmin;
			Vec3 vMaxs = pBox->bbmax;
			Vec3 vCheckMins = (vMins + flBoneSubtract / flModelScale) * flBoneScale;
			Vec3 vCheckMaxs = (vMaxs - flBoneSubtract / flModelScale) * flBoneScale;

			Vec3 vPoint = (vMins + vMaxs) / 2;
			Vec3 vOrigin; Math::VectorTransform(vPoint, aBones[pBox->bone], vOrigin);

			if (vEyePos.DistToSqr(vOrigin) > flMaxRange)
				continue;

			Vec3 vAngles; bool bChanged = Aim(G::CurrentUserCmd->viewangles, Math::CalcAngle(vEyePos, vOrigin), vAngles);
			Vec3 vForward; Math::AngleVectors(vAngles, &vForward);

			if (bChanged || SDK::VisPos(pLocal, tTarget.m_pEntity, vEyePos, vOrigin))
			{
				if ((!bChanged || Math::RayToOBB(vEyePos, vForward, vCheckMins, vCheckMaxs, aBones[pBox->bone], flModelScale) && SDK::VisPos(pLocal, tTarget.m_pEntity, vEyePos, vEyePos + vForward * vEyePos.DistTo(vOrigin))))
				{
					tTarget.m_vAngleTo = vAngles;
					tTarget.m_pRecord = pRecord;
					tTarget.m_vPos = vOrigin;
					tTarget.m_nAimedHitbox = iHitbox;
					tTarget.m_bBacktrack = true;
					return true;
				}
			}
		}
	}

	return false;
}

bool CAimbotHitscan::Aim(Vec3 vCurAngle, Vec3 vToAngle, Vec3& vOut, int iMethod)
{
	switch (iMethod)
	{
	case Vars::Aimbot::General::AimTypeEnum::Plain:
		vOut = vToAngle;
		return true;

	case Vars::Aimbot::General::AimTypeEnum::Silent:
		G::CurrentUserCmd->viewangles = vToAngle;
		return false;

	case Vars::Aimbot::General::AimTypeEnum::Smooth:
	{
		Vec3 vDelta = Math::AngleDiff(vToAngle, vCurAngle);
		float flSmooth = Vars::Aimbot::General::SmoothingAmount.Value;
		if (flSmooth <= 0.f) flSmooth = 1.f;

		vDelta /= flSmooth;
		vOut = vCurAngle + vDelta;
		return true;
	}

	case Vars::Aimbot::General::AimTypeEnum::Assistive:
	{
		Vec3 vDelta = Math::AngleDiff(vToAngle, vCurAngle);
		float flAssist = Vars::Aimbot::General::AssistStrength.Value;
		if (flAssist <= 0.f) flAssist = 1.f;

		vDelta /= flAssist;
		vOut = vCurAngle + vDelta;
		return true;
	}
	}

	vOut = vToAngle;
	return true;
}

void CAimbotHitscan::Aim(CUserCmd* pCmd, Vec3& vAngle, int iMethod)
{
	Vec3 vOldAngle = pCmd->viewangles;
	Vec3 vAimAngle;

	if (Aim(vOldAngle, vAngle, vAimAngle, iMethod))
	{
		if (iMethod != Vars::Aimbot::General::AimTypeEnum::Silent)
		{
			pCmd->viewangles = vAimAngle;
			SDK::FixMovement(pCmd, vOldAngle, vAimAngle);
		}
	}
	else
	{
		pCmd->viewangles = vAimAngle;
	}

	G::AimPoint.m_vPos = vAimAngle;
	G::AimPoint.m_iTickCount = I::GlobalVars->tickcount;
	G::AimPoint.m_iDuration = Vars::Aimbot::General::AimFOV.Value / 10.f;
}

bool CAimbotHitscan::ShouldRun(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd)
{
	if (!pLocal || !pWeapon || !pCmd)
		return false;

	if (!pLocal->IsAlive() || pLocal->IsAGhost())
		return false;

	if (!pLocal->CanAttack())
		return false;

	if (!SDK::AttribHookValue(1, "mult_dmg", pWeapon))
		return false;

	if (I::EngineVGui->IsGameUIVisible())
		return false;

	// Linux-internals style checks
	if (!pWeapon->CanPrimaryAttack() && !pWeapon->CanSecondaryAttack())
		return false;

	return true;
}

bool CAimbotHitscan::ShouldFire(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd, const Target_t& tTarget)
{
	if (!G::CurrentUserCmd)
		return false;

	if (pLocal->m_iClass() == TF_CLASS_SNIPER && pWeapon->GetWeaponID() == TF_WEAPON_SNIPER_RIFLE)
	{
		if (Vars::Aimbot::Hitscan::WaitForHeadshot.Value && !pLocal->IsScoped())
			return false;

		if (tTarget.m_nAimedHitbox == HITBOX_HEAD)
		{
			if (Vars::Aimbot::Hitscan::WaitForHeadshot.Value && pLocal->IsScoped())
			{
				if (pWeapon->GetWeaponID() != TF_WEAPON_SNIPIFIER)
				{
					float flChargeTime = I::GlobalVars->curtime - pLocal->m_flSniperChargeTime();
					if (flChargeTime < 1.0f)
						return false;
				}
			}
		}
	}

	if (tTarget.m_pEntity->IsPlayer())
	{
		if (!pWeapon->CanShootAt(tTarget.m_pEntity->As<CTFPlayer>(), tTarget.m_bBacktrack ? tTarget.m_pRecord->m_flSimulationTime : I::GlobalVars->curtime))
			return false;
	}
	else
	{
		if (!pWeapon->CanPrimaryAttack())
			return false;
	}

	return true;
}

void CAimbotHitscan::Run(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd)
{
	if (!ShouldRun(pLocal, pWeapon, pCmd))
		return;

	m_bRunning = false;
	G::CurrentTargetIdx = 0;
	G::AimTarget = {};

	auto vTargets = SortTargets(pLocal, pWeapon);
	if (vTargets.empty())
		return;

	Target_t& tTarget = vTargets.front();
	m_bRunning = true;
	G::CurrentTargetIdx = tTarget.m_pEntity->entindex();

	if (!CanHit(tTarget, pLocal, pWeapon))
		return;

	G::AimTarget.m_vPos = tTarget.m_vPos;
	G::AimTarget.m_iTickCount = I::GlobalVars->tickcount;
	G::AimTarget.m_iDuration = 1;

	if (ShouldFire(pLocal, pWeapon, pCmd, tTarget))
	{
		if (G::CurrentUserCmd)
		{
			bool bShouldAttack = true;
			if (Vars::Aimbot::General::AutoScope.Value && pWeapon->GetWeaponID() == TF_WEAPON_SNIPER_RIFLE && !pLocal->IsScoped())
			{
				G::CurrentUserCmd->buttons |= IN_ATTACK2;
				bShouldAttack = false;
			}

			if (bShouldAttack)
			{
				G::CurrentUserCmd->buttons |= IN_ATTACK;
				F::Ticks.GetTickShift(pWeapon, tTarget.m_bBacktrack ? tTarget.m_pRecord->m_flSimulationTime : I::GlobalVars->curtime, pLocal->GetTickBase());
			}
		}

		if (Vars::Aimbot::General::AimType.Value != Vars::Aimbot::General::AimTypeEnum::Silent)
			Aim(pCmd, tTarget.m_vAngleTo);
		else
			Aim(pCmd, tTarget.m_vAngleTo);
	}
	else
	{
		if (Vars::Aimbot::General::AimType.Value == Vars::Aimbot::General::AimTypeEnum::Smooth || Vars::Aimbot::General::AimType.Value == Vars::Aimbot::General::AimTypeEnum::Assistive)
			Aim(pCmd, tTarget.m_vAngleTo);
	}
}