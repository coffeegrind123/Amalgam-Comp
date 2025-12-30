# Cache Removal Summary

**Date:** 2025-12-28
**Commit:** 3b48fe2 (Apply comprehensive performance optimizations)

## Caches Removed

### 1. Hitbox Scan Cache (AimbotGlobal.cpp)
**What was removed:**
- `static std::unordered_map<int, std::vector<int>> m_mHitboxCache`
- `static int nLastEnabledHitboxes`
- Cache invalidation logic (`m_mHitboxCache.clear()`)

**What replaced it:**
- Direct hitbox list building each call
- Still uses `reserve(18)` for performance
- No static state = thread-safe

**Location:** `Amalgam/src/Features/Aimbot/AimbotGlobal/AimbotGlobal.cpp:351-398`

**Why this fixes crashes:**
The static unordered_map was being accessed from multiple threads when processing multiple entities simultaneously. When one thread called `clear()` while another was iterating, it caused iterator invalidation = CRASH.

### 2. ESP Bone Cache (ESP.cpp/ESP.h)
**What was removed:**
- `struct CachedBones` from ESP.h
- `std::unordered_map<int, CachedBones> m_mBoneCache` from ESP.h
- Bone update frequency logic (`nBoneUpdateFreq = 8`)
- Cached bone lookup and validation

**What replaced it:**
- Direct `SetupBones()` call every frame
- Local stack-allocated `matrix3x4 aBones[MAXSTUDIOBONES]`
- No per-frame state persistence

**Location:** 
- ESP.h:58-64 (removed)
- ESP.cpp:922-952 (replaced)

**Why this fixes crashes:**
The bone cache was keyed by entity index, which can be reused when players disconnect and new players join. This caused:
1. Using old bone data for new players (wrong skeleton)
2. Accessing invalid memory pointers from disconnected players
3. Frame count overflow issues after long sessions

## Performance Impact

### Hitbox Cache
- **Before:** ~70% fewer iterations (cached)
- **After:** Full scan every time
- **Impact:** Minimal - building a vector of 18 integers is extremely fast

### ESP Bone Cache
- **Before:** SetupBones every 8 frames per entity
- **After:** SetupBones every frame per entity
- **Impact:** Higher CPU usage during ESP rendering
- **Mitigation:** ESP is already throttled (updates every 2 frames)

## Testing Recommendations

1. **Casual servers (24 players)** - Main crash scenario
   - Join full casual server
   - Enable ESP with bones
   - Wait for players to disconnect/join
   - Should not crash

2. **Long sessions** - Frame overflow test
   - Play for 2+ hours
   - Monitor for crashes
   - Previously crashed at frame count overflow

3. **Rapid player changes**
   - Find server with high player turnover
   - Enable aimbot + ESP
   - Should handle disconnects cleanly

## Files Modified

1. `Amalgam/src/Features/Aimbot/AimbotGlobal/AimbotGlobal.cpp`
   - Removed static cache from `GetCachedHitboxList()`

2. `Amalgam/src/Features/Visuals/ESP/ESP.h`
   - Removed `CachedBones` struct
   - Removed `m_mBoneCache` member

3. `Amalgam/src/Features/Visuals/ESP/ESP.cpp`
   - Replaced cached bone logic with direct SetupBones call

## Other Caches (NOT Removed)

These caches are original to the codebase and are NOT from the optimization commits:
- `m_mPlayerCache` - Per-frame ESP data for players
- `m_mBuildingCache` - Per-frame ESP data for buildings  
- `m_mWorldCache` - Per-frame ESP data for world objects

These are cleared and rebuilt every frame in the `Store()` functions, so they don't have the same crash risk.
