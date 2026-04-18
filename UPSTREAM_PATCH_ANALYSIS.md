# Upstream Patch Analysis for Game Compatibility

## Patch Status Summary

### 1. `07c7ab2` - "fix for latest game update" (Mar 2026) - MOST CRITICAL
**Status**: Complex conflicts, requires manual implementation
**Key Changes**:
- Changes `ProjSimEnum::Trace` to `ProjSimEnum::Redirect` throughout codebase
- Updates `GetProjectileFireSetup` function signature from `(..., bool bPipes, ...)` to `(..., float flForwardRedirect, float flForwardCutoff, ...)`
- Updates menu options from `Vars::Visuals::Trajectory::Pipes` to `Vars::Visuals::Trajectory::ForwardRedirect` and `ForwardCutoff`
- Removes significant code from `Cbuf_ExecuteCommand.cpp`

**Our Status**: Major conflicts. Our codebase has different structure in many files. This is a breaking API change that affects multiple systems.

### 2. `236439d` - "fix for latest game update" (Dec 2025)
**Status**: Mostly already applied in our fork
**Key Changes**:
- Updates function signatures (CBaseViewModel_ShouldFlipViewModel, GetClientInterpAmount)
- Adds `m_flGravity` as regular NETVAR instead of NETVAR_OFF
- Adds `m_bFlipViewModels` NETVAR to CTFPlayer
- Comments out CTFPlayer_UpdateStepSound hook

**Our Status**: Already have most changes:
- `m_flGravity` is already `NETVAR(m_flGravity, float, "CBaseEntity", "m_flGravity")` at line 63
- No `NETVAR_OFF` line for `m_flGravity` exists (already removed)
- `m_bFlipViewModels` already exists at line 157
- Function signatures already updated to new patterns
- `CTFPlayer_UpdateStepSound.cpp` doesn't exist in our fork

### 3. `36c2899` - "tickbase related changes/fixes" (Jan 2026)
**Status**: Needs review, likely important for game compatibility
**Key Changes**:
- Adds `CTFPlayerShared_ShouldSuppressPrediction.cpp` hook file
- Updates tickbase manipulation logic in multiple files
- Fixes condition checks in Misc.cpp

### 4. `fce4740` - "fix stack trace crashing the game" (Nov 2025)
**Status**: Needs implementation
**Key Changes**:
- Renames `CrashLog` to `ErrorLog`
- Adds current time output in error logs
- Fixes stack trace crash issue

## Recommended Action Plan

### Immediate Priority (Game Compatibility):
1. **Implement the `GetProjectileFireSetup` API change** - This is critical for projectile weapons to work with latest game update
   - Update function signature in SDK.h and SDK.cpp
   - Update all call sites throughout the codebase
   - Update enum from `Trace` to `Redirect`

2. **Add missing hook file** - Create `CTFPlayerShared_ShouldSuppressPrediction.cpp` from patch 36c2899

3. **Rename CrashLog to ErrorLog** - Apply changes from patch fce4740

### Already Applied:
- SDK netvar updates (236439d) - Already in our codebase
- Function signature updates - Already updated

### Lower Priority:
- Menu option renaming (Pipes → ForwardRedirect/ForwardCutoff)
- Minor logic fixes in tickbase code

## Implementation Notes

The projectile simulation changes are the most critical for game compatibility. The game likely changed how projectile firing works, requiring the `bPipes` boolean parameter to be replaced with more precise `flForwardRedirect` and `flForwardCutoff` float parameters.

Our fork appears to be more up-to-date than expected for the SDK changes (236439d), suggesting we may have already backported some compatibility fixes.