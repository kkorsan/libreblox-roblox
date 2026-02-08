#ifdef _WIN32
    #define NOMINMAX
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <dbghelp.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <time.h>

    // MinGW does not always have strsafe.h. Using standard snprintf instead.
    #ifndef _MSC_VER
        #include <stdint.h>
        #define StringCchPrintf(dst, size, fmt, ...) snprintf(dst, size, fmt, __VA_ARGS__)
        #define StringCchCopy(dst, size, src) (strncpy(dst, src, size), dst[size-1] = '\0')
        #define StringCchCat(dst, size, src) strncat(dst, src, size - strlen(dst) - 1)
        #define __out_ecount(x)
    #else
        #include <strsafe.h>
    #endif
#endif

#include "util/Analytics.h"
#include "CrashReporter.h"
#include "raknet/EmailSender.h"
#include "raknet/FileList.h"
#include "raknet/FileOperations.h"
#include "rbx/TaskScheduler.h"
#include "rbx/Log.h"
#include "rbx/CEvent.h"
#include "rbx/Debug.h"
#include "rbx/Boost.hpp"
#include "FastLog.h"
#include "util/standardout.h"
#include "boost/bind.hpp"

#ifdef _MSC_VER
    #pragma optimize("", off)
    #pragma warning ( disable : 4996 )
#endif

CrashReporter* CrashReporter::singleton = NULL;

LOGGROUP(HangDetection)
LOGGROUP(Crash)
DYNAMIC_FASTINTVARIABLE(WriteFullDmpPercent, 0)

CrashReporter::CrashReporter() :
    threadResult(0), exceptionInfo(NULL), reportCrashEvent(FALSE),
    hangReportingEnabled(false), isAlive(true), deadlockCounter(0),
    destructing(false), immediateUploadEnabled(true)
{
    assert(singleton == NULL);
    singleton = this;
}

CrashReporter::~CrashReporter()
{
    destructing = true;
    if(watcherThread)
    {
        FASTLOG(FLog::Crash, "Closing crashreporter thread on destroy");
        reportCrashEvent.Set();
        watcherThread->join();
    }
}

HRESULT CrashReporter::GenerateDmpFileName(char* dumpFilepath, int cchdumpFilepath, bool fastLog, bool fullDmp)
{
    char appDescriptor[_MAX_PATH];
    char dumpFilename[_MAX_PATH];
    HRESULT hr = S_OK;

    StringCchPrintf(appDescriptor, _MAX_PATH, "%s %s", controls.appName, controls.appVersion);

    StringCchCopy(dumpFilepath, cchdumpFilepath, controls.pathToMinidump);
    WriteFileWithDirectories(dumpFilepath, 0, 0);
    AddSlash(dumpFilepath);

    unsigned i, dumpFilenameLen;
    StringCchCopy(dumpFilename, _MAX_PATH, appDescriptor);
    dumpFilenameLen = (unsigned)strlen(appDescriptor);
    for (i = 0; i < dumpFilenameLen; i++)
        if (dumpFilename[i] == ':')
            dumpFilename[i] = '.';

    StringCchCat(dumpFilepath, cchdumpFilepath, dumpFilename);

    if (fullDmp)
        StringCchCat(dumpFilepath, cchdumpFilepath, ".Full");

    StringCchCat(dumpFilepath, cchdumpFilepath, fastLog ? ".txt" : controls.crashExtention);

    return S_OK;
}

LONG CrashReporter::ProcessExceptionHelper(struct _EXCEPTION_POINTERS *ExceptionInfo, bool writeFullDmp, bool noMsg, char* dumpFilepath)
{
    int dumpType = controls.minidumpType;
    if (writeFullDmp)
        dumpType |= MiniDumpWithFullMemory;

    if (FAILED(GenerateDmpFileName(dumpFilepath, _MAX_PATH, false, writeFullDmp)))
        return EXCEPTION_CONTINUE_SEARCH;

    HANDLE hFile = CreateFile(dumpFilepath, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return EXCEPTION_CONTINUE_SEARCH;

    MINIDUMP_EXCEPTION_INFORMATION eInfo;
    eInfo.ThreadId = GetCurrentThreadId();
    eInfo.ExceptionPointers = ExceptionInfo;
    eInfo.ClientPointers = FALSE;

    LONG result = EXCEPTION_EXECUTE_HANDLER;
    if (!MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, (MINIDUMP_TYPE)dumpType, ExceptionInfo ? &eInfo : NULL, NULL, NULL))
    {
        DWORD hr = GetLastError();
        DWORD bytesWritten = 0;
        char msg[1024];
        sprintf(msg, "MiniDumpWriteDump() failed with hr = 0x%08lx", hr);
        WriteFile(hFile, msg, (DWORD)(strlen(msg) + 1), &bytesWritten, NULL);
        result = EXCEPTION_CONTINUE_SEARCH;
    }

    CloseHandle(hFile);
    return result;
}

namespace RBX {
    std::string specialCrashType = "first";
    bool gCrashIsSpecial = false;
}

LONG CrashReporter::ProcessException(struct _EXCEPTION_POINTERS *ExceptionInfo, bool noMsg)
{
    char dumpFilepath[_MAX_PATH] = {};
    LONG result = EXCEPTION_EXECUTE_HANDLER;

#ifdef RBX_RCC_SECURITY
    if (RBX::gCrashIsSpecial)
    {
        result = ProcessExceptionHelper(ExceptionInfo, false, noMsg, dumpFilepath);
        RBX::Analytics::InfluxDb::Points analyticsPoints;
        size_t startPoint = 0;
        for (size_t i = 0; i < _MAX_PATH; ++i)
        {
            if (dumpFilepath[i] == '\\') startPoint = i + 1;
            else if (dumpFilepath[i] == 0) break;
        }
        analyticsPoints.addPoint("type", RBX::specialCrashType.c_str());
        analyticsPoints.addPoint("path", &dumpFilepath[startPoint]);
        analyticsPoints.report("report", 10000, true);
        RBX::Analytics::GoogleAnalytics::trackEventWithoutThrottling("Error", "LoggableCrash", &dumpFilepath[startPoint], 0, true);
    }
    else
#endif
    {
        bool full = (rand() % 100 < DFInt::WriteFullDmpPercent);
        result = ProcessExceptionHelper(ExceptionInfo, full, noMsg, dumpFilepath);
    }
    return result;
}

LONG CrashReporter::ProcessExceptionInThead(struct _EXCEPTION_POINTERS *excpInfo)
{
    if(watcherThread)
    {
        exceptionInfo = excpInfo;
        reportCrashEvent.Set();
        watcherThread->join();
        watcherThread.reset();
        return this->threadResult;
    }
    return ProcessException(excpInfo, false);
}

void CrashReporter::TheadFunc(struct _EXCEPTION_POINTERS *excpInfo)
{
    this->threadResult = ProcessException(excpInfo, false);
}

static LONG ProcessExceptionStatic(PEXCEPTION_POINTERS excpInfo)
{
    if (excpInfo == NULL)
    {
        // MinGW does not support __try/__except blocks like MSVC.
        // We use RaiseException and let the UnhandledExceptionFilter catch it.
        RaiseException(EXCEPTION_BREAKPOINT, 0, 0, NULL);
        return EXCEPTION_EXECUTE_HANDLER;
    }
    return CrashReporter::singleton->ProcessExceptionInThead(excpInfo);
}

LONG WINAPI CrashExceptionFilter(struct _EXCEPTION_POINTERS *excpInfo)
{
    LONG result = ProcessExceptionStatic(excpInfo);
    _exit(EXIT_FAILURE);
}

LPTOP_LEVEL_EXCEPTION_FILTER WINAPI MyDummySetUnhandledExceptionFilter(LPTOP_LEVEL_EXCEPTION_FILTER lpTopLevelExceptionFilter)
{
    return NULL;
}

BOOL PreventSetUnhandledExceptionFilter()
{
    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    if (hKernel32 == NULL) return FALSE;
    void *pOrgEntry = (void*)GetProcAddress(hKernel32, "SetUnhandledExceptionFilter");
    if (pOrgEntry == NULL) return FALSE;

    unsigned char newJump[12];
    uintptr_t dwOrgEntryAddr = (uintptr_t)pOrgEntry;
    uintptr_t dwNewEntryAddr = (uintptr_t)&MyDummySetUnhandledExceptionFilter;

#ifdef _M_IX86
    // 32-bit JMP
    uintptr_t dwRelativeAddr = dwNewEntryAddr - (dwOrgEntryAddr + 5);
    newJump[0] = 0xE9;
    memcpy(&newJump[1], &dwRelativeAddr, 4);
    SIZE_T patchSize = 5;
#else
    // 64-bit absolute JMP (standard MinGW-w64)
    newJump[0] = 0x48; newJump[1] = 0xB8; // MOV RAX, <addr>
    memcpy(&newJump[2], &dwNewEntryAddr, 8);
    newJump[10] = 0xFF; newJump[11] = 0xE0; // JMP RAX
    SIZE_T patchSize = 12;
#endif

    DWORD oldProtect;
    VirtualProtect(pOrgEntry, patchSize, PAGE_EXECUTE_READWRITE, &oldProtect);
    SIZE_T bytesWritten;
    BOOL bRet = WriteProcessMemory(GetCurrentProcess(), pOrgEntry, newJump, patchSize, &bytesWritten);
    VirtualProtect(pOrgEntry, patchSize, oldProtect, &oldProtect);

    return bRet;
}

void fixExceptionsThroughKernel()
{
    HMODULE hKernelDll = GetModuleHandleA("kernel32.dll");
    if(hKernelDll)
    {
        typedef BOOL (WINAPI* _SetPolicy)(DWORD);
        typedef BOOL (WINAPI* _GetPolicy)(LPDWORD);
        _SetPolicy set = (_SetPolicy)GetProcAddress(hKernelDll, "SetProcessUserModeExceptionPolicy");
        _GetPolicy get = (_GetPolicy)GetProcAddress(hKernelDll, "GetProcessUserModeExceptionPolicy");

        if (set && get)
        {
            DWORD dwFlags;
            if (get(&dwFlags)) set(dwFlags & ~0x1);
        }
    }
}

void CrashReporter::Start()
{
    SetUnhandledExceptionFilter(CrashExceptionFilter);
    PreventSetUnhandledExceptionFilter();
    watcherThread.reset(new boost::thread(boost::bind(&CrashReporter::WatcherThreadFunc, CrashReporter::singleton)));
}

void CrashReporter::WatcherThreadFunc()
{
    RBX::set_thread_name("CrashReporter_WatcherThreadFunc");
    bool quit = false;
    while(!quit && !destructing)
    {
        if(!reportCrashEvent.Wait(180000))
        {
            if(!isAlive && hangReportingEnabled)
            {
                if(deadlockCounter++ == 0)
                {
                    RBX::TaskScheduler::singleton().printJobs();
                    // Simplified: Raise exception and let filter handle it
                    RaiseException(EXCEPTION_BREAKPOINT, 0, 0, NULL);
                }
            }
            isAlive = false;
        }
        else if(!destructing)
        {
            threadResult = ProcessException(exceptionInfo, false);
            quit = true;
            LaunchUploadProcess();
        }
    }
}

void CrashReporter::LaunchUploadProcess()
{
    if (!immediateUploadEnabled) return;

    char filename[MAX_PATH];
    if (GetModuleFileNameA(NULL, filename, MAX_PATH))
    {
        if (!strstr(filename, "RCCService"))
        {
            STARTUPINFO si = {sizeof(si)};
            PROCESS_INFORMATION pi;
            char cmd[512];
            snprintf(cmd, sizeof(cmd), "\"%s\" -d", filename);
            CreateProcess(NULL, cmd, NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }
}

void CrashReporter::NotifyAlive() { hangReportingEnabled = true; isAlive = true; }
void CrashReporter::DisableHangReporting() { hangReportingEnabled = false; }
void CrashReporter::EnableImmediateUpload(bool enabled) { immediateUploadEnabled = enabled; }
