#include "SafeAccess.h"
#include <memoryapi.h>
#include <stdio.h>

PVOID CSafeAccess::m_hVectoredHandler = nullptr;
uintptr_t CSafeAccess::m_pNullObjectPage = 0;
bool CSafeAccess::m_bInitialized = false;

void CSafeAccess::AllocateNullObjectPage()
{
	// Allocate a large block of zero-initialized memory (1MB)
	// When we catch null pointer access, we'll redirect to this memory
	m_pNullObjectPage = reinterpret_cast<uintptr_t>(
		VirtualAlloc(nullptr, 1024 * 1024, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)
	);

	if (m_pNullObjectPage)
	{
		// Zero out the entire block
		memset(reinterpret_cast<void*>(m_pNullObjectPage), 0, 1024 * 1024);
	}
}

LONG WINAPI CSafeAccess::VectoredExceptionHandler(PEXCEPTION_POINTERS pExceptionInfo)
{
	if (!pExceptionInfo || !m_bInitialized || !m_pNullObjectPage)
		return EXCEPTION_CONTINUE_SEARCH;

	DWORD exceptionCode = pExceptionInfo->ExceptionRecord->ExceptionCode;

	// Only handle access violations
	if (exceptionCode != EXCEPTION_ACCESS_VIOLATION)
		return EXCEPTION_CONTINUE_SEARCH;

	// Get the faulting address
	uintptr_t faultAddress = static_cast<uintptr_t>(
		pExceptionInfo->ExceptionRecord->ExceptionInformation[1]
	);

	// Check if this is a null pointer access (within first 1MB)
	// This range covers nullptr + offsets for member variables and vtables
	if (faultAddress < 0x100000)
	{
		DWORD accessType = static_cast<DWORD>(
			pExceptionInfo->ExceptionRecord->ExceptionInformation[0]
		);

#ifdef _WIN64
		PCONTEXT ctx = pExceptionInfo->ContextRecord;

		// Strategy: Redirect the base pointer to our safe memory block
		// This allows the instruction to execute normally without modification

		// The faulting instruction is trying to access memory at faultAddress
		// We need to find which register holds the base pointer and redirect it

		// Common patterns:
		// mov rax, [rcx+offset] - RCX is base
		// mov rax, [rax+offset] - RAX is base
		// mov rax, [rdx+offset] - RDX is base

		// Check each register to see if it's in the null range
		if (ctx->Rax < 0x10000) ctx->Rax = m_pNullObjectPage + ctx->Rax;
		if (ctx->Rbx < 0x10000) ctx->Rbx = m_pNullObjectPage + ctx->Rbx;
		if (ctx->Rcx < 0x10000) ctx->Rcx = m_pNullObjectPage + ctx->Rcx;
		if (ctx->Rdx < 0x10000) ctx->Rdx = m_pNullObjectPage + ctx->Rdx;
		if (ctx->Rsi < 0x10000) ctx->Rsi = m_pNullObjectPage + ctx->Rsi;
		if (ctx->Rdi < 0x10000) ctx->Rdi = m_pNullObjectPage + ctx->Rdi;
		if (ctx->Rbp < 0x10000) ctx->Rbp = m_pNullObjectPage + ctx->Rbp;
		if (ctx->R8 < 0x10000)  ctx->R8  = m_pNullObjectPage + ctx->R8;
		if (ctx->R9 < 0x10000)  ctx->R9  = m_pNullObjectPage + ctx->R9;
		if (ctx->R10 < 0x10000) ctx->R10 = m_pNullObjectPage + ctx->R10;
		if (ctx->R11 < 0x10000) ctx->R11 = m_pNullObjectPage + ctx->R11;
		if (ctx->R12 < 0x10000) ctx->R12 = m_pNullObjectPage + ctx->R12;
		if (ctx->R13 < 0x10000) ctx->R13 = m_pNullObjectPage + ctx->R13;
		if (ctx->R14 < 0x10000) ctx->R14 = m_pNullObjectPage + ctx->R14;
		if (ctx->R15 < 0x10000) ctx->R15 = m_pNullObjectPage + ctx->R15;

		// Let the instruction execute with the corrected pointers
		return EXCEPTION_CONTINUE_EXECUTION;
#else
		// 32-bit version
		PCONTEXT ctx = pExceptionInfo->ContextRecord;
		if (ctx->Eax < 0x10000) ctx->Eax = static_cast<DWORD>(m_pNullObjectPage + ctx->Eax);
		if (ctx->Ebx < 0x10000) ctx->Ebx = static_cast<DWORD>(m_pNullObjectPage + ctx->Ebx);
		if (ctx->Ecx < 0x10000) ctx->Ecx = static_cast<DWORD>(m_pNullObjectPage + ctx->Ecx);
		if (ctx->Edx < 0x10000) ctx->Edx = static_cast<DWORD>(m_pNullObjectPage + ctx->Edx);
		if (ctx->Esi < 0x10000) ctx->Esi = static_cast<DWORD>(m_pNullObjectPage + ctx->Esi);
		if (ctx->Edi < 0x10000) ctx->Edi = static_cast<DWORD>(m_pNullObjectPage + ctx->Edi);
		if (ctx->Ebp < 0x10000) ctx->Ebp = static_cast<DWORD>(m_pNullObjectPage + ctx->Ebp);
		return EXCEPTION_CONTINUE_EXECUTION;
#endif
	}

	// Not a null pointer we can handle - let other handlers try
	return EXCEPTION_CONTINUE_SEARCH;
}

void CSafeAccess::Initialize()
{
	if (m_bInitialized)
		return;

	// Allocate null object page
	AllocateNullObjectPage();

	// Register vectored exception handler
	// Use 1 (first handler) to catch before debuggers
	m_hVectoredHandler = AddVectoredExceptionHandler(1, VectoredExceptionHandler);

	m_bInitialized = (m_hVectoredHandler != nullptr);
}

void CSafeAccess::Shutdown()
{
	if (!m_bInitialized)
		return;

	// Remove exception handler
	if (m_hVectoredHandler)
	{
		RemoveVectoredExceptionHandler(m_hVectoredHandler);
		m_hVectoredHandler = nullptr;
	}

	// Free null object page
	if (m_pNullObjectPage)
	{
		VirtualFree(reinterpret_cast<void*>(m_pNullObjectPage), 0, MEM_RELEASE);
		m_pNullObjectPage = 0;
	}

	m_bInitialized = false;
}
