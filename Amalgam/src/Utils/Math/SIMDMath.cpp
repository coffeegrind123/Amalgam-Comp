#include "SIMDMath.h"
#include <cstring>
#include <algorithm>
#include <cmath>

// Static member definitions
bool CSIMDMath::s_bAVX2Supported = false;
bool CSIMDMath::s_bSSESupported = false;
bool CSIMDMath::s_bSIMDEnabled = false;
uint64_t CSIMDMath::s_nTotalSIMDOperations = 0;
double CSIMDMath::s_fTotalSIMDTimeMs = 0.0f;

void CSIMDMath::Initialize()
{
    PERF_TIMER_SIMD();

    // Detect SIMD capabilities at runtime
    if (IsAVX2Supported())
    {
        s_bAVX2Supported = true;
        s_bSSESupported = true;
        s_bSIMDEnabled = true;
    }
    else if (IsSSESupported())
    {
        s_bSSESupported = true;
        s_bSIMDEnabled = true;
    }
    else
    {
        s_bSIMDEnabled = false;
    }
}

bool CSIMDMath::IsAVX2Supported()
{
    if (s_bAVX2Supported) return true;

    int cpuInfo[4];
    __cpuid(cpuInfo, 0);

    if (cpuInfo[0] >= 7)
    {
        __cpuid(cpuInfo, 7);
        return (cpuInfo[1] & (1 << 5)) != 0; // AVX2 bit
    }

    return false;
}

bool CSIMDMath::IsSSESupported()
{
    if (s_bSSESupported) return true;

    int cpuInfo[4];
    __cpuid(cpuInfo, 1);
    return (cpuInfo[3] & (1 << 25)) != 0; // SSE bit
}

bool CSIMDMath::IsSIMDEnabled()
{
    return s_bSIMDEnabled;
}

void CSIMDMath::BatchDistance(const Vec3* points, const Vec3& center, float* distances, size_t count)
{
    PERF_TIMER_SIMD();

    if (!s_bSIMDEnabled || !points || !distances)
    {
        BatchDistanceScalar(points, center, distances, count);
        return;
    }

    if (s_bAVX2Supported && IsAligned(points, 32) && IsAligned(distances, 32))
    {
        BatchDistanceAVX2(points, center, distances, count);
    }
    else if (s_bSSESupported && IsAligned(points, 16) && IsAligned(distances, 16))
    {
        BatchDistanceSSE(points, center, distances, count);
    }
    else
    {
        BatchDistanceScalar(points, center, distances, count);
    }

    s_nTotalSIMDOperations += count;
}

void CSIMDMath::BatchDistance2D(const Vec3* points, const Vec3& center, float* distances, size_t count)
{
    PERF_TIMER_SIMD();

    if (!s_bSIMDEnabled || !points || !distances)
    {
        // Fallback to scalar implementation
        for (size_t i = 0; i < count; ++i)
        {
            float dx = points[i].x - center.x;
            float dy = points[i].y - center.y;
            distances[i] = sqrtf(dx * dx + dy * dy);
        }
        return;
    }

    // Process 4 vectors at a time with SSE
    size_t simdCount = count & ~3; // Round down to multiple of 4
    size_t remainder = count - simdCount;

    __m128 centerVec = _mm_set_ps(center.x, center.x, center.x, center.x);
    __m128 centerVec2 = _mm_set_ps(center.y, center.y, center.y, center.y);

    for (size_t i = 0; i < simdCount; i += 4)
    {
        // Load 4 vectors
        __m128 vx = _mm_set_ps(points[i + 3].x, points[i + 2].x, points[i + 1].x, points[i].x);
        __m128 vy = _mm_set_ps(points[i + 3].y, points[i + 2].y, points[i + 1].y, points[i].y);

        // Calculate differences
        __m128 dx = _mm_sub_ps(vx, centerVec);
        __m128 dy = _mm_sub_ps(vy, centerVec2);

        // Calculate squared distances
        __m128 dx2 = _mm_mul_ps(dx, dx);
        __m128 dy2 = _mm_mul_ps(dy, dy);
        __m128 dist2 = _mm_add_ps(dx2, dy2);

        // Square root
        __m128 dist = _mm_sqrt_ps(dist2);

        // Store results
        _mm_storeu_ps(&distances[i], dist);
    }

    // Handle remainder
    for (size_t i = simdCount; i < count; ++i)
    {
        float dx = points[i].x - center.x;
        float dy = points[i].y - center.y;
        distances[i] = sqrtf(dx * dx + dy * dy);
    }

    s_nTotalSIMDOperations += count;
}

void CSIMDMath::BatchNormalize(Vec3* vectors, size_t count)
{
    PERF_TIMER_SIMD();

    if (!s_bSIMDEnabled || !vectors)
    {
        BatchNormalizeScalar(vectors, count);
        return;
    }

    if (s_bAVX2Supported)
    {
        BatchNormalizeAVX2(vectors, count);
    }
    else if (s_bSSESupported)
    {
        BatchNormalizeSSE(vectors, count);
    }
    else
    {
        BatchNormalizeScalar(vectors, count);
    }

    s_nTotalSIMDOperations += count;
}

void CSIMDMath::BatchNormalizeSafe(Vec3* vectors, size_t count, float epsilon)
{
    PERF_TIMER_SIMD();

    // First normalize all vectors
    BatchNormalize(vectors, count);

    // Then check for zero-length vectors and handle them
    for (size_t i = 0; i < count; ++i)
    {
        float length = vectors[i].Length();
        if (length < epsilon)
        {
            // Set to default normalized vector (1, 0, 0)
            vectors[i] = Vec3(1.0f, 0.0f, 0.0f);
        }
    }
}

void CSIMDMath::BatchDotProduct(const Vec3* vectors1, const Vec3* vectors2, float* results, size_t count)
{
    PERF_TIMER_SIMD();

    if (!s_bSIMDEnabled || !vectors1 || !vectors2 || !results)
    {
        BatchDotProductScalar(vectors1, vectors2, results, count);
        return;
    }

    if (s_bAVX2Supported)
    {
        BatchDotProductAVX2(vectors1, vectors2, results, count);
    }
    else if (s_bSSESupported)
    {
        BatchDotProductSSE(vectors1, vectors2, results, count);
    }
    else
    {
        BatchDotProductScalar(vectors1, vectors2, results, count);
    }

    s_nTotalSIMDOperations += count;
}

void CSIMDMath::BatchCrossProduct(const Vec3* vectors1, const Vec3* vectors2, Vec3* results, size_t count)
{
    PERF_TIMER_SIMD();

    // Cross products are more complex to vectorize effectively
    // For now, use optimized scalar implementation
    for (size_t i = 0; i < count; ++i)
    {
        const Vec3& a = vectors1[i];
        const Vec3& b = vectors2[i];
        results[i] = Vec3(
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        );
    }

    s_nTotalSIMDOperations += count;
}

void CSIMDMath::BatchAdd(const Vec3* vectors1, const Vec3* vectors2, Vec3* results, size_t count)
{
    PERF_TIMER_SIMD();

    if (!s_bSSESupported || !vectors1 || !vectors2 || !results)
    {
        for (size_t i = 0; i < count; ++i)
        {
            results[i] = vectors1[i] + vectors2[i];
        }
        return;
    }

    size_t simdCount = count & ~3;

    for (size_t i = 0; i < simdCount; i += 4)
    {
        __m128 v1x = _mm_set_ps(vectors1[i + 3].x, vectors1[i + 2].x, vectors1[i + 1].x, vectors1[i].x);
        __m128 v1y = _mm_set_ps(vectors1[i + 3].y, vectors1[i + 2].y, vectors1[i + 1].y, vectors1[i].y);
        __m128 v1z = _mm_set_ps(vectors1[i + 3].z, vectors1[i + 2].z, vectors1[i + 1].z, vectors1[i].z);

        __m128 v2x = _mm_set_ps(vectors2[i + 3].x, vectors2[i + 2].x, vectors2[i + 1].x, vectors2[i].x);
        __m128 v2y = _mm_set_ps(vectors2[i + 3].y, vectors2[i + 2].y, vectors2[i + 1].y, vectors2[i].y);
        __m128 v2z = _mm_set_ps(vectors2[i + 3].z, vectors2[i + 2].z, vectors2[i + 1].z, vectors2[i].z);

        __m128 rx = _mm_add_ps(v1x, v2x);
        __m128 ry = _mm_add_ps(v1y, v2y);
        __m128 rz = _mm_add_ps(v1z, v2z);

        float rx_array[4], ry_array[4], rz_array[4];
        _mm_storeu_ps(rx_array, rx);
        _mm_storeu_ps(ry_array, ry);
        _mm_storeu_ps(rz_array, rz);

        for (int j = 0; j < 4; ++j)
        {
            results[i + j] = Vec3(rx_array[j], ry_array[j], rz_array[j]);
        }
    }

    // Handle remainder
    for (size_t i = simdCount; i < count; ++i)
    {
        results[i] = vectors1[i] + vectors2[i];
    }

    s_nTotalSIMDOperations += count;
}

void CSIMDMath::BatchSubtract(const Vec3* vectors1, const Vec3* vectors2, Vec3* results, size_t count)
{
    PERF_TIMER_SIMD();

    if (!s_bSSESupported || !vectors1 || !vectors2 || !results)
    {
        for (size_t i = 0; i < count; ++i)
        {
            results[i] = vectors1[i] - vectors2[i];
        }
        return;
    }

    size_t simdCount = count & ~3;

    for (size_t i = 0; i < simdCount; i += 4)
    {
        __m128 v1x = _mm_set_ps(vectors1[i + 3].x, vectors1[i + 2].x, vectors1[i + 1].x, vectors1[i].x);
        __m128 v1y = _mm_set_ps(vectors1[i + 3].y, vectors1[i + 2].y, vectors1[i + 1].y, vectors1[i].y);
        __m128 v1z = _mm_set_ps(vectors1[i + 3].z, vectors1[i + 2].z, vectors1[i + 1].z, vectors1[i].z);

        __m128 v2x = _mm_set_ps(vectors2[i + 3].x, vectors2[i + 2].x, vectors2[i + 1].x, vectors2[i].x);
        __m128 v2y = _mm_set_ps(vectors2[i + 3].y, vectors2[i + 2].y, vectors2[i + 1].y, vectors2[i].y);
        __m128 v2z = _mm_set_ps(vectors2[i + 3].z, vectors2[i + 2].z, vectors2[i + 1].z, vectors2[i].z);

        __m128 rx = _mm_sub_ps(v1x, v2x);
        __m128 ry = _mm_sub_ps(v1y, v2y);
        __m128 rz = _mm_sub_ps(v1z, v2z);

        float rx_array[4], ry_array[4], rz_array[4];
        _mm_storeu_ps(rx_array, rx);
        _mm_storeu_ps(ry_array, ry);
        _mm_storeu_ps(rz_array, rz);

        for (int j = 0; j < 4; ++j)
        {
            results[i + j] = Vec3(rx_array[j], ry_array[j], rz_array[j]);
        }
    }

    // Handle remainder
    for (size_t i = simdCount; i < count; ++i)
    {
        results[i] = vectors1[i] - vectors2[i];
    }

    s_nTotalSIMDOperations += count;
}

void CSIMDMath::BatchMultiply(const Vec3* vectors, float scalar, Vec3* results, size_t count)
{
    PERF_TIMER_SIMD();

    if (!s_bSSESupported || !vectors || !results)
    {
        for (size_t i = 0; i < count; ++i)
        {
            results[i] = vectors[i] * scalar;
        }
        return;
    }

    __m128 scalarVec = _mm_set_ps(scalar, scalar, scalar, scalar);
    size_t simdCount = count & ~3;

    for (size_t i = 0; i < simdCount; i += 4)
    {
        __m128 vx = _mm_set_ps(vectors[i + 3].x, vectors[i + 2].x, vectors[i + 1].x, vectors[i].x);
        __m128 vy = _mm_set_ps(vectors[i + 3].y, vectors[i + 2].y, vectors[i + 1].y, vectors[i].y);
        __m128 vz = _mm_set_ps(vectors[i + 3].z, vectors[i + 2].z, vectors[i + 1].z, vectors[i].z);

        __m128 rx = _mm_mul_ps(vx, scalarVec);
        __m128 ry = _mm_mul_ps(vy, scalarVec);
        __m128 rz = _mm_mul_ps(vz, scalarVec);

        float rx_array[4], ry_array[4], rz_array[4];
        _mm_storeu_ps(rx_array, rx);
        _mm_storeu_ps(ry_array, ry);
        _mm_storeu_ps(rz_array, rz);

        for (int j = 0; j < 4; ++j)
        {
            results[i + j] = Vec3(rx_array[j], ry_array[j], rz_array[j]);
        }
    }

    // Handle remainder
    for (size_t i = simdCount; i < count; ++i)
    {
        results[i] = vectors[i] * scalar;
    }

    s_nTotalSIMDOperations += count;
}

void CSIMDMath::BatchDivide(const Vec3* vectors, float scalar, Vec3* results, size_t count)
{
    PERF_TIMER_SIMD();

    if (!s_bSSESupported || !vectors || !results || scalar == 0.0f)
    {
        for (size_t i = 0; i < count; ++i)
        {
            results[i] = vectors[i] / scalar;
        }
        return;
    }

    __m128 scalarVec = _mm_set_ps(scalar, scalar, scalar, scalar);
    size_t simdCount = count & ~3;

    for (size_t i = 0; i < simdCount; i += 4)
    {
        __m128 vx = _mm_set_ps(vectors[i + 3].x, vectors[i + 2].x, vectors[i + 1].x, vectors[i].x);
        __m128 vy = _mm_set_ps(vectors[i + 3].y, vectors[i + 2].y, vectors[i + 1].y, vectors[i].y);
        __m128 vz = _mm_set_ps(vectors[i + 3].z, vectors[i + 2].z, vectors[i + 1].z, vectors[i].z);

        __m128 rx = _mm_div_ps(vx, scalarVec);
        __m128 ry = _mm_div_ps(vy, scalarVec);
        __m128 rz = _mm_div_ps(vz, scalarVec);

        float rx_array[4], ry_array[4], rz_array[4];
        _mm_storeu_ps(rx_array, rx);
        _mm_storeu_ps(ry_array, ry);
        _mm_storeu_ps(rz_array, rz);

        for (int j = 0; j < 4; ++j)
        {
            results[i + j] = Vec3(rx_array[j], ry_array[j], rz_array[j]);
        }
    }

    // Handle remainder
    for (size_t i = simdCount; i < count; ++i)
    {
        results[i] = vectors[i] / scalar;
    }

    s_nTotalSIMDOperations += count;
}

// Single vector optimized operations
float CSIMDMath::FastLength(const Vec3& v)
{
    PERF_TIMER_SIMD();

    if (s_bSSESupported)
    {
        __m128 vec = _mm_set_ps(0, v.z, v.y, v.x);
        __m128 squared = _mm_mul_ps(vec, vec);
        __m128 sum = _mm_hadd_ps(squared, squared);
        sum = _mm_hadd_ps(sum, sum);
        float result;
        _mm_store_ss(&result, _mm_sqrt_ps(sum));
        return result;
    }
    else
    {
        return v.Length();
    }
}

float CSIMDMath::FastLength2D(const Vec3& v)
{
    PERF_TIMER_SIMD();

    if (s_bSSESupported)
    {
        __m128 vec = _mm_set_ps(0, 0, v.y, v.x);
        __m128 squared = _mm_mul_ps(vec, vec);
        __m128 sum = _mm_hadd_ps(squared, squared);
        sum = _mm_hadd_ps(sum, sum);
        float result;
        _mm_store_ss(&result, _mm_sqrt_ps(sum));
        return result;
    }
    else
    {
        return v.Length2D();
    }
}

float CSIMDMath::FastDistance(const Vec3& a, const Vec3& b)
{
    PERF_TIMER_SIMD();

    if (s_bSSESupported)
    {
        __m128 va = _mm_set_ps(0, a.z, a.y, a.x);
        __m128 vb = _mm_set_ps(0, b.z, b.y, b.x);
        __m128 diff = _mm_sub_ps(va, vb);
        __m128 squared = _mm_mul_ps(diff, diff);
        __m128 sum = _mm_hadd_ps(squared, squared);
        sum = _mm_hadd_ps(sum, sum);
        float result;
        _mm_store_ss(&result, _mm_sqrt_ps(sum));
        return result;
    }
    else
    {
        return a.DistTo(b);
    }
}

float CSIMDMath::FastDistance2D(const Vec3& a, const Vec3& b)
{
    PERF_TIMER_SIMD();

    if (s_bSSESupported)
    {
        __m128 va = _mm_set_ps(0, 0, a.y, a.x);
        __m128 vb = _mm_set_ps(0, 0, b.y, b.x);
        __m128 diff = _mm_sub_ps(va, vb);
        __m128 squared = _mm_mul_ps(diff, diff);
        __m128 sum = _mm_hadd_ps(squared, squared);
        sum = _mm_hadd_ps(sum, sum);
        float result;
        _mm_store_ss(&result, _mm_sqrt_ps(sum));
        return result;
    }
    else
    {
        return a.DistTo2D(b);
    }
}

float CSIMDMath::FastDotProduct(const Vec3& a, const Vec3& b)
{
    PERF_TIMER_SIMD();

    if (s_bSSESupported)
    {
        __m128 va = _mm_set_ps(0, a.z, a.y, a.x);
        __m128 vb = _mm_set_ps(0, b.z, b.y, b.x);
        __m128 mul = _mm_mul_ps(va, vb);
        __m128 sum = _mm_hadd_ps(mul, mul);
        sum = _mm_hadd_ps(sum, sum);
        float result;
        _mm_store_ss(&result, sum);
        return result;
    }
    else
    {
        return a.Dot(b);
    }
}

Vec3 CSIMDMath::FastNormalize(const Vec3& v)
{
    PERF_TIMER_SIMD();

    float length = FastLength(v);
    if (length == 0.0f) return Vec3(1, 0, 0);

    return v / length;
}

Vec3 CSIMDMath::FastNormalizeSafe(const Vec3& v, float epsilon)
{
    PERF_TIMER_SIMD();

    float length = FastLength(v);
    if (length < epsilon) return Vec3(1, 0, 0);

    return v / length;
}

Vec3 CSIMDMath::FastCrossProduct(const Vec3& a, const Vec3& b)
{
    PERF_TIMER_SIMD();

    return Vec3(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    );
}

float CSIMDMath::FastAngleBetweenVectors(const Vec3& a, const Vec3& b)
{
    PERF_TIMER_SIMD();

    float dot = FastDotProduct(a, b);
    float lengthA = FastLength(a);
    float lengthB = FastLength(b);

    if (lengthA == 0.0f || lengthB == 0.0f) return 0.0f;

    float cosAngle = dot / (lengthA * lengthB);
    cosAngle = (std::max)(-1.0f, (std::min)(1.0f, cosAngle));

    return acosf(cosAngle) * (180.0f / 3.14159265358979323846f);
}

void CSIMDMath::BatchAngleBetweenVectors(const Vec3* vectors1, const Vec3* vectors2, float* results, size_t count)
{
    PERF_TIMER_SIMD();

    for (size_t i = 0; i < count; ++i)
    {
        results[i] = FastAngleBetweenVectors(vectors1[i], vectors2[i]);
    }

    s_nTotalSIMDOperations += count;
}

float CSIMDMath::FastFOVDistance(const Vec3& viewAngle, const Vec3& targetAngle, float fov)
{
    PERF_TIMER_SIMD();

    float angleDiff = FastAngleBetweenVectors(viewAngle, targetAngle);
    return angleDiff / fov;
}

void CSIMDMath::BatchFOVDistances(const Vec3& viewAngle, const Vec3* targetAngles, float* distances, size_t count)
{
    PERF_TIMER_SIMD();

    for (size_t i = 0; i < count; ++i)
    {
        distances[i] = FastFOVDistance(viewAngle, targetAngles[i], 180.0f); // Default FOV for normalization
    }

    s_nTotalSIMDOperations += count;
}

void CSIMDMath::BatchTransformPoints(const Vec3* points, const matrix3x4* matrix, Vec3* results, size_t count)
{
    PERF_TIMER_SIMD();

    // For now, use scalar implementation - matrix operations are complex to vectorize effectively
    for (size_t i = 0; i < count; ++i)
    {
        const Vec3& p = points[i];
        const matrix3x4& m = matrix[i];

        results[i] = Vec3(
            p.x * m[0][0] + p.y * m[0][1] + p.z * m[0][2] + m[0][3],
            p.x * m[1][0] + p.y * m[1][1] + p.z * m[1][2] + m[1][3],
            p.x * m[2][0] + p.y * m[2][1] + p.z * m[2][2] + m[2][3]
        );
    }

    s_nTotalSIMDOperations += count;
}

void CSIMDMath::BatchTransformVectors(const Vec3* vectors, const matrix3x4* matrix, Vec3* results, size_t count)
{
    PERF_TIMER_SIMD();

    // For now, use scalar implementation - matrix operations are complex to vectorize effectively
    for (size_t i = 0; i < count; ++i)
    {
        const Vec3& v = vectors[i];
        const matrix3x4& m = matrix[i];

        results[i] = Vec3(
            v.x * m[0][0] + v.y * m[0][1] + v.z * m[0][2],
            v.x * m[1][0] + v.y * m[1][1] + v.z * m[1][2],
            v.x * m[2][0] + v.y * m[2][1] + v.z * m[2][2]
        );
    }

    s_nTotalSIMDOperations += count;
}

// Performance monitoring
void CSIMDMath::GetSIMDStats(uint64_t& totalOperations, double& avgTimeMs)
{
    totalOperations = s_nTotalSIMDOperations;
    avgTimeMs = s_nTotalSIMDOperations > 0 ? s_fTotalSIMDTimeMs / s_nTotalSIMDOperations : 0.0;
}

void CSIMDMath::ResetSIMDStats()
{
    s_nTotalSIMDOperations = 0;
    s_fTotalSIMDTimeMs = 0.0;
}

// Internal SIMD implementation functions
void CSIMDMath::BatchDistanceAVX2(const Vec3* points, const Vec3& center, float* distances, size_t count)
{
    // AVX2 implementation - process 8 vectors at a time
    size_t avxCount = count & ~7; // Round down to multiple of 8
    size_t remainder = count - avxCount;

    __m256 centerVecX = _mm256_set1_ps(center.x);
    __m256 centerVecY = _mm256_set1_ps(center.y);
    __m256 centerVecZ = _mm256_set1_ps(center.z);

    for (size_t i = 0; i < avxCount; i += 8)
    {
        // Load 8 vectors
        __m256 vx = _mm256_set_ps(
            points[i + 7].x, points[i + 6].x, points[i + 5].x, points[i + 4].x,
            points[i + 3].x, points[i + 2].x, points[i + 1].x, points[i].x
        );
        __m256 vy = _mm256_set_ps(
            points[i + 7].y, points[i + 6].y, points[i + 5].y, points[i + 4].y,
            points[i + 3].y, points[i + 2].y, points[i + 1].y, points[i].y
        );
        __m256 vz = _mm256_set_ps(
            points[i + 7].z, points[i + 6].z, points[i + 5].z, points[i + 4].z,
            points[i + 3].z, points[i + 2].z, points[i + 1].z, points[i].z
        );

        // Calculate differences
        __m256 dx = _mm256_sub_ps(vx, centerVecX);
        __m256 dy = _mm256_sub_ps(vy, centerVecY);
        __m256 dz = _mm256_sub_ps(vz, centerVecZ);

        // Calculate squared distances
        __m256 dx2 = _mm256_mul_ps(dx, dx);
        __m256 dy2 = _mm256_mul_ps(dy, dy);
        __m256 dz2 = _mm256_mul_ps(dz, dz);
        __m256 dist2 = _mm256_add_ps(_mm256_add_ps(dx2, dy2), dz2);

        // Square root
        __m256 dist = _mm256_sqrt_ps(dist2);

        // Store results
        _mm256_storeu_ps(&distances[i], dist);
    }

    // Handle remainder
    for (size_t i = avxCount; i < count; ++i)
    {
        distances[i] = (points[i] - center).Length();
    }
}

void CSIMDMath::BatchDistanceSSE(const Vec3* points, const Vec3& center, float* distances, size_t count)
{
    // SSE implementation - process 4 vectors at a time
    size_t sseCount = count & ~3; // Round down to multiple of 4
    size_t remainder = count - sseCount;

    __m128 centerVecX = _mm_set_ps(center.x, center.x, center.x, center.x);
    __m128 centerVecY = _mm_set_ps(center.y, center.y, center.y, center.y);
    __m128 centerVecZ = _mm_set_ps(center.z, center.z, center.z, center.z);

    for (size_t i = 0; i < sseCount; i += 4)
    {
        // Load 4 vectors
        __m128 vx = _mm_set_ps(points[i + 3].x, points[i + 2].x, points[i + 1].x, points[i].x);
        __m128 vy = _mm_set_ps(points[i + 3].y, points[i + 2].y, points[i + 1].y, points[i].y);
        __m128 vz = _mm_set_ps(points[i + 3].z, points[i + 2].z, points[i + 1].z, points[i].z);

        // Calculate differences
        __m128 dx = _mm_sub_ps(vx, centerVecX);
        __m128 dy = _mm_sub_ps(vy, centerVecY);
        __m128 dz = _mm_sub_ps(vz, centerVecZ);

        // Calculate squared distances
        __m128 dx2 = _mm_mul_ps(dx, dx);
        __m128 dy2 = _mm_mul_ps(dy, dy);
        __m128 dz2 = _mm_mul_ps(dz, dz);
        __m128 dist2 = _mm_add_ps(_mm_add_ps(dx2, dy2), dz2);

        // Square root
        __m128 dist = _mm_sqrt_ps(dist2);

        // Store results
        _mm_storeu_ps(&distances[i], dist);
    }

    // Handle remainder
    for (size_t i = sseCount; i < count; ++i)
    {
        distances[i] = (points[i] - center).Length();
    }
}

void CSIMDMath::BatchDistanceScalar(const Vec3* points, const Vec3& center, float* distances, size_t count)
{
    // Scalar fallback implementation
    for (size_t i = 0; i < count; ++i)
    {
        distances[i] = (points[i] - center).Length();
    }
}

void CSIMDMath::BatchNormalizeAVX2(Vec3* vectors, size_t count)
{
    // AVX2 normalization implementation
    size_t avxCount = count & ~7; // Round down to multiple of 8

    for (size_t i = 0; i < avxCount; i += 8)
    {
        // Load 8 vectors
        __m256 vx = _mm256_set_ps(
            vectors[i + 7].x, vectors[i + 6].x, vectors[i + 5].x, vectors[i + 4].x,
            vectors[i + 3].x, vectors[i + 2].x, vectors[i + 1].x, vectors[i].x
        );
        __m256 vy = _mm256_set_ps(
            vectors[i + 7].y, vectors[i + 6].y, vectors[i + 5].y, vectors[i + 4].y,
            vectors[i + 3].y, vectors[i + 2].y, vectors[i + 1].y, vectors[i].y
        );
        __m256 vz = _mm256_set_ps(
            vectors[i + 7].z, vectors[i + 6].z, vectors[i + 5].z, vectors[i + 4].z,
            vectors[i + 3].z, vectors[i + 2].z, vectors[i + 1].z, vectors[i].z
        );

        // Calculate squared length
        __m256 length2 = _mm256_add_ps(_mm256_add_ps(
            _mm256_mul_ps(vx, vx),
            _mm256_mul_ps(vy, vy)
        ), _mm256_mul_ps(vz, vz));

        // Calculate reciprocal square root (faster than sqrt + divide)
        __m256 rcpLength = _mm256_rsqrt_ps(length2);

        // Normalize vectors
        __m256 nx = _mm256_mul_ps(vx, rcpLength);
        __m256 ny = _mm256_mul_ps(vy, rcpLength);
        __m256 nz = _mm256_mul_ps(vz, rcpLength);

        // Store results
        float nx_array[8], ny_array[8], nz_array[8];
        _mm256_storeu_ps(nx_array, nx);
        _mm256_storeu_ps(ny_array, ny);
        _mm256_storeu_ps(nz_array, nz);

        for (int j = 0; j < 8; ++j)
        {
            vectors[i + j] = Vec3(nx_array[j], ny_array[j], nz_array[j]);
        }
    }

    // Handle remainder
    for (size_t i = avxCount; i < count; ++i)
    {
        vectors[i] = vectors[i].Normalized();
    }
}

void CSIMDMath::BatchNormalizeSSE(Vec3* vectors, size_t count)
{
    // SSE normalization implementation
    size_t sseCount = count & ~3; // Round down to multiple of 4

    for (size_t i = 0; i < sseCount; i += 4)
    {
        // Load 4 vectors
        __m128 vx = _mm_set_ps(vectors[i + 3].x, vectors[i + 2].x, vectors[i + 1].x, vectors[i].x);
        __m128 vy = _mm_set_ps(vectors[i + 3].y, vectors[i + 2].y, vectors[i + 1].y, vectors[i].y);
        __m128 vz = _mm_set_ps(vectors[i + 3].z, vectors[i + 2].z, vectors[i + 1].z, vectors[i].z);

        // Calculate squared length
        __m128 length2 = _mm_add_ps(_mm_add_ps(
            _mm_mul_ps(vx, vx),
            _mm_mul_ps(vy, vy)
        ), _mm_mul_ps(vz, vz));

        // Calculate reciprocal square root
        __m128 rcpLength = _mm_rsqrt_ps(length2);

        // Normalize vectors
        __m128 nx = _mm_mul_ps(vx, rcpLength);
        __m128 ny = _mm_mul_ps(vy, rcpLength);
        __m128 nz = _mm_mul_ps(vz, rcpLength);

        // Store results
        float nx_array[4], ny_array[4], nz_array[4];
        _mm_storeu_ps(nx_array, nx);
        _mm_storeu_ps(ny_array, ny);
        _mm_storeu_ps(nz_array, nz);

        for (int j = 0; j < 4; ++j)
        {
            vectors[i + j] = Vec3(nx_array[j], ny_array[j], nz_array[j]);
        }
    }

    // Handle remainder
    for (size_t i = sseCount; i < count; ++i)
    {
        vectors[i] = vectors[i].Normalized();
    }
}

void CSIMDMath::BatchNormalizeScalar(Vec3* vectors, size_t count)
{
    // Scalar fallback implementation
    for (size_t i = 0; i < count; ++i)
    {
        vectors[i] = vectors[i].Normalized();
    }
}

void CSIMDMath::BatchDotProductAVX2(const Vec3* vectors1, const Vec3* vectors2, float* results, size_t count)
{
    // AVX2 dot product implementation - process 8 vectors at a time
    size_t avxCount = count & ~7; // Round down to multiple of 8

    for (size_t i = 0; i < avxCount; i += 8)
    {
        // Load 8 vector pairs
        __m256 v1x = _mm256_set_ps(
            vectors1[i + 7].x, vectors1[i + 6].x, vectors1[i + 5].x, vectors1[i + 4].x,
            vectors1[i + 3].x, vectors1[i + 2].x, vectors1[i + 1].x, vectors1[i].x
        );
        __m256 v1y = _mm256_set_ps(
            vectors1[i + 7].y, vectors1[i + 6].y, vectors1[i + 5].y, vectors1[i + 4].y,
            vectors1[i + 3].y, vectors1[i + 2].y, vectors1[i + 1].y, vectors1[i].y
        );
        __m256 v1z = _mm256_set_ps(
            vectors1[i + 7].z, vectors1[i + 6].z, vectors1[i + 5].z, vectors1[i + 4].z,
            vectors1[i + 3].z, vectors1[i + 2].z, vectors1[i + 1].z, vectors1[i].z
        );

        __m256 v2x = _mm256_set_ps(
            vectors2[i + 7].x, vectors2[i + 6].x, vectors2[i + 5].x, vectors2[i + 4].x,
            vectors2[i + 3].x, vectors2[i + 2].x, vectors2[i + 1].x, vectors2[i].x
        );
        __m256 v2y = _mm256_set_ps(
            vectors2[i + 7].y, vectors2[i + 6].y, vectors2[i + 5].y, vectors2[i + 4].y,
            vectors2[i + 3].y, vectors2[i + 2].y, vectors2[i + 1].y, vectors2[i].y
        );
        __m256 v2z = _mm256_set_ps(
            vectors2[i + 7].z, vectors2[i + 6].z, vectors2[i + 5].z, vectors2[i + 4].z,
            vectors2[i + 3].z, vectors2[i + 2].z, vectors2[i + 1].z, vectors2[i].z
        );

        // Calculate dot products
        __m256 dot = _mm256_add_ps(_mm256_add_ps(
            _mm256_mul_ps(v1x, v2x),
            _mm256_mul_ps(v1y, v2y)
        ), _mm256_mul_ps(v1z, v2z));

        // Store results
        _mm256_storeu_ps(&results[i], dot);
    }

    // Handle remainder
    for (size_t i = avxCount; i < count; ++i)
    {
        results[i] = vectors1[i].Dot(vectors2[i]);
    }
}

void CSIMDMath::BatchDotProductSSE(const Vec3* vectors1, const Vec3* vectors2, float* results, size_t count)
{
    // SSE dot product implementation - process 4 vectors at a time
    size_t sseCount = count & ~3; // Round down to multiple of 4

    for (size_t i = 0; i < sseCount; i += 4)
    {
        // Load 4 vector pairs
        __m128 v1x = _mm_set_ps(vectors1[i + 3].x, vectors1[i + 2].x, vectors1[i + 1].x, vectors1[i].x);
        __m128 v1y = _mm_set_ps(vectors1[i + 3].y, vectors1[i + 2].y, vectors1[i + 1].y, vectors1[i].y);
        __m128 v1z = _mm_set_ps(vectors1[i + 3].z, vectors1[i + 2].z, vectors1[i + 1].z, vectors1[i].z);

        __m128 v2x = _mm_set_ps(vectors2[i + 3].x, vectors2[i + 2].x, vectors2[i + 1].x, vectors2[i].x);
        __m128 v2y = _mm_set_ps(vectors2[i + 3].y, vectors2[i + 2].y, vectors2[i + 1].y, vectors2[i].y);
        __m128 v2z = _mm_set_ps(vectors2[i + 3].z, vectors2[i + 2].z, vectors2[i + 1].z, vectors2[i].z);

        // Calculate dot products
        __m128 dot = _mm_add_ps(_mm_add_ps(
            _mm_mul_ps(v1x, v2x),
            _mm_mul_ps(v1y, v2y)
        ), _mm_mul_ps(v1z, v2z));

        // Store results
        _mm_storeu_ps(&results[i], dot);
    }

    // Handle remainder
    for (size_t i = sseCount; i < count; ++i)
    {
        results[i] = vectors1[i].Dot(vectors2[i]);
    }
}

void CSIMDMath::BatchDotProductScalar(const Vec3* vectors1, const Vec3* vectors2, float* results, size_t count)
{
    // Scalar fallback implementation
    for (size_t i = 0; i < count; ++i)
    {
        results[i] = vectors1[i].Dot(vectors2[i]);
    }
}

// Helper functions for SIMD register operations
__m256 CSIMDMath::LoadVec3ToAVX2(const Vec3& v)
{
    return _mm256_set_ps(0.0f, 0.0f, 0.0f, 0.0f, v.z, v.y, v.x, 0.0f);
}

__m128 CSIMDMath::LoadVec3ToSSE(const Vec3& v)
{
    return _mm_set_ps(0.0f, v.z, v.y, v.x);
}

Vec3 CSIMDMath::StoreAVX2ToVec3(const __m256& reg)
{
    float temp[8];
    _mm256_storeu_ps(temp, reg);
    return Vec3(temp[0], temp[1], temp[2]);
}

Vec3 CSIMDMath::StoreSSEToVec3(const __m128& reg)
{
    float temp[4];
    _mm_storeu_ps(temp, reg);
    return Vec3(temp[0], temp[1], temp[2]);
}

bool CSIMDMath::IsAligned(const void* ptr, size_t alignment)
{
    return (reinterpret_cast<uintptr_t>(ptr) & (alignment - 1)) == 0;
}

void* CSIMDMath::AllocateAligned(size_t size, size_t alignment)
{
    return _aligned_malloc(size, alignment);
}

void CSIMDMath::FreeAligned(void* ptr)
{
    _aligned_free(ptr);
}

// Fast trigonometric functions with SIMD optimizations
float CSIMDMath::FastCos(float x)
{
    PERF_TIMER_SIMD();

    // Range reduction to [-π, π]
    x = fmodf(x + SIMD_PI_CONST, SIMD_TWO_PI_CONST);
    if (x < 0) x += SIMD_TWO_PI_CONST;
    if (x > SIMD_PI_CONST) x -= SIMD_TWO_PI_CONST;

    // Use minimax polynomial approximation for cos(x)
    // cos(x) ≈ 1 - x²/2! + x⁴/4! - x⁶/6! + x⁸/8!
    // Optimized coefficients for maximum accuracy in range [-π, π]
    const float x2 = x * x;
    const float x4 = x2 * x2;
    const float x6 = x4 * x2;
    const float x8 = x4 * x4;

    // Minimax polynomial coefficients (9th degree approximation)
    const float result = 1.0f - x2 * 0.5f + x4 * 0.0416666667f - x6 * 0.00138888889f + x8 * 0.0000248015873f;

    s_nTotalSIMDOperations++;
    return result;
}

float CSIMDMath::FastSin(float x)
{
    PERF_TIMER_SIMD();

    // Range reduction to [-π, π]
    x = fmodf(x + SIMD_PI_CONST, SIMD_TWO_PI_CONST);
    if (x < 0) x += SIMD_TWO_PI_CONST;
    if (x > SIMD_PI_CONST) x -= SIMD_TWO_PI_CONST;

    // Use minimax polynomial approximation for sin(x)
    // sin(x) ≈ x - x³/3! + x⁵/5! - x⁷/7! + x⁹/9!
    const float x2 = x * x;
    const float x3 = x * x2;
    const float x5 = x3 * x2;
    const float x7 = x5 * x2;
    const float x9 = x7 * x2;

    // Minimax polynomial coefficients (9th degree approximation)
    const float result = x - x3 * 0.166666667f + x5 * 0.00833333333f - x7 * 0.000198412698f + x9 * 0.00000275573192f;

    s_nTotalSIMDOperations++;
    return result;
}

float CSIMDMath::FastTan(float x)
{
    PERF_TIMER_SIMD();

    // tan(x) = sin(x) / cos(x) for numerical stability
    const float sinVal = FastSin(x);
    const float cosVal = FastCos(x);

    // Prevent division by zero
    if (fabs(cosVal) < 1e-6f) {
        return (sinVal > 0) ? 1e6f : -1e6f;
    }

    const float result = sinVal / cosVal;
    s_nTotalSIMDOperations++;
    return result;
}

void CSIMDMath::FastSinCos(float x, float& sinOut, float& cosOut)
{
    PERF_TIMER_SIMD();

    // Range reduction to [-π, π] (done once for both functions)
    x = fmodf(x + SIMD_PI_CONST, SIMD_TWO_PI_CONST);
    if (x < 0) x += SIMD_TWO_PI_CONST;
    if (x > SIMD_PI_CONST) x -= SIMD_TWO_PI_CONST;

    // Calculate powers once and reuse
    const float x2 = x * x;
    const float x3 = x * x2;
    const float x4 = x2 * x2;
    const float x5 = x3 * x2;
    const float x6 = x4 * x2;
    const float x7 = x5 * x2;
    const float x8 = x4 * x4;
    const float x9 = x7 * x2;

    // Minimax polynomial for sin(x)
    sinOut = x - x3 * 0.166666667f + x5 * 0.00833333333f - x7 * 0.000198412698f + x9 * 0.00000275573192f;

    // Minimax polynomial for cos(x)
    cosOut = 1.0f - x2 * 0.5f + x4 * 0.0416666667f - x6 * 0.00138888889f + x8 * 0.0000248015873f;

    s_nTotalSIMDOperations += 2; // Count as two operations
}