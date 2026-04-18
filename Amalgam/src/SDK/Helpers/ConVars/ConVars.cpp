#include "ConVars.h"

#include "../../Definitions/Interfaces/ICVar.h"

<<<<<<< HEAD
void CConVars::Initialize()
{
	ConCommandBase* pCmdBase = I::CVar->GetCommands();
	while (pCmdBase != nullptr)
	{
		mFlagMap[pCmdBase] = pCmdBase->m_nFlags;
		pCmdBase->m_nFlags &= ~(FCVAR_HIDDEN | FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT | FCVAR_NOT_CONNECTED);
		pCmdBase = pCmdBase->m_pNext;
	}
}

void CConVars::Unload()
{
	for (auto& [pCmdBase, nFlags] : mFlagMap)
	{
		if (pCmdBase)
			pCmdBase->m_nFlags = nFlags;
	}
=======
bool CConVars::Unlock()
{
	if (!m_bUnlocked)
	{
		ConCommandBase* pCmdBase = I::CVar->GetCommands();
		while (pCmdBase != nullptr)
		{
			m_mFlagMap[pCmdBase] = pCmdBase->m_nFlags;
			pCmdBase->m_nFlags &= ~(FCVAR_HIDDEN | FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT | FCVAR_NOT_CONNECTED);
			pCmdBase = pCmdBase->m_pNext;
		}
		m_bUnlocked = true;

		return true;
	}
	return false;
}

bool CConVars::Restore()
{
	if (m_bUnlocked)
	{
		for (auto& [pCmdBase, nFlags] : m_mFlagMap)
			pCmdBase->m_nFlags = nFlags;
		m_mFlagMap.clear();
		m_bUnlocked = false;

		return true;
	}
	return false;
}

bool CConVars::Modify(bool bUnlock)
{
	if (bUnlock == m_bUnlocked)
		return false;

	if (bUnlock)
		return Unlock();
	else
		return Restore();
>>>>>>> upstream/master
}

ConVar* CConVars::FindVar(const char* sCVar)
{
	auto uHash = FNV1A::Hash32(sCVar);
<<<<<<< HEAD
	if (!mCVarMap.contains(uHash))
		mCVarMap[uHash] = I::CVar->FindVar(sCVar);
	return mCVarMap[uHash];
}

void CConVars::Unlock()
{
	ConCommandBase* pCmdBase = I::CVar->GetCommands();
	while (pCmdBase != nullptr)
	{
		pCmdBase->m_nFlags &= ~(FCVAR_HIDDEN | FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT | FCVAR_NOT_CONNECTED);
		pCmdBase = pCmdBase->m_pNext;
	}
}

void CConVars::Restore()
{
	for (auto& [pCmdBase, nFlags] : mFlagMap)
	{
		if (pCmdBase)
			pCmdBase->m_nFlags = nFlags;
	}
}
=======
	if (!m_mCVarMap.contains(uHash))
		m_mCVarMap[uHash] = I::CVar->FindVar(sCVar);
	return m_mCVarMap[uHash];
}
>>>>>>> upstream/master
