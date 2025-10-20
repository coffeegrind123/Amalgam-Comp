#pragma once
#include "../../../Utils/Feature/Feature.h"
#include <chrono>
#include <string>

class CPerformanceMonitor
{
private:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;

    struct PerformanceMetrics
    {
        TimePoint startTime;
        TimePoint lastUpdateTime;
        uint64_t totalCalls = 0;
        double totalTimeMs = 0.0;
        double minTimeMs = 999999.0;
        double maxTimeMs = 0.0;
        double avgTimeMs = 0.0;
        double currentFps = 0.0;
        std::string name;
    };

    static PerformanceMetrics s_FrameStageNotify;
    static PerformanceMetrics s_EntityStore;
    static PerformanceMetrics s_PlayerUtilsUpdate;
    static PerformanceMetrics s_KeyValuesAlloc;
    static PerformanceMetrics s_MemoryPools;
    static bool s_bInitialized;

    static void UpdateMetrics(PerformanceMetrics& metrics, double elapsedMs)
    {
        metrics.totalCalls++;
        metrics.totalTimeMs += elapsedMs;
        metrics.avgTimeMs = metrics.totalTimeMs / metrics.totalCalls;

        if (elapsedMs < metrics.minTimeMs)
            metrics.minTimeMs = elapsedMs;
        if (elapsedMs > metrics.maxTimeMs)
            metrics.maxTimeMs = elapsedMs;

        // Calculate FPS based on recent performance
        auto now = Clock::now();
        auto timeSinceUpdate = std::chrono::duration_cast<std::chrono::milliseconds>(now - metrics.lastUpdateTime).count();
        if (timeSinceUpdate >= 1000) // Update every second
        {
            metrics.currentFps = 1000.0 / (elapsedMs > 0.0 ? elapsedMs : 1.0);
            metrics.lastUpdateTime = now;
        }
    }

public:
    static void Initialize()
    {
        if (s_bInitialized)
            return;

        auto now = Clock::now();

        s_FrameStageNotify = {now, now, 0, 0.0, 999999.0, 0.0, 0.0, 0.0, "FrameStageNotify"};
        s_EntityStore = {now, now, 0, 0.0, 999999.0, 0.0, 0.0, 0.0, "EntityStore"};
        s_PlayerUtilsUpdate = {now, now, 0, 0.0, 999999.0, 0.0, 0.0, 0.0, "PlayerUtilsUpdate"};
        s_KeyValuesAlloc = {now, now, 0, 0.0, 999999.0, 0.0, 0.0, 0.0, "KeyValuesAlloc"};

        s_bInitialized = true;
    }

    class ScopedTimer
    {
    private:
        PerformanceMetrics& m_metrics;
        TimePoint m_startTime;

    public:
        explicit ScopedTimer(PerformanceMetrics& metrics)
            : m_metrics(metrics), m_startTime(Clock::now())
        {}

        ~ScopedTimer()
        {
            auto endTime = Clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(endTime - m_startTime).count();
            double elapsedMs = elapsed / 1000.0;
            UpdateMetrics(m_metrics, elapsedMs);
        }
    };

    // Timer macros for easy usage
    #define PERF_TIMER_FRAMESTAGE() CPerformanceMonitor::ScopedTimer timer(CPerformanceMonitor::GetFrameStageMetrics())
    #define PERF_TIMER_ENTITY() CPerformanceMonitor::ScopedTimer timer(CPerformanceMonitor::GetEntityStoreMetrics())
    #define PERF_TIMER_PLAYER() CPerformanceMonitor::ScopedTimer timer(CPerformanceMonitor::GetPlayerUtilsMetrics())
    #define PERF_TIMER_KEYVALUES() CPerformanceMonitor::ScopedTimer timer(CPerformanceMonitor::GetKeyValuesMetrics())
#define PERF_TIMER_MEMORY_POOLS() CPerformanceMonitor::ScopedTimer timer(CPerformanceMonitor::GetMemoryPoolsMetrics())

    // Get metrics for external timing
    static PerformanceMetrics& GetFrameStageMetrics() { return s_FrameStageNotify; }
    static PerformanceMetrics& GetEntityStoreMetrics() { return s_EntityStore; }
    static PerformanceMetrics& GetPlayerUtilsMetrics() { return s_PlayerUtilsUpdate; }
    static PerformanceMetrics& GetKeyValuesMetrics() { return s_KeyValuesAlloc; }
    static PerformanceMetrics& GetMemoryPoolsMetrics() { return s_MemoryPools; }

    // Get performance statistics for memory pools
    static void GetMemoryPoolsStats(uint64_t& calls, double& avgMs)
    {
        calls = s_MemoryPools.totalCalls;
        avgMs = s_MemoryPools.avgTimeMs;
    }

    // Get performance statistics
    static void GetFrameStageStats(uint64_t& calls, double& avgMs, double& fps)
    {
        calls = s_FrameStageNotify.totalCalls;
        avgMs = s_FrameStageNotify.avgTimeMs;
        fps = s_FrameStageNotify.currentFps;
    }

    static void GetEntityStoreStats(uint64_t& calls, double& avgMs)
    {
        calls = s_EntityStore.totalCalls;
        avgMs = s_EntityStore.avgTimeMs;
    }

    static void GetPlayerUtilsStats(uint64_t& calls, double& avgMs)
    {
        calls = s_PlayerUtilsUpdate.totalCalls;
        avgMs = s_PlayerUtilsUpdate.avgTimeMs;
    }

    static void GetKeyValuesStats(uint64_t& calls, double& avgMs)
    {
        calls = s_KeyValuesAlloc.totalCalls;
        avgMs = s_KeyValuesAlloc.avgTimeMs;
    }

    // Reset all metrics
    static void ResetMetrics()
    {
        Initialize();
        s_FrameStageNotify.totalCalls = 0;
        s_FrameStageNotify.totalTimeMs = 0.0;
        s_EntityStore.totalCalls = 0;
        s_EntityStore.totalTimeMs = 0.0;
        s_PlayerUtilsUpdate.totalCalls = 0;
        s_PlayerUtilsUpdate.totalTimeMs = 0.0;
        s_KeyValuesAlloc.totalCalls = 0;
        s_KeyValuesAlloc.totalTimeMs = 0.0;
    }

    // Print performance report (for debugging)
    static void PrintPerformanceReport()
    {
        // Implementation would go here for console output
        // This is useful for debugging performance issues
    }

    // Check if performance optimizations are working
    static bool IsOptimized()
    {
        return s_FrameStageNotify.avgTimeMs < 1.0 && // FrameStage should be under 1ms
               s_EntityStore.avgTimeMs < 0.5 &&      // Entity store should be under 0.5ms
               s_PlayerUtilsUpdate.avgTimeMs < 0.3;   // Player utils should be under 0.3ms
    }
};