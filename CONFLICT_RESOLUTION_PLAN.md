# Conflict Resolution Plan for Amalgam-Comp Fork

## Overview
This document outlines the strategy for resolving 98 merge conflicts between our competitive-focused fork and upstream Amalgam. The goal is to preserve our competitive features while integrating upstream improvements.

## Key Principles
1. **Preserve competitive features** - Our fork's value is in competitive enhancements
2. **Integrate game compatibility fixes** - Upstream updates for latest game versions are critical
3. **Adopt improved architecture where beneficial** - Upstream's Groups system may be better
4. **Maintain safety and performance** - Keep our error handling and optimizations

## Phase 1: Assessment & Backup

### 1.1 Create Backup
```bash
# Create backup of current state
cp -r /tmp/Amalgam-Comp /tmp/Amalgam-Comp-backup-$(date +%Y%m%d_%H%M%S)
```

### 1.2 Identify Critical Conflicts
Based on analysis, prioritize:
1. `Core.cpp` - Core initialization
2. `Amalgam.vcxproj` - Build configuration
3. `ESP.cpp` - Visuals architecture
4. Game update fixes (patch_07c7ab2.patch)

## Phase 2: Core Architecture Resolution

### 2.1 Core.cpp Strategy
**Our enhancements to preserve:**
- File-based logging for debugging
- Safe access handler initialization
- SIMD math optimizations
- Enhanced error handling with try-catch blocks

**Upstream changes to integrate:**
- Structural initialization improvements
- Any bug fixes in core loading logic

**Approach:** Create hybrid initialization that:
1. Starts with our enhanced logging and safe access
2. Integrates upstream structural improvements
3. Preserves SIMD optimizations
4. Maintains comprehensive error handling

### 2.2 DllMain.cpp
Check for entry point changes. Likely keep our version with upstream fixes if any.

## Phase 3: Build Configuration

### 3.1 Amalgam.vcxproj Strategy
**Our configuration to preserve:**
- `ReleaseFreetypeAVX2` configuration (competitive optimization)
- AVX2 and Freetype support
- x64 platform focus

**Upstream changes to consider:**
- Removal of WIN32 definitions
- Streamlined configurations
- Any necessary compiler flags for compatibility

**Approach:** Merge project file manually, keeping our optimized configurations while integrating upstream structural improvements.

## Phase 4: Visuals System Integration

### 4.1 Groups Architecture Adoption
Upstream introduced a Groups system for ESP. We should:
1. **Analyze Groups.cpp/h** - Understand the architecture
2. **Map our competitive features to Groups** - Each feature becomes a group
3. **Preserve feature logic** - Keep our implementation details
4. **Adapt to Groups API** - Use group registration/toggling

### 4.2 ESP.cpp Resolution
**Our competitive features:**
- UberTracker, MedicUberBar, HealthBarESP
- PlayerTrails, SentryESP, StickyESP
- FocusFire, PylonESP, AmmoTracker
- MarkSpot, SplashRadius, CraterCheck
- EnemyCam, StickyCam, OffScreenIndicators
- SpectateAll, ScoreboardRevealer, FlatTextures
- Match HUD Enhancement, Safe Bunnyhop
- Disable Freezecam, No Hats, Hider ESP

**Approach:**
1. Keep our feature implementations intact
2. Adapt to use Groups system if beneficial
3. Ensure all toggles remain in COMP tab
4. Test each feature after integration

### 4.3 Glow.cpp and Materials.cpp
Merge safety improvements (try-catch, null checks) with upstream rendering improvements.

## Phase 5: Aimbot System

### 5.1 Strategy
**Our enhancements:** Competitive optimizations
**Upstream changes:** Bug fixes and algorithm improvements

**Approach:** 
1. Apply upstream bug fixes (patch_07c7ab2.patch)
2. Preserve our competitive logic
3. Test thoroughly for regressions

## Phase 6: SDK & Hooks

### 6.1 New Hooks Integration
Upstream added hooks:
- `CBasePlayer_CalcObserverView.cpp`
- `CBasePlayer_CalcView.cpp` 
- `CHLTVCamera_CalcView.cpp`
- `CInput_ValidateUserCmd.cpp`
- `CTFPlayerShared_ShouldSuppressPrediction.cpp`

**Approach:** Add these hooks while preserving our namespace structure (`U::` → `H::`).

### 6.2 SDK Definitions
Update SDK definitions from upstream for game compatibility.

## Phase 7: Matrix Chat Integration

### 7.1 Preservation Strategy
Matrix integration is unique to our fork. Must:
1. Keep all Matrix-related files intact
2. Ensure no conflicts with upstream changes
3. Test chat functionality after merge

## Phase 8: Testing Protocol

### 8.1 Build Testing
After each phase:
```bash
# Test compilation
msbuild Amalgam.sln /p:Configuration=ReleaseFreetypeAVX2 /p:Platform=x64
```

### 8.2 Feature Validation
Create checklist of competitive features to verify:
- [ ] UberTracker
- [ ] MedicUberBar
- [ ] HealthBarESP
- [ ] PlayerTrails
- [ ] SentryESP
- [ ] StickyESP
- [ ] FocusFire
- [ ] PylonESP
- [ ] AmmoTracker
- [ ] MarkSpot (Matrix integration)
- [ ] All other competitive features

### 8.3 Game Compatibility
Test with current TF2 version to ensure upstream fixes work.

## Phase 9: Documentation Update

### 9.1 Update README
Reflect merged feature set and any changes to configuration.

### 9.2 Update Configuration Documentation
Document Groups system if adopted.

## Risk Mitigation

### High Risk Areas:
1. **Matrix integration** - Unique to fork, test thoroughly
2. **Competitive feature compatibility with Groups** - May require adaptation
3. **Build configuration conflicts** - Could break compilation

### Mitigation:
1. **Incremental merging** - One category at a time
2. **Frequent backups** - Rollback points
3. **Feature testing** - Validate each competitive feature
4. **Fallback branches** - Maintain working versions

## Timeline

### Week 1: Core & Build
- Resolve Core.cpp and project file conflicts
- Ensure compilation works

### Week 2: Visuals Architecture
- Integrate Groups system
- Adapt competitive features
- Test visual features

### Week 3: Aimbot & Gameplay
- Apply upstream fixes
- Test aimbot functionality

### Week 4: SDK, Hooks & Final Testing
- Update SDK definitions
- Add new hooks
- Comprehensive testing

## Success Criteria
1. All competitive features functional
2. Game compatibility fixes integrated
3. Code compiles without errors
4. No performance regressions
5. Matrix chat works
6. Groups system adopted (if beneficial)

## Rollback Plan
If critical issues arise:
1. Revert to backup
2. Maintain separate upstream-tracking branch
3. Consider feature-by-feature rebase instead of full merge