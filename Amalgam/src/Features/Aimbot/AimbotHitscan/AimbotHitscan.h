#pragma once
#include "../../../SDK/SDK.h"

#include "../AimbotGlobal/AimbotGlobal.h"

class CAimbotHitscan
{
private:
<<<<<<< HEAD
	enum AimDirection { LEFT = 0, RIGHT };

	std::vector<Target_t> GetTargets(CTFPlayer* pLocal, CTFWeaponBase* pWeapon);
	std::vector<Target_t> SortTargets(CTFPlayer* pLocal, CTFWeaponBase* pWeapon);

=======
>>>>>>> upstream/master
	int GetHitboxPriority(int nHitbox, CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CBaseEntity* pTarget);
	int CanHit(Target_t& tTarget, CTFPlayer* pLocal, CTFWeaponBase* pWeapon);

	bool Aim(Vec3 vCurAngle, Vec3 vToAngle, Vec3& vOut, int iMethod = Vars::Aimbot::General::AimType.Value);
	void Aim(CUserCmd* pCmd, Vec3& vAngle, int iMethod = Vars::Aimbot::General::AimType.Value);
	bool ShouldFire(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd, const Target_t& tTarget);

<<<<<<< HEAD
	bool ShouldWarpPredict(CTFPlayer* pTarget);
	Vec3 WarpPredictDelta(CTFPlayer* pTarget, const Vec3& vEyePos, const Vec3& vOrigin);

	Vec3 m_vEyePos = {};

	void ClearLegitAimStepVars();

	// Smooth aim state variables
	float m_flCurAimTime = 0.0f;
	float m_flLegitAimStepIncTimeOverShoot = 0.0f;
	bool m_bReachedLegitAimStepTarget = false;
	bool m_bInitializedLegitAimStepDirection = false;
	AimDirection m_LegitAimStartDirection = LEFT;
	Vec3 m_vLegitAimStepInitialDelta = {};
	int m_nLegitAimCurveType = 0;

=======
	Vec3 m_vEyePos = {};

>>>>>>> upstream/master
public:
	void Run(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd);
};

ADD_FEATURE(CAimbotHitscan, AimbotHitscan);