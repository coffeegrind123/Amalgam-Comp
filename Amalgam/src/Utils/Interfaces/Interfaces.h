#pragma once
#include "../Feature/Feature.h"
#include <vector>

struct InterfaceInit_t
{
	void** m_pPtr;
	const char* m_sDLL;
	const char* m_sName;
	int8_t m_nType; // 0: find interface, 1: get export, 2: sig scan
	int8_t m_nOffset;
	int8_t m_nDereferenceCount;

	InterfaceInit_t(void** pPtr, const char* sDLL, const char* sName, int8_t nType, int8_t nOffset = 0, int8_t nDereferenceCount = 0);
};

#define MAKE_INTERFACE_VERSION(type, symbol, dll, version) namespace I { inline type *symbol = nullptr; } \
namespace MAKE_INTERFACE_SCOPE \
{\
	inline InterfaceInit_t symbol##InterfaceInit_t(reinterpret_cast<void**>(&I::symbol), dll, version, 0); \
}

#define MAKE_INTERFACE_EXPORT(type, symbol, dll, name, deref) namespace I { inline type *symbol = nullptr; } \
namespace MAKE_INTERFACE_SCOPE \
{\
	inline InterfaceInit_t symbol##InterfaceInit_t(reinterpret_cast<void**>(&I::symbol), dll, name, 1, 0, deref); \
}

#define MAKE_INTERFACE_SIGNATURE(type, symbol, dll, signature, offset, deref) namespace I { inline type *symbol = nullptr; } \
namespace MAKE_INTERFACE_SCOPE \
{\
	inline InterfaceInit_t symbol##InterfaceInit_t(reinterpret_cast<void**>(&I::symbol), dll, signature, 2, offset, deref); \
}

#define MAKE_INTERFACE_NULL(type, symbol) namespace I { inline type *symbol = nullptr; }

class CInterfaces
{
private:
	std::vector<InterfaceInit_t*> m_vInterfaces = {};
	bool m_bFailed = false;

	// Enhanced interface caching with validation (TF2 Linux Internal inspired)
	static constexpr size_t MAX_CACHED_INTERFACES = 64;
	static void* s_pCachedInterfaces[MAX_CACHED_INTERFACES];
	static const char* s_pCachedNames[MAX_CACHED_INTERFACES];
	static bool s_bCachedValid[MAX_CACHED_INTERFACES];
	static uint32_t s_nCacheHits;
	static uint32_t s_nCacheMisses;
	static uint32_t s_nValidationCalls;

	// Validate interface integrity (cross-platform compatible)
	static bool ValidateInterface(void* pInterface, const char* szName)
	{
		s_nValidationCalls++;
		if (!pInterface) return false;

		// Basic pointer validation
		if (!pInterface) return false;

		// Check vtable validity (skip complex validation for Windows compatibility)
		void** pVTable = *(void***)pInterface;
		return pVTable != nullptr;
	}

	// Fast hash for interface name caching
	static uint32_t HashInterfaceName(const char* szName)
	{
		uint32_t hash = 5381;
		while (*szName)
		{
			hash = ((hash << 5) + hash) + (unsigned char)(*szName);
			szName++;
		}
		return hash;
	}

	// Find cached interface slot by name hash
	static int FindCachedInterface(const char* szName, uint32_t hash)
	{
		uint32_t nameHash = hash ? hash : HashInterfaceName(szName);

		for (int i = 0; i < MAX_CACHED_INTERFACES; ++i)
		{
			if (s_bCachedValid[i] &&
				(!s_pCachedNames[i] || strcmp(s_pCachedNames[i], szName) == 0))
			{
				return i;
			}
		}
		return -1;
	}

public:
	bool Initialize();

	inline void AddInterface(InterfaceInit_t* pInterface)
	{
		m_vInterfaces.push_back(pInterface);
	}

	// Enhanced interface access with caching and validation
	template<typename T>
	static T* GetInterface(const char* szName, void* (*initializer)(const char*))
	{
		// Check cache first (fast path)
		uint32_t nameHash = HashInterfaceName(szName);
		int cacheIndex = FindCachedInterface(szName, nameHash);

		if (cacheIndex >= 0 && s_bCachedValid[cacheIndex])
		{
			s_nCacheHits++;

			// Validate cached interface periodically (every 1000 accesses)
			if (s_nCacheHits % 1000 == 0)
			{
				if (!ValidateInterface(s_pCachedInterfaces[cacheIndex], szName))
				{
					// Cache invalid, clear and fallback
					s_bCachedValid[cacheIndex] = false;
					s_pCachedInterfaces[cacheIndex] = nullptr;
					s_pCachedNames[cacheIndex] = nullptr;
				}
			}

			return static_cast<T*>(s_pCachedInterfaces[cacheIndex]);
		}

		// Cache miss, initialize interface
		s_nCacheMisses++;
		T* pInterface = static_cast<T*>(initializer(szName));

		if (!pInterface)
		{
			return nullptr;
		}

		// Validate before caching
		if (!ValidateInterface(pInterface, szName))
		{
			return nullptr;
		}

		// Find empty cache slot
		for (int i = 0; i < MAX_CACHED_INTERFACES; ++i)
		{
			if (!s_bCachedValid[i])
			{
				// Cache the interface
				s_pCachedInterfaces[i] = pInterface;
				s_pCachedNames[i] = szName;
				s_bCachedValid[i] = true;
				return pInterface;
			}
		}

		// Cache full, return without caching
		return pInterface;
	}

	// Performance statistics
	static void GetCacheStats(uint32_t& hits, uint32_t& misses, uint32_t& validations)
	{
		hits = s_nCacheHits;
		misses = s_nCacheMisses;
		validations = s_nValidationCalls;
	}

	static void ResetStats()
	{
		s_nCacheHits = 0;
		s_nCacheMisses = 0;
		s_nValidationCalls = 0;
	}

	// Clear cache (for debugging)
	static void ClearCache()
	{
		for (int i = 0; i < MAX_CACHED_INTERFACES; ++i)
		{
			s_pCachedInterfaces[i] = nullptr;
			s_pCachedNames[i] = nullptr;
			s_bCachedValid[i] = false;
		}
		ResetStats();
	}
};

// Static member definitions for enhanced caching system
inline void* CInterfaces::s_pCachedInterfaces[MAX_CACHED_INTERFACES] = {nullptr};
inline const char* CInterfaces::s_pCachedNames[MAX_CACHED_INTERFACES] = {nullptr};
inline bool CInterfaces::s_bCachedValid[MAX_CACHED_INTERFACES] = {false};
inline uint32_t CInterfaces::s_nCacheHits = 0;
inline uint32_t CInterfaces::s_nCacheMisses = 0;
inline uint32_t CInterfaces::s_nValidationCalls = 0;

ADD_FEATURE_CUSTOM(CInterfaces, Interfaces, U);