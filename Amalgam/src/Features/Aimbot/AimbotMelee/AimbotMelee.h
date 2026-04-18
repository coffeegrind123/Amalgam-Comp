#pragma once
#include "../../../SDK/SDK.h"

#include "../AimbotGlobal/AimbotGlobal.h"

class CAimbotMelee
{
private:
<<<<<<< HEAD
	enum AimDirection { LEFT = 0, RIGHT };

	std::vector<Target_t> GetTargets(CTFPlayer* pLocal, CTFWeaponBase* pWeapon);
	bool AimFriendlyBuilding(CBaseObject* pBuilding);
	std::vector<Target_t> SortTargets(CTFPlayer* pLocal, CTFWeaponBase* pWeapon);

	int GetSwingTime(CTFWeaponBase* pWeapon);
	void SimulatePlayers(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, std::vector<Target_t> vTargets, Vec3& vEyePos);
	bool CanBackstab(CBaseEntity* pTarget, CTFPlayer* pLocal, Vec3 vEyeAngles);
	int CanHit(Target_t& tTarget, CTFPlayer* pLocal, CTFWeaponBase* pWeapon, Vec3 vEyePos);

=======
	int GetSwingTime(CTFWeaponBase* pWeapon, bool bVar = true);
	void UpdateInfo(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd, std::vector<Target_t> vTargets);
	bool CanBackstab(CBaseEntity* pTarget, CTFPlayer* pLocal, Vec3 vEyeAngles);
	int CanHit(Target_t& tTarget, CTFPlayer* pLocal, CTFWeaponBase* pWeapon);
	
>>>>>>> upstream/master
	bool Aim(Vec3 vCurAngle, Vec3 vToAngle, Vec3& vOut, int iMethod = Vars::Aimbot::General::AimType.Value);
	void Aim(CUserCmd* pCmd, Vec3& vAngle, int iMethod = Vars::Aimbot::General::AimType.Value);

	bool FindNearestBuildPoint(CBaseObject* pBuilding, CTFPlayer* pLocal, Vec3& vPoint);
	bool RunSapper(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd);

	Vec3 m_vEyePos = {};
	float m_flRange = 0.f;
	bool m_bShouldSwing = false;

	int m_iDoubletapTicks = 0;
	bool m_bShouldSwing = false;

	void ClearLegitAimStepVars();

	// Smooth aim state variables
	float m_flCurAimTime = 0.0f;
	float m_flLegitAimStepIncTimeOverShoot = 0.0f;
	bool m_bReachedLegitAimStepTarget = false;
	bool m_bInitializedLegitAimStepDirection = false;
	AimDirection m_LegitAimStartDirection = LEFT;
	Vec3 m_vLegitAimStepInitialDelta = {};
	int m_nLegitAimCurveType = 0;

	std::unordered_map<int, std::deque<TickRecord>> m_mRecordMap;
	std::unordered_map<int, std::vector<Vec3>> m_mPaths;

public:
	void Run(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd);
};

ADD_FEATURE(CAimbotMelee, AimbotMelee);