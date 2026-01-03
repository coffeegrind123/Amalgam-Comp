#pragma once
#include "BaseTypes.h"
#include "../Types.h"

typedef int SideType;

#define	SIDE_FRONT 0
#define	SIDE_BACK 1
#define	SIDE_ON 2

#define VP_EPSILON 0.01f

class VPlane
{
public:
	Vector m_Normal;
	vec_t m_Dist;

	// OPTIMIZED: Default constructor ensures initialized state, prevents undefined behavior
	VPlane() : m_Normal(0.0f, 0.0f, 0.0f), m_Dist(0.0f) {}

	// OPTIMIZED: Parameterized constructor using initializer list for efficiency
	VPlane(const Vector& normal, vec_t dist) : m_Normal(normal), m_Dist(dist) {}

	// OPTIMIZED: Direct construction from components avoids temporary Vector object
	VPlane(vec_t nx, vec_t ny, vec_t nz, vec_t dist) : m_Normal(nx, ny, nz), m_Dist(dist) {}

	// OPTIMIZED: Inline distance calculation, marked const for correctness
	inline vec_t DistTo(const Vector& pt) const {
		return m_Normal.Dot(pt) - m_Dist;
	}

	// SAFETY: Safe side determination with epsilon tolerance to prevent floating-point errors
	inline SideType GetSide(const Vector& pt, vec_t epsilon = VP_EPSILON) const {
		vec_t dist = DistTo(pt);
		if (dist > epsilon) {
			return SIDE_FRONT;
		}
		if (dist < -epsilon) {
			return SIDE_BACK;
		}
		return SIDE_ON; // SAFETY: Within epsilon tolerance
	}

	// OPTIMIZED: Inline point projection onto plane, const-correct
	inline Vector Project(const Vector& pt) const {
		vec_t dist = DistTo(pt);
		return pt - (m_Normal * dist); // OPTIMIZED: Direct calculation avoids intermediate variables
	}

	// SAFETY: Safe normalization with zero-length check to prevent division by zero
	inline void Normalize() {
		vec_t length = m_Normal.Length();
		if (length > 0.0001f) { // FIXED: Prevent divide-by-zero crash
			vec_t invLength = 1.0f / length;
			m_Normal *= invLength;
			m_Dist *= invLength;
		}
		// SAFETY: If length near zero, plane remains unchanged (safe default behavior)
	}

	// OPTIMIZED: Inline validity check, marked const
	inline bool IsValid() const {
		// SAFETY: Check for NaN/Inf values that could cause crashes in calculations
		return m_Normal.IsValid() && !isnan(m_Dist) && !isinf(m_Dist);
	}
};