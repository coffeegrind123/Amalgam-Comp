#include "WarpPrediction.h"

namespace WarpPrediction {
    static inline float Phi() { return 1.6180339887f; }
    static inline float Cross2D(const Vec3& a, const Vec3& b) { return fabsf(a.x * b.y - a.y * b.x); }

    bool ShouldPredict(CTFPlayer* target) {
        if (!target) return false;
        return H::Entities.GetLagCompensation(target->entindex());
    }

    Vec3 PredictDelta(CTFPlayer* target, const Vec3& eyePos, const Vec3& origin) {
        Vec3 v = target ? target->m_vecVelocity() : Vec3();
        Vec3 v2d = { v.x, v.y, 0.f };
        float speed = v2d.Length();
        if (speed <= 0.f) return {};

        Vec3 dir = (origin - eyePos);
        dir.z = 0.f;
        if (dir.IsZero()) return {};
        dir.Normalize();
        Vec3 vNorm = v2d / speed;

        float d = std::clamp(dir.Dot(vNorm), -1.f, 1.f);
        float c = Cross2D(dir, vNorm);

        float w = sqrtf(std::max(eyePos.DistTo(origin), 0.f));
        float dt = std::clamp((Phi() * w) / std::max(speed, 1.f), TICK_INTERVAL, 0.5f);
        float scale = std::clamp(0.5f + 0.5f * d + 0.25f * c, 0.f, 1.5f);
        return v * (dt * scale);
    }
}
