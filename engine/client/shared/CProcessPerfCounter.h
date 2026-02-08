#pragma once

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <sys/sysinfo.h>
#include <unistd.h>
#endif

class CProcessPerfCounter
{
public:
    static CProcessPerfCounter* getInstance()
    {
        static CProcessPerfCounter instance;
        return &instance;
    }

    void CollectData()
    {
#ifndef _WIN32
        // Not implemented for Linux
#endif
    }

    double GetProcessCores()
    {
#ifdef _WIN32
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        return sysInfo.dwNumberOfProcessors;
#else
        return get_nprocs();
#endif
    }

    double GetTotalProcessorTime()
    {
#ifdef _WIN32
        return 0.0;
#else
        return 0.0;
#endif
    }

    double GetProcessorTime()
    {
#ifdef _WIN32
        return 0.0;
#else
        return 0.0;
#endif
    }

    double GetPrivateBytes()
    {
#ifdef _WIN32
        PROCESS_MEMORY_COUNTERS pmc;
        GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
        return pmc.PrivateUsage;
#else
        return 0.0;
#endif
    }

    double GetPrivateWorkingSetBytes()
    {
#ifdef _WIN32
        PROCESS_MEMORY_COUNTERS pmc;
        GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
        return pmc.WorkingSetSize;
#else
        return 0.0;
#endif
    }

    double GetElapsedTime()
    {
#ifdef _WIN32
        return 0.0;
#else
        return 0.0;
#endif
    }

    double GetVirtualBytes()
    {
#ifdef _WIN32
        PROCESS_MEMORY_COUNTERS pmc;
        GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
        return pmc.PagefileUsage;
#else
        struct sysinfo memInfo;
        sysinfo(&memInfo);
        long long virtualMemUsed = memInfo.totalram - memInfo.freeram;
        virtualMemUsed += memInfo.totalswap - memInfo.freeswap;
        virtualMemUsed *= memInfo.mem_unit;
        return virtualMemUsed;
#endif
    }

    double GetPageFileBytes()
    {
#ifdef _WIN32
        PROCESS_MEMORY_COUNTERS pmc;
        GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
        return pmc.PagefileUsage;
#else
        return 0.0;
#endif
    }

    double GetPageFaultsPerSecond()
    {
#ifdef _WIN32
        PROCESS_MEMORY_COUNTERS pmc;
        GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
        return pmc.PageFaultCount;
#else
        return 0.0;
#endif
    }
};
