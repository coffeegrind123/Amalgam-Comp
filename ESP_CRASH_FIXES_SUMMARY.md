# ESP Crash Fixes - Building Names, Objectives, Ammo

**Date:** 2025-12-28
**Issue:** Enabling building names, objectives, ammo, etc. crashes game

## Root Cause

Unsafe `As<>` casts in ESP `Store` functions without validation:
1. Cast could return `nullptr` if entity type doesn't match
2. Code immediately accessed the cast pointer without null check
3. **CRASH** when dereferencing null pointer

## Fixes Applied

### 1. StorePlayers (Line 46-50)
**Before:**
```cpp
auto pPlayer = pEntity->As<CTFPlayer>();
int iIndex = pPlayer->entindex();  // CRASH if pPlayer is null
```

**After:**
```cpp
if (!pEntity || pEntity->IsDormant())
    continue;
auto pPlayer = pEntity->As<CTFPlayer>();
if (!pPlayer)
    continue;
int iIndex = pPlayer->entindex();  // SAFE
```

### 2. StoreBuildings (Line 473-475)
**Before:**
```cpp
auto pBuilding = pEntity->As<CBaseObject>();
auto pOwner = pBuilding->m_hBuilder().Get();  // CRASH if pBuilding is null
```

**After:**
```cpp
auto pBuilding = pEntity->As<CBaseObject>();
if (!pBuilding)
    continue;
auto pOwner = pBuilding->m_hBuilder().Get();  // SAFE
```

### 3. StoreBuildings - Sentry Casts (Line 545-549, 557-569)
**Before:**
```cpp
if (pBuilding->IsSentrygun() && pBuilding->As<CObjectSentrygun>()->m_bPlayerControlled())
    // CRASH if As<> returns nullptr

pBuilding->As<CObjectSentrygun>()->GetAmmoCount(...);  // CRASH if null
```

**After:**
```cpp
if (pBuilding->IsSentrygun())
{
    auto pSentry = pBuilding->As<CObjectSentrygun>();
    if (pSentry && pSentry->m_bPlayerControlled())
        // SAFE

    if (pSentry)
    {
        pSentry->GetAmmoCount(...);  // SAFE
    }
}
```

### 4. StoreObjective - Intel Cast (Line 763-765)
**Before:**
```cpp
auto pIntel = pEntity->As<CCaptureFlag>();
switch (pIntel->m_nFlagStatus())  // CRASH if pIntel is null
```

**After:**
```cpp
auto pIntel = pEntity->As<CCaptureFlag>();
if (!pIntel)
    break;
switch (pIntel->m_nFlagStatus())  // SAFE
```

## Pattern Applied

**All As<> casts now follow safe pattern:**
```cpp
auto pTyped = pEntity->As<SomeType>();
if (!pTyped)
    continue/break;  // Skip this entity
// Now safe to use pTyped
```

## Files Modified

1. `Amalgam/src/Features/Visuals/ESP/ESP.cpp`
   - Line 46-50: StorePlayers validation
   - Line 471-475: StoreBuildings base validation
   - Line 545-549: Sentry wrangler check validation
   - Line 557-569: Sentry ammo check validation
   - Line 763-765: Objective intel validation

## Testing

Enable all ESP options:
- ✅ Building names - should NOT crash
- ✅ Building flags/level/ammo - should NOT crash
- ✅ Objectives - should NOT crash
- ✅ Player names - should NOT crash
- ✅ World items (health, ammo, etc.) - should NOT crash

All should render correctly without crashing on entity disconnects.
