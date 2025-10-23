#include "ProjWeaponInfo.h"
#include <unordered_map>

Vec3 ProjectileInfo::GetOffset(bool bDucking, bool bIsFlipped) const
{
	if (bIsFlipped)
		return Vec3(m_vecOffset.x, -m_vecOffset.y, m_vecOffset.z);
	return m_vecOffset;
}

Vec3 ProjectileInfo::GetAngleOffset(float flChargeBeginTime) const
{
	return m_vecAngleOffset;
}

Vec3 ProjectileInfo::GetVelocity(float flChargeBeginTime) const
{
	return m_vecVelocity;
}

Vec3 ProjectileInfo::GetAngularVelocity(float flChargeBeginTime) const
{
	return m_vecAngularVelocity;
}

float ProjectileInfo::GetGravity(float flChargeBeginTime) const
{
	return m_flGravity;
}

float ProjectileInfo::GetLifetime(float flChargeBeginTime) const
{
	return m_flLifetime;
}

namespace ProjWeaponInfo
{
	static std::unordered_map<int, const ProjectileInfo*> g_WeaponMap;
	static std::vector<ProjectileInfo> g_ProjectileInfos;

	static void RegisterWeapon(const ProjectileInfo& info, const std::vector<int>& itemDefIndices)
	{
		g_ProjectileInfos.push_back(info);
		const ProjectileInfo* pInfo = &g_ProjectileInfos.back();

		for (int itemDefIndex : itemDefIndices)
			g_WeaponMap[itemDefIndex] = pInfo;
	}

	void Initialize()
	{
		if (!g_ProjectileInfos.empty())
			return;

		ProjectileInfo info;

		info = {};
		info.m_vecVelocity = Vec3(1100, 0, 0);
		info.m_vecMaxs = Vec3(0, 0, 0);
		info.m_iAlignDistance = 2000;
		info.m_flDamageRadius = 146.0f;
		info.m_bHasGravity = false;
		info.m_vecOffset = Vec3(23.5f, 12.0f, -3.0f);
		RegisterWeapon(info, {
			18, 205, 228, 658, 800, 809, 889, 898, 907, 916, 965, 974, 1085,
			15006, 15014, 15028, 15043, 15052, 15057, 15081, 15104, 15105, 15129, 15130, 15150
		});

		info = {};
		info.m_vecVelocity = Vec3(1100, 0, 0);
		info.m_vecMaxs = Vec3(0, 0, 0);
		info.m_iAlignDistance = 2000;
		info.m_flDamageRadius = 146.0f;
		info.m_bHasGravity = false;
		info.m_iCollisionType = CollisionType::NONE;
		info.m_vecOffset = Vec3(23.5f, 12.0f, -3.0f);
		RegisterWeapon(info, {237});

		info = {};
		info.m_vecVelocity = Vec3(1100, 0, 0);
		info.m_vecMaxs = Vec3(0, 0, 0);
		info.m_iAlignDistance = 2000;
		info.m_flDamageRadius = 116.8f;
		info.m_bHasGravity = false;
		info.m_vecOffset = Vec3(23.5f, 12.0f, -3.0f);
		RegisterWeapon(info, {730});

		info = {};
		info.m_vecVelocity = Vec3(1100, 0, 0);
		info.m_vecMaxs = Vec3(0, 0, 0);
		info.m_iAlignDistance = 2000;
		info.m_flDamageRadius = 131.4f;
		info.m_bHasGravity = false;
		info.m_vecOffset = Vec3(23.5f, 12.0f, -3.0f);
		RegisterWeapon(info, {1104});

		info = {};
		info.m_vecVelocity = Vec3(1640, 0, 0);
		info.m_vecMaxs = Vec3(0, 0, 0);
		info.m_iAlignDistance = 2000;
		info.m_flDamageRadius = 44.0f;
		info.m_bHasGravity = false;
		info.m_vecOffset = Vec3(23.5f, 12.0f, -3.0f);
		RegisterWeapon(info, {127});

		info = {};
		info.m_vecVelocity = Vec3(1540, 0, 0);
		info.m_vecMaxs = Vec3(0, 0, 0);
		info.m_iAlignDistance = 2000;
		info.m_flDamageRadius = 146.0f;
		info.m_bHasGravity = false;
		info.m_vecOffset = Vec3(23.5f, 12.0f, -3.0f);
		RegisterWeapon(info, {414});

		info = {};
		info.m_vecVelocity = Vec3(1100, 0, 0);
		info.m_vecMaxs = Vec3(0, 0, 0);
		info.m_iAlignDistance = 2000;
		info.m_flDamageRadius = 146.0f;
		info.m_bHasGravity = false;
		info.m_vecOffset = Vec3(23.5f, 0.0f, -3.0f);
		RegisterWeapon(info, {513});

		info = {};
		info.m_vecVelocity = Vec3(1600, 0, 0);
		info.m_vecMaxs = Vec3(1, 1, 1);
		info.m_bHasGravity = false;
		info.m_vecOffset = Vec3(3.0f, 7.0f, -9.0f);
		RegisterWeapon(info, {1178});

		info = {};
		info.m_vecVelocity = Vec3(1200, 0, 0);
		info.m_vecMaxs = Vec3(0, 0, 0);
		info.m_bHasGravity = false;
		info.m_iCollisionType = CollisionType::NONE;
		info.m_vecOffset = Vec3(23.5f, 12.0f, 8.0f);
		RegisterWeapon(info, {442, 1153});

		info = {};
		info.m_vecVelocity = Vec3(1216, 0, 0);
		info.m_vecMaxs = Vec3(0, 0, 0);
		info.m_flGravity = 0.4f;
		info.m_bHasGravity = true;
		info.m_flDamageRadius = 146.0f;
		info.m_vecOffset = Vec3(16.0f, 6.0f, -8.0f);
		RegisterWeapon(info, {19, 206, 207, 659, 798, 807, 887, 896, 905, 914, 963, 972, 1083, 15006, 15014, 15028, 15043, 15052, 15057, 15081, 15104, 15105, 15129, 15130, 15150});

		info = {};
		info.m_vecVelocity = Vec3(1216, 0, 0);
		info.m_vecMaxs = Vec3(0, 0, 0);
		info.m_flGravity = 0.4f;
		info.m_bHasGravity = true;
		info.m_flDamageRadius = 146.0f;
		info.m_iCollisionType = CollisionType::NONE;
		info.m_vecOffset = Vec3(16.0f, 6.0f, -8.0f);
		RegisterWeapon(info, {308});

		info = {};
		info.m_vecVelocity = Vec3(1350, 0, 0);
		info.m_vecMaxs = Vec3(0, 0, 0);
		info.m_flGravity = 0.5f;
		info.m_bHasGravity = true;
		info.m_flDamageRadius = 146.0f;
		info.m_vecOffset = Vec3(16.0f, 6.0f, -8.0f);
		RegisterWeapon(info, {996});

		info = {};
		info.m_vecVelocity = Vec3(925, 0, 0);
		info.m_vecMaxs = Vec3(4.0f, 4.0f, 4.0f);
		info.m_flGravity = 0.4f;
		info.m_bHasGravity = true;
		info.m_flDamageRadius = 180.0f;
		info.m_flLifetime = 2.f;
		info.m_vecOffset = Vec3(16.0f, 6.0f, -8.0f);
		RegisterWeapon(info, {1101});

		info = {};
		info.m_vecVelocity = Vec3(2700, 0, 0);
		info.m_vecMaxs = Vec3(0, 0, 0);
		info.m_flGravity = 0.5f;
		info.m_bHasGravity = true;
		info.m_flDamageRadius = 0.0f;
		info.m_iCollisionType = CollisionType::HEAL_TEAMMATES;
		info.m_vecOffset = Vec3(16.0f, 6.0f, -8.0f);
		RegisterWeapon(info, {305});

		info = {};
		info.m_vecVelocity = Vec3(3000, 0, 0);
		info.m_vecMaxs = Vec3(0, 0, 0);
		info.m_flGravity = 0.2f;
		info.m_bHasGravity = true;
		info.m_flLifetime = 10.0f;
		info.m_flDamageRadius = 0.0f;
		info.m_bCharges = true;
		info.m_vecOffset = Vec3(23.5f, 8.0f, -3.0f);
		RegisterWeapon(info, {56, 1005, 1092});

		info = {};
		info.m_vecVelocity = Vec3(2000, 0, 0);
		info.m_vecMaxs = Vec3(0, 0, 0);
		info.m_bHasGravity = false;
		info.m_flDamageRadius = 0.0f;
		info.m_vecOffset = Vec3(23.5f, 8.0f, -3.0f);
		RegisterWeapon(info, {56, 1005, 1092});

		info = {};
		info.m_vecVelocity = Vec3(1800, 0, 0);
		info.m_vecMaxs = Vec3(0, 0, 0);
		info.m_bHasGravity = false;
		info.m_flDamageRadius = 0.0f;
		info.m_vecOffset = Vec3(23.5f, 8.0f, -3.0f);
		RegisterWeapon(info, {1100});

		info = {};
		info.m_vecVelocity = Vec3(1875, 0, 0);
		info.m_vecMaxs = Vec3(0, 0, 0);
		info.m_bHasGravity = false;
		info.m_flDamageRadius = 146.0f;
		info.m_vecOffset = Vec3(23.5f, 8.0f, -3.0f);
		RegisterWeapon(info, {40, 1179, 1181});

		info = {};
		info.m_vecVelocity = Vec3(1450, 0, 0);
		info.m_vecMaxs = Vec3(0, 0, 0);
		info.m_bHasGravity = false;
		info.m_flDamageRadius = 0.0f;
		info.m_vecOffset = Vec3(23.5f, 8.0f, -3.0f);
		RegisterWeapon(info, {351});

		info = {};
		info.m_vecVelocity = Vec3(2600, 0, 0);
		info.m_vecMaxs = Vec3(0, 0, 0);
		info.m_bHasGravity = false;
		info.m_flDamageRadius = 0.0f;
		info.m_vecOffset = Vec3(23.5f, 8.0f, -3.0f);
		RegisterWeapon(info, {595});

		info = {};
		info.m_vecVelocity = Vec3(3000, 0, 0);
		info.m_vecMaxs = Vec3(0, 0, 0);
		info.m_bHasGravity = false;
		info.m_flDamageRadius = 0.0f;
		info.m_vecOffset = Vec3(23.5f, 8.0f, -3.0f);
		RegisterWeapon(info, {1179});

		info = {};
		info.m_vecVelocity = Vec3(1990, 0, 0);
		info.m_vecMaxs = Vec3(0, 0, 0);
		info.m_bHasGravity = false;
		info.m_flDamageRadius = 198.0f;
		info.m_vecOffset = Vec3(23.5f, 8.0f, -3.0f);
		RegisterWeapon(info, {41, 1081});

		info = {};
		info.m_vecVelocity = Vec3(1100, 0, 0);
		info.m_vecMaxs = Vec3(0, 0, 0);
		info.m_bHasGravity = false;
		info.m_flDamageRadius = 153.0f;
		info.m_vecOffset = Vec3(23.5f, 8.0f, -3.0f);
		RegisterWeapon(info, {1180});

		info = {};
		info.m_vecVelocity = Vec3(3000, 0, 0);
		info.m_vecMaxs = Vec3(1, 1, 1);
		info.m_bHasGravity = false;
		info.m_flDamageRadius = 110.0f;
		info.m_vecOffset = Vec3(23.5f, 12.0f, -3.0f);
		RegisterWeapon(info, {39, 1086});

		info = {};
		info.m_vecVelocity = Vec3(2000, 0, 0);
		info.m_vecMaxs = Vec3(1, 1, 1);
		info.m_bHasGravity = false;
		info.m_flDamageRadius = 110.0f;
		info.m_vecOffset = Vec3(23.5f, 12.0f, -3.0f);
		RegisterWeapon(info, {740, 1081});

		info = {};
		info.m_vecVelocity = Vec3(1450, 0, 0);
		info.m_vecMaxs = Vec3(1, 1, 1);
		info.m_bHasGravity = false;
		info.m_flDamageRadius = 110.0f;
		info.m_vecOffset = Vec3(23.5f, 12.0f, -3.0f);
		RegisterWeapon(info, {595, 1178});

		info = {};
		info.m_vecVelocity = Vec3(1865, 0, 0);
		info.m_vecMaxs = Vec3(4.5f, 4.5f, 4.5f);
		info.m_flGravity = 0.001f;
		info.m_bHasGravity = false;
		info.m_flDamageRadius = 110.0f;
		info.m_vecOffset = Vec3(23.5f, 12.0f, -3.0f);
		RegisterWeapon(info, {1178});

		info = {};
		info.m_vecVelocity = Vec3(1216, 0, 0);
		info.m_vecMaxs = Vec3(3.5f, 3.5f, 3.5f);
		info.m_flGravity = 0.5f;
		info.m_bHasGravity = true;
		info.m_flDamageRadius = 146.0f;
		info.m_vecOffset = Vec3(16.0f, 6.0f, -8.0f);
		RegisterWeapon(info, {20, 1007, 1151});

		info = {};
		info.m_vecVelocity = Vec3(1216, 0, 0);
		info.m_vecMaxs = Vec3(3.5f, 3.5f, 3.5f);
		info.m_flGravity = 0.5f;
		info.m_bHasGravity = true;
		info.m_flDamageRadius = 146.0f;
		info.m_iCollisionType = CollisionType::NONE;
		info.m_vecOffset = Vec3(16.0f, 6.0f, -8.0f);
		RegisterWeapon(info, {309});
	}

	const ProjectileInfo* GetProjectileInfo(int itemDefinitionIndex)
	{
		Initialize();

		auto it = g_WeaponMap.find(itemDefinitionIndex);
		if (it != g_WeaponMap.end())
			return it->second;

		return nullptr;
	}
}
