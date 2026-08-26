/* common preprocessor defines, 2.14.4 */

/* For an example, see https://www.boost.org/doc/libs/1_60_0/boost/config/compiler/gcc.hpp */

#ifndef prepdefs_h
#define prepdefs_h

#include <limits.h>
#include <stddef.h>  /* for size_t */

#if (defined (__SVR4) && defined (__sun))
#define __SOLARIS
#endif

#if defined(__x86_64__) || defined(_M_X64) || defined(i386) || defined(__i386__) || defined(__X86__) || defined(__i386) || defined(_M_IX86)
#define __INTEL
#endif

#if defined(i386) || defined(__i386__) || defined(__X86__) || defined(__i386) || defined(_M_IX86)
#define __INTEL32
#endif

#if defined(__x86_64__) || defined(_M_X64)
#define __INTEL64
#endif

#if defined(__ARM_ARCH_2__) || defined(__ARM_ARCH_3__)  || defined(__ARM_ARCH_3M__)  || defined(__ARM_ARCH_4T__) || defined(__TARGET_ARM_4T) \
|| defined(__ARM_ARCH_5_)   || defined(__ARM_ARCH_5E_)  || defined(__ARM_ARCH_6__)   || defined(__ARM_ARCH_6J__) || defined(__ARM_ARCH_6K__) \
|| defined(__ARM_ARCH_6T2_) || defined(__ARM_ARCH_6Z__) || defined(__ARM_ARCH_6ZK__) || defined(__ARM_ARCH_7__)  || defined(__ARM_ARCH_7A__) \
|| defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7S__)  || defined(__aarch64__)     || defined(_M_ARM64) \
|| defined(__ARM)
#define __ARMCPU
#endif

#if defined(__powerpc) || defined(__powerpc__) || defined(__powerpc64__) || defined(__POWERPC__) || defined(__ppc__) || defined(__PPC__) || defined(_ARCH_PPC)
#define __PPCCPU
#endif

#if defined(__INTEL) && defined(__GNUC__)
#define CPUINFO
#endif

/* Just for cpuid.h header inclusion: */
#if (defined(__GNUC__) && !(defined(OPENSUSE) || defined(LUA_RASPI_STRETCH) || defined(__OS2__) || defined(__APPLE__) || defined(__powerpc__)))
#define CPUID
#endif

/* 32 or 64 bit ?  2.25.5 */
#if LONG_MAX == 2147483647L
#define IS32BIT
#elif LONG_MAX == 9223372036854775807L
#define IS64BIT
#else
#error long int is not a 32bit or 64bit type
#endif

/* There are functions defined in agnhlps.c that perform vectorized reading which is very fast but unsafe
   when meeting memory boundaries, potentially triggering segmentation faults or causing the logic to fail.
   They are tools_streq, tools_strlen, tools_strcmp, etc. Thus, they are been deactivated here. 7.3.2 */
#if (defined(__GNUC__) && defined(IS32BIT))
/* #define IS32BITALIGNED */
#endif

#if defined(__ARMCPU) && defined(IS32BIT)
#define __ARMCPU32
#endif

#if defined(__ARMCPU) && defined(IS64BIT)
#define __ARMCPU64
#endif

/* inline ... */

#if defined __GNUC__ && !defined __GNUC_STDC_INLINE__ && !defined __GNUC_GNU_INLINE__
#define __GNUC_GNU_INLINE__ 1
#endif

#ifndef NOINLINE
#define INLINE inline
#else
#define INLINE
#endif

#if defined(__GNUC__) || defined(__clang__)
#define FORCE_INLINE __attribute__((always_inline)) inline
#elif defined(_MSC_VER)
#define FORCE_INLINE __forceinline
#else
#define FORCE_INLINE inline
#endif


#if SIZE_MAX == 0xFFFFFFFFUL
/* this is a 32-bit system (size_t is 4 bytes) */
#define SIZE_T_BITS 32
#elif SIZE_MAX == 0xFFFFFFFFFFFFFFFFULL
/* this is a 64-bit system (size_t is 8 bytes) */
#define SIZE_T_BITS 64
#else
/* handle other/unknown sizes */
#define SIZE_T_BITS 0
#endif

#endif
