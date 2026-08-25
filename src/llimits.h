/*
** $Id: llimits.h,v 1.69 2005/12/27 17:12:00 roberto Exp $
** Limits, basic types, and some other `installation-dependent' definitions
** See Copyright Notice in agena.h
*/

#ifndef llimits_h
#define llimits_h

#include <limits.h>
#include <stddef.h>

#include "agena.h"

typedef LUAI_UINT32 lu_int32;
typedef LUAI_UMEM lu_mem;
typedef LUAI_MEM l_mem;

/* chars used as small naturals (so that `char' is reserved for characters) */
typedef unsigned char lu_byte;


#define MAX_SIZET  ((size_t)(~(size_t)0)-2)

/* maximum size visible for Lua (must be representable in a lua_Integer) */
#define MAX_SIZE  (sizeof(size_t) < sizeof(lua_Integer) ? MAX_SIZET \
                          : (size_t)(LUA_MAXINTEGER))

#define MAX_LUMEM  ((lu_mem)(~(lu_mem)0)-2)

#define MAX_INT (INT_MAX - 2)  /* maximum value of an int (-2 for safety) */

/*
** conversion of pointer to integer
** this is for hashing only; there is no problem if the integer
** cannot hold the whole pointer value
*/
#define IntPoint(p)  ((unsigned int)(lu_mem)(p))

/* type to ensure maximum alignment */
typedef LUAI_USER_ALIGNMENT_T L_Umaxalign;

/* result of a `usual argument conversion' over lua_Number */
typedef LUAI_UACNUMBER l_uacNumber;

/* internal assertions for in-house debugging */
#ifdef lua_assert

#define check_exp(c,e)   (lua_assert(c), (e))
#define api_check(l,e)   lua_assert(e)

#else

#define lua_assert(c)    ((void)0)
#define check_exp(c,e)   (e)
#define api_check        luai_apicheck

#endif

#ifndef UNUSED
#define UNUSED(x)  ((void)(x))  /* to avoid warnings */
#endif

#ifndef cast
#define cast(t, exp)  ((t)(exp))
#endif

#define cast_byte(i)    cast(lu_byte, (i))
#define cast_num(i)     cast(lua_Number, (i))
#define cast_int(i)     cast(int, (i))
#define cast_char(i)    cast(char, (i))
#define cast_charp(i)   cast(char *, (i))
#define cast_void(i)    cast(void, (i))
#define cast_voidp(i)   cast(void *, (i))

/* cast a signed lua_Integer to lua_Unsigned, 2.21.1, taken from Lua 5.4.0 RC 4 */
#if !defined(l_castS2U)
/* #define l_castS2U(i)  ((lua_Unsigned)(i)) */
#define l_castS2U(i)  ((ptrdiff_t)(i))  /* 2.26.0 */
#endif

/*
** cast a lua_Unsigned to a signed lua_Integer; this cast is
** not strict ISO C, but two-complement architectures should
** work fine. 2.21.1, taken from Lua 5.4.0 RC 4
*/
#if !defined(l_castU2S)
#define l_castU2S(i)  ((lua_Integer)(i))
#endif

#ifndef LUA_MAXINTEGER
#define LUA_MAXINTEGER    INT_MAX
#endif
#ifndef LUA_MININTEGER
#define LUA_MININTEGER    INT_MIN
#endif

/*
** type for virtual-machine instructions
** must be an unsigned with (at least) 4 bytes (see details in lopcodes.h)
*/
typedef lu_int32 Instruction;



/* maximum stack for a Lua function */
#define MAXSTACK  250

/* minimum size for the string table (must be power of 2) */
#ifndef MINSTRTABSIZE
#define MINSTRTABSIZE  32
#endif

/* minimum size for string buffer */
#ifndef LUA_MINBUFFER
#define LUA_MINBUFFER  32
#endif

#ifndef lua_lock
#define lua_lock(L)     ((void) 0)
#define lua_unlock(L)   ((void) 0)
#endif

#ifndef luai_threadyield
#define luai_threadyield(L)     {lua_unlock(L); lua_lock(L);}
#endif

/***************************************************************************************************/

/*
** lua_number2int is a macro to convert lua_Number to int.
** lua_number2integer is a macro to convert lua_Number to lua_Integer.
** lua_number2unsigned is a macro to convert a lua_Number to a lua_Unsigned.
** lua_unsigned2number is a macro to convert a lua_Unsigned to a lua_Number.
** luai_hashnum is a macro to hash a lua_Number value into an integer.
** The hash must be deterministic and give reasonable values for
** both small and large values (outside the range of integers).
*/

#if defined(MS_ASMTRICK) || defined(LUA_MSASMTRICK)  /* { */
/* trick with Microsoft assembler for X86 */

#define lua_number2int(i,n)  __asm {__asm fld n   __asm fistp i}
#define lua_number2integer(i,n)    lua_number2int(i, n)
#define lua_number2unsigned(i,n)  \
  {__int64 l; __asm {__asm fld n   __asm fistp l} i = (unsigned int)l;}

#elif defined(LUA_IEEE754TRICK)    /* }{ */
/* the next trick should work on any machine using IEEE754 with
   a 32-bit int type */

union luai_Cast { double l_d; LUA_INT32 l_p[2]; };

#if !defined(LUA_IEEEENDIAN)  /* { */
#define LUAI_EXTRAIEEE  \
  static const union luai_Cast ieeeendian = {-(33.0 + 6755399441055744.0)};
#define LUA_IEEEENDIANLOC  (ieeeendian.l_p[1] == 33)
#else
#define LUA_IEEEENDIANLOC  LUA_IEEEENDIAN
#define LUAI_EXTRAIEEE    /* empty */
#endif        /* } */

#define lua_number2int32(i,n,t) \
  { LUAI_EXTRAIEEE \
    volatile union luai_Cast u; u.l_d = (n) + 6755399441055744.0; \
    (i) = (t)u.l_p[LUA_IEEEENDIANLOC]; }

#define luai_hashnum(i,n)  \
  { volatile union luai_Cast u; u.l_d = (n) + 1.0;  /* avoid -0 */ \
    (i) = u.l_p[0]; (i) += u.l_p[1]; }  /* add double bits for his hash */

#define lua_number2int(i,n)       lua_number2int32(i, n, int)
#define lua_number2unsigned(i,n)  lua_number2int32(i, n, lua_Unsigned)

/* the trick can be expanded to lua_Integer when it is a 32-bit value */
#if defined(LUA_IEEELL)
#define lua_number2integer(i,n)   lua_number2int32(i, n, lua_Integer)
#endif

#endif        /* } */

/* the following definitions always work, but may be slow */

#if !defined(lua_number2int)
#define lua_number2int(i,n)  ((i)=(int)(n))
#endif

#if !defined(lua_number2integer)
#define lua_number2integer(i,n)  ((i)=(lua_Integer)(n))
#endif

#if !defined(lua_number2unsigned)  /* { */
/* the following definition assures proper modulo behavior */
#if defined(LUA_NUMBER_DOUBLE) || defined(LUA_NUMBER_FLOAT)
#if !(defined(__DJGPP__) || defined(__OS2__) || defined(LUA_ANSI))
#include <math.h>
#endif
#define SUPUNSIGNED  ((lua_Number)(~(lua_Unsigned)0) + 1)
#define lua_number2unsigned(i,n)  \
  ((i)=(lua_Unsigned)((n) - sun_floor((n)/SUPUNSIGNED)*SUPUNSIGNED))
#else
#define lua_number2unsigned(i,n)  ((i)=(lua_Unsigned)(n))
#endif
#endif        /* } */

#if !defined(lua_unsigned2number)
/* on several machines, coercion from unsigned to double is slow, so it may be worth to avoid */
#define lua_unsigned2number(u)  \
    (((u) <= (lua_Unsigned)INT_MAX) ? (lua_Number)(int)(u) : (lua_Number)(u))
#endif

/*
** macro to control inclusion of some hard tests on stack reallocation
*/
#ifndef HARDSTACKTESTS
#define condhardstacktests(x)  ((void)0)
#else
#define condhardstacktests(x)  x
#endif

#endif


