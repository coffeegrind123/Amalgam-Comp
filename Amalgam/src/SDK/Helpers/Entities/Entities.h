#pragma once
#include "../../../Utils/Macros/Macros.h"
#include "../../Definitions/Classes.h"
<<<<<<< HEAD
#include "../Cache/CacheManager.h"
#include <unordered_map>
#include <vector>
=======
#include "../../Vars.h"
>>>>>>> upstream/master

Enum(Entity, Invalid = -1,
	PlayerAll, PlayerEnemy, PlayerTeam,
	BuildingAll, BuildingEnemy, BuildingTeam,
	PickupHealth, PickupAmmo, PickupMoney, PickupPowerup, PickupSpellbook, PickupGargoyle,
	WorldProjectile, WorldObjective, WorldNPC, WorldBomb,
	LocalStickies, LocalFlares, SniperDots
)

// Entity storage constants for MSVC compatibility
#ifndef MAX_PLAYERS
#define MAX_PLAYERS 64
#endif
#define MAX_BUILDINGS 128
#define MAX_PROJECTILES 256

// Optimized hot-path entity storage
class COptimizedEntityStorage
{
private:
    // Optimized vectors for hot groups (O(1) access, better cache locality)

    std::vector<CBaseEntity*> m_vPlayersAll;
    std::vector<CBaseEntity*> m_vPlayersEnemies;
    std::vector<CBaseEntity*> m_vPlayersTeammates;
    std::vector<CBaseEntity*> m_vBuildingsEnemies;
    std::vector<CBaseEntity*> m_vWorldProjectiles;

    // Fallback to unordered_map for less frequently accessed groups
    std::unordered_map<EGroupType, std::vector<CBaseEntity*>> m_mFallbackGroups;

    // Cache optimization
    bool m_bGroupsDirty = true;
    uint32_t m_nLastUpdateTick = 0;

public:
    COptimizedEntityStorage()
    {
        // Pre-allocate memory for hot vectors to avoid reallocations
        m_vPlayersAll.reserve(MAX_PLAYERS);
        m_vPlayersEnemies.reserve(MAX_PLAYERS);
        m_vPlayersTeammates.reserve(MAX_PLAYERS);
        m_vBuildingsEnemies.reserve(MAX_BUILDINGS);
        m_vWorldProjectiles.reserve(MAX_PROJECTILES);
    }

    // Fast access for hot groups
    const std::vector<CBaseEntity*>& GetPlayersAll() const { return m_vPlayersAll; }
    const std::vector<CBaseEntity*>& GetPlayersEnemies() const { return m_vPlayersEnemies; }
    const std::vector<CBaseEntity*>& GetPlayersTeammates() const { return m_vPlayersTeammates; }
    const std::vector<CBaseEntity*>& GetBuildingsEnemies() const { return m_vBuildingsEnemies; }
    const std::vector<CBaseEntity*>& GetWorldProjectiles() const { return m_vWorldProjectiles; }

    // Clear all optimized vectors (fast operation)
    void ClearOptimizedGroups()
    {
        m_vPlayersAll.clear();
        m_vPlayersEnemies.clear();
        m_vPlayersTeammates.clear();
        m_vBuildingsEnemies.clear();
        m_vWorldProjectiles.clear();
        m_bGroupsDirty = true;
    }

    // Add entity to optimized storage
    void AddEntity(CBaseEntity* pEntity, EGroupType group)
    {
        switch (group)
        {
        case EGroupType::PLAYERS_ALL:
            m_vPlayersAll.push_back(pEntity);
            break;
        case EGroupType::PLAYERS_ENEMIES:
            m_vPlayersEnemies.push_back(pEntity);
            break;
        case EGroupType::PLAYERS_TEAMMATES:
            m_vPlayersTeammates.push_back(pEntity);
            break;
        case EGroupType::BUILDINGS_ENEMIES:
            m_vBuildingsEnemies.push_back(pEntity);
            break;
        case EGroupType::WORLD_PROJECTILES:
            m_vWorldProjectiles.push_back(pEntity);
            break;
        default:
            // Use fallback for less common groups
            m_mFallbackGroups[group].push_back(pEntity);
            break;
        }
        m_bGroupsDirty = true;
    }

    // Get group (optimized for hot groups, fallback for others)
    const std::vector<CBaseEntity*>& GetGroup(EGroupType group)
    {
        switch (group)
        {
        case EGroupType::PLAYERS_ALL:
            return m_vPlayersAll;
        case EGroupType::PLAYERS_ENEMIES:
            return m_vPlayersEnemies;
        case EGroupType::PLAYERS_TEAMMATES:
            return m_vPlayersTeammates;
        case EGroupType::BUILDINGS_ENEMIES:
            return m_vBuildingsEnemies;
        case EGroupType::WORLD_PROJECTILES:
            return m_vWorldProjectiles;
        default:
            return m_mFallbackGroups[group];
        }
    }

    // Memory optimization - shrink vectors if significantly oversized
    void OptimizeMemory()
    {
        m_vPlayersAll.shrink_to_fit();
        m_vPlayersEnemies.shrink_to_fit();
        m_vPlayersTeammates.shrink_to_fit();
        m_vBuildingsEnemies.shrink_to_fit();
        m_vWorldProjectiles.shrink_to_fit();
    }

    bool IsDirty() const { return m_bGroupsDirty; }
    void MarkClean() { m_bGroupsDirty = false; }
    uint32_t GetLastUpdateTick() const { return m_nLastUpdateTick; }
    void SetUpdateTick(uint32_t tick) { m_nLastUpdateTick = tick; }
};

struct DormantData
{
	Vec3 Location;
	float LastUpdate = 0.f;
};

struct VelFixRecord
{
	Vec3 m_vecOrigin;
	float m_flSimulationTime;
};

class CEntities
{
private:
	bool ManageDormancy(CBaseEntity* pEntity);

	CTFPlayer* m_pLocal = nullptr;
	CTFWeaponBase* m_pLocalWeapon = nullptr;
	CTFPlayerResource* m_pPlayerResource = nullptr;

<<<<<<< HEAD
	// Legacy group system (kept for compatibility)
	std::unordered_map<EGroupType, std::vector<CBaseEntity*>> m_mGroups = {};

	// Optimized entity storage for hot paths
	COptimizedEntityStorage m_OptimizedStorage;

	// Performance metrics
	uint32_t m_nLastStoreTick = 0;
	mutable uint32_t m_nOptimizationHits = 0;
	mutable uint32_t m_nOptimizationMisses = 0;

	std::unordered_map<int, float> m_mSimTimes = {}, m_mOldSimTimes = {}, m_mDeltaTimes = {}, m_mLagTimes = {};
=======
	std::unordered_map<EntityEnum::EntityEnum, std::vector<CBaseEntity*>> m_mGroups = {};

	std::unordered_map<int, float> m_mDeltaTimes = {}, m_mLagTimes = {};
>>>>>>> upstream/master
	std::unordered_map<int, int> m_mChokes = {}, m_mSetTicks = {};
	std::unordered_map<int, Vec3> m_mOldAngles = {}, m_mEyeAngles = {};
	std::unordered_map<int, bool> m_mLagCompensation = {};
	std::unordered_map<int, DormantData> m_mDormancy = {};
	std::unordered_map<int, Vec3> m_mAvgVelocities = {};
	std::unordered_map<int, uint32_t> m_mModels = {};
	std::unordered_map<int, std::deque<VelFixRecord>> m_mOrigins = {};

	std::unordered_map<int, int> m_mIPriorities = {};
	std::unordered_map<uint32_t, int> m_mUPriorities = {};
	std::unordered_map<int, bool> m_mIFriends = {};
	std::unordered_map<uint32_t, bool> m_mUFriends = {};
	std::unordered_map<int, int> m_mIParty = {};
	std::unordered_map<uint32_t, int> m_mUParty = {};
	std::unordered_map<int, bool> m_mIF2P = {};
	std::unordered_map<uint32_t, bool> m_mUF2P = {};
	std::unordered_map<int, int> m_mILevels = {};
	std::unordered_map<uint32_t, int> m_mULevels = {};
	uint32_t m_uAccountID;
	int m_iPartyCount = 0;

public:
	void Store();
	void Clear(bool bShutdown = false);
	void ManualNetwork(const StartSoundParams_t& params);

	bool IsHealth(uint32_t uHash);
	bool IsAmmo(uint32_t uHash);
	bool IsPowerup(uint32_t uHash);
	bool IsSpellbook(uint32_t uHash);

	CTFPlayer* GetLocal();
	CTFWeaponBase* GetWeapon();
	CTFPlayerResource* GetResource();

	const std::vector<CBaseEntity*>& GetGroup(const EntityEnum::EntityEnum iGroup);

<<<<<<< HEAD
	// Optimized hot-path access methods (use these for performance)
	const std::vector<CBaseEntity*>& GetPlayersAll() const;
	const std::vector<CBaseEntity*>& GetPlayersEnemies() const;
	const std::vector<CBaseEntity*>& GetPlayersTeammates() const;
	const std::vector<CBaseEntity*>& GetBuildingsEnemies() const;
	const std::vector<CBaseEntity*>& GetWorldProjectiles() const;

	// Performance metrics
	void GetOptimizationStats(uint32_t& hits, uint32_t& misses) const;
	float GetOptimizationHitRate() const;

	float GetSimTime(int iIndex);
	float GetOldSimTime(int iIndex);
=======
>>>>>>> upstream/master
	float GetDeltaTime(int iIndex);
	float GetLagTime(int iIndex);
	int GetChoke(int iIndex);
	Vec3 GetEyeAngles(int iIndex);
	Vec3 GetDeltaAngles(int iIndex);
	bool GetLagCompensation(int iIndex);
	void SetLagCompensation(int iIndex, bool bLagComp);
	bool GetDormancy(int iIndex);
	Vec3* GetAvgVelocity(int iIndex);
	void SetAvgVelocity(int iIndex, Vec3 vAvgVelocity);
	uint32_t GetModel(int iIndex);
	std::deque<VelFixRecord>* GetOrigins(int iIndex);

	int GetPriority(int iIndex);
	int GetPriority(uint32_t uAccountID);
	bool IsFriend(int iIndex);
	bool IsFriend(uint32_t uAccountID);
	bool InParty(int iIndex);
	bool InParty(uint32_t uAccountID);
	bool IsF2P(int iIndex);
	bool IsF2P(uint32_t uAccountID);
	int GetLevel(int iIndex);
	int GetLevel(uint32_t uAccountID);
	int GetParty(int iIndex);
	int GetParty(uint32_t uAccountID);
	int GetPartyCount();
};

ADD_FEATURE_CUSTOM(CEntities, Entities, H);