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
		Vec3 vMouseDelta = G::CurrentUserCmd->viewangles.DeltaAngle(G::LastUserCmd->viewangles);
		Vec3 vTargetDelta = vToAngle.DeltaAngle(G::LastUserCmd->viewangles);
		float flMouseDelta = vMouseDelta.Length2D(), flTargetDelta = vTargetDelta.Length2D();
		vTargetDelta = vTargetDelta.Normalized() * std::min(flMouseDelta, flTargetDelta);
		vOut = vCurAngle - vMouseDelta + vMouseDelta.LerpAngle(vTargetDelta, Vars::Aimbot::General::AssistStrength.Value / 100.f);
		bReturn = true;
		break;
	}

	Math::ClampAngles(vOut);
	return bReturn;
}

void CAimbotProjectile::Aim(CUserCmd* pCmd, Vec3& vAngle, int iMethod)
{
	bool bUnsure = F::Ticks.IsTimingUnsure() || F::Ticks.GetTicks(H::Entities.GetWeapon());

	switch (iMethod)
	{
	case Vars::Aimbot::General::AimTypeEnum::Plain:
		if (G::Attacking != 1 && !bUnsure)
			break;
		[[fallthrough]];
	case Vars::Aimbot::General::AimTypeEnum::Smooth:
	case Vars::Aimbot::General::AimTypeEnum::Assistive:
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
		target.m_vOrigin = pEntity->GetAbsOrigin();
		target.m_vVelocity = pEntity->m_vecVelocity();
		target.m_vMins = pEntity->m_vecMins();
		target.m_vMaxs = pEntity->m_vecMaxs();
		target.m_iHealth = pPlayer->m_iHealth();
		target.m_iMaxHealth = pPlayer->GetMaxHealth();
		target.m_iClass = pPlayer->m_iClass();
		target.m_iTeam = pEntity->m_iTeamNum();
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

		Vec3 vVelocity = pWeaponInfo->GetVelocity(0.f);
		float flProjSpeed = vVelocity.Length2D();
		float flTravelTime = flDist / (flProjSpeed > 0.f ? flProjSpeed : 1.f);

		// Phase 3: Use player movement simulation
		if (!SimulatePlayerMovement(candidate, pLocal, flTravelTime))
		{
			// Fallback to simple linear prediction
			candidate.m_vFinalPos = candidate.m_vOrigin + candidate.m_vVelocity * flTravelTime;
		}

		candidate.m_flTimeToHit = flTravelTime;

		if (flTravelTime > Vars::Aimbot::Projectile::MaxSimulationTime.Value)
			continue;

		// Phase 3: Run multipoint for visibility check and optimal aim position
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

		if (candidate.m_flScore < Vars::Aimbot::Projectile::MinScore.Value)
			continue;

		vTargets.push_back(candidate);
	}

	std::sort(vTargets.begin(), vTargets.end(), [](const ProjTargetData_t& a, const ProjTargetData_t& b) {
		return a.m_flScore > b.m_flScore;
	});

	int maxTargets = Vars::Aimbot::Projectile::MaxTargets.Value;
	if ((int)vTargets.size() > maxTargets)
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
		Utils::Trace(vEyePos, vTestPos, MASK_SHOT, &filterWorld, &trace);

		if (trace.fraction >= 0.99f)
		{
			vOut = vTestPos;
			return true;
		}
	}

	// If no multipoint visible, use center
	vOut = vPredictedPos;
	vOut.z += vTargetMaxs.z * 0.5f;
	return false;
}

bool CAimbotProjectile::SimulatePlayerMovement(ProjTargetData_t& target, CTFPlayer* pLocal, float flTravelTime)
{
	if (!target.m_pEntity || !pLocal)
		return false;

	auto pPlayer = target.m_pEntity->As<CTFPlayer>();
	if (!pPlayer)
		return false;

	// Use existing MovementSimulation if velocity is significant
	if (target.m_vVelocity.Length() < 10.f)
	{
		target.m_vFinalPos = target.m_vOrigin;
		target.m_vSimPath.push_back(target.m_vOrigin);
		return true;
	}

	// Use Performance Mode for simpler prediction
	if (Vars::Aimbot::Projectile::PerformanceMode.Value)
	{
		// Simple linear prediction
		target.m_vFinalPos = target.m_vOrigin + target.m_vVelocity * flTravelTime;
		target.m_vSimPath.push_back(target.m_vOrigin);
		target.m_vSimPath.push_back(target.m_vFinalPos);
		return true;
	}

	// Advanced prediction with MovementSimulation
	MoveStorage moveData;
	if (!F::MoveSim.Initialize(pPlayer, moveData, false, true))
	{
		// Fallback to linear if simulation fails
		target.m_vFinalPos = target.m_vOrigin + target.m_vVelocity * flTravelTime;
		return true;
	}

	// Simulate ticks
	int simTicks = static_cast<int>((flTravelTime / I::GlobalVars->interval_per_tick) + 0.5f);
	simTicks = std::min(simTicks, 64); // Cap at 64 ticks

	for (int i = 0; i < simTicks; i++)
	{
		F::MoveSim.RunTick(moveData, true);

		if (moveData.m_bFailed)
			break;
	}

	if (!moveData.m_bFailed && !moveData.m_vPath.empty())
	{
		target.m_vFinalPos = moveData.m_vPath.back();
		target.m_vSimPath = moveData.m_vPath;
	}
	else
	{
		// Fallback to linear if simulation failed
		target.m_vFinalPos = target.m_vOrigin + target.m_vVelocity * flTravelTime;
	}

	F::MoveSim.Restore(moveData);
	return true;
}

void CAimbotProjectile::Run(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd)
{
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
				m_bRunning = true;
				G::AimTarget.m_iEntIndex = target.m_pEntity->entindex();

				Vec3 vEyePos = pLocal->GetShootPos();
				Vec3 vTargetPos = target.m_vFinalPos.IsZero() ? target.m_vOrigin : target.m_vFinalPos;

				// Use ballistic solver from Phase 1
				Vec3 vVelocity = pWeaponInfo->GetVelocity(0.f);
				float flSpeed = vVelocity.Length2D();
				float flGravity = 800.f * pWeaponInfo->GetGravity(0.f);

				Vec3 vAimAngles;
				float flTime = 0.f;
				if (ProjAimMath::SolveBallisticArc(vEyePos, vTargetPos, flSpeed, flGravity, vAimAngles, flTime))
				{
					G::AimTarget.m_iTickCount = I::GlobalVars->tickcount;
					G::AimTarget.m_iDuration = 1;

					// Apply aim
					Aim(pCmd, vAimAngles, Vars::Aimbot::General::AimType.Value);

					// Auto shoot if enabled
					if (Vars::Aimbot::Projectile::AutoShoot.Value && G::CurrentUserCmd)
					{
						if (pWeapon->GetWeaponID() == TF_WEAPON_COMPOUND_BOW)
						{
							if (Vars::Aimbot::Projectile::Modifiers.Value & Vars::Aimbot::Projectile::ModifiersEnum::AutoScope && !pLocal->IsScoped())
							{
								G::CurrentUserCmd->buttons |= IN_ATTACK2;
							}
							else
							{
								G::CurrentUserCmd->buttons |= IN_ATTACK;
							}
						}
						else
						{
							G::CurrentUserCmd->buttons |= IN_ATTACK;
						}
					}
					return;
				}
			}
		}
	}

	// Fall back to original targeting system
	auto vTargets = SortTargets(pLocal, pWeapon);
	if (vTargets.empty())
		return;

	Target_t& tTarget = vTargets.front();
	m_bRunning = true;
	G::AimTarget.m_iEntIndex = tTarget.m_pEntity->entindex();

	if (!CanHit(tTarget, pLocal, pWeapon))
		return;

	G::AimTarget.m_iEntIndex = tTarget.m_pEntity->entindex();
	G::AimTarget.m_iTickCount = I::GlobalVars->tickcount;
	G::AimTarget.m_iDuration = 1;

	// Determine if we should fire
	bool bShouldFire = ShouldFire(pLocal, pWeapon, pCmd, tTarget);

	if (bShouldFire && G::CurrentUserCmd)
	{
		// Handle weapon-specific firing logic
		if (pWeapon->GetWeaponID() == TF_WEAPON_COMPOUND_BOW)
		{
			// Check if we need to scope
			if (Vars::Aimbot::Projectile::Modifiers.Value & Vars::Aimbot::Projectile::ModifiersEnum::AutoScope && !pLocal->IsScoped())
			{
				G::CurrentUserCmd->buttons |= IN_ATTACK2;
				bShouldFire = false; // Don't fire yet, scope first
			}
			else
			{
				// Handle bow charging
				G::CurrentUserCmd->buttons |= IN_ATTACK;
			}
		}
		else
		{
			// Normal firing for other projectile weapons
			G::CurrentUserCmd->buttons |= IN_ATTACK;
		}
	}

	// Apply aim based on aim type
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

	// Linux-internals style checks
	if (!pWeapon->CanPrimaryAttack())
		return false;

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
