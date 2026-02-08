#pragma once

// #if defined(_WIN32) && !defined(RBX_STUDIO_BUILD) && !defined(RBX_PLATFORM_DURANGO)
// #include "security/RandomConstant.h"

// // This is junk code generation.  All of this is intended to be inlined
// // and generally do nothing.

// // there are 15*17 different seeds from this.
// // (RBX_BUILDSEED%15 + 1) is between 1 and 16, and is coprime to 17.
// #define RBX_JUNK (junk< ((RBX_BUILDSEED%15 + 1)*__LINE__ + RBX_BUILDSEED) % 17>())

// template <int N> inline void junk() {}

// #if defined(_MSC_VER)
// #define RBX_NOP0 __asm _emit 0x0f __asm _emit 0x1f __asm _emit 0x00
// #define RBX_NOP1 __asm _emit 0x8d __asm _emit 0x76 __asm _emit 0x00
// #define RBX_NOP2 __asm _emit 0x0f __asm _emit 0x1f __asm _emit 0x40 __asm _emit 0x00
// #define RBX_NOP3 __asm _emit 0x8d __asm _emit 0x74 __asm _emit 0x26 __asm _emit 0x00
// #define RBX_NOP4 __asm _emit 0x90 __asm _emit 0x8d __asm _emit 0x74 __asm _emit 0x26 __asm _emit 0x00
// #define RBX_NOP5 __asm _emit 0x66 __asm _emit 0x0f __asm _emit 0x1f __asm _emit 0x44 __asm _emit 0x00 __asm _emit 0x00
// #define RBX_NOP6 __asm _emit 0x8d __asm _emit 0xb6 __asm _emit 0x00 __asm _emit 0x00 __asm _emit 0x00 __asm _emit 0x00
// #else
// /* Portable GCC/Clang equivalent: emit the same raw bytes. */
// #define RBX_NOP0 __asm__ __volatile__(".byte 0x0f, 0x1f, 0x00")
// #define RBX_NOP1 __asm__ __volatile__(".byte 0x8d, 0x76, 0x00")
// #define RBX_NOP2 __asm__ __volatile__(".byte 0x0f, 0x1f, 0x40, 0x00")
// #define RBX_NOP3 __asm__ __volatile__(".byte 0x8d, 0x74, 0x26, 0x00")
// #define RBX_NOP4 __asm__ __volatile__(".byte 0x90, 0x8d, 0x74, 0x26, 0x00")
// #define RBX_NOP5 __asm__ __volatile__(".byte 0x66, 0x0f, 0x1f, 0x44, 0x00, 0x00")
// #define RBX_NOP6 __asm__ __volatile__(".byte 0x8d, 0xb6, 0x00, 0x00, 0x00, 0x00")
// #endif
// template<> inline void junk<0>()
// {
//     RBX_NOP0;
// }


// template<> inline void junk<1>()
// {
//     RBX_NOP1;
// }

// template<> inline void junk<2>()
// {
//     RBX_NOP2;
// }

// template<> inline void junk<3>()
// {
//     RBX_NOP3;
// }

// template<> inline void junk<4>()
// {
//     RBX_NOP4;
// }

// template<> inline void junk<5>()
// {
//     RBX_NOP5;
// }

// template<> inline void junk<6>()
// {
//     RBX_NOP6;
// }

// template<> inline void junk<7>()
// {
//     RBX_NOP0;
//     RBX_NOP1;
// }

// template<> inline void junk<8>()
// {
//     RBX_NOP1;
//     RBX_NOP2;
// }

// template<> inline void junk<9>()
// {
//     RBX_NOP1;
//     RBX_NOP3;
// }

// template<> inline void junk<10>()
// {
//     RBX_NOP4;
//     RBX_NOP0;
// }

// template<> inline void junk<11>()
// {
//     RBX_NOP3;
//     RBX_NOP5;
// }

// template<> inline void junk<12>()
// {
//     RBX_NOP2;
//     RBX_NOP5;
// }

// template<> inline void junk<13>()
// {
//     RBX_NOP6;
//     RBX_NOP0;
// }

// template<> inline void junk<14>()
// {
//     RBX_NOP2;
//     RBX_NOP1;
// }

// template<> inline void junk<15>()
// {
//     RBX_NOP4;
//     RBX_NOP5;
// }

// template<> inline void junk<16>()
// {
//     RBX_NOP3;
//     RBX_NOP2;
// }
// #else
#define RBX_JUNK
// #endif
