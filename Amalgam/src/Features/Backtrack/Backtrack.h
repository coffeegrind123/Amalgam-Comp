#pragma once
#include "../../SDK/SDK.h"
#include <optional>

class CIncomingSequence
{
public:
	int m_nInReliableState;
	int m_nSequenceNr;
	float m_flTime;

	CIncomingSequence(int iState, int iSequence, float flTime)
	{
		m_nInReliableState = iState;
		m_nSequenceNr = iSequence;
		m_flTime = flTime;
	}
};

struct BoneMatrix
{
	matrix3x4 m_aBones[MAXSTUDIOBONES];
};

struct HitboxInfo
{
	int m_iBone = -1;
	int m_nHitbox = -1;
	Vec3 m_vCenter = {};
	Vec3 m_iMin = {};
	Vec3 m_iMax = {};
};

struct TickRecord
{
	float m_flSimTime = 0.f;
	Vec3 m_vOrigin = {};
	Vec3 m_vMins = {};
	Vec3 m_vMaxs = {};
	BoneMatrix m_BoneMatrix = {};
	std::vector<HitboxInfo> m_vHitboxInfos = {};
	bool m_bOnShot = false;
	Vec3 m_vBreak = {};
	bool m_bInvalid = false;
};

class CBacktrack
{
	void UpdateDatagram();
	void MakeRecords();
	void CleanRecords();

	std::unordered_map<CBaseEntity*, std::deque<TickRecord>> m_mRecords = {};
	std::unordered_map<int, bool> m_mDidShoot = {};

	std::deque<CIncomingSequence> m_dSequences;
	int m_iLastInSequence = 0;
	int m_nOldInSequenceNr = 0;
	int m_nOldInReliableState = 0;
	int m_nLastInSequenceNr = 0;
	int m_nOldTickBase = 0;
	float m_flMaxUnlag = 1.f;

	float m_flFakeLatency = 0.f;
	float m_flFakeInterp = 0.015f;

	struct CrosshairRecordInfo_t
	{
		float m_flMinDist = -1.f;
		float m_flFov = -1.f;
		bool m_bInsideThisRecord = false; // If our shoot pos is inside this record
	};

private:
	bool m_bSettingUpBones = false;

public:
	void Store();
	void SendLerp();
	void Draw(CTFPlayer* pLocal);
	void Reset();

	bool GetRecords(CBaseEntity* pEntity, std::vector<TickRecord*>& vReturn);
	std::vector<TickRecord*> GetValidRecords(std::vector<TickRecord*>& vRecords, CTFPlayer* pLocal = nullptr, bool bDistance = false, float flTimeMod = 0.f);

	float GetReal(int iFlow = MAX_FLOWS, bool bNoFake = true);
	float GetWishFake();
	float GetWishLerp();
	float GetFakeLatency();
	float GetFakeInterp();
	float GetWindow();
	void SetLerp(IGameEvent* pEvent);
	int GetAnticipatedChoke(int iMethod = Vars::Aimbot::General::AimType.Value);

	void ResolverUpdate(CBaseEntity* pEntity);
	void ReportShot(int iIndex);
	void AdjustPing(CNetChannel* netChannel);
	void RestorePing(CNetChannel* netChannel);

	// Crosshair backtrack - adjust tick when manually aiming
	std::optional<TickRecord> GetHitRecord(CBaseEntity* pEntity, CTFWeaponBase* pWeapon, CUserCmd* pCmd, CrosshairRecordInfo_t& InfoOut, const Vec3 vAngles, const Vec3 vPos);
	void BacktrackToCrosshair(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd);

	int m_iTickCount = 0;
	float m_flSentInterp = -1.f;

	TickRecord m_tRecord = {}; // for temporary use
};

ADD_FEATURE(CBacktrack, Backtrack);