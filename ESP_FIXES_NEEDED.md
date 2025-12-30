# ESP Compilation Fixes Needed

**Date:** 2025-12-28
**Status:** Ready to apply

## Issues Found

1. **Missing cache clearing** - Store() needs to clear caches
2. **Unsafe As<> casts** - Multiple locations need null checks
3. **Missing entity validation** - Store functions need validation

## Fixes to Apply

### 1. Store() - Add cache clearing (Line 12-14)
After function opening, add:
```cpp
// CRASH FIX: Clear stale entity pointers from previous frame  
m_mPlayerCache.clear();
m_mBuildingCache.clear();
m_mWorldCache.clear();
```

### 2. StoreBuildings - Sentry Wrangler Check (~Line 549)
**Current (CRASH):**
```cpp
if (pBuilding->IsSentrygun() && pBuilding->As<CObjectSentrygun>()->m_bPlayerControlled())
    tCache.m_vText.emplace_back(...);
```

**Fix To:**
```cpp
if (pBuilding->IsSentrygun())
{
    auto pSentry = pBuilding->As<CObjectSentrygun>();
    if (pSentry && pSentry->m_bPlayerControlled())
        tCache.m_vText.emplace_back(...);
}
```

### 3. StoreBuildings - Sentry Ammo Check (~Line 561)
**Current (CRASH):**
```cpp
if (pBuilding->IsSentrygun() && !pBuilding->m_bBuilding())
{
    int iShells, ...;
    pBuilding->As<CObjectSentrygun>()->GetAmmoCount(...);
    ...
}
```

**Fix To:**
```cpp
if (pBuilding->IsSentrygun() && !pBuilding->m_bBuilding())
{
    auto pSentry = pBuilding->As<CObjectSentrygun>();
    if (pSentry)
    {
        int iShells, ...;
        pSentry->GetAmmoCount(...);
        ...
    }
}
```

### 4. StoreObjective - Intel Cast (~Line 763)
Add after `auto pIntel = pEntity->As<CCaptureFlag>();`:
```cpp
if (!pIntel)
    break;
```

### 5. StoreProjectiles - Add validation (Line 603)
After for loop opens, add:
```cpp
if (!pEntity || pEntity->IsDormant())
    continue;
```

### 6. StoreObjective - Add validation (Line 723)
After for loop opens, add:
```cpp
if (!pEntity || pEntity->IsDormant())
    continue;
```

### 7. StoreWorld loops - Add validation (Lines 779, 799, 809, 819, 829, 859, 869, 879, 889)
Each loop needs after the opening:
```cpp
if (!pEntity || pEntity->IsDormant())
    continue;
```

### 8. DrawPlayers - Add validation (Line 906)
After `for (auto& [pEntity, tCache] : m_mPlayerCache)`:
```cpp
// CRASH FIX: Validate cached entity pointer
if (!pEntity || pEntity->IsDormant())
    continue;

auto pPlayer = pEntity->As<CTFPlayer>();
if (!pPlayer)
    continue;
```

### 9. DrawBuildings - Add validation (Line 1057)
### 10. DrawWorld - Add validation (Line 1110)

## Pattern

All entity access must follow:
```cpp
// Before accessing entity:
if (!pEntity || pEntity->IsDormant())
    continue;

// Before casting:
auto pTyped = pEntity->As<SomeType>();
if (!pTyped)
    continue/break;
```

## Status

✅ StorePlayers validation - APPLIED
✅ StoreBuildings validation - APPLIED  
⏳ Sentry casts - NEED TO APPLY
⏳ Other Store functions - NEED TO APPLY
⏳ Draw functions - NEED TO APPLY

## Next Steps

Apply the remaining fixes following the patterns above. This will resolve all compilation errors and crash issues.
