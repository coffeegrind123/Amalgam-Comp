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

	// Validate pointers
	if (!pLocal || !pTarget || !pWeapon)
		return tPrediction;

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
	if (!pLocal)
		return false;

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
		case TF_WEAPON_ROCKETLAUNCHER_DIRECTHIT:
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
		// Validate entity pointer
		if (!tTarget.m_pEntity)
			continue;

		if (tTarget.m_pEntity->IsPlayer())
		{
			CTFPlayer* pTargetPlayer = tTarget.m_pEntity->As<CTFPlayer>();
			if (!pTargetPlayer)
				continue;

			int nHealth = pTargetPlayer->m_iHealth();

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

	// If no best target found but we have targets, return empty (shouldn't happen with valid targets)
	return {};
}

bool CAimbotProjectile::CanHit(Target_t& tTarget, CTFPlayer* pLocal, CTFWeaponBase* pWeapon)
{
	// Validate target entity
	if (!tTarget.m_pEntity)
		return false;

	// Simplified projectile hit detection
	Vec3 vLocalPos = pLocal->GetShootPos();
	const float flMaxRange = powf(pWeapon->GetRange(), 2.f);

	if (vLocalPos.DistToSqr(tTarget.m_vPos) > flMaxRange)
		return false;

	// For players, use prediction
	if (tTarget.m_pEntity->IsPlayer())
	{
		CTFPlayer* pTargetPlayer = tTarget.m_pEntity->As<CTFPlayer>();
		if (!pTargetPlayer)
			return false;

		ProjectilePrediction_t tPrediction = PredictProjectilePosition(pLocal, pTargetPlayer, pWeapon);
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
	case Vars::Aimbot::General::AimTypeEnum::Locking:
		vOut = vToAngle;
		break;
	case Vars::Aimbot::General::AimTypeEnum::Smooth:
		vOut = vCurAngle.LerpAngle(vToAngle, Vars::Aimbot::General::AssistStrength.Value / 100.f);
		bReturn = true;
		break;
	case Vars::Aimbot::General::AimTypeEnum::Assistive:
	{
		// Assistive mode: smoothly blend current view toward target when moving mouse
		// This provides aim assistance without full lock-on
		vOut = vCurAngle.LerpAngle(vToAngle, Vars::Aimbot::General::AssistStrength.Value / 100.f);
		bReturn = true;
		break;
	}
	}

	Math::ClampAngles(vOut);
	return bReturn;
}

void CAimbotProjectile::Aim(CUserCmd* pCmd, Vec3& vAngle, int iMethod)
{
	bool bUnsure = F::Ticks.IsTimingUnsure() || F::Ticks.GetTicks(H::Entities.GetWeapon());

	// For smooth and assistive modes, calculate smoothed angle
	if (iMethod == Vars::Aimbot::General::AimTypeEnum::Smooth)
	{
		Vec3 vCurrentAngle = I::EngineClient->GetViewAngles();

		// Detect target change - if target changed, smooth from last aim angle to new target
		int iCurrentTarget = G::AimTarget.m_iEntIndex;
		bool bTargetChanged = (iCurrentTarget != m_iLastTargetIndex && m_iLastTargetIndex != 0);

		// Use last aim angle as starting point if target just changed, otherwise use current view
		Vec3 vFromAngle = bTargetChanged ? m_vLastAimAngle : vCurrentAngle;

		Vec3 vSmoothedAngle;
		if (Aim(vFromAngle, vAngle, vSmoothedAngle, iMethod))
		{
			pCmd->viewangles = vSmoothedAngle;
			I::EngineClient->SetViewAngles(vSmoothedAngle);

			// Track this target and angle for next frame
			m_iLastTargetIndex = iCurrentTarget;
			m_vLastAimAngle = vSmoothedAngle;
			return;
		}
	}
	else if (iMethod == Vars::Aimbot::General::AimTypeEnum::Assistive)
	{
		// Assistive only works when mouse is actually moving
		Vec3 vMouseDelta = pCmd->viewangles.DeltaAngle(G::LastUserCmd->viewangles);
		float flMouseMovement = vMouseDelta.Length2D();

		// Only assist if there's actual mouse movement (threshold to ignore tiny jitter)
		if (flMouseMovement > 0.1f)
		{
			Vec3 vSmoothedAngle;
			if (Aim(I::EngineClient->GetViewAngles(), vAngle, vSmoothedAngle, iMethod))
			{
				pCmd->viewangles = vSmoothedAngle;
				I::EngineClient->SetViewAngles(vSmoothedAngle);
				return;
			}
		}
		// If no mouse movement, don't aim at all (no tracking)
		return;
	}

	switch (iMethod)
	{
	case Vars::Aimbot::General::AimTypeEnum::Plain:
		// Plain mode: aim when we're attacking OR when autoshoot will attack OR timing is uncertain
		if (G::Attacking == 1 || bUnsure || (pCmd->buttons & IN_ATTACK))
		{
			pCmd->viewangles = vAngle;
			I::EngineClient->SetViewAngles(vAngle);
		}
		break;
	case Vars::Aimbot::General::AimTypeEnum::Smooth:
	case Vars::Aimbot::General::AimTypeEnum::Assistive:
		pCmd->viewangles = vAngle;
		I::EngineClient->SetViewAngles(vAngle);
		break;
	case Vars::Aimbot::General::AimTypeEnum::Silent:
		if (G::Attacking == 1 || bUnsure || (pCmd->buttons & IN_ATTACK))
		{
			SDK::FixMovement(pCmd, vAngle);
			pCmd->viewangles = vAngle;
			G::SilentAngles = true;
		}
		break;
	case Vars::Aimbot::General::AimTypeEnum::Locking:
		SDK::FixMovement(pCmd, vAngle);
		pCmd->viewangles = vAngle;
		G::SilentAngles = true;
	}
}

// ===== Phase 2: Smart Targeting System =====

void CAimbotProjectile::GatherEntities(const char* szClassName, bool bIncludeTeam, CTFPlayer* pLocal, std::vector<ProjTargetData_t>& vOut)
{
	if (!pLocal)
		return;

	for (auto pEntity : H::Entities.GetGroup(EGroupType::PLAYERS_ALL))
	{
		if (!pEntity || pEntity == pLocal)
			continue;

		if (pEntity->IsDormant())
			continue;

		if (!bIncludeTeam && pEntity->m_iTeamNum() == pLocal->m_iTeamNum())
			continue;

		auto pPlayer = pEntity->As<CTFPlayer>();
		if (!pPlayer || !pPlayer->IsAlive() || pPlayer->m_iHealth() <= 0)
			continue;

		if (ShouldIgnoreTarget(pEntity, pLocal))
			continue;

		ProjTargetData_t target = {};
		target.m_pEntity = pEntity;
		target.m_vOrigin = pPlayer->GetAbsOrigin();
		target.m_vVelocity = pPlayer->m_vecVelocity();
		target.m_vMins = pPlayer->m_vecMins();
		target.m_vMaxs = pPlayer->m_vecMaxs();
		target.m_iHealth = pPlayer->m_iHealth();
		target.m_iMaxHealth = pPlayer->GetMaxHealth();
		target.m_iClass = pPlayer->m_iClass();
		target.m_iTeam = pPlayer->m_iTeamNum();
		target.m_bIsUbered = pPlayer->InCond(TF_COND_INVULNERABLE) || pPlayer->InCond(TF_COND_INVULNERABLE_HIDE_UNLESS_DAMAGED);

		vOut.push_back(target);
	}
}

bool CAimbotProjectile::ShouldIgnoreTarget(CBaseEntity* pEntity, CTFPlayer* pLocal)
{
	if (!pEntity || !pLocal)
		return true;

	auto pPlayer = pEntity->As<CTFPlayer>();
	if (!pPlayer)
		return false;

	int ignoreFlags = Vars::Aimbot::Projectile::IgnoreConditions.Value;

	if ((ignoreFlags & Vars::Aimbot::Projectile::IgnoreConditionsEnum::Cloaked) && pPlayer->InCond(TF_COND_STEALTHED))
		return true;

	if ((ignoreFlags & Vars::Aimbot::Projectile::IgnoreConditionsEnum::Disguised) && pPlayer->InCond(TF_COND_DISGUISED))
		return true;

	if ((ignoreFlags & Vars::Aimbot::Projectile::IgnoreConditionsEnum::Ubercharged) &&
		(pPlayer->InCond(TF_COND_INVULNERABLE) || pPlayer->InCond(TF_COND_INVULNERABLE_HIDE_UNLESS_DAMAGED)))
		return true;

	if ((ignoreFlags & Vars::Aimbot::Projectile::IgnoreConditionsEnum::Bonked) && pPlayer->InCond(TF_COND_PHASE))
		return true;

	if ((ignoreFlags & Vars::Aimbot::Projectile::IgnoreConditionsEnum::Taunting) && pPlayer->InCond(TF_COND_TAUNTING))
		return true;

	if ((ignoreFlags & Vars::Aimbot::Projectile::IgnoreConditionsEnum::Friends) && H::Entities.IsFriend(pEntity->entindex()))
		return true;

	if ((ignoreFlags & Vars::Aimbot::Projectile::IgnoreConditionsEnum::Kritzkrieged) && pPlayer->InCond(TF_COND_CRITBOOSTED))
		return true;

	if ((ignoreFlags & Vars::Aimbot::Projectile::IgnoreConditionsEnum::Vaccinated) &&
		(pPlayer->InCond(TF_COND_MEDIGUN_UBER_BULLET_RESIST) ||
		 pPlayer->InCond(TF_COND_MEDIGUN_UBER_BLAST_RESIST) ||
		 pPlayer->InCond(TF_COND_MEDIGUN_UBER_FIRE_RESIST)))
		return true;

	return false;
}

float CAimbotProjectile::CalculateScore(const ProjTargetData_t& target, const Vec3& vEyePos, const Vec3& vViewAngles, bool bIncludeTeam, CTFPlayer* pLocal)
{
	if (!pLocal)
		return 0.f;

	float flScore = 0.f;
	int weightFlags = Vars::Aimbot::Projectile::TargetWeights.Value;

	if ((weightFlags & Vars::Aimbot::Projectile::TargetWeightsEnum::Distance) && Vars::Aimbot::Projectile::DistanceWeight.Value > 0.f)
	{
		float flMaxDist = Vars::Aimbot::Projectile::MaxDistance.Value;
		float flDistScore = 1.f - std::min(target.m_flDistance / flMaxDist, 1.f);
		flScore += flDistScore * Vars::Aimbot::Projectile::DistanceWeight.Value;
	}

	if ((weightFlags & Vars::Aimbot::Projectile::TargetWeightsEnum::Health) && Vars::Aimbot::Projectile::HealthWeight.Value > 0.f)
	{
		float flHealthScore = 1.f - std::min((float)target.m_iHealth / (float)target.m_iMaxHealth, 1.f);
		flScore += flHealthScore * Vars::Aimbot::Projectile::HealthWeight.Value;
	}

	if ((weightFlags & Vars::Aimbot::Projectile::TargetWeightsEnum::FOV) && Vars::Aimbot::Projectile::FOVWeight.Value > 0.f)
	{
		Vec3 vAngleTo = ProjAimMath::PositionAngles(vEyePos, target.m_vFinalPos.IsZero() ? target.m_vOrigin : target.m_vFinalPos);
		float flFOV = ProjAimMath::AngleFov(vViewAngles, vAngleTo);
		float flFOVScore = 1.f - std::min(flFOV / Vars::Aimbot::General::AimFOV.Value, 1.f);
		flScore += flFOVScore * Vars::Aimbot::Projectile::FOVWeight.Value;
	}

	if ((weightFlags & Vars::Aimbot::Projectile::TargetWeightsEnum::Visibility) && Vars::Aimbot::Projectile::VisibilityWeight.Value > 0.f)
	{
		flScore += Vars::Aimbot::Projectile::VisibilityWeight.Value;
	}

	if ((weightFlags & Vars::Aimbot::Projectile::TargetWeightsEnum::Speed) && Vars::Aimbot::Projectile::SpeedWeight.Value > 0.f)
	{
		float flSpeed = target.m_vVelocity.Length();
		float flMaxSpeed = 400.f;
		float flSpeedScore = 1.f - std::min(flSpeed / flMaxSpeed, 1.f);
		flScore += flSpeedScore * Vars::Aimbot::Projectile::SpeedWeight.Value;
	}

	if (target.m_iClass == TF_CLASS_MEDIC && Vars::Aimbot::Projectile::MedicPriority.Value > 0.f)
		flScore += Vars::Aimbot::Projectile::MedicPriority.Value;

	if (target.m_iClass == TF_CLASS_SNIPER && Vars::Aimbot::Projectile::SniperPriority.Value > 0.f)
		flScore += Vars::Aimbot::Projectile::SniperPriority.Value;

	if (target.m_bIsUbered && Vars::Aimbot::Projectile::UberPenalty.Value != 0.f)
		flScore += Vars::Aimbot::Projectile::UberPenalty.Value;

	if (bIncludeTeam && target.m_iTeam == pLocal->m_iTeamNum())
		flScore += 5.f;

	return flScore;
}

std::vector<ProjTargetData_t> CAimbotProjectile::GetTargetsSmart(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, const ProjectileInfo* pWeaponInfo, bool bIncludeTeam)
{
	std::vector<ProjTargetData_t> vTargets;

	if (!pLocal || !pWeapon || !pWeaponInfo)
		return vTargets;

	std::vector<ProjTargetData_t> vCandidates;
	GatherEntities("CTFPlayer", bIncludeTeam, pLocal, vCandidates);

	if (vCandidates.empty())
		return vTargets;

	Vec3 vLocalPos = pLocal->GetAbsOrigin();
	Vec3 vEyePos = pLocal->GetShootPos();
	Vec3 vViewAngles = I::EngineClient->GetViewAngles();
	float flMaxDist = Vars::Aimbot::Projectile::MaxDistance.Value;

	for (auto& candidate : vCandidates)
	{
		float flDist = (candidate.m_vOrigin - vLocalPos).Length();
		if (flDist > flMaxDist)
			continue;

		candidate.m_flDistance = flDist;

		Vec3 vAngleTo = ProjAimMath::PositionAngles(vEyePos, candidate.m_vOrigin);
		float flFOV = ProjAimMath::AngleFov(vViewAngles, vAngleTo);
		if (flFOV > Vars::Aimbot::General::AimFOV.Value)
			continue;

		candidate.m_flFOV = flFOV;

		// Use SEOwnedDE time-based prediction - simulates tick-by-tick and matches projectile travel time
		// This will automatically find the right position where projectile arrival matches player position
		if (!SolveProjectileTarget(pLocal, pWeapon, pWeaponInfo, candidate, nullptr))
		{
			// Could not solve projectile path to target
			continue;
		}

		// candidate.m_vFinalPos and candidate.m_flTimeToHit are set by SolveProjectileTarget

		if (candidate.m_flTimeToHit > Vars::Aimbot::Projectile::MaxSimulationTime.Value)
			continue;

		// Run multipoint for final visibility check
		Vec3 vMultipointPos;
		if (RunMultipoint(candidate.m_pEntity, pWeapon, pWeaponInfo, vEyePos, candidate.m_vFinalPos, vMultipointPos))
		{
			candidate.m_vFinalPos = vMultipointPos;
		}
		else
		{
			// Not visible, skip this target
			continue;
		}

		candidate.m_flScore = CalculateScore(candidate, vEyePos, vViewAngles, bIncludeTeam, pLocal);

		// Only filter by MinScore if it's above 0 (allows disabling the filter)
		if (Vars::Aimbot::Projectile::MinScore.Value > 0.f && candidate.m_flScore < Vars::Aimbot::Projectile::MinScore.Value)
			continue;

		vTargets.push_back(candidate);
	}

	// Sort by score (highest first)
	std::sort(vTargets.begin(), vTargets.end(), [](const ProjTargetData_t& a, const ProjTargetData_t& b) {
		return a.m_flScore > b.m_flScore;
	});

	// Limit to max targets
	int maxTargets = Vars::Aimbot::Projectile::MaxTargets.Value;
	if (maxTargets > 0 && (int)vTargets.size() > maxTargets)
		vTargets.resize(maxTargets);

	return vTargets;
}

// ===== Phase 3: Multipoint & Simulation =====

bool CAimbotProjectile::RunMultipoint(CBaseEntity* pTarget, CTFWeaponBase* pWeapon, const ProjectileInfo* pWeaponInfo, const Vec3& vEyePos, const Vec3& vPredictedPos, Vec3& vOut)
{
	if (!pTarget || !pWeapon || !pWeaponInfo)
		return false;

	// Determine which z-offsets to use based on weapon type
	static const float normalOffsets[] = { 0.5f, 0.7f, 0.9f, 0.4f, 0.2f };
	static const float huntsmanOffsets[] = { 0.9f, 0.7f, 0.5f, 0.4f, 0.2f };
	static const float splashOffsets[] = { 0.2f, 0.4f, 0.5f, 0.7f, 0.9f };

	const float* offsets = normalOffsets;
	int offsetCount = 5;

	int weaponID = pWeapon->GetWeaponID();
	bool bHuntsman = (weaponID == TF_WEAPON_COMPOUND_BOW);
	bool bSplash = (weaponID == TF_WEAPON_ROCKETLAUNCHER ||
					weaponID == TF_WEAPON_PIPEBOMBLAUNCHER ||
					weaponID == TF_WEAPON_GRENADELAUNCHER);

	if (bHuntsman)
		offsets = huntsmanOffsets;
	else if (bSplash && pWeaponInfo->m_flDamageRadius > 0.f)
		offsets = splashOffsets;

	Vec3 vTargetMaxs = pTarget->m_vecMaxs();

	// Use consistent fraction threshold (like hitscan and Linux)
	float flRequiredFraction = 0.95f;

	// Try each multipoint offset
	for (int i = 0; i < offsetCount; i++)
	{
		float zOffset = vTargetMaxs.z * offsets[i];
		Vec3 vTestPos = vPredictedPos;
		vTestPos.z += zOffset;

		// Trace from eye to multipoint
		CTraceFilterHitscan filter;
		filter.pSkip = H::Entities.GetLocal();
		trace_t trace;
		CTraceFilterWorldAndPropsOnly filterWorld;
		SDK::Trace(vEyePos, vTestPos, MASK_SHOT, &filterWorld, &trace);

		// Use distance-based fraction threshold (same as HealthBarESP)
		if (trace.fraction > flRequiredFraction || !trace.DidHit())
		{
			vOut = vTestPos;
			return true;
		}
	}

	// If no multipoint visible, still use center (don't reject the target entirely)
	vOut = vPredictedPos;
	vOut.z += vTargetMaxs.z * 0.5f;
	return true;  // Changed from false to true - accept center point even if not fully visible
}

// SEOwnedDE-style time-based projectile solving
bool CAimbotProjectile::SolveProjectileTarget(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, const ProjectileInfo* pWeaponInfo, ProjTargetData_t& target, CUserCmd* pCmd)
{
	if (!target.m_pEntity || !target.m_pEntity->IsPlayer())
		return false;

	auto pPlayer = target.m_pEntity->As<CTFPlayer>();
	if (!pPlayer)
		return false;

	Vec3 vShootPos = pLocal->GetShootPos();

	// Get projectile info
	float flChargeTime = pWeaponInfo->GetChargeTime(pWeapon);
	Vec3 vVelocity = pWeaponInfo->GetVelocity(flChargeTime);
	float flSpeed = vVelocity.Length2D();
	float flGravity = 800.f * pWeaponInfo->GetGravity(flChargeTime);

	bool bDucked = pPlayer->m_fFlags() & FL_DUCKING;
	bool bOnGround = pPlayer->m_fFlags() & FL_ONGROUND;

	// Initialize movement simulation - SEOwnedDE approach: no fallback
	// If we can't simulate movement, we can't do proper time-based prediction
	MoveStorage moveData;
	if (!F::MoveSim.Initialize(pPlayer, moveData, false, true))
		return false;

	target.m_vSimPath.clear();

	// Simulate tick-by-tick like SEOwnedDE
	int maxTicks = TIME_TO_TICKS(Vars::Aimbot::Projectile::MaxSimulationTime.Value);
	for (int nTick = 0; nTick < maxTicks; nTick++)
	{
		target.m_vSimPath.push_back(moveData.m_MoveData.m_vecAbsOrigin);

		// Run simulation tick
		F::MoveSim.RunTick(moveData, true);

		if (moveData.m_bFailed)
			break;

		Vec3 vTargetPos = moveData.m_MoveData.m_vecAbsOrigin;

		// Offset for hitbox (center of player)
		Vec3 vMins = pPlayer->m_vecMins();
		Vec3 vMaxs = pPlayer->m_vecMaxs();
		vTargetPos.z += (vMins.z + vMaxs.z) * 0.5f;

		// Calculate projectile travel time to this position
		Vec3 vAimAngles;
		float flTimeToTarget = 0.f;
		if (!ProjAimMath::SolveBallisticArc(vShootPos, vTargetPos, flSpeed, flGravity, vAimAngles, flTimeToTarget))
			continue;

		// Add latency
		float flLatency = (I::EngineClient->GetNetChannelInfo() ? I::EngineClient->GetNetChannelInfo()->GetLatency(FLOW_OUTGOING) : 0.f);
		int nTargetTick = TIME_TO_TICKS(flTimeToTarget + flLatency);

		// Handle sticky arm time
		if (pWeapon->GetWeaponID() == TF_WEAPON_PIPEBOMBLAUNCHER)
		{
			float flStickyArmTime = SDK::AttribHookValue(0.8f, "sticky_arm_time", pLocal);
			if (TICKS_TO_TIME(nTargetTick) < flStickyArmTime)
			{
				nTargetTick += TIME_TO_TICKS(fabsf(flTimeToTarget - flStickyArmTime));
			}
		}

		// Check if projectile travel time matches simulation time
		if (nTargetTick == nTick || nTargetTick == nTick - 1)
		{
			// Try splash damage for rockets
			if (pWeapon->GetWeaponID() == TF_WEAPON_ROCKETLAUNCHER ||
				pWeapon->GetWeaponID() == TF_WEAPON_ROCKETLAUNCHER_DIRECTHIT)
			{
				if (GenerateSplashPoints(pLocal, pWeapon, pWeaponInfo, target, vShootPos))
				{
					F::MoveSim.Restore(moveData);
					return true;
				}
			}

			// Direct hit - store position, time, and aim angles
			target.m_vFinalPos = vTargetPos;
			target.m_flTimeToHit = flTimeToTarget;
			target.m_vAimAngles = vAimAngles;
			F::MoveSim.Restore(moveData);
			return true;
		}
	}

	F::MoveSim.Restore(moveData);
	return false;
}

// SEOwnedDE-style splash damage sphere generation
bool CAimbotProjectile::GenerateSplashPoints(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, const ProjectileInfo* pWeaponInfo, ProjTargetData_t& target, const Vec3& vShootPos)
{
	if (!target.m_pEntity || !target.m_pEntity->IsPlayer())
		return false;

	auto pPlayer = target.m_pEntity->As<CTFPlayer>();
	if (!pPlayer)
		return false;

	// Get current simulated position
	Vec3 vCenter = target.m_vFinalPos;

	// Determine splash radius based on weapon
	float flRadius = 180.f; // Default rocket launcher radius
	if (pWeapon->GetWeaponID() == TF_WEAPON_ROCKETLAUNCHER_DIRECTHIT)
		flRadius = 80.f;

	// Generate sphere of points (Fibonacci sphere distribution)
	int numPoints = 80;
	std::vector<Vec3> potentialPoints;

	for (int n = 0; n < numPoints; n++)
	{
		float a1 = acosf(1.0f - 2.0f * (static_cast<float>(n) / static_cast<float>(numPoints)));
		float a2 = (static_cast<float>(M_PI) * (3.0f - sqrtf(5.0f))) * static_cast<float>(n);

		Vec3 offset = Vec3(sinf(a1) * cosf(a2), sinf(a1) * sinf(a2), cosf(a1));
		Vec3 point = vCenter + (offset * flRadius);

		// Trace from center to point to find wall/floor
		CGameTrace trace = {};
		CTraceFilterWorldAndPropsOnly filter = {};
		SDK::Trace(vCenter, point, MASK_SOLID, &filter, &trace);

		// If we hit something, that's a potential splash point
		if (trace.fraction < 0.99f)
		{
			potentialPoints.push_back(trace.endpos);
		}
	}

	if (potentialPoints.empty())
		return false;

	// Sort by distance to target - closest splash points are best
	std::sort(potentialPoints.begin(), potentialPoints.end(), [&](const Vec3& a, const Vec3& b) {
		return a.DistTo(target.m_vFinalPos) < b.DistTo(target.m_vFinalPos);
	});

	// Get projectile info for solving
	float flChargeTime = pWeaponInfo->GetChargeTime(pWeapon);
	Vec3 vVelocity = pWeaponInfo->GetVelocity(flChargeTime);
	float flSpeed = vVelocity.Length2D();
	float flGravity = 800.f * pWeaponInfo->GetGravity(flChargeTime);

	// Try each splash point
	for (const auto& point : potentialPoints)
	{
		Vec3 vAimAngles;
		float flTime = 0.f;

		if (!ProjAimMath::SolveBallisticArc(vShootPos, point, flSpeed, flGravity, vAimAngles, flTime))
			continue;

		// Verify we can actually shoot to this point
		CGameTrace trace = {};
		CTraceFilterWorldAndPropsOnly filter = {};
		SDK::TraceHull(vShootPos, point, Vec3(-4.f, -4.f, -4.f), Vec3(4.f, 4.f, 4.f), MASK_SOLID, &filter, &trace);

		if (trace.fraction > 0.99f || !trace.allsolid)
		{
			target.m_vFinalPos = point;
			target.m_flTimeToHit = flTime;
			target.m_vAimAngles = vAimAngles;
			return true;
		}
	}

	return false;
}

// ===== Phase 5: Visuals =====

void CAimbotProjectile::DrawVisuals()
{
	if (!m_bHasVisuals)
		return;

	int visualFlags = Vars::Aimbot::Projectile::Visuals.Value;
	float flTime = I::GlobalVars->curtime + Vars::Aimbot::Projectile::VisualsTime.Value;

	// Draw player movement path
	if ((visualFlags & Vars::Aimbot::Projectile::VisualsEnum::PlayerPath) && !m_vPlayerPath.empty())
	{
		DrawPath_t path;
		path.m_vPath = m_vPlayerPath;
		path.m_flTime = flTime;
		path.m_tColor = {136, 192, 208, 255}; // Cyan
		path.m_iStyle = 0;
		path.m_bZBuffer = false;
		G::PathStorage.push_back(path);
	}

	// Draw projectile trajectory path
	if ((visualFlags & Vars::Aimbot::Projectile::VisualsEnum::ProjectilePath) && !m_vProjectilePath.empty())
	{
		DrawPath_t path;
		path.m_vPath = m_vProjectilePath;
		path.m_flTime = flTime;
		path.m_tColor = {235, 203, 139, 255}; // Gold
		path.m_iStyle = 0;
		path.m_bZBuffer = false;
		G::PathStorage.push_back(path);
	}

	// Draw target hitbox
	if ((visualFlags & Vars::Aimbot::Projectile::VisualsEnum::TargetHitbox) && m_CurrentTarget.m_pEntity)
	{
		DrawBox_t box;
		box.m_vOrigin = m_CurrentTarget.m_vFinalPos.IsZero() ? m_CurrentTarget.m_vOrigin : m_CurrentTarget.m_vFinalPos;
		box.m_vMins = m_CurrentTarget.m_vMins;
		box.m_vMaxs = m_CurrentTarget.m_vMaxs;
		box.m_vAngles = Vec3(0, 0, 0);
		box.m_flTime = flTime;
		box.m_tColorEdge = {136, 192, 208, 255}; // Cyan edge
		box.m_tColorFace = {136, 192, 208, 50};  // Transparent cyan face
		box.m_bZBuffer = false;
		G::BoxStorage.push_back(box);
	}

	// Reset visuals flag after drawing
	m_bHasVisuals = false;
}

void CAimbotProjectile::Run(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd)
{
	// Phase 5: Draw visuals from previous tick
	DrawVisuals();

	// Check if aim type is Off
	if (Vars::Aimbot::General::AimType.Value == Vars::Aimbot::General::AimTypeEnum::Off)
		return;

	// Check if we should run projectile aimbot
	if (!ShouldRun(pLocal, pWeapon, pCmd))
		return;

	m_bRunning = false;
	G::AimTarget.m_iEntIndex = 0;
	G::AimTarget = {};

	// Phase 2: Use smart targeting if enabled
	if (Vars::Aimbot::Projectile::SmartTargeting.Value)
	{
		int itemDefIndex = pWeapon->m_iItemDefinitionIndex();
		const ProjectileInfo* pWeaponInfo = ProjWeaponInfo::GetProjectileInfo(itemDefIndex);

		if (pWeaponInfo)
		{
			// Check if this weapon can target teammates (crossbow, sandvich, etc.)
			bool bIncludeTeam = (pWeapon->GetWeaponID() == TF_WEAPON_CROSSBOW);

			auto vSmartTargets = GetTargetsSmart(pLocal, pWeapon, pWeaponInfo, bIncludeTeam);
			if (!vSmartTargets.empty())
			{
				ProjTargetData_t& target = vSmartTargets.front();

				// Validate target entity before using
				if (!target.m_pEntity || target.m_pEntity->IsDormant())
					return;

				m_bRunning = true;
				G::AimTarget.m_iEntIndex = target.m_pEntity->entindex();

				Vec3 vEyePos = pLocal->GetShootPos();
				Vec3 vTargetPos = target.m_vFinalPos.IsZero() ? target.m_vOrigin : target.m_vFinalPos;

				// Validate positions aren't invalid
				if (!vEyePos.IsValid() || !vTargetPos.IsValid())
					return;

				// Validate we have aim angles from SolveProjectileTarget
				// Note: aim angles CAN be near-zero if target is directly in front, so we check IsValid not IsZero
				if (!target.m_vAimAngles.IsValid())
					return;

				// Use the pre-calculated aim angles from SolveProjectileTarget
				Vec3 vAimAngles = target.m_vAimAngles;
				float flTime = target.m_flTimeToHit;
				float flChargeTime = pWeaponInfo->GetChargeTime(pWeapon);

				// Phase 5: Store visual data
				m_CurrentTarget = target;
				m_vPlayerPath = target.m_vSimPath;
				m_bHasVisuals = true;

				// Phase 5: Simulate projectile trajectory if enabled
				if (Vars::Aimbot::Projectile::Visuals.Value & Vars::Aimbot::Projectile::VisualsEnum::ProjectilePath)
				{
					ProjectileSimulationInfo projInfo;
					if (F::ProjSim.GetInfo(pLocal, pWeapon, vAimAngles, projInfo, ProjSimEnum::Trace))
					{
						// Initialize and simulate projectile
						if (F::ProjSim.Initialize(projInfo, true, false))
						{
							m_vProjectilePath.clear();
							m_vProjectilePath.push_back(projInfo.m_vPos);

							// Simulate up to target time or max 100 ticks (clamped for safety)
							int maxTicks = std::clamp(static_cast<int>(flTime / I::GlobalVars->interval_per_tick) + 10, 1, 100);
							for (int i = 0; i < maxTicks; i++)
							{
								// Safety: validate position is still valid
								if (!projInfo.m_vPos.IsValid())
									break;

								F::ProjSim.RunTick(projInfo, true);
								if (!projInfo.m_vPath.empty())
									m_vProjectilePath = projInfo.m_vPath;

								// Stop if we're close to target or path is long enough
								if ((projInfo.m_vPos - vTargetPos).Length() < 50.f || projInfo.m_vPath.size() > 100)
									break;
							}
						}
					}
				}

				// Validate trajectory if enabled
				if (Vars::Aimbot::Projectile::ValidateTrajectory.Value)
				{
					// TODO: Check if projectile path actually hits the target
					// For now, we trust the ballistic solver
				}

				G::AimTarget.m_iTickCount = I::GlobalVars->tickcount;
				G::AimTarget.m_iDuration = 1;

				// Phase 4: Auto shoot with charge weapon support
				bool bShouldShoot = Vars::Aimbot::General::AutoShoot.Value;
				if (bShouldShoot && pCmd)
				{
					if (pWeaponInfo->m_bCharges)
					{
						// Charge weapon handling (Huntsman, Loose Cannon)
						if (flChargeTime < 0.01f)
						{
							// Just started charging, hold attack
							pCmd->buttons |= IN_ATTACK;
							G::Attacking = 1;
						}
						else
						{
							// Charging in progress, release to fire
							pCmd->buttons &= ~IN_ATTACK;
						}
					}
					else if (pWeapon->GetWeaponID() == TF_WEAPON_COMPOUND_BOW)
					{
						// Huntsman without charge
						if (Vars::Aimbot::Projectile::Modifiers.Value & Vars::Aimbot::Projectile::ModifiersEnum::AutoScope && !pLocal->IsScoped())
						{
							pCmd->buttons |= IN_ATTACK2;
						}
						else
						{
							pCmd->buttons |= IN_ATTACK;
							G::Attacking = 1;
						}
					}
					else
					{
						pCmd->buttons |= IN_ATTACK;
						G::Attacking = 1;
					}
				}

				// Apply aim - works with autoshoot OR manual firing
				// For Plain mode to work without autoshoot, we need G::Attacking set or IN_ATTACK held
				Aim(pCmd, vAimAngles, Vars::Aimbot::General::AimType.Value);
				return;
			}
		}
	}

	// Fallback: If smart targeting is disabled OR didn't find anything, use simple targeting
	// This ensures the aimbot works even for unsupported weapons or when smart targeting fails
	if (!Vars::Aimbot::Projectile::SmartTargeting.Value || !m_bRunning)
	{
		auto vTargets = SortTargets(pLocal, pWeapon);
		if (!vTargets.empty())
		{
			Target_t& tTarget = vTargets.front();

			// Validate target entity
			if (!tTarget.m_pEntity)
				return;

			m_bRunning = true;
			G::AimTarget.m_iEntIndex = tTarget.m_pEntity->entindex();
			G::AimTarget.m_iTickCount = I::GlobalVars->tickcount;
			G::AimTarget.m_iDuration = 1;

			// Simple aiming - just point at the target position
			Aim(pCmd, tTarget.m_vAngleTo, Vars::Aimbot::General::AimType.Value);

			// Auto shoot if enabled
			if (Vars::Aimbot::General::AutoShoot.Value && pCmd)
			{
				pCmd->buttons |= IN_ATTACK;
				G::Attacking = 1;
			}
		}
	}
}

bool CAimbotProjectile::ShouldFire(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd, const Target_t& tTarget)
{
	// Check if auto shoot is enabled
	if (!Vars::Aimbot::General::AutoShoot.Value)
		return false;

	if (!G::CurrentUserCmd)
		return false;

	// Check if weapon can fire
	if (!pWeapon->CanPrimaryAttack())
		return false;

	// Handle weapon-specific fire conditions
	switch (pWeapon->GetWeaponID())
	{
	case TF_WEAPON_COMPOUND_BOW:
		// Compound bow - simplified check
		break;

	case TF_WEAPON_ROCKETLAUNCHER_DIRECTHIT:
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
		if (Vars::Aimbot::Projectile::Modifiers.Value & Vars::Aimbot::Projectile::ModifiersEnum::AvoidRockets)
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

	// Don't check CanPrimaryAttack here - we want to aim even if weapon is on cooldown
	// The ShouldFire function will check if we can actually shoot

	return true;
}
float CAimbotProjectile::GetSplashRadius(CTFWeaponBase* pWeapon, CTFPlayer* pPlayer)
{
	if (!pWeapon || !pPlayer)
		return 0.f;

	float flRadius = 0.f;
	switch (pWeapon->GetWeaponID())
	{
	case TF_WEAPON_ROCKETLAUNCHER:
	case TF_WEAPON_ROCKETLAUNCHER_DIRECTHIT:
	case TF_WEAPON_PARTICLE_CANNON:
	case TF_WEAPON_PIPEBOMBLAUNCHER:
		flRadius = 146.f;
		break;
	case TF_WEAPON_FLAREGUN:
	case TF_WEAPON_FLAREGUN_REVENGE:
		if (pWeapon->As<CTFFlareGun>()->GetFlareGunType() == FLAREGUN_SCORCHSHOT)
			flRadius = 110.f;
		break;
	}
	
	if (!flRadius)
		return 0.f;

	flRadius = SDK::AttribHookValue(flRadius, "mult_explosion_radius", pWeapon);
	switch (pWeapon->GetWeaponID())
	{
	case TF_WEAPON_ROCKETLAUNCHER:
	case TF_WEAPON_ROCKETLAUNCHER_DIRECTHIT:
	case TF_WEAPON_PARTICLE_CANNON:
		if (pPlayer->InCond(TF_COND_BLASTJUMPING) && SDK::AttribHookValue(1.f, "rocketjump_attackrate_bonus", pWeapon) != 1.f)
			flRadius *= 0.8f;
		break;
	}
	
	return flRadius * Vars::Aimbot::Projectile::SplashRadius.Value / 100;
}

float CAimbotProjectile::GetSplashRadius(CBaseEntity* pProjectile, CTFWeaponBase* pWeapon, CTFPlayer* pPlayer, float flScale, CTFWeaponBase* pAirblast)
{
	if (!pProjectile)
		return 0.f;

	float flRadius = 0.f;
	if (pAirblast)
	{
		pWeapon = pAirblast;
		pPlayer = pWeapon->m_hOwner()->As<CTFPlayer>();
	}
	
	switch (pProjectile->GetClassID())
	{
	case ETFClassID::CTFWeaponBaseGrenadeProj:
	case ETFClassID::CTFWeaponBaseMerasmusGrenade:
	case ETFClassID::CTFProjectile_Rocket:
	case ETFClassID::CTFProjectile_SentryRocket:
	case ETFClassID::CTFProjectile_EnergyBall:
		flRadius = 146.f;
		break;
	case ETFClassID::CTFGrenadePipebombProjectile:
		if (pProjectile->As<CTFGrenadePipebombProjectile>()->HasStickyEffects())
			flRadius = 146.f;
		break;
	case ETFClassID::CTFProjectile_Flare:
		if (pWeapon && pWeapon->As<CTFFlareGun>()->GetFlareGunType() == FLAREGUN_SCORCHSHOT)
			flRadius = 110.f;
		break;
	}
	
	if (pPlayer && pWeapon)
	{
		flRadius = SDK::AttribHookValue(flRadius, "mult_explosion_radius", pWeapon);
		switch (pProjectile->GetClassID())
		{
		case ETFClassID::CTFProjectile_Rocket:
		case ETFClassID::CTFProjectile_SentryRocket:
			if (pPlayer->InCond(TF_COND_BLASTJUMPING) && SDK::AttribHookValue(1.f, "rocketjump_attackrate_bonus", pWeapon) != 1.f)
				flRadius *= 0.8f;
			break;
		}
	}
	
	return flRadius * flScale;
}

bool CAimbotProjectile::AutoAirblast(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd, CBaseEntity* pProjectile)
{
	if (!pLocal || !pWeapon || !pCmd || !pProjectile)
		return false;

	// Simplified autoairblast - just returns false for now
	// Full implementation would require complete projectile prediction system
	return false;
}
