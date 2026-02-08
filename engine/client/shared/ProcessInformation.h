#pragma once

#ifdef _WIN32
    #include <windows.h>
    #include <psapi.h>
#else
    #include <sys/types.h>
    #include <sys/sysinfo.h>
    #include <unistd.h>
#endif

namespace RBX
{
    class ProcessInformation
    {
    private:
#ifdef _WIN32
        PROCESS_INFORMATION pi;
        bool inUse;
#endif

    public:
        ProcessInformation() {
#ifdef _WIN32
            ZeroMemory(&pi, sizeof(pi));
            inUse = false;
#endif
        }

        ~ProcessInformation() {
            #ifdef _WIN32
            CloseProcess();
            #endif
        }

        // Memory Usage Helper
        static size_t GetVirtualMemoryUsage() {
#ifdef _WIN32
            PROCESS_MEMORY_COUNTERS pmc;
            if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
                return pmc.PagefileUsage;
            return 0;
#else
            struct sysinfo memInfo;
            sysinfo(&memInfo);
            long long virtualMemUsed = memInfo.totalram - memInfo.freeram;
            virtualMemUsed += memInfo.totalswap - memInfo.freeswap;
            virtualMemUsed *= memInfo.mem_unit;
            return (size_t)virtualMemUsed;
#endif
        }

#ifdef _WIN32
        // Compatibility for SharedLauncher
        operator LPPROCESS_INFORMATION() { return &pi; }

        bool InUse() const { return inUse; }
        void SetInUse(bool val) { inUse = val; }

        void CloseProcess() {
            if (pi.hProcess) {
                CloseHandle(pi.hProcess);
                pi.hProcess = NULL;
            }
            if (pi.hThread) {
                CloseHandle(pi.hThread);
                pi.hThread = NULL;
            }
            inUse = false;
        }

        DWORD WaitForSingleObject(DWORD timeout) {
            if (!pi.hProcess) return WAIT_FAILED;
            return ::WaitForSingleObject(pi.hProcess, timeout);
        }

        bool GetExitCode(DWORD& exitCode) {
            if (!pi.hProcess) return false;
            return ::GetExitCodeProcess(pi.hProcess, &exitCode) != 0;
        }
#endif
    };
}

// Global alias to fix the "not declared" error in SharedLauncher
typedef RBX::ProcessInformation CProcessInformation;
