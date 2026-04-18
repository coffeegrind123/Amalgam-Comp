#pragma once

class Timer
{
private:
	// OPTIMIZED: Use double to match PlatFloatTime() return type - prevents precision loss
	double m_flLast = 0.0;

public:
	Timer();

	// Check if interval has elapsed without updating timer
	bool Check(float flS) const;

	// Check interval and update timer if elapsed
	bool Run(float flS);
<<<<<<< HEAD

	// Update timer to current time
=======
>>>>>>> upstream/master
	void Update();
};