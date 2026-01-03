#pragma once
#include "../Interfaces/IGameEvents.h"

// SAFETY: Base class for game event listeners with null-safe event handling
// Derived classes must validate event pointer before use in FireGameEvent()
class CGameEventListener : public IGameEventListener2
{
public:
	// SAFETY: Pure virtual - implementers MUST validate event parameter for null
	// Event pointer may be null in edge cases during shutdown or map changes
	virtual void FireGameEvent(IGameEvent* event) = 0;

	// OPTIMIZED: Inline getter for registration state (avoids function call overhead)
	inline bool IsRegistered() const { return m_bRegisteredForEvents; }

protected:
	// SAFETY: Initialize registration state to prevent undefined behavior
	CGameEventListener() : m_bRegisteredForEvents(false) {}

	virtual ~CGameEventListener() = default;

private:
	bool m_bRegisteredForEvents = false; // FIXED: Initialize to prevent undefined behavior
};