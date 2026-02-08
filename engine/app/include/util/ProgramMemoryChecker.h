#pragma once

#include <boost/scoped_ptr.hpp>
#include "rbx/rbxTime.h"
#include <vector>
#include "security/RandomConstant.h"
#include "v8datamodel/HackDefines.h"
#include "util/HeapValue.h"
#include "security/ApiSecurity.h"

namespace RBX {

namespace Hasher
{
    // The PMC constructor assumes a specific ordering of these items.
    enum HashSection
    {
        kGoldHashStart = 0,
        kGoldHashEnd = 1,
        kGoldHashRot = 2,
        kRdataHash = 3,
        kVmpPlainHash = 4,
        kVmpMutantHash = 5,
        kIatHash = 6,
        kMiscHash = 7,
        kMsvcHash = 8,
        kVmp0MiscHash = 9,
        kVmp1MiscHash = 10,
        kNonGoldHashRot = 11,
        kNumberOfSectionHashes = 12,
        kGoldHashStruct = 12,
        kAllHashStruct = 13,
        kNumberOfHashes = 14
    };

    // These do not have a 1:1 relation with the indicies above.
    enum HashFailures
    {
        // 0x?000
        kVmp1MiscHashFail = 1<<15,
        kVmp0MiscHashFail = 1<<14,
        kVmpMutantHashFail = 1<<13,
        kIatHashFail = 1<<12,
        // 0x0?00
        kGoldHashFail = 1<<11,
        kNonceFail = 1<<10,
        kAllHashStructFail = 1<<9,
        kGoldHashStructFail = 1<<8,
        // 0x00?0
        kNonGoldHashRotFail = 1<<7,
        kVmpPlainHashFail = 1<<6,
        kMsvcHashFail = 1<<5,
        kRdataHashFail = 1<<4,
        // 0x000?
        kMiscHashFail = 1<<3,
        kGoldHashRotFail = 1<<2,
        kGoldHashEndFail = 1<<1,
        kGoldHashStartFail = 1<<0
    };

    static const unsigned int kGoldHashMask = kGoldHashFail;

    static const unsigned int kDiffHashMask = kGoldHashStartFail | kGoldHashEndFail
        | kMiscHashFail | kRdataHashFail | kMsvcHashFail | kGoldHashStructFail
        | kAllHashStructFail | kVmpPlainHashFail | kVmpMutantHashFail | kVmp0MiscHashFail
        | kVmp1MiscHashFail | kIatHashFail;

    static const unsigned int kMovingHashMask = kNonceFail | kGoldHashRotFail
        | kNonGoldHashRotFail;

    static const int zeroPad[4] = {0,0,0,0};

    static const unsigned int kPmcNonceGoodInc = 3692164867;
    static const unsigned int kPmcNonceBadInc =  3692164869;
    static const unsigned int kPmcNonceGoodIncInv = 2880154539; //0xABABABAB supplied by irc user.  unimportant fact.

}

struct ScanRegion
{
    char* startingAddress;
    unsigned int size;

    // ".text" and ".rdata" appear in several places in RAM, so it is safe to have them as literals.
    // likewise, exploiters would quickly realize that .text and .rdata are scanned.
    // if they change the value to something else, it would crash.
    static ScanRegion getScanRegion(const char* moduleName, const char* RegionName);

    ScanRegion() : startingAddress(NULL), size(0){}
    ScanRegion(const ScanRegion &initValue) : startingAddress(initValue.startingAddress), size(initValue.size){}
    ScanRegion(char* startingAddress, unsigned int size) : startingAddress(startingAddress), size(size) {}
};

struct ScanRegionTest : ScanRegion
{
    void* hashState;
    unsigned int lastHashValue;
    bool closeHash;
    bool useHashValueInStructHash;
    bool useHashAddrSizeInStructHash;

    ScanRegionTest() : ScanRegion(),
        hashState(NULL),
        lastHashValue(0),
        closeHash(false),
        useHashValueInStructHash(true),
        useHashAddrSizeInStructHash(true) {}
    ScanRegionTest(ScanRegion initValue) : ScanRegion(initValue),
        hashState(NULL),
        lastHashValue(0),
        closeHash(false),
        useHashValueInStructHash(true),
        useHashAddrSizeInStructHash(true) {}
};

struct PmcHashContainer
{
    typedef std::vector<unsigned int> HashVector;
    unsigned int nonce;
    HashVector hash;
    PmcHashContainer(const PmcHashContainer& init);
    PmcHashContainer() : nonce(0) {}
};

extern PmcHashContainer pmcHash;

class ProgramMemoryChecker
{
protected:
    unsigned int hsceHashOrReduced;
    unsigned int hsceHashAndReduced;
public:
    static const int kHASH_SEED_INIT = 42;
    static const int kAllDone = 0xCCCCCCCC; // A number that is non-zero.
    static const int kLuaLockOk = 0x1842783;
    static const int kLuaLockBad = 0;
    static const int kSteps = 30;
    static const unsigned int kBlock = 16;
    ProgramMemoryChecker();
    unsigned int bytesPerStep;
    unsigned int currentRegion;
    const char* currentMemory;
    std::vector<ScanRegionTest> scanningRegions;
    unsigned int lastCompletedHash;
    unsigned int lastGoldenHash;
    Time lastCompletedTime;

    unsigned int step();
    unsigned int getLastCompletedHash() const;
    unsigned int getLastGoldenHash() const;
    Time getLastCompletedTime() const;

    void getLastHashes(PmcHashContainer::HashVector& outHashes) const;
    // This is a hash of the hashes, as well as a hash of the region information.
    unsigned int hashScanningRegions(size_t regions = Hasher::kNumberOfHashes-2) const;

    // return hash of HumanoidState::computeEvent, update hsceHashOrReduce.
    unsigned int updateHsceHash();
    unsigned int getHsceOrHash() const;
    unsigned int getHsceAndHash() const;

    // Should look at return code.
    int isLuaLockOk() const;

    // Check for stealthedit. Stealthedit sets some pages to non-executable
    // and then catches the resulting exception, using the opportunity
    // to redirect to a modified page without disturbing the hash mechanism.
    // http://www.szemelyesintegracio.hu/cheats/41-game-hacking-articles/419-stealthedit
    static bool areMemoryPagePermissionsSetupForHacking();

};

#ifdef _WIN32
__declspec(align(8)) extern const char* const maskAddr;
__declspec(align(8)) extern const char* const goldHash;
unsigned int protectVmpSections();
#else
__attribute__((__aligned__(8))) extern const char* const maskAddr;
__attribute__((__aligned__(8))) extern const char* const goldHash;
#endif


namespace Security{
    // The storage for hash checker related security constants.
    extern volatile const size_t rbxGoldHash;

    // The lower part of .text
    extern volatile const uintptr_t rbxLowerBase;
    extern volatile const size_t rbxLowerSize;

    // The upper part of .text
    extern volatile const uintptr_t rbxUpperBase;
    extern volatile const size_t rbxUpperSize;

    // the .rdata section
    extern volatile const uintptr_t rbxRdataBase;
    extern volatile const size_t rbxRdataSize;

    // the vmp sections
    extern volatile const uintptr_t rbxVmpBase;
    extern volatile const size_t rbxVmpSize;

    // the Import Address (thunk) Table
    extern volatile const uintptr_t rbxIatBase;
    extern volatile const size_t rbxIatSize;

    // the vmp sections (plain .text section)
    extern volatile const uintptr_t rbxVmpPlainBase;
    extern volatile const size_t rbxVmpPlainSize;

    // the vmp sections (mutation .text section)
    extern volatile const uintptr_t rbxVmpMutantBase;
    extern volatile const size_t rbxVmpMutantSize;

    // the vmp sections (don't know)
    extern volatile const uintptr_t rbxVmp0MiscBase;
    extern volatile const size_t rbxVmp0MiscSize;

    // the vmp sections (don't know)
    extern volatile const uintptr_t rbxVmp1MiscBase;
    extern volatile const size_t rbxVmp1MiscSize;

    // the .rdata section without IAT
    extern volatile const uintptr_t rbxRdataNoIatBase;
    extern volatile const size_t rbxRdataNoIatSize;
}

}
