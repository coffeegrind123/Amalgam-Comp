#include "AimbotProjectile.h"

#include "../Aimbot.h"
#include "../../Simulation/MovementSimulation/MovementSimulation.h"
#include "../../Simulation/ProjectileSimulation/ProjectileSimulation.h"
#include "../../Ticks/Ticks.h"
#include "../../Visuals/Visuals.h"
#include "../AutoAirblast/AutoAirblast.h"

//#define SPLASH_DEBUG1 // normal splash visualization
//#define SPLASH_DEBUG2 // obstructed splash visualization
//#define SPLASH_DEBUG3 // simple splash visualization
//#define SPLASH_DEBUG4 // points visualization
//#define SPLASH_DEBUG5 // trace visualization
//#define SPLASH_DEBUG6 // trace count

#ifdef SPLASH_DEBUG6
static std::unordered_map<std::string, int> mTraceCount = {};
#endif

static inline Vec3 GetSimulatedPos(const MoveStorage& tStorage)
{
	const Vec3 vSim = tStorage.m_MoveData.m_vecAbsOrigin;
	if (tStorage.m_flPredictedDelta <= 0.f)
		return vSim;

	const float flIntervalStart = tStorage.m_flPredictedSimTime - tStorage.m_flPredictedDelta;
	const float flProgress = (tStorage.m_flSimTime - flIntervalStart) / tStorage.m_flPredictedDelta;
	const float t = std::clamp(flProgress, 0.f, 1.f);
	return tStorage.m_vPredictedOrigin.Lerp(vSim, t);
}

struct RestoreInfo_t
{
	Vec3 m_vOrigin;
	Vec3 m_vMins;
	Vec3 m_vMaxs;
};

std::vector<Target_t> CAimbotProjectile::GetTargets(CTFPlayer* pLocal, CTFWeaponBase* pWeapon)
{
	std::vector<Target_t> vTargets;
	const auto iSort = Vars::Aimbot::General::TargetSelection.Value;

	// CRITICAL FIX: Use pLocal->GetShootPos() directly instead of cached Ticks position
	const Vec3 vLocalPos = pLocal->GetShootPos();
	const Vec3 vLocalAngles = I::EngineClient->GetViewAngles();

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
			if (!pEntity || pEntity->IsDormant())
				continue;
			bool bTeammate = pEntity->m_iTeamNum() == pLocal->m_iTeamNum();
			if (F::AimbotGlobal.ShouldIgnore(pEntity, pLocal, pWeapon))
				continue;

			if (bTeammate)
			{
				if (pEntity->As<CTFPlayer>()->m_iHealth() >= pEntity->As<CTFPlayer>()->GetMaxHealth()
					|| Vars::Aimbot::Healing::FriendsOnly.Value && !H::Entities.IsFriend(pEntity->entindex()) && !H::Entities.InParty(pEntity->entindex()))
					continue;
			}

			float flFOVTo; Vec3 vPos, vAngleTo;
			if (!F::AimbotGlobal.PlayerBoneInFOV(pEntity->As<CTFPlayer>(), vLocalPos, vLocalAngles, flFOVTo, vPos, vAngleTo))
				continue;

			float flDistTo = iSort == Vars::Aimbot::General::TargetSelectionEnum::Distance ? vLocalPos.DistTo(vPos) : 0.f;
			vTargets.emplace_back(pEntity, TargetEnum::Player, vPos, vAngleTo, flFOVTo, flDistTo, bTeammate ? 0 : F::AimbotGlobal.GetPriority(pEntity->entindex()));
		}

		if (pWeapon->GetWeaponID() == TF_WEAPON_LUNCHBOX)
			return vTargets;
	}

	if (Vars::Aimbot::General::Target.Value)
	{
		bool bIsRescueRanger = pWeapon->GetWeaponID() == TF_WEAPON_SHOTGUN_BUILDING_RESCUE;
		for (auto pEntity : H::Entities.GetGroup(bIsRescueRanger ? EGroupType::BUILDINGS_ALL : EGroupType::BUILDINGS_ENEMIES))
		{
			if (!pEntity || pEntity->IsDormant())
				continue;
			if (F::AimbotGlobal.ShouldIgnore(pEntity, pLocal, pWeapon))
				continue;

			if (pEntity->m_iTeamNum() == pLocal->m_iTeamNum() && pEntity->As<CBaseObject>()->m_iHealth() >= pEntity->As<CBaseObject>()->m_iMaxHealth())
				continue;

			Vec3 vPos = pEntity->GetCenter();
			Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vPos);
			float flFOVTo = Math::CalcFovScaled(vLocalPos, vPos, vLocalAngles);
			if (flFOVTo > Vars::Aimbot::General::AimFOV.Value)
				continue;

			float flDistTo = iSort == Vars::Aimbot::General::TargetSelectionEnum::Distance ? vLocalPos.DistTo(vPos) : 0.f;
			vTargets.emplace_back(pEntity, pEntity->IsSentrygun() ? TargetEnum::Sentry : pEntity->IsDispenser() ? TargetEnum::Dispenser : TargetEnum::Teleporter, vPos, vAngleTo, flFOVTo, flDistTo);
		}
	}

	if (Vars::Aimbot::General::Target.Value & Vars::Aimbot::General::TargetEnum::Stickies)
	{
		bool bShouldAim = false;
		switch (pWeapon->GetWeaponID())
		{
		case TF_WEAPON_PIPEBOMBLAUNCHER:
			if (SDK::AttribHookValue(0, "stickies_detonate_stickies", pWeapon) == 1)
				bShouldAim = true;
			break;
		case TF_WEAPON_FLAREGUN:
		case TF_WEAPON_FLAREGUN_REVENGE:
			if (pWeapon->As<CTFFlareGun>()->GetFlareGunType() == FLAREGUN_SCORCHSHOT)
				bShouldAim = true;
		}

		if (bShouldAim)
		{
			for (auto pEntity : H::Entities.GetGroup(EGroupType::WORLD_PROJECTILES))
			{
				if (!pEntity || pEntity->IsDormant())
					continue;
				if (F::AimbotGlobal.ShouldIgnore(pEntity, pLocal, pWeapon))
					continue;

				Vec3 vPos = pEntity->GetCenter();
				Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vPos);
				float flFOVTo = Math::CalcFovScaled(vLocalPos, vPos, vLocalAngles);
				if (flFOVTo > Vars::Aimbot::General::AimFOV.Value)
					continue;

				float flDistTo = iSort == Vars::Aimbot::General::TargetSelectionEnum::Distance ? vLocalPos.DistTo(vPos) : 0.f;
				vTargets.emplace_back(pEntity, TargetEnum::Sticky, vPos, vAngleTo, flFOVTo, flDistTo);
			}
		}
	}

	if (Vars::Aimbot::General::Target.Value & Vars::Aimbot::General::TargetEnum::NPCs) // does not predict movement
	{
		for (auto pEntity : H::Entities.GetGroup(EGroupType::WORLD_NPC))
		{
			if (!pEntity || pEntity->IsDormant())
				continue;
			Vec3 vPos = pEntity->GetCenter();
			Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vPos);
			float flFOVTo = Math::CalcFovScaled(vLocalPos, vPos, vLocalAngles);
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

	F::AimbotGlobal.SortTargets(vTargets, Vars::Aimbot::General::TargetSelection.Value);
	vTargets.resize(std::min(size_t(Vars::Aimbot::General::MaxTargets.Value), vTargets.size()));
	F::AimbotGlobal.SortPriority(vTargets);
	return vTargets;
}



float CAimbotProjectile::GetSplashRadius(CTFWeaponBase* pWeapon, CTFPlayer* pPlayer)
{
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
	}
	return flRadius * Vars::Aimbot::Projectile::SplashRadius.Value / 100;
}

float CAimbotProjectile::GetSplashRadius(CBaseEntity* pProjectile, CTFWeaponBase* pWeapon, CTFPlayer* pPlayer, float flScale, CTFWeaponBase* pAirblast)
{
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
		}
	}
	return flRadius * flScale;
}

static inline int GetSplashMode(CTFWeaponBase* pWeapon)
{
	if (Vars::Aimbot::Projectile::RocketSplashMode.Value)
	{
		switch (pWeapon->GetWeaponID())
		{
		case TF_WEAPON_ROCKETLAUNCHER:
		case TF_WEAPON_ROCKETLAUNCHER_DIRECTHIT:
		case TF_WEAPON_PARTICLE_CANNON:
			return Vars::Aimbot::Projectile::RocketSplashMode.Value;
		}
	}

	return Vars::Aimbot::Projectile::RocketSplashModeEnum::Regular;
}

static inline float PrimeTime(CTFWeaponBase* pWeapon)
{
	if (Vars::Aimbot::Projectile::Modifiers.Value & Vars::Aimbot::Projectile::ModifiersEnum::UsePrimeTime && pWeapon->GetWeaponID() == TF_WEAPON_PIPEBOMBLAUNCHER)
	{
		static auto tf_grenadelauncher_livetime = H::ConVars.FindVar("tf_grenadelauncher_livetime");
		const float flLiveTime = tf_grenadelauncher_livetime->GetFloat();
		return SDK::AttribHookValue(flLiveTime, "sticky_arm_time", pWeapon);
	}

	return 0.f;
}

static inline int GetHitboxPriority(int nHitbox, Target_t& tTarget, Info_t& tInfo, CBaseEntity* pProjectile = nullptr)
{
	if (Vars::Aimbot::Projectile::Hitboxes.Value & Vars::Aimbot::Projectile::HitboxesEnum::Auto)
	{
		bool bLower = Vars::Aimbot::Projectile::Hitboxes.Value & Vars::Aimbot::Projectile::HitboxesEnum::PrioritizeFeet
			&& tTarget.m_iTargetType == TargetEnum::Player && tTarget.m_pEntity->As<CTFPlayer>()->IsOnGround() /*&& tInfo.m_flRadius*/;

		bool bHeadshot = Vars::Aimbot::Projectile::Hitboxes.Value & Vars::Aimbot::Projectile::HitboxesEnum::Head
			&& tTarget.m_iTargetType == TargetEnum::Player;
		if (bHeadshot)
		{
			if (!pProjectile)
				bHeadshot = tInfo.m_pWeapon->GetWeaponID() == TF_WEAPON_COMPOUND_BOW;
			else
				bHeadshot = pProjectile->GetClassID() == ETFClassID::CTFProjectile_Arrow && pProjectile->As<CTFProjectile_Arrow>()->CanHeadshot();
		}
		if (Vars::Aimbot::Projectile::Hitboxes.Value & Vars::Aimbot::Projectile::HitboxesEnum::BodyaimIfLethal
			&& bHeadshot && !pProjectile && tInfo.m_pWeapon->m_hOwner().GetEntryIndex() == I::EngineClient->GetLocalPlayer())
		{
			float flCharge = I::GlobalVars->curtime - tInfo.m_pWeapon->As<CTFPipebombLauncher>()->m_flChargeBeginTime();
			float flDamage = Math::RemapVal(flCharge, 0.f, 1.f, 50.f, 120.f);
			if (tInfo.m_pLocal->IsMiniCritBoosted())
				flDamage *= 1.36f;
			if (flDamage >= tTarget.m_pEntity->As<CTFPlayer>()->m_iHealth())
				bHeadshot = false;

			if (tInfo.m_pLocal->IsCritBoosted()) // for reliability
				bHeadshot = false;
		}

		if (bHeadshot)
			tTarget.m_nAimedHitbox = HITBOX_HEAD;

		switch (nHitbox)
		{
		case 0: return Vars::Aimbot::Projectile::Hitboxes.Value & Vars::Aimbot::Projectile::HitboxesEnum::Head ? (bHeadshot ? 0 : 2) : 3;
		case 1: return Vars::Aimbot::Projectile::Hitboxes.Value & Vars::Aimbot::Projectile::HitboxesEnum::Body ? (bHeadshot ? 3 : (bLower ? 1 : 0)) : 3;
		case 2: return Vars::Aimbot::Projectile::Hitboxes.Value & Vars::Aimbot::Projectile::HitboxesEnum::Feet ? (bHeadshot ? 3 : (bLower ? 0 : 1)) : 3;
		}
	}
	else
	{
		switch (nHitbox)
		{
		case 0: return Vars::Aimbot::Projectile::Hitboxes.Value & Vars::Aimbot::Projectile::HitboxesEnum::Head ? 0 : 3;
		case 1: return Vars::Aimbot::Projectile::Hitboxes.Value & Vars::Aimbot::Projectile::HitboxesEnum::Body ? 1 : 3;
		case 2: return Vars::Aimbot::Projectile::Hitboxes.Value & Vars::Aimbot::Projectile::HitboxesEnum::Feet ? 2 : 3;
		}
	}

	return 3;
};

std::unordered_map<int, Vec3> CAimbotProjectile::GetDirectPoints(Target_t& tTarget, Info_t& tInfo, CBaseEntity* pProjectile)
{
	std::unordered_map<int, Vec3> mPoints = {};

	const Vec3 vMins = tTarget.m_pEntity->m_vecMins(), vMaxs = tTarget.m_pEntity->m_vecMaxs();
	for (int i = 0; i < 3; i++)
	{
		const int iPriority = GetHitboxPriority(i, tTarget, tInfo, pProjectile);
		if (iPriority == 3)
			continue;

		switch (i)
		{
		case 0:
			if (tTarget.m_nAimedHitbox == HITBOX_HEAD)
			{
				auto aBones = H::Entities.GetBones(tTarget.m_pEntity->entindex());
				if (!aBones)
					break;

				//Vec3 vOff = tTarget.m_pEntity->As<CBaseAnimating>()->GetHitboxOrigin(aBones, HITBOX_HEAD) - tTarget.m_pEntity->m_vecOrigin();

				// https://www.youtube.com/watch?v=_PSGD-pJUrM, might be better??
				Vec3 vCenter, vBBoxMins, vBBoxMaxs; tTarget.m_pEntity->As<CBaseAnimating>()->GetHitboxInfo(aBones, HITBOX_HEAD, &vCenter, &vBBoxMins, &vBBoxMaxs);
				Vec3 vOff = vCenter + (vBBoxMins + vBBoxMaxs) / 2 - tTarget.m_pEntity->m_vecOrigin();

				float flLow = 0.f;
				Vec3 vDelta = tTarget.m_vPos + tInfo.m_vTargetEye - tInfo.m_vLocalEye;
				if (vDelta.z > 0)
				{
					float flXY = vDelta.Length2D();
					if (flXY)
						flLow = Math::RemapVal(vDelta.z / flXY, 0.f, 0.5f, 0.f, 1.f);
					else
						flLow = 1.f;
				}

				float flLerp = (Vars::Aimbot::Projectile::HuntsmanLerp.Value + (Vars::Aimbot::Projectile::HuntsmanLerpLow.Value - Vars::Aimbot::Projectile::HuntsmanLerp.Value) * flLow) / 100.f;
				float flAdd = Vars::Aimbot::Projectile::HuntsmanAdd.Value + (Vars::Aimbot::Projectile::HuntsmanAddLow.Value - Vars::Aimbot::Projectile::HuntsmanAdd.Value) * flLow;
				vOff.z += flAdd;
				vOff.z = vOff.z + (vMaxs.z - vOff.z) * flLerp;

				vOff.x = std::clamp(vOff.x, vMins.x + Vars::Aimbot::Projectile::HuntsmanClamp.Value, vMaxs.x - Vars::Aimbot::Projectile::HuntsmanClamp.Value);
				vOff.y = std::clamp(vOff.y, vMins.y + Vars::Aimbot::Projectile::HuntsmanClamp.Value, vMaxs.y - Vars::Aimbot::Projectile::HuntsmanClamp.Value);
				vOff.z = std::clamp(vOff.z, vMins.z + Vars::Aimbot::Projectile::HuntsmanClamp.Value, vMaxs.z - Vars::Aimbot::Projectile::HuntsmanClamp.Value);
				mPoints[iPriority] = vOff;
			}
			else
				mPoints[iPriority] = Vec3(0, 0, vMaxs.z - Vars::Aimbot::Projectile::VerticalShift.Value);
			break;
		case 1: mPoints[iPriority] = Vec3(0, 0, (vMaxs.z - vMins.z) / 2); break;
		case 2: mPoints[iPriority] = Vec3(0, 0, vMins.z + Vars::Aimbot::Projectile::VerticalShift.Value); break;
		}
	}

	return mPoints;
}

// seode
static inline std::vector<std::pair<Vec3, int>> ComputeSphere(float flRadius, int iSamples, float flNthroot)
{
	std::vector<std::pair<Vec3, int>> vPoints;
	vPoints.reserve(iSamples + 1);

	// pasted from northern, thanks
	const float flRotateX = (Vars::Aimbot::Projectile::SplashRotateX.Value < 0.f) ?
		SDK::StdRandomFloat(0.f, 360.f) :
		Vars::Aimbot::Projectile::SplashRotateX.Value;

	const float flRotateY = (Vars::Aimbot::Projectile::SplashRotateY.Value < 0.f) ?
		SDK::StdRandomFloat(0.f, 360.f) :
		Vars::Aimbot::Projectile::SplashRotateY.Value;

	// precompute rotation matrix
	float sp, sy, cp, cy;
	Math::SinCos(DEG2RAD(flRotateX), &sp, &cp);
	Math::SinCos(DEG2RAD(flRotateY), &sy, &cy);

	const Vec3 vX(cy * cp, -sy, cy * sp);
	const Vec3 vY(sy * cp, cy, sy * sp);
	const Vec3 vZ(-sp, 0.f, cp);

	int iPointType = Vars::Aimbot::Projectile::SplashGrates.Value ?
		(PointTypeEnum::Regular | PointTypeEnum::Obscured) :
		PointTypeEnum::Regular;

	if (Vars::Aimbot::Projectile::RocketSplashMode.Value ==
		Vars::Aimbot::Projectile::RocketSplashModeEnum::SpecialHeavy) {
		iPointType |= (PointTypeEnum::ObscuredExtra | PointTypeEnum::ObscuredMulti);
	}

	const float GOLDEN_ANGLE = 2.399963229728653f;
	const float invSamples = 1.0f / iSamples;

	for (int i = 0; i < iSamples; ++i) {
		const float t = (i + 0.5f) * invSamples;

		const float z_unit = 1.0f - 2.0f * t;
		const float sinPhi = sqrtf(1.0f - z_unit * z_unit);

		const float theta = GOLDEN_ANGLE * i;
		float sinTheta, cosTheta;
		Math::SinCos(theta, &sinTheta, &cosTheta);

		const float x_unit = cosTheta * sinPhi;
		const float y_unit = sinTheta * sinPhi;
		// z_unit is already calculated

		// manual rotation
		const float x_final = (vX.x * x_unit + vX.y * y_unit + vX.z * z_unit) * flRadius;
		const float y_final = (vY.x * x_unit + vY.y * y_unit + vY.z * z_unit) * flRadius;
		const float z_final = (vZ.x * x_unit + vZ.y * y_unit + vZ.z * z_unit) * flRadius;

		vPoints.emplace_back(Vec3(x_final, y_final, z_final), iPointType);
	}

	// same here
	const float bx = vX.z * -flRadius;
	const float by = vY.z * -flRadius;
	const float bz = vZ.z * -flRadius;
	vPoints.emplace_back(Vec3(bx, by, bz), iPointType);

	return vPoints;
};

template <class T>
static inline void TracePoint(Vec3& vPoint, int& iType, Vec3& vTargetEye, Info_t& tInfo, T& vPoints, std::function<bool(CGameTrace& trace, bool& bErase, bool& bNormal)> checkPoint, int i = 0)
{
	const int iOriginalType = iType;
	size_t iStartSize = vPoints.size();

	bool bErase = false;
	bool bNormal = false;
	CGameTrace trace;
	CTraceFilterWorldAndPropsOnly filter;

#if defined(SPLASH_DEBUG1) || defined(SPLASH_DEBUG2)
	auto DrawTrace = [](bool bSuccess, Color_t tColor, CGameTrace& trace) {
		Vec3 vMins = Vec3(-1, -1, -1) / (bSuccess ? 1 : 2);
		Vec3 vMaxs = Vec3(1, 1, 1) / (bSuccess ? 1 : 2);
		Vec3 vAngles = Math::VectorAngles(trace.plane.normal);

		G::BoxStorage.emplace_back(trace.endpos, vMins, vMaxs, vAngles,
			I::GlobalVars->curtime + 60.f,
			tColor.Alpha(tColor.a / (bSuccess ? 1 : 10)),
			Color_t(0, 0, 0, 0));

		G::LineStorage.emplace_back(std::pair<Vec3, Vec3>(trace.startpos, trace.endpos),
			I::GlobalVars->curtime + 60.f,
			tColor.Alpha(tColor.a / (bSuccess ? 1 : 10)));
	};
#endif

	if (iType & PointTypeEnum::Regular)
	{
		SDK::Trace(tInfo.m_vLocalEye, vPoint, MASK_SOLID, &filter, &trace);

#ifdef SPLASH_DEBUG6
		s_mTraceCount["Splash regular"]++;
#endif


		if (checkPoint(trace, bErase, bNormal))
		{

			if (i % Vars::Aimbot::Projectile::SplashNormalSkip.Value)
				vPoints.pop_back();

#ifdef SPLASH_DEBUG1
			else if (!bNormal)
				DrawTrace(false, Vars::Colors::Local.Value, trace);
#endif
		}


		if (bErase)
		{
			iType = 0;
			return;
		}
		else if (bNormal)
		{
			iType &= ~PointTypeEnum::Regular;
		}
		else
		{
			iType &= ~PointTypeEnum::Obscured;
		}
	}

	if (iType & PointTypeEnum::ObscuredExtra)
	{
		bErase = false;
		bNormal = false;


		Vec3 toTarget = vTargetEye - tInfo.m_vLocalEye;
		Vec3 toPoint = vPoint - vTargetEye;
		if (toTarget.Dot(toPoint) > 0.f)
		{
			bNormal = true;
		}
		else
		{
			if (!(iOriginalType & PointTypeEnum::Regular))
			{
				SDK::Trace(tInfo.m_vLocalEye, vPoint, MASK_SOLID | CONTENTS_NOSTARTSOLID, &filter, &trace);

#ifdef SPLASH_DEBUG6
				s_mTraceCount["Splash rocket (2)"]++;
#endif

				bNormal = !trace.m_pEnt || trace.fraction > 1.f || trace.allsolid || trace.startsolid;

#ifdef SPLASH_DEBUG2
				DrawTrace(!bNormal, Vars::Colors::IndicatorTextMid.Value, trace);
#endif
			}

			if (!bNormal)
			{

				if (trace.m_pEnt && (trace.m_pEnt->GetClassID() != ETFClassID::CWorld || trace.hitbox))
					filter.pSkip = trace.m_pEnt;


				Vec3 startPos = trace.endpos - (vTargetEye - vPoint).Normalized();
				SDK::Trace(startPos, vPoint, MASK_SOLID | CONTENTS_NOSTARTSOLID, &filter, &trace);
				filter.pSkip = nullptr;

#ifdef SPLASH_DEBUG6
				s_mTraceCount["Splash rocket (2, 2)"]++;
#endif


				bNormal = trace.fraction > 1.f || trace.allsolid || trace.startsolid ||
					(trace.startpos - trace.endpos).IsZero() ||
					trace.surface.flags & SURF_SKY;

#ifdef SPLASH_DEBUG2
				DrawTrace(!bNormal, Vars::Colors::IndicatorTextBad.Value, trace);
#endif

				if (!bNormal && checkPoint(trace, bErase, bNormal))
				{
					SDK::Trace(trace.endpos + trace.plane.normal, vTargetEye,
						MASK_SHOT | CONTENTS_NOSTARTSOLID, &filter, &trace);

#ifdef SPLASH_DEBUG6
					s_mTraceCount["Splash rocket check (2)"]++;
#endif

#ifdef SPLASH_DEBUG2
					DrawTrace(trace.fraction >= 1.f, Vars::Colors::IndicatorTextMisc.Value, trace);
#endif



					if (trace.DidHit())
						vPoints.pop_back();
				}
			}
		}


		if (vPoints.size() != iStartSize)
		{
			iType = 0;
		}
		else if (bErase || bNormal)
		{
			iType &= ~PointTypeEnum::ObscuredExtra;
		}
	}



	if (iType & PointTypeEnum::Obscured)
	{
		bErase = false;
		bNormal = false;

		Vec3 toTarget = vTargetEye - tInfo.m_vLocalEye;
		Vec3 toPoint = vPoint - vTargetEye;
		if (toTarget.Dot(toPoint) > 0.f)
		{
			bNormal = true;
		}

		else if (tInfo.m_iSplashMode == Vars::Aimbot::Projectile::RocketSplashModeEnum::Regular)
		{

			SDK::Trace(vPoint, vTargetEye, MASK_SHOT | CONTENTS_NOSTARTSOLID, &filter, &trace);

#ifdef SPLASH_DEBUG6
			s_mTraceCount["Splash grate check"]++;
#endif

			bNormal = trace.DidHit();

#ifdef SPLASH_DEBUG2
			DrawTrace(!bNormal, Vars::Colors::IndicatorGood.Value, trace);
#endif

			if (!bNormal)
			{
				SDK::Trace(vPoint, vTargetEye, MASK_SOLID | CONTENTS_NOSTARTSOLID, &filter, &trace);

#ifdef SPLASH_DEBUG6
				s_mTraceCount["Splash grate"]++;
#endif

				checkPoint(trace, bErase, bNormal);

#ifdef SPLASH_DEBUG2
				DrawTrace(!bNormal, Vars::Colors::Local.Value, trace);
#endif
			}
		}
		else
		{

			SDK::Trace(vPoint, vTargetEye, MASK_SOLID | CONTENTS_NOSTARTSOLID, &filter, &trace);

#ifdef SPLASH_DEBUG6
			s_mTraceCount["Splash rocket (1)"]++;
#endif



			bNormal = trace.fraction > 1.f || trace.allsolid || trace.startsolid ||
				trace.surface.flags & SURF_SKY;

#ifdef SPLASH_DEBUG2
			DrawTrace(!bNormal, Vars::Colors::IndicatorMid.Value, trace);
#endif



			if (!bNormal && (trace.surface.flags & SURF_NODRAW))
			{
				if (iType & PointTypeEnum::ObscuredMulti)
				{
					CGameTrace trace2;
					Vec3 startPos = trace.endpos - (vPoint - vTargetEye).Normalized();
					SDK::Trace(startPos, vTargetEye, MASK_SOLID | CONTENTS_NOSTARTSOLID, &filter, &trace2);

#ifdef SPLASH_DEBUG6
					s_mTraceCount["Splash rocket (1, 2)"]++;
#endif

					bNormal = trace2.fraction > 1.f || trace2.allsolid || trace2.startsolid ||
						(trace2.startpos - trace2.endpos).IsZero() ||
						trace2.surface.flags & (SURF_NODRAW | SURF_SKY);

#ifdef SPLASH_DEBUG2
					DrawTrace(!bNormal, Vars::Colors::IndicatorTextBad.Value, trace2);
#endif

					if (!bNormal)
						trace = trace2;
				}
				else
				{
					bNormal = true;
				}
			}
			if (!bNormal && checkPoint(trace, bErase, bNormal))
			{
				SDK::Trace(trace.endpos + trace.plane.normal, vTargetEye,
					MASK_SHOT | CONTENTS_NOSTARTSOLID, &filter, &trace);

#ifdef SPLASH_DEBUG6
				s_mTraceCount["Splash rocket check (1)"]++;
#endif

#ifdef SPLASH_DEBUG2
				DrawTrace(trace.fraction >= 1.f, Vars::Colors::IndicatorTextMisc.Value, trace);
#endif



				if (trace.DidHit())
					vPoints.pop_back();
			}
		}


		if (vPoints.size() != iStartSize)
		{
			iType = 0;
		}
		else if (bErase || bNormal)
		{
			iType &= ~PointTypeEnum::Obscured;
		}
		else
		{

			iType &= ~PointTypeEnum::Regular;
		}
	}
}

// possibly add air splash for autodet weapons
std::vector<Point_t> CAimbotProjectile::GetSplashPoints(Target_t& tTarget, std::vector<std::pair<Vec3, int>>& vSpherePoints, Info_t& tInfo, int iSimTime)
{
	std::vector<std::pair<Point_t, float>> vPointDistances = {};

	Vec3 vTargetEye = tTarget.m_vPos + tInfo.m_vTargetEye;

	auto checkPoint = [&](CGameTrace& trace, bool& bErase, bool& bNormal)
		{	
			bErase = !trace.m_pEnt || trace.fraction == 1.f || trace.surface.flags & SURF_SKY || !trace.m_pEnt->GetAbsVelocity().IsZero();
			if (bErase)
				return false;

			Point_t tPoint = { trace.endpos, {} };
			if (!tInfo.m_flGravity)
			{
				Vec3 vForward = (tInfo.m_vLocalEye - trace.endpos).Normalized();
				bNormal = vForward.Dot(trace.plane.normal) <= 0;
			}
			if (!bNormal)
			{
				CalculateAngle(tInfo.m_vLocalEye, tPoint.m_vPoint, tInfo, iSimTime, tPoint.m_Solution);
				if (tInfo.m_flGravity)
				{
					Vec3 vPos = tInfo.m_vLocalEye + Vec3(0, 0, (tInfo.m_flGravity * 800.f * pow(tPoint.m_Solution.m_flTime, 2)) / 2);
					Vec3 vForward = (vPos - tPoint.m_vPoint).Normalized();
					bNormal = vForward.Dot(trace.plane.normal) <= 0;
				}
			}
			if (bNormal)
				return false;

			bErase = tPoint.m_Solution.m_iCalculated == CalculatedEnum::Good;
			if (!bErase || !tInfo.m_flPrimeTime && int(tPoint.m_Solution.m_flTime / TICK_INTERVAL) + 1 != iSimTime)
				return false;

			vPointDistances.emplace_back(tPoint, tPoint.m_vPoint.DistTo(tTarget.m_vPos));
			return true;
		};

	int i = 0;
	for (auto it = vSpherePoints.begin(); it != vSpherePoints.end();)
	{
		Vec3 vPoint = it->first + vTargetEye;
		int& iType = it->second;

		Solution_t solution; CalculateAngle(tInfo.m_vLocalEye, vPoint, tInfo, iSimTime, solution, false);
		
		if (solution.m_iCalculated == CalculatedEnum::Bad)
			iType = 0;
		else if (abs(solution.m_flTime - TICKS_TO_TIME(iSimTime)) < tInfo.m_flRadiusTime || tInfo.m_flPrimeTime && iSimTime == tInfo.m_iPrimeTime)
			TracePoint(vPoint, iType, vTargetEye, tInfo, vPointDistances, checkPoint, i++);

		if (!(iType & ~PointTypeEnum::ObscuredMulti))
			it = vSpherePoints.erase(it);
		else
			++it;
	}

	std::sort(vPointDistances.begin(), vPointDistances.end(), [&](const auto& a, const auto& b) -> bool
		{
			return a.second < b.second;
		});

	std::vector<Point_t> vPoints = {};
	int iSplashCount = std::min(
		tInfo.m_flPrimeTime && iSimTime == tInfo.m_iPrimeTime ? Vars::Aimbot::Projectile::SplashCountDirect.Value : tInfo.m_iSplashCount,
		int(vPointDistances.size())
	);
	for (int i = 0; i < iSplashCount; i++)
		vPoints.push_back(vPointDistances[i].first);

	const Vec3 vOriginal = tTarget.m_pEntity->GetAbsOrigin();
	tTarget.m_pEntity->SetAbsOrigin(tTarget.m_vPos);
	for (auto it = vPoints.begin(); it != vPoints.end();)
	{
		auto& vPoint = *it;
		bool bValid = vPoint.m_Solution.m_iCalculated != CalculatedEnum::Pending;
		if (bValid)
		{
			Vec3 vPos; tTarget.m_pEntity->m_Collision()->CalcNearestPoint(vPoint.m_vPoint, &vPos);
			bValid = vPoint.m_vPoint.DistTo(vPos) < tInfo.m_flRadius;
		}

		if (bValid)
			++it;
		else
			it = vPoints.erase(it);
	}
	tTarget.m_pEntity->SetAbsOrigin(vOriginal);

	return vPoints;
}

void CAimbotProjectile::SetupSplashPoints(Target_t& tTarget, std::vector<std::pair<Vec3, int>>& vSpherePoints, Info_t& tInfo, std::vector<std::pair<Vec3, Vec3>>& vSimplePoints)
{
	vSimplePoints.clear();
	Vec3 vTargetEye = tTarget.m_vPos + tInfo.m_vTargetEye;

	auto checkPoint = [&](CGameTrace& trace, bool& bErase, bool& bNormal)
		{
			bErase = !trace.m_pEnt || trace.fraction == 1.f || trace.surface.flags & SURF_SKY || !trace.m_pEnt->GetAbsVelocity().IsZero();
			if (bErase)
				return false;

			Point_t tPoint = { trace.endpos, {} };
			if (!tInfo.m_flGravity)
			{
				Vec3 vForward = (tInfo.m_vLocalEye - trace.endpos).Normalized();
				bNormal = vForward.Dot(trace.plane.normal) <= 0;
			}
			if (!bNormal)
			{
				CalculateAngle(tInfo.m_vLocalEye, tPoint.m_vPoint, tInfo, 0, tPoint.m_Solution, false);
				if (tInfo.m_flGravity)
				{
					Vec3 vPos = tInfo.m_vLocalEye + Vec3(0, 0, (tInfo.m_flGravity * 800.f * pow(tPoint.m_Solution.m_flTime, 2)) / 2);
					Vec3 vForward = (vPos - tPoint.m_vPoint).Normalized();
					bNormal = vForward.Dot(trace.plane.normal) <= 0;
				}
			}
			if (bNormal)
				return false;

			if (tPoint.m_Solution.m_iCalculated != CalculatedEnum::Bad)
			{
				vSimplePoints.emplace_back(tPoint.m_vPoint, trace.plane.normal);
				return true;
			}
			return false;
		};

	int i = 0;
	for (auto& vSpherePoint : vSpherePoints)
	{
		Vec3 vPoint = vSpherePoint.first + vTargetEye;
		int& iType = vSpherePoint.second;

		Solution_t solution; CalculateAngle(tInfo.m_vLocalEye, vPoint, tInfo, 0, solution, false);

		if (solution.m_iCalculated != CalculatedEnum::Bad)
			TracePoint(vPoint, iType, vTargetEye, tInfo, vSimplePoints, checkPoint, i++);
	}
}

std::vector<Point_t> CAimbotProjectile::GetSplashPointsSimple(Target_t& tTarget, std::vector<std::pair<Vec3, Vec3>>& vSpherePoints, Info_t& tInfo, int iSimTime)
{
	std::vector<std::pair<Point_t, float>> vPointDistances = {};

	Vec3 vTargetEye = tTarget.m_vPos + tInfo.m_vTargetEye;

	auto checkPoint = [&](Vec3& vPoint, bool& bErase)
		{
			Point_t tPoint = { vPoint, {} };
			CalculateAngle(tInfo.m_vLocalEye, tPoint.m_vPoint, tInfo, iSimTime, tPoint.m_Solution);

			bErase = tPoint.m_Solution.m_iCalculated == CalculatedEnum::Good;
			if (!bErase || !tInfo.m_flPrimeTime && int(tPoint.m_Solution.m_flTime / TICK_INTERVAL) + 1 != iSimTime)
				return false;

			vPointDistances.emplace_back(tPoint, tPoint.m_vPoint.DistTo(tTarget.m_vPos));
			return true;
		};
	for (auto it = vSpherePoints.begin(); it != vSpherePoints.end();)
	{
		Vec3& vPoint = it->first;
		Vec3& vNormal = it->second;
		bool bErase = false;

		if (checkPoint(vPoint, bErase))
		{
#ifdef SPLASH_DEBUG3
			{
				Vec3 vMins = Vec3(-1, -1, -1) * (bErase ? 0.5f : 1.f), vMaxs = Vec3(1, 1, 1) * (bErase ? 0.5f : 1.f);
				Vec3 vAngles = Math::VectorAngles(vNormal);
				G::BoxStorage.emplace_back(vPoint, vMins, vMaxs, vAngles, I::GlobalVars->curtime + 60.f, Vars::Colors::Halloween.Value, Color_t(0, 0, 0, 0));
				G::LineStorage.emplace_back(std::pair<Vec3, Vec3>(tInfo.m_vLocalEye, vPoint), I::GlobalVars->curtime + 60.f, Vars::Colors::Halloween.Value);
			}
#endif

			/*
			CGameTrace trace = {};
			CTraceFilterWorldAndPropsOnly filter = {};

			SDK::Trace(vPoint + vNormal, vTargetEye, MASK_SHOT, &filter, &trace);
#ifdef SPLASH_DEBUG6
			mTraceCount["Splash trace check"]++;
#endif
#ifdef SPLASH_DEBUG3
			{
				Vec3 vMins = Vec3(-1, -1, -1) * (bErase ? 0.5f : 1.f), vMaxs = Vec3(1, 1, 1) * (bErase ? 0.5f : 1.f);
				Vec3 vAngles = Math::VectorAngles(trace.plane.normal);
				G::BoxStorage.emplace_back(trace.endpos, vMins, vMaxs, vAngles, I::GlobalVars->curtime + 60.f, Vars::Colors::Powerup.Value.Alpha(Vars::Colors::Powerup.Value.a / (bErase ? 10 : 1)), Color_t(0, 0, 0, 0));
				G::LineStorage.emplace_back(std::pair<Vec3, Vec3>(trace.startpos, trace.endpos), I::GlobalVars->curtime + 60.f, Vars::Colors::Powerup.Value.Alpha(Vars::Colors::Powerup.Value.a / (bErase ? 10 : 1)));
			}
#endif
			if (trace.fraction < 1.f)
				vPointDistances.pop_back();
			*/
		}

		if (bErase)
			it = vSpherePoints.erase(it);
		else
			++it;
	}

	std::sort(vPointDistances.begin(), vPointDistances.end(), [&](const auto& a, const auto& b) -> bool
		{
			return a.second < b.second;
		});

	std::vector<Point_t> vPoints = {};
	int iSplashCount = std::min(
		tInfo.m_flPrimeTime && iSimTime == tInfo.m_iPrimeTime ? Vars::Aimbot::Projectile::SplashCountDirect.Value : tInfo.m_iSplashCount,
		int(vPointDistances.size())
	);
	for (int i = 0; i < iSplashCount; i++)
		vPoints.push_back(vPointDistances[i].first);

	const Vec3 vOriginal = tTarget.m_pEntity->GetAbsOrigin();
	tTarget.m_pEntity->SetAbsOrigin(tTarget.m_vPos);
	for (auto it = vPoints.begin(); it != vPoints.end();)
	{
		auto& vPoint = *it;
		bool bValid = vPoint.m_Solution.m_iCalculated != CalculatedEnum::Pending;
		if (bValid)
		{
			Vec3 vPos = {}; tTarget.m_pEntity->m_Collision()->CalcNearestPoint(vPoint.m_vPoint, &vPos);
			bValid = vPoint.m_vPoint.DistTo(vPos) < tInfo.m_flRadius;
		}

		if (bValid)
			++it;
		else
			it = vPoints.erase(it);
	}
	tTarget.m_pEntity->SetAbsOrigin(vOriginal);

	return vPoints;
}

static inline float AABBLine(Vec3 vMins, Vec3 vMaxs, Vec3 vStart, Vec3 vDir)
{
	Vec3 a = {
		(vMins.x - vStart.x) / vDir.x,
		(vMins.y - vStart.y) / vDir.y,
		(vMins.z - vStart.z) / vDir.z
	};
	Vec3 b = {
		(vMaxs.x - vStart.x) / vDir.x,
		(vMaxs.y - vStart.y) / vDir.y,
		(vMaxs.z - vStart.z) / vDir.z
	};
	Vec3 c = {
		std::min(a.x, b.x),
		std::min(a.y, b.y),
		std::min(a.z, b.z)
	};
	return std::max(std::max(c.x, c.y), c.z);
}
static inline Vec3 PullPoint(Vec3 vPoint, Vec3 vLocalPos, Info_t& tInfo, Vec3 vMins, Vec3 vMaxs, Vec3 vTargetPos)
{
	auto HeightenLocalPos = [&]()
		{	// basic trajectory pass
			const float flGrav = tInfo.m_flGravity * 800.f;
			if (!flGrav)
				return vPoint;

			const Vec3 vDelta = vTargetPos - vLocalPos;
			const float flDist = vDelta.Length2D();

			const float flRoot = pow(tInfo.m_flVelocity, 4) - flGrav * (flGrav * pow(flDist, 2) + 2.f * vDelta.z * pow(tInfo.m_flVelocity, 2));
			if (flRoot < 0.f)
				return vPoint;
			float flPitch = atan((pow(tInfo.m_flVelocity, 2) - sqrt(flRoot)) / (flGrav * flDist));

			float flTime = flDist / (cos(flPitch) * tInfo.m_flVelocity) - tInfo.m_flOffsetTime;
			return vLocalPos + Vec3(0, 0, (flGrav * pow(flTime, 2)) / 2);
		};

	vLocalPos = HeightenLocalPos();
	Vec3 vForward, vRight, vUp; Math::AngleVectors(Math::CalcAngle(vLocalPos, vPoint), &vForward, &vRight, &vUp);
	vLocalPos += (vForward * tInfo.m_vOffset.x) + (vRight * tInfo.m_vOffset.y) + (vUp * tInfo.m_vOffset.z);
	return vLocalPos + (vPoint - vLocalPos) * fabsf(AABBLine(vMins + vTargetPos, vMaxs + vTargetPos, vLocalPos, vPoint - vLocalPos));
}



static inline void SolveProjectileSpeed(CTFWeaponBase* pWeapon, const Vec3& vLocalPos, const Vec3& vTargetPos, float& flVelocity, float& flDragTime, const float flGravity)
{
	if (!F::ProjSim.m_pObj->IsDragEnabled() || F::ProjSim.m_pObj->m_dragBasis.IsZero())
		return;

	const float flGrav = flGravity * 800.0f;
	const Vec3 vDelta = vTargetPos - vLocalPos;
	const float flDist = vDelta.Length2D();

	const float flRoot = pow(flVelocity, 4) - flGrav * (flGrav * pow(flDist, 2) + 2.f * vDelta.z * pow(flVelocity, 2));
	if (flRoot < 0.f)
		return;

	float flPitchLow = atan((pow(flVelocity, 2) - sqrt(flRoot)) / (flGrav * flDist));
	float flPitchHigh = atan((pow(flVelocity, 2) + sqrt(flRoot)) / (flGrav * flDist));

	const float flPitch = flPitchLow; // Use low arc for drag calculation
	const float flTime = flDist / (cos(flPitch) * flVelocity);

	float flDrag = 0.f;
	if (Vars::Aimbot::Projectile::DragOverride.Value)
		flDrag = Vars::Aimbot::Projectile::DragOverride.Value;
	else
	{
		switch (pWeapon->m_iItemDefinitionIndex()) // the remaps are dumb but they work so /shrug
		{
		case Demoman_m_GrenadeLauncher:
		case Demoman_m_GrenadeLauncherR:
		case Demoman_m_FestiveGrenadeLauncher:
		case Demoman_m_Autumn:
		case Demoman_m_MacabreWeb:
		case Demoman_m_Rainbow:
		case Demoman_m_SweetDreams:
		case Demoman_m_CoffinNail:
		case Demoman_m_TopShelf:
		case Demoman_m_Warhawk:
		case Demoman_m_ButcherBird:
		case Demoman_m_TheIronBomber: flDrag = Math::RemapVal(flVelocity, 1217.f, k_flMaxVelocity, 0.120f, 0.200f); break; // 0.120 normal, 0.200 capped, 0.300 v3000
		case Demoman_m_TheLochnLoad: flDrag = Math::RemapVal(flVelocity, 1504.f, k_flMaxVelocity, 0.070f, 0.085f); break; // 0.070 normal, 0.085 capped, 0.120 v3000
		case Demoman_m_TheLooseCannon: flDrag = Math::RemapVal(flVelocity, 1454.f, k_flMaxVelocity, 0.385f, 0.530f); break; // 0.385 normal, 0.530 capped, 0.790 v3000
		case Demoman_s_StickybombLauncher:
		case Demoman_s_StickybombLauncherR:
		case Demoman_s_FestiveStickybombLauncher:
		case Demoman_s_TheQuickiebombLauncher:
		case Demoman_s_TheScottishResistance: flDrag = Math::RemapVal(flVelocity, 922.f, k_flMaxVelocity, 0.085f, 0.190f); break; // 0.085 low, 0.190 capped, 0.230 v2400
		case Scout_s_TheFlyingGuillotine:
		case Scout_s_TheFlyingGuillotineG: flDrag = 0.310f; break;
		case Scout_t_TheSandman: flDrag = 0.180f; break;
		case Scout_t_TheWrapAssassin: flDrag = 0.285f; break;
		case Scout_s_MadMilk:
		case Scout_s_MutatedMilk:
		case Sniper_s_Jarate:
		case Sniper_s_FestiveJarate:
		case Sniper_s_TheSelfAwareBeautyMark: flDrag = 0.057f; break;
		}
	}

	float flOverride = Vars::Aimbot::Projectile::TimeOverride.Value;
	flDragTime = powf(flTime, 2) * flDrag / (flOverride ? flOverride : 1.5f); // rough estimate to prevent m_flTime being too low
	flVelocity = flVelocity - flVelocity * flTime * flDrag;
}
void CAimbotProjectile::CalculateAngle(const Vec3& vLocalPos, const Vec3& vTargetPos, Info_t& tInfo, int iSimTime, Solution_t& out, bool bAccuracy, int iPitchPreference)
{
	if (out.m_iCalculated != CalculatedEnum::Pending)
		return;

	const float flGrav = tInfo.m_flGravity * 800.f;

	float flPitch, flYaw;
	{	// basic trajectory pass
		float flVelocity = tInfo.m_flVelocity, flDragTime = 0.f;
		if (F::ProjSim.m_pObj->IsDragEnabled() && !F::ProjSim.m_pObj->m_dragBasis.IsZero() && tInfo.m_pWeapon)
		{
			Vec3 vForward, vRight, vUp; Math::AngleVectors(Math::CalcAngle(vLocalPos, vTargetPos), &vForward, &vRight, &vUp);
			Vec3 vShootPos = vLocalPos + (vForward * tInfo.m_vOffset.x) + (vRight * tInfo.m_vOffset.y) + (vUp * tInfo.m_vOffset.z);
			SolveProjectileSpeed(tInfo.m_pWeapon, vShootPos, vTargetPos, flVelocity, flDragTime, tInfo.m_flGravity);
		}

		Vec3 vDelta = vTargetPos - vLocalPos;
		float flDist = vDelta.Length2D();

		Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vTargetPos);
		if (!flGrav)
			flPitch = -DEG2RAD(vAngleTo.x);
		else
		{	// arch
			float flRoot = pow(flVelocity, 4) - flGrav * (flGrav * pow(flDist, 2) + 2.f * vDelta.z * pow(flVelocity, 2));
			if (out.m_iCalculated = flRoot < 0.f ? CalculatedEnum::Bad : CalculatedEnum::Pending)
				return;
			float flPitchLow2 = atan((pow(flVelocity, 2) - sqrt(flRoot)) / (flGrav * flDist));
			float flPitchHigh2 = atan((pow(flVelocity, 2) + sqrt(flRoot)) / (flGrav * flDist));

			if (iPitchPreference == 1)
				flPitch = flPitchLow2;
			else if (iPitchPreference == 2)
				flPitch = flPitchHigh2;
			else
			{
				bool bObstructed2 = !SDK::VisPosWorld(nullptr, nullptr, vLocalPos, vTargetPos, MASK_SHOT);
				flPitch = bObstructed2 ? flPitchHigh2 : flPitchLow2;
			}
		}
		out.m_flTime = flDist / (cos(flPitch) * flVelocity) - tInfo.m_flOffsetTime + flDragTime;
		out.m_flPitch = flPitch = -RAD2DEG(flPitch) - tInfo.m_vAngFix.x;
		out.m_flYaw = flYaw = vAngleTo.y - tInfo.m_vAngFix.y;
	}

	int iTimeTo = int(out.m_flTime / TICK_INTERVAL) + 1;
	if (!tInfo.m_vOffset.IsZero())
	{
		if (out.m_iCalculated = iTimeTo > iSimTime ? CalculatedEnum::Time : CalculatedEnum::Pending)
			return;
	}
	else
	{
		out.m_iCalculated = iTimeTo > iSimTime ? CalculatedEnum::Time : CalculatedEnum::Good;
		return;
	}

	int iFlags = (bAccuracy ? ProjSimEnum::Redirect : ProjSimEnum::None) | ProjSimEnum::NoRandomAngles | ProjSimEnum::PredictCmdNum;
#ifdef SPLASH_DEBUG6
	if (iFlags & ProjSimEnum::Redirect)
	{
		if (Vars::Visuals::Trajectory::Override.Value)
		{
			if (Vars::Visuals::Trajectory::ForwardRedirect.Value)
				mTraceCount["Setup trace calculate"]++;
		}
		else
		{
			switch (tInfo.m_pWeapon->GetWeaponID())
			{
			case TF_WEAPON_ROCKETLAUNCHER:
			case TF_WEAPON_ROCKETLAUNCHER_DIRECTHIT:
			case TF_WEAPON_PARTICLE_CANNON:
			case TF_WEAPON_RAYGUN:
			case TF_WEAPON_DRG_POMSON:
			case TF_WEAPON_FLAREGUN:
			case TF_WEAPON_FLAREGUN_REVENGE:
			case TF_WEAPON_COMPOUND_BOW:
			case TF_WEAPON_CROSSBOW:
			case TF_WEAPON_SHOTGUN_BUILDING_RESCUE:
			case TF_WEAPON_SYRINGEGUN_MEDIC:
			case TF_WEAPON_FLAME_BALL:
				mTraceCount["Setup trace calculate"]++;
			}
		}
	}
#endif
	ProjectileInfo tProjInfo = {};
	if (out.m_iCalculated = !F::ProjSim.GetInfo(tInfo.m_pLocal, tInfo.m_pWeapon, { flPitch, flYaw, 0 }, tProjInfo, iFlags) ? CalculatedEnum::Bad : CalculatedEnum::Pending)
		return;

	{	// calculate trajectory from projectile origin
		float flVelocity = tInfo.m_flVelocity, flDragTime = 0.f;
		SolveProjectileSpeed(tInfo.m_pWeapon, tProjInfo.m_vPos, vTargetPos, flVelocity, flDragTime, tInfo.m_flGravity);

		Vec3 vDelta = vTargetPos - tProjInfo.m_vPos;
		float flDist = vDelta.Length2D();

		Vec3 vAngleTo = Math::CalcAngle(tProjInfo.m_vPos, vTargetPos);
		if (!flGrav)
			out.m_flPitch = -DEG2RAD(vAngleTo.x);
		else
		{	// arch
			float flRoot = pow(flVelocity, 4) - flGrav * (flGrav * pow(flDist, 2) + 2.f * vDelta.z * pow(flVelocity, 2));
			if (out.m_iCalculated = flRoot < 0.f ? CalculatedEnum::Bad : CalculatedEnum::Pending)
				return;
			out.m_flPitch = atan((pow(flVelocity, 2) - sqrt(flRoot)) / (flGrav * flDist));
		}
		out.m_flTime = flDist / (cos(out.m_flPitch) * flVelocity) + flDragTime;
	}

	{	// correct yaw
		Vec3 vShootPos = (tProjInfo.m_vPos - vLocalPos).To2D();
		Vec3 vTarget = vTargetPos - vLocalPos;
		Vec3 vForward; Math::AngleVectors(tProjInfo.m_vAng, &vForward); vForward.Normalize2D();
		float flB = 2 * (vShootPos.x * vForward.x + vShootPos.y * vForward.y);
		float flC = vShootPos.Length2DSqr() - vTarget.Length2DSqr();
		auto vSolutions = Math::SolveQuadratic(1.f, flB, flC);
		if (!vSolutions.empty())
		{
			vShootPos += vForward * vSolutions.front();
			out.m_flYaw = flYaw - (RAD2DEG(atan2(vShootPos.y, vShootPos.x)) - flYaw);
			flYaw = RAD2DEG(atan2(vShootPos.y, vShootPos.x));
		}
	}

	{	// correct pitch
		if (flGrav)
		{
			flPitch -= tProjInfo.m_vAng.x;
			out.m_flPitch = -RAD2DEG(out.m_flPitch) + flPitch - tInfo.m_vAngFix.x;
		}
		else
		{
			Vec3 vShootPos = Math::RotatePoint(tProjInfo.m_vPos - vLocalPos, {}, { 0, -flYaw, 0 }); vShootPos.y = 0;
			Vec3 vTarget = Math::RotatePoint(vTargetPos - vLocalPos, {}, { 0, -flYaw, 0 });
			Vec3 vForward; Math::AngleVectors(tProjInfo.m_vAng - Vec3(0, flYaw, 0), &vForward); vForward.y = 0; vForward.Normalize();
			float flB = 2 * (vShootPos.x * vForward.x + vShootPos.z * vForward.z);
			float flC = (powf(vShootPos.x, 2) + powf(vShootPos.z, 2)) - (powf(vTarget.x, 2) + powf(vTarget.z, 2));
			auto vSolutions = Math::SolveQuadratic(1.f, flB, flC);
			if (!vSolutions.empty())
			{
				vShootPos += vForward * vSolutions.front();
				out.m_flPitch = flPitch - (RAD2DEG(atan2(-vShootPos.z, vShootPos.x)) - flPitch);
			}
		}
	}

	iTimeTo = int(out.m_flTime / TICK_INTERVAL) + 1;
	out.m_iCalculated = iTimeTo > iSimTime ? CalculatedEnum::Time : CalculatedEnum::Good;
}



bool CAimbotProjectile::TestAngle(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, Target_t& tTarget, Vec3& vPoint, Vec3& vAngles, int iSimTime, bool bSplash, bool* pHitSolid, std::vector<Vec3>* pProjectilePath)
{
	int iFlags = ProjSimEnum::Redirect | ProjSimEnum::InitCheck | ProjSimEnum::NoRandomAngles | ProjSimEnum::PredictCmdNum;
#ifdef SPLASH_DEBUG6
	if (Vars::Visuals::Trajectory::Override.Value)
	{
		if (Vars::Visuals::Trajectory::ForwardRedirect.Value)
			mTraceCount["Setup trace test"]++;
	}
	else
	{
		switch (pWeapon->GetWeaponID())
		{
		case TF_WEAPON_ROCKETLAUNCHER:
		case TF_WEAPON_ROCKETLAUNCHER_DIRECTHIT:
		case TF_WEAPON_PARTICLE_CANNON:
		case TF_WEAPON_RAYGUN:
		case TF_WEAPON_DRG_POMSON:
		case TF_WEAPON_FLAREGUN:
		case TF_WEAPON_FLAREGUN_REVENGE:
		case TF_WEAPON_COMPOUND_BOW:
		case TF_WEAPON_CROSSBOW:
		case TF_WEAPON_SHOTGUN_BUILDING_RESCUE:
		case TF_WEAPON_SYRINGEGUN_MEDIC:
		case TF_WEAPON_FLAME_BALL:
			mTraceCount["Setup trace test"]++;
		}
	}
	mTraceCount["Trace init check test"]++;
#endif
	ProjectileInfo tProjInfo = {};
	if (!F::ProjSim.GetInfo(pLocal, pWeapon, vAngles, tProjInfo, iFlags)
		|| !F::ProjSim.Initialize(tProjInfo))
		return false;


	CGameTrace trace = {};
	CTraceFilterCollideable filter = {}; filter.pSkip = bSplash ? tTarget.m_pEntity : pLocal; filter.iPlayer = bSplash ? PLAYER_NONE : PLAYER_DEFAULT;
	int nMask = MASK_SOLID;
	F::ProjSim.SetupTrace(filter, nMask, pWeapon);

#ifdef SPLASH_DEBUG5
	G::BoxStorage.emplace_back(vPoint, tProjInfo.m_vHull * -1, tProjInfo.m_vHull, Vec3(), I::GlobalVars->curtime + 5.f, Color_t(255, 0, 0), Color_t(0, 0, 0, 0));
#endif

	if (!tProjInfo.m_flGravity)
	{
		SDK::TraceHull(tProjInfo.m_vPos, vPoint, tProjInfo.m_vHull * -1, tProjInfo.m_vHull, nMask, &filter, &trace);
#ifdef SPLASH_DEBUG6
		mTraceCount["Nograv trace"]++;
#endif
#ifdef SPLASH_DEBUG5
		G::LineStorage.emplace_back(std::pair<Vec3, Vec3>(tProjInfo.m_vPos, vPoint), I::GlobalVars->curtime + 5.f, Color_t(0, 0, 0));
#endif
		if (trace.fraction < 0.999f && trace.m_pEnt != tTarget.m_pEntity)
			return false;
	}

	bool bDidHit = false, bPrimeTime = false;
	const RestoreInfo_t tOriginal = { tTarget.m_pEntity->GetAbsOrigin(), tTarget.m_pEntity->m_vecMins(), tTarget.m_pEntity->m_vecMaxs() };
	tTarget.m_pEntity->SetAbsOrigin(tTarget.m_vPos);
	tTarget.m_pEntity->m_vecMins() = { std::clamp(tTarget.m_pEntity->m_vecMins().x, -24.f, 0.f), std::clamp(tTarget.m_pEntity->m_vecMins().y, -24.f, 0.f), tTarget.m_pEntity->m_vecMins().z };
	tTarget.m_pEntity->m_vecMaxs() = { std::clamp(tTarget.m_pEntity->m_vecMaxs().x, 0.f, 24.f), std::clamp(tTarget.m_pEntity->m_vecMaxs().y, 0.f, 24.f), tTarget.m_pEntity->m_vecMaxs().z };
	for (int n = 1; n <= iSimTime; n++)
	{
		Vec3 vOld = F::ProjSim.GetOrigin();
		F::ProjSim.RunTick(tProjInfo);
		Vec3 vNew = F::ProjSim.GetOrigin();

		if (bDidHit)
		{
			trace.endpos = vNew;
			continue;
		}

		if (!bSplash)
		{
			SDK::TraceHull(vOld, vNew, tProjInfo.m_vHull * -1, tProjInfo.m_vHull, nMask, &filter, &trace);
#ifdef SPLASH_DEBUG6
			mTraceCount["Direct trace"]++;
#endif

#ifdef SPLASH_DEBUG5
			G::LineStorage.emplace_back(std::pair<Vec3, Vec3>(vOld, vNew), I::GlobalVars->curtime + 5.f, Color_t(255, 0, 0));
#endif
		}
		else
		{
			static Vec3 vStaticPos = {};
			if (n == 1 || bPrimeTime)
				vStaticPos = vOld;
			if (n % Vars::Aimbot::Projectile::SplashTraceInterval.Value && n != iSimTime && !bPrimeTime)
				continue;

			SDK::TraceHull(vStaticPos, vNew, tProjInfo.m_vHull * -1, tProjInfo.m_vHull, nMask, &filter, &trace);
#ifdef SPLASH_DEBUG6
			mTraceCount["Splash trace"]++;
#endif
#ifdef SPLASH_DEBUG5
			G::LineStorage.emplace_back(std::pair<Vec3, Vec3>(vStaticPos, vNew), I::GlobalVars->curtime + 5.f, Color_t(255, 0, 0));
#endif
			vStaticPos = vNew;
		}
		if (trace.DidHit())
		{
			if (pHitSolid)
				*pHitSolid = true;

			bool bTime = bSplash
				? trace.endpos.DistTo(vPoint) < tProjInfo.m_flVelocity * TICK_INTERVAL + tProjInfo.m_vHull.z
				: iSimTime - n < 5 || pWeapon->GetWeaponID() == TF_WEAPON_LUNCHBOX; // projectile so slow it causes problems if we don't waive this check
			bool bTarget = trace.m_pEnt == tTarget.m_pEntity || bSplash;
			bool bValid = bTarget && bTime;
			if (bValid && bSplash)
			{
				bValid = SDK::VisPosWorld(nullptr, tTarget.m_pEntity, trace.endpos, vPoint, nMask);
#ifdef SPLASH_DEBUG6
				mTraceCount["Splash vispos"]++;
#endif
				if (bValid)
				{
					Vec3 vFrom = trace.endpos;
					switch (pWeapon->GetWeaponID())
					{
					case TF_WEAPON_ROCKETLAUNCHER:
					case TF_WEAPON_ROCKETLAUNCHER_DIRECTHIT:
					case TF_WEAPON_PARTICLE_CANNON:
						vFrom += trace.plane.normal;
					}

					CGameTrace eyeTrace = {};
					SDK::Trace(vFrom, tTarget.m_vPos + tTarget.m_pEntity->As<CTFPlayer>()->GetViewOffset(), MASK_SHOT, &filter, &eyeTrace);
					bValid = eyeTrace.fraction == 1.f;
#ifdef SPLASH_DEBUG6
					mTraceCount["Eye trace"]++;
#endif
				}
			}

#ifdef SPLASH_DEBUG5
			G::BoxStorage.pop_back();
			if (bValid)
				G::BoxStorage.emplace_back(vPoint, tProjInfo.m_vHull * -1, tProjInfo.m_vHull, Vec3(), I::GlobalVars->curtime + 5.f, Color_t(0, 255, 0), Color_t(0, 0, 0, 0));
			else if (!bTime)
			{
				G::BoxStorage.emplace_back(vPoint, tProjInfo.m_vHull * -1, tProjInfo.m_vHull, Vec3(), I::GlobalVars->curtime + 5.f, Color_t(255, 0, 255), Color_t(0, 0, 0, 0));
				if (bSplash)
				{
					G::BoxStorage.emplace_back(trace.endpos, Vec3(-1, -1, -1), Vec3(1, 1, 1), Vec3(), I::GlobalVars->curtime + 5.f, Color_t(0, 0, 0), Color_t(0, 0, 0, 0));
					G::BoxStorage.emplace_back(vPoint, Vec3(-1, -1, -1), Vec3(1, 1, 1), Vec3(), I::GlobalVars->curtime + 5.f, Color_t(255, 255, 255), Color_t(0, 0, 0, 0));
				}
			}
			else
				G::BoxStorage.emplace_back(vPoint, tProjInfo.m_vHull * -1, tProjInfo.m_vHull, Vec3(), I::GlobalVars->curtime + 5.f, Color_t(0, 0, 255), Color_t(0, 0, 0, 0));
#endif

			if (bValid)
			{
				if (bSplash)
				{
					int iPopCount = Vars::Aimbot::Projectile::SplashTraceInterval.Value - trace.fraction * Vars::Aimbot::Projectile::SplashTraceInterval.Value;
					for (int i = 0; i < iPopCount && !tProjInfo.m_vPath.empty(); i++)
						tProjInfo.m_vPath.pop_back();
				}

				// attempted to have a headshot check though this seems more detrimental than useful outside of smooth aimbot
				if (tTarget.m_nAimedHitbox == HITBOX_HEAD && pProjectilePath &&
					Vars::Aimbot::General::AimType.Value == Vars::Aimbot::General::AimTypeEnum::Smooth)
				{	// loop and see if closest hitbox is head
					auto pModel = tTarget.m_pEntity->GetModel();
					if (!pModel) break;
					auto pHDR = I::ModelInfoClient->GetStudiomodel(pModel);
					if (!pHDR) break;
					auto pSet = pHDR->pHitboxSet(tTarget.m_pEntity->As<CTFPlayer>()->m_nHitboxSet());
					if (!pSet) break;

					auto aBones = H::Entities.GetBones(tTarget.m_pEntity->entindex());
					if (!aBones)
						break;

					Vec3 vOffset = tOriginal.m_vOrigin - tTarget.m_vPos;
					Vec3 vPos = trace.endpos + F::ProjSim.GetVelocity().Normalized() * 16 + vOffset;

					float flClosest = 0.f; int iClosest = -1;
					for (int i = 0; i < pSet->numhitboxes; ++i)
					{
						auto pBox = pSet->pHitbox(i);
						if (!pBox)
							continue;

						Vec3 vCenter; Math::VectorTransform({}, aBones[pBox->bone], vCenter);

						const float flDist = vPos.DistTo(vCenter);
						if (iClosest != -1 && flDist < flClosest || iClosest == -1)
						{
							flClosest = flDist;
							iClosest = i;
						}
					}
					if (iClosest != HITBOX_HEAD)
						break;
				}

				bDidHit = true;
			}
			else if (!bSplash && bTarget && pWeapon->GetWeaponID() == TF_WEAPON_PIPEBOMBLAUNCHER)
			{	// run for more ticks to check for splash
				iSimTime = n + 5;
				bSplash = bPrimeTime = true;
			}
			else
				break;

			if (!bSplash)
				trace.endpos = vNew;

			if (!bTarget || bSplash && !bPrimeTime)
				break;
		}
	}
	tTarget.m_pEntity->SetAbsOrigin(tOriginal.m_vOrigin);
	tTarget.m_pEntity->m_vecMins() = tOriginal.m_vMins;
	tTarget.m_pEntity->m_vecMaxs() = tOriginal.m_vMaxs;

	if (bDidHit && pProjectilePath)
	{
		tProjInfo.m_vPath.push_back(trace.endpos);
		*pProjectilePath = tProjInfo.m_vPath;
	}

	return bDidHit;
}

int CAimbotProjectile::CanHit(Target_t& tTarget, CTFPlayer* pLocal, CTFWeaponBase* pWeapon)
{
	//if (Vars::Aimbot::General::Ignore.Value & Vars::Aimbot::General::IgnoreEnum::Unsimulated && H::Entities.GetChoke(tTarget.m_pEntity->entindex()) > Vars::Aimbot::General::TickTolerance.Value)
	//	return false;

	ProjectileInfo tProjInfo = {};
	if (!F::ProjSim.GetInfo(pLocal, pWeapon, {}, tProjInfo, ProjSimEnum::NoRandomAngles | ProjSimEnum::PredictCmdNum)
		|| !F::ProjSim.Initialize(tProjInfo, false))
		return false;

	MoveStorage tStorage;
	F::MoveSim.Initialize(tTarget.m_pEntity, tStorage);
	tTarget.m_vPos = tTarget.m_pEntity->m_vecOrigin();

	Info_t tInfo = { pLocal, pWeapon };
	tInfo.m_vLocalEye = pLocal->GetShootPos();
	tInfo.m_vTargetEye = tTarget.m_pEntity->As<CTFPlayer>()->GetViewOffset();
	tInfo.m_flLatency = F::Backtrack.GetReal() + TICKS_TO_TIME(F::Backtrack.GetAnticipatedChoke());

	Vec3 vVelocity = F::ProjSim.GetVelocity();
	tInfo.m_flVelocity = vVelocity.Length();
	tInfo.m_vAngFix = Math::VectorAngles(vVelocity);

	tInfo.m_vHull = tProjInfo.m_vHull.Min(3);
	tInfo.m_vOffset = tProjInfo.m_vPos - tInfo.m_vLocalEye; tInfo.m_vOffset.y *= -1;
	tInfo.m_flOffsetTime = tInfo.m_vOffset.Length() / tInfo.m_flVelocity; // silly

	float flSize = tTarget.m_pEntity->GetSize().Length();
	tInfo.m_flGravity = tProjInfo.m_flGravity;
	tInfo.m_iSplashCount = !tInfo.m_flGravity ? Vars::Aimbot::Projectile::SplashCountDirect.Value : Vars::Aimbot::Projectile::SplashCountArc.Value;
	tInfo.m_flRadius = GetSplashRadius(pWeapon, pLocal);
	tInfo.m_flRadiusTime = tInfo.m_flRadius / tInfo.m_flVelocity;
	tInfo.m_flBoundingTime = tInfo.m_flRadiusTime + flSize / tInfo.m_flVelocity;

	tInfo.m_iSplashMode = GetSplashMode(pWeapon);
	tInfo.m_flPrimeTime = PrimeTime(pWeapon);
	tInfo.m_iPrimeTime = TIME_TO_TICKS(tInfo.m_flPrimeTime);



	int iReturn = false;
	int iMaxTime = TIME_TO_TICKS(std::min(tProjInfo.m_flLifetime, Vars::Aimbot::Projectile::MaxSimulationTime.Value));
	int iSplash = Vars::Aimbot::Projectile::SplashPrediction.Value && tInfo.m_flRadius ? Vars::Aimbot::Projectile::SplashPrediction.Value : Vars::Aimbot::Projectile::SplashPredictionEnum::Off;
	int iMulti = Vars::Aimbot::Projectile::SplashMode.Value;
	int iPoints = !tInfo.m_flGravity ? Vars::Aimbot::Projectile::SplashPointsDirect.Value : Vars::Aimbot::Projectile::SplashPointsArc.Value;

	auto mDirectPoints = iSplash == Vars::Aimbot::Projectile::SplashPredictionEnum::Only ? std::unordered_map<int, Vec3>() : GetDirectPoints(tTarget, tInfo);
	auto vSpherePoints = !iSplash ? std::vector<std::pair<Vec3, int>>() : ComputeSphere(tInfo.m_flRadius + flSize, iPoints, Vars::Aimbot::Projectile::SplashNthRoot.Value);
#ifdef SPLASH_DEBUG4
	for (auto& [vPoint, _] : vSpherePoints)
		G::BoxStorage.emplace_back(tTarget.m_pEntity->m_vecOrigin() + tInfo.m_vTargetEye + vPoint * tInfo.m_flRadius / (tInfo.m_flRadius + flSize), Vec3(-1, -1, -1), Vec3(1, 1, 1), Vec3(), I::GlobalVars->curtime + 60.f, Color_t(0, 0, 0, 0), Vars::Colors::Local.Value);
#endif
	
	Vec3 vAngleTo, vPredicted, vTarget;
	int iLowestPriority = std::numeric_limits<int>::max(); float flLowestDist = std::numeric_limits<float>::max();
	int iLowestSmoothPriority = iLowestPriority; float flLowestSmoothDist = flLowestDist;
	for (int i = 1 - TIME_TO_TICKS(tInfo.m_flLatency); i <= iMaxTime; i++)
	{
		if (!tStorage.m_bFailed)
		{
			F::MoveSim.RunTick(tStorage);
			tTarget.m_vPos = GetSimulatedPos(tStorage);
		}
		if (i < 0)
			continue;

		bool bDirectBreaks = true;
		std::vector<Point_t> vSplashPoints = {};
		if (iSplash)
		{
			Solution_t solution; CalculateAngle(tInfo.m_vLocalEye, tTarget.m_vPos, tInfo, i, solution, false);
			if (solution.m_iCalculated != CalculatedEnum::Bad)
			{
				bDirectBreaks = false;

				const float flTimeTo = solution.m_flTime - TICKS_TO_TIME(i);
				if (flTimeTo < tInfo.m_flBoundingTime)
				{
					static std::vector<std::pair<Vec3, Vec3>> vSimplePoints = {};
					if (iMulti == Vars::Aimbot::Projectile::SplashModeEnum::Single)
					{
						SetupSplashPoints(tTarget, vSpherePoints, tInfo, vSimplePoints);
						if (!vSimplePoints.empty())
							iMulti++;
						else
						{
							iSplash = Vars::Aimbot::Projectile::SplashPredictionEnum::Off;
							goto skipSplash;
						}
					}

					if ((iMulti == Vars::Aimbot::Projectile::SplashModeEnum::Multi ? vSpherePoints.empty() : vSimplePoints.empty())
						|| flTimeTo < -tInfo.m_flBoundingTime && (tInfo.m_flPrimeTime ? i > tInfo.m_iPrimeTime : true))
						break;
					else if (tInfo.m_flPrimeTime ? i >= tInfo.m_iPrimeTime : true)
					{
						if (iMulti == Vars::Aimbot::Projectile::SplashModeEnum::Multi)
							vSplashPoints = GetSplashPoints(tTarget, vSpherePoints, tInfo, i);
						else
							vSplashPoints = GetSplashPointsSimple(tTarget, vSimplePoints, tInfo, i);
					}
				}
			}
		}
		skipSplash:
		if (bDirectBreaks && mDirectPoints.empty())
			break;

		std::vector<std::tuple<Point_t, int, int>> vPoints = {};
		for (auto& [iIndex, vPoint] : mDirectPoints)
			vPoints.emplace_back(Point_t(tTarget.m_vPos + vPoint, {}), iIndex + (iSplash == Vars::Aimbot::Projectile::SplashPredictionEnum::Prefer ? tInfo.m_iSplashCount : 0), iIndex);
		for (auto& vPoint : vSplashPoints)
			vPoints.emplace_back(vPoint, iSplash == Vars::Aimbot::Projectile::SplashPredictionEnum::Include ? 3 : 0, -1);

		int j = 0;
		for (auto& [vPoint, iPriority, iIndex] : vPoints) // get most ideal point
		{
			const bool bSplash = iIndex == -1;
			Vec3 vOriginalPoint = vPoint.m_vPoint;

			if (Vars::Aimbot::Projectile::HuntsmanPullPoint.Value && tTarget.m_nAimedHitbox == HITBOX_HEAD)
				vPoint.m_vPoint = PullPoint(vPoint.m_vPoint, tInfo.m_vLocalEye, tInfo, tTarget.m_pEntity->m_vecMins() + tProjInfo.m_vHull, tTarget.m_pEntity->m_vecMaxs() - tProjInfo.m_vHull, tTarget.m_vPos);
				//vPoint.m_vPoint = PullPoint(vPoint.m_vPoint, tInfo.m_vLocalEye, tInfo, tTarget.m_pEntity->m_vecMins(), tTarget.m_pEntity->m_vecMaxs(), tTarget.m_vPos);

			float flDist = bSplash ? tTarget.m_vPos.DistTo(vPoint.m_vPoint) : flLowestDist;
			bool bPriority = bSplash ? iPriority <= iLowestPriority : iPriority < iLowestPriority;
			bool bTime = bSplash || tInfo.m_iPrimeTime < i || tStorage.m_MoveData.m_vecVelocity.IsZero();
			bool bDist = !bSplash || flDist < flLowestDist;
			if (!bSplash && !bPriority)
				mDirectPoints.erase(iIndex);
			if (!bPriority || !bTime || !bDist)
				continue;

			bool bCalculatedAny = false;
			bool bHitSolidAny = false;

			for (int iPitch = 1; iPitch <= 2; iPitch++)
			{
				vPoint.m_Solution.m_iCalculated = CalculatedEnum::Pending;
				CalculateAngle(tInfo.m_vLocalEye, vPoint.m_vPoint, tInfo, i, vPoint.m_Solution, true, iPitch);

				if (!bSplash && (vPoint.m_Solution.m_iCalculated == CalculatedEnum::Good || vPoint.m_Solution.m_iCalculated == CalculatedEnum::Bad))
					bCalculatedAny = true;
				if (vPoint.m_Solution.m_iCalculated != CalculatedEnum::Good)
					continue;

				if (Vars::Aimbot::Projectile::HuntsmanPullPoint.Value && tTarget.m_nAimedHitbox == HITBOX_HEAD)
				{
					Solution_t tSolution;
					CalculateAngle(tInfo.m_vLocalEye, vOriginalPoint, tInfo, std::numeric_limits<int>::max(), tSolution, true, iPitch);
					vPoint.m_Solution.m_flPitch = tSolution.m_flPitch, vPoint.m_Solution.m_flYaw = tSolution.m_flYaw;
				}

				Vec3 vAngles; Aim(G::CurrentUserCmd->viewangles, { vPoint.m_Solution.m_flPitch, vPoint.m_Solution.m_flYaw, 0.f }, vAngles);
				std::vector<Vec3> vProjLines; bool bHitSolid = false;

				if (TestAngle(pLocal, pWeapon, tTarget, vPoint.m_vPoint, vAngles, i, bSplash, &bHitSolid, &vProjLines))
				{
					iLowestPriority = iPriority; flLowestDist = flDist;
					vAngleTo = vAngles, vPredicted = tTarget.m_vPos, vTarget = vOriginalPoint;
					m_flTimeTo = vPoint.m_Solution.m_flTime + tInfo.m_flLatency;
					m_vPlayerPath = tStorage.m_vPath;
					m_vPlayerPath.push_back(tStorage.m_MoveData.m_vecAbsOrigin);
					m_vProjectilePath = vProjLines;
					if (bHitSolid) bHitSolidAny = true;
					break;
				}
				else switch (Vars::Aimbot::General::AimType.Value)
				{
				case Vars::Aimbot::General::AimTypeEnum::Smooth:
					if (Vars::Aimbot::General::SmoothStrength.Value == 100.f)
						break;
					[[fallthrough]];
				default:
				{
					bPriority = bSplash ? iPriority <= iLowestSmoothPriority : iPriority < flLowestSmoothDist;
					bDist = !bSplash || flDist < flLowestDist;
					if (!bPriority || !bDist)
						continue;

					Vec3 vPlainAngles; Aim({}, { vPoint.m_Solution.m_flPitch, vPoint.m_Solution.m_flYaw, 0.f }, vPlainAngles, Vars::Aimbot::General::AimTypeEnum::Plain);
					if (TestAngle(pLocal, pWeapon, tTarget, vPoint.m_vPoint, vPlainAngles, i, bSplash, &bHitSolid))
					{
						iLowestSmoothPriority = iPriority; flLowestSmoothDist = flDist;
						vAngleTo = vAngles, vPredicted = tTarget.m_vPos;
						m_vPlayerPath = tStorage.m_vPath;
						m_vPlayerPath.push_back(tStorage.m_MoveData.m_vecAbsOrigin);
						iReturn = 2;
					}
				}
				}
				if (bHitSolid) bHitSolidAny = true;
			}

			if (bCalculatedAny && !bSplash)
				mDirectPoints.erase(iIndex);

			if (!j && bHitSolidAny)
				m_flTimeTo = vPoint.m_Solution.m_flTime + tInfo.m_flLatency;
			j++;
		}
	}
	F::MoveSim.Restore(tStorage);

	tTarget.m_vPos = vTarget;
	tTarget.m_vAngleTo = vAngleTo;
	if (tTarget.m_iTargetType != TargetEnum::Player || !tStorage.m_bFailed) // don't attempt to aim at players when movesim fails
	{
		bool bMain = iLowestPriority != std::numeric_limits<int>::max();
		bool bAny = bMain || iLowestSmoothPriority != std::numeric_limits<int>::max();

		if (bAny && (Vars::Colors::BoundHitboxEdge.Value.a || Vars::Colors::BoundHitboxFace.Value.a || Vars::Colors::BoundHitboxEdgeIgnoreZ.Value.a || Vars::Colors::BoundHitboxFaceIgnoreZ.Value.a))
		{
			tInfo.m_vHull = tInfo.m_vHull.Max(1);
			float flProjectileTime = TICKS_TO_TIME(m_vProjectilePath.size());
			float flTargetTime = tStorage.m_bFailed ? flProjectileTime : TICKS_TO_TIME(m_vPlayerPath.size());

			bool bBox = Vars::Visuals::Hitbox::BoundsEnabled.Value & Vars::Visuals::Hitbox::BoundsEnabledEnum::OnShot;
			bool bPoint = Vars::Visuals::Hitbox::BoundsEnabled.Value & Vars::Visuals::Hitbox::BoundsEnabledEnum::AimPoint;
			if (bBox)
			{
				if (Vars::Colors::BoundHitboxEdgeIgnoreZ.Value.a || Vars::Colors::BoundHitboxFaceIgnoreZ.Value.a)
					m_vBoxes.emplace_back(vPredicted, tTarget.m_pEntity->m_vecMins(), tTarget.m_pEntity->m_vecMaxs(), Vec3(), I::GlobalVars->curtime + (Vars::Visuals::Simulation::Timed.Value ? flTargetTime : Vars::Visuals::Hitbox::DrawDuration.Value), Vars::Colors::BoundHitboxEdgeIgnoreZ.Value, Vars::Colors::BoundHitboxFaceIgnoreZ.Value);
				if (Vars::Colors::BoundHitboxEdge.Value.a || Vars::Colors::BoundHitboxFace.Value.a)
					m_vBoxes.emplace_back(vPredicted, tTarget.m_pEntity->m_vecMins(), tTarget.m_pEntity->m_vecMaxs(), Vec3(), I::GlobalVars->curtime + (Vars::Visuals::Simulation::Timed.Value ? flTargetTime : Vars::Visuals::Hitbox::DrawDuration.Value), Vars::Colors::BoundHitboxEdge.Value, Vars::Colors::BoundHitboxFace.Value, true);
			}
			if (bMain && bPoint)
			{
				if (Vars::Colors::BoundHitboxEdgeIgnoreZ.Value.a || Vars::Colors::BoundHitboxFaceIgnoreZ.Value.a)
					m_vBoxes.emplace_back(vTarget, tInfo.m_vHull * -1, tInfo.m_vHull, Vec3(), I::GlobalVars->curtime + (Vars::Visuals::Simulation::Timed.Value ? flProjectileTime : Vars::Visuals::Hitbox::DrawDuration.Value), Vars::Colors::BoundHitboxEdgeIgnoreZ.Value, Vars::Colors::BoundHitboxFaceIgnoreZ.Value);
				if (Vars::Colors::BoundHitboxEdge.Value.a || Vars::Colors::BoundHitboxFace.Value.a)
					m_vBoxes.emplace_back(vTarget, tInfo.m_vHull * -1, tInfo.m_vHull, Vec3(), I::GlobalVars->curtime + (Vars::Visuals::Simulation::Timed.Value ? flProjectileTime : Vars::Visuals::Hitbox::DrawDuration.Value), Vars::Colors::BoundHitboxEdge.Value, Vars::Colors::BoundHitboxFace.Value, true);
			}
		}

		if (bMain)
			return true;
	}

	return iReturn;
}



void CAimbotProjectile::ClearLegitAimStepVars()
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

bool CAimbotProjectile::Aim(Vec3 vCurAngle, Vec3 vToAngle, Vec3& vOut, int iMethod)
{
	if (Vec3* pDoubletapAngle = F::Ticks.GetShootAngle())
	{
		vOut = *pDoubletapAngle;
		return true;
	}

	bool bReturn = false;
	switch (iMethod)
	{
	case Vars::Aimbot::General::AimTypeEnum::Plain:
	case Vars::Aimbot::General::AimTypeEnum::Silent:
		vOut = vToAngle;
		break;
	case Vars::Aimbot::General::AimTypeEnum::Smooth:
	{
		Vec3 vOldAngles = vCurAngle;

		Vec3 vDelta = vToAngle - vCurAngle;
		Math::NormalizeAngle(vDelta.x);
		Math::NormalizeAngle(vDelta.y);
		Math::ClampAngles(vDelta);

		if (!m_bInitializedLegitAimStepDirection)
		{
			m_LegitAimStartDirection = vDelta.y > 0 ? LEFT : RIGHT;
			m_vLegitAimStepInitialDelta = vDelta;
			m_bInitializedLegitAimStepDirection = true;
		}

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
				m_flCurAimTime = 0.0f;
				m_bInitializedLegitAimStepDirection = false;
				m_bReachedLegitAimStepTarget = false;  // CRITICAL: Fix camera flicking issue
				m_vLegitAimStepInitialDelta = vDelta;
			}
		}

		static float flLastAimStepTime = 0.0f;
		if (flLastAimStepTime == 0.0f)
			flLastAimStepTime = I::GlobalVars->curtime;

		float flFrameTime = std::min(I::GlobalVars->curtime - flLastAimStepTime, 0.1f);
		flLastAimStepTime = I::GlobalVars->curtime;

		m_flCurAimTime += flFrameTime;

		float flFOV = std::max(0.001f, Math::CalcFov(vOldAngles, vToAngle));

		float flSmoothScale = std::max(0.1f, Vars::Aimbot::General::SmoothStrength.Value);
		float flSmoothTime = m_flCurAimTime * flSmoothScale + (m_bReachedLegitAimStepTarget ? 0.1f / flFOV : 0.33f / flFOV);

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

		if (G::CurrentUserCmd && (abs(G::CurrentUserCmd->mousedx) > 2 || abs(G::CurrentUserCmd->mousedy) > 2))
		{
			flSmoothTime *= 0.25f;
			m_flCurAimTime = std::max(0.0f, m_flCurAimTime - (flFrameTime * 2.0f));
		}

		vOut = vToAngle;
		SmoothAngle(vOldAngles, vOut, flSmoothTime);

		Vec3 vDeltaAngle = (vOut - vOldAngles);
		Math::NormalizeAngle(vDeltaAngle.x);
		Math::NormalizeAngle(vDeltaAngle.y);

		switch (m_nLegitAimCurveType)
		{
		case 0:
			if (vDeltaAngle.y > 0.4f)
			{
				float flInc = RandFloatRange(0.265f, 0.291f) - std::min(0.0f, (1.0f / flFOV)) - flSmoothTime;
				if (m_vLegitAimStepInitialDelta.x < 0)
					vOut.x -= flInc;
				else
					vOut.x += flInc;
				vOut.x = std::clamp(Math::NormalizeAngle(vOut.x), -89.f, 89.f);
			}
			else if (vDeltaAngle.y < -0.4f)
			{
				float flInc = RandFloatRange(0.252f, 0.294f) - std::min(0.0f, (1.0f / flFOV)) - flSmoothTime;
				if (m_vLegitAimStepInitialDelta.x < 0)
					vOut.x -= flInc;
				else
					vOut.x += flInc;
				vOut.x = std::clamp(Math::NormalizeAngle(vOut.x), -89.f, 89.f);
			}
			else if (vDeltaAngle.y > 0.2f)
			{
				float flInc = RandFloatRange(-0.002f, 0.0385f) - std::min(0.0f, (1.0f / flFOV)) - flSmoothTime;
				if (m_vLegitAimStepInitialDelta.x < 0)
					vOut.x -= flInc;
				else
					vOut.x += flInc;
				vOut.x = std::clamp(Math::NormalizeAngle(vOut.x), -89.f, 89.f);
			}
			else if (vDeltaAngle.y < -0.2f)
			{
				float flInc = RandFloatRange(-0.00212f, 0.032f) - std::min(0.0f, (1.0f / flFOV)) - flSmoothTime;
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
				float flInc = RandFloatRange(0.265f, 0.331f) - std::min(0.0f, (1.0f / flFOV)) - flSmoothTime;
				if (m_vLegitAimStepInitialDelta.x < 0)
					vOut.x -= flInc;
				else
					vOut.x += flInc;
				vOut.x = std::clamp(Math::NormalizeAngle(vOut.x), -89.f, 89.f);
			}
			else if (vDeltaAngle.y < -0.4f)
			{
				float flInc = RandFloatRange(0.252f, 0.324f) - std::min(0.0f, (1.0f / flFOV)) - flSmoothTime;
				if (m_vLegitAimStepInitialDelta.x < 0)
					vOut.x -= flInc;
				else
					vOut.x += flInc;
				vOut.x = std::clamp(Math::NormalizeAngle(vOut.x), -89.f, 89.f);
			}
			else if (vDeltaAngle.y > 0.2f)
			{
				float flInc = RandFloatRange(-0.002f, 0.0385f) - std::min(0.0f, (1.0f / flFOV)) - flSmoothTime;
				if (m_vLegitAimStepInitialDelta.x < 0)
					vOut.x -= flInc;
				else
					vOut.x += flInc;
				vOut.x = std::clamp(Math::NormalizeAngle(vOut.x), -89.f, 89.f);
			}
			else if (vDeltaAngle.y < -0.2f)
			{
				float flInc = RandFloatRange(-0.00212f, 0.032f) - std::min(0.0f, (1.0f / flFOV)) - flSmoothTime;
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
				float flInc = RandFloatRange(0.2f, 0.25f) - std::min(0.0f, (1.0f / flFOV)) - flSmoothTime;
				if (m_vLegitAimStepInitialDelta.x < 0)
					vOut.x -= flInc;
				else
					vOut.x += flInc;
				vOut.x = std::clamp(Math::NormalizeAngle(vOut.x), -89.f, 89.f);
			}
			else if (vDeltaAngle.y < -0.4f)
			{
				float flInc = RandFloatRange(0.15f, 0.20f) - std::min(0.0f, (1.0f / flFOV)) - flSmoothTime;
				if (m_vLegitAimStepInitialDelta.x < 0)
					vOut.x -= flInc;
				else
					vOut.x += flInc;
				vOut.x = std::clamp(Math::NormalizeAngle(vOut.x), -89.f, 89.f);
			}
			else if (vDeltaAngle.y > 0.2f)
			{
				float flInc = RandFloatRange(-0.005f, 0.018f) - std::min(0.0f, (1.0f / flFOV)) - flSmoothTime;
				if (m_vLegitAimStepInitialDelta.x < 0)
					vOut.x -= flInc;
				else
					vOut.x += flInc;
				vOut.x = std::clamp(Math::NormalizeAngle(vOut.x), -89.f, 89.f);
			}
			else if (vDeltaAngle.y < -0.2f)
			{
				float flInc = RandFloatRange(-0.001f, 0.04f) - std::min(0.0f, (1.0f / flFOV)) - flSmoothTime;
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
				// NaturalHuman: Physics-based curve derived from real mouse movement data
				const float flTotalDist = vDeltaAngle.Length2D();
				const float flProgress = std::min(1.0f, flSmoothTime / std::max(0.001f, flFOV * 0.1f));
				const float flPeakTime = 0.2f;
				float flVelocityFactor;

				if (flProgress < flPeakTime)
				{
					flVelocityFactor = std::pow(flProgress / flPeakTime, 1.5f);
				}
				else
				{
					const float flRemaining = 1.0f - flPeakTime;
					const float flDecayProgress = (flProgress - flPeakTime) / flRemaining;
					flVelocityFactor = std::exp(-2.5f * flDecayProgress);
				}

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
void CAimbotProjectile::Aim(CUserCmd* pCmd, Vec3& vAngle, int iMethod)
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
		if (auto pWeapon = H::Entities.GetWeapon();
			G::Attacking == 1 || bUnsure || pWeapon && pWeapon->GetWeaponID() == TF_WEAPON_FLAMETHROWER)
		{
			SDK::FixMovement(pCmd, vAngle);
			pCmd->viewangles = vAngle;
			G::PSilentAngles = true;
		}
		break;
	}
}

static inline void CancelShot(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd, int& iLastTickCancel)
{
	switch (pWeapon->GetWeaponID())
	{
	case TF_WEAPON_COMPOUND_BOW:
	{
		pCmd->buttons |= IN_ATTACK2;
		pCmd->buttons &= ~IN_ATTACK;
		break;
	}
	case TF_WEAPON_CANNON:
	case TF_WEAPON_PIPEBOMBLAUNCHER:
	{
		for (int i = 0; i < MAX_WEAPONS; i++)
		{
			auto pSwap = pLocal->GetWeaponFromSlot(i);
			if (!pSwap || pSwap == pWeapon || !pSwap->CanBeSelected())
				continue;

			pCmd->weaponselect = pSwap->entindex();
			iLastTickCancel = pWeapon->entindex();
			break;
		}
	}
	}
}

static inline void DrawVisuals(int iResult, Target_t& tTarget, std::vector<Vec3>& vPlayerPath, std::vector<Vec3>& vProjectilePath, std::vector<DrawBox_t>& vBoxes)
{
	if (G::Attacking == 1 || !Vars::Aimbot::General::AutoShoot.Value || iResult != 1)
	{
		bool bPlayerPath = Vars::Visuals::Simulation::PlayerPath.Value;
		bool bProjectilePath = Vars::Visuals::Simulation::ProjectilePath.Value && (G::Attacking == 1 || Vars::Debug::Info.Value) && iResult == 1;
		bool bBoxes = Vars::Visuals::Hitbox::BoundsEnabled.Value & (Vars::Visuals::Hitbox::BoundsEnabledEnum::OnShot | Vars::Visuals::Hitbox::BoundsEnabledEnum::AimPoint);
		bool bRealPath = Vars::Visuals::Simulation::RealPath.Value && iResult == 1;
		if (bPlayerPath || bProjectilePath || bBoxes || bRealPath)
		{
			G::PathStorage.clear();
			G::BoxStorage.clear();
			G::LineStorage.clear();

			if (bPlayerPath)
			{
				if (Vars::Colors::PlayerPathIgnoreZ.Value.a)
					G::PathStorage.emplace_back(vPlayerPath, Vars::Visuals::Simulation::Timed.Value ? -int(vPlayerPath.size()) : I::GlobalVars->curtime + Vars::Visuals::Simulation::DrawDuration.Value, Vars::Colors::PlayerPathIgnoreZ.Value, Vars::Visuals::Simulation::PlayerPath.Value);
				if (Vars::Colors::PlayerPath.Value.a)
					G::PathStorage.emplace_back(vPlayerPath, Vars::Visuals::Simulation::Timed.Value ? -int(vPlayerPath.size()) : I::GlobalVars->curtime + Vars::Visuals::Simulation::DrawDuration.Value, Vars::Colors::PlayerPath.Value, Vars::Visuals::Simulation::PlayerPath.Value, true);
			}
			if (bProjectilePath)
			{
				if (Vars::Colors::ProjectilePathIgnoreZ.Value.a)
					G::PathStorage.emplace_back(vProjectilePath, Vars::Visuals::Simulation::Timed.Value ? -int(vProjectilePath.size()) - TIME_TO_TICKS(F::Backtrack.GetReal()) : I::GlobalVars->curtime + Vars::Visuals::Simulation::DrawDuration.Value, Vars::Colors::ProjectilePathIgnoreZ.Value, Vars::Visuals::Simulation::ProjectilePath.Value);
				if (Vars::Colors::ProjectilePath.Value.a)
					G::PathStorage.emplace_back(vProjectilePath, Vars::Visuals::Simulation::Timed.Value ? -int(vProjectilePath.size()) - TIME_TO_TICKS(F::Backtrack.GetReal()) : I::GlobalVars->curtime + Vars::Visuals::Simulation::DrawDuration.Value, Vars::Colors::ProjectilePath.Value, Vars::Visuals::Simulation::ProjectilePath.Value, true);
			}
			if (bBoxes)
				G::BoxStorage.insert(G::BoxStorage.end(), vBoxes.begin(), vBoxes.end());
			if (bRealPath)
				F::Aimbot.Store(tTarget.m_pEntity, vPlayerPath.size());
		}
	}
}

bool CAimbotProjectile::RunMain(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd)
{
	const int nWeaponID = pWeapon->GetWeaponID();

	static int iStaticAimType = Vars::Aimbot::General::AimType.Value;
	const int iLastAimType = iStaticAimType;
	const int iRealAimType = Vars::Aimbot::General::AimType.Value;

	switch (nWeaponID)
	{
	case TF_WEAPON_COMPOUND_BOW:
	case TF_WEAPON_PIPEBOMBLAUNCHER:
	case TF_WEAPON_CANNON:
		if (!Vars::Aimbot::General::AutoShoot.Value && G::Attacking && !iRealAimType && iLastAimType)
			Vars::Aimbot::General::AimType.Value = iLastAimType;
		break;
	default:
		if (G::Throwing && !iRealAimType && iLastAimType)
			Vars::Aimbot::General::AimType.Value = iLastAimType;
	}
	iStaticAimType = Vars::Aimbot::General::AimType.Value;

	if (F::AimbotGlobal.ShouldHoldAttack(pWeapon))
		pCmd->buttons |= IN_ATTACK;
	if (!Vars::Aimbot::General::AimType.Value
		|| !F::AimbotGlobal.ShouldAim() && nWeaponID != TF_WEAPON_FLAMETHROWER)
		return false;

	auto vTargets = SortTargets(pLocal, pWeapon);
	if (vTargets.empty())
		return false;

	if (Vars::Aimbot::Projectile::Modifiers.Value & Vars::Aimbot::Projectile::ModifiersEnum::ChargeWeapon && iRealAimType
		&& (nWeaponID == TF_WEAPON_COMPOUND_BOW || nWeaponID == TF_WEAPON_PIPEBOMBLAUNCHER))
	{
		pCmd->buttons |= IN_ATTACK;
		if (!G::CanPrimaryAttack && !G::Reloading && Vars::Aimbot::General::AimType.Value == Vars::Aimbot::General::AimTypeEnum::Silent)
			return false;
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

#if defined(SPLASH_DEBUG1) || defined(SPLASH_DEBUG2) || defined(SPLASH_DEBUG3) || defined(SPLASH_DEBUG5)
	G::LineStorage.clear();
#endif
#if defined(SPLASH_DEBUG1) || defined(SPLASH_DEBUG2) || defined(SPLASH_DEBUG3) || defined(SPLASH_DEBUG4) || defined(SPLASH_DEBUG5)
	G::BoxStorage.clear();
#endif
	for (auto& tTarget : vTargets)
	{
		// Skip targets that don't match our current target if switch delay is active
		if (!bCanSwitchTarget && tTarget.m_pEntity->entindex() != G::AimTarget.m_iEntIndex)
			continue;

		m_flTimeTo = std::numeric_limits<float>::max();
		m_vPlayerPath.clear(); m_vProjectilePath.clear(); m_vBoxes.clear();

		const int iResult = CanHit(tTarget, pLocal, pWeapon);
		if (iResult != 1 && pWeapon->GetWeaponID() == TF_WEAPON_CANNON && Vars::Aimbot::Projectile::Modifiers.Value & Vars::Aimbot::Projectile::ModifiersEnum::ChargeWeapon && !(pCmd->buttons & IN_ATTACK))
		{
			float flCharge = pWeapon->As<CTFGrenadeLauncher>()->m_flDetonateTime() > 0.f
				? std::clamp(pWeapon->As<CTFGrenadeLauncher>()->m_flDetonateTime() - I::GlobalVars->curtime, 0.f, 1.f)
				: 1.f;
			if (flCharge < m_flTimeTo)
			{
				if (pWeapon->As<CTFGrenadeLauncher>()->m_flDetonateTime() > 0.f)
					CancelShot(pLocal, pWeapon, pCmd, m_iLastTickCancel);
			}
			else
			{
				if (m_iLastTickCancel)
					pCmd->weaponselect = m_iLastTickCancel = 0;
				pCmd->buttons |= IN_ATTACK;
			}
		}
		if (!iResult) continue;
		if (iResult == 2)
		{
			// Only switch target if we can (delay expired)
			if (bCanSwitchTarget)
			{
				G::AimTarget = { tTarget.m_pEntity->entindex(), I::GlobalVars->tickcount, 32, flCurrentTime };
			}
			DrawVisuals(iResult, tTarget, m_vPlayerPath, m_vProjectilePath, m_vBoxes);
			Aim(pCmd, tTarget.m_vAngleTo);
			break;
		}

		// Only switch target if we can (delay expired)
		if (bCanSwitchTarget || tTarget.m_pEntity->entindex() == G::AimTarget.m_iEntIndex)
		{
			G::AimTarget = { tTarget.m_pEntity->entindex(), I::GlobalVars->tickcount, 32, flCurrentTime };
			G::AimPoint = { tTarget.m_vPos, I::GlobalVars->tickcount };
		}

		if (Vars::Aimbot::General::AutoShoot.Value)
		{
			switch (nWeaponID)
			{
			case TF_WEAPON_COMPOUND_BOW:
			case TF_WEAPON_PIPEBOMBLAUNCHER:
				pCmd->buttons |= IN_ATTACK;
				if (pWeapon->As<CTFPipebombLauncher>()->m_flChargeBeginTime() > 0.f)
					pCmd->buttons &= ~IN_ATTACK;
				break;
			case TF_WEAPON_CANNON:
				pCmd->buttons |= IN_ATTACK;
				if (pWeapon->As<CTFGrenadeLauncher>()->m_flDetonateTime() > 0.f)
				{
					if (m_iLastTickCancel)
						pCmd->weaponselect = m_iLastTickCancel = 0;
					if (Vars::Aimbot::Projectile::Modifiers.Value & Vars::Aimbot::Projectile::ModifiersEnum::ChargeWeapon)
					{
						float flCharge = pWeapon->As<CTFGrenadeLauncher>()->m_flDetonateTime() - I::GlobalVars->curtime;
						if (std::clamp(flCharge, 0.f, 1.f) < m_flTimeTo)
							pCmd->buttons &= ~IN_ATTACK;
					}
					else
						pCmd->buttons &= ~IN_ATTACK;
				}
				break;
			case TF_WEAPON_BAT_WOOD:
			case TF_WEAPON_BAT_GIFTWRAP:
			case TF_WEAPON_LUNCHBOX:
				pCmd->buttons &= ~IN_ATTACK, pCmd->buttons |= IN_ATTACK2;
				break;
			default:
				pCmd->buttons |= IN_ATTACK;
				if (pWeapon->m_iItemDefinitionIndex() == Soldier_m_TheBeggarsBazooka)
				{
					if (pWeapon->m_iClip1() > 0)
						pCmd->buttons &= ~IN_ATTACK;
				}
			}
		}

		F::Aimbot.m_bRan = G::Attacking = SDK::IsAttacking(pLocal, pWeapon, pCmd, true);
		DrawVisuals(iResult, tTarget, m_vPlayerPath, m_vProjectilePath, m_vBoxes);

		Aim(pCmd, tTarget.m_vAngleTo);
		if (G::PSilentAngles)
		{
			switch (nWeaponID)
			{
			case TF_WEAPON_FLAMETHROWER: // angles show up anyways
			case TF_WEAPON_CLEAVER: // can't psilent with these weapons, they use SetContextThink
			case TF_WEAPON_JAR:
			case TF_WEAPON_JAR_MILK:
			case TF_WEAPON_JAR_GAS:
			case TF_WEAPON_BAT_WOOD:
			case TF_WEAPON_BAT_GIFTWRAP:
				G::PSilentAngles = false, G::SilentAngles = true;
			}
		}
		return true;
	}

	return false;
}

void CAimbotProjectile::Run(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd)
{
	const bool bSuccess = RunMain(pLocal, pWeapon, pCmd);
#if defined(SPLASH_DEBUG6)
	if (Vars::Aimbot::General::AimType.Value && !mTraceCount.empty())
	{
		int iTraceCount = 0;
		for (auto& [_, iTraces] : mTraceCount)
			iTraceCount += iTraces;
		SDK::Output("Traces", std::format("{}", iTraceCount).c_str());
		for (auto& [sType, iTraces] : mTraceCount)
			SDK::Output("Traces", std::format("{}: {}", sType, iTraces).c_str());
	}
	mTraceCount.clear();
#endif

	float flAmount = 0.f;
	if (pWeapon->GetWeaponID() == TF_WEAPON_PIPEBOMBLAUNCHER)
	{
		const float flCharge = pWeapon->As<CTFPipebombLauncher>()->m_flChargeBeginTime() > 0.f ? I::GlobalVars->curtime - pWeapon->As<CTFPipebombLauncher>()->m_flChargeBeginTime() : 0.f;
		flAmount = Math::RemapVal(flCharge, 0.f, SDK::AttribHookValue(4.f, "stickybomb_charge_rate", pWeapon), 0.f, 1.f);
	}
	else if (pWeapon->GetWeaponID() == TF_WEAPON_CANNON)
	{
		const float flMortar = SDK::AttribHookValue(0.f, "grenade_launcher_mortar_mode", pWeapon);
		const float flCharge = pWeapon->As<CTFGrenadeLauncher>()->m_flDetonateTime() > 0.f ? I::GlobalVars->curtime - pWeapon->As<CTFGrenadeLauncher>()->m_flDetonateTime() : -flMortar;
		flAmount = flMortar ? Math::RemapVal(flCharge, -flMortar, 0.f, 0.f, 1.f) : 0.f;
	}

	if (pWeapon->GetWeaponID() == TF_WEAPON_PIPEBOMBLAUNCHER && G::OriginalMove.m_iButtons & IN_ATTACK && Vars::Aimbot::Projectile::AutoRelease.Value && flAmount > Vars::Aimbot::Projectile::AutoRelease.Value / 100)
		pCmd->buttons &= ~IN_ATTACK;
	else if (G::CanPrimaryAttack && Vars::Aimbot::Projectile::Modifiers.Value & Vars::Aimbot::Projectile::ModifiersEnum::CancelCharge)
	{
		if (m_bLastTickHeld && (G::LastUserCmd->buttons & IN_ATTACK && !(pCmd->buttons & IN_ATTACK) && !bSuccess || flAmount > 0.95f))
			CancelShot(pLocal, pWeapon, pCmd, m_iLastTickCancel);
	}

	m_bLastTickHeld = Vars::Aimbot::General::AimType.Value;
}



// TestAngle and CanHit shares a bunch of code, possibly merge somehow

bool CAimbotProjectile::TestAngle(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CBaseEntity* pProjectile, Target_t& tTarget, Info_t& tInfo, Vec3& vPoint, Vec3& vAngles, int iSimTime, bool bSplash, std::vector<Vec3>* pProjectilePath)
{
	ProjectileInfo tProjInfo = {};
	F::ProjSim.GetInfo(pProjectile, tProjInfo);
	CGameTrace trace = {};
	{
		CTraceFilterWorldAndPropsOnly filter = {};

		Vec3 vEyePos = pLocal->GetShootPos();
		tProjInfo.m_vPos = pProjectile->GetAbsOrigin();

		Vec3 vPos = vPoint;
		if (tInfo.m_flGravity)
			vPos += Vec3(0, 0, (tInfo.m_flGravity * 800.f * pow(TICKS_TO_TIME(iSimTime), 2)) / 2);
		Vec3 vForward = (vPos - tProjInfo.m_vPos).Normalized();
		tProjInfo.m_vAng = Math::VectorAngles(vForward);

		SDK::Trace(tProjInfo.m_vPos, tProjInfo.m_vPos + vForward * MAX_TRACE_LENGTH, MASK_SOLID, &filter, &trace);
		vAngles = Math::CalcAngle(vEyePos, trace.endpos);
		vForward = (vEyePos - trace.endpos).Normalized();
		if (vForward.Dot(trace.plane.normal) <= 0)
			return false;

		SDK::Trace(vEyePos, trace.endpos, MASK_SOLID, &filter, &trace);
		if (trace.fraction < 0.999f)
			return false;

		if (!F::AutoAirblast.CanAirblastEntity(pLocal, pWeapon, pProjectile, vAngles))
			return false;
	}
	if (!F::ProjSim.Initialize(tProjInfo, false, true))
		return false;

	CTraceFilterCollideable filter = {}; filter.pSkip = bSplash ? tTarget.m_pEntity : pLocal; filter.iPlayer = bSplash ? PLAYER_NONE : PLAYER_DEFAULT;
	int nMask = MASK_SOLID;
	F::ProjSim.SetupTrace(filter, nMask, pProjectile);

	if (!tProjInfo.m_flGravity)
	{
		SDK::TraceHull(tProjInfo.m_vPos, vPoint, tProjInfo.m_vHull * -1, tProjInfo.m_vHull, nMask, &filter, &trace);
		if (trace.fraction < 0.999f && trace.m_pEnt != tTarget.m_pEntity)
			return false;
	}

	bool bDidHit = false;
	const Vec3 vOriginal = tTarget.m_pEntity->GetAbsOrigin();
	tTarget.m_pEntity->SetAbsOrigin(tTarget.m_vPos);
	for (int n = 1; n <= iSimTime; n++)
	{
		Vec3 vOld = F::ProjSim.GetOrigin();
		F::ProjSim.RunTick(tProjInfo);
		Vec3 vNew = F::ProjSim.GetOrigin();

		if (bDidHit)
		{
			trace.endpos = vNew;
			continue;
		}

		if (!bSplash)
			SDK::TraceHull(vOld, vNew, tProjInfo.m_vHull * -1, tProjInfo.m_vHull, nMask, &filter, &trace);
		else
		{
			static Vec3 vStaticPos = {};
			if (n == 1)
				vStaticPos = vOld;
			if (n % Vars::Aimbot::Projectile::SplashTraceInterval.Value && n != iSimTime)
				continue;

			SDK::TraceHull(vStaticPos, vNew, tProjInfo.m_vHull * -1, tProjInfo.m_vHull, nMask, &filter, &trace);
			vStaticPos = vNew;
		}
		if (trace.DidHit())
		{
			bool bTime = bSplash
				? trace.endpos.DistTo(vPoint) < tProjInfo.m_flVelocity * TICK_INTERVAL + tProjInfo.m_vHull.z
				: iSimTime - n < 5;
			bool bTarget = trace.m_pEnt == tTarget.m_pEntity || bSplash;
			bool bValid = bTarget && bTime;
			if (bValid && bSplash)
			{
				bValid = SDK::VisPosWorld(nullptr, tTarget.m_pEntity, trace.endpos, vPoint, nMask);
				if (bValid)
				{
					Vec3 vFrom = trace.endpos;
					switch (pProjectile->GetClassID())
					{
					case ETFClassID::CTFProjectile_Rocket:
					case ETFClassID::CTFProjectile_SentryRocket:
					case ETFClassID::CTFProjectile_EnergyBall:
						vFrom += trace.plane.normal;
					}

					CGameTrace eyeTrace = {};
					SDK::Trace(vFrom, tTarget.m_vPos + tTarget.m_pEntity->As<CTFPlayer>()->GetViewOffset(), MASK_SHOT, &filter, &eyeTrace);
					bValid = eyeTrace.fraction == 1.f;
				}
			}

			if (bValid)
			{
				if (bSplash)
				{
					int iPopCount = Vars::Aimbot::Projectile::SplashTraceInterval.Value - trace.fraction * Vars::Aimbot::Projectile::SplashTraceInterval.Value;
					for (int i = 0; i < iPopCount && !tProjInfo.m_vPath.empty(); i++)
						tProjInfo.m_vPath.pop_back();
				}

				bDidHit = true;
			}
			else
				break;

			if (!bSplash)
				trace.endpos = vNew;

			if (!bTarget || bSplash)
				break;
		}
	}
	tTarget.m_pEntity->SetAbsOrigin(vOriginal);

	if (bDidHit && pProjectilePath)
	{
		tProjInfo.m_vPath.push_back(trace.endpos);
		*pProjectilePath = tProjInfo.m_vPath;
	}

	return bDidHit;
}


bool CAimbotProjectile::CanHit(Target_t& tTarget, CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CBaseEntity* pProjectile)
{
	ProjectileInfo tProjInfo = {};
	F::ProjSim.GetInfo(pProjectile, tProjInfo);
	if (!F::ProjSim.Initialize(tProjInfo, false, true))
		return false;

	MoveStorage tStorage;
	F::MoveSim.Initialize(tTarget.m_pEntity, tStorage);
	tTarget.m_vPos = tTarget.m_pEntity->m_vecOrigin();

	Info_t tInfo = { pLocal, tProjInfo.m_pWeapon };
	tInfo.m_flLatency = F::Backtrack.GetReal() + TICKS_TO_TIME(F::Backtrack.GetAnticipatedChoke());
	tInfo.m_vHull = pProjectile->m_vecMaxs().Min(3);
	{
		CGameTrace trace = {};
		CTraceFilterWorldAndPropsOnly filter = {};

		for (int i = TIME_TO_TICKS(tInfo.m_flLatency); i > 0; i--)
		{
			Vec3 vOld = F::ProjSim.GetOrigin();
			F::ProjSim.RunTick(tProjInfo);
			Vec3 vNew = F::ProjSim.GetOrigin();

			SDK::TraceHull(vOld, vNew, tProjInfo.m_vHull * -1, tProjInfo.m_vHull, MASK_SOLID, &filter, &trace);
			tProjInfo.m_vPos = trace.endpos;
		}
		tInfo.m_vLocalEye = tProjInfo.m_vPos; // just assume from the projectile without any offset, check validity later
		pProjectile->SetAbsOrigin(tProjInfo.m_vPos);
	}
	tInfo.m_vTargetEye = tTarget.m_pEntity->As<CTFPlayer>()->GetViewOffset();

	tInfo.m_flVelocity = tProjInfo.m_flVelocity;

	tInfo.m_flGravity = tProjInfo.m_flGravity;
	tInfo.m_iSplashCount = !tInfo.m_flGravity ? Vars::Aimbot::Projectile::SplashCountDirect.Value : Vars::Aimbot::Projectile::SplashCountArc.Value;

	float flSize = tTarget.m_pEntity->GetSize().Length();
	tInfo.m_flRadius = GetSplashRadius(pProjectile, tProjInfo.m_pWeapon, tProjInfo.m_pOwner, Vars::Aimbot::Projectile::SplashRadius.Value / 100, pWeapon);
	tInfo.m_flRadiusTime = tInfo.m_flRadius / tInfo.m_flVelocity;
	tInfo.m_flBoundingTime = tInfo.m_flRadiusTime + flSize / tInfo.m_flVelocity;



	int iMaxTime = TIME_TO_TICKS(Vars::Aimbot::Projectile::MaxSimulationTime.Value);
	int iSplash = Vars::Aimbot::Projectile::SplashPrediction.Value && tInfo.m_flRadius ? Vars::Aimbot::Projectile::SplashPrediction.Value : Vars::Aimbot::Projectile::SplashPredictionEnum::Off;
	int iMulti = Vars::Aimbot::Projectile::SplashMode.Value;
	int iPoints = !tInfo.m_flGravity ? Vars::Aimbot::Projectile::SplashPointsDirect.Value : Vars::Aimbot::Projectile::SplashPointsArc.Value;

	auto mDirectPoints = iSplash == Vars::Aimbot::Projectile::SplashPredictionEnum::Only ? std::unordered_map<int, Vec3>() : GetDirectPoints(tTarget, tInfo, pProjectile);
	auto vSpherePoints = !iSplash ? std::vector<std::pair<Vec3, int>>() : ComputeSphere(tInfo.m_flRadius + flSize, iPoints, Vars::Aimbot::Projectile::SplashNthRoot.Value);

	Vec3 vAngleTo, vPredicted, vTarget;
	int iLowestPriority = std::numeric_limits<int>::max(); float flLowestDist = std::numeric_limits<float>::max();
	for (int i = 1 - TIME_TO_TICKS(tInfo.m_flLatency); i <= iMaxTime; i++)
	{
		if (!tStorage.m_bFailed)
		{
			F::MoveSim.RunTick(tStorage);
			tTarget.m_vPos = GetSimulatedPos(tStorage);
		}
		if (i < 0)
			continue;

		bool bDirectBreaks = true;
		std::vector<Point_t> vSplashPoints = {};
		if (iSplash)
		{
			Solution_t solution; CalculateAngle(tInfo.m_vLocalEye, tTarget.m_vPos, tInfo, i, solution, false);
			if (solution.m_iCalculated != CalculatedEnum::Bad)
			{
				bDirectBreaks = false;

				const float flTimeTo = solution.m_flTime - TICKS_TO_TIME(i);
				if (flTimeTo < tInfo.m_flBoundingTime)
				{
					static std::vector<std::pair<Vec3, Vec3>> vSimplePoints = {};
					if (iMulti == Vars::Aimbot::Projectile::SplashModeEnum::Single)
					{
						SetupSplashPoints(tTarget, vSpherePoints, tInfo, vSimplePoints);
						if (!vSimplePoints.empty())
							iMulti++;
						else
						{
							iSplash = Vars::Aimbot::Projectile::SplashPredictionEnum::Off;
							goto skipSplash;
						}
					}

					if ((iMulti == Vars::Aimbot::Projectile::SplashModeEnum::Multi ? vSpherePoints.empty() : vSimplePoints.empty())
						|| flTimeTo < -tInfo.m_flBoundingTime)
						break;
					else
					{
						if (iMulti == Vars::Aimbot::Projectile::SplashModeEnum::Multi)
							vSplashPoints = GetSplashPoints(tTarget, vSpherePoints, tInfo, i);
						else
							vSplashPoints = GetSplashPointsSimple(tTarget, vSimplePoints, tInfo, i);
					}
				}
			}
		}
		skipSplash:
		if (bDirectBreaks && mDirectPoints.empty())
			break;

		std::vector<std::tuple<Point_t, int, int>> vPoints = {};
		for (auto& [iIndex, vPoint] : mDirectPoints)
			vPoints.emplace_back(Point_t(tTarget.m_vPos + vPoint, {}), iIndex + (iSplash == Vars::Aimbot::Projectile::SplashPredictionEnum::Prefer ? tInfo.m_iSplashCount : 0), iIndex);
		for (auto& vPoint : vSplashPoints)
			vPoints.emplace_back(vPoint, iSplash == Vars::Aimbot::Projectile::SplashPredictionEnum::Include ? 3 : 0, -1);

		for (auto& [vPoint, iPriority, iIndex] : vPoints) // get most ideal point
		{
			const bool bSplash = iIndex == -1;
			Vec3 vOriginalPoint = vPoint.m_vPoint;

			if (Vars::Aimbot::Projectile::HuntsmanPullPoint.Value && tTarget.m_nAimedHitbox == HITBOX_HEAD)
				vPoint.m_vPoint = PullPoint(vPoint.m_vPoint, tInfo.m_vLocalEye, tInfo, tTarget.m_pEntity->m_vecMins() + tProjInfo.m_vHull, tTarget.m_pEntity->m_vecMaxs() - tProjInfo.m_vHull, tTarget.m_vPos);
				//vPoint.m_vPoint = PullPoint(vPoint.m_vPoint, tInfo.m_vLocalEye, tInfo, tTarget.m_pEntity->m_vecMins(), tTarget.m_pEntity->m_vecMaxs(), tTarget.m_vPos);

			float flDist = bSplash ? tTarget.m_vPos.DistTo(vPoint.m_vPoint) : flLowestDist;
			bool bPriority = bSplash ? iPriority <= iLowestPriority : iPriority < iLowestPriority;
			bool bTime = bSplash || tInfo.m_iPrimeTime < i || tStorage.m_MoveData.m_vecVelocity.IsZero();
			bool bDist = !bSplash || flDist < flLowestDist;
			if (!bSplash && !bPriority)
				mDirectPoints.erase(iIndex);
			if (!bPriority || !bTime || !bDist)
				continue;

			bool bCalculatedAny = false;

			for (int iPitch = 1; iPitch <= 2; iPitch++)
			{
				vPoint.m_Solution.m_iCalculated = CalculatedEnum::Pending;
				CalculateAngle(tInfo.m_vLocalEye, vPoint.m_vPoint, tInfo, i, vPoint.m_Solution, true, iPitch);

				if (!bSplash && (vPoint.m_Solution.m_iCalculated == CalculatedEnum::Good || vPoint.m_Solution.m_iCalculated == CalculatedEnum::Bad))
					bCalculatedAny = true;
				if (vPoint.m_Solution.m_iCalculated != CalculatedEnum::Good)
					continue;

				if (Vars::Aimbot::Projectile::HuntsmanPullPoint.Value && tTarget.m_nAimedHitbox == HITBOX_HEAD)
				{
					Solution_t tSolution;
					CalculateAngle(tInfo.m_vLocalEye, vOriginalPoint, tInfo, std::numeric_limits<int>::max(), tSolution, true, iPitch);
					vPoint.m_Solution.m_flPitch = tSolution.m_flPitch, vPoint.m_Solution.m_flYaw = tSolution.m_flYaw;
				}

				Vec3 vAngles; Aim(G::CurrentUserCmd->viewangles, { vPoint.m_Solution.m_flPitch, vPoint.m_Solution.m_flYaw, 0.f }, vAngles, Vars::Aimbot::General::AimTypeEnum::Plain);
				std::vector<Vec3> vProjLines;

				if (TestAngle(pLocal, pWeapon, pProjectile, tTarget, tInfo, vPoint.m_vPoint, vAngles, i, bSplash, &vProjLines))
				{
					iLowestPriority = iPriority; flLowestDist = flDist;
					vAngleTo = vAngles, vPredicted = tTarget.m_vPos, vTarget = vOriginalPoint;
					m_flTimeTo = vPoint.m_Solution.m_flTime + tInfo.m_flLatency;
					m_vPlayerPath = tStorage.m_vPath;
					m_vPlayerPath.push_back(tStorage.m_MoveData.m_vecAbsOrigin);
					m_vProjectilePath = vProjLines;
					break;
				}
			}

			if (bCalculatedAny && !bSplash)
				mDirectPoints.erase(iIndex);
		}
	}
	F::MoveSim.Restore(tStorage);

	tTarget.m_vPos = vTarget;
	tTarget.m_vAngleTo = vAngleTo;
	if (tTarget.m_iTargetType != TargetEnum::Player || !tStorage.m_bFailed) // don't attempt to aim at players when movesim fails
	{
		if (iLowestPriority != std::numeric_limits<int>::max())
		{
			if (Vars::Colors::BoundHitboxEdge.Value.a || Vars::Colors::BoundHitboxFace.Value.a || Vars::Colors::BoundHitboxEdgeIgnoreZ.Value.a || Vars::Colors::BoundHitboxFaceIgnoreZ.Value.a)
			{
				tInfo.m_vHull = tInfo.m_vHull.Max(1);
				float flProjectileTime = TICKS_TO_TIME(m_vProjectilePath.size());
				float flTargetTime = tStorage.m_bFailed ? flProjectileTime : TICKS_TO_TIME(m_vPlayerPath.size());

				bool bBox = Vars::Visuals::Hitbox::BoundsEnabled.Value & Vars::Visuals::Hitbox::BoundsEnabledEnum::OnShot;
				bool bPoint = Vars::Visuals::Hitbox::BoundsEnabled.Value & Vars::Visuals::Hitbox::BoundsEnabledEnum::AimPoint;
				if (bBox)
				{
					if (Vars::Colors::BoundHitboxEdgeIgnoreZ.Value.a || Vars::Colors::BoundHitboxFaceIgnoreZ.Value.a)
						m_vBoxes.emplace_back(vPredicted, tTarget.m_pEntity->m_vecMins(), tTarget.m_pEntity->m_vecMaxs(), Vec3(), I::GlobalVars->curtime + (Vars::Visuals::Simulation::Timed.Value ? flTargetTime : Vars::Visuals::Hitbox::DrawDuration.Value), Vars::Colors::BoundHitboxEdgeIgnoreZ.Value, Vars::Colors::BoundHitboxFaceIgnoreZ.Value);
					if (Vars::Colors::BoundHitboxEdge.Value.a || Vars::Colors::BoundHitboxFace.Value.a)
						m_vBoxes.emplace_back(vPredicted, tTarget.m_pEntity->m_vecMins(), tTarget.m_pEntity->m_vecMaxs(), Vec3(), I::GlobalVars->curtime + (Vars::Visuals::Simulation::Timed.Value ? flTargetTime : Vars::Visuals::Hitbox::DrawDuration.Value), Vars::Colors::BoundHitboxEdge.Value, Vars::Colors::BoundHitboxFace.Value, true);
				}
				if (bPoint)
				{
					if (Vars::Colors::BoundHitboxEdgeIgnoreZ.Value.a || Vars::Colors::BoundHitboxFaceIgnoreZ.Value.a)
						m_vBoxes.emplace_back(vTarget, tInfo.m_vHull * -1, tInfo.m_vHull, Vec3(), I::GlobalVars->curtime + (Vars::Visuals::Simulation::Timed.Value ? flProjectileTime : Vars::Visuals::Hitbox::DrawDuration.Value), Vars::Colors::BoundHitboxEdgeIgnoreZ.Value, Vars::Colors::BoundHitboxFaceIgnoreZ.Value);
					if (Vars::Colors::BoundHitboxEdge.Value.a || Vars::Colors::BoundHitboxFace.Value.a)
						m_vBoxes.emplace_back(vTarget, tInfo.m_vHull * -1, tInfo.m_vHull, Vec3(), I::GlobalVars->curtime + (Vars::Visuals::Simulation::Timed.Value ? flProjectileTime : Vars::Visuals::Hitbox::DrawDuration.Value), Vars::Colors::BoundHitboxEdge.Value, Vars::Colors::BoundHitboxFace.Value, true);
				}
			}

			return true;
		}
	}

	return false;
}

bool CAimbotProjectile::AutoAirblast(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd, CBaseEntity* pProjectile)
{
	auto vTargets = SortTargets(pLocal, pWeapon);
	if (vTargets.empty())
		return false;

	//if (!G::AimTarget.m_iEntIndex)
	//	G::AimTarget = { vTargets.front().m_pEntity->entindex(), I::GlobalVars->tickcount, 0 };

	for (auto& tTarget : vTargets)
	{
		m_flTimeTo = std::numeric_limits<float>::max();
		m_vPlayerPath.clear(); m_vProjectilePath.clear(); m_vBoxes.clear();

		const bool bResult = CanHit(tTarget, pLocal, pWeapon, pProjectile);
		if (!bResult) continue;

		G::AimTarget = { tTarget.m_pEntity->entindex(), I::GlobalVars->tickcount, 32, I::EngineClient->Time() };
		G::AimPoint = { tTarget.m_vPos, I::GlobalVars->tickcount };

		G::Attacking = true;
		DrawVisuals(1, tTarget, m_vPlayerPath, m_vProjectilePath, m_vBoxes);

		Aim(pCmd, tTarget.m_vAngleTo, Vars::Aimbot::General::AimTypeEnum::Silent);
		return true;
	}

	return false;
}