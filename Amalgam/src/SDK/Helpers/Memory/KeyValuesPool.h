#pragma once
#include "../../../Utils/Feature/Feature.h"
#include <vector>
#include <bitset>

class CKeyValuesPool
{
private:
    static constexpr int POOL_SIZE = 128; // Reasonable pool size for most use cases
    static constexpr int FALLBACK_THRESHOLD = 64; // When to fall back to new allocations

    struct PoolEntry
    {
        KeyValues* pKeyValues = nullptr;
        bool bInUse = false;
        char szName[64]; // Store the name for debugging
    };

    std::vector<PoolEntry> m_vPool;
    int m_nNextFree = 0;
    int m_nInUse = 0;
    int m_nTotalAllocations = 0;
    int m_nPoolHits = 0;

    // Singleton instance
    static CKeyValuesPool* s_pInstance;
    static CKeyValuesPool* GetInstance()
    {
        if (!s_pInstance)
        {
            s_pInstance = new CKeyValuesPool();
        }
        return s_pInstance;
    }

    // Find next available slot
    int FindFreeSlot()
    {
        // Simple linear search - good enough for small pools
        for (int i = m_nNextFree; i < m_vPool.size(); i++)
        {
            if (!m_vPool[i].bInUse)
            {
                m_nNextFree = i + 1;
                return i;
            }
        }

        // Search from beginning if we reached end
        for (int i = 0; i < m_nNextFree; i++)
        {
            if (!m_vPool[i].bInUse)
            {
                m_nNextFree = i + 1;
                return i;
            }
        }

        return -1; // No free slots
    }

public:
    CKeyValuesPool()
    {
        m_vPool.reserve(POOL_SIZE);
        for (int i = 0; i < POOL_SIZE; i++)
        {
            m_vPool.push_back({nullptr, false, ""});
        }
    }

    ~CKeyValuesPool()
    {
        // Clean up all allocated KeyValues
        for (auto& entry : m_vPool)
        {
            if (entry.pKeyValues)
            {
                entry.pKeyValues->deleteThis();
                entry.pKeyValues = nullptr;
            }
        }
    }

    // Allocate a KeyValues object
    KeyValues* Allocate(const char* pszName)
    {
        m_nTotalAllocations++;

        // Try to use pool first
        int nSlot = FindFreeSlot();
        if (nSlot != -1)
        {
            if (!m_vPool[nSlot].pKeyValues)
            {
                m_vPool[nSlot].pKeyValues = new KeyValues(pszName);
            }
            else
            {
                // Reuse existing KeyValues by reinitializing
                m_vPool[nSlot].pKeyValues->deleteThis();
                m_vPool[nSlot].pKeyValues = new KeyValues(pszName);
            }

            m_vPool[nSlot].bInUse = true;
            strncpy_s(m_vPool[nSlot].szName, sizeof(m_vPool[nSlot].szName), pszName, _TRUNCATE);
            m_nInUse++;
            m_nPoolHits++;
            return m_vPool[nSlot].pKeyValues;
        }

        // Pool is full or corrupted, fall back to new allocation
        return new KeyValues(pszName);
    }

    // Free a KeyValues object back to pool
    void Free(KeyValues* pKeyValues)
    {
        if (!pKeyValues)
            return;

        // Find the KeyValues in our pool
        for (auto& entry : m_vPool)
        {
            if (entry.pKeyValues == pKeyValues)
            {
                entry.bInUse = false;
                m_nInUse--;
                // Update next free slot if this is earlier
                int nIndex = &entry - &m_vPool[0];
                if (nIndex < m_nNextFree)
                {
                    m_nNextFree = nIndex;
                }
                return;
            }
        }

        // Not found in pool, delete directly
        pKeyValues->deleteThis();
    }

    // Get statistics
    void GetStats(int& nTotal, int& nInUse, int& nPoolHits) const
    {
        nTotal = m_nTotalAllocations;
        nInUse = m_nInUse;
        nPoolHits = m_nPoolHits;
    }

    // Clear all allocations (useful for debugging)
    void Clear()
    {
        for (auto& entry : m_vPool)
        {
            if (entry.pKeyValues)
            {
                entry.pKeyValues->deleteThis();
                entry.pKeyValues = nullptr;
            }
            entry.bInUse = false;
            entry.szName[0] = '\0';
        }
        m_nNextFree = 0;
        m_nInUse = 0;
    }

    // Static interface for easy use
    static KeyValues* Alloc(const char* pszName)
    {
        return GetInstance()->Allocate(pszName);
    }

    static void Dealloc(KeyValues* pKeyValues)
    {
        GetInstance()->Free(pKeyValues);
    }

    static void GetPoolStats(int& nTotal, int& nInUse, int& nPoolHits)
    {
        GetInstance()->GetStats(nTotal, nInUse, nPoolHits);
    }

    static void ClearPool()
    {
        GetInstance()->Clear();
    }
};

// RAII wrapper for automatic cleanup
class CScopedKeyValues
{
private:
    KeyValues* m_pKeyValues;

public:
    explicit CScopedKeyValues(const char* pszName)
        : m_pKeyValues(CKeyValuesPool::Alloc(pszName))
    {
    }

    ~CScopedKeyValues()
    {
        if (m_pKeyValues)
        {
            CKeyValuesPool::Dealloc(m_pKeyValues);
        }
    }

    // Get the KeyValues pointer
    KeyValues* Get() const { return m_pKeyValues; }

    // Conversion operators
    operator KeyValues*() const { return m_pKeyValues; }
    operator bool() const { return m_pKeyValues != nullptr; }

    // Arrow operator for easy access
    KeyValues* operator->() const { return m_pKeyValues; }
};