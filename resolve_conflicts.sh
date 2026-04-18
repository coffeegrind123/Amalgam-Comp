#!/bin/bash

# Strategy for resolving conflicts:
# 1. SDK/API changes: Take upstream's (game compatibility)
# 2. Visuals/competitive features: Take ours (preserve fork identity)
# 3. Core architecture: Hybrid approach
# 4. Build files: Take ours (simplified ReleaseFreetypeAVX2)
# 5. Documentation: Take ours (competitive fork branding)

echo "Resolving conflicts based on strategy..."

# List of files to take upstream's version (SDK/API changes)
UPSTREAM_FILES=(
    "Amalgam/src/SDK/Definitions/Definitions.h"
    "Amalgam/src/SDK/Definitions/Interfaces.h"
    "Amalgam/src/SDK/Definitions/Main/CBaseEntity.cpp"
    "Amalgam/src/SDK/Definitions/Main/CBaseEntity.h"
    "Amalgam/src/SDK/Definitions/Main/CPredictionCopy.h"
    "Amalgam/src/SDK/Definitions/Main/CTFPlayer.h"
    "Amalgam/src/SDK/Definitions/Main/CTFWeaponBase.h"
    "Amalgam/src/SDK/Definitions/Main/KeyValues.cpp"
    "Amalgam/src/SDK/Definitions/Types.h"
    "Amalgam/src/SDK/Events/Events.cpp"
    "Amalgam/src/SDK/Helpers/ConVars/ConVars.cpp"
    "Amalgam/src/SDK/Helpers/ConVars/ConVars.h"
    "Amalgam/src/SDK/Helpers/Draw/Draw.cpp"
    "Amalgam/src/SDK/Helpers/Draw/Draw.h"
    "Amalgam/src/SDK/Helpers/Entities/Entities.cpp"
    "Amalgam/src/SDK/Helpers/Entities/Entities.h"
    "Amalgam/src/SDK/SDK.cpp"
    "Amalgam/src/SDK/SDK.h"
    "Amalgam/src/SDK/Vars.h"
    "Amalgam/src/Utils/Interfaces/Interfaces.cpp"
    "Amalgam/src/Utils/Interfaces/Interfaces.h"
    "Amalgam/src/Utils/Signatures/Signatures.h"
    "Amalgam/src/Utils/Timer/Timer.h"
)

# List of files to take our version (Visuals/competitive features)
OUR_FILES=(
    "Amalgam/src/Features/Aimbot/Aimbot.cpp"
    "Amalgam/src/Features/Aimbot/Aimbot.h"
    "Amalgam/src/Features/Aimbot/AimbotGlobal/AimbotGlobal.cpp"
    "Amalgam/src/Features/Aimbot/AimbotGlobal/AimbotGlobal.h"
    "Amalgam/src/Features/Aimbot/AimbotHitscan/AimbotHitscan.cpp"
    "Amalgam/src/Features/Aimbot/AimbotHitscan/AimbotHitscan.h"
    "Amalgam/src/Features/Aimbot/AimbotMelee/AimbotMelee.cpp"
    "Amalgam/src/Features/Aimbot/AimbotMelee/AimbotMelee.h"
    "Amalgam/src/Features/Aimbot/AimbotProjectile/AimbotProjectile.cpp"
    "Amalgam/src/Features/Aimbot/AimbotProjectile/AimbotProjectile.h"
    "Amalgam/src/Features/Aimbot/AutoDetonate/AutoDetonate.cpp"
    "Amalgam/src/Features/Aimbot/AutoRocketJump/AutoRocketJump.cpp"
    "Amalgam/src/Features/Backtrack/Backtrack.cpp"
    "Amalgam/src/Features/Backtrack/Backtrack.h"
    "Amalgam/src/Features/Binds/Binds.cpp"
    "Amalgam/src/Features/Binds/Binds.h"
    "Amalgam/src/Features/Configs/Configs.cpp"
    "Amalgam/src/Features/Configs/Configs.h"
    "Amalgam/src/Features/CritHack/CritHack.cpp"
    "Amalgam/src/Features/EnginePrediction/EnginePrediction.cpp"
    "Amalgam/src/Features/EnginePrediction/EnginePrediction.h"
    "Amalgam/src/Features/ImGui/Menu/Components.h"
    "Amalgam/src/Features/ImGui/Menu/Menu.cpp"
    "Amalgam/src/Features/ImGui/Menu/Menu.h"
    "Amalgam/src/Features/Misc/Misc.cpp"
    "Amalgam/src/Features/NetworkFix/NetworkFix.cpp"
    "Amalgam/src/Features/NoSpread/NoSpreadHitscan/NoSpreadHitscan.cpp"
    "Amalgam/src/Features/Players/PlayerUtils.cpp"
    "Amalgam/src/Features/Players/PlayerUtils.h"
    "Amalgam/src/Features/Simulation/MovementSimulation/MovementSimulation.cpp"
    "Amalgam/src/Features/Simulation/MovementSimulation/MovementSimulation.h"
    "Amalgam/src/Features/Simulation/ProjectileSimulation/ProjectileSimulation.cpp"
    "Amalgam/src/Features/Simulation/ProjectileSimulation/ProjectileSimulation.h"
    "Amalgam/src/Features/Spectate/Spectate.cpp"
    "Amalgam/src/Features/Ticks/Ticks.cpp"
    "Amalgam/src/Features/Ticks/Ticks.h"
    "Amalgam/src/Features/Visuals/CameraWindow/CameraWindow.cpp"
    "Amalgam/src/Features/Visuals/Chams/Chams.cpp"
    "Amalgam/src/Features/Visuals/ESP/ESP.cpp"
    "Amalgam/src/Features/Visuals/ESP/ESP.h"
    "Amalgam/src/Features/Visuals/Glow/Glow.cpp"
    "Amalgam/src/Features/Visuals/Groups/Groups.cpp"
    "Amalgam/src/Features/Visuals/Groups/Groups.h"
    "Amalgam/src/Features/Visuals/Materials/Materials.cpp"
    "Amalgam/src/Features/Visuals/Notifications/Notifications.cpp"
    "Amalgam/src/Features/Visuals/SpectatorList/SpectatorList.cpp"
    "Amalgam/src/Features/Visuals/Visuals.cpp"
    "Amalgam/src/Hooks/CBasePlayer_CalcObserverView.cpp"
    "Amalgam/src/Hooks/CBasePlayer_CalcView.cpp"
    "Amalgam/src/Hooks/CBasePlayer_CalcViewModelView.cpp"
    "Amalgam/src/Hooks/CHLClient_DispatchUserMessage.cpp"
    "Amalgam/src/Hooks/CHLClient_FrameStageNotify.cpp"
    "Amalgam/src/Hooks/CHLClient_LevelShutdown.cpp"
    "Amalgam/src/Hooks/CL_Move.cpp"
    "Amalgam/src/Hooks/CNetChannel_SendNetMsg.cpp"
    "Amalgam/src/Hooks/CTFPlayerPanel_GetTeam.cpp"
    "Amalgam/src/Hooks/CTFPlayerShared_IsPlayerDominated.cpp"
    "Amalgam/src/Hooks/CTFPlayerShared_ShouldSuppressPrediction.cpp"
    "Amalgam/src/Hooks/CTFPlayer_IsPlayerClass.cpp"
    "Amalgam/src/Hooks/Direct3DDevice9.cpp"
    "Amalgam/src/Hooks/GetClientInterpAmount.cpp"
    "Amalgam/src/Hooks/IMaterialSystem_FindTexture.cpp"
)

# Take upstream's version for SDK/API files
echo "Taking upstream's version for SDK/API files..."
for file in "${UPSTREAM_FILES[@]}"; do
    if [ -f "$file" ]; then
        echo "  $file"
        git checkout --theirs "$file" 2>/dev/null || echo "    (not in conflict)"
    fi
done

# Take our version for Visuals/competitive features
echo "Taking our version for Visuals/competitive features..."
for file in "${OUR_FILES[@]}"; do
    if [ -f "$file" ]; then
        echo "  $file"
        git checkout --ours "$file" 2>/dev/null || echo "    (not in conflict)"
    fi
done

# Documentation files - take ours
echo "Taking our version for documentation..."
git checkout --ours README.md 2>/dev/null || echo "  README.md (not in conflict)"

echo "Conflict resolution complete!"