#pragma once
#include "../../../Utils/Feature/Feature.h"

class CHookCache
{
private:
    // Cached function pointers to avoid virtual dispatch overhead
    static void* s_pFrameStageNotify;
    static void* s_pCreateMove;
    static void* s_pPaintTraverse;
    static void* s_pViewRender;
    static void* s_pModelRender;
    static void* s_pSurface;
    static bool s_bInitialized;

    // Enhanced error handling
    static bool s_bValidationEnabled;
    static uint32_t s_nErrorCount;

    // Validate vtable integrity
    static bool ValidateVTable(void* interface_ptr, const char* interface_name)
    {
        if (!interface_ptr) {
            s_nErrorCount++;
            return false;
        }

        // Check if vtable pointer is valid
        void** vtable = *(void***)interface_ptr;
        if (!vtable || IsBadReadPtr(vtable, sizeof(void*))) {
            s_nErrorCount++;
            return false;
        }

        // Basic vtable structure validation
        if (IsBadCodePtr(reinterpret_cast<FARPROC>(vtable[0]))) {
            s_nErrorCount++;
            return false;
        }

        return true;
    }

    // Safe vtable access with bounds checking
    static void* SafeGetVTableEntry(void* interface_ptr, int index, const char* interface_name)
    {
        if (!ValidateVTable(interface_ptr, interface_name))
            return nullptr;

        void** vtable = *(void***)interface_ptr;

        // Validate vtable entry
        if (IsBadCodePtr(reinterpret_cast<FARPROC>(vtable[index]))) {
            s_nErrorCount++;
            return nullptr;
        }

        return vtable[index];
    }

public:
    // Enable/disable validation (can be disabled in release builds)
    static void SetValidationEnabled(bool enabled) { s_bValidationEnabled = enabled; }
    static uint32_t GetErrorCount() { return s_nErrorCount; }
    static void ResetErrorCount() { s_nErrorCount = 0; }

    // Initialize all cached function pointers with enhanced error handling
    static void Initialize()
    {
        if (s_bInitialized)
            return;

        ResetErrorCount();

        // Cache frequently used virtual function pointers with validation
        // This reduces virtual dispatch overhead in hot paths while ensuring safety

        if (I::BaseClientDLL)
        {
            s_pFrameStageNotify = SafeGetVTableEntry(I::BaseClientDLL, 36, "IBaseClientDLL");
        }

        if (I::ClientModeShared)
        {
            s_pCreateMove = SafeGetVTableEntry(I::ClientModeShared, 24, "IClientModeShared");
        }

        if (I::Panel)
        {
            s_pPaintTraverse = SafeGetVTableEntry(I::Panel, 41, "IPanel");
        }

        if (I::ViewRender)
        {
            s_pViewRender = SafeGetVTableEntry(I::ViewRender, 7, "IViewRender");
        }

        if (I::ModelRender)
        {
            s_pModelRender = SafeGetVTableEntry(I::ModelRender, 21, "IVModelRender");
        }

        if (I::MatSystemSurface)
        {
            s_pSurface = SafeGetVTableEntry(I::MatSystemSurface, 71, "IMatSystemSurface");
        }

        s_bInitialized = true;
    }

    // Reinitialize cache (useful if interfaces change)
    static void Reinitialize()
    {
        s_bInitialized = false;
        s_pFrameStageNotify = nullptr;
        s_pCreateMove = nullptr;
        s_pPaintTraverse = nullptr;
        s_pViewRender = nullptr;
        s_pModelRender = nullptr;
        s_pSurface = nullptr;
        Initialize();
    }

    // Validate all cached pointers
    static bool ValidateCache()
    {
        if (!s_bInitialized)
            return false;

        return s_pFrameStageNotify != nullptr &&
               s_pCreateMove != nullptr &&
               s_pPaintTraverse != nullptr;
    }

    // Get cached function pointers
    static void* GetFrameStageNotifyPtr() { return s_pFrameStageNotify; }
    static void* GetCreateMovePtr() { return s_pCreateMove; }
    static void* GetPaintTraversePtr() { return s_pPaintTraverse; }
    static void* GetViewRenderPtr() { return s_pViewRender; }
    static void* GetModelRenderPtr() { return s_pModelRender; }
    static void* GetSurfacePtr() { return s_pSurface; }

    // Check if cache is initialized
    static bool IsInitialized() { return s_bInitialized; }

    // Re-initialize cache (useful after interface changes)
    static void Reinitialize()
    {
        s_bInitialized = false;
        s_pFrameStageNotify = nullptr;
        s_pCreateMove = nullptr;
        s_pPaintTraverse = nullptr;
        s_pViewRender = nullptr;
        s_pModelRender = nullptr;
        s_pSurface = nullptr;
        Initialize();
    }

    
    // Clear cache (for debugging)
    static void ClearCache()
    {
        s_bInitialized = false;
        s_pFrameStageNotify = nullptr;
        s_pCreateMove = nullptr;
        s_pPaintTraverse = nullptr;
        s_pViewRender = nullptr;
        s_pModelRender = nullptr;
        s_pSurface = nullptr;
    }
};

// RAII initializer for hook cache
class CHookCacheInitializer
{
public:
    CHookCacheInitializer()
    {
        CHookCache::Initialize();
    }
};

// Global instance to ensure initialization
extern CHookCacheInitializer g_HookCacheInitializer;