#pragma once
#include "../Feature/Feature.h"
#include <Windows.h>

class CCrashLog
{
public:
    void Initialize(LPVOID lpParam = nullptr);
    void Unload();
};

ADD_FEATURE_CUSTOM(CCrashLog, CrashLog, U);
