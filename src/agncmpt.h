/*
** $Id: agncmpt.h $
** Compatibility definitions
** See Copyright notices in this file, and - where not indicated - in agena.h
** initiated by Alexander Walz, before 2009
*/

#ifndef agncmpt_h
#define agncmpt_h

#include <locale.h>

#include "agnhlps.h"
#include "prepdefs.h"

/* In OpenSUSE 10.3, trunc, log2, exp2, fma, fdim do not exist although their definitions are included in tgmath.h */

#if (defined(__SOLARIS)) || defined(__linux__) && !defined(OPENSUSE) || defined(__APPLE__) || defined(__OS2__)  /* trunc, log2, exp2, fma, fdim */
#include "cephes.h"
#include <tgmath.h>
#endif

#ifndef typeof
#define typeof(x) __typeof__(x)
#endif
#ifndef typeof
#define typeof(x) __typeof(x)
#endif

/* DJGPP does not have fma, fdim, trunc, lstat, tgamma */
#if !(defined(_WIN32) || defined(__SOLARIS) || defined(__linux__) && !defined(OPENSUSE) || defined(__APPLE__) || defined(__OS2__))  /* 2.8.4 / 2.17.0 */

#ifndef lstat
#define lstat stat
#endif

#ifndef strtold
#define strtold strtod
#endif

#ifndef fma
#define fma tools_fma
#endif

/* Returns the positive difference between x and y. The positive difference is x - y if x is greater than y, and 0 otherwise. */
#ifndef fdim
#define fdim tools_fdim
#endif

#ifndef trunc
#define trunc(x) sun_trunc(x)  /* 2.29.2 */
#endif

#ifndef fmal
#define fmal tools_fmal
#endif

#ifndef log2l
#define log2l tools_log2l
#endif

#ifndef fmodl
#define fmodl sun_fmodl
#endif

#ifndef nextafterl
#define nextafterl sun_nextafterl
#endif

#ifndef truncl
#define truncl(x)  ( ((x) > 0.0L) ? sun_floorl((x)) : sun_ceill((x)) )
#endif

#ifndef tgamma
#define tgamma(x) gamma(x)
#endif

#ifndef remquo
#define remquo sun_remquo
#endif

#ifndef round
#define round sun_round
#endif

#endif  /* end of fma, fdim, trunc, lstat, etc. */


#ifdef __DJGPP__

#undef fabsl
#define fabsl tools_fabsl

#undef sinl
#define sinl sun_sinl

#undef cosl
#define cosl sun_cosl

#undef tanl
#define tanl sun_tanl

#undef expl
#define expl tools_expl

#undef logl
#define logl tools_logl

#undef sqrtl
#define sqrtl tools_sqrtl

#undef cbrtl
#define cbrtl tools_cbrtl

#undef asinl
#define asinl sun_asinl

#undef acosl
#define acosl sun_acosl

#undef atanl
#define atanl sun_atanl

#undef sinhl
#define sinhl tools_sinhl

#undef coshl
#define coshl tools_coshl

#undef tanhl
#define tanhl tools_tanhl

#undef fmal
#define fmal tools_fmal

#undef hypotl
#define hypotl tools_hypotl

#undef asinhl
#define asinhl sun_asinhl

#undef acoshl
#define acoshl sun_acoshl

#undef atanhl
#define atanhl sun_atanhl

#undef log2l
#define log2l tools_log2l

#undef log10l
#define log10l tools_log10l

#undef ilogbl
#define ilogbl tools_ilogbl

#undef atan2l
#define atan2l atan2

#undef powl
#define powl tools_powl

#undef expm1l
#define expm1l tools_expm1l

#undef log1pl
#define log1pl tools_log1pl

#undef erfl
#define erfl sun_erfl

#undef erfcl
#define erfcl sun_erfcl

#undef modfl
#define modfl sun_modfl

#undef ceill
#define ceill sun_ceill

#undef floorl
#define floorl sun_floorl

#undef truncl
#define truncl(x)  ( ((x) > 0.0L) ? floorl((x)) : ceill((x)) )

#undef fmodl
#define fmodl sun_fmodl

#undef fdiml
#define fdiml(x,y) (((x) > (y)) ? (x) - (y) : 0.0L)

#undef fminl
#define fminl fMin

#undef fmaxl
#define fmaxl fMax

#undef nanl
#define nanl nan

#undef nextafterl
#define nextafterl sun_nextafterl

#undef copysignl
#define copysignl sun_copysignl

#undef ldexpl
#define ldexpl sun_scalbnl

#undef frexpl
#define frexpl sun_frexpl

#undef scalbnl
#define scalbnl sun_scalbnl

#endif  /* of DJGPP */

#if defined(__DJGPP__) || defined(OPENSUSE)

/* Somehow DJGPP does not find fmaf although it exists ... we deliberately do not redefine fmaf to fma,
   to prevent any possible costly casts. Importing float.h does not help. */
#undef fmaf
#define fmaf(x,y,z) ((x)*(y)+(z))

#endif  /* of DJGPP & OPENSUSE */

#if 100*__GNUC__+__GNUC_MINOR__ >= 303
#ifndef NAN
#define NAN       __builtin_nanf("")
#endif
#ifndef INFINITY
#define INFINITY  __builtin_inff()
#endif
#else  /* not GCC >>= 303 */
#ifndef NAN
#define NAN       (0.0f/0.0f)
#endif
#ifndef INFINITY
#define INFINITY  1e5000f
#endif
#endif

/* From the GCC huge_val.h file: "Generally there is no separate `long double' format and it is the same as `double'." */
#ifndef HUGE_VALL
#define HUGE_VALL HUGE_VAL
#endif

#ifndef ULLONG_MAX
#define ULLONG_MAX 0xFFFFFFFFFFFFFFFF
#endif

#ifndef set_errno
#define set_errno(x) errno = (x)
#endif

/* Helps static branch prediction so hot path can be better optimized.  */
#ifdef __GNUC__
#define predict_true(x) __builtin_expect(!!(x), 1)
#define predict_false(x) __builtin_expect(x, 0)
#else
#define predict_true(x) (x)
#define predict_false(x) (x)
#endif

/* For OS/2 and OpenSUSE 10.3. For whatever reason, log2 does not exist in GCC OS/2, although defined in math.h with __USE_GNU compiler switch. */
#if !(defined(_WIN32) || defined(__SOLARIS) || defined(__linux__) && !defined(OPENSUSE) || defined(__DJGPP__) || defined(__APPLE__))  /* 2.8.4 fix */

#undef log2
#define log2(x)  ( ((x)) > 0 ? log((x)) * M_LOG2E : AGN_NAN )  /* sun_log creates round-off errors with 8 and 64 in OpenSUSE 10.3 ! Changed 2.12.0 RC 4 */

#undef exp2
#define exp2 cephes_exp2

#undef tgamma
#define tgamma gamma

#endif

/* faster than exp((x) * LN10//log(10)); DJGPP _has_ exp10 */
/* 2.8.4, we use the Cephes implementation for the antilog10 operator on all platforms: */
#undef exp10  /* 2.8.4 improvement */
#define exp10 cephes_exp10

#undef scalb  /* for any BSD-style code */
#define scalb ldexp

#if defined(__SOLARIS)  /* 2.8.4, 2.11.0 RC2 */
#ifndef isunordered
#define isunordered(x, y) __builtin_isunordered(x, y)
#endif
#elif !(defined(_WIN32) || defined(__linux__) && !defined(OPENSUSE) || defined(__APPLE__) || defined(__OS2__) )
#ifndef isunordered  /* prevent warnings in OpenSUSE */
#define isunordered(x, y)  (!(((x) != AGN_NAN) && ((y) != AGN_NAN)))
#endif
#endif

#if defined(__SOLARIS)  /* 2.8.4, 2.11.0 RC2  */
#ifndef isfinite
#define isfinite(x) __builtin_isfinite(x)
#endif
#elif !(defined(_WIN32) || defined(__linux__) && !defined(OPENSUSE) || defined(__APPLE__) || defined(__OS2__) || defined(__HAIKU__) )
#ifndef isfinite  /* prevent warnings in OpenSUSE */
#define isfinite tools_isfinite
#endif
#endif

#if defined(__SOLARIS)  /* 2.8.4 fix and extension, 2.11.0 RC2  */
#ifndef signbit
#define signbit(x) __builtin_signbit(x)
#endif
#elif !(defined(_WIN32) || defined(__linux__) && !defined(OPENSUSE) || defined(__APPLE__) || defined(__OS2__) )  /* DJGPP does not know signbit */
#undef signbit
#define signbit tools_signbit  /* FIXME: testing for -0.0 does not work in OpenSUSE 10.3/GCC 4.4.5 */
#endif

/* Solaris 10 does not have isinf */
#if defined(__SOLARIS)  /* 2.9.0 */
#ifndef isinf
#define isinf(x)  ((x) == -HUGE_VAL || (x) == HUGE_VAL)
#endif
#endif

/* needed to compile under Solaris; orig in MinGW: 0x8000 */
/* #if !defined(_WIN32) && !defined(__DJGPP__) && !defined(__OS2__) */
#ifndef O_BINARY
#define O_BINARY      0x0000
#endif

/* Assertively, MinGWs _open() wrapping to MS CreateFile does not support O_SYNC at all (see: https://sourceforge.net/p/mingw/bugs/525/) */
#ifndef O_SYNC
#define O_SYNC        0x0000   /* just to be neutral with ORring */
#endif

/* at least in GCC there are problems with fmax, see http://gcc.gnu.org/ml/gcc-bugs/2004-01/msg00620.html:
  `The symptom is that values of the mathematical functions fmaxf, fminf, fmax, fmin, fmaxl, fminl are
   all wrong. This makes the results of some numerical computations junks.` - 0.26.4, optimised 2.37.9 */
#define fMax(a,b) ({ typeof (a) _a = (a); typeof (b) _b = (b); _a > _b ? _a : _b; })
#define fMin(a,b) ({ typeof (a) _a = (a); typeof (b) _b = (b); _a < _b ? _a : _b; })

/* define constant `undefined` */
#ifdef NAN
#define AGN_NAN NAN
#elif defined(INFINITY)
#define AGN_NAN (INFINITY/INFINITY)
#else
#define AGN_NAN (tools_nan())
#endif

/* define complex unit I, which natively is not known to Solaris */
#ifdef __unix__
#undef _Complex_I
#define _Complex_I   (0.0F + 1.0iF)
#undef I
#define I   _Complex_I
#endif

/* The floating-point expression evaluation method, not known in MinGW/GCC 3.4.6
       -1  indeterminate
        0  evaluate all operations and constants just to the range and
           precision of the type
        1  evaluate operations and constants of type float and double
           to the range and precision of the double type, evaluate
           long double operations and constants to the range and
           precision of the long double type
        2  evaluate all operations and constants to the range and
           precision of the long double type;
   According to various newsgroup, its value is always 0. */
#ifndef FLT_EVAL_METHOD
#define FLT_EVAL_METHOD 0
#endif


#ifndef math_opt_barrier
#define math_opt_barrier(x) \
({ __typeof (x) __x = x; __asm ("" : "+m" (__x)); __x; })
#define math_force_eval(x) __asm __volatile ("" : : "m" (x))
#endif

#if defined(__DJGPP__)
#  define fopen64 fopen
#  define ftello64 ftell
#  define _ftelli64 ftello64
#  define fseeko64 fseek
#  define _fseeki64 fseeko64
#  define lseek64 lseek
#else
#if defined(__SOLARIS)  /* 2.11.0 RC3 */
#  define _fseeki64 fseeko64
#  define _ftelli64 ftello64
#else
#  define fopen64 fopen
#  define ftello64 ftell
#  define _ftelli64 ftello64
#  define fseeko64 fseek
#  define _fseeki64 fseeko64
#  define lseek64 lseek
#endif
#endif

/* TYPEDEF 2.14.4 */
#include <stdint.h>

#ifndef ULLONG_MAX
#  ifdef ULONG_LONG_MAX
#    define ULLONG_MAX ULONG_LONG_MAX
#  endif
#endif

#ifndef LLONG_MIN
#  ifdef LONG_LONG_MIN
#    define LLONG_MIN LONG_LONG_MIN
#  endif
#endif


/* still not defined ? */
#ifndef LLONG_MAX
#  define LLONG_MAX	9223372036854775807LL
#endif

/* still not defined ? */
#ifndef LLONG_MIN
#  define LLONG_MIN	(-LLONG_MAX - 1LL)
#endif

/* Maximum value an `unsigned long long int' can hold.  (Minimum is 0.)  */
#ifndef ULLONG_MAX
#   define ULLONG_MAX	18446744073709551615ULL
#endif

#ifndef LLONG_MAX
#  ifdef LONG_LONG_MAX
#    define LLONG_MAX LONG_LONG_MAX
#  endif
#endif

#ifndef __ARMCPU
#define TRUNC  trunc
#else
#define TRUNC  sun_trunc
#endif

/* Make good for missing drand48 in legacy 32-bit MinGW, advised by Gemini AI */
#ifdef _WIN32
#ifndef drand48
#define drand48() ((double)rand() / (RAND_MAX + 1.0))
#endif
#endif

#if !defined(umount2) && defined(umount)
  #define umount2((x), (y))  umount((x))
#endif

#ifndef FP_ZERO
#define FP_ZERO          0x4000
#endif
#ifndef FP_NORMAL
#define FP_NORMAL        0x0400
#endif
#ifndef FP_NAN
#define FP_NAN           0x0100
#endif
#ifndef FP_SUBNORMAL
#define FP_SUBNORMAL     (FP_NORMAL | FP_ZERO)
#endif
#ifndef FP_INFINITE
#define FP_INFINITE      (FP_NAN | FP_NORMAL)
#endif
#ifndef FP_ILOGBNAN
#define FP_ILOGBNAN      ((int)0x80000000)
#endif

/* Predictive branching, 2.15.5, idea and macros taken from Arm Optimized Routines */
#ifdef __GNUC__
# define likely(x)       __builtin_expect (!!(x), 1)
# define unlikely(x)     __builtin_expect ((x), 0)
#else
# define likely(x)       (x)
# define unlikely(x)     (x)
#endif

#if (defined(_WIN32) && !(defined(__MINGW32__) && (100*__GNUC__+__GNUC_MINOR__ >= 1000)))
#undef _countof
#define _countof(array) (sizeof(array) / sizeof(array[0]))
#endif

/* for code taken from Lua 5.4.4; 2.21.1/2.22.0/2.26.2 */
#define notm(tm)            ttisnil(tm)
#define isempty(v)          ttisnil(v)
#define isvalid(L, o)       (!ttisnil(o) || o != luaO_nilobject)
#define ttisinteger         ttisnumber  /* see also: ttisint in lobject.h */
#define ttisfloat           ttisnumber
#define setivalue           setnvalue
#define setfltvalue         setnvalue
#define fltvalue            nvalue
#define index2value         index2adr
#define s2v(o)              ((o))  /* ex: (&(o)->value) */
#define obj_at(L,k)	        (L->ci->base+(k) - 1)
#define ctb(o)              iscollectable(o)
#define luaT_callTMres      callTMres
#define luaV_equalobj       luaV_equalval
#define LUA_OPEQ            OP_EQ
#define LUA_OPLT            OP_LT
#define LUA_OPLE            OP_LE
#define LUA_OPADD           OP_ADD
#define LUA_OPSUB           OP_SUB
#define LUA_OPMUL           OP_MUL
#define LUA_OPDIV           OP_DIV
#define LUA_OPPOW           OP_POW
#define LUA_OPIDIV          OP_INTDIV
#define LUA_OPUNM           OP_UNM
#define LUA_OPMOD           OP_MOD
#define LUA_OPBNOT          OP_NOT
#define TM_NEWINDEX         TM_WRITEINDEX
#define TM_LEN              TM_SIZE
#define isabstkey(o)        ((o) == luaO_nilobject)
#define invalidateTMcache(t)   ((t)->flags = 0)
#define luaT_callTM         callTM
/* test for pseudo index */
#define ispseudo(i)         ((i) <= LUA_REGISTRYINDEX)
/* test for upvalue */
#define isupvalue(i)        ((i) < LUA_CONSTANTSINDEX)
#define lisspace(c)         (isspace(c))
#define lisxdigit(c)        (isxdigit(c))
#define cast_uchar(i)       cast(unsigned char, (i))
#define cast_uint(i)        cast(unsigned int, (i))
#define cast_voidp(i)       cast(void *, (i))
#define lua_getlocaledecpoint()  (localeconv()->decimal_point[0])
#define vslen(o)            (tsvalue(o)->len)
#define cvt2num(o)          (ttisstring(o))
#define yieldable(L)        (((L)->nCcalls & 0xffff0000) == 0)
#define rawtt(o)            ((o)->tt)
#define withvariant(t)      (t)  /* instead of ((t) & 0x3F) */
#define ttypetag(o)         withvariant(rawtt(o))
#define checktag(o,t)       (rawtt(o) == (t))
#define ttisfulluserdata(o) (ttisuserdata(o))
#define ltolower(c)         (tolower(c))
#define lislalpha(c)        (isalpha(c) || (c) == '_')
#define lislalnum(c)        (isalnum(c) || (c) == '_')
#define lisdigit(c)         (isdigit(c))
#define lisspace(c)         (isspace(c))
#define lisprint(c)         (isprint(c))
#define lisxdigit(c)        (isxdigit(c))
/* convert an object to a float (without string coercion) */
#define tonumberns(o,n)     (ttisfloat(o) ? ((n) = fltvalue(o), 1) : 0)
#define luaL_checkversion   {}
/* 2.22.0, taken from Lua 5.4.0: */
#define lua_rawlen(L,i)     lua_objlen(L, (i))

#if !defined(l_getc)    /* { */
#if defined(LUA_USE_POSIX)
#define l_getc(f)           getc_unlocked(f)
#define l_lockfile(f)       flockfile(f)
#define l_unlockfile(f)     funlockfile(f)
#else
#define l_getc(f)           getc(f)
#define l_lockfile(f)       ((void)0)
#define l_unlockfile(f)     ((void)0)
#endif
#endif              /* } */
/* end of code taken from Lua 5.4.0 RC 4; 2.21.1/2.22.0 */

#ifdef _WIN32
#define LOCALE_NAME_MAX_LENGTH 85  /* see: https://learn.microsoft.com/de-de/windows/win32/intl/locale-name-constants */
#define TCIFLUSH  (PURGE_RXABORT | PURGE_RXCLEAR)
#define TCOFLUSH  (PURGE_TXABORT | PURGE_TXCLEAR)
#define TCIOFLUSH  (TCIFLUSH | TCOFLUSH)
#define TIOCM_CAR  EV_RLSD
#define TIOCM_CTS  EV_CTS
#define TIOCM_DSR  EV_DSR
#define TIOCM_RNG  EV_RING

#ifndef O_NOCTTY
#define O_NOCTTY   0
#endif
#ifndef O_SYNC
#define O_SYNC     0
#endif

#else
#include <termios.h>

/* At least in OpenSUSE, the following names are defined preventing compilation of rlhmath.c: */
#undef A0
#undef B0

/* modem lines */
#ifndef TIOCMGET
#define TIOCMGET  0x5415
#endif
#ifndef TIOCMBIS
#define TIOCMBIS  0x5416
#endif
#ifndef TIOCMBIC
#define TIOCMBIC  0x5417
#endif
#ifndef TIOCMSET
#define TIOCMSET  0x5418
#endif
#ifndef TIOCM_LE
#define TIOCM_LE  0x001
#endif
#ifndef TIOCM_DTR
#define TIOCM_DTR  0x002
#endif
#ifndef TIOCM_RTS
#define TIOCM_RTS  0x004
#endif
#ifndef TIOCM_ST
#define TIOCM_ST  0x008
#endif
#ifndef TIOCM_SR
#define TIOCM_SR  0x010
#endif
#ifndef TIOCM_CTS
#define TIOCM_CTS  0x020
#endif
#ifndef TIOCM_CAR
#define TIOCM_CAR  0x040
#endif
#ifndef TIOCM_RNG
#define TIOCM_RNG  0x080
#endif
#ifndef TIOCM_DSR
#define TIOCM_DSR  0x100
#endif
#ifndef TIOCM_CD
#define TIOCM_CD  TIOCM_CAR
#endif
#ifndef TIOCM_RI
#define TIOCM_RI  TIOCM_RNG
#endif
#ifndef TIOCM_OUT1
#define TIOCM_OUT1  0x2000
#endif
#ifndef TIOCM_OUT2
#define TIOCM_OUT2  0x4000
#endif
#ifndef TIOCM_LOOP
#define TIOCM_LOOP  0x8000
#endif

#ifndef B57600
#define  B57600   0010001
#endif
#ifndef B115200
#define  B115200  0010002
#endif
#ifndef B230400
#define  B230400  0010003
#endif
#ifndef B460800
#define  B460800  0010004
#endif
#ifndef B500000
#define  B500000  0010005
#endif
#ifndef B576000
#define  B576000  0010006
#endif
#ifndef B921600
#define  B921600  0010007
#endif
#ifndef B1000000
#define  B1000000 0010010
#endif
#ifndef B1152000
#define  B1152000 0010011
#endif
#ifndef B1500000
#define  B1500000 0010012
#endif
#ifndef B2000000
#define  B2000000 0010013
#endif
#ifndef B2500000
#define  B2500000 0010014
#endif
#ifndef B3000000
#define  B3000000 0010015
#endif
#ifndef B3500000
#define  B3500000 0010016
#endif
#ifndef B4000000
#define  B4000000 0010017
#endif
#ifndef __MAX_BAUD
#define __MAX_BAUD B4000000
#endif

#ifndef CRTSCTS
# define CRTSCTS  020000000000    /* flow control */
#endif

#ifndef IXANY
#define IXANY  0004000
#endif

#endif  /* of `TTY` */

/* Generated by Gemini AI, put to the public domain, November 04, 2025, 6.3.7;
   does not work with most GCC compilers when imported into other header files, though. */
#ifndef RESTRICT
  #if defined(__cplusplus)
    #if defined(__GNUC__) || defined(__clang__)
      #define RESTRICT __restrict__
    #elif defined(_MSC_VER)
      #define RESTRICT __restrict
    #else
      #define RESTRICT
    #endif
  #elif __STDC_VERSION__ >= 199901L
    #define RESTRICT restrict
  #else
    #define RESTRICT
  #endif
#endif

#endif  /* of agncmpt_h */

