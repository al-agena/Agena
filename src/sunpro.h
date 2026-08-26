/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */

#ifndef sunpro_h
#define sunpro_h

#include "agncfg.h"
#include "agnconf.h"

#ifdef __cplusplus
extern "C" {
#endif

#if BYTE_ORDER == BIG_ENDIAN
typedef union {
  double value;
  struct {
    uint32_t msw;
    uint32_t lsw;
  } parts;
} ieee_double_shape_type;
#else
typedef union {
  double value;
  struct {
    uint32_t lsw;
    uint32_t msw;
  } parts;
} ieee_double_shape_type;
#endif


/* Get two 32 bit ints from a double.  */

#define EXTRACT_WORDS(ix0,ix1,d) \
do { \
  ieee_double_shape_type ew_u; \
  ew_u.value = (d); \
  (ix0) = ew_u.parts.msw; \
  (ix1) = ew_u.parts.lsw; \
} while (0)

/* Get the more significant 32 bit int from a double.  */

#define GET_HIGH_WORD(i,d) \
do { \
  ieee_double_shape_type gh_u; \
  gh_u.value = (d); \
  (i) = gh_u.parts.msw; \
} while (0)

/* Get the less significant 32 bit int from a double.  */

#define GET_LOW_WORD(i,d) \
do { \
  ieee_double_shape_type gl_u; \
  gl_u.value = (d); \
  (i) = gl_u.parts.lsw; \
} while (0)

/* Set a double from two 32 bit ints.  */

#define INSERT_WORDS(d,ix0,ix1) \
do { \
  ieee_double_shape_type iw_u; \
  iw_u.parts.msw = (ix0); \
  iw_u.parts.lsw = (ix1); \
  (d) = iw_u.value; \
} while (0)

/* Set the more significant 32 bits of a double from an int.  */

#define SET_HIGH_WORD(d,v) \
do { \
  ieee_double_shape_type sh_u; \
  sh_u.value = (d); \
  sh_u.parts.msw = (v); \
  (d) = sh_u.value; \
} while (0)

/* Set the less significant 32 bits of a double from an int.  */

#define SET_LOW_WORD(d,v) \
do { \
  ieee_double_shape_type sl_u; \
  sl_u.value = (d); \
  sl_u.parts.lsw = (v); \
  (d) = sl_u.value; \
} while (0)

/* Big and Little Endian shortcuts for GET_HIGH, etc. */
#if BYTE_ORDER == BIG_ENDIAN
#define __HI(x) *(int*)&x
#define __LO(x) *(1+(int*)&x)
#define __HIp(x) *(int*)x
#define __LOp(x) *(1+(int*)x)
#else
#define __HI(x) *(1+(int*)&x)
#define __LO(x) *(int*)&x
#define __HIp(x) *(1+(int*)x)
#define __LOp(x) *(int*)x
#endif

/* A union which permits us to convert between a float and a 32 bit int. */

typedef union {
  float value;
  uint32_t word;
} ieee_float_shape_type;


typedef union {  /* 2.14.6 */
  float f;
  int32_t i;
} ieee_float_shape_signedint;


/* Same for doubles. */

typedef union {
  double f;
  uint64_t i;
  /* struct {
    uint64_t mantissa : 52;
    uint64_t exponent : 11;
    uint64_t sign     : 1;
  } parts; */
} double_cast;


#if BYTE_ORDER == BIG_ENDIAN
typedef union {
  double v;
  struct {
    uint64_t sign          :  1;
    uint64_t exponent      : 11;  /* biased exponent */
    uint64_t mantissa_high : 20;
    uint64_t mantissa_low  : 32;
  } c;
} double_ieee754;
#else
typedef union {
  double v;
  struct {
    uint64_t mantissa_low  : 32;
    uint64_t mantissa_high : 20;
    uint64_t exponent      : 11;  /* biased exponent */
    uint64_t sign          :  1;
  } c;
} double_ieee754;
#endif


/*
#if BYTE_ORDER == BIG_ENDIAN
#define U64_MAKE(lo,hi)     (((uint64_t)(lo) << 32) | (uint32_t)(hi))
#define U64_HI(x)           ((uint32_t)(x))
#define U64_LO(x)           ((uint32_t)((x) >> 32))
#else
#define U64_MAKE(lo,hi)     (((uint64_t)(hi) << 32) | (uint32_t)(lo))
#define U64_LO(x)           ((uint32_t)(x))
#define U64_HI(x)           ((uint32_t)((x) >> 32))
#endif
*/

/* Get a 32 bit int from a float.  */

#define GET_FLOAT_WORD(i,d) \
do { \
  ieee_float_shape_type gf_u; \
  gf_u.value = (d); \
  (i) = gf_u.word; \
} while (0)

/* Set a float from a 32 bit int.  */

#define SET_FLOAT_WORD(d,i) \
do { \
  ieee_float_shape_type sf_u; \
  sf_u.word = (i); \
  (d) = sf_u.value; \
} while (0)


/* taken from musl-1.1.22/src/internal/math.h */
#define FORCE_EVAL(x) do { \
  if (sizeof(x) == sizeof(float)) { \
    volatile float __x; \
    __x = (x); \
    (void)__x; \
  } else if (sizeof(x) == sizeof(double)) { \
    volatile double __x; \
    __x = (x); \
    (void)__x; \
  } else { \
    volatile long double __x; \
    __x = (x); \
    (void)__x; \
  } \
} while(0)


#if BYTE_ORDER == BIG_ENDIAN
typedef union {
  long double value;
  struct {
    int sign_exponent:16;
    unsigned int empty:16;
    uint32_t msw;
    uint32_t lsw;
  } parts;
} ieee_long_double_shape_type;
#else
typedef union {
  long double value;
  struct {
    uint32_t lsw;
    uint32_t msw;
    int sign_exponent:16;
    unsigned int empty:16;
  } parts;
} ieee_long_double_shape_type;
#endif

/* Get int from the exponent of a long double.  */

#define GET_LDOUBLE_EXP(exp,d) \
do { \
  ieee_long_double_shape_type ge_u; \
  ge_u.value = (d); \
  (exp) = ge_u.parts.sign_exponent; \
} while (0)

/* Set exponent of a long double from an int.  */

#define SET_LDOUBLE_EXP(d,exp) \
do { \
  ieee_long_double_shape_type se_u; \
  se_u.value = (d); \
  se_u.parts.sign_exponent = (exp); \
  (d) = se_u.value; \
} while (0)


/*-
 * Copyright (c) 2002, 2003 David Schultz <das@FreeBSD.ORG>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 * Taken from source files: lib\libc\ia64\_fpmath.h & lib\libc\gen\fpclassify.c
 */

union IEEEl2bits {
  long double	e;
  struct {
#if BYTE_ORDER != BIG_ENDIAN
    unsigned int manl      : 32;
    unsigned int manh      : 32;
    unsigned int exp       : 15;
    unsigned int sign      : 1;
    /* unsigned long junk  : 48; */
#else /* _BIG_ENDIAN */
    /* unsigned long junk  : 48; */
    unsigned int sign      : 1;
    unsigned int exp       : 15;
    unsigned int manh      : 32;
    unsigned int manl      : 32;
#endif
  } bits;
  struct {
#if BYTE_ORDER != BIG_ENDIAN
    unsigned long long man : 64;
    unsigned int expsign   : 16;
    unsigned int junk      : 16;
#else
    unsigned int junk      : 16;
    unsigned int expsign   : 16;
    unsigned long long man : 64;
#endif
  } xbits;

};

/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 *
 * Taken from: openlibm-0.4.1/src/math_private.h & openlibm-0.4.1/src/e_hypotl.c
 */

/* Get expsign as a 16 bit int from a long double.  */

#define	GET_LDBL_EXPSIGN(i,d) \
do { \
  union IEEEl2bits ge_u; \
  ge_u.e = (d); \
  (i) = ge_u.xbits.expsign; \
} while (0)

/* Set expsign of a long double from a 16 bit int.  */

#define	SET_LDBL_EXPSIGN(d,v) \
do { \
  union IEEEl2bits se_u; \
  se_u.e = (d); \
  se_u.xbits.expsign = (v); \
  (d) = se_u.e; \
} while (0)

#define	GET_LDBL_MAN(h, l, v) do { \
  union IEEEl2bits uv; \
  uv.e = v; \
  h = uv.bits.manh; \
  l = uv.bits.manl; \
} while (0)

#undef GET_HIGH_WORDL
#define	GET_HIGH_WORDL(i, v)	GET_LDBL_EXPSIGN(i, v)
#undef SET_HIGH_WORDL
#define	SET_HIGH_WORDL(v, i)	SET_LDBL_EXPSIGN(v, i)


#define	LDBL_MANH_SIZE	32
#define	LDBL_MANL_SIZE	32

#define	LDBL_TO_ARRAY32(u, a) do { \
	(a)[0] = (uint32_t)(u).bits.manl; \
	(a)[1] = (uint32_t)(u).bits.manh; \
} while (0)

#if BYTE_ORDER != BIG_ENDIAN
#define	LDBL_NBIT	0x80000000
#else  /* Big Endian */
#define	LDBL_NBIT	0x80
#endif

#define	mask_nbit_l(u)	((u).bits.manh &= ~LDBL_NBIT)

/* Taken from .../musl/src/internal/libm.h */

#define SIZEOFLDBL (sizeof(long double))

#if LDBL_MANT_DIG == 53 && LDBL_MAX_EXP == 1024
union ldshape {
	long double f;  /* WARNING: .d and .f do not update each other */
  agn_Complex z;
  uint64_t ui64;
  int64_t i64;
	struct {
		uint64_t m;
		uint16_t se;
		uint16_t junk;
	} i;
	unsigned char c[SIZEOFLDBL];
	double d;  /* WARNING: .d and .f do not update each other */
};
#elif (LDBL_MANT_DIG == 64 && LDBL_MAX_EXP == 16384 && BYTE_ORDER != BIG_ENDIAN)
union ldshape {
	long double f;  /* WARNING: .d and .f do not update each other */
  agn_Complex z;
  uint64_t ui64;
  int64_t i64;
	struct {
		uint64_t m;
		uint16_t se;
		uint16_t junk;
	} i;
	unsigned char c[SIZEOFLDBL];
	double d;  /* WARNING: .d and .f do not update each other */
};
#elif LDBL_MANT_DIG == 113 && LDBL_MAX_EXP == 16384 && BYTE_ORDER != BIG_ENDIAN
union ldshape {
	long double f;  /* WARNING: .d and .f do not update each other */
  agn_Complex z;
  uint64_t ui64;
  int64_t i64;
	struct {
		uint64_t lo;
		uint32_t mid;
		uint16_t top;
		uint16_t se;
	} i;
	struct {
		uint64_t lo;
		uint64_t hi;
	} i2;
	unsigned char c[SIZEOFLDBL];
	double d;  /* WARNING: .d and .f do not update each other */
};
#elif LDBL_MANT_DIG == 113 && LDBL_MAX_EXP == 16384 && BYTE_ORDER == BIG_ENDIAN
union ldshape {
	long double f;  /* WARNING: .d and .f do not update each other */
  agn_Complex z;
  uint64_t ui64;
  int64_t i64;
	struct {
		uint16_t se;
		uint16_t top;
		uint32_t mid;
		uint64_t lo;
	} i;
	struct {
		uint64_t hi;
		uint64_t lo;
	} i2;
	unsigned char c[SIZEOFLDBL];
	double d;  /* WARNING: .d and .f do not update each other */
};
#elif defined(__ARMCPU)
union ldshape {
	double f;  /* WARNING: .d and .f do not update each other */
  agn_Complex z;
  uint64_t ui64;
  int64_t i64;
	struct {
		uint64_t m;
		uint16_t se;
		uint16_t junk;
	} i;
	unsigned char c[SIZEOFLDBL];
	double d;  /* WARNING: .d and .f do not update each other */
};
#else
#error Unsupported long double representation
#endif

#if LDBL_MANT_DIG == 64
#define CLOSETO1(u) (u.i.m>>56 >= 0xf7)
#define CLEARBOTTOM(u) (u.i.m &= -1ULL << 32)
#elif LDBL_MANT_DIG == 113
#define CLOSETO1(u) (u.i.top >= 0xee00)
#define CLEARBOTTOM(u) (u.i.lo = 0)
#endif

#ifdef __cplusplus
}
#endif

#endif
/* _SUNPRO_H_ */

