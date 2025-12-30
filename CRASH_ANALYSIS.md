# Crash Analysis - Casual Games

## Critical Issues Found

### 1. **CRITICAL: Uncached Return in ShouldIgnore** (AimbotGlobal.cpp:174-175)

**Location:** `src/Features/Aimbot/AimbotGlobal/AimbotGlobal.cpp:174-175`

```cpp
if (pLocal->m_iTeamNum() == pEntity->m_iTeamNum())
    return false;  // MISSING: cache.nTick and cache.bShouldIgnore not set!
```

**Impact:**
- Cache inconsistency - teammates bypass the cache every time
- If the entity is removed between calls, `pEntity` could be dangling
- Could cause crash if entity is accessed after being freed

**Fix Required:**
```cpp
if (pLocal->m_iTeamNum() == pEntity->m_iTeamNum())
{
    cache.nTick = nCurrentTick;
    cache.bShouldIgnore = false;
    return false;
}
```

---

### 2. **CRITICAL: Race Condition in GetCachedHitboxList** (AimbotGlobal.cpp:347-410)

**Location:** `src/Features/Aimbot/AimbotGlobal/AimbotGlobal.cpp`

**Problem:**
```cpp
static std::unordered_map<int, std::vector<int>> m_mHitboxCache;
// ...
if (nLastEnabledHitboxes != iHitboxes)
{
    m_mHitboxCache.clear();  // DANGER: Clears cache while other threads may be reading!
```

**Impact:**
- Multiple threads can access `PlayerBoneInFOV()` simultaneously in casual games
- Thread A checks cache, Thread B calls `clear()`, Thread A accesses invalidated iterator = CRASH
- Static map is shared across all entities - no thread safety

**Fix Required:**
- Remove `static` and make it a member variable with proper synchronization
- OR use atomic operations with lock-free pattern
- OR use thread-local storage

---

### 3. **HIGH: Invalid Entity Index Access** (ESP.cpp:928-938)

**Location:** `src/Features/Visuals/ESP/ESP.cpp:928`

**Problem:**
```cpp
int nEntIndex = pPlayer->entindex();  // Could be invalid if player disconnected
auto& cachedBones = m_mBoneCache[nEntIndex];  // Accesses map with potentially stale index
```

**Impact:**
- When player disconnects and another joins with same entity index
- Old cached bones might be used for new player
- `SetupBones` on wrong entity model = CRASH

**Fix Required:**
- Add validation: `if (nEntIndex <= 0 || nEntIndex >= MAX_EDICTS) continue;`
- Clear cache entries on entity disconnect
- Add unique entity ID to cache (not just index)

---

### 4. **HIGH: Frame Count Overflow** (ESP.cpp:933)

**Location:** `src/Features/Visuals/ESP/ESP.cpp:933`

**Problem:**
```cpp
(I::GlobalVars->framecount - cachedBones.flLastUpdate) >= nBoneUpdateFreq
```

**Impact:**
- If `framecount` overflows (happens after ~6 hours at 60fps)
- Arithmetic becomes: `(small_number - large_number)` = huge negative number
- Cache never updates or always updates incorrectly
- Could use invalid bones = CRASH

**Fix Required:**
```cpp
// Use delta with overflow protection
int nFrameDelta = (I::GlobalVars->framecount - cachedBones.flLastUpdate) & 0x7FFFFFFF;
if (nFrameDelta >= nBoneUpdateFreq)
```

---

### 5. **MEDIUM: std::min Bug in Smooth Aim** (AimbotHitscan.cpp:762)

**Location:** `src/Features/Aimbot/AimbotHitscan/AimbotHitscan.cpp:762`

**Problem:**
```cpp
const float flFOVMult = std::min(0.0f, flFOVReciprocal);  // ALWAYS 0.0f!
// flFOVReciprocal = 1.0f / flFOV where flFOV >= 0.001f (always positive)
// std::min(0.0f, positive) always returns 0.0f
```

**Impact:**
- Mathematical error causes incorrect smoothing behavior
- Might cause erratic aimbot movement
- Could lead to anti-cheat detection or gameplay issues

**Original Code Was:**
```cpp
std::min(0.0f, (1.0f / flFOV))  // This was meant to be negative when flFOV < 0
```

**Fix Required:**
```cpp
// Should likely be std::max or use different logic
const float flFOVMult = -flFOVReciprocal;  // Negate for subtraction effect
```

---

## Summary

| Issue | Severity | Commit Introduced | Crash Probability |
|-------|----------|-------------------|-------------------|
| Uncached teammate return | CRITICAL | Latest (Length2D cache) | High in casual |
| Hitbox cache race condition | CRITICAL | 3b48fe2 (Optimizations) | Very High in casual |
| Invalid entity index | HIGH | 3b48fe2 (Optimizations) | Medium |
| Frame count overflow | HIGH | 3b48fe2 (Optimizations) | Low (after 6+ hours) |
| std::min bug | MEDIUM | 3b48fe2 (Optimizations) | Low (logic error) |

## Recommended Fixes Priority

1. **Fix GetCachedHitboxList race condition** - Most likely cause of casual crashes
2. **Fix uncached return in ShouldIgnore** - Can cause dangling pointer access
3. **Add entity index validation** - Prevents stale data access
4. **Fix frame count overflow** - Prevents long-session crashes
5. **Fix std::min bug** - Corrects aimbot behavior

