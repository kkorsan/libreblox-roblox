#ifdef _WIN32
    #include "stdafx.h"
    #include <windows.h>
#else
    #include <unistd.h>
    #include <string.h>
    #include <link.h> // For dl_iterate_phdr
#endif

#include "util/ProgramMemoryChecker.h"
#include "rbx/rbxTime.h"
#include "FastLog.h"
#include "rbx/Debug.h"
#include "util/xxhash.h"
#include <boost/algorithm/string.hpp>

#include "security/ApiSecurity.h"
#include "v8datamodel/HackDefines.h"
#include "humanoid/HumanoidState.h"
#include "g3d/Vector3.h"
#include "g3d/Matrix3.h"

// --- Cross-Platform Macros ---
#if defined(_MSC_VER)
    #define FORCE_INLINE __forceinline
    #define ALIGN_8 __declspec(align(8))
    #define PUSH_OPTIMIZE_OFF __pragma(optimize("", off))
    #define POP_OPTIMIZE __pragma(optimize("", on))
#else
    #define FORCE_INLINE __attribute__((always_inline)) inline
    #define ALIGN_8 __attribute__((aligned(8)))
    #define PUSH_OPTIMIZE_OFF _Pragma("GCC push_options") _Pragma("GCC optimize (\"O0\")")
    #define POP_OPTIMIZE _Pragma("GCC pop_options")
#endif

LOGGROUP(US14116)
FASTFLAG(DebugLocalRccServerConnection)

namespace {
    struct PhdrCallbackData {
        const char* moduleName;
        uintptr_t baseAddr;
        const char* sectionName;
        uintptr_t sectionAddr;
        size_t sectionSize;
    };

    // Placeholder: Implementation differs wildly between Win32 (VirtualQuery/PE headers)
    // and Linux (dl_iterate_phdr/ELF headers)
    FORCE_INLINE bool getSectionInfo(const char* moduleName, const char* name, uintptr_t& baseAddr, size_t& size) {
        baseAddr = 0;
        size = 0;
        return false;
    }

    bool isExePage(uintptr_t addr) {
#ifdef _WIN32
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery((LPCVOID)addr, &mbi, sizeof(mbi))) {
            return (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE));
        }
#else
        // Linux: Parse /proc/self/maps
#endif
        return false;
    }

    const char* const kDotText = ".text";
}

namespace RBX {
    namespace Security {
        volatile const size_t rbxGoldHash = 0;
    }

    // Fixed alignment and naming for cross-compiler support
    ALIGN_8 const char* const maskAddr = nullptr;
    ALIGN_8 const char* const goldHash = nullptr;

    namespace CryptStrings {
        bool cmpNtQueryVirtualMemory(const char* inString) { return false; }
        bool cmpZwFilterToken(const char* inString) { return false; }
        bool cmpNtGetContextThread(const char* inString) { return false; }
        bool cmpZwLoadKey(const char* inString) { return false; }
    }
}

namespace RBX {
    using namespace Hasher;

    PmcHashContainer pmcHash;

    ScanRegion ScanRegion::getScanRegion(const char* moduleName, const char* regionName) {
        ScanRegion result;
        uintptr_t addr;
        size_t regionSize;
        if (getSectionInfo(moduleName, regionName, addr, regionSize)) {
            result.startingAddress = (char*)addr;
            result.size = static_cast<unsigned int>(regionSize);
        }
        return result;
    }

    PmcHashContainer::PmcHashContainer(const PmcHashContainer& init) : nonce(init.nonce), hash(init.hash) {}

    ProgramMemoryChecker::ProgramMemoryChecker()
        : hsceHashOrReduced(0),
          hsceHashAndReduced(0),
          bytesPerStep(0),
          currentRegion(0),
          currentMemory(nullptr),
          lastCompletedHash(0),
          lastGoldenHash(0),
          lastCompletedTime(Time::nowFast())
    {
        ScanRegionTest nonceLocation(ScanRegion(reinterpret_cast<char*>(&pmcHash.nonce), 4));
        nonceLocation.closeHash = true;
        nonceLocation.useHashValueInStructHash = false;
        scanningRegions.resize(kNumberOfSectionHashes);

        uintptr_t textBase = 0, rdataBase = 0, lowerBase = 0, upperBase = 0;
        size_t textSize = 0, rdataSize = 0, lowerSize = 0, upperSize = 0;

        // Platform-specific section naming
#ifdef _WIN32
        const char* rdataName = ".rdata";
        const char* msvcLib = "msvcr120.dll";
#else
        const char* rdataName = ".rodata";
        const char* msvcLib = "libc.so.6";
#endif

        scanningRegions[kGoldHashStart] = ScanRegionTest(ScanRegion(reinterpret_cast<char*>(lowerBase), lowerSize));
        scanningRegions[kGoldHashEnd] = ScanRegionTest(ScanRegion(reinterpret_cast<char*>(upperBase), upperSize));
        scanningRegions[kGoldHashRot] = nonceLocation;
        scanningRegions[kRdataHash] = ScanRegionTest(ScanRegion(reinterpret_cast<char*>(rdataBase), rdataSize));
        scanningRegions[kIatHash] = ScanRegionTest(ScanRegion(nullptr, 0));
        scanningRegions[kMsvcHash] = ScanRegionTest(ScanRegion::getScanRegion(msvcLib, kDotText));
        scanningRegions[kNonGoldHashRot] = nonceLocation;
    }

    bool ProgramMemoryChecker::areMemoryPagePermissionsSetupForHacking() { return false; }

    unsigned int ProgramMemoryChecker::step() { return 0; }

PUSH_OPTIMIZE_OFF
    int ProgramMemoryChecker::isLuaLockOk() const {
        return ProgramMemoryChecker::kLuaLockOk;
    }
POP_OPTIMIZE

    unsigned int ProgramMemoryChecker::getLastCompletedHash() const { return lastCompletedHash; }
    unsigned int ProgramMemoryChecker::getLastGoldenHash() const { return lastGoldenHash; }
    Time ProgramMemoryChecker::getLastCompletedTime() const { return lastCompletedTime; }
    void ProgramMemoryChecker::getLastHashes(PmcHashContainer::HashVector& outHashes) const {}
    unsigned int ProgramMemoryChecker::hashScanningRegions(size_t regions) const { return 0; }
    unsigned int ProgramMemoryChecker::updateHsceHash() { return 0; }
    unsigned int ProgramMemoryChecker::getHsceOrHash() const { return hsceHashOrReduced; }
    unsigned int ProgramMemoryChecker::getHsceAndHash() const { return hsceHashAndReduced; }

    unsigned int protectSections() {
#ifdef _WIN32
        // Use VirtualProtect
#else
        // Use mprotect
#endif
        return 0;
    }
}
