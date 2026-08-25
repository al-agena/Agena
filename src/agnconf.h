/*
** $Id: agnconf.h, 2009/03/01 15:40:26 $
** Configuration file for Agena
** See Copyright Notice in agena.h
*/

#ifndef agnconf_h
#define agnconf_h

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include "prepdefs.h"

/* Leave this complex include here, otherwise AgenaEdit will not compile */

#ifdef __cplusplus
#if defined(__linux__) || defined(__SOLARIS)
#include <complex.h>
#else
#include <complex>
using std::complex;
#endif
#endif

#ifdef __cplusplus
extern "C"
{
#endif

/*
** ==================================================================
** Search for "@@" to find all configurable definitions.
** ===================================================================
*/


/*
@@ LUA_ANSI controls the use of non-ansi features.
** CHANGE it (define it) if you want Lua to avoid the use of any
** non-ansi feature or library.
*/
#if defined(__STRICT_ANSI__)
#undef LUA_ANSI
#define LUA_ANSI
#endif


#if !defined(LUA_ANSI) && defined(_WIN32)
#define LUA_WIN
#endif

#if defined(LUA_USE_LINUX)
#define LUA_USE_POSIX
#define LUA_USE_DLOPEN      /* needs an extra library: -ldl */
#define LUA_USE_READLINE    /* needs some extra libraries */
#endif

#if defined(LUA_USE_LINUXPLAIN)
#define LUA_USE_POSIX
#define LUA_USE_DLOPEN      /* needs an extra library: -ldl */
#endif

#if defined(LUA_USE_MACOSX)
#define LUA_USE_POSIX
#define LUA_DL_DYLD      /* does not need extra library */
#endif

/*
@@ LUA_USE_POSIX includes all functionality listed as X/Open System
@* Interfaces Extension (XSI).
** CHANGE it (define it) if your system is XSI compatible.
*/
#if defined(LUA_USE_POSIX)
#define LUA_USE_MKSTEMP
#define LUA_USE_ISATTY
#define LUA_USE_POPEN
#ifndef __HAIKU__
#define LUA_USE_ULONGJMP
#endif
#endif

/*
@@ LUA_PATH and LUA_CPATH are the names of the environment variables that
@* Lua check to set its paths.
@* checks for initialisation code.
** CHANGE them if you want different names.
*/
#define LUA_PATH        "LUA_PATH"
#define LUA_CPATH       "LUA_CPATH"


/*
@@ LUA_PATH_DEFAULT is the default path that Lua uses to look for
@* Lua libraries.
@@ LUA_CPATH_DEFAULT is the default path that Lua uses to look for
@* C libraries.
** CHANGE them if your machine has a non-conventional directory
** hierarchy or if you want to install your libraries in
** non-conventional directories.
*/
#if defined(_WIN32)
#define AGN_CLIB  ".dll"
#else
#define AGN_CLIB  ".so"
#endif


/*
@@ LUA_DIRSEP is the directory separator (for submodules).
** CHANGE it if your machine does not use "/" as the directory separator
** and is not Windows. (On Windows Lua automatically uses "\".)
*/
#if defined(_WIN32)
#define LUA_DIRSEP   "\\"
#else
#define LUA_DIRSEP   "/"
#endif


/*
@@ LUA_PATHSEP is the character that separates templates in a path.
@@ LUA_PATH_MARK is the string that marks the substitution points in a
@* template.
@@ LUA_EXECDIR in a Windows path is replaced by the executable's
@* directory.
@@ LUA_IGMARK is a mark to ignore all before it when building the
@* luaopen_ function name.
** CHANGE them if for some reason your system cannot use those
** characters. (E.g., if one of those characters is a common character
** in file/directory names.) Probably you do not need to change them.
*/
#define LUA_PATHSEP   ";"
#define LUA_IGMARK   "-"


/*
@@ LUA_INTEGER is the integral type used by lua_pushinteger/lua_tointeger.
** CHANGE that if ptrdiff_t is not adequate on your machine. (On most
** machines, ptrdiff_t gives a good choice between int or long.)
**
** NOTE: On 32-bit and 64-bit platforms, strings.pack/unpack only work
** with signed 32-bit integers.
*/
#if defined(IS64BIT)  /* 4.4.3a/4.4.5 fixes for x86 Debian 64-bit and Raspi ARM64 */
#define LUA_INTEGER   int32_t
#else
#define LUA_INTEGER   ptrdiff_t  /*  */
#endif


/*
@@ LUA_API is a mark for all core API functions.
@@ LUALIB_API is a mark for all standard library functions.
** CHANGE them if you need to define those functions in some special way.
** For instance, if you want to create one Windows DLL with the core and
** the libraries, you may want to use the following definition (define
** LUA_BUILD_AS_DLL to get it).
*/
#if defined(LUA_BUILD_AS_DLL)

#if defined(LUA_CORE) || defined(LUA_LIB)
#define LUA_API __declspec(dllexport)
#else
#define LUA_API __declspec(dllimport)
#endif

#else

#define LUA_API      extern

#endif

/* more often than not the libs go together with the core */
#define LUALIB_API   LUA_API


/*
@@ LUAI_FUNC is a mark for all extern functions that are not to be
@* exported to outside modules.
@@ LUAI_DATA is a mark for all extern (const) variables that are not to
@* be exported to outside modules.
** CHANGE them if you need to mark them in some special way. Elf/gcc
** (versions 3.2 and later) mark them as "hidden" to optimize access
** when Lua is compiled as a shared library.
*/
#if defined(luaall_c)
#define LUAI_FUNC   static
#define LUAI_DATA   /* empty */

/* You may want to avoid useless compiler warnings in Solaris by excluding __SOLARIS
   here, see: http://lua-users.org/lists/lua-l/2008-07/msg00165.html. With GCC 6.3.0
   everything works fine under Solaris. */
#elif defined(__GNUC__) && ((__GNUC__*100 + __GNUC_MINOR__) >= 302) && \
      defined(__ELF__)
#define LUAI_FUNC   __attribute__((visibility("hidden"))) extern
#define LUAI_DATA   LUAI_FUNC
#else
#define LUAI_FUNC   extern
#define LUAI_DATA   extern
#endif



/*
@@ LUA_QL describes how error messages quote program elements.
** CHANGE it if you want a different appearance.
*/
#define LUA_QL(x)   "`" x "`"
#define LUA_QS      LUA_QL("%s")


/*
@@ LUA_IDSIZE gives the maximum size for the description of the source
@* of a function in debug information.
** CHANGE it if you want a different size.
*/
#define LUA_IDSIZE   60


/*
** {==================================================================
** Stand-alone configuration
** ===================================================================
*/

#if defined(lua_c) || defined(luaall_c)

/*
@@ lua_stdin_is_tty detects whether the standard input is a 'tty' (that
@* is, whether we're running lua interactively).
** CHANGE it if you have a better definition for non-POSIX/non-Windows
** systems.
*/
#if defined(LUA_USE_ISATTY)
#include <unistd.h>
#define lua_stdin_is_tty()   isatty(0)
#elif defined(LUA_WIN)
#include <io.h>
#include <stdio.h>
#define lua_stdin_is_tty()   _isatty(_fileno(stdin))
#else
#define lua_stdin_is_tty()   1  /* assume stdin is a tty */
#endif


/*
@@ LUA_PROMPT is the default prompt used by stand-alone Lua.
@@ LUA_PROMPT2 is the default continuation prompt used by stand-alone Lua.
** CHANGE them if you want different prompts. (You can also change the
** prompts dynamically, assigning to globals _PROMPT/_PROMPT2.)
*/
#define LUA_PROMPT      "> "
#define LUA_PROMPT2      "> "

/*
@@ LUA_PROGNAME is the default name for the stand-alone Lua program.
** CHANGE it if your stand-alone interpreter has a different name and
** your system is not able to detect that name automatically.
*/
#define LUA_PROGNAME      "agena"


/*
@@ LUA_MAXINPUT is the maximum length for an input line in the
@* stand-alone interpreter.
** CHANGE it if you need longer lines.
*/
#define LUA_MAXINPUT   2048  /* Agena 1.6.0 */


/*
@@ lua_readline defines how to show a prompt and then read a line from
@* the standard input.
@@ lua_saveline defines how to "save" a read line in a "history".
@@ lua_freeline defines how to free a line read by lua_readline.
** CHANGE them if you want to improve this functionality (e.g., by using
** GNU readline and history facilities).
*/
#if defined(LUA_USE_READLINE)  /* with DJGPP, use readline-6.2 instead of 8.0 in order to compile, see:
  ftp://ftp.fu-berlin.de/pc/languages/djgpp/deleted/v2gnu/ */
#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>
#define lua_readline(L,b,p)   ((void)L, ((b)=readline(p)) != NULL)
#define lua_saveline(L,idx) \
   if (lua_strlen(L,idx) > 0)  /* non-empty line? */ \
     add_history(lua_tostring(L, idx));  /* add it to history */
#define lua_freeline(L,b)   ((void)L, free(b))
#else
#define lua_readline(L,b,p) \
   ((void)L, fputs(p, stdout), fflush(stdout),  /* show prompt */ \
   fgets(b, LUA_MAXINPUT, stdin) != NULL)  /* get line */
#define lua_saveline(L,idx)   { (void)L; (void)idx; }
#define lua_freeline(L,b)   { (void)L; (void)b; }
#endif

#endif

/* }================================================================== */


/*
@@ LUAI_GCPAUSE defines the default pause between garbage-collector cycles
@* as a percentage.
** CHANGE it if you want the GC to run faster or slower (higher values
** mean larger pauses which mean slower collection.) You can also change
** this value dynamically.
*/
#define LUAI_GCPAUSE   200  /* 200% (wait memory to double before next GC) */


/*
@@ LUAI_GCMUL defines the default speed of garbage collection relative to
@* memory allocation as a percentage.
** CHANGE it if you want to change the granularity of the garbage
** collection. (Higher values mean coarser collections. 0 represents
** infinity, where each step performs a full collection.) You can also
** change this value dynamically.
*/
#define LUAI_GCMUL   200 /* GC runs 'twice the speed' of memory allocation */

/*
@@ LUA_COMPAT_LOADLIB controls compatibility about global loadlib.
** CHANGE it to undefined as soon as you do not need a global 'loadlib'
** function (the function is still available as 'package.loadlib').
*/
#undef LUA_COMPAT_LOADLIB

/*
@@ LUA_COMPAT_VARARG controls compatibility with old vararg feature.
** CHANGE it to undefined as soon as your programs use only '...' to
** access vararg parameters (instead of the old 'arg' table).

#define LUA_COMPAT_VARARG
*/

/*
@@ LUA_COMPAT_OPENLIB controls compatibility with old 'luaL_openlib'
@* behavior.
** CHANGE it to undefined as soon as you replace to 'luaL_register'
** your uses of 'luaL_openlib'
*/
#define LUA_COMPAT_OPENLIB


/*
@@ luai_apicheck is the assert macro used by the Lua-C API.
** CHANGE luai_apicheck if you want Lua to perform some checks in the
** parameters it gets from API calls. This may slow down the interpreter
** a bit, but may be quite useful when debugging C code that interfaces
** with Lua. A useful redefinition is to use assert.h.
*/
#if defined(LUA_USE_APICHECK)
#include <assert.h>
#define luai_apicheck(L,o)   { (void)L; assert(o); }
#else
#define luai_apicheck(L,o)   { (void)L; }
#endif


/*
@@ LUAI_BITSINT defines the number of bits in an int.
** CHANGE here if Lua cannot automatically detect the number of bits of
** your machine. Probably you do not need to change this.
*/
/* avoid overflows in comparison */
#if INT_MAX - 20 < 32760
#define LUAI_BITSINT   16
#elif INT_MAX > 2147483640L
/* int has at least 32 bits */
#define LUAI_BITSINT   32
#else
#error "you must define LUA_BITSINT with number of bits in an integer"
#endif

/* initial seeds for math.random */
#define AGN_RANDOM_MW  10
#define AGN_RANDOM_MZ   7

/*
@@ LUAI_UINT32 is an unsigned integer with at least 32 bits.
@@ LUAI_INT32 is an signed integer with at least 32 bits.
@@ LUAI_UMEM is an unsigned integer big enough to count the total
@* memory used by Lua.
@@ LUAI_MEM is a signed integer big enough to count the total memory
@* used by Lua.
** CHANGE here if for some weird reason the default definitions are not
** good enough for your machine. (The definitions in the 'else'
** part always works, but may waste space on machines with 64-bit
** longs.) Probably you do not need to change this.
*/

#if LUAI_BITSINT >= 32
#define LUAI_UINT32     uint32_t
#define LUA_UINT32      LUAI_UINT32
#define LUAI_ULINT32    unsigned int  /* 2.3.1 */
#define LUAI_INT32      int
#define LUA_INT32       LUAI_INT32    /* 2.5.4 */
#define LUA_INT64       int64_t       /* 2.9.0 */
#define LUA_UINT64      uint64_t      /* 2.9.0 */
#define LUAI_MAXINT32   INT_MAX
#define LUAI_MININT32   INT_MIN
#define LUAI_UMEM       size_t
#define LUAI_MEM        ptrdiff_t
#else
/* 16-bit ints */
#define LUAI_UINT32     unsigned long
#define LUA_UINT32      LUAI_UINT32
#define LUAI_ULINT32    unsigned long  /* 2.3.1, FIXME does this work on 16-bit platforms ? */
#define LUAI_INT32      long
#define LUA_INT64       long long  /* 2.9.0 */
#define LUA_UINT64      unsigned long long  /* 2.9.0 */
#define LUAI_MAXINT32   LONG_MAX
#define LUAI_MININT32   LONG_MIN
#define LUAI_UMEM       unsigned long
#define LUAI_MEM        long
#endif

#ifndef LUA_MAXINTEGER
#define LUA_MAXINTEGER  LUAI_MAXINT32
#endif
#ifndef LUA_MININTEGER
#define LUA_MININTEGER  LUAI_MININT32
#endif

#define AGN_LASTCONTINT   9007199254740992ULL  /* = (sun_pow(2, 53)), 2.10.0/2.17.1 change, may not be 16-bit compliant FIXME */
#define AGN_DBLNONFRAC    4503599627370496.0   /* 2^52 is the threshold where doubles lose fractional precision */

#ifndef CHARSIZE
#define CHARSIZE  sizeof(char)
#endif

#ifndef uchar
/* macro to `unsign' a character */
#define uchar(c) ((unsigned char)(c))
#endif

/* value used for padding */
#if !defined(LUAL_PACKPADBYTE)
#define LUAL_PACKPADBYTE    0x00
#endif

/* maximum size for the binary representation of an integer */
#define MAXINTSIZE  16

/* maximum integer exponent in the computation of complex powers; controls whether to calculate
   the power in a loop using basic arithmetic or by calls to sun_sincos, sun_atan2, sun_pow
   which determines the runtime behaviour. */
#define AGN_MAXIPOWITER 30

/*
@@ LUAI_MAXCALLS limits the number of nested calls.
** CHANGE it if you need really deep recursive calls. This limit is
** arbitrary; its only purpose is to stop infinite recursion before
** exhausting memory.
*/
#define LUAI_MAXCALLS   20000

/* Maximum number of hash slots in a remember table, 7.5.6.
   WARNING: The more slots, the slower remember tables will become. */
#define AGNI_RTSLOTS    LUAI_MAXCALLS


/*
@@ LUAI_MAXCSTACK limits the number of Lua stack slots that a C function
@* can use.
** CHANGE it if you need lots of (Lua) stack space for your C
** functions. This limit is arbitrary; its only purpose is to stop C
** functions to consume unlimited stack space.
*/
#define LUAI_MAXCSTACK   4096  /* changed from 2048 to 4096 slots 3.15.2 */



/*
** {==================================================================
** CHANGE (to smaller values) the following definitions if your system
** has a small C stack. (Or you may want to change them to larger
** values if your system has a large C stack and these limits are
** too rigid for you.) Some of these constants control the size of
** stack-allocated arrays used by the compiler or the interpreter, while
** others limit the maximum number of recursive calls that the compiler
** or the interpreter can perform. Values too large may cause a C stack
** overflow for some forms of deep constructs.
** ===================================================================
*/


/*
@@ LUAI_MAXCCALLS is the maximum depth for nested C calls (short) and
@* syntactical nested non-terminals in a program.
*/
#define LUAI_MAXCCALLS      200


/*
@@ LUAI_MAXVARS is the maximum number of local variables per function
@* (must be smaller than 250).
*/
#define LUAI_MAXVARS      200


/*
@@ LUAI_MAXUPVALUES is the maximum number of upvalues per function
@* (must be smaller than 250).
*/
#define LUAI_MAXUPVALUES   60


/*
@@ LUAL_BUFFERSIZE is the buffer size used by the lauxlib buffer system.
*/
#if defined(__DJGPP__) || defined(__OS2__) || defined(__linux__) || defined(OPENSUSE)
/* 2.34.9 fix: in DJGPP BUFSIZ is 16k; in OS/2, legacy OpenSUSE and modern Linux (32bit and 64bit) BUFSIZ is 8k.
   We set it to 512 bytes, the Windows default, to significantly speed up I/O and string assembly. Especially,
   the speed increase on native, non-VM FreeDOS 1.3, is 2.5 times. */
#define LUAL_BUFFERSIZE      512
#else
#define LUAL_BUFFERSIZE      BUFSIZ
#endif

/*
@@ LUAI_MAXALIGN defines fields that, when used in a union, ensure
** maximum alignment for the other items in that union.
*/
#define LUAI_MAXALIGN  lua_Number n; double u; void *s; lua_Integer i; long l

/* }====================================================================== */

/* Key to file-handle type */
#define LUA_FILEHANDLE       "FILE*"
#define AGN_FILEHANDLE       "BINIOFILE*"

/* }====================================================================== */

/*
@@ LUAI_REGSIZE is the default size of an Agena register data structure.
*/
#define LUAI_REGSIZE         16

/*
@@ Numeric for loops with fractional step size: in [-1-|step| , 1+|step|] round halfway up.
   See lm.c/OP_NUMLOOP & tools_ndigplaces:
*/
#define AGN_FORNUMRNDDIR   4

/*
@@ LUAI_NTYPELIST determines how many types may be given in a type check list of a procedure.
   (GREP_POINT) types; if you add new number `types', 2.12.1 extension

  If you add further types, remember to change lua_typecheck, but the penalty of using (u)int64_t is 25 % performance loss. Formula:

  sum := 0
  for i from 0 to 4 do     # 0 to 4 = #LUAI_NTYPELIST
     sum := sum + 19*19^i  # 19     = LUAI_NTYPELISTMOD
  od
  sum:           ->    2613659
  environ.maxint -> 2147483647 (maximum value for int32_t)
*/
#define lua_typecheck       int32_t

/* int32_t or uint32_t do not support more than 7 given types with 19 types available, with a max of five given types, keep room for more new types */
#define LUAI_NTYPELIST     5

/*
** {==================================================================
** Dependencies with C99 and other C details
** ===================================================================
*/

/*
@@ l_sprintf is equivalent to 'snprintf' or 'sprintf' in C89.
** (All uses in Lua have only one format item.)
*/
#if !defined(LUA_USE_C89)
#define l_sprintf(s,sz,f,i)  snprintf(s,sz,f,i)
#else
#define l_sprintf(s,sz,f,i)  ((void)(sz), sprintf(s,f,i))
#endif


/*
** {==================================================================
@@ LUA_NUMBER is the type of numbers in Lua.
** CHANGE the following definitions only if you want to build Lua
** with a number type different from double. You may also need to
** change lua_number2int & lua_number2integer.
** ===================================================================
*/

#define LUA_NUMBER_DOUBLE
#define LUA_NUMBER   double

/* for math.xrandom/seed, 2.27.10 */
#include <float.h>
#define l_mathop(op)    op
#define l_floatatt(n)   (DBL_##n)
/* end of math.xrandom/seed */

#ifndef __cplusplus

#ifndef PROPCMPLX
#include <complex.h>
#define agn_Complex  double complex  /* complex number data type */
#define agn_pComplex agn_Complex     /* type of a pointer into a complex double array */
#define real(z) creal(z)
#define imag(z) cimag(z)
#else
typedef double agn_Complex[2];       /* complex number data type */
typedef double *agn_pComplex;        /* type of a pointer into a complex double array */
#define real(z) z[0]
#define imag(z) z[1]
#endif

#else  /* we are in C++ */

/* The following may look very weird and surely is, but this is the only way to compile AgenaEdit */
#ifndef PROPCMPLX

#ifdef __linux__
#include <complex.h>
#else
#include <complex>
using std::complex;
#endif
#define agn_Complex std::complex<double>
/* #define agn_Complex double complex */
#define real(z) creal(z)
#define imag(z) cimag(z)

#else

#define agn_Complex std::complex<double>

#endif  /* of PROPCMPLX */

#endif  /* __cplusplus */

/*
@@ LUAI_UACNUMBER is the result of an 'usual argument conversion'
@* over a number.
*/
#define LUAI_UACNUMBER   double


/*
@@ LUA_NUMBER_SCAN is the format for reading numbers.
@@ LUA_NUMBER_FMT is the format for writing numbers.
@@ lua_number2str converts a number to a string.
@@ LUAI_MAXNUMBER2STR is maximum size of previous conversion.
@@ lua_str2number converts a string to a number.
*/
#define LUA_NUMBER_SCAN      "%lf"
#define LUA_NUMBER_FMT       "%.14g"  /* use a leading zero if you want to output less than ten digits (see agn_setdigits) */
#define lua_number2str(s,n)  sprintf((s), L->numberformat, (n))
#define LUAI_MAXNUMBER2STR   32 /* 16 digits, sign, point, and \0 */
#define lua_str2number(s,p)  strtod((s), (p))  /* GREP "STRTOD" if you change this as other functions depend on the ERANGE error thrown by strtod */


/*
@@ Default name for the logfile.
*/
#define AGN_LOGFILE   "agena.log"


/*
@@ lua_numbertointeger converts a float number with an integral value
** to an integer, or returns 0 if float is not within the range of
** a lua_Integer.  (The range comparisons are tricky because of
** rounding. The tests here assume a two-complement representation,
** where MININTEGER always has an exact representation as a float;
** MAXINTEGER may not have one, and therefore its conversion to float
** may have an ill-defined value.) Taken from Lua 5.4.0 RC 5, 2.21.2
*/
#define lua_numbertointeger(n,p) \
  ((n) >= (LUA_NUMBER)(LUA_MININTEGER) && \
   (n) < -(LUA_NUMBER)(LUA_MININTEGER) && \
      (*(p) = (LUA_INTEGER)(n), 1))

/*
@@ The luai_num* macros define the primitive operations over numbers.
*/


#include <math.h>
#include "agncmpt.h"

/* #define luai_numsign(x)     ( ((x) == 0) ? 0 : (((x) < 0) ? -1 : ((x) > 0 ? 1 : AGN_NAN)) ) */ /* use tools_sign instead, it is 4 % faster */
/* #define luai_numsignum(x)   ( ((x) < 0) ? -1 : ((x) >= 0) ? 1 : AGN_NAN ) */ /* use tools_signum instead, it is 4 % faster */
#define luai_numadd(a,b)    ((a)+(b))
#define luai_numsub(a,b)    ((a)-(b))
#define luai_nummul(a,b)    ((a)*(b))
#define luai_numdiv(a,b)    ( ((b) != 0) ? ((a)/(b)) : (AGN_NAN) )  /* changed 0.10.0 */
#define luai_numrecip(a)    ( ((a) != 0) ? (1/(a)) : (AGN_NAN) )    /* added 1.13.0 */
#define luai_numpercent(a,b)  ( (a)*(b)/100 )  /* added 1.11.3, fixed 2.1 RC 1 */
#define luai_numpercentratio(a,b)  ( ((b) != 0) ? ((a)/(b))*100 : (AGN_NAN) )      /* added 1.10.6 */
#define luai_numpercentchange(a,b) ( ((a) != 0) ? ((b)/(a))*100-100 : (AGN_NAN) )  /* added 2.10.0 */
#define luai_numpercentadd(a,b) ( (a) + ((a)*(b)/100) )  /* added 1.11.3, fixed 2.1 RC 1 */
#define luai_numpercentsub(a,b) ( (a) - ((a)*(b)/100) )  /* added 1.11.3, fixed 2.1 RC 1 */
#define luai_numintdiv(a,b)  sun_intdiv(a,b)  /* 0.32.2b fix, use sun_intdiv instead, 6 % faster */
#define luai_nummod(a,b)    ((a) - sun_floor((a)/(b))*(b))  /* works like in Maple; the SunPro `remainder` version returns non-Maple results */
#define luai_numpow(a,b)    (sun_pow((a),(b),1))     /* 2.11.1 tuning, 1 for faster integer exponentiation */
/* #define luai_numipow(a,b)   (tools_intpow((a),(b))) */ /* added 0.9.2; integer exponentiation is 40 % faster than pow */
#define luai_numipow(a,b)   (cephes_powi((a),(b)))  /* added 0.9.2; integer exponentiation is 40 % faster than pow, changed 3.16.3 */
#define luai_numabs(a)      (fabs((a)))      /* added 0.6.0; fabs is faster than a macro */
#define luai_numasin(a)     (sun_asin((a)))  /* added 0.27.0, although Sun's implementation is not faster */
#define luai_numacos(a)     (sun_acos((a)))  /* added 0.27.0, although Sun's implementation is not faster */
#define luai_numasec(a) ({ \
  lua_Number __nasecx; \
  __nasecx = 1/(a); \
  __nasecx = ( (fabs((__nasecx)) > 1) ? AGN_NAN : acos((__nasecx)) ); \
  (__nasecx); \
})
#define luai_numatan(a)     (sun_atan((a)))    /* added 0.7.1, 2.11.0 tuning */
#define luai_numexp(a)      (sun_exp((a)))     /* added 0.6.0, 2.11.0 tuning */
#define luai_numantilog2(a) (tools_exp2((a)))  /* added 2.4.1, changed 2.29.1 */
#define luai_numantilog10(a) (exp10((a)))      /* added 2.4.1 */
#define luai_numsqrt(a)     ( ((a) >= 0) ? (sqrt((a))) : (AGN_NAN) )  /* changed 0.10.0, Sun's sqrt is 7 times slower in this context */
#define luai_numln(a)       (sun_log((a)))     /* changed 0.10.0, 2.11.0/2.14.13 tuning, no need to check for <= 0 any longer */
#define luai_numsin(a)      (sun_sin((a)))     /* added 0.6.0, 2.11.0 tuning */
/* #define luai_numsinc(a)     (fabs(a) < 1e-4 ? 1 - (0.1666666666666666666667)*a*a : sun_sin((a))/(a)) */ /* added 2.4.2, changed 2.21.6,
  see: http://ab-initio.mit.edu/Faddeeva.cc; changed 6.7.9 */
/* added 2.4.2, changed 2.21.6, see: http://ab-initio.mit.edu/Faddeeva.cc; changed 6.7.9
  Maple V Release 4:
  > Digits := 25: interface(prettyprint=0):
  > convert(series(sin(x)/x, x=0, 12), polynom);  # get rid of O() term
  > evalf(convert(", horner, x));
  > f := unapply(subs(x^2=z, "), z); */
#define luai_numsinc(a) ({ \
  lua_Number __z; \
  if (fabs(a) < 1e-4) { \
    __z = (a)*(a); \
    __z = 1. + (-.1666666666666666666666667 + \
               (0.8333333333333333333333333e-2 + \
               (-.1984126984126984126984127e-3 + \
               (0.2755731922398589065255732e-5 - \
               0.2505210838544171877505211e-7*__z)*__z)*__z)*__z)*__z; \
  } else { \
    __z = sun_sin((a))/(a); \
  } \
  (__z); \
})
#define luai_numcos(a)      (sun_cos((a)))     /* added 0.6.0, 2.11.0 tuning */
#define luai_numbea(a)      (AGN_NAN*(a))      /* added 2.1.2, multiplication with a to prevent compiler warning */
#define luai_numflip(a)     ( (a)*0 )          /* added 2.1.2, multiplication with a to prevent compiler warning, changed 2.12.3 */
#define luai_numtan(a)      (sun_tan((a)))     /* added 0.7.1, 2.11.0 tuning */
#define luai_numsinh(a)     (sun_sinh((a)))    /* added 0.23.0 */
#define luai_numcosh(a)     (sun_cosh((a)))    /* added 0.23.0 */
/* #define luai_numsinhcosh    sun_sinhcosh */ /* added 2.32.6, 4.5.7 overflow tweak */
#define luai_numtanh(a)     (sun_tanh((a)))    /* added 0.23.0 */
#define luai_numtanhl(a)    (tools_tanhl((a))) /* added 6.6.9 */
#define luai_numcot(a)      ((sun_sin(a) == 0.0) ? AGN_NAN : 1/sun_tan(a))  /* added 3.1.3.4 */
#define luai_numcotl(a)     ((sun_sinl(a) == 0.0L) ? AGN_NAN : 1.0L/sun_tanl(a))  /* added 6.6.9 */
#define luai_numcsc(a) ({ \
  lua_Number r; \
  r = sun_sin(a); \
  r = (r == 0.0) ? AGN_NAN : 1/r; \
  (r); \
})
#define luai_numcscl(a) ({ \
  long double r; \
  r = sun_sinl(a); \
  r = (r == 0.0L) ? AGN_NAN : 1.0L/r; \
  (r); \
})
#define luai_numsec(a) ({ \
  lua_Number r; \
  r = sun_cos(a); \
  r = (r == 0.0) ? AGN_NAN : 1/r; \
  (r); \
})
#define luai_numsecl(a) ({ \
  long double r; \
  r = sun_cosl(a); \
  r = (r == 0.0L) ? AGN_NAN : 1.0L/r; \
  (r); \
})
#define luai_numint(a)      (sun_trunc((a)))   /* added 0.6.0, changed from trunc to sun_trunc 2.35.0 */
#define luai_numfrac(a)     (sun_frac(a))      /* added 2.3.3; tuned 2.29.5, 3 % faster than ((a) - trunc((a))) */
#define luai_numentier(a)   (sun_floor((a)))   /* added 0.7.1, 2.11.1 tuning */
#define luai_numiseven(a)   (tools_iseven(a))  /* added 0.7.1, 2.14.13, ISFLOAT(a) && a & 1 == 0, or fmod((a), 2) == 0 are slower */
#define luai_numisodd(a)    (tools_isodd(a))   /* added 2.10.0, 2.14.13, fmod((a), 2) != 0 is slower */
#define luai_numisminuszero(a)  (((a) == 0 && signbit((a)) != 0))  /* added 2.10.0  */
#define luai_numlngamma(a)  ( ((a) > 0) ? (sun_lgamma((a))) : (AGN_NAN) )  /* changed 0.10.0; tuned 2.17.4, use Sun implementation; in Agena, we won't allow for non-positive args */
#define luai_numunm(a)      (-(a))
#if defined(__linux__)  /* introduced 2.18.1 */
#define luai_numeq(a,b)     (((a) == (b)) || ((tools_isnan(((lua_Number)a)) && tools_isnan(((lua_Number)b)))))  /* changed 0.10.0, 2.12 RC 3, Raspi won't compile otherwise, changed 2.18.1 */
#else
#define luai_numeq(a,b)     (((a) == (b)) || ((tools_isnan(a) && tools_isnan(b))))
#endif
#define luai_numlt(a,b)     ((a) < (b))
#define luai_numle(a,b)     ((a) <= (b))
#if defined(__linux__)
#define luai_numisnan(a)    (tools_isnan(((lua_Number)a)))  /* undefined ? patch 2.12.0 RC 3, would not compile in Raspbian Stretch otherwise, changed 2.18.1 */
#else
#define luai_numisnan(a)    (tools_isnan((a)))  /* undefined ? */
#endif
#define luai_numisfinite(a) (isfinite((a)))  /* added 0.9.2; +/- infinity or undefined ? */
#define luai_numisinfinite(a) ((a) == HUGE_VAL || (a) == -HUGE_VAL)  /* added 2.10.0, +/- infinity ? */
#define luai_bnot(a)        (~(LUA_INT64)(a))                        /* added 0.27.0, changed 2.9.0 */
#define luai_numconjugate(a)  ((a))                                  /* added 2.1.2 */
#define luai_numpeps(a)     (sun_nextafter((a), HUGE_VAL))           /* added 2.9.1, changed 2.11.0 */
#define luai_nummeps(a)     (sun_nextafter((a), -HUGE_VAL))          /* added 2.9.1, changed 2.11.0 */
#define luai_numabsdiff(a, b)  ((a) > (b) ? (a) - (b) : (b) - (a))   /* added 2.9.1 */


/*
@@ The agnc_* macros define the primitive operations over complex numbers.
*/

#ifndef PROPCMPLX
#define agnc_add(a,b) { \
  agn_Complex r; \
  __real__ r = creal(a) + creal(b); \
  __imag__ r = cimag(a) + cimag(b); \
  (r); \
}

#define agnc_sub(a,b) { \
  agn_Complex r; \
  __real__ r = creal(a) - creal(b); \
  __imag__ r = cimag(a) - cimag(b); \
  (r); \
}

/* 10 percent faster than using GCC's generic complex multiplication with C's `*` */
/* if a*c = undefined (= I*infinity or infinity*I), no correction due to performance */
#define agnc_mul(x,y) { \
  agn_Complex r; \
  lua_Number a, b, c, d; \
  a = creal(x); b = cimag(x); c = creal(y); d = cimag(y); \
  __real__ r = fma(a, c, -b*d); \
  __imag__ r = fma(a, d, b*c); \
  (r); \
}

/* 22 % faster than using GCC's generic complex division;

   Before:
   (1.0 + 0*I)/(1e200 + 1e200*I):
   2.0747577844405e-020-2.0747577844405e-020*I

   After:
   5e-201

   Before:
   (1.0 + 0*I)/(1e-160+ 1e-160*I):
   1.2049599325514e-021-1.2049599325514e-021*I

   After:
   5e+159-5.0000556647063e+159*I

   Before:
   (1.0 + 0*I)/(1e300 + I):
   0

  After:
  1e-300 */
/* 2.11.1 tuning: 20 percent faster, patched 2.17.6, Kahan-Smith Complex Division 6.7.9/7.1.6 */
#define agnc_div(x,y) { \
  agn_Complex r; \
  lua_Number a, b, c, d, den, ratio; \
  a = creal(x); b = cimag(x); c = creal(y); d = cimag(y); \
  if ((c) == 0 && (d) == 0) (r) = AGN_NAN; \
  else { \
    if (fabs(c) >= fabs(d)) { \
      ratio = d/c; \
      den = fma(ratio, d, c); \
      __real__ r = fma(b, ratio, a)/den; \
      __imag__ r = fma(-a, ratio, b)/den; \
    } else { \
      ratio = c/d; \
      den = fma(ratio, c, d); \
      __real__ r = fma(a, ratio, b)/den; \
      __imag__ r = fma(b, ratio, -a)/den; \
    } \
  } \
  (r); \
}

#define agnc_recip(x) { \
  agn_Complex r; \
  lua_Number c, d, den, ratio; \
  c = creal(x); d = cimag(x); \
  if ((c) == 0 && (d) == 0) (r) = AGN_NAN; \
  else { \
    if (fabs(c) >= fabs(d)) { \
      ratio = d/c; \
      den = fma(ratio, d, c); \
      __real__ r = 1.0/den; \
      __imag__ r = (-ratio)/den; \
    } else { \
      ratio = c/d; \
      den = fma(ratio, c, d); \
      __real__ r = (ratio)/den; \
      __imag__ r = -1.0/den; \
    } \
  } \
  (r); \
}

#define agnc_pow(a,b)  tools_cpow((a),(b))

/* 2.11.2 tuning 3 % faster */
#define agnc_antilog2(a) { \
  agn_Complex r; \
  lua_Number x, y, t3, t4, si, co; \
  x = creal(a); y = cimag(a); \
  t3 = sun_exp(x*LN2); \
  t4 = y*LN2; \
  sun_sincos(t4, &si, &co); \
  __real__ r = t3*co; \
  __imag__ r = t3*si; \
  (r); \
}

/* 2.11.2 tuning 3 % faster */
#define agnc_antilog10(a) { \
  agn_Complex r; \
  lua_Number x, y, t3, t4, si, co; \
  x = creal(a); y = cimag(a); \
  t3 = sun_exp(x*LN10); \
  t4 = y*LN10; \
  sun_sincos(t4, &si, &co); \
  __real__ r = t3*co; \
  __imag__ r = t3*si; \
  (r); \
}

#define agnc_unm(a)         ((-1)*(a))  /* using __real__/__imag__ is a little bit slower */

/* isnan does not accept double complex numbers, 2.12.0 RC 3/7.1.7 fixes for all OS'ses */
#define agnc_eq(a,b)        (((a)==(b)) || ((tools_isnanc((a)) && tools_isnanc((b)))))

/* 0.31.2 patch: if imag(r) == -0, then other complex functions return the imaginary unit with the wrong sign;
   switch from generic `csin` to Maple's `evalc format` 2.11.1, twice as fast. */
#define agnc_sin(z) { \
  agn_Complex r; \
  lua_Number a, b, si, co, sih, coh; \
  (a) = creal(z); \
  (b) = cimag(z); \
  sun_sincos((a), &si, &co); \
  sun_sinhcosh((b), &sih, &coh); \
  __real__ (r) = (si*coh); \
  __imag__ (r) = (co*sih) + 0.0; \
  (r); \
}


#define agnc_cos(z) { \
  agn_Complex r; \
  lua_Number a, b, si, co, sih, coh; \
  (a) = creal(z); \
  (b) = cimag(z); \
  sun_sincos((a), &si, &co); \
  sun_sinhcosh((b), &sih, &coh); \
  __real__ (r) = (co*coh); \
  __imag__ (r) = (-si*sih) + 0.0; \
  (r); \
}


#define agnc_tan(a)         (tools_ctan((a)))   /* 2.11.1 tuning */

/* 0.31.2 patch: if imag(r) == -0, then other complex functions return the imaginary unit with the wrong sign */

/* 2.1.2, FRACTINT's buggy cos function till v16, creates beautiful fractals */
#define agnc_cosxx(z) { \
  agn_Complex r; \
  lua_Number a, b, si, co, sih, coh; \
  (a) = creal(z); \
  (b) = cimag(z); \
  sun_sincos((a), &si, &co); \
  sun_sinhcosh((b), &sih, &coh); \
  __real__ (r) = (co*coh); \
  __imag__ (r) = (si*sih) + 0.0; \
  (r); \
}

/* 2.1.2, 2.11.1 change: avoid problems with infinity!infinity which otherwise would get
   converted to undefined!infinity. */
#define agnc_flip(z) { \
  agn_Complex r; \
  __real__ (r) = cimag(z); \
  __imag__ (r) = creal(z); \
  (r); \
}

/* 2.1.2, no need to change as __imag__ version (mere negation of imaginary part) is not faster  */
#define agnc_conjugate(a) { \
  agn_Complex r; \
  (r) = conj(a); \
  (r); \
}

/* 2.11.1 tuning; sinh(z) = (exp(z) - exp(-z))/2 with exp(-z) = 1/exp(z) */
#define agnc_sinh(a) { \
  agn_Complex r; \
  lua_Number x, y, si, co, u, v; \
  x = creal(a); \
  y = cimag(a); \
  sun_sincos(y, &si, &co); \
  u = sun_exp(x); v = 1.0/u; \
  u = 0.5*(u + v); v = u - v; \
  __real__ r = (v*co); \
  __imag__ r = (u*si); \
  (r); \
}

/* cosh(z) = (exp(z) + exp(-z))/2 with exp(-z) = 1/exp(z) */
#define agnc_cosh(a) { \
  agn_Complex r; \
  lua_Number x, y, si, co, u, v; \
  x = creal(a); \
  y = cimag(a); \
  sun_sincos(y, &si, &co); \
  u = sun_exp(x); v = 1.0/u; \
  u = 0.5*(u + v); v = u - v; \
  __real__ r = (u*co); \
  __imag__ r = (v*si); \
  (r); \
}

#define agnc_tanh(z)        (tools_ctanh((z)))   /* a Sun-based implementation is slower */

#define agnc_exp(z) { \
  agn_Complex r; \
  lua_Number a, b, t1; \
  (a) = __real__ (z); \
  (b) = __imag__ (z); \
  t1 = sun_exp((a)); \
  sun_sincos((b), &a, &b); \
  __real__ r = (t1*b); \
  __imag__ r = (t1*a); \
  (r); \
}

#define agnc_atan(z)        (tools_catan((z)))

#define agnc_sinc(z) { \
  agn_Complex (r); \
  lua_Number a, b; \
  a = creal(z); b = cimag(z); \
  if (a == 0.0 && b == 0.0) \
    (r) = 1 + 0*I; \
  else { \
    lua_Number si, co, sih, coh; \
    sun_sincos(a, &si, &co); \
    sun_sinhcosh(b, &sih, &coh); \
    __real__ r = si*coh; \
    __imag__ r = co*sih + 0.0; \
    (r) = tools_cdiv((r), (z)); \
  } \
  (r); \
}

/* 2.1.2 */
#define agnc_bea(z) { \
  agn_Complex r; \
  lua_Number x, y, si, co, sih, coh; \
  x = creal(z); \
  y = cimag(z); \
  sun_sincos(x, &si, &co); \
  sun_sinhcosh(y, &sih, &coh); \
  __real__ r = (si*sih); \
  __imag__ r = (co*coh) + 0.0; \
  (r); \
}


/* patched 0.31.2, ANSI C casin returns wrong results */

#if (!(defined(__SOLARIS) && defined(__sparc)))  /* no Sun SPARC ? */

#define agnc_asin(a)        (tools_casin((a)))

/* patched 0.31.2, ANSI C cacos returns wrong results, e.g. arccos(-1) = undefined, and arccos(1) = undefined */
#define agnc_acos(a) { \
  agn_Complex (r); \
  r = tools_casin((a)); \
  __real__ (r) = PIO2 - __real__ r; \
  __imag__ (r) = - __imag__ r; \
  (r); \
}

#define agnc_asec(a) { \
  agn_Complex (r); \
  r = tools_casin((1/a)); \
  __real__ (r) = PIO2 - __real__ r; \
  __imag__ (r) = - __imag__ r; \
  (r); \
}

#else  /* now Sun Solaris 10 SPARC follows */

#define agnc_asin(z) { \
  agn_Complex (r); \
  (r) = casin((z)); \
  if ((cimag((z)) == 0 || cimag((z)) == -0) && creal((z)) > 0) (r) = conj((r)); \
  (r); \
}

#define agnc_acos(z) { \
  agn_Complex (r); \
  (r) = cacos((z)); \
  if ((cimag((z)) == 0 || cimag((z)) == -0) && creal((z)) > 0) (r) = conj((r)); \
  (r); \
}

#define agnc_asec(z) { \
  agn_Complex (r), (s); \
  s = 1/(z); \
  (r) = cacos((s)); \
  if ((cimag((s)) == 0 || cimag((s)) == -0) && creal((s)) > 0) (r) = conj((r)); \
  (r); \
}

#endif  /* of if Sun Solaris 10 SPARC */


/* #define agnc_ln(a)         (((a) == 0) ? (AGN_NAN) : clog((a))) */ /* 0.26.1 fix */
/* 2.11.1 twice as fast as the GCC clog implementation
   The 0.5*ln() version is 10 % faster than the ldexp(ln(), -1) implementation; 2.32.6 */
#define agnc_ln(z) { \
  agn_Complex r; \
  lua_Number a, b, t; \
  (a) = __real__ (z); \
  (b) = __imag__ (z); \
  if ((a) == 0.0 && (b) == 0.0) { \
    __real__ r = AGN_NAN; \
    __imag__ r = 0.0; \
  } else { \
    t = sun_pytha((a), (b)); \
    __real__ r = 0.5*sun_log(t); \
    __imag__ r = sun_atan2((b), (a)); \
  } \
  (r); \
}

#define agnc_sqrt slm_csqrt

/* 2.11.1 tuning, 3 times faster; on Windows, tools_hypot and cabs return the most equal results other than sun_hypot */
#define agnc_abs(a) { \
  lua_Number __absxx; \
  __absxx = sun_hypot(creal(a), cimag(a)); \
  (__absxx); \
}

#define agnc_absdiff(a, b) { \
  lua_Number __absdiffxx; \
  agn_Complex r; \
  r = ((a) - (b)); \
  __absdiffxx = sun_hypot(creal(r), cimag(r)); \
  (__absdiffxx); \
}

/* works exactly like the floor operator as implemented in Maple V Release 4, Maple 14 */
#define agnc_entier(a) { \
  lua_Number fre, fim, re, im, t; \
  agn_Complex (r), (X); \
  re = (creal(a)); \
  im = (cimag(a)); \
  fre = sun_floor((re)); \
  fim = sun_floor((im)); \
  re = (re) - fre; \
  im = (im) - fim; \
  t = re + im; \
  if ((t) < 1) { \
    X = 0.0+0.0*I; } \
  else if ((t) >= 1.0 && (re >= im)) { \
    X = 1.0+0.0*I; } \
  else { \
    X = 1.0*I; } \
  r = fre + I*fim + X; \
  (r); \
}

/* 0.33.2 */
#define agnc_int(a) { \
  lua_Number re, im; \
  agn_Complex (r); \
  re = (creal(a)); \
  im = (cimag(a)); \
  r = sun_trunc((re)) + I*sun_trunc((im)); \
  (r); \
}

/* 2.3.3 */
#define agnc_frac(a) { \
  lua_Number re, im; \
  agn_Complex (r); \
  re = (creal(a)); \
  im = (cimag(a)); \
  r = ((re) - sun_trunc((re))) + I*((im) - sun_trunc((im))); \
  (r); \
}

/* 0.22.2: compute the INTEGER power of (z::complex)^(e::integer). If n is a posint and |n| < 30 then
   use faster iteration formula. If |n| >= 30, then the iteration formula is slower on an Atom 330 CPU
   than the general formula. Tuned 2.35.3 */
#define agnc_ipow(z,e) {  \
  lua_Number (a), (b), (n);  \
  agn_Complex (r); \
  (a) = creal((z)); (b) = cimag((z)); \
  if (cimag((e)) != 0.0) luaG_runerror(L, "exponent must be of type number"); \
  n = creal((e)); \
  if (fabs(n) <= AGN_MAXIPOWITER) { \
    if ((n) == 0.0) { \
      r = (a == 0.0 && b == 0.0) ? AGN_NAN : 1.0 + 0.0*I; \
    } else { \
      lua_Number (ra), (rb), (t); \
      int i, isneg; \
      isneg = n < 0.0; if (isneg) n = -n; \
      ra = (a); rb = (b); \
      for (i=1; i < (int)((n)); i++) { \
        t = (a); \
        (a) = fma((a), ra, -(b)*rb); \
        (b) = fma(t, rb, (b)*ra); \
      } \
      r = (a) + (b)*I; \
      if (isneg) r = tools_crecip(r); \
    } \
  }  \
  else { \
    lua_Number t1, t5, t6, t7, t8, t9; \
    t1 = sun_pytha((a), (b)); \
    t5 = sun_pow(t1, 0.5*(n), 0); \
    t6 = sun_atan2((b),(a)); \
    t7 = (n)*t6; \
    sun_sincos(t7, &t9, &t8); \
    r = t5*(t8 + I*t9); \
  }; \
  (r); \
}


#define agnc_lngamma(z) { \
  (cephes_clgam(z)); \
}


/* like in Maple V R4 */
#define agnc_signum(a) { \
  agn_Complex (r); \
  lua_Number d; \
  d = sun_hypot((creal(a)), (cimag(a))); \
  r = (d == 0.0) ? 0.0 : a/d; \
  (r); \
}

/* Maple's evalc version is half as fast */

#else  /* PROPCMPLX start */

#define agnc_add(z,a,b,c,d)    agnCmplx_create((z), ((a)+(c)), ((b)+(d)))
#define agnc_sub(z,a,b,c,d)    agnCmplx_create((z), ((a)-(c)), ((b)-(d)))
#define agnc_mul(z,a,b,c,d)    agnCmplx_create((z), (fma((a),(c),-(b)*(d))), (fma((a),(d),(b)*(c))))

/* 6.7.9/7.1.6: Introduced Kahan-Smith Complex Division to prevent underflow and overflow:

  Before:
  (1.0 + 0*I)/(1e200 + 1e200*I):
  2.0747577844405e-020-2.0747577844405e-020*I

  After:
  5e-201

  Before:
  (1.0 + 0*I)/(1e-160+ 1e-160*I):
  1.2049599325514e-021-1.2049599325514e-021*I

  After:
  5e+159-5.0000556647063e+159*I

  Before:
  (1.0 + 0*I)/(1e300 + I):
  0

  After:
  1e-300 */
#define agnc_div(z,a,b,c,d) { \
  lua_Number den, ratio; \
  if ((c) == 0 && (d) == 0) { \
    agnCmplx_create((z), AGN_NAN, AGN_NAN); \
  } else { \
    if (fabs(c) >= fabs(d)) { \
      ratio = (d)/(c); \
      den = fma(ratio, (d), (c)); \
      agnCmplx_create((z), fma((b), ratio, (a))/den, fma(-(a), ratio, (b))/den); \
    } else { \
      ratio = (c)/(d); \
      den = fma(ratio, (c), (d)); \
      agnCmplx_create((z), fma((a), ratio, (b))/den, fma((b), ratio, -(a))/den); \
    } \
  } \
}

#define agnc_recip(z,c,d) { \
  lua_Number den, ratio; \
  if ((c) == 0 && (d) == 0) { \
    agnCmplx_create((z), AGN_NAN, AGN_NAN); \
  } else { \
    if (fabs(c) >= fabs(d)) { \
      ratio = (d)/(c); \
      den = fma(ratio, (d), (c)); \
      agnCmplx_create((z), (1.0)/den, (-ratio)/den); \
    } else { \
      ratio = (c)/(d); \
      den = fma(ratio, (c), (d)); \
      agnCmplx_create((z), (ratio)/den, (-1.0)/den); \
    } \
  } \
}

/* patched 1.12.7, 1.12.9; 2.35.2/3 accuracy patch for integral powers taken from agnc_ipow */
#define agnc_pow(z,a,b,c,d) { \
  lua_Number t1, t4, t6, t9, t12, si, co; \
  if ( \
    (a) == 0 && (b) == 0 && \
    ( ( (c) == 0 && (d) != 0 ) || ( (c) > 0 && (d) == 0 ) ) \
    ) { \
    agnCmplx_create((z), 0, 0); \
  } else { \
    if (d == 0 && ((tools_isposint(c) && c <= AGN_MAXIPOWITER) || c == -1)) { \
      if (c == -1) { \
        lua_Number x, y; \
        tools_crecip((a), (b), &x, &y); \
        agnCmplx_create((z), x, y); \
      } else { \
        int i; \
        lua_Number aa, bb, ra, rb, t; \
        ra = (a); rb = (b); aa = (a); bb = (b); \
        for (i=1; i < (int)c; i++) { \
          t = aa; \
          aa = fma(aa, ra, -bb*rb); \
          bb = fma(t, rb, bb*ra); \
        } \
        agnCmplx_create((z), aa, bb); \
      } \
    } else { \
      t1 = sun_pytha((a), (b)); t4 = sun_log(t1); t6 = sun_atan2((b),(a)); \
      t9 = sun_exp(fma((c), t4*0.5, -(d)*t6)); t12 = fma(d, t4*0.5, (c)*t6); \
      sun_sincos(t12, &si, &co); \
      agnCmplx_create((z), t9*co, t9*si); \
    } \
  } \
}  /* 2.4.1 modified to check for very small imaginary values */

#define agnc_unm(z,a,b)        agnCmplx_create((z), (-(a)), (-(b)))

#define agnc_eq(a,b,c,d)       (((a)==(c) && (b)==(d)) || ((tools_isnan((a)) && tools_isnan((c)))))

#define agnc_sin(z,a,b) { \
  lua_Number i, si, co, sih, coh; \
  sun_sincos(a, &si, &co); \
  sun_sinhcosh(b, &sih, &coh); \
  (i) = co*sih; \
  agnCmplx_create((z), si*coh, i + 0.0); \
}

#define agnc_cos(z,a,b) { \
  lua_Number i, si, co, sih, coh; \
  sun_sincos(a, &si, &co); \
  sun_sinhcosh(b, &sih, &coh); \
  (i) = -si*sih; \
  agnCmplx_create((z), co*coh, i + 0.0); \
}

/* 2.1.2 */
#define agnc_cosxx(z,a,b) { \
  lua_Number i, si, co, sih, coh; \
  sun_sincos(a, &si, &co); \
  sun_sinhcosh(b, &sih, &coh); \
  (i) = -si*sih; \
  agnCmplx_create((z), co*coh, -i + 0.0); \
}

/* 2.1.2 */
#define agnc_flip(z,a,b) { \
    agnCmplx_create((z), (b), (a)); \
}

/* 2.1.2 */
#define agnc_conjugate(z,a,b) { \
  if ( (b) == 0 ) { \
    agnCmplx_create((z), (a), 0); \
  } else { \
    agnCmplx_create( (z), (a), (-(b)) ); \
  } \
}

/* 2.1.2 */
#define agnc_bea(z,a,b) { \
  lua_Number i, si, co, sih, coh; \
  sun_sincos(a, &si, &co); \
  sun_sinhcosh(b, &sih, &coh); \
  (i) = co*coh; \
  agnCmplx_create((z), si*sih, i + 0.0); \
}


#define agnc_tan(z,a,b) { \
  lua_Number t2, t5, t6, t8, si, coh; \
  sun_sincos(a, &si, &t2); \
  sun_sinhcosh(b, &t5, &coh); \
  t6 = sun_pytha(t2, t5); \
  t8 = (t6 == 0.0) ? AGN_NAN : 1.0/t6; \
  agnCmplx_create((z), si*t2*t8, t5*coh*t8); \
}

/* 2.12.4 patch for OS/2: deliberately query b = 0 to prevent round-off errors and false undefined's,
   2.17.5 fix for |a| > 1 && b = 0

> readlib(C):
> assume(a, real, b, real):
> z := a+I*b:
> C(evalc(arcsin(z)), optimized);
*/

#define agnc_asin(z,a,b) { \
  if ((b) == 0) { \
    if (fabs(a) <= 1) { \
      agnCmplx_create((z), sun_asin((a)), 0); \
    } else { \
      lua_Number __casin_a, __casin_b; \
      tools_casin((a), (b), &__casin_a, &__casin_b); \
      agnCmplx_create((z), __casin_a, __casin_b); \
    } \
  } else { \
    lua_Number t1, t2, t3, t4, t6, t12, t13, t14; \
    t1 = (a)*(a); \
    t2 = (b)*(b); \
    t3 = 1.0 + t2; \
    t4 = sqrt(t1 + 2.0*(a) + t3); \
    t6 = sqrt(t1 - 2.0*(a) + t3); \
    t12 = tools_square((t4 + t6)*0.5); \
    t13 = t12 - 1.0; \
    if (fabs(t13) < 1.2e-019) (t13) = 0.0; \
    t14 = sqrt((t13)); \
    agnCmplx_create((z), sun_asin(((t4) - (t6))*0.5), tools_csgn((b), -(a)) * sun_log(((t4) + (t6))*0.5 + (t14))); \
  } \
}

/* 2.12.4 patch for OS/2: deliberately query b = 0 to prevent round-off errors and false undefined's,
   2.17.5 fix for |a| > 1 && b = 0 */
/* DO _NOT_ USE SUN_pow if b == 0 !!! Use pow to prevent wrong results */
#define agnc_acos(z,a,b) { \
  if ((b) == 0) { \
    if (fabs(a) <= 1) { \
      agnCmplx_create((z), sun_acos((a)), 0); \
    } else { \
      lua_Number __cacos_a, __cacos_b; \
      tools_cacos((a), (b), &__cacos_a, &__cacos_b); \
      agnCmplx_create((z), __cacos_a, __cacos_b); \
    } \
  } else { \
    lua_Number t1, t2, t3, t4, t6, t12, t13, t14, t15, r, i; \
    t1 = a*a; \
    t2 = b*b; \
    t3 = 1.0 + t2; \
    t4 = sqrt(t1 + 2.0*a + t3); \
    t6 = sqrt(t1 - 2.0*a + t3); \
    t12 = tools_square((t4 + t6)*0.5); \
    t13 = t12 - 1.0; \
    if (fabs(t13) < 1.2e-019) t13 = 0.0; \
    t14 = sqrt(t13); \
    t15 = (t4 - t6)*0.5; \
    i = tools_csgn(-b, a)*sun_log((t4 + t6)*0.5 + t14); \
    if (t15 == 1.0) r = 0.0; \
    else if (t15 == -1.0) r = PI; \
    else r = sun_acos(t15); \
    agnCmplx_create((z), (r), (i)); \
  } \
}

/* 2.12.4 patch for OS/2: deliberately query b = 0 to prevent round-off errors and false undefined's, 2.1.2 */
/* DO _NOT_ USE SUN_pow if b == 0 !!! Use pow to prevent wrong results. 2.17.5 fix for b = 0 */
#define agnc_asec(z,a,b) { \
  if ((b) == 0) { \
    if ((a) == 0) { \
      agnCmplx_create((z), AGN_NAN, 0); \
    } else { \
      if (fabs(a) >= 1) { \
        agnCmplx_create((z), sun_acos((1/(a))), 0); \
      } else { \
        lua_Number __casec_a, __casec_b; \
        tools_casec((a), (b), &__casec_a, &__casec_b); \
        agnCmplx_create((z), __casec_a, __casec_b); \
      } \
    } \
  } else { \
    lua_Number t1, t2, t3, t4, t6, t12, t13, t14, t15, r, i, a0, a1, b1; \
    a0 = sun_pytha((a), (b)); \
    a1 = (a)/a0; \
    b1 = -(b)/a0; \
    t1 = a1*a1; \
    t2 = b1*b1; \
    t3 = 1.0 + t2; \
    t4 = sqrt(t1 + 2.0*a1 + t3); \
    t6 = sqrt(t1 - 2.0*a1 + t3); \
    t12 = tools_square((t4 + t6)*0.5); \
    t13 = t12 - 1.0; \
    if (fabs(t13) < 1.2e-019) t13 = 0.0; \
    t14 = sqrt(t13); \
    t15 = (t4 - t6)*0.5; \
    i = tools_csgn(-b1, a1)*sun_log((t4 + t6)*0.5 + t14); \
    if (t15 == 1.0) r = 0.0; \
    else if (t15 == -1.0) r = PI; \
    else r = sun_acos(t15); \
    agnCmplx_create((z), (r), (i)); \
  } \
}

/* calling tools_sincos is not faster */
#define agnc_sinh(z,a,b) { \
  lua_Number si, co, sih, coh; \
  sun_sinhcosh(a, &sih, &coh); \
  sun_sincos(b, &si, &co); \
  agnCmplx_create((z), sih*co, coh*si); \
}

#define agnc_cosh(z,a,b) { \
  lua_Number si, co, sih, coh; \
  sun_sinhcosh(a, &sih, &coh); \
  sun_sincos(b, &si, &co); \
  agnCmplx_create((z), coh*co, sih*si); \
}

#define agnc_tanh(z,a,b) { \
  lua_Number t1, t3, t5, t8, si, coh; \
  sun_sinhcosh(a, &t1, &coh); \
  sun_sincos(b, &si, &t5); \
  t3 = sun_pytha(t1, t5); \
  t8 = (t3 == 0.0) ? AGN_NAN : 1/t3; \
  agnCmplx_create((z), t1*coh*t8, si*t5*t8); \
}

/* patched 2.11.2 */
#define agnc_atan(z,a,b) { \
  lua_Number t3, t5, t6, t9, r, i; \
  t3 = (b) + 1.0; t5 = (a)*(a); t6 = t3*t3; t9 = tools_square((b) - 1.0); \
  if (a == 0 || a == -0) { \
    if ((b == 1 || b == -1)) { \
      r = AGN_NAN; \
      i = AGN_NAN; \
      goto lcatan2; \
    } \
    else if (b < -1) { \
      r = -PI*0.5; \
      goto lcatan1; \
    } \
  } \
  r = sun_atan2((a), 1.0 - (b))*0.5 - sun_atan2(-(a), t3)*0.5; \
lcatan1: \
  i = sun_log((t5 + t6)/(t5 + t9))*0.25; \
lcatan2: \
  agnCmplx_create((z), r, i + 0.0); \
}

#define agnc_exp(z,a,b) { \
  lua_Number t1, si, co; \
  sun_sincos(b, &si, &co); \
  t1 = sun_exp((a)); \
  agnCmplx_create((z), t1*co, t1*si); \
}

#define agnc_ln(z,a,b) { \
  lua_Number t1, t2; \
  t1 = (a)*(a); t2 = (b)*(b); \
  if (t1 == 0 && t2 == 0) { \
    agnCmplx_create((z), AGN_NAN, 0); } \
  else { \
    agnCmplx_create((z), 0.5*sun_log(t1 + t2), sun_atan2((b), (a))); } \
}

/* This is quite accurate especially with small real and imaginary parts. 2.35.2 */
#define agnc_sqrt(z,a,b) { \
  lua_Number re, im; \
  slm_csqrt(a, b, &re, &im); \
  agnCmplx_create((z), re, im); \
}

/* 2.9.0: compute magnitude, using hypotenuse for the error to be much smaller, 2.9.8 improvement */
#define agnc_abs(a,b)          ( sun_hypot((a), (b)) )
#define agnc_absdiff(a,b,c,d)  ( sun_hypot(((a)-(c)), ((b)-(d))) )  /* 2.10.2 */

#define agnc_entier(z,a,b) { \
  lua_Number aa, bb, fa, fb, t, X, Y; \
  fa = sun_floor((a)); \
  fb = sun_floor((b)); \
  aa = (a) - fa; \
  bb = (b) - fb; \
  t = aa + bb; \
  if ((t) < 1) { \
    X = 0; Y = 0; } \
  else if ((t) >= 1 && (aa >= bb)) { \
    X = 1; Y = 0; } \
  else { \
    X = 0; Y = 1; } \
  agnCmplx_create((z), fa + X, fb + Y); \
}

/* 0.33.2, tuned 2.1 RC 2 */
#define agnc_int(z,a,b) { \
  agnCmplx_create((z), sun_trunc((a)), sun_trunc((b))); \
}

#define agnc_frac(z,a,b) { \
  agnCmplx_create((z), (a) - sun_trunc((a)), (b) - sun_trunc((b))); \
}

#define agnc_signum(z,x,y) { \
  lua_Number t4, t5; \
  t4 = sun_hypot((x), (y)); \
  if (t4 == 0.0) { \
    agnCmplx_create((z), 0.0, 0.0); \
  } else { \
    t5 = 1/t4; \
    agnCmplx_create((z), x*t5, y*t5); \
  } \
}

/* 0.22.2: with n = 3, A and B need to be defined in order to compile correctly
   (a and b cannot be assigned values) */
#define agnc_ipow(z,a,b,n,p) { \
  if ((p) != 0) luaG_runerror(L, "exponent must be of type number"); \
  if ( (int)(n) <= AGN_MAXIPOWITER ) { \
    if ((n) == 0.0) { \
      if (a == 0 && b == 0) { \
        agnCmplx_create((z), AGN_NAN, AGN_NAN); \
      } else { \
        agnCmplx_create((z), 1.0, 0.0); \
      } \
    } else { \
      lua_Number (ra), (rb), (t), (A), (B); \
      int i; \
      A = (a); B = (b); \
      ra = (a); rb = (b); \
      for (i=1; i < (n); i++) { \
        t = (A); \
        (A) = fma((A), ra, -(B)*rb); \
        (B) = fma(t, rb, (B)*ra); \
      } \
      if (n < 0) { \
        lua_Number aa, bb; \
        tools_crecip(A, B, &aa, &bb); \
        A = aa; B = bb; \
      } \
      agnCmplx_create((z), (A), (B)); \
    } \
  } else { \
    lua_Number t1, t5, t6, t7, t8, t9; \
    t1 = sun_pytha((a), (b)); \
    t5 = sun_pow(t1, 0.5*(n), 0); \
    t6 = sun_atan2((b),(a)); \
    t7 = (n)*t6; \
    sun_sincos(t7, &t9, &t8); \
    agnCmplx_create((z), t5*t8, t5*t9); \
  }; \
}

/* LEAVE THIS HERE, OTHERWISE GCC WON'T COMPILE */
#include "cephes.h"  /* for complex lngamma */

#define agnc_lngamma(z,a,b) { \
  double re, im; \
  cephes_clgam((a), (b), &(re), &(im)); \
  agnCmplx_create((z), (re), (im)); \
}

#define agnc_antilog2(z,x,y) { \
  lua_Number t3, t4, si, co; \
  t3 = sun_exp(x*LN2); \
  t4 = y*LN2; \
  sun_sincos(t4, &si, &co); \
  agnCmplx_create((z), t3*co, t3*si); \
}

#define agnc_antilog10(z,x,y) { \
  lua_Number t3, t4, si, co; \
  t3 = sun_exp(x*LN10); \
  t4 = y*LN10; \
  sun_sincos(t4, &si, &co); \
  agnCmplx_create((z), t3*co, t3*si); \
}

#define agnc_sinc(z,a,b) { \
  lua_Number i, t3, t7, t8, t12, t13, si, co, sih, coh; \
  if ((a) == 0 && (b) == 0) { \
    agnCmplx_create((z), 1, 0); \
  } else { \
    sun_sincos(a, &si, &co); \
    sun_sinhcosh(b, &sih, &coh); \
    t3 = si*coh; \
    t7 = 1/sun_pytha((a), (b)); \
    t8 = a*t7; \
    t12 = co*sih; \
    t13 = b*t7; \
    i = fma(t12, t8, -t3*t13); \
    agnCmplx_create((z), fma(t3, t8, t12*t13), (i) + 0.0); \
  } \
}

#endif  /* of PROPCMPLX */


#if defined(LUA_CORE)
/* nothing any more listed here since 2.14.13 */
#endif


/* The following definitions are good for most cases here */

/* LUA_INTEGER_FMT is the format for writing integers */
#define LUA_INTEGER_FMT		"%" LUA_INTEGER_FRMLEN "d"

/* LUAI_UACINT is the result of a 'default argument promotion' */
#define LUAI_UACINT		LUA_INTEGER

/*
@@ LUA_UNSIGNED is the integral type used by lua_pushunsigned/lua_tounsigned.
** It must have at least 32 bits. Taken from Lua 5.2.4.
*/
#define LUA_UNSIGNED  unsigned LUA_INT32

/* lua_integer2str converts an integer to a string */
#define lua_integer2str(s,sz,n)  \
	l_sprintf((s), sz, LUA_INTEGER_FMT, (LUAI_UACINT)(n))

/* number of bits to consider in a number */
#if !defined(LUA_NBITS)
#define LUA_NBITS  LUAI_BITSINT
#endif

/* 2.9.0 */
#if !defined(LUA_NBITS64)
#define LUA_NBITS64  64
#endif


#if defined(__GNUC__) && !defined(LUA_NOBUILTIN)
#define luai_likely(x)    (__builtin_expect(((x) != 0), 1))
#define luai_unlikely(x)  (__builtin_expect(((x) != 0), 0))
#else
#define luai_likely(x)    (x)
#define luai_unlikely(x)  (x)
#endif

#define l_likely(x)   luai_likely(x)
#define l_unlikely(x) luai_unlikely(x)

#define AGN_FORNUMADJBAILOUT 12345 /* bail out of tools_roundf immediately */

/*
** Some tricks with doubles
*/

#if defined(LUA_NUMBER_DOUBLE) && !defined(LUA_ANSI)  /* { */
/*
** The next definitions activate some tricks to speed up the
** conversion from doubles to integer types, mainly to LUA_UNSIGNED.
**
@@ LUA_MSASMTRICK uses Microsoft assembler to avoid clashes with a
** DirectX idiosyncrasy.
**
@@ LUA_IEEE754TRICK uses a trick that should work on any machine
** using IEEE754 with a 32-bit integer type.
**
@@ LUA_IEEELL extends the trick to LUA_INTEGER; should only be
** defined when LUA_INTEGER is a 32-bit integer.
**
@@ LUA_IEEEENDIAN is the endianness of doubles in your machine
** (0 for little endian, 1 for big endian); if not defined, Lua will
** check it dynamically for LUA_IEEE754TRICK (but not for LUA_NANTRICK).
**
@@ LUA_NANTRICK controls the use of a trick to pack all types into
** a single double value, using NaN values to represent non-number
** values. The trick only works on 32-bit machines (ints and pointers
** are 32-bit values) with numbers represented as IEEE 754-2008 doubles
** with conventional endianness (12345678 or 87654321), in CPUs that do
** not produce signaling NaN values (all NaNs are quiet).
*/

/* Microsoft compiler on a Pentium (32 bit) ? */
#if defined(LUA_WIN) && defined(_MSC_VER) && defined(_M_IX86)  /* { */

#define LUA_MSASMTRICK
#define LUA_IEEEENDIAN    0
#define LUA_NANTRICK


/* pentium 32 bits? */
#elif defined(__i386__) || defined(__i386) || defined(__X86__) /* }{ */

#define LUA_IEEE754TRICK
#define LUA_IEEELL
#define LUA_IEEEENDIAN    0
#define LUA_NANTRICK

/* pentium 64 bits? */
#elif defined(__x86_64)            /* }{ */

#define LUA_IEEE754TRICK
#define LUA_IEEEENDIAN    0

#elif defined(__POWERPC__) || defined(__ppc__)      /* }{ */

#define LUA_IEEE754TRICK
#define LUA_IEEEENDIAN    1

#else                /* }{ */

/* assume IEEE754 and a 32-bit integer type */
#define LUA_IEEE754TRICK

#endif                /* } */

#endif              /* } */

/* }================================================================== */



/* }================================================================== */


/*
@@ LUAI_USER_ALIGNMENT_T is a type that requires maximum alignment.
** CHANGE it if your system requires alignments larger than double. (For
** instance, if your system supports long doubles and they must be
** aligned in 16-byte boundaries, then you should add long double in the
** union.) Probably you do not need to change this.
*/
#define LUAI_USER_ALIGNMENT_T   union { double u; void *s; long l; }

#define LUA_EXTRASPACE                (sizeof(void *))

/*
@@ LUAI_THROW/LUAI_TRY define how Lua does exception handling.
** CHANGE them if you prefer to use longjmp/setjmp even with C++
** or if want/don't to use _longjmp/_setjmp instead of regular
** longjmp/setjmp. By default, Lua handles errors with exceptions when
** compiling as C++ code, with _longjmp/_setjmp when asked to use them,
** and with longjmp/setjmp otherwise.
*/
#if defined(LUA_USE_ULONGJMP)  /* 2.1 RC 2 */

/* in Unix, try _longjmp/_setjmp (more efficient) */
#define LUAI_THROW(L,c)   _longjmp((c)->b, 1)
#define LUAI_TRY(L,c,a)   if (_setjmp((c)->b) == 0) { a }
#define luai_jmpbuf       jmp_buf

#else
/* default handling with long jumps */
#define LUAI_THROW(L,c)   longjmp((c)->b, 1)
#define LUAI_TRY(L,c,a)   if (setjmp((c)->b) == 0) { a }
#define luai_jmpbuf       jmp_buf

#endif


/*
@@ LUA_MAXCAPTURES is the maximum number of captures that a pattern
@* can do during pattern-matching.
** CHANGE it if you need more captures. This limit is arbitrary.
*/
#define LUA_MAXCAPTURES   32


/*
@@ lua_tmpnam is the function that the OS library uses to create a
@* temporary name.
@@ LUA_TMPNAMBUFSIZE is the maximum size of a name created by lua_tmpnam.
** CHANGE them if you have an alternative to tmpnam (which is considered
** insecure) or if you want the original tmpnam anyway.  By default, Lua
** uses tmpnam except when POSIX is available, where it uses mkstemp.
*/
#if defined(loslib_c) || defined(luaall_c)

#if defined(LUA_USE_MKSTEMP)
#include <unistd.h>
#define LUA_TMPNAMBUFSIZE   32
#define lua_tmpnam(b,e)   { \
   strcpy(b, "/tmp/lua_XXXXXX"); \
   e = mkstemp(b); \
   if (e != -1) close(e); \
   e = (e == -1); }
#else
#define LUA_TMPNAMBUFSIZE   1000
/* L_tmpnam */
#define lua_tmpnam(b,e)      { e = (tmpnam(b) == NULL); }
#endif

#endif


/*
@@ lua_popen spawns a new process connected to the current one through
@* the file streams.
** CHANGE it if you have a way to implement it in your system.
*/
#if defined(LUA_USE_POPEN)

#define lua_popen(L,c,m)   ((void)L, popen(c,m))
#define lua_pclose(L,file) ((void)L, (pclose(file)))  /* changed 2.22.0 */

#elif defined(LUA_WIN)

#define lua_popen(L,c,m)   ((void)L, _popen(c,m))
/* #define lua_pclose(L,file)   ((void)L, (_pclose(file) != -1)) */
#define lua_pclose(L,file) ((void)L, (_pclose(file)))  /* changed 2.22.0 */

#else

#define lua_popen(L,c,m)   ((void)((void)c, m),  \
      luaL_error(L, LUA_QL("popen") " not supported"), (FILE*)0)
#define lua_pclose(L,file) ((void)((void)L, file), 0)

#endif

/*
@@ LUA_DL_* define which dynamic-library system Lua should use.
** CHANGE here if Lua has problems choosing the appropriate
** dynamic-library system for your platform (either Windows' DLL, Mac's
** dyld, or Unix's dlopen). If your system is some kind of Unix, there
** is a good chance that it has dlopen, so LUA_DL_DLOPEN will work for
** it.  To use dlopen you also need to adapt the src/Makefile (probably
** adding -ldl to the linker options), so Lua does not select it
** automatically.  (When you change the makefile to add -ldl, you must
** also add -DLUA_USE_DLOPEN.)
** If you do not want any kind of dynamic library, undefine all these
** options.
** By default, _WIN32 gets LUA_DL_DLL and MAC OS X gets LUA_DL_DYLD.
*/
#if defined(LUA_USE_DLOPEN)
#define LUA_DL_DLOPEN
#endif

#if defined(LUA_WIN)
#define LUA_DL_DLL
#endif


/*
@@ LUAI_EXTRASPACE allows you to add user-specific data in a lua_State
@* (the data goes just *before* the lua_State pointer).
** CHANGE (define) this if you really need that. This value must be
** a multiple of the maximum alignment required for your machine.
*/
#define LUAI_EXTRASPACE      0


/*
@@ luai_userstate* allow user-specific actions on threads.
** CHANGE them if you defined LUAI_EXTRASPACE and need to do something
** extra when a thread is created/deleted/resumed/yielded.
*/

#define luai_userstateopen(L)        ((void)L)
#define luai_userstateclose(L)       ((void)L)
#define luai_userstatethread(L,L1)   ((void)L)
#define luai_userstatefree(L)        ((void)L)
#define luai_userstateresume(L,n)    ((void)L)
#define luai_userstateyield(L,n)     ((void)L)


/*
@@ LUA_INTFRMLEN is the length modifier for integer conversions
@* in 'string.format'.
@@ LUA_INTFRM_T is the integer type corresponding to the previous length
@* modifier.
** CHANGE them if your system supports long long or does not support long.
*/

#if defined(LUA_USELONGLONG)

#define LUA_INTFRMLEN     "ll"
#define LUA_INTFRM_T      long long

#else

#define LUA_INTFRMLEN     "l"
#define LUA_INTFRM_T      long

#endif


#if defined(SSE2)
#define SSE2COMP 1
#else
#define SSE2COMP 0
#endif


/* =================================================================== */

/*
** Local configuration. You can use this space to add your redefinitions
** without modifying the main part of the file.
*/

/* used in llex.c to denote end of short strings (`$'<string>)
   -> CHANGE THE MANUAL IF ALTERED */

#define AGN_SHORTSTRINGDELIM " ,~[]{}();:#'=?&%$§\"!^@<>|\r\n\t"

#ifdef __OS2__
#define AGN_DIRECTORY_SEPARATOR    '\\'
#define AGN_PATH_SEPARATOR         ';'
#elif (defined(__unix__) && !defined(__DJGPP__)) || defined(__APPLE__) || defined(__HAIKU__)
#define AGN_DIRECTORY_SEPARATOR    '/'
#define AGN_PATH_SEPARATOR         ':'
#elif defined(_WIN32)
#define AGN_DIRECTORY_SEPARATOR    '\\'
#define AGN_PATH_SEPARATOR         ';'
#else
#define AGN_DIRECTORY_SEPARATOR    '\0'
#define AGN_PATH_SEPARATOR         '\0'
#endif

#ifdef __cplusplus
}
#endif

#endif

