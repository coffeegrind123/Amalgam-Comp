#include "HookCache.h"

// Static member definitions
void* CHookCache::s_pFrameStageNotify = nullptr;
void* CHookCache::s_pCreateMove = nullptr;
void* CHookCache::s_pPaintTraverse = nullptr;
void* CHookCache::s_pViewRender = nullptr;
void* CHookCache::s_pModelRender = nullptr;
void* CHookCache::s_pSurface = nullptr;
bool CHookCache::s_bInitialized = false;

// Global instance for RAII initialization
static CHookCacheInitializer g_HookCacheInitializer;