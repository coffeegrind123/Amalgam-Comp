# ESP Bones Crash Fix

**Date:** 2025-12-28
**Issue:** Enabling bones in ESP menu crashes instantly

## Root Cause

The ESP cache system was storing entity pointers that could become stale:

1. `Store()` runs and populates `m_mPlayerCache` with entity pointers
2. Before `Draw()` runs, entities disconnect
3. `Draw()` iterates over cache with stale entity pointers
4. **CRASH** when calling `SetupBones()` or accessing members

## Fix Applied

### 1. Cache Clearing Moved to Store()
```cpp
void CESP::Store(CTFPlayer* pLocal)
{
    // Clear stale entity pointers from previous frame
    m_mPlayerCache.clear();
    m_mBuildingCache.clear();
    m_mWorldCache.clear();
    
    // Populate fresh cache...
}
```

### 2. Entity Validation in All Draw Functions
Added validation at the START of each cache iteration:

**DrawPlayers:**
```cpp
for (auto& [pEntity, tCache] : m_mPlayerCache)
{
    // Validate before any access
    if (!pEntity || pEntity->IsDormant())
        continue;
    
    auto pPlayer = pEntity->As<CTFPlayer>();
    if (!pPlayer)
        continue;
    
    // Now safe to use pPlayer for bones...
}
```

**DrawBuildings:**
```cpp
for (auto& [pEntity, tCache] : m_mBuildingCache)
{
    if (!pEntity || pEntity->IsDormant())
        continue;
    // Safe to access...
}
```

**DrawWorld:**
```cpp
for (auto& [pEntity, tCache] : m_mWorldCache)
{
    if (!pEntity || pEntity->IsDormant())
        continue;
    // Safe to access...
}
```

### 3. Bones Setup Safety
- `pPlayer` is validated BEFORE `SetupBones()` call
- Bones are computed fresh every frame (no caching)
- Early `continue` on stale entities prevents crash

## Why This Works

1. **Cache cleared every frame** - No stale pointers carried over
2. **Validation in Draw** - Catches entities that disconnected between Store/Draw
3. **Dormant check** - Skips entities being removed by game
4. **Null pointer check** - Prevents dereferencing invalid memory

## Files Modified

1. `Amalgam/src/Features/Visuals/ESP/ESP.h` - Cache declarations
2. `Amalgam/src/Features/Visuals/ESP/ESP.cpp`:
   - `Store()` - Added cache clearing
   - `DrawPlayers()` - Added entity validation
   - `DrawBuildings()` - Added entity validation
   - `DrawWorld()` - Added entity validation

## Testing

Enable ESP with bones:
- Should NOT crash instantly
- Should handle player disconnects
- Should handle players joining mid-game
- Bones should render correctly on all players
