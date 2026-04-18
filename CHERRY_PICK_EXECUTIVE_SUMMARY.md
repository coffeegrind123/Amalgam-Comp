# Executive Summary: Cherry-Pick Strategy for Amalgam-Comp

## Key Findings

1. **High Divergence**: 367 files modified in competitive fork vs upstream
2. **Low Cherry-Pick Success**: Only 3/150 upstream commits don't touch modified files
3. **Even "Safe" Commits Conflict**: README, file deletions cause conflicts due to fork divergence

## Recommended Strategy

### Immediate Priority (Game Compatibility)
1. **Manual patch application** for game update fixes:
   - `07c7ab2` - Mar 2026 game update
   - `236439d` - Dec 2025 game update  
   - `769b516` - Nov 2025 game update

2. **Focus on SDK/hook files** - critical for game compatibility
3. **Skip UI/documentation** - our fork has different goals

### Selective Integration
1. **Test carefully**: `c70f787` (melee autowarp fix)
2. **Evaluate case-by-case**: Performance fixes like `5e8586d` (stickybomb detection)
3. **Skip**: README/UI changes that conflict with competitive features

### Technical Approach
- Use `git apply --reject` for manual patch application
- Enable `git rerere` to record conflict resolutions
- Create integration branch for testing
- Manual review of all changes (no automatic cherry-picking)

## Risk Assessment
- **High Risk**: Automatic cherry-picking (will fail)
- **Medium Risk**: Manual patch application (requires careful review)
- **Low Risk**: Selective SDK updates only

## Action Plan
1. Week 1: Apply critical game update patches manually
2. Week 2-3: Evaluate and test performance fixes
3. Ongoing: Manual review of upstream commits, focus on game compatibility

## Conclusion
Given the extensive competitive modifications, a conservative manual integration approach is required. Focus on game compatibility fixes while preserving competitive features. Automatic cherry-picking is not feasible due to high divergence.