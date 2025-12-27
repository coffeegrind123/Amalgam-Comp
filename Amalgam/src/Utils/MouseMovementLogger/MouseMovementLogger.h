#pragma once
#include "../Feature/Feature.h"
#include "../../SDK/Definitions/Types.h"
#include <vector>
#include <fstream>
#include <chrono>

struct MouseMovementSample_t
{
	float m_flTimestamp;
	float m_flDeltaTime;
	Vec3 m_vAngles;
	Vec3 m_vDeltaAngle;
	float m_flDeltaPitch;
	float m_flDeltaYaw;
	float m_flTotalPitch;
	float m_flTotalYaw;
};

class CMouseMovementLogger
{
public:
	void Initialize();
	void Unload();
	void Update(const Vec3& vCurrentAngles);
	void SetRecording(bool bRecording);
	bool IsRecording() const { return m_bRecording; }
	int GetSampleCount() const { return static_cast<int>(m_vSamples.size()); }

private:
	void StartRecording();
	void StopRecording();
	void FlushToFile();
	void Reset();

	bool m_bRecording = false;
	bool m_bWasRecording = false;
	Vec3 m_vStartAngles = {};
	Vec3 m_vLastAngles = {};
	float m_flStartTime = 0.f;
	float m_flLastTime = 0.f;
	std::vector<MouseMovementSample_t> m_vSamples;
	std::ofstream m_File;
	int m_nSessionId = 0;
};

ADD_FEATURE_CUSTOM(CMouseMovementLogger, MouseMovementLogger, U);
