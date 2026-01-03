#pragma once
#include <Windows.h>
#include <cstdint>

class CSafeAccess
{
private:
	static PVOID m_hVectoredHandler;
	static uintptr_t m_pNullObjectPage;
	static bool m_bInitialized;

	static LONG WINAPI VectoredExceptionHandler(PEXCEPTION_POINTERS pExceptionInfo);
	static void AllocateNullObjectPage();

public:
	void Initialize();
	void Shutdown();
};

namespace U { inline CSafeAccess SafeAccess; }
