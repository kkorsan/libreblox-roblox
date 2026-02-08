#pragma once
#include <stdint.h>
#include "security/FuzzyTokens.h"
#include "security/RandomConstant.h"

#if defined(RBX_PLATFORM_DURANGO)
#define NOINLINE __declspec(noinline)
#elif defined(_WIN32)
#include <windows.h>
#include <winternl.h>
#undef min

#define FORCEINLINE __forceinline
#define NOINLINE __declspec(noinline)

// All of the .text
namespace RBX{ namespace Security {
    extern volatile const uintptr_t rbxTextBase;
    extern volatile const size_t rbxTextSize;
    extern volatile const uintptr_t rbxTextEndNeg;
    extern volatile const size_t rbxTextSizeNeg;
    extern volatile const uintptr_t rbxVmpBase;
    extern volatile const size_t rbxVmpSize;
}
}


#else
#define FORCEINLINE inline __attribute__((always_inline))
#define NOINLINE __attribute__((noinline))
#endif


// This file defines some secure caller function to prevent key functions
// from being called from a dll.
//
// For many functions, this is actually intractable.  With just a return address
// check, someone can simply push the location of a "C3" byte (ret) in our code
// and then jump to our function.  The return address will appear to be in our code.
//
// When the check becomes stronger, it is a simple matter to find the target
// function called from another function in our code.  From this point it is easy
// to call (into) that function.  Even if the result isn't directly returned, it
// will typically be on the stack.
//
// callbacks are problematic.  On one hand, we can check callbacks before setting
// and before calling them.  However, callbacks could be set to "CC" bytes (int 3) in
// our code, which would throw an exception that could be handled.
//
// Secure calling in this file almost certainly will not work within a VM protected
// section as the registers and stack will be nonstandard.
//
// It will be trivial to find where these checks are done as well, as they all
// modify the same global values.

namespace RBX
{
FORCEINLINE static bool isRbxTextAddr(const void* const ptr)
{
    return true;
}

template<unsigned int value> FORCEINLINE void callCheckSetBasicFlag(unsigned int flags)
{
    Tokens::sendStatsToken.addFlagFast(value);
    Tokens::simpleToken |= value;
}

// 33222222222 2   1  111  11111100  000 0     0       000
// 10987654321 0   9  876  54321098  765 4     3       210
// rsvd        cpy pi veh  ntapi         cs    locked  name
static const unsigned int kNameApiOffset = 0;
static const unsigned int kRbxLockedApiOffset = 3;
static const unsigned int kChangeStateApiOffset = 4;
static const unsigned int kNtApiNoNtdll = (1<<8);
static const unsigned int kNtApiNoText = (1<<9);
static const unsigned int kNtApiNoApi = (1<<10);
static const unsigned int kNtApiNoSyscall = (1<<11);
static const unsigned int kNtApiNoTemplate = (1<<12);
static const unsigned int kNtApiEarly = (1<<13);
static const unsigned int kNtApiHash = (1<<14);
static const unsigned int kNtApiNoCall = (1<<15);
static const unsigned int kVehWpmFail = (1<<16);
static const unsigned int kVehPrologFail = (1<<17);
static const unsigned int kVehNoNtdll = (1<<18);
static const unsigned int kPingItem = (1<<19);
static const unsigned int kScriptContextCopy = (1<<20);
static const unsigned int kLuaHooked = (1<<21);


template<unsigned int offset> FORCEINLINE void callCheckSetApiFlag(unsigned int flags)
{
    Tokens::apiToken.addFlagSafe(flags << offset);
}

FORCEINLINE void callCheckNop(unsigned int flags)
{
}

// This checks that the return address:
//  1) is within the .text section
//  2) calling instruction has a correct relative offset. (must be call imm32)
//  3) caller of calling function is within our .text section
// Disabled in NoOpt because this won't be inlined.
static const int kCallCheckCodeOnly = 1;
static const int kCallCheckCallArg = 2;
static const int kCallCheckCallersCode = 3;
static const int kCallCheckRegCall = 4;

template<int level, void(*action)(unsigned int)>
FORCEINLINE static unsigned int checkRbxCaller(const void* const funcAddress)
{
    return 0;
}

namespace Security{
    static const unsigned int kCheckDefault = 0;
    static const unsigned int kCheckReturnAddr = 1;
    static const unsigned int kCheckNoThreadInit = 2;
    static const unsigned int kAllowVmpAll = 4; // I'm not sure if I only need to look at mutant, mutant+plain, etc...
}


struct CallChainInfo
{
    uint32_t handler;
    uint32_t ret;
    CallChainInfo() : handler(0), ret(0) {}
    CallChainInfo(uint32_t handler, uint32_t ret) : handler(handler), ret(ret) {}
};

template<size_t kMaxDepth> FORCEINLINE uint32_t detectDllByExceptionChainTeb(unsigned int kFlags)
{
    return 0;
}
template<size_t kMaxDepth> FORCEINLINE uint32_t detectDllByExceptionChainStack(void* addrOfFirstArg, unsigned int kFlags)
{
    return 0;
}

template<size_t kMaxDepth> FORCEINLINE void generateCallInfo(void* addrOfFirstArg, std::vector<CallChainInfo>& info)
{
    return;
}


}
