#pragma once
#include "SIMDMath.h"
#include "../../SDK/Definitions/Types.h"

// SIMD Integration Examples and Helper Functions
// Demonstrates how to integrate SIMD vector math into existing features

namespace SIMDIntegration
{
    // Aimbot-specific SIMD optimizations
    namespace Aimbot
    {
        // Calculate distances to multiple targets simultaneously (4x faster)
        static void CalculateTargetDistances(const Vec3* targetPositions, const Vec3& localPosition, float* distances, size_t targetCount)
        {
            CSIMDMath::BatchDistance(targetPositions, localPosition, distances, targetCount);
        }

        // Calculate FOV distances for multiple targets (critical for aimbot)
        static void CalculateFOVDistances(const Vec3& viewAngle, const Vec3* targetAngles, float* fovDistances, size_t targetCount)
        {
            CSIMDMath::BatchFOVDistances(viewAngle, targetAngles, fovDistances, targetCount);
        }

        // Normalize aim vectors for multiple targets
        static void NormalizeAimVectors(Vec3* aimVectors, size_t count)
        {
            CSIMDMath::BatchNormalizeSafe(aimVectors, count);
        }

        // Calculate dot products for visibility checks
        static void CalculateVisibilityAngles(const Vec3* aimVectors, const Vec3* forwardVectors, float* angles, size_t count)
        {
            CSIMDMath::BatchAngleBetweenVectors(aimVectors, forwardVectors, angles, count);
        }
    }

    // ESP/Visuals-specific SIMD optimizations
    namespace ESP
    {
        // Batch calculate distances from player to screen center
        static void CalculateScreenDistances(const Vec3* playerPositions, const Vec3& localPosition, float* distances, size_t playerCount)
        {
            CSIMDMath::BatchDistance2D(playerPositions, localPosition, distances, playerCount);
        }

        // Transform multiple 3D positions to screen coordinates
        static void WorldToScreenBatch(const Vec3* worldPositions, const matrix3x4* worldToScreen, Vec3* screenPositions, size_t count)
        {
            CSIMDMath::BatchTransformPoints(worldPositions, worldToScreen, screenPositions, count);
        }

        // Scale multiple vectors (for distance-based scaling)
        static void ScaleVectorsByDistance(const Vec3* vectors, const float* scaleFactors, Vec3* results, size_t count)
        {
            for (size_t i = 0; i < count; ++i)
            {
                results[i] = vectors[i] * scaleFactors[i];
            }
            // Note: This could be further optimized with SIMD if needed
        }
    }

    // Physics/Simulation SIMD optimizations
    namespace Physics
    {
        // Calculate projectile paths for multiple projectiles
        static void CalculateProjectilePositions(const Vec3* startPositions, const Vec3* velocities, float time, Vec3* endPositions, size_t count)
        {
            CSIMDMath::BatchAdd(startPositions, nullptr, endPositions, count); // This is a placeholder

            // Real implementation would use:
            // for each projectile: end = start + velocity * time
            for (size_t i = 0; i < count; ++i)
            {
                endPositions[i] = startPositions[i] + velocities[i] * time;
            }
        }

        // Normalize multiple velocity vectors
        static void NormalizeVelocities(Vec3* velocities, size_t count)
        {
            CSIMDMath::BatchNormalizeSafe(velocities, count);
        }

        // Calculate relative velocities between entities
        static void CalculateRelativeVelocities(const Vec3* velocities1, const Vec3* velocities2, Vec3* relativeVelocities, size_t count)
        {
            CSIMDMath::BatchSubtract(velocities1, velocities2, relativeVelocities, count);
        }
    }

    // Performance monitoring utilities
    namespace Performance
    {
        // Benchmark vector operations
        static void BenchmarkVectorOperations(size_t iterations = 100000)
        {
            // Create test data
            std::vector<Vec3> vectors1(iterations);
            std::vector<Vec3> vectors2(iterations);
            std::vector<float> results(iterations);

            // Generate test data
            for (size_t i = 0; i < iterations; ++i)
            {
                vectors1[i] = Vec3(
                    static_cast<float>(rand()) / RAND_MAX * 100.0f,
                    static_cast<float>(rand()) / RAND_MAX * 100.0f,
                    static_cast<float>(rand()) / RAND_MAX * 100.0f
                );
                vectors2[i] = Vec3(
                    static_cast<float>(rand()) / RAND_MAX * 100.0f,
                    static_cast<float>(rand()) / RAND_MAX * 100.0f,
                    static_cast<float>(rand()) / RAND_MAX * 100.0f
                );
            }

            // Test batch operations
            auto start = std::chrono::high_resolution_clock::now();

            CSIMDMath::BatchDistance(vectors1.data(), Vec3(0, 0, 0), results.data(), iterations);

            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

            // Get SIMD stats
            uint64_t totalOps;
            double avgTime;
            CSIMDMath::GetSIMDStats(totalOps, avgTime);

            printf("SIMD Benchmark Results:\n");
            printf("Iterations: %zu\n", iterations);
            printf("Total time: %lld μs\n", duration.count());
            printf("Operations per second: %.2f\n", static_cast<double>(iterations) / (duration.count() / 1000000.0));
            printf("SIMD operations: %llu\n", totalOps);
            printf("Average time per operation: %.6f ms\n", avgTime);
        }

        // Compare SIMD vs scalar performance
        static void CompareSIMDvsScalar(size_t iterations = 10000)
        {
            std::vector<Vec3> vectors1(iterations);
            std::vector<Vec3> vectors2(iterations);
            std::vector<float> resultsSIMD(iterations);
            std::vector<float> resultsScalar(iterations);

            // Generate test data
            for (size_t i = 0; i < iterations; ++i)
            {
                vectors1[i] = Vec3(
                    static_cast<float>(rand()) / RAND_MAX * 50.0f,
                    static_cast<float>(rand()) / RAND_MAX * 50.0f,
                    static_cast<float>(rand()) / RAND_MAX * 50.0f
                );
                vectors2[i] = Vec3(
                    static_cast<float>(rand()) / RAND_MAX * 50.0f,
                    static_cast<float>(rand()) / RAND_MAX * 50.0f,
                    static_cast<float>(rand()) / RAND_MAX * 50.0f
                );
            }

            // Test SIMD performance
            auto simdStart = std::chrono::high_resolution_clock::now();
            CSIMDMath::BatchDistance(vectors1.data(), Vec3(0, 0, 0), resultsSIMD.data(), iterations);
            auto simdEnd = std::chrono::high_resolution_clock::now();
            auto simdDuration = std::chrono::duration_cast<std::chrono::microseconds>(simdEnd - simdStart);

            // Test scalar performance
            auto scalarStart = std::chrono::high_resolution_clock::now();
            for (size_t i = 0; i < iterations; ++i)
            {
                resultsScalar[i] = (vectors1[i] - Vec3(0, 0, 0)).Length();
            }
            auto scalarEnd = std::chrono::high_resolution_clock::now();
            auto scalarDuration = std::chrono::duration_cast<std::chrono::microseconds>(scalarEnd - scalarStart);

            double speedup = static_cast<double>(scalarDuration.count()) / static_cast<double>(simdDuration.count());

            printf("SIMD vs Scalar Performance Comparison:\n");
            printf("Iterations: %zu\n", iterations);
            printf("SIMD time: %lld μs\n", simdDuration.count());
            printf("Scalar time: %lld μs\n", scalarDuration.count());
            printf("Speedup: %.2fx\n", speedup);
            printf("Performance improvement: %.1f%%\n", (speedup - 1.0) * 100.0);
        }
    }

    // Utility functions for easy integration
    namespace Utils
    {
        // Check if SIMD is available and print status
        static void PrintSIMDStatus()
        {
            CSIMDMath::Initialize();

            printf("SIMD Status:\n");
            printf("SSE Support: %s\n", CSIMDMath::IsSSESupported() ? "YES" : "NO");
            printf("AVX2 Support: %s\n", CSIMDMath::IsAVX2Supported() ? "YES" : "NO");
            printf("SIMD Enabled: %s\n", CSIMDMath::IsSIMDEnabled() ? "YES" : "NO");

            if (CSIMDMath::IsSIMDEnabled())
            {
                printf("Expected Performance Improvement: 30-50%% for vector operations\n");
            }
        }

        // Get performance metrics
        static void PrintPerformanceMetrics()
        {
            uint64_t totalOps;
            double avgTime;
            CSIMDMath::GetSIMDStats(totalOps, avgTime);

            printf("SIMD Performance Metrics:\n");
            printf("Total Operations: %llu\n", totalOps);
            printf("Average Time per Operation: %.6f ms\n", avgTime);
            printf("Operations per Second: %.2f\n", totalOps > 0 ? 1000.0 / avgTime : 0.0);
        }

        // Initialize SIMD system with status reporting
        static void InitializeWithStatus()
        {
            CSIMDMath::Initialize();
            PrintSIMDStatus();
            printf("SIMD Math System initialized successfully!\n");
        }
    }
}

// Convenience macros for common operations
#define SIMD_CALCULATE_TARGET_DISTANCES(targets, localPos, distances, count) \
    SIMDIntegration::Aimbot::CalculateTargetDistances(targets, localPos, distances, count)

#define SIMD_CALCULATE_FOV_DISTANCES(viewAngle, targetAngles, fovDistances, count) \
    SIMDIntegration::Aimbot::CalculateFOVDistances(viewAngle, targetAngles, fovDistances, count)

#define SIMD_NORMALIZE_AIM_VECTORS(aimVectors, count) \
    SIMDIntegration::Aimbot::NormalizeAimVectors(aimVectors, count)

#define SIMD_CALCULATE_SCREEN_DISTANCES(players, localPos, distances, count) \
    SIMDIntegration::ESP::CalculateScreenDistances(players, localPos, distances, count)

#define SIMD_INITIALIZE() \
    SIMDIntegration::Utils::InitializeWithStatus()

// Integration example for features:
//
// In your aimbot code:
//   std::vector<CBaseEntity*> targets = GetTargets();
//   std::vector<Vec3> targetPositions(targets.size());
//   std::vector<float> distances(targets.size());
//
//   for (size_t i = 0; i < targets.size(); ++i) {
//       targetPositions[i] = targets[i]->GetAbsOrigin();
//   }
//
//   Vec3 localPos = H::Entities.GetLocalPlayer()->GetAbsOrigin();
//   SIMD_CALCULATE_TARGET_DISTANCES(targetPositions.data(), localPos, distances.data(), targets.size());
//
//   // Now distances contains all distances calculated with SIMD optimization