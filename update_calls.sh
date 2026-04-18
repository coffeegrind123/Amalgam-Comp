#!/bin/bash
FILE="/tmp/Amalgam-Comp/Amalgam/src/Features/Simulation/ProjectileSimulation/ProjectileSimulation.cpp"

# Update pattern 1: !bRedirect ? true : Vars::Visuals::Trajectory::Pipes.Value
# → Vars::Visuals::Trajectory::ForwardRedirect.Value, !bRedirect ? 1.f : Vars::Visuals::Trajectory::ForwardCutoff.Value
sed -i "s/!bRedirect ? true : Vars::Visuals::Trajectory::Pipes\.Value/Vars::Visuals::Trajectory::ForwardRedirect.Value, !bRedirect ? 1.f : Vars::Visuals::Trajectory::ForwardCutoff.Value/g" "$FILE"

# Update pattern 2: !bRedirect ? true : false
# → bRedirect ? 2000.f : 0.f, 0.1f
sed -i "s/!bRedirect ? true : false/bRedirect ? 2000.f : 0.f, 0.1f/g" "$FILE"

# Update pattern 3: true (as 6th parameter)
# → 0.f, 0.f
# This is trickier - need to match GetProjectileFireSetup calls with true as 6th param
# Let's do it step by step
sed -i "s/GetProjectileFireSetup(\([^,]*\), \([^,]*\), \([^,]*\), \([^,]*\), \([^,]*\), true, \([^)]*\))/GetProjectileFireSetup(\1, \2, \3, \4, \5, 0.f, 0.f, \6)/g" "$FILE"

# Also update calls with true at end (no bQuick parameter)
sed -i "s/GetProjectileFireSetup(\([^,]*\), \([^,]*\), \([^,]*\), \([^,]*\), \([^,]*\), true)/GetProjectileFireSetup(\1, \2, \3, \4, \5, 0.f, 0.f)/g" "$FILE"

# Update bQuick parameter name (7th param becomes 8th after adding flCutoff)
sed -i "s/, bQuick)/, bQuick)/g" "$FILE"  # This keeps it the same since we added a param before it

echo "Updated GetProjectileFireSetup calls"