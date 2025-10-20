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

public:
    // Initialize all cached function pointers
    static void Initialize()
    {
        if (s_bInitialized)
            return;

        // Cache frequently used virtual function pointers
        // This reduces virtual dispatch overhead in hot paths

        if (I::BaseClientDLL)
        {
            // Get vtable pointer
            void** vtable = *(void***)I::BaseClientDLL;
            if (vtable)
            {
                s_pFrameStageNotify = vtable[36]; // FrameStageNotify vtable index
            }
        }

        if (I::ClientModeShared)
        {
            void** vtable = *(void***)I::ClientModeShared;
            if (vtable)
            {
                s_pCreateMove = vtable[24]; // CreateMove vtable index
            }
        }

        if (I::Panel)
        {
            void** vtable = *(void***)I::Panel;
            if (vtable)
            {
                s_pPaintTraverse = vtable[41]; // PaintTraverse vtable index
            }
        }

        if (I::ViewRender)
        {
            void** vtable = *(void***)I::ViewRender;
            if (vtable)
            {
                s_pViewRender = vtable[7]; // RenderView vtable index
            }
        }

        if (I::ModelRender)
        {
            void** vtable = *(void***)I::ModelRender;
            if (vtable)
            {
                s_pModelRender = vtable[21]; // DrawModelExecute vtable index
            }
        }

        if (I::MatSystemSurface)
        {
            void** vtable = *(void***)I::MatSystemSurface;
            if (vtable)
            {
                s_pSurface = vtable[71]; // GetTextSize vtable index
            }
        }

        s_bInitialized = true;
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

    // Validate cached pointers (debugging)
    static bool ValidateCache()
    {
        if (!s_bInitialized)
            return false;

        return (s_pFrameStageNotify != nullptr &&
                s_pCreateMove != nullptr &&
                s_pPaintTraverse != nullptr);
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