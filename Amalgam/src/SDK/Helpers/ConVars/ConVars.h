#pragma once
<<<<<<< HEAD
#include "../../../Utils/Feature/Feature.h"
#include "../../Definitions/Misc/ConVar.h"
=======
#include "../../Definitions/Misc/ConVar.h"
#include "../../../Utils/Macros/Macros.h"
>>>>>>> upstream/master
#include "../../../Utils/Hash/FNV1A.h"
#include <unordered_map>

class CConVars
{
private:
<<<<<<< HEAD
	std::unordered_map<uint32_t, ConVar*> mCVarMap = {};
	std::unordered_map<ConCommandBase*, int> mFlagMap = {};

public:
	void Initialize();
	void Unload();
	ConVar* FindVar(const char* sCVar);
	void Unlock();
	void Restore();
};

ADD_FEATURE_CUSTOM(CConVars, ConVars, H);
=======
	std::unordered_map<uint32_t, ConVar*> m_mCVarMap = {};
	std::unordered_map<ConCommandBase*, int> m_mFlagMap = {};

	bool m_bUnlocked = false;

public:
	bool Unlock();
	bool Restore();
	bool Modify(bool bUnlock);

	ConVar* FindVar(const char* sCVar);
};

ADD_FEATURE_CUSTOM(CConVars, ConVars, H);
>>>>>>> upstream/master
