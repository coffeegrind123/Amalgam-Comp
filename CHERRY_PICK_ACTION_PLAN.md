# Cherry-Pick Action Plan for Amalgam-Comp

## Analysis Summary

After analyzing the repository, here are the key findings:

### Repository State
- **367 files modified** in our competitive fork vs upstream
- **Only 3 out of 150 upstream commits** don't touch our modified files
- **Extensive competitive features** added (UberTracker, Matrix integration, etc.)
- **Complete README rewrite** for competitive fork

### Critical Discovery
Even "safe" commits conflict because:
1. Our fork has completely rewritten documentation
2. Files may have been renamed or restructured
3. The codebase has diverged significantly

## Actionable Plan

### Phase 1: Immediate Actions (This Week)

#### 1.1 Enable Git Rerere
```bash
git config --global rerere.enabled true
git config --global rerere.autoupdate true
```

#### 1.2 Create Integration Infrastructure
```bash
# Create permanent integration branch
git checkout -b upstream-integration

# Set up diff3 for better conflict resolution
git config merge.conflictstyle diff3
```

#### 1.3 Test Minimal Cherry-Picks
Try these in order (easiest to hardest):
```bash
# 1. File deletion (bffae99) - may conflict if file doesn't exist
git cherry-pick bffae99

# 2. TickHandler fix (c70f787) - simple one-line fix
git cherry-pick c70f787

# 3. README fix (aa8eb67) - will definitely conflict, skip or manually resolve
```

### Phase 2: Critical Game Updates (Next 2 Weeks)

#### 2.1 Manual Patch Application for Game Compatibility
For commits `07c7ab2`, `236439d`, `769b516`:
```bash
# Create patches for manual review
git format-patch -1 07c7ab2 --stdout > game_update_2026_03.patch
git format-patch -1 236439d --stdout > game_update_2025_12.patch
git format-patch -1 769b516 --stdout > game_update_2025_11.patch

# Apply with reject files
git apply --reject game_update_2026_03.patch
```

#### 2.2 Priority Files to Merge
Focus on these file types from game update commits:
- `SDK/Definitions/*.h` - Game interface definitions
- `Hooks/*.cpp` - Game hook implementations  
- `SDK/SDK.cpp` - SDK initialization

### Phase 3: Selective Performance Fixes (Month 1)

#### 3.1 Evaluate These Commits Case-by-Case
- `5e8586d` - "improve autodet for grounded stickies"
- `1cde95b` - "rework how points are handled in projectile aim"
- `56fd4a2` - "condense debug hook preprocessor & vars"

#### 3.2 Manual Review Process
For each commit:
```bash
# Create test branch
git checkout -b test-<commit-hash>

# Apply without committing
git cherry-pick -n <hash>

# Review changes
git diff --cached

# Manually edit conflicted files
# Commit only accepted changes
```

### Phase 4: Long-term Strategy (Ongoing)

#### 4.1 Consider Alternative Approaches
Given the high conflict rate, consider:
1. **Periodic rebase** instead of cherry-pick
2. **Maintain separate upstream tracking branch**
3. **Manual synchronization** of critical fixes only

#### 4.2 Create Update Protocol
Document which types of upstream changes to:
- **Always accept**: SDK updates, game compatibility fixes
- **Sometimes accept**: Performance improvements, bug fixes
- **Usually reject**: UI changes, feature additions that conflict with competitive features

## Specific Commit Recommendations

### ✅ RECOMMENDED (Try to integrate)
| Commit | Reason | Approach |
|--------|--------|----------|
| `07c7ab2` | Game update fix (Mar 2026) | Manual patch application |
| `236439d` | Game update fix (Dec 2025) | Manual patch application |
| `769b516` | Game update (Nov 2025) | Manual patch application |
| `c70f787` | Melee autowarp fix | Cherry-pick with conflict resolution |

### ⚠️ EVALUATE (Test carefully)
| Commit | Reason | Risk |
|--------|--------|------|
| `5e8586d` | Stickybomb detection | Medium - touches aimbot logic |
| `1cde95b` | Projectile aiming | High - conflicts with our aimbot |
| `56fd4a2` | Code cleanup | Low - but touches 108 files |

### ❌ SKIP (Not worth the effort)
| Commit | Reason |
|--------|--------|
| `aa8eb67` | README fix - our README is completely different |
| Most visual/UI commits | Our competitive UI is heavily modified |
| Feature additions | Likely conflict with competitive features |

## Conflict Resolution Guide

### When to Accept Upstream Changes
- SDK/interface definitions
- Game mechanic fixes
- Crash fixes
- Security patches

### When to Keep Our Changes
- Competitive feature implementations
- Matrix chat integration
- Performance optimizations we've added
- UI/UX for competitive play

### Manual Resolution Required For
- Aimbot algorithms
- Visual rendering
- Configuration systems
- Hook implementations

## Testing Protocol

After each integration:
1. **Build test**: `msbuild Amalgam.sln`
2. **Smoke test**: Run basic functionality
3. **Competitive feature test**: Verify all competitive systems work
4. **Performance test**: Check for regressions

## Risk Mitigation

### Backup Strategy
```bash
# Before major integrations
git tag backup-pre-integration-$(date +%Y%m%d)
git push origin backup-pre-integration-$(date +%Y%m%d)
```

### Rollback Plan
```bash
# If integration fails
git reset --hard backup-pre-integration-*
git push -f origin integration-test
```

## Conclusion

Given the extensive modifications in this competitive fork, a conservative approach is essential:

1. **Focus on game compatibility fixes** - these are critical
2. **Skip documentation/UI changes** - our fork has different goals
3. **Manual review for everything** - automatic cherry-picking won't work
4. **Test thoroughly** - competitive features are the value proposition

The recommended priority is:
1. Game update fixes (`07c7ab2`, `236439d`, `769b516`)
2. Critical bug fixes that don't affect competitive logic
3. Performance improvements that complement our optimizations
4. Everything else (evaluate case-by-case)

This approach minimizes risk while ensuring the fork remains compatible with game updates and benefits from important upstream fixes.