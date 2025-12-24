#pragma once
#include "../../SDK/SDK.h"

class CAutoDisguise
{
private:
    // Track recent kills to prevent duplicate triggers
    struct KillRecord
    {
        int m_iVictimIndex;
        float m_flKillTime;
        int m_iWeaponID;
    };
    std::vector<KillRecord> m_vRecentKills;

    // State tracking for death events
    bool m_bPendingDisguise = false;
    int m_iPendingVictimIndex = -1;
    float m_flPendingTime = 0.f;

    // Alternative kill detection: track last damager and life state
    struct VictimInfo
    {
        int m_iLastDamager = -1; // attacker index
        byte m_lifeState = 0;    // previous life state
        float m_flLastDamageTime = 0.f;
        int m_iWeaponID = 0;     // weapon used for last damage
    };
    std::unordered_map<int, VictimInfo> m_mVictimInfo;

public:
    void Event(IGameEvent* pEvent, uint32_t uHash, CTFPlayer* pLocal);
    void Run(CTFPlayer* pLocal); // New function for state-based triggering

    // Helper functions for comprehensive death detection
    bool CanDisguiseAsVictim(CTFPlayer* pVictim, CTFPlayer* pLocal);
    bool IsSpyWeapon(int iWeaponID);
    void ExecuteDisguise(CTFPlayer* pVictim, CTFPlayer* pLocal);
    void AddKillRecord(int iVictimIndex, int iWeaponID);
    bool HasRecentKill(int iVictimIndex);
    void CheckPendingDisguise(CTFPlayer* pLocal);
};

ADD_FEATURE(CAutoDisguise, AutoDisguise);
