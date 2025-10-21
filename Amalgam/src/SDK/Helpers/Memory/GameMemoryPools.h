#pragma once
#include "../../../Utils/Feature/Feature.h"
#include <vector>
#include <bitset>
#include <unordered_map>
#include <string>

// Forward declarations for pooled types
struct Target_t;
class CGameTrace;

// Memory pool constants for MSVC compatibility
#define KEYVALUES_POOL_SIZE 128
#define STRING_POOL_SIZE 256
#define MAX_STRING_LENGTH 256
#define VECTOR_POOL_SIZE 512
#define COLOR_POOL_SIZE 256
#define TARGET_POOL_SIZE 64
#define TRACE_POOL_SIZE 32

// Expanded memory pool system for frequently allocated game objects
class CGameMemoryPools
{
private:
    // KeyValues pool (already implemented)
    static KeyValues* s_pKeyValuesPool[KEYVALUES_POOL_SIZE];
    static std::bitset<KEYVALUES_POOL_SIZE> s_bKeyValuesUsed;

    // String pool for frequently allocated strings
    static char s_sStringPool[STRING_POOL_SIZE][MAX_STRING_LENGTH];
    static std::bitset<STRING_POOL_SIZE> s_bStringUsed;
    static std::unordered_map<std::string_view, char*> s_mStringCache;

    // Vector math pool for Vec3, Vec2 operations
    static float s_fVectorPool[VECTOR_POOL_SIZE * 4]; // 4 floats per Vec3
    static std::bitset<VECTOR_POOL_SIZE> s_bVectorUsed;

    // Color pool for color operations
    static uint32_t s_nColorPool[COLOR_POOL_SIZE];
    static std::bitset<COLOR_POOL_SIZE> s_bColorUsed;

    // Target pool for aimbot target objects (high-frequency allocation)
    static Target_t s_pTargetPool[TARGET_POOL_SIZE];
    static std::bitset<TARGET_POOL_SIZE> s_bTargetUsed;

    // CGameTrace pool for trace operations (frequent in aimbot)
    static CGameTrace s_pTracePool[TRACE_POOL_SIZE];
    static std::bitset<TRACE_POOL_SIZE> s_bTraceUsed;

    // Statistics
    static uint32_t s_nKeyValuesAllocs;
    static uint32_t s_nStringAllocs;
    static uint32_t s_nVectorAllocs;
    static uint32_t s_nColorAllocs;
    static uint32_t s_nTargetAllocs;
    static uint32_t s_nTraceAllocs;

public:
    // KeyValues allocation (enhanced from existing system)
    static KeyValues* AllocKeyValues(const char* pszName)
    {
        // Use existing KeyValuesPool system
        return CKeyValuesPool::Alloc(pszName);
    }

    static void FreeKeyValues(KeyValues* pKeyValues)
    {
        CKeyValuesPool::Free(pKeyValues);
    }

    // String allocation for frequently used strings
    static const char* AllocString(const std::string& str)
    {
        // Check cache first
        auto it = s_mStringCache.find(str);
        if (it != s_mStringCache.end())
        {
            s_nStringAllocs++;
            return it->second;
        }

        // Find free slot
        for (int i = 0; i < STRING_POOL_SIZE; ++i)
        {
            if (!s_bStringUsed[i])
            {
                s_bStringUsed[i] = true;

                // Copy string (ensure null termination)
                size_t copyLength = std::min(str.length(), size_t(MAX_STRING_LENGTH - 1));
                strncpy(s_sStringPool[i], str.c_str(), copyLength);
                s_sStringPool[i][copyLength] = '\0';

                // Cache the string
                char* cachedPtr = s_sStringPool[i];
                s_mStringCache[str] = cachedPtr;
                s_nStringAllocs++;

                return cachedPtr;
            }
        }

        // Pool full, fallback to heap allocation
        s_nStringAllocs++;
        char* pHeapStr = new char[str.length() + 1];
        strcpy(pHeapStr, str.c_str());
        s_mStringCache[str] = pHeapStr;
        return pHeapStr;
    }

    // Vector allocation (3D vectors)
    static float* AllocVector()
    {
        for (int i = 0; i < VECTOR_POOL_SIZE; ++i)
        {
            if (!s_bVectorUsed[i])
            {
                s_bVectorUsed[i] = true;
                s_nVectorAllocs++;
                return &s_fVectorPool[i * 4]; // 4 floats = Vec3
            }
        }

        // Pool full, fallback to heap allocation
        s_nVectorAllocs++;
        return new float[4]; // Vec3 = 4 floats
    }

    static void FreeVector(float* pVector)
    {
        // Check if it's from our pool
        if (pVector >= s_fVectorPool && pVector < s_fVectorPool + (VECTOR_POOL_SIZE * 4))
        {
            size_t index = (pVector - s_fVectorPool) / 4;
            if (index < VECTOR_POOL_SIZE)
            {
                s_bVectorUsed[index] = false;
                return;
            }
        }

        // Heap allocation, delete it
        delete[] pVector;
    }

    // Color allocation
    static uint32_t* AllocColor()
    {
        for (int i = 0; i < COLOR_POOL_SIZE; ++i)
        {
            if (!s_bColorUsed[i])
            {
                s_bColorUsed[i] = true;
                s_nColorAllocs++;
                return &s_nColorPool[i];
            }
        }

        // Pool full, fallback to heap allocation
        s_nColorAllocs++;
        return new uint32_t;
    }

    static void FreeColor(uint32_t* pColor)
    {
        // Check if it's from our pool
        if (pColor >= s_nColorPool && pColor < s_nColorPool + COLOR_POOL_SIZE)
        {
            size_t index = pColor - s_nColorPool;
            if (index < COLOR_POOL_SIZE)
            {
                s_bColorUsed[index] = false;
                return;
            }
        }

        // Heap allocation, delete it
        delete pColor;
    }

    // Statistics
    static void GetAllocationStats(uint32_t& keyValues, uint32_t& strings, uint32_t& vectors, uint32_t& colors)
    {
        keyValues = s_nKeyValuesAllocs;
        strings = s_nStringAllocs;
        vectors = s_nVectorAllocs;
        colors = s_nColorAllocs;
    }

    static float GetPoolUtilization()
    {
        float keyValuesUtil = static_cast<float>(s_bKeyValuesUsed.count()) / KEYVALUES_POOL_SIZE;
        float stringUtil = static_cast<float>(s_bStringUsed.count()) / STRING_POOL_SIZE;
        float vectorUtil = static_cast<float>(s_bVectorUsed.count()) / VECTOR_POOL_SIZE;
        float colorUtil = static_cast<float>(s_bColorUsed.count()) / COLOR_POOL_SIZE;

        return (keyValuesUtil + stringUtil + vectorUtil + colorUtil) / 4.0f;
    }

    // Clear all pools (useful for debugging)
    static void ClearAllPools()
    {
        s_bKeyValuesUsed.reset();
        s_bStringUsed.reset();
        s_bVectorUsed.reset();
        s_bColorUsed.reset();
        s_mStringCache.clear();

        // Reset statistics
        s_nKeyValuesAllocs = 0;
        s_nStringAllocs = 0;
        s_nVectorAllocs = 0;
        s_nColorAllocs = 0;
    }

    // Initialize pools
    static void Initialize()
    {
        ClearAllPools();
    }

private:
    // Private constructor to prevent instantiation
    CGameMemoryPools() = delete;
    ~CGameMemoryPools() = delete;
};

// RAII helper for automatic pool management
class CPooledString
{
private:
    const char* m_pString;

public:
    CPooledString(const std::string& str)
        : m_pString(CGameMemoryPools::AllocString(str))
    {}

    ~CPooledString()
    {
        // String pool doesn't need explicit freeing
        // Memory is reused when pools are reset
    }

    const char* Get() const { return m_pString; }
    operator const char*() const { return m_pString; }
};

// RAII helper for pooled vectors
class CPooledVector
{
private:
    float* m_pVector;

public:
    CPooledVector()
        : m_pVector(CGameMemoryPools::AllocVector())
    {}

    ~CPooledVector()
    {
        CGameMemoryPools::FreeVector(m_pVector);
    }

    float* Get() const { return m_pVector; }
    operator float*() const { return m_pVector; }
};

// RAII helper for pooled colors
class CPooledColor
{
private:
    uint32_t* m_pColor;

public:
    CPooledColor()
        : m_pColor(CGameMemoryPools::AllocColor())
    {}

    ~CPooledColor()
    {
        CGameMemoryPools::FreeColor(m_pColor);
    }

    uint32_t* Get() const { return m_pColor; }
    operator uint32_t*() const { return m_pColor; }
};