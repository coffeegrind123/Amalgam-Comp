#include "AutoDisguise.h"
#include <unordered_map>

void CAutoDisguise::Event(IGameEvent* pEvent, uint32_t uHash, CTFPlayer* pLocal)
{
    // Only process if enabled
    if (!Vars::Misc::AutoDisguise::Enabled.Value)
        return;

    switch (uHash)
    {
    case FNV1A::Hash32Const("player_death"):
    {
        // Get the attacker (should be local player)
        int iAttacker = I::EngineClient->GetPlayerForUserID(pEvent->GetInt("attacker"));
        if (iAttacker != I::EngineClient->GetLocalPlayer())
            return;

        // Get the victim
        int iVictim = I::EngineClient->GetPlayerForUserID(pEvent->GetInt("userid"));
        if (iVictim <= 0 || iVictim > MAX_PLAYERS)
            return;

        // Get victim's player entity
        CTFPlayer* pVictim = I::ClientEntityList->GetClientEntity(iVictim)->As<CTFPlayer>();
        if (!pVictim)
            return;

        // Check if this is a duplicate kill
        if (HasRecentKill(iVictim))
            return;

        // Get weapon ID from the event
        int iWeaponID = pEvent->GetInt("weaponid");

        // Add kill record to prevent duplicates
        AddKillRecord(iVictim, iWeaponID);

        // Primary death event processing
        if (!pLocal || !pLocal->IsAlive())
        {
            // Player died shortly after kill, schedule disguise for respawn
            m_bPendingDisguise = true;
            m_iPendingVictimIndex = iVictim;
            m_flPendingTime = I::GlobalVars->curtime + 0.5f; // Delay for respawn
            return;
        }

        // Check if local player is Spy
        if (pLocal->m_iClass() != TF_CLASS_SPY)
            return;

        // Check if weapon is Spy weapon
        if (!IsSpyWeapon(iWeaponID))
            return;

        // Check if we can disguise as this victim
        if (!CanDisguiseAsVictim(pVictim, pLocal))
            return;

        // Execute disguise immediately
        ExecuteDisguise(pVictim, pLocal);
        break;
    }
    case FNV1A::Hash32Const("player_hurt"):
    {
        // Secondary trigger: player_hurt event that results in death
        int iAttacker = I::EngineClient->GetPlayerForUserID(pEvent->GetInt("attacker"));
        if (iAttacker != I::EngineClient->GetLocalPlayer())
            return;

        int iVictim = I::EngineClient->GetPlayerForUserID(pEvent->GetInt("userid"));
        if (iVictim <= 0 || iVictim > MAX_PLAYERS)
            return;

        int iHealth = pEvent->GetInt("health");
        int iDamage = pEvent->GetInt("damageamount");

        // Update victim info for alternative kill detection
        VictimInfo& info = m_mVictimInfo[iVictim];
        info.m_iLastDamager = iAttacker;
        info.m_flLastDamageTime = I::GlobalVars->curtime;
        info.m_iWeaponID = pEvent->GetInt("weaponid");

        // Check if this damage will likely result in death
        CTFPlayer* pVictim = I::ClientEntityList->GetClientEntity(iVictim)->As<CTFPlayer>();
        if (pVictim && pVictim->IsAlive() && iHealth <= 0)
        {
            // Victim will die from this damage, prepare disguise
            int iWeaponID = info.m_iWeaponID;

            if (pLocal && pLocal->IsAlive() &&
                pLocal->m_iClass() == TF_CLASS_SPY &&
                IsSpyWeapon(iWeaponID) &&
                CanDisguiseAsVictim(pVictim, pLocal))
            {
                // Schedule disguise with slight delay to ensure death occurs
                m_bPendingDisguise = true;
                m_iPendingVictimIndex = iVictim;
                m_flPendingTime = I::GlobalVars->curtime + 0.1f;
            }
        }
        break;
    }
    case FNV1A::Hash32Const("player_spawn"):
    {
        // Handle respawn disguise
        int iIndex = I::EngineClient->GetPlayerForUserID(pEvent->GetInt("userid"));
        if (iIndex == I::EngineClient->GetLocalPlayer() && m_bPendingDisguise)
        {
            // Local player respawned, check if we need to disguise
            CTFPlayer* pVictim = I::ClientEntityList->GetClientEntity(m_iPendingVictimIndex)->As<CTFPlayer>();
            if (pVictim && pLocal && pLocal->IsAlive() &&
                pLocal->m_iClass() == TF_CLASS_SPY &&
                CanDisguiseAsVictim(pVictim, pLocal))
            {
                ExecuteDisguise(pVictim, pLocal);
                m_bPendingDisguise = false;
            }
        }
        break;
    }
    }
}

void CAutoDisguise::Run(CTFPlayer* pLocal)
{
    // Clean up old kill records
    float flCurrentTime = I::GlobalVars->curtime;
    for (auto it = m_vRecentKills.begin(); it != m_vRecentKills.end();)
    {
        if (flCurrentTime - it->m_flKillTime > 5.0f) // Keep records for 5 seconds
            it = m_vRecentKills.erase(it);
        else
            ++it;
    }

    // Clean up old victim info (older than 10 seconds)
    for (auto it = m_mVictimInfo.begin(); it != m_mVictimInfo.end();)
    {
        if (flCurrentTime - it->second.m_flLastDamageTime > 10.0f)
            it = m_mVictimInfo.erase(it);
        else
            ++it;
    }

    // Check pending disguise
    if (m_bPendingDisguise && flCurrentTime >= m_flPendingTime)
    {
        CheckPendingDisguise(pLocal);
    }

    // Alternative kill detection via life state changes
    if (!pLocal || !pLocal->IsAlive() || pLocal->m_iClass() != TF_CLASS_SPY)
        return;

    for (auto& [iVictimIndex, info] : m_mVictimInfo)
    {
        // Only process if we were the last damager
        if (info.m_iLastDamager != I::EngineClient->GetLocalPlayer())
            continue;

        CTFPlayer* pVictim = I::ClientEntityList->GetClientEntity(iVictimIndex)->As<CTFPlayer>();
        if (!pVictim)
            continue;

        byte currentLifeState = pVictim->m_lifeState();
        byte previousLifeState = info.m_lifeState;

        // If life state changed from alive (0) to dead (1 or 2)
        if (previousLifeState == 0 && currentLifeState != 0)
        {
            // Victim just died, check if we can disguise
            if (!HasRecentKill(iVictimIndex) &&
                IsSpyWeapon(info.m_iWeaponID) &&
                CanDisguiseAsVictim(pVictim, pLocal))
            {
                ExecuteDisguise(pVictim, pLocal);
                AddKillRecord(iVictimIndex, info.m_iWeaponID);
            }
        }

        // Update stored life state
        info.m_lifeState = currentLifeState;
    }
}

bool CAutoDisguise::CanDisguiseAsVictim(CTFPlayer* pVictim, CTFPlayer* pLocal)
{
    if (!pVictim || !pLocal)
        return false;

    // Only disguise as victim's class if they're on opposite team
    if (pVictim->m_iTeamNum() == pLocal->m_iTeamNum())
        return false;

    // Get victim's class
    int iVictimClass = pVictim->m_iClass();

    // Validate class is within valid range
    if (iVictimClass < TF_CLASS_SCOUT || iVictimClass > TF_CLASS_ENGINEER)
        return false;

    return true;
}

bool CAutoDisguise::IsSpyWeapon(int iWeaponID)
{
    // Check if weapon is Spy knife or revolver
    bool bIsKnife = (iWeaponID == TF_WEAPON_KNIFE);
    bool bIsRevolver = (iWeaponID == TF_WEAPON_REVOLVER);

    // Support for other Spy revolvers
    if (!bIsRevolver)
    {
        switch (iWeaponID)
        {
        case 61: // Ambassador
        case 224: // L'Etranger
        case 460: // Enforcer
        case 525: // Diamondback
        case 161: // Big Kill
            bIsRevolver = true;
            break;
        }
    }

    // Support for other Spy knives
    if (!bIsKnife)
    {
        switch (iWeaponID)
        {
        case 225: // Your Eternal Reward
        case 356: // Conniver's Kunai
        case 461: // Big Earner
        case 574: // Wanga Prick
        case 638: // Sharp Dresser
        case 649: // Spy-cicle
        case 727: // Black Rose
            bIsKnife = true;
            break;
        }
    }

    return bIsKnife || bIsRevolver;
}

void CAutoDisguise::ExecuteDisguise(CTFPlayer* pVictim, CTFPlayer* pLocal)
{
    if (!pVictim || !pLocal)
        return;

    // Get victim's class
    int iVictimClass = pVictim->m_iClass();

    // Send disguise command: "disguise <class> <team>"
    // Team: -1 for enemy team (auto-detect), 1 for RED, 2 for BLU
    // Class: victim's class number (1-9)
    char sCmd[64];
    snprintf(sCmd, sizeof(sCmd), "disguise %d -1", iVictimClass);

    // Execute the disguise command silently
    I::EngineClient->ServerCmd(sCmd, true);
}

void CAutoDisguise::AddKillRecord(int iVictimIndex, int iWeaponID)
{
    // Add kill record to prevent duplicate triggers
    KillRecord record;
    record.m_iVictimIndex = iVictimIndex;
    record.m_flKillTime = I::GlobalVars->curtime;
    record.m_iWeaponID = iWeaponID;

    m_vRecentKills.push_back(record);
}

bool CAutoDisguise::HasRecentKill(int iVictimIndex)
{
    // Check if we've already processed a kill for this victim recently
    float flCurrentTime = I::GlobalVars->curtime;
    for (const auto& record : m_vRecentKills)
    {
        if (record.m_iVictimIndex == iVictimIndex &&
            flCurrentTime - record.m_flKillTime < 3.0f) // 3 second window
            return true;
    }
    return false;
}

void CAutoDisguise::CheckPendingDisguise(CTFPlayer* pLocal)
{
    if (!m_bPendingDisguise || !pLocal)
        return;

    CTFPlayer* pVictim = I::ClientEntityList->GetClientEntity(m_iPendingVictimIndex)->As<CTFPlayer>();

    // Check conditions again before executing
    if (pVictim && pLocal->IsAlive() &&
        pLocal->m_iClass() == TF_CLASS_SPY &&
        CanDisguiseAsVictim(pVictim, pLocal))
    {
        ExecuteDisguise(pVictim, pLocal);
    }

    // Reset pending state
    m_bPendingDisguise = false;
    m_iPendingVictimIndex = -1;
    m_flPendingTime = 0.f;
}
