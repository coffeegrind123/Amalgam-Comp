#pragma once
#include "../../../SDK/SDK.h"

#ifdef NO_COLLISION
#pragma push_macro("NO_COLLISION")
#undef NO_COLLISION
#endif

enum class ProjectileType
{
	BASIC,
	PSEUDO,
	SIMUL
};

enum class CollisionType
{
	NORMAL,
	HEAL_TEAMMATES,
	HEAL_BUILDINGS,
	HEAL_HURT,
	NO_COLLISION
};

#ifdef NO_COLLISION
#pragma pop_macro("NO_COLLISION")
#endif

struct ProjectileInfo
{
	ProjectileType m_iType = ProjectileType::BASIC;
	Vec3 m_vecOffset = Vec3(0, 0, 0);
	Vec3 m_vecAbsoluteOffset = Vec3(0, 0, 0);
	Vec3 m_vecAngleOffset = Vec3(0, 0, 0);
	Vec3 m_vecVelocity = Vec3(0, 0, 0);
	Vec3 m_vecAngularVelocity = Vec3(0, 0, 0);
	Vec3 m_vecMins = Vec3(0, 0, 0);
	Vec3 m_vecMaxs = Vec3(0, 0, 0);
	float m_flGravity = 0.001f;
	float m_flDrag = 0.0f;
	float m_flElasticity = 0.0f;
	int m_iAlignDistance = 0;
	CollisionType m_iCollisionType = CollisionType::NORMAL;
	float m_flCollideWithTeammatesDelay = 0.25f;
	float m_flLifetime = 99999.0f;
	float m_flDamageRadius = 0.0f;
	bool m_bStopOnHittingEnemy = true;
	bool m_bCharges = false;
	bool m_bHasGravity = true;

	Vec3 GetOffset(bool bDucking, bool bIsFlipped) const;
	Vec3 GetAngleOffset(float flChargeBeginTime) const;
	Vec3 GetVelocity(float flChargeTime) const;
	Vec3 GetAngularVelocity(float flChargeBeginTime) const;
	float GetGravity(float flChargeTime) const;
	float GetLifetime(float flChargeBeginTime) const;
	bool HasGravity() const { return m_bHasGravity; }
	float GetChargeTime(class CTFWeaponBase* pWeapon) const;
};

namespace ProjWeaponInfo
{
	const ProjectileInfo* GetProjectileInfo(int itemDefinitionIndex);
	void Initialize();
}
