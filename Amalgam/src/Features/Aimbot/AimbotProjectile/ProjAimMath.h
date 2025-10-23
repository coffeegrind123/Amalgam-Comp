#pragma once
#include "../../../SDK/SDK.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace ProjAimMath
{
	inline float RadToDeg(float rad)
	{
		return rad * (180.0f / M_PI);
	}

	inline float DegToRad(float deg)
	{
		return deg * (M_PI / 180.0f);
	}

	inline bool IsNaN(float x)
	{
		return x != x;
	}

	inline Vec3 PositionAngles(const Vec3& source, const Vec3& dest)
	{
		Vec3 delta = source - dest;
		float pitch = RadToDeg(atan2(delta.z, delta.Length2D()));
		float yaw = RadToDeg(atan2(delta.y, delta.x));

		if (delta.x >= 0.0f)
			yaw += 180.0f;

		if (IsNaN(pitch))
			pitch = 0.0f;
		if (IsNaN(yaw))
			yaw = 0.0f;

		return Vec3(pitch, yaw, 0.0f);
	}

	inline float AngleFov(const Vec3& vFrom, const Vec3& vTo)
	{
		Vec3 vSrcForward; Math::AngleVectors(vFrom, &vSrcForward);
		Vec3 vDstForward; Math::AngleVectors(vTo, &vDstForward);

		float flDot = vDstForward.Dot(vSrcForward);
		float flLengthSqr = vDstForward.Length();
		float fov = RadToDeg(acosf(flDot / (flLengthSqr * flLengthSqr)));

		if (IsNaN(fov))
			fov = 0.0f;

		return fov;
	}

	inline Vec3 NormalizeVector(const Vec3& vec)
	{
		float length = vec.Length();
		if (length > 0.0f)
			return vec / length;
		return Vec3(0, 0, 0);
	}

	inline bool SolveBallisticArc(const Vec3& p0, const Vec3& p1, float speed, float gravity, Vec3& outAngles, float& outTime)
	{
		Vec3 diff = p1 - p0;
		float dx = diff.Length2D();
		float dy = diff.z;
		float speed2 = speed * speed;
		float g = gravity;

		float root = speed2 * speed2 - g * (g * dx * dx + 2.0f * dy * speed2);
		if (root < 0.0f)
			return false; // No solution

		float sqrt_root = sqrtf(root);
		float angle = atan((speed2 - sqrt_root) / (g * dx)); // Low arc

		float yaw = RadToDeg(atan2(diff.y, diff.x));
		float pitch = -RadToDeg(angle); // Negative because upward is negative pitch

		float cosAngle = cosf(DegToRad(pitch));
		outTime = (cosAngle != 0.0f) ? (dx / (cosAngle * speed)) : 0.0f;

		outAngles = Vec3(pitch, yaw, 0.0f);
		return true;
	}

	inline bool SolveBallisticArcBoth(const Vec3& p0, const Vec3& p1, float speed, float gravity, Vec3& outLowArc, Vec3& outHighArc)
	{
		Vec3 diff = p1 - p0;
		float dx = sqrtf(diff.x * diff.x + diff.y * diff.y);
		if (dx == 0.0f)
			return false;

		float dy = diff.z;
		float g = gravity;
		float speed2 = speed * speed;

		float root = speed2 * speed2 - g * (g * dx * dx + 2.0f * dy * speed2);
		if (root < 0.0f)
			return false;

		float sqrt_root = sqrtf(root);
		float theta_low = atan((speed2 - sqrt_root) / (g * dx));
		float theta_high = atan((speed2 + sqrt_root) / (g * dx));

		float yaw = RadToDeg(atan2(diff.y, diff.x));
		float pitch_low = -RadToDeg(theta_low);
		float pitch_high = -RadToDeg(theta_high);

		outLowArc = Vec3(pitch_low, yaw, 0.0f);
		outHighArc = Vec3(pitch_high, yaw, 0.0f);
		return true;
	}

	inline float EstimateTravelTime(const Vec3& shootPos, const Vec3& targetPos, float speed)
	{
		float distance = (targetPos - shootPos).Length2D();
		return (speed > 0.0f) ? (distance / speed) : 0.0f;
	}

	inline float GetBallisticFlightTime(const Vec3& p0, const Vec3& p1, float speed, float gravity)
	{
		Vec3 diff = p1 - p0;
		float dx = sqrtf(diff.x * diff.x + diff.y * diff.y);
		float dy = diff.z;
		float speed2 = speed * speed;
		float g = gravity;

		float discriminant = speed2 * speed2 - g * (g * dx * dx + 2.0f * dy * speed2);
		if (discriminant < 0.0f)
			return 0.0f;

		float sqrt_discriminant = sqrtf(discriminant);
		float angle = atan((speed2 - sqrt_discriminant) / (g * dx));

		float vz = speed * sinf(angle);
		float flight_time = (vz + sqrtf(vz * vz + 2.0f * g * dy)) / g;

		return flight_time;
	}

	inline Vec3 DirectionToAngles(const Vec3& direction)
	{
		float pitch = RadToDeg(asinf(-direction.z));
		float yaw = RadToDeg(atan2(direction.y, direction.x));
		return Vec3(pitch, yaw, 0.0f);
	}

	inline Vec3 RotateOffsetAlongDirection(const Vec3& offset, const Vec3& direction)
	{
		Vec3 forward = NormalizeVector(direction);
		Vec3 up(0, 0, 1);
		Vec3 right = NormalizeVector(forward.Cross(up));
		up = NormalizeVector(right.Cross(forward));

		return forward * offset.x + right * offset.y + up * offset.z;
	}
}
