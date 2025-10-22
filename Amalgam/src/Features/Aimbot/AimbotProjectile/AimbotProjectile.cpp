#include "AimbotProjectile.h"

#include "../Aimbot.h"
#include "../../Simulation/MovementSimulation/MovementSimulation.h"
#include "../../Simulation/ProjectileSimulation/ProjectileSimulation.h"
#include "../../Ticks/Ticks.h"
#include "../../Visuals/Visuals.h"
#include "../AutoAirblast/AutoAirblast.h"

// Linux-internals inspired simple projectile prediction
struct ProjectilePrediction_t
{
	Vec3 vPredictedPos;
	float flTimeToTarget;
	bool bValid;
};

ProjectilePrediction_t PredictProjectilePosition(CTFPlayer* pLocal, CTFPlayer* pTarget, CTFWeaponBase* pWeapon)
{
	ProjectilePrediction_t tPrediction = {};

	Vec3 vLocalPos = pLocal->GetShootPos();
	Vec3 vTargetPos = pTarget->GetAbsOrigin();
	Vec3 vTargetVel = pTarget->m_vecVelocity();

	// Get projectile speed from weapon - use range as approximation for projectile weapons
	float flProjSpeed = pWeapon->GetRange();
	if (flProjSpeed <= 0.0f)
		return tPrediction;

	// Simple linear prediction (like Linux-internals would use)
	float flLatency = I::EngineClient->GetNetChannelInfo() ?
		I::EngineClient->GetNetChannelInfo()->GetAvgLatency(FLOW_OUTGOING) +
		I::EngineClient->GetNetChannelInfo()->GetAvgLatency(FLOW_INCOMING) : 0.1f;

	// Predict target position after travel time
	float flInitialDist = vLocalPos.DistTo(vTargetPos);
	float flTravelTime = flInitialDist / flProjSpeed;

	// Compensate for latency and movement
	flTravelTime += flLatency;

	// Simple prediction: target_pos + target_velocity * travel_time
	Vec3 vPredictedPos = vTargetPos + vTargetVel * flTravelTime;

	// Gravity compensation for certain weapons
	if (pWeapon->GetWeaponID() == TF_WEAPON_PIPEBOMBLAUNCHER ||
		pWeapon->GetWeaponID() == TF_WEAPON_GRENADELAUNCHER ||
		pWeapon->GetWeaponID() == TF_WEAPON_ROCKETLAUNCHER)
	{
		// Simple ballistic calculation
		float flGravity = 800.0f; // TF2 gravity
		vPredictedPos.z -= (0.5f * flGravity * flTravelTime * flTravelTime);
	}

	tPrediction.vPredictedPos = vPredictedPos;
	tPrediction.flTimeToTarget = flTravelTime;
	tPrediction.bValid = true;

	return tPrediction;
}

// Simplified FOV calculation for projectiles
float CalculateProjectileFOV(Vec3 vLocalAngles, Vec3 vLocalPos, Vec3 vTargetPos)
{
	Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vTargetPos);
	return Math::CalcFov(vLocalAngles, vAngleTo);
}

// Simple visibility check for projectiles
bool IsProjectilePathClear(CTFPlayer* pLocal, Vec3 vTargetPos)
{
	Vec3 vLocalPos = pLocal->GetShootPos();

	CGameTrace trace = {};
	CTraceFilterHitscan filter = {};
	filter.pSkip = pLocal;

	// Simple line trace for projectile path
	SDK::TraceHull(vLocalPos, vTargetPos, Vec3(-1, -1, -1), Vec3(1, 1, 1), MASK_SOLID, &filter, &trace);

	return trace.fraction > 0.95f;
}

std::vector<Target_t> CAimbotProjectile::GetTargets(CTFPlayer* pLocal, CTFWeaponBase* pWeapon)
{
	std::vector<Target_t> vTargets;
	const auto iSort = Vars::Aimbot::General::TargetSelection.Value;

	const Vec3 vLocalPos = F::Ticks.GetShootPos();
	const Vec3 vLocalAngles = I::EngineClient->GetViewAngles();

	// Enhanced targeting with Linux-internals reliability
	{
		EGroupType eGroupType = EGroupType::GROUP_INVALID;
		if (Vars::Aimbot::General::Target.Value & Vars::Aimbot::General::TargetEnum::Players)
			eGroupType = EGroupType::PLAYERS_ENEMIES;
		if (Vars::Aimbot::Healing::AutoHeal.Value)
		{
			switch (pWeapon->GetWeaponID())
			{
			case TF_WEAPON_CROSSBOW: eGroupType = eGroupType == EGroupType::PLAYERS_ENEMIES ? EGroupType::PLAYERS_ALL : EGroupType::PLAYERS_TEAMMATES; break;
			case TF_WEAPON_LUNCHBOX: eGroupType = EGroupType::PLAYERS_TEAMMATES; break;
			}
		}

		for (auto pEntity : H::Entities.GetGroup(eGroupType))
		{
			bool bTeammate = pEntity->m_iTeamNum() == pLocal->m_iTeamNum();
			if (F::AimbotGlobal.ShouldIgnore(pEntity, pLocal, pWeapon))
				continue;

			if (bTeammate)
			{
				if (pEntity->As<CTFPlayer>()->m_iHealth() >= pEntity->As<CTFPlayer>()->GetMaxHealth()
					|| Vars::Aimbot::Healing::FriendsOnly.Value && !H::Entities.IsFriend(pEntity->entindex()) && !H::Entities.InParty(pEntity->entindex()))
					continue;
			}

			// Use simplified Linux-internals style prediction
			ProjectilePrediction_t tPrediction = PredictProjectilePosition(pLocal, pEntity->As<CTFPlayer>(), pWeapon);
			if (!tPrediction.bValid)
				continue;

			float flFOVTo = CalculateProjectileFOV(vLocalAngles, vLocalPos, tPrediction.vPredictedPos);
			if (flFOVTo > Vars::Aimbot::General::AimFOV.Value)
				continue;

			// Basic visibility check
			if (!IsProjectilePathClear(pLocal, tPrediction.vPredictedPos))
			{
				// If direct path is blocked, allow splash damage weapons to continue
				if (pWeapon->GetWeaponID() != TF_WEAPON_ROCKETLAUNCHER &&
					pWeapon->GetWeaponID() != TF_WEAPON_ROCKETLAUNCHER_DIRECTHIT &&
					pWeapon->GetWeaponID() != TF_WEAPON_GRENADELAUNCHER &&
					pWeapon->GetWeaponID() != TF_WEAPON_PIPEBOMBLAUNCHER)
					continue;
			}

			Vec3 vAngleTo = Math::CalcAngle(vLocalPos, tPrediction.vPredictedPos);
			float flDistTo = iSort == Vars::Aimbot::General::TargetSelectionEnum::Distance ? vLocalPos.DistTo(tPrediction.vPredictedPos) : 0.f;
			vTargets.emplace_back(pEntity, TargetEnum::Player, tPrediction.vPredictedPos, vAngleTo, flFOVTo, flDistTo, bTeammate ? 0 : F::AimbotGlobal.GetPriority(pEntity->entindex()));
		}

		if (pWeapon->GetWeaponID() == TF_WEAPON_LUNCHBOX)
			return vTargets;
	}

	// Preserve original advanced targeting for buildings
	if (Vars::Aimbot::General::Target.Value)
	{
		bool bIsRescueRanger = pWeapon->GetWeaponID() == TF_WEAPON_SHOTGUN_BUILDING_RESCUE;
		for (auto pEntity : H::Entities.GetGroup(bIsRescueRanger ? EGroupType::BUILDINGS_ALL : EGroupType::BUILDINGS_ENEMIES))
		{
			if (F::AimbotGlobal.ShouldIgnore(pEntity, pLocal, pWeapon))
				continue;

			if (pEntity->m_iTeamNum() == pLocal->m_iTeamNum() && pEntity->As<CBaseObject>()->m_iHealth() >= pEntity->As<CBaseObject>()->m_iMaxHealth())
				continue;

			Vec3 vPos = pEntity->GetCenter();
			ProjectilePrediction_t tPrediction = PredictProjectilePosition(pLocal, nullptr, pWeapon);
			if (!tPrediction.bValid)
				continue;

			// For buildings, use simple direct targeting
			Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vPos);
			float flFOVTo = Math::CalcFov(vLocalAngles, vAngleTo);
			if (flFOVTo > Vars::Aimbot::General::AimFOV.Value)
				continue;

			float flDistTo = iSort == Vars::Aimbot::General::TargetSelectionEnum::Distance ? vLocalPos.DistTo(vPos) : 0.f;
			vTargets.emplace_back(pEntity, pEntity->IsSentrygun() ? TargetEnum::Sentry : pEntity->IsDispenser() ? TargetEnum::Dispenser : TargetEnum::Teleporter, vPos, vAngleTo, flFOVTo, flDistTo);
		}
	}

	// Stickies targeting with simplified logic
	if (Vars::Aimbot::General::Target.Value & Vars::Aimbot::General::TargetEnum::Stickies)
	{
		bool bShouldAim = false;
		switch (pWeapon->GetWeaponID())
		{
		case TF_WEAPON_PIPEBOMBLAUNCHER:
		case TF_WEAPON_GRENADELAUNCHER:
			bShouldAim = true;
			break;
		case TF_WEAPON_DIRECTHIT:
		case TF_WEAPON_ROCKETLAUNCHER:
			bShouldAim = true;
			break;
		}

		if (bShouldAim)
		{
			for (auto pEntity : H::Entities.GetGroup(EGroupType::WORLD_PROJECTILES))
			{
				if (F::AimbotGlobal.ShouldIgnore(pEntity, pLocal, pWeapon))
					continue;

				auto pProjectile = pEntity->As<CBaseProjectile>();
				if (!pProjectile || pProjectile->m_iTeamNum() == pLocal->m_iTeamNum())
					continue;

				Vec3 vPos = pEntity->m_vecOrigin();
				Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vPos);
				float flFOVTo = Math::CalcFov(vLocalAngles, vAngleTo);
				if (flFOVTo > Vars::Aimbot::General::AimFOV.Value)
					continue;

				float flDistTo = iSort == Vars::Aimbot::General::TargetSelectionEnum::Distance ? vLocalPos.DistTo(vPos) : 0.f;
				vTargets.emplace_back(pEntity, TargetEnum::Sticky, vPos, vAngleTo, flFOVTo, flDistTo);
			}
		}
	}

	// NPC targeting
	if (Vars::Aimbot::General::Target.Value & Vars::Aimbot::General::TargetEnum::NPCs)
	{
		for (auto pEntity : H::Entities.GetGroup(EGroupType::WORLD_NPC))
		{
			if (F::AimbotGlobal.ShouldIgnore(pEntity, pLocal, pWeapon))
				continue;

			Vec3 vPos = pEntity->GetCenter();
			ProjectilePrediction_t tPrediction = PredictProjectilePosition(pLocal, nullptr, pWeapon);
			if (!tPrediction.bValid)
				continue;

			Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vPos);
			float flFOVTo = Math::CalcFov(vLocalAngles, vAngleTo);
			if (flFOVTo > Vars::Aimbot::General::AimFOV.Value)
				continue;

			float flDistTo = iSort == Vars::Aimbot::General::TargetSelectionEnum::Distance ? vLocalPos.DistTo(vPos) : 0.f;
			vTargets.emplace_back(pEntity, TargetEnum::NPC, vPos, vAngleTo, flFOVTo, flDistTo);
		}
	}

	return vTargets;
}

std::vector<Target_t> CAimbotProjectile::SortTargets(CTFPlayer* pLocal, CTFWeaponBase* pWeapon)
{
	auto vTargets = GetTargets(pLocal, pWeapon);
	if (vTargets.empty())
		return vTargets;

	// Use Linux-internals style simple target selection
	float flSmallestFOV = FLT_MAX;
	float flSmallestDistance = FLT_MAX;
	int nSmallestHealth = INT_MAX;
	int nLargestHealth = INT_MIN;

	Target_t* pBestTarget = nullptr;

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

bool CAimbotProjectile::CanHit(Target_t& tTarget, CTFPlayer* pLocal, CTFWeaponBase* pWeapon)
{
	// Simplified projectile hit detection
	Vec3 vLocalPos = pLocal->GetShootPos();
	const float flMaxRange = powf(pWeapon->GetRange(), 2.f);

	if (vLocalPos.DistToSqr(tTarget.m_vPos) > flMaxRange)
		return false;

	// For players, use prediction
	if (tTarget.m_pEntity->IsPlayer())
	{
		ProjectilePrediction_t tPrediction = PredictProjectilePosition(pLocal, tTarget.m_pEntity->As<CTFPlayer>(), pWeapon);
		if (!tPrediction.bValid)
			return false;

		tTarget.m_vPos = tPrediction.vPredictedPos;
		tTarget.m_vAngleTo = Math::CalcAngle(vLocalPos, tPrediction.vPredictedPos);
		tTarget.m_bBacktrack = false;

		// Add some randomness for splash damage weapons to prevent predictable patterns
		if (pWeapon->GetWeaponID() == TF_WEAPON_ROCKETLAUNCHER ||
			pWeapon->GetWeaponID() == TF_WEAPON_GRENADELAUNCHER ||
			pWeapon->GetWeaponID() == TF_WEAPON_PIPEBOMBLAUNCHER)
		{
			// Small random offset for splash damage area
			float flRandomOffset = Vars::Aimbot::Projectile::PredictionRandomness.Value * 0.01f;
			if (flRandomOffset > 0.0f)
			{
				tTarget.m_vPos.x += (std::rand() % 100 - 50) * flRandomOffset;
				tTarget.m_vPos.y += (std::rand() % 100 - 50) * flRandomOffset;
				tTarget.m_vPos.z += (std::rand() % 50 - 25) * flRandomOffset;
			}
		}

		return true;
	}

	// For buildings and other entities, use simple direct targeting
	return true;
}

bool CAimbotProjectile::Aim(Vec3 vCurAngle, Vec3 vToAngle, Vec3& vOut, int iMethod)
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

void CAimbotProjectile::Aim(CUserCmd* pCmd, Vec3& vAngle, int iMethod)
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

void CAimbotProjectile::Run(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd)
{
	// Check if we should run projectile aimbot
	if (!ShouldRun(pLocal, pWeapon, pCmd))
		return;

	// Check if this is actually a projectile weapon
	if (pWeapon->GetProjectileSpeed() <= 0.0f &&
		pWeapon->GetWeaponID() != TF_WEAPON_PIPEBOMBLAUNCHER &&
		pWeapon->GetWeaponID() != TF_WEAPON_GRENADELAUNCHER &&
		pWeapon->GetWeaponID() != TF_WEAPON_FLAREGUN &&
		pWeapon->GetWeaponID() != TF_WEAPON_COMPOUND_BOW &&
		pWeapon->GetWeaponID() != TF_WEAPON_ROCKETLAUNCHER_DIRECTHIT &&
		pWeapon->GetWeaponID() != TF_WEAPON_ROCKETLAUNCHER &&
		pWeapon->GetWeaponID() != TF_WEAPON_CROSSBOW &&
		pWeapon->GetWeaponID() != TF_WEAPON_SYRINGEGUN_MEDIC)
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

	// Check if we should fire
	if (ShouldFire(pLocal, pWeapon, pCmd, tTarget))
	{
		if (G::CurrentUserCmd)
		{
			// Handle weapon-specific firing logic
			if (pWeapon->GetWeaponID() == TF_WEAPON_COMPOUND_BOW)
			{
				// Handle bow charging
				if (pWeapon->GetChargeBeginTime() == 0.0f)
				{
					G::CurrentUserCmd->buttons |= IN_ATTACK;
				}
				else if (pWeapon->GetChargeBeginTime() > 0.0f)
				{
					float flChargeTime = I::GlobalVars->curtime - pWeapon->GetChargeBeginTime();
					if (flChargeTime >= 1.0f)
					{
						G::CurrentUserCmd->buttons &= ~IN_ATTACK;
					}
				}
			}
			else
			{
				// Normal firing for other projectile weapons
				G::CurrentUserCmd->buttons |= IN_ATTACK;
			}

			F::Ticks.GetTickShift(pWeapon, I::GlobalVars->curtime, pLocal->GetTickBase());
		}

		// Apply aim
		if (Vars::Aimbot::General::AimType.Value != Vars::Aimbot::General::AimTypeEnum::Silent)
			Aim(pCmd, tTarget.m_vAngleTo);
		else
			Aim(pCmd, tTarget.m_vAngleTo);
	}
	else
	{
		// Assistive aiming when not firing
		if (Vars::Aimbot::General::AimType.Value == Vars::Aimbot::General::AimTypeEnum::Smooth ||
			Vars::Aimbot::General::AimType.Value == Vars::Aimbot::General::AimTypeEnum::Assistive)
			Aim(pCmd, tTarget.m_vAngleTo);
	}
}

bool CAimbotProjectile::ShouldFire(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd, const Target_t& tTarget)
{
	if (!G::CurrentUserCmd)
		return false;

	// Check if weapon can fire
	if (!pWeapon->CanPrimaryAttack())
		return false;

	// Handle weapon-specific fire conditions
	switch (pWeapon->GetWeaponID())
	{
	case TF_WEAPON_COMPOUND_BOW:
		// Only fire when bow is sufficiently charged
		if (pWeapon->GetChargeBeginTime() > 0.0f)
		{
			float flChargeTime = I::GlobalVars->curtime - pWeapon->GetChargeBeginTime();
			if (flChargeTime < 0.8f) // Don't fire undercharged arrows
				return false;
		}
		break;

	case TF_WEAPON_DIRECTHIT:
		// Direct hit benefits from precise timing
		if (tTarget.m_pEntity->IsPlayer())
		{
			CTFPlayer* pTarget = tTarget.m_pEntity->As<CTFPlayer>();
			if (pTarget->IsOnGround())
			{
				// Lead ground targets more precisely
				Vec3 vPredicted = tTarget.m_vPos;
				vPredicted.z += pTarget->m_vecVelocity().z * 0.1f;
				if (pLocal->GetShootPos().DistTo(vPredicted) > 800.0f)
					return false; // Too far for reliable direct hit
			}
		}
		break;

	case TF_WEAPON_ROCKETLAUNCHER:
		// Check for rocket jumping prevention
		if (Vars::Aimbot::Projectile::AvoidRockets.Value)
		{
			float flDistanceToTarget = pLocal->GetShootPos().DistTo(tTarget.m_vPos);
			float flSelfDamageRadius = 120.0f; // Rocket splash radius

			if (flDistanceToTarget < flSelfDamageRadius * 1.5f)
				return false; // Too close, would self-damage
		}
		break;
	}

	return true;
}

bool CAimbotProjectile::ShouldRun(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd)
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
	if (!pWeapon->CanPrimaryAttack())
		return false;

	return true;
}