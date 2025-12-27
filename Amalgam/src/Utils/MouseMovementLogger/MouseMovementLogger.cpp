#include "MouseMovementLogger.h"
#include "../../SDK/SDK.h"
#include "../../Features/Configs/Configs.h"
#include <Windows.h>
#include <format>
#include <iostream>

void CMouseMovementLogger::Initialize()
{
	m_nSessionId = 0;
}

void CMouseMovementLogger::Unload()
{
	if (m_bRecording)
		StopRecording();
}

void CMouseMovementLogger::Update(const Vec3& vCurrentAngles)
{
	// Only record if debug setting is enabled
	if (!Vars::Debug::MouseMovementLogger.Value)
	{
		if (m_bRecording)
			StopRecording();
		return;
	}

	const bool bMouse2Down = GetAsyncKeyState(VK_RBUTTON) & 0x8000;
	const float flCurrentTime = I::GlobalVars->curtime;

	if (bMouse2Down && !m_bWasRecording)
	{
		m_vStartAngles = vCurrentAngles;
		m_vLastAngles = vCurrentAngles;
		StartRecording();
	}
	else if (!bMouse2Down && m_bWasRecording)
		StopRecording();
	else if (bMouse2Down && m_bRecording)
	{
		MouseMovementSample_t tSample = {};
		tSample.m_flTimestamp = flCurrentTime - m_flStartTime;
		tSample.m_flDeltaTime = flCurrentTime - m_flLastTime;
		tSample.m_vAngles = vCurrentAngles;
		tSample.m_vDeltaAngle = vCurrentAngles - m_vLastAngles;
		tSample.m_flDeltaPitch = tSample.m_vDeltaAngle.x;
		tSample.m_flDeltaYaw = Math::NormalizeAngle(tSample.m_vDeltaAngle.y);
		tSample.m_flTotalPitch = vCurrentAngles.x - m_vStartAngles.x;
		tSample.m_flTotalYaw = Math::NormalizeAngle(vCurrentAngles.y - m_vStartAngles.y);

		m_vSamples.push_back(tSample);
		m_vLastAngles = vCurrentAngles;
		m_flLastTime = flCurrentTime;
	}

	m_bWasRecording = bMouse2Down;
}

void CMouseMovementLogger::SetRecording(bool bRecording)
{
	if (bRecording && !m_bRecording)
		StartRecording();
	else if (!bRecording && m_bRecording)
		StopRecording();
}

void CMouseMovementLogger::StartRecording()
{
	m_bRecording = true;
	m_flStartTime = I::GlobalVars->curtime;
	m_flLastTime = m_flStartTime;

	std::string sPath = F::Configs.m_sConfigPath + std::format("mouse_movement_log_{:04d}.csv", m_nSessionId);
	m_File.open(sPath);
	if (m_File.is_open())
	{
		m_File << "session_id,timestamp_ms,delta_time_ms,pitch,yaw,delta_pitch,delta_yaw,total_pitch,total_yaw\n";
	}
}

void CMouseMovementLogger::StopRecording()
{
	m_bRecording = false;
	FlushToFile();
	Reset();
	m_nSessionId++;
}

void CMouseMovementLogger::FlushToFile()
{
	if (!m_File.is_open() || m_vSamples.empty())
		return;

	for (const auto& tSample : m_vSamples)
	{
		m_File << std::format("{},{:.6f},{:.6f},{:.6f},{:.6f},{:.6f},{:.6f},{:.6f},{:.6f}\n",
			m_nSessionId,
			tSample.m_flTimestamp * 1000.f,
			tSample.m_flDeltaTime * 1000.f,
			tSample.m_vAngles.x,
			tSample.m_vAngles.y,
			tSample.m_flDeltaPitch,
			tSample.m_flDeltaYaw,
			tSample.m_flTotalPitch,
			tSample.m_flTotalYaw);
	}

	m_File.close();
}

void CMouseMovementLogger::Reset()
{
	m_vSamples.clear();
	m_vStartAngles = {};
	m_vLastAngles = {};
}
