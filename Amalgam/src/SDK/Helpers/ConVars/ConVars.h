#pragma once
#include "../../../Utils/Feature/Feature.h"
#include "../../Definitions/Misc/ConVar.h"
#include "../../../Utils/Hash/FNV1A.h"
#include <unordered_map>

class CConVars
{
private:
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
