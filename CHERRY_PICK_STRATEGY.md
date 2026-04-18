# Cherry-Pick Strategy for Amalgam-Comp Fork

## Executive Summary

This repository is a competitive-focused fork of Amalgam with 367 modified files. Most upstream commits (147/150) touch files we've modified, making automatic cherry-picking challenging. This document outlines a strategy to selectively integrate upstream fixes while preserving our competitive features.

## Repository Analysis

### Current State
- **Modified files**: 367 files changed vs upstream
- **Upstream commits**: ~150 recent commits
- **Safe commits**: Only 3 commits don't touch our modified files
- **Competitive features**: Added extensive competitive systems (UberTracker, MedicUberBar, HealthBarESP, etc.)
- **Matrix integration**: Enhanced chat system with E2E encryption

### Key Modified Areas
1. **Core systems**: Core.cpp, DllMain.cpp, BytePatches
2. **Aimbot systems**: Multiple aimbot implementations with optimizations
3. **Visual systems**: ESP, backtrack, glow effects
4. **Competitive features**: 20+ competitive information systems
5. **Matrix chat**: Custom C++ Matrix implementation

## Cherry-Pick Categorization

### Category 1: Safe to Cherry-Pick (3 commits)
These commits don't touch our modified files and should apply cleanly:

1. **bffae99** - "forgot, actually delete the file"
   - Deletes `CTFPlayer_UpdateStepSound.cpp`
   - Impact: Low - removes unused hook file
   - Risk: Low

2. **aa8eb67** - "fix intended readme behavior"  
   - README.md fix only
   - Impact: Documentation only
   - Risk: None

3. **c70f787** - "fix melee autowarp not working"
   - One-line fix in `TickHandler.cpp`
   - Impact: Bug fix for melee warping
   - Risk: Low (but may conflict due to file history)

### Category 2: Critical Game Update Fixes (Manual Merge Required)
These commits fix issues with game updates and should be manually integrated:

1. **07c7ab2** - "fix for latest game update" (Mar 6, 2026)
   - Key changes: Projectile simulation fixes, SDK updates
   - 256-line deletion in `Cbuf_ExecuteCommand.cpp`
   - **Priority**: HIGH - likely essential for current game version

2. **236439d** - "fix for latest game update" (Dec 2025)
   - Hook and SDK definition updates
   - **Priority**: HIGH - game compatibility fixes

3. **769b516** - "game update" (Nov 2025)
   - Interface and weapon base updates
   - **Priority**: MEDIUM - SDK compatibility

### Category 3: Performance & Bug Fixes (Evaluate Case-by-Case)
These commits improve performance or fix bugs but touch modified files:

1. **5e8586d** - "improve autodet for grounded stickies"
   - Touches: Aimbot.cpp, AutoDetonate.cpp
   - Benefit: Improved stickybomb detection
   - **Decision**: Manual review needed - our aimbot may have different logic

2. **56fd4a2** - "condense debug hook preprocessor & vars"
   - Touches 108 files (107 modified by us)
   - Benefit: Code cleanup
   - **Decision**: LOW PRIORITY - mostly stylistic

3. **1cde95b** - "rework how points are handled in projectile aim"
   - Touches 14 files (all modified by us)
   - Benefit: Improved projectile aiming
   - **Decision**: MEDIUM PRIORITY - but conflicts likely

### Category 4: Feature Updates (Likely Conflict with Our Features)
These add/change features that may conflict with our competitive systems:

1. **ed04dd5** - "visual changes and fixes glow should be more consistent"
   - Touches 35 files (31 modified by us)
   - **Decision**: HIGH CONFLICT RISK - our visual systems are heavily modified

2. **f82af0a** - "change how points are generated for rocket splash"
   - Touches 19 files (16 modified by us)
   - **Decision**: MEDIUM CONFLICT RISK - affects projectile calculations

## Implementation Strategy

### Phase 1: Safe Cherry-Picks
1. Create integration branch: `git checkout -b upstream-integration`
2. Attempt cherry-pick of Category 1 commits:
   ```
   git cherry-pick aa8eb67  # README fix (safest)
   git cherry-pick bffae99  # File deletion
   git cherry-pick c70f787  # TickHandler fix
   ```
3. Resolve any conflicts (use `git rerere` to record resolutions)

### Phase 2: Critical Game Updates
1. For each critical commit (07c7ab2, 236439d, 769b516):
   - Create patch: `git format-patch -1 <hash>`
   - Examine patch manually
   - Apply selective changes using `git apply --reject` or manual editing
   - Focus on SDK/hook updates that don't conflict with competitive features

### Phase 3: Selective Performance Fixes
1. Create test branches for specific fixes
2. Use `git cherry-pick -n` (no-commit) to stage changes
3. Manually review each hunk in conflicted files
4. Accept upstream changes for:
   - Bug fixes that don't affect competitive logic
   - Performance improvements in shared systems
   - Security fixes

### Phase 4: Feature Evaluation
1. For feature updates, create comparison branches
2. Use `git diff upstream/master..master -- <file>` to see our modifications
3. Decide case-by-case:
   - If upstream feature doesn't conflict: merge
   - If conflicts with competitive feature: keep ours
   - If complementary: manual integration

## Technical Approach

### 1. Git Rerere Setup
```bash
git config --global rerere.enabled true
git config --global rerere.autoupdate true
```

### 2. Integration Branch Workflow
```bash
# Create clean integration branch
git checkout -b integration-test
git fetch upstream

# Test cherry-pick with conflict resolution
git cherry-pick -Xpatience <hash>

# Record resolution for future use
git commit
```

### 3. Patch File Strategy for Complex Changes
```bash
# Create patch for manual review
git format-patch -1 --stdout <hash> > /tmp/patch.diff

# Apply with reject files for conflicts
git apply --reject /tmp/patch.diff

# Manually review .rej files
```

### 4. Three-Way Merge for Critical Files
For files like `AimbotProjectile.cpp`:
```bash
# Create temporary merge base
git merge-base HEAD upstream/master

# Use diff3 for better conflict markers
git config merge.conflictstyle diff3
```

## Conflict Resolution Guidelines

### Always Accept Upstream:
- SDK definition updates (`CTFPlayer.h`, `CTFWeaponBase.h`)
- Game interface changes
- Security fixes
- Crash fixes

### Always Keep Our Version:
- Competitive feature implementations
- Matrix chat integration
- Performance optimizations we've added
- UI/UX changes for competitive play

### Manual Resolution Required:
- Aimbot logic changes
- Visual effect implementations
- Configuration systems
- Hook implementations

## Risk Assessment

### Low Risk (Proceed):
- Documentation updates
- File deletions of unused code
- Typo fixes
- Build system changes

### Medium Risk (Review Carefully):
- Performance optimizations
- Bug fixes in shared systems
- Configuration format changes

### High Risk (Extensive Testing Required):
- Aimbot algorithm changes
- Visual rendering changes
- Network/hook modifications
- Core system architecture changes

## Testing Protocol

1. **Build Test**: Ensure code compiles after each integration
2. **Functionality Test**: Verify competitive features still work
3. **Regression Test**: Check for performance regressions
4. **Game Compatibility Test**: Ensure works with current game version

## Recommended Priority Order

1. **Immediate** (Week 1):
   - Category 1 safe commits
   - Critical game update fixes (07c7ab2, 236439d)

2. **Short-term** (Week 2-3):
   - Performance fixes (5e8586d, 1cde95b)
   - SDK compatibility updates

3. **Long-term** (Month 1-2):
   - Feature evaluations
   - Code cleanup merges

## Fallback Strategy

If conflicts prove too complex:
1. Maintain separate upstream tracking branch
2. Periodically rebase competitive features on top of upstream
3. Use `git merge --no-ff` with extensive conflict resolution
4. Consider maintaining as a true fork with occasional rebases

## Conclusion

Given the extensive modifications in this competitive fork, a conservative cherry-pick strategy is recommended. Focus on:
1. Game compatibility fixes (critical)
2. Bug fixes that don't affect competitive logic
3. Security updates
4. Performance improvements that complement rather than replace our optimizations

Use manual patch application and selective merging rather than bulk cherry-picking to preserve the competitive advantage features that distinguish this fork.