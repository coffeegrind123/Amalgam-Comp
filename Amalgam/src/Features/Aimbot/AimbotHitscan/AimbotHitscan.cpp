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
	// Use GetHitboxCenter instead of GetBonePosition
	matrix3x4 aBones[MAXSTUDIOBONES];
	if (!pTarget->SetupBones(aBones, MAXSTUDIOBONES, BONE_USED_BY_ANYTHING, I::GlobalVars->curtime))
		return false;

	Vec3 vTargetPos = pTarget->GetHitboxCenter(aBones, nBone);
	Vec3 vLocalPos = pLocal->GetShootPos();

	CGameTrace trace = {};
	CTraceFilterHitscan filter = {};
	filter.pSkip = pLocal;

	SDK::TraceHull(vLocalPos, vTargetPos, Vec3(-3, -3, -3), Vec3(3, 3, 3), MASK_SHOT, &filter, &trace);

	return (trace.m_pEnt == pTarget || trace.fraction > 0.97f);
}

// Simplified hitbox selection based on Linux-internals approach
int GetOptimalBone(CTFPlayer* pLocal, CTFPlayer* pTarget, CTFWeaponBase* pWeapon)
{
	// Use Linux-internals logic but expanded for more weapons
	const int iBodyBone = 5;  // Body bone index
	const int iHeadBone = 0; // Head bone index (standard hitbox)

	// Head targeting logic similar to Linux-internals
	if (pLocal->m_iClass() == TF_CLASS_SNIPER)
	{
		if (pLocal->IsScoped() && pTarget->m_iHealth() > 50)
			return iHeadBone;
	}
	else if (pLocal->m_iClass() == TF_CLASS_SPY)
	{
		// Check if weapon can headshot (revolver, ambassador, etc.)
		if (pWeapon->GetWeaponID() == TF_WEAPON_REVOLVER ||
			pWeapon->GetWeaponID() == TF_WEAPON_KNIFE)
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

			// Use SetupBones and GetHitboxCenter for reliable bone position
			matrix3x4 aBones[MAXSTUDIOBONES];
			if (!pEntity->As<CTFPlayer>()->SetupBones(aBones, MAXSTUDIOBONES, BONE_USED_BY_ANYTHING, I::GlobalVars->curtime))
				continue;

			Vec3 vTargetPos = pEntity->As<CTFPlayer>()->GetHitboxCenter(aBones, nOptimalBone);

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
	float flSmallestFOV = FLT_MAX;
	float flSmallestDistance = FLT_MAX;
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

			case Vars::Aimbot::General::TargetSelectionEnum::LeastHealth:
				if (nHealth < nSmallestHealth)
				{
					nSmallestHealth = nHealth;
					pBestTarget = &tTarget;
				}
				break;

			case Vars::Aimbot::General::TargetSelectionEnum::MostHealth:
				if (nHealth > nLargestHealth)
				{
					nLargestHealth = nHealth;
					pBestTarget = &tTarget;
				}
				break;

			default:
				// Fallback to FOV-based selection
			if (tTarget.m_flFOVTo < flSmallestFOV)
				{
					flSmallestFOV = tTarget.m_flFOVTo;
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
	if (pWeapon->GetWeaponID() == TF_WEAPON_SNIPERRIFLE || pWeapon->GetWeaponID() == TF_WEAPON_SNIPERRIFLE_DECAP)
	{
		if (Vars::Aimbot::Hitscan::Modifiers.Value & Vars::Aimbot::Hitscan::ModifiersEnum::WaitForHeadshot && !pLocal->IsZoomed())
			return 3;

		bHeadshot = true;

		if (bHeadshot)
		{
			// Simple damage calculation - remove complex bonus calculations
			int iDamage = static_cast<int>(pWeapon->GetDamage());
			if (pLocal->IsZoomed())
				iDamage *= 3; // Simple 3x damage bonus when zoomed

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

		// Use SetupBones and GetHitboxCenter for reliable bone position
		matrix3x4 aBones[MAXSTUDIOBONES];
		if (!tTarget.m_pEntity->As<CTFPlayer>()->SetupBones(aBones, MAXSTUDIOBONES, BONE_USED_BY_ANYTHING, I::GlobalVars->curtime))
			return false;

		Vec3 vTargetPos = tTarget.m_pEntity->As<CTFPlayer>()->GetHitboxCenter(aBones, nOptimalBone);

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

	// CanHit function should return true/false for hit detection
	return true;
}

bool CAimbotHitscan::Aim(Vec3 vCurAngle, Vec3 vToAngle, Vec3& vOut, int iMethod)
{
	switch (iMethod)
	{
	case Vars::Aimbot::General::AimTypeEnum::Off:
		return false;

	case Vars::Aimbot::General::AimTypeEnum::Plain:
		vOut = vToAngle;
		return true;

	case Vars::Aimbot::General::AimTypeEnum::Silent:
		// Silent aim is handled specially - just return the angle
		vOut = vToAngle;
		return true;

	case Vars::Aimbot::General::AimTypeEnum::Smooth:
	{
		// Smooth aim - use a fixed smooth factor (not FOV)
		Vec3 vDelta = vToAngle - vCurAngle;
		// Normalize angles
		while (vDelta.x > 180.0f) vDelta.x -= 360.0f;
		while (vDelta.x < -180.0f) vDelta.x += 360.0f;
		while (vDelta.y > 180.0f) vDelta.y -= 360.0f;
		while (vDelta.y < -180.0f) vDelta.y += 360.0f;

		// Use a smooth factor of 3.0 for gradual transition
		float flSmooth = 3.0f;
		vDelta /= flSmooth;
		vOut = vCurAngle + vDelta;
		return true;
	}

	case Vars::Aimbot::General::AimTypeEnum::Locking:
		// Locking mode - snap and hold
		vOut = vToAngle;
		return true;

	case Vars::Aimbot::General::AimTypeEnum::Assistive:
	{
		Vec3 vDelta = vToAngle - vCurAngle;
		// Normalize angles
		while (vDelta.x > 180.0f) vDelta.x -= 360.0f;
		while (vDelta.x < -180.0f) vDelta.x += 360.0f;
		while (vDelta.y > 180.0f) vDelta.y -= 360.0f;
		while (vDelta.y < -180.0f) vDelta.y += 360.0f;

		// Use AssistStrength value (default 25%)
		float flAssist = Vars::Aimbot::General::AssistStrength.Value;
		if (flAssist <= 0.f) flAssist = 1.f;
		flAssist = 100.f / flAssist; // Convert percentage to divisor

		vDelta /= flAssist;
		vOut = vCurAngle + vDelta;
		return true;
	}
	}

	return false;
}

void CAimbotHitscan::Aim(CUserCmd* pCmd, Vec3& vAngle, int iMethod)
{
	Vec3 vOldAngle = pCmd->viewangles;
	Vec3 vAimAngle;

	if (!Aim(vOldAngle, vAngle, vAimAngle, iMethod))
		return;

	// Handle different aim types
	if (iMethod == Vars::Aimbot::General::AimTypeEnum::Silent)
	{
		// Silent aim: set angles in G::CurrentUserCmd but not in viewangles
		if (G::CurrentUserCmd)
			G::CurrentUserCmd->viewangles = vAimAngle;
	}
	else
	{
		// All other modes: set viewangles normally
		pCmd->viewangles = vAimAngle;
		SDK::FixMovement(pCmd, vOldAngle, vAimAngle);
	}

	G::AimPoint.m_vOrigin = vAimAngle;
	G::AimPoint.m_iTickCount = I::GlobalVars->tickcount;
	G::AimPoint.m_iDuration = 1;
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
	// Check if auto shoot is enabled
	if (!Vars::Aimbot::General::AutoShoot.Value)
		return false;

	if (!G::CurrentUserCmd)
		return false;

	// Check if weapon can fire
	if (!pWeapon->CanPrimaryAttack())
		return false;

	// Check weapon-specific conditions
	if (pLocal->m_iClass() == TF_CLASS_SNIPER && pWeapon->GetWeaponID() == TF_WEAPON_SNIPERRIFLE)
	{
		// Check modifier flags
		if (Vars::Aimbot::Hitscan::Modifiers.Value & Vars::Aimbot::Hitscan::ModifiersEnum::ScopedOnly)
		{
			if (!pLocal->IsScoped())
				return false;
		}

		if (Vars::Aimbot::Hitscan::Modifiers.Value & Vars::Aimbot::Hitscan::ModifiersEnum::WaitForHeadshot)
		{
			if (!G::CanHeadshot)
				return false;
		}

		if (Vars::Aimbot::Hitscan::Modifiers.Value & Vars::Aimbot::Hitscan::ModifiersEnum::WaitForCharge)
		{
			// Check if rifle is sufficiently charged
			auto pSniperRifle = pWeapon->As<CTFSniperRifle>();
			if (pSniperRifle && pSniperRifle->m_flChargedDamage() < 150.f)
				return false;
		}
	}

	return true;
}

void CAimbotHitscan::Run(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd)
{
	// Check if aim type is Off
	if (Vars::Aimbot::General::AimType.Value == Vars::Aimbot::General::AimTypeEnum::Off)
		return;

	if (!ShouldRun(pLocal, pWeapon, pCmd))
		return;

	m_bRunning = false;
	G::AimTarget.m_iEntIndex = 0;
	G::AimTarget = {};

	auto vTargets = SortTargets(pLocal, pWeapon);
	if (vTargets.empty())
		return;

	Target_t& tTarget = vTargets.front();
	m_bRunning = true;
	G::AimTarget.m_iEntIndex = tTarget.m_pEntity->entindex();

	if (!CanHit(tTarget, pLocal, pWeapon))
		return;

	G::AimPoint.m_vOrigin = tTarget.m_vPos;
	G::AimTarget.m_iTickCount = I::GlobalVars->tickcount;
	G::AimTarget.m_iDuration = 1;

	// Determine if we should fire
	bool bShouldFire = ShouldFire(pLocal, pWeapon, pCmd, tTarget);

	if (bShouldFire && G::CurrentUserCmd)
	{
		// Handle auto-scoping before firing
		if (Vars::Aimbot::Hitscan::Modifiers.Value & Vars::Aimbot::Hitscan::ModifiersEnum::AutoScope &&
			pWeapon->GetWeaponID() == TF_WEAPON_SNIPERRIFLE && !pLocal->IsScoped())
		{
			G::CurrentUserCmd->buttons |= IN_ATTACK2;
			bShouldFire = false; // Don't fire yet, scope first
		}
		else
		{
			// Fire the weapon
			G::CurrentUserCmd->buttons |= IN_ATTACK;
		}
	}

	// Apply aim based on aim type and whether we're firing
	int iAimType = Vars::Aimbot::General::AimType.Value;

	// Always aim in these modes
	if (iAimType == Vars::Aimbot::General::AimTypeEnum::Plain ||
		iAimType == Vars::Aimbot::General::AimTypeEnum::Silent ||
		iAimType == Vars::Aimbot::General::AimTypeEnum::Locking)
	{
		Aim(pCmd, tTarget.m_vAngleTo, iAimType);
	}
	// Aim when firing or always for smooth/assistive
	else if (iAimType == Vars::Aimbot::General::AimTypeEnum::Smooth ||
			 iAimType == Vars::Aimbot::General::AimTypeEnum::Assistive)
	{
		Aim(pCmd, tTarget.m_vAngleTo, iAimType);
	}
}