#include "Timer.h"

#include "../../SDK/SDK.h"

Timer::Timer()
{
	// OPTIMIZED: Use double directly to match PlatFloatTime() return type
	m_flLast = SDK::PlatFloatTime();
}

bool Timer::Check(float flS) const
{
	// OPTIMIZED: Use double to avoid precision loss from float conversion
	// This prevents subtle timing errors in long-running timers
	const double flCurrentTime = SDK::PlatFloatTime();
	return flCurrentTime - m_flLast >= static_cast<double>(flS);
}

bool Timer::Run(float flS)
{
	// OPTIMIZED: Inline Check() logic to avoid redundant PlatFloatTime() call
	const double flCurrentTime = SDK::PlatFloatTime();
	if (flCurrentTime - m_flLast >= static_cast<double>(flS))
	{
		// Update with already-fetched time instead of calling PlatFloatTime() again
		m_flLast = flCurrentTime;
		return true;
	}
	return false;
}

void Timer::Update()
{
	// OPTIMIZED: Use double directly to match PlatFloatTime() return type
	m_flLast = SDK::PlatFloatTime();
}