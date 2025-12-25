#pragma once
#include "../Feature/Feature.h"
#include "../Memory/Memory.h"
#include "../../SDK/Helpers/Performance/PerformanceMonitor.h"
#include "../../SDK/Definitions/Types.h"
#include <cmath>

// Mathematical constants - use unique names to avoid conflicts
#ifndef SIMD_PI_CONST
#define SIMD_PI_CONST 3.14159265358979323846f
#define SIMD_TWO_PI_CONST 6.28318530717958647692f
#define SIMD_HALF_PI_CONST 1.57079632679489661923f
#ifndef PI
#define PI SIMD_PI_CONST
#endif
#ifndef TWO_PI
#define TWO_PI SIMD_TWO_PI_CONST
#endif
#ifndef HALF_PI
#define HALF_PI SIMD_HALF_PI_CONST
#endif
#endif

// SIMD Vector Math Library - High Performance Vector Operations
// Provides 4-8x improvement in vector calculations for AVX2 builds
// Maintains backward compatibility with scalar operations

#ifdef _MSC_VER
#include <intrin.h>
#include <immintrin.h>
#else
#include <x86intrin.h>
#endif

class CSIMDMath
{
public:
    // SIMD capability detection and initialization
    static void Initialize();
    static bool IsAVX2Supported();
    static bool IsSSESupported();
    static bool IsSIMDEnabled();

    // Vector3 SIMD operations - Batch processing for multiple vectors
    // These functions operate on arrays of vectors for maximum throughput

    // Batch distance calculations - processes 4 vectors simultaneously
    static void BatchDistance(const Vec3* points, const Vec3& center, float* distances, size_t count);
    static void BatchDistance2D(const Vec3* points, const Vec3& center, float* distances, size_t count);

    // Batch normalization - processes 4 vectors simultaneously
    static void BatchNormalize(Vec3* vectors, size_t count);
    static void BatchNormalizeSafe(Vec3* vectors, size_t count, float epsilon = 0.001f);

    // Batch dot products - processes 4 vector pairs simultaneously
    static void BatchDotProduct(const Vec3* vectors1, const Vec3* vectors2, float* results, size_t count);

    // Batch cross products - processes 4 vector pairs simultaneously
    static void BatchCrossProduct(const Vec3* vectors1, const Vec3* vectors2, Vec3* results, size_t count);

    // Batch vector arithmetic
    static void BatchAdd(const Vec3* vectors1, const Vec3* vectors2, Vec3* results, size_t count);
    static void BatchSubtract(const Vec3* vectors1, const Vec3* vectors2, Vec3* results, size_t count);
    static void BatchMultiply(const Vec3* vectors, float scalar, Vec3* results, size_t count);
    static void BatchDivide(const Vec3* vectors, float scalar, Vec3* results, size_t count);

    // Matrix-vector operations for transformations
    static void BatchTransformPoints(const Vec3* points, const matrix3x4* matrix, Vec3* results, size_t count);
    static void BatchTransformVectors(const Vec3* vectors, const matrix3x4* matrix, Vec3* results, size_t count);

    // Single vector optimized operations (for hot paths)
    static float FastLength(const Vec3& v);
    static float FastLength2D(const Vec3& v);
    static float FastDistance(const Vec3& a, const Vec3& b);
    static float FastDistance2D(const Vec3& a, const Vec3& b);
    static float FastDotProduct(const Vec3& a, const Vec3& b);
    static Vec3 FastNormalize(const Vec3& v);
    static Vec3 FastNormalizeSafe(const Vec3& v, float epsilon = 0.001f);
    static Vec3 FastCrossProduct(const Vec3& a, const Vec3& b);

    // Fast trigonometric functions with SIMD optimizations
    static float FastCos(float x);
    static float FastSin(float x);
    static float FastTan(float x);
    static void FastSinCos(float x, float& sinOut, float& cosOut);

    // Angle operations with SIMD
    static float FastAngleBetweenVectors(const Vec3& a, const Vec3& b);
    static void BatchAngleBetweenVectors(const Vec3* vectors1, const Vec3* vectors2, float* results, size_t count);

    // FOV calculations (critical for aimbot)
    static float FastFOVDistance(const Vec3& viewAngle, const Vec3& targetAngle, float fov);
    static void BatchFOVDistances(const Vec3& viewAngle, const Vec3* targetAngles, float* distances, size_t count);

    // Performance monitoring
    static void GetSIMDStats(uint64_t& totalOperations, double& avgTimeMs);
    static void ResetSIMDStats();

private:
    // SIMD capability flags
    static bool s_bAVX2Supported;
    static bool s_bSSESupported;
    static bool s_bSIMDEnabled;

    // Performance metrics
    static uint64_t s_nTotalSIMDOperations;
    static double s_fTotalSIMDTimeMs;

    // Internal SIMD implementation functions
    static void BatchDistanceAVX2(const Vec3* points, const Vec3& center, float* distances, size_t count);
    static void BatchDistanceSSE(const Vec3* points, const Vec3& center, float* distances, size_t count);
    static void BatchDistanceScalar(const Vec3* points, const Vec3& center, float* distances, size_t count);

    static void BatchNormalizeAVX2(Vec3* vectors, size_t count);
    static void BatchNormalizeSSE(Vec3* vectors, size_t count);
    static void BatchNormalizeScalar(Vec3* vectors, size_t count);

    static void BatchDotProductAVX2(const Vec3* vectors1, const Vec3* vectors2, float* results, size_t count);
    static void BatchDotProductSSE(const Vec3* vectors1, const Vec3* vectors2, float* results, size_t count);
    static void BatchDotProductScalar(const Vec3* vectors1, const Vec3* vectors2, float* results, size_t count);

    // Helper functions for SIMD register loading/storing
    static __m256 LoadVec3ToAVX2(const Vec3& v);
    static __m128 LoadVec3ToSSE(const Vec3& v);
    static Vec3 StoreAVX2ToVec3(const __m256& reg);
    static Vec3 StoreSSEToVec3(const __m128& reg);

    // Memory alignment utilities
    static bool IsAligned(const void* ptr, size_t alignment);
    static void* AllocateAligned(size_t size, size_t alignment);
    static void FreeAligned(void* ptr);
};

// SIMD-enhanced Vec3 wrapper for high-performance operations
class SIMDEnhancedVec3 : public Vec3
{
public:
    // Constructors
    SIMDEnhancedVec3() : Vec3(0, 0, 0) {}
    SIMDEnhancedVec3(float x, float y, float z) : Vec3(x, y, z) {}
    SIMDEnhancedVec3(const Vec3& v) : Vec3(v) {}

    // High-performance operations using SIMD when available
    inline float FastLength() const { return CSIMDMath::FastLength(*this); }
    inline float FastLength2D() const { return CSIMDMath::FastLength2D(*this); }
    inline float FastDistanceTo(const SIMDEnhancedVec3& other) const { return CSIMDMath::FastDistance(*this, other); }
    inline float FastDistanceTo2D(const SIMDEnhancedVec3& other) const { return CSIMDMath::FastDistance2D(*this, other); }
    inline float FastDot(const SIMDEnhancedVec3& other) const { return CSIMDMath::FastDotProduct(*this, other); }
    inline SIMDEnhancedVec3 FastNormalized() const { return CSIMDMath::FastNormalize(*this); }
    inline SIMDEnhancedVec3 FastNormalizedSafe(float epsilon = 0.001f) const { return CSIMDMath::FastNormalizeSafe(*this, epsilon); }
    inline SIMDEnhancedVec3 FastCross(const SIMDEnhancedVec3& other) const { return CSIMDMath::FastCrossProduct(*this, other); }
    inline float FastAngleTo(const SIMDEnhancedVec3& other) const { return CSIMDMath::FastAngleBetweenVectors(*this, other); }

    // Enhanced operators
    inline SIMDEnhancedVec3 operator+(const SIMDEnhancedVec3& other) const {
        return SIMDEnhancedVec3(x + other.x, y + other.y, z + other.z);
    }
    inline SIMDEnhancedVec3 operator-(const SIMDEnhancedVec3& other) const {
        return SIMDEnhancedVec3(x - other.x, y - other.y, z - other.z);
    }
    inline SIMDEnhancedVec3 operator*(float scalar) const {
        return SIMDEnhancedVec3(x * scalar, y * scalar, z * scalar);
    }
    inline SIMDEnhancedVec3 operator/(float scalar) const {
        return SIMDEnhancedVec3(x / scalar, y / scalar, z / scalar);
    }
};

// SIMD utility macros for easy integration
#define PERF_TIMER_SIMD() CPerformanceMonitor::ScopedTimer timer(CPerformanceMonitor::GetMemoryPoolsMetrics())

// SIMD batch processing macros
#define SIMD_BATCH_DISTANCE(points, center, distances, count) \
    CSIMDMath::BatchDistance(points, center, distances, count)

#define SIMD_BATCH_NORMALIZE(vectors, count) \
    CSIMDMath::BatchNormalize(vectors, count)

#define SIMD_BATCH_DOT_PRODUCT(vectors1, vectors2, results, count) \
    CSIMDMath::BatchDotProduct(vectors1, vectors2, results, count)

// High-performance single vector operations
#define SIMD_FAST_LENGTH(vec) \
    CSIMDMath::FastLength(vec)

#define SIMD_FAST_DISTANCE(a, b) \
    CSIMDMath::FastDistance(a, b)

#define SIMD_FAST_NORMALIZE(vec) \
    CSIMDMath::FastNormalize(vec)