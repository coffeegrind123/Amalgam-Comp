#pragma once
#include "../../SDK/SDK.h"

// Feature: Temporary enemy health display
// Works via CTFPlayer_IsPlayerClass hook that makes local player appear as spy
// This allows TF2's native target ID system to display enemy health
class CTempShowHealth
{
public:
	// SAFETY: Always validates pLocal pointer before use
	void Run(CTFPlayer* pLocal);

	bool m_bEnabled = false; // Disabled by default
};

ADD_FEATURE(CTempShowHealth, TempShowHealth);