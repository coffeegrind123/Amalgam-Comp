#include "MedicUberBar.h"
#include <cmath>

void CMedicUberBar::Draw()
{
    if (!Vars::Competitive::Features::MedicUberBar.Value)
        return;

    // Early exits (match native ESP pattern)
    if (I::EngineVGui->IsGameUIVisible())
        return;

    auto pLocal = H::Entities.GetLocal();
    if (!pLocal)
        return;

    Vec3 localPos = pLocal->GetAbsOrigin();
    int localTeam = pLocal->m_iTeamNum();

    // Process all players looking for medics
    for (auto pEntity : H::Entities.GetGroup(EGroupType::PLAYERS_ALL))
    {
        auto pPlayer = pEntity->As<CTFPlayer>();
        if (!pPlayer || !pPlayer->IsAlive() || pPlayer->IsDormant() || pPlayer == pLocal)
            continue;

        // Only process medics
        if (pPlayer->m_iClass() != TF_CLASS_MEDIC)
            continue;

        // Skip if cloaked or disguised
        if (pPlayer->InCond(TF_COND_STEALTHED) || pPlayer->InCond(TF_COND_DISGUISED))
            continue;

        bool isFriendly = (pPlayer->m_iTeamNum() == localTeam);

        // Check team filtering
        if (isFriendly && !Vars::Competitive::MedicUberBar::ShowFriendly.Value)
            continue;
        if (!isFriendly && !Vars::Competitive::MedicUberBar::ShowEnemy.Value)
            continue;

        Vec3 playerPos = pPlayer->GetAbsOrigin();

        // Distance check
        Vec3 delta = playerPos - localPos;
        float distSqr = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
        if (distSqr > (Vars::Competitive::MedicUberBar::MaxDistance.Value * Vars::Competitive::MedicUberBar::MaxDistance.Value))
            continue;

        // Get uber percentage
        float uberPercentage = GetUberPercentage(pPlayer);
        bool isAlive = pPlayer->IsAlive();

        // Check visibility
        bool isVisible = IsVisible(playerPos);

        // Draw uber bar if enabled and visible (or through walls enabled)
        if (Vars::Competitive::MedicUberBar::ShowThroughWalls.Value || isVisible)
        {
            Vec3 screenPos;
            // Use head height position for medic uber bars
            Vec3 headPos = playerPos;
            headPos.z += 75.0f; // Head height for medics

            if (SDK::W2S(headPos, screenPos))
            {
                // Allow uber bars slightly off-screen for close range players
                if (screenPos.x >= -100 && screenPos.x <= H::Draw.m_nScreenW + 100 &&
                    screenPos.y >= -100 && screenPos.y <= H::Draw.m_nScreenH + 100)
                {
                    int x = (int)(screenPos.x - Vars::Competitive::MedicUberBar::BarWidth.Value / 2);
                    int y = (int)(screenPos.y - 20); // Above head
                    DrawUberBar(x, y, Vars::Competitive::MedicUberBar::BarWidth.Value, uberPercentage, isAlive);
                }
            }
        }
    }
}

Color_t CMedicUberBar::GetUberBarColor(float uberPercentage, bool isAlive)
{
    if (!isAlive)
    {
        Color_t result = Vars::Competitive::MedicUberBar::DeadColor.Value;
        result.a = 255; // Always 100% opacity
        return result;
    }

    if (uberPercentage >= 100.0f)
    {
        Color_t result = Vars::Competitive::MedicUberBar::FullUberColor.Value;
        result.a = 255; // Always 100% opacity
        return result;
    }
    else if (uberPercentage >= 67.0f)
    {
        Color_t result = Vars::Competitive::MedicUberBar::HighUberColor.Value;
        result.a = 255; // Always 100% opacity
        return result;
    }
    else if (uberPercentage >= 34.0f)
    {
        Color_t result = Vars::Competitive::MedicUberBar::MidUberColor.Value;
        result.a = 255; // Always 100% opacity
        return result;
    }
    else
    {
        Color_t result = Vars::Competitive::MedicUberBar::LowUberColor.Value;
        result.a = 255; // Always 100% opacity
        return result;
    }
}

void CMedicUberBar::DrawUberBar(int x, int y, int width, float uberPercentage, bool isAlive)
{
    int height = Vars::Competitive::MedicUberBar::BarHeight.Value;

    // Calculate bar fill based on uber percentage
    int uberBarSize = (int)(width * (uberPercentage / 100.0f));

    // Background (black with full opacity)
    H::Draw.FillRect(x, y, width, height, {0, 0, 0, 255});

    // Uber fill bar
    Color_t uberColor = GetUberBarColor(uberPercentage, isAlive);
    if (uberBarSize > 2)
        H::Draw.FillRect(x + 1, y + 1, uberBarSize - 2, height - 2, uberColor);

    // Show percentage text if enabled
    if (Vars::Competitive::MedicUberBar::ShowPercentage.Value)
    {
        std::string percentText = isAlive ? (std::to_string(static_cast<int>(uberPercentage)) + "%") : "DEAD";
        Color_t textColor = {255, 255, 255, 255}; // Always 100% opacity

        auto font = H::Fonts.GetFont(FONT_INDICATORS);
        int textX = x + width / 2;
        int textY = y + height / 2;

        H::Draw.String(font, textX, textY, textColor, ALIGN_CENTER, percentText.c_str());
    }
}

bool CMedicUberBar::IsVisible(const Vec3& vPos)
{
    auto pLocal = H::Entities.GetLocal();
    if (!pLocal)
        return false;

    Vec3 vEyePos = pLocal->GetAbsOrigin() + pLocal->m_vecViewOffset();
    Vec3 targetPos = vPos;
    targetPos.z += 60.0f; // Head height

    // Check distance first - if very close, be more lenient with visibility
    float distance = (targetPos - vEyePos).Length();
    if (distance < 150.0f)
    {
        // At very close range, skip trace check to prevent disappearing
        return true;
    }

    CGameTrace trace = {};
    CTraceFilterHitscan filter = {};
    filter.pSkip = pLocal;

    SDK::Trace(vEyePos, targetPos, MASK_VISIBLE, &filter, &trace);

    // Use more lenient trace fraction for close range
    float requiredFraction = (distance < 300.0f) ? 0.75f : 0.90f;

    return trace.fraction > requiredFraction;
}

std::string CMedicUberBar::GetWeaponType(int iItemDefinitionIndex)
{
    switch (iItemDefinitionIndex)
    {
        case 29:   // Standard Medigun
        case 211:  // Festive Medigun
        case 663:  // Silver Botkiller Medigun
            return "UBER";
        case 35:   // Kritzkrieg
            return "KRITZ";
        case 411:  // Quick-Fix
            return "QUICKFIX";
        case 998:  // Vaccinator
            return "VACCINATOR";
        default:
            return "UBER";
    }
}

float CMedicUberBar::GetUberPercentage(CTFPlayer* pMedic)
{
    if (!pMedic || !pMedic->IsAlive())
        return 0.0f;

    // Get the medigun (secondary weapon)
    auto pMedigun = pMedic->GetWeaponFromSlot(SLOT_SECONDARY);
    if (!pMedigun)
        return 0.0f;

    auto pMedigunWeapon = pMedigun->As<CWeaponMedigun>();
    if (!pMedigunWeapon)
        return 0.0f;

    try {
        float flChargeLevel = pMedigunWeapon->m_flChargeLevel();
        return flChargeLevel * 100.0f;
    }
    catch (...) {
        return 0.0f;
    }
}