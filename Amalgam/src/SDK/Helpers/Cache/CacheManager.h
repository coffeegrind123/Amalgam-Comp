#pragma once
#include "../../../Utils/Feature/Feature.h"
#include <chrono>
#include <unordered_map>

class CCacheManager
{
private:
    // Use high-resolution timer for precise timing
    using Clock = std::chrono::high_resolution_clock;
    static constexpr float CACHE_CLEAR_INTERVAL = 1.0f; // Clear cache every 1 second like TF2 Linux Internal

    struct CacheInfo {
        float lastClearTime = 0.0f;
        uint64_t cacheHits = 0;
        uint64_t cacheMisses = 0;
        bool needsUpdate = false;
    };

    static CacheInfo s_EntityCache;
    static CacheInfo s_PlayerUtilsCache;
    static CacheInfo s_BacktrackCache;
    static CacheInfo s_MoveSimCache;
    static CacheInfo s_CritHackCache;
    static CacheInfo s_AimbotCache;

    static bool ShouldClearCache(float currentTime, CacheInfo& cacheInfo)
    {
        return (currentTime - cacheInfo.lastClearTime) >= CACHE_CLEAR_INTERVAL;
    }

    static void ClearCacheIfNeeded(CacheInfo& cacheInfo, float currentTime)
    {
        if (ShouldClearCache(currentTime, cacheInfo))
        {
            cacheInfo.lastClearTime = currentTime;
            cacheInfo.cacheHits = 0;
            cacheInfo.cacheMisses = 0;
            cacheInfo.needsUpdate = true;
        }
    }

    static void UpdateCacheStats(CacheInfo& cacheInfo, bool bHit)
    {
        if (bHit)
            cacheInfo.cacheHits++;
        else
            cacheInfo.cacheMisses++;
    }

public:
    // Initialize the cache manager
    static void Initialize()
    {
        float currentTime = static_cast<float>(Clock::now().time_since_epoch().count());
        s_EntityCache.lastClearTime = currentTime;
        s_PlayerUtilsCache.lastClearTime = currentTime;
        s_BacktrackCache.lastClearTime = currentTime;
        s_MoveSimCache.lastClearTime = currentTime;
        s_CritHackCache.lastClearTime = currentTime;
        s_AimbotCache.lastClearTime = currentTime;
    }

    // Check if entity cache should be cleared
    static bool ShouldClearEntityCache()
    {
        return ShouldClearCache(static_cast<float>(Clock::now().time_since_epoch().count()), s_EntityCache);
    }

    static bool ShouldClearPlayerUtilsCache()
    {
        return ShouldClearCache(static_cast<float>(Clock::now().time_since_epoch().count()), s_PlayerUtilsCache);
    }

    static bool ShouldClearBacktrackCache()
    {
        return ShouldClearCache(static_cast<float>(Clock::now().time_since_epoch().count()), s_BacktrackCache);
    }

    static bool ShouldClearMoveSimCache()
    {
        return ShouldClearCache(static_cast<float>(Clock::now().time_since_epoch().count()), s_MoveSimCache);
    }

    static bool ShouldClearCritHackCache()
    {
        return ShouldClearCache(static_cast<float>(Clock::now().time_since_epoch().count()), s_CritHackCache);
    }

    static bool ShouldClearAimbotCache()
    {
        return ShouldClearCache(static_cast<float>(Clock::now().time_since_epoch().count()), s_AimbotCache);
    }

    // Clear entity cache and update stats
    static void ClearEntityCache()
    {
        float currentTime = static_cast<float>(Clock::now().time_since_epoch().count());
        ClearCacheIfNeeded(s_EntityCache, currentTime);
    }

    static void ClearPlayerUtilsCache()
    {
        float currentTime = static_cast<float>(Clock::now().time_since_epoch().count());
        ClearCacheIfNeeded(s_PlayerUtilsCache, currentTime);
    }

    static void ClearBacktrackCache()
    {
        float currentTime = static_cast<float>(Clock::now().time_since_epoch().count());
        ClearCacheIfNeeded(s_BacktrackCache, currentTime);
    }

    static void ClearMoveSimCache()
    {
        float currentTime = static_cast<float>(Clock::now().time_since_epoch().count());
        ClearCacheIfNeeded(s_MoveSimCache, currentTime);
    }

    static void ClearCritHackCache()
    {
        float currentTime = static_cast<float>(Clock::now().time_since_epoch().count());
        ClearCacheIfNeeded(s_CritHackCache, currentTime);
    }

    static void ClearAimbotCache()
    {
        float currentTime = static_cast<float>(Clock::now().time_since_epoch().count());
        ClearCacheIfNeeded(s_AimbotCache, currentTime);
    }

    // Update cache statistics
    static void UpdateEntityCacheStats(bool bHit)
    {
        UpdateCacheStats(s_EntityCache, bHit);
    }

    static void UpdatePlayerUtilsCacheStats(bool bHit)
    {
        UpdateCacheStats(s_PlayerUtilsCache, bHit);
    }

    static void UpdateBacktrackCacheStats(bool bHit)
    {
        UpdateCacheStats(s_BacktrackCache, bHit);
    }

    static void UpdateMoveSimCacheStats(bool bHit)
    {
        UpdateCacheStats(s_MoveSimCache, bHit);
    }

    static void UpdateCritHackCacheStats(bool bHit)
    {
        UpdateCacheStats(s_CritHackCache, bHit);
    }

    static void UpdateAimbotCacheStats(bool bHit)
    {
        UpdateCacheStats(s_AimbotCache, bHit);
    }

    // Get cache statistics for debugging
    static void GetEntityCacheStats(uint64_t& nHits, uint64_t& nMisses, float& nHitRate)
    {
        uint64_t nTotal = s_EntityCache.cacheHits + s_EntityCache.cacheMisses;
        nHits = s_EntityCache.cacheHits;
        nMisses = s_EntityCache.cacheMisses;
        nHitRate = nTotal > 0 ? (static_cast<float>(nHits) / static_cast<float>(nTotal)) : 0.0f;
    }

    static void GetPlayerUtilsCacheStats(uint64_t& nHits, uint64_t& nMisses, float& nHitRate)
    {
        uint64_t nTotal = s_PlayerUtilsCache.cacheHits + s_PlayerUtilsCache.cacheMisses;
        nHits = s_PlayerUtilsCache.cacheHits;
        nMisses = s_PlayerUtilsCache.cacheMisses;
        nHitRate = nTotal > 0 ? (static_cast<float>(nHits) / static_cast<float>(nTotal)) : 0.0f;
    }

    static void GetBacktrackCacheStats(uint64_t& nHits, uint64_t& nMisses, float& nHitRate)
    {
        uint64_t nTotal = s_BacktrackCache.cacheHits + s_BacktrackCache.cacheMisses;
        nHits = s_BacktrackCache.cacheHits;
        nMisses = s_BacktrackCache.cacheMisses;
        nHitRate = nTotal > 0 ? (static_cast<float>(nHits) / static_cast<float>(nTotal)) : 0.0f;
    }

    static void GetMoveSimCacheStats(uint64_t& nHits, uint64_t nMisses, float& nHitRate)
    {
        uint64_t nTotal = s_MoveSimCache.cacheHits + s_MoveSimCache.cacheMisses;
        nHits = s_MoveSimCache.cacheHits;
        nMisses = s_MoveSimCache.cacheMisses;
        nHitRate = nTotal > 0 ? (static_cast<float>(nHits) / static_cast<float>(nTotal)) : 0.0f;
    }

    static void GetCritHackCacheStats(uint64_t& nHits, uint64_t& nMisses, float& nHitRate)
    {
        uint64_t nTotal = s_CritHackCache.cacheHits + s_CritHackCache.cacheMisses;
        nHits = s_CritHackCache.cacheHits;
        nMisses = s_CritHackCache.cacheMisses;
        nHitRate = nTotal > 0 ? (static_cast<float>(nHits) / static_cast<float>(nTotal)) : 0.0f;
    }

    static void GetAimbotCacheStats(uint64_t& nHits, uint64_t& nMisses, float& nHitRate)
    {
        uint64_t nTotal = s_AimbotCache.cacheHits + s_AimbotCache.cacheMisses;
        nHits = s_AimbotCache.cacheHits;
        nMisses = s_AimbotCache.cacheMisses;
        nHitRate = nTotal > 0 ? (static_cast<float>(nHits) / static_cast<float>(nTotal)) : 0.0f;
    }

    // Clear all caches at once
    static void ClearAllCaches()
    {
        float currentTime = static_cast<float>(Clock::now().time_since_epoch().count());

        ClearEntityCache();
        ClearPlayerUtilsCache();
        ClearBacktrackCache();
        ClearMoveSimCache();
        ClearCritHackCache();
        ClearAimbotCache();
    }

    // Get global cache statistics
    static void GetAllCacheStats(uint64_t& nTotalHits, uint64_t& nTotalMisses, float& nHitRate)
    {
        nTotalHits = s_EntityCache.cacheHits + s_PlayerUtilsCache.cacheHits + s_BacktrackCache.cacheHits +
                    s_MoveSimCache.cacheHits + s_CritHackCache.cacheHits + s_AimbotCache.cacheHits;
        nTotalMisses = s_EntityCache.cacheMisses + s_PlayerUtilsCache.cacheMisses + s_BacktrackCache.cacheMisses +
                       s_MoveSimCache.cacheMisses + s_CritHackCache.cacheMisses + s_AimbotCache.cacheMisses;
        uint64_t nTotal = nTotalHits + nTotalMisses;
        nHitRate = nTotal > 0 ? (static_cast<float>(nTotalHits) / static_cast<float>(nTotal)) : 0.0f;
    }

    // Check if any caches need clearing
    static bool NeedsCacheClear()
    {
        float currentTime = static_cast<float>(Clock::now().time_since_epoch().count());
        return ShouldClearEntityCache() || ShouldClearPlayerUtilsCache() ||
               ShouldClearBacktrackCache() || ShouldClearMoveSimCache() ||
               ShouldClearCritHackCache() || ShouldClearAimbotCache();
    }
};

// RAII wrapper for automatic cache clearing timing
class CCacheTimer
{
private:
    float m_fStartTime;

public:
    CCacheTimer() : m_fStartTime(static_cast<float>(std::chrono::high_resolution_clock::now().time_since_epoch().count()))
    {
        CCacheManager::Initialize();
    }

    bool ShouldClear()
    {
        return CCacheManager::NeedsCacheClear();
    }

    float GetElapsedTime() const
    {
        return static_cast<float>(std::chrono::high_resolution_clock::now().time_since_epoch().count()) - m_fStartTime;
    }

    void ResetTimer()
    {
        m_fStartTime = static_cast<float>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    }
};

// Static member definitions
CCacheManager::CacheInfo CCacheManager::s_EntityCache;
CCacheManager::CacheInfo CCacheManager::s_PlayerUtilsCache;
CCacheManager::CacheInfo CCacheManager::s_BacktrackCache;
CCacheManager::CacheInfo CCacheManager::s_MoveSimCache;
CCacheManager::CacheInfo CCacheManager::s_CritHackCache;
CCacheManager::CacheInfo CCacheManager::s_AimbotCache;