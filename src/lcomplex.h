/*
** $Id: lcomplex.h v 0.1, based on ltable.h v2.10 2006/01/10 13:13:06 roberto Exp $
** Agena Complex Numbers
** See Copyright Notice in agena.h
*/

#ifndef lcomplex_h
#define lcomplex_h

#include "lobject.h"
#include "agnhlps.h"

#ifndef PROPCMPLX

#include "agnconf.h"  /* for definition of I */
#include <complex.h>
#define complexreal(t)     (creal((t->value.c)))
#define compleximag(t)     (cimag((t->value.c)))

#else

#define complexreal(t)     (t->value.c[0])
#define compleximag(t)     (t->value.c[1])

#endif

#define agnCmplx_create(z,re,im) { \
  z[0] = (re); \
  z[1] = (im); \
}

#define agnCmplx_add(z,a,b,c,d)    agnCmplx_create((z), ((a)+(c)), ((b)+(d)))
#define agnCmplx_sub(z,a,b,c,d)    agnCmplx_create((z), ((a)-(c)), ((b)-(d)))
#define agnCmplx_mul(z,a,b,c,d)    agnCmplx_create((z), ((a)*(c)-(b)*(d)), ((a)*(d)+(b)*(c)))
#define agnCmplx_div(z,a,b,c,d) \
  do { \
    lua_Number _c = (c); lua_Number _d = (d); \
    lua_Number _e = (_c*_c + _d*_d); \
    agnCmplx_create((z), ((a)*_c + (b)*_d)/_e, ((b)*_c - (a)*_d)/_e); \
  } while (0)

#define agnCmplx_recip(z,c,d) \
  do { \
    lua_Number _c = (c); \
    lua_Number _d = (d); \
    lua_Number _e = (_c*_c + _d*_d); \
    agnCmplx_create((z), \
      (_c / _e), \
      (_e == 0) ? AGN_NAN : ((_d != 0) ? (-(_d) / _e) : 0) \
    ); \
  } while (0)

#ifdef PROPCMPLX
#define agnCmplx_sqrt(z,a,b) { \
  lua_Number __re, __im; \
  slm_csqrt(a, b, &__re, &__im); \
  agnCmplx_create((z), __re, __im); \
}

#define agnCmplx_log(z,a,b) { \
  lua_Number __re, __im; \
  tools_clog(a, b, &__re, &__im); \
  agnCmplx_create((z), __re, __im); \
}
#endif

/*
** C++ Compatibility Bridge
** This protects the function prototypes from name mangling
*/
#ifdef __cplusplus
extern "C" {
#endif

#ifdef PROPCMPLX
/* Underlying C implementations for complex math */
void slm_csqrt (lua_Number a, lua_Number b, lua_Number *re, lua_Number *im);
void tools_clog (lua_Number a, lua_Number b, lua_Number *re, lua_Number *im);
#endif

#ifdef __cplusplus
}
#endif

#endif

