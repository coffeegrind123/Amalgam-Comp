#pragma once
#include "../../../../SDK/SDK.h"

namespace WarpPrediction {
    bool ShouldPredict(CTFPlayer* target);
    Vec3 PredictDelta(CTFPlayer* target, const Vec3& eyePos, const Vec3& origin);
    inline Vec3 PredictPos(CTFPlayer* target, const Vec3& eyePos, const Vec3& origin) { return origin + PredictDelta(target, eyePos, origin); }
}
