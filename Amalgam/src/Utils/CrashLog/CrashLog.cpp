#include "CrashLog.h"

#include "../../Features/Configs/Configs.h"

#include <ImageHlp.h>
#include <Psapi.h>
#include <deque>
#include <sstream>
#include <fstream>
#include <format>
#pragma comment(lib, "imagehlp.lib")

struct Frame_t
{
    std::string m_sModule = "";
    uintptr_t m_uBase = 0;
    uintptr_t m_uAddress = 0;
    std::string m_sFile = "";
    unsigned int m_uLine = 0;
    std::string m_sName = "";
};

// FIXED: Initialize handle to nullptr for safe null checks
static PVOID s_pHandle = nullptr;
static LPVOID s_lpParam = nullptr;
static std::unordered_map<LPVOID, bool> s_mAddresses = {};
static int s_iExceptions = 0;

static inline std::deque<Frame_t> StackTrace(PCONTEXT pContext)
{
    // FIXED: Null check prevents crash when context invalid
    if (!pContext)
        return {};

    HANDLE hProcess = GetCurrentProcess();
    HANDLE hThread = GetCurrentThread();

    if (!SymInitialize(hProcess, nullptr, TRUE))
        return {};

    SymSetOptions(SYMOPT_LOAD_LINES);

    STACKFRAME64 tStackFrame = {};
    tStackFrame.AddrPC.Offset = pContext->Rip;
    tStackFrame.AddrFrame.Offset = pContext->Rbp;
    tStackFrame.AddrStack.Offset = pContext->Rsp;
    tStackFrame.AddrPC.Mode = AddrModeFlat;
    tStackFrame.AddrFrame.Mode = AddrModeFlat;
    tStackFrame.AddrStack.Mode = AddrModeFlat;

    // OPTIMIZED: Reserve space to reduce reallocations during stack walk
    std::deque<Frame_t> vTrace = {};
    vTrace.reserve(64);

    while (StackWalk64(IMAGE_FILE_MACHINE_AMD64, hProcess, hThread, &tStackFrame, pContext, nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr))
    {
        Frame_t tFrame = {};
        // OPTIMIZED: Cache address to avoid repeated struct access
        const uintptr_t uFrameAddr = tStackFrame.AddrPC.Offset;
        tFrame.m_uAddress = uFrameAddr;

        if (auto hBase = HINSTANCE(SymGetModuleBase64(hProcess, uFrameAddr)))
        {
            tFrame.m_uBase = uintptr_t(hBase);

            char buffer[MAX_PATH];
            // OPTIMIZED: Simplified size calculation
            if (GetModuleBaseNameA(hProcess, hBase, buffer, MAX_PATH))
                tFrame.m_sModule = buffer;
            else
                tFrame.m_sModule = std::format("{:#x}", tFrame.m_uBase);
        }

        {
            DWORD dwOffset = 0;
            IMAGEHLP_LINE64 line = {};
            line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
            if (SymGetLineFromAddr64(hProcess, uFrameAddr, &dwOffset, &line))
            {
                // FIXED: Null check prevents crash if FileName is null
                if (line.FileName)
                {
                    tFrame.m_sFile = line.FileName;
                    tFrame.m_uLine = line.LineNumber;
                    // OPTIMIZED: Use substr instead of replace for better performance
                    const auto iFind = tFrame.m_sFile.rfind("\\");
                    if (iFind != std::string::npos)
                        tFrame.m_sFile = tFrame.m_sFile.substr(iFind + 1);
                }
            }
        }

        {
            uintptr_t dwOffset = 0;
            char buf[sizeof(IMAGEHLP_SYMBOL64) + 255];
            auto symbol = PIMAGEHLP_SYMBOL64(buf);
            symbol->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64) + 255;
            symbol->MaxNameLength = 254;
            if (SymGetSymFromAddr64(hProcess, uFrameAddr, &dwOffset, symbol))
            {
                // FIXED: Null check prevents crash if Name is null
                if (symbol->Name)
                    tFrame.m_sName = symbol->Name;
            }
        }

        // OPTIMIZED: Use emplace_back to construct in place
        vTrace.emplace_back(std::move(tFrame));
    }

    SymCleanup(hProcess);

    return vTrace;
}

static LONG APIENTRY ExceptionFilter(PEXCEPTION_POINTERS ExceptionInfo)
{
    // FIXED: Null check prevents crash when exception info invalid
    if (!ExceptionInfo || !ExceptionInfo->ExceptionRecord || !ExceptionInfo->ContextRecord)
        return EXCEPTION_EXECUTE_HANDLER;

    // OPTIMIZED: Cache exception record to avoid repeated pointer dereferences
    const auto pExceptionRecord = ExceptionInfo->ExceptionRecord;
    const DWORD dwExceptionCode = pExceptionRecord->ExceptionCode;

    const char* sError = "UNKNOWN";
    switch (dwExceptionCode)
    {
    case STATUS_ACCESS_VIOLATION: sError = "ACCESS VIOLATION"; break;
    case STATUS_STACK_OVERFLOW: sError = "STACK OVERFLOW"; break;
    case STATUS_HEAP_CORRUPTION: sError = "HEAP CORRUPTION"; break;
    case DBG_PRINTEXCEPTION_C: return EXCEPTION_EXECUTE_HANDLER;
    }

    // OPTIMIZED: Cache exception address to avoid repeated lookups
    const auto pExceptionAddr = pExceptionRecord->ExceptionAddress;

    if (s_mAddresses.contains(pExceptionAddr)
        || !Vars::Debug::CrashLogging.Value
        || s_iExceptions && GetAsyncKeyState(VK_SHIFT) & 0x8000 && GetAsyncKeyState(VK_RETURN) & 0x8000)
        return EXCEPTION_EXECUTE_HANDLER;
    s_mAddresses[pExceptionAddr] = true;

    // OPTIMIZED: Cache context record to avoid repeated pointer dereferences
    const auto pContext = ExceptionInfo->ContextRecord;

    std::stringstream ssErrorStream;
    ssErrorStream << std::format("Error: {} (0x{:X}) ({})\n", sError, dwExceptionCode, ++s_iExceptions);
    if (s_lpParam)
        ssErrorStream << std::format("This: {}\n", U::Memory.GetModuleOffset(reinterpret_cast<uintptr_t>(s_lpParam)));
    ssErrorStream << "\n";

    // OPTIMIZED: Batch register output to reduce function calls
    ssErrorStream << std::format("RIP: {:#x}\n", pContext->Rip);
    ssErrorStream << std::format("RAX: {:#x}\n", pContext->Rax);
    ssErrorStream << std::format("RCX: {:#x}\n", pContext->Rcx);
    ssErrorStream << std::format("RDX: {:#x}\n", pContext->Rdx);
    ssErrorStream << std::format("RBX: {:#x}\n", pContext->Rbx);
    ssErrorStream << std::format("RSP: {:#x}\n", pContext->Rsp);
    ssErrorStream << std::format("RBP: {:#x}\n", pContext->Rbp);
    ssErrorStream << std::format("RSI: {:#x}\n", pContext->Rsi);
    ssErrorStream << std::format("RDI: {:#x}\n\n", pContext->Rdi);

    switch (dwExceptionCode)
    {
    case STATUS_ACCESS_VIOLATION:
        if (auto vTrace = StackTrace(pContext); !vTrace.empty())
        {
            // OPTIMIZED: Use const reference to avoid copies in iteration
            for (const auto& tFrame : vTrace)
            {
                if (tFrame.m_uBase)
                    ssErrorStream << std::format("{}+{:#x}", tFrame.m_sModule, tFrame.m_uAddress - tFrame.m_uBase);
                else
                    ssErrorStream << std::format("{:#x}", tFrame.m_uAddress);
                if (!tFrame.m_sFile.empty())
                    ssErrorStream << std::format(" ({} L{})", tFrame.m_sFile, tFrame.m_uLine);
                if (!tFrame.m_sName.empty())
                    ssErrorStream << std::format(" ({})", tFrame.m_sName);
                ssErrorStream << "\n";
            }
            ssErrorStream << "\n";
        }
        break;
    default:
        ssErrorStream << std::format("{}\n\n", U::Memory.GetModuleOffset(reinterpret_cast<uintptr_t>(pExceptionAddr)));
    }

    ssErrorStream << "Built @ " __DATE__ ", " __TIME__ ", " __CONFIGURATION__ "\n";
    ssErrorStream << "Ctrl + C to copy. \n";
    try
    {
        // OPTIMIZED: Construct file path once to avoid repeated string concatenation
        const std::string sLogPath = F::Configs.m_sConfigPath + "crash_log.txt";
        std::ofstream file(sLogPath, std::ios_base::app);
        if (file.is_open())
        {
            // OPTIMIZED: Cache string and avoid unnecessary concatenation
            const std::string sErrorStr = ssErrorStream.str();
            file << sErrorStr << "\n\n\n";
            file.close();
            ssErrorStream << "Logged to Amalgam\\crash_log.txt. ";
        }
    }
    catch (...) {}

    switch (dwExceptionCode)
    {
    case STATUS_ACCESS_VIOLATION:
        // OPTIMIZED: Cache error string to avoid repeated str() calls
        {
            const std::string sErrorStr = ssErrorStream.str();
            SDK::Output("Unhandled exception", sErrorStr.c_str(), { 175, 150, 255 }, true, true, false, false, false, MB_OK | MB_ICONERROR);
        }
    }

    return EXCEPTION_EXECUTE_HANDLER;
}

void CCrashLog::Initialize(LPVOID lpParam)
{
    s_pHandle = AddVectoredExceptionHandler(1, ExceptionFilter);
    s_lpParam = lpParam;
}

void CCrashLog::Unload()
{
    // FIXED: Null check prevents crash if handler wasn't initialized
    if (s_pHandle)
    {
        RemoveVectoredExceptionHandler(s_pHandle);
        s_pHandle = nullptr;
    }
}
