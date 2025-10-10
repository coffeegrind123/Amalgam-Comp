#pragma once
#include "../../../SDK/SDK.h"

class CMedicUberBar
{
private:
    // Helper functions
    Color_t GetUberBarColor(float uberPercentage, bool isAlive);
    void DrawUberBar(int x, int y, int width, float uberPercentage, bool isAlive);
    bool IsVisible(const Vec3& vPos);
    std::string GetWeaponType(int iItemDefinitionIndex);
    float GetUberPercentage(CTFPlayer* pMedic);

public:
    void Draw();
};

ADD_FEATURE(CMedicUberBar, MedicUberBar);