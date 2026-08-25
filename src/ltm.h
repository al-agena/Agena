/*
** $Id: ltm.h,v 2.6 2005/06/06 13:30:25 roberto Exp $
** Tag methods
** See Copyright Notice in agena.h
*/

#ifndef ltm_h
#define ltm_h


#include "lobject.h"


/* WARNING: if you change the order of this enumeration,
   grep "ORDER TM" and
   grep "ORDER OP" (lobjects.c/luaO_arith) */
typedef enum {
  TM_INDEX,
  TM_WRITEINDEX,
  TM_GC,
  TM_WEAK,
  TM_EQ,  /* last tag method with `fast' access */
  TM_ADD,
  TM_SUB,
  TM_MUL,
  TM_DIV,
  TM_INTDIV,     /* added 0.5.4 */
  TM_MOD,
  TM_POW,
  TM_IPOW,
  TM_UNM,
  TM_LT,
  TM_LE,
  TM_CONCAT,
  TM_CALL,
  TM_EEQ,        /* added 0.22.1 */
  TM_AEQ,        /* added 2.1.4 */
  TM_SIZE,       /* added 0.6.0 */
  TM_ABS,        /* added 0.7.0 */
  TM_MINUS,      /* added 2.3.0 */
  TM_IN,         /* added 0.11.0 */
  TM_NOTIN,      /* added 2.16.14 */
  TM_OFTYPE,     /* added 2.5.3 */
  TM_INTERSECT,  /* added 2.3.0 */
  TM_UNION,      /* added 2.3.0 */
  TM_EMPTY,      /* added 2.15.4 */
  TM_FILLED,     /* added 2.15.4 */
  TM_QSUMUP,      /* added 0.12.3, qsumup mm */
  TM_SUMUP,
  /* only 32 metamethods are supported by fasttm function -
     add non-fasttm metamethods from here;
     change the manual if you add new tags */
  TM_TAN,        /* added 0.7.0, moved beyond 32-threshold in 2.27.4 */
  TM_ARCTAN,     /* added 0.7.0, moved beyond 32-threshold in 2.27.4 **/
  TM_COS,        /* added 0.7.0 */
  TM_INT,        /* added 0.7.0 */
  TM_FRAC,       /* added 2.3.3 */
  TM_EVEN,       /* added 0.7.1 */
  TM_EXP,        /* added 0.7.0 */
  TM_LN,         /* added 0.7.0 */
  TM_SIGN,       /* added 0.7.0 */
  TM_SINH,       /* added 0.23.0 */
  TM_COSH,       /* added 0.23.0 */
  TM_TANH,       /* added 0.23.0 */
  TM_ARCSIN,     /* added 0.27.0 */
  TM_ARCCOS,     /* added 0.27.0 */
  TM_RECIP,      /* added 2.1 RC 1 */
  TM_NAN,        /* added 2.1.2 */
  TM_COSXX,      /* added 2.1.2 */
  TM_BEA,        /* added 2.1.2 */
  TM_FLIP,       /* added 2.1.2 */
  TM_CONJUGATE,  /* added 2.1.2 */
  TM_ARCSEC,     /* added 2.1.2 */
  TM_LNGAMMA,    /* added 0.9.0 */
  TM_FINITE,     /* added 0.9.2 */
  TM_SQRT,       /* added 0.7.0 */
  TM_ENTIER,     /* added 0.7.0 */
  TM_ANTILOG2,   /* added 2.4.1 */
  TM_ANTILOG10,  /* added 2.4.1 */
  TM_SIGNUM,     /* added 2.4.1 */
  TM_SIN,        /* added 0.7.0 */
  TM_SINC,       /* added 2.4.2 */
  TM_QMDEV,      /* added 2.4.2 */
  TM_CIS,        /* added 2.5.1 */
  TM_ODD,        /* added 2.9.0 */
  TM_ABSDIFF,    /* added 2.9.8 */
  TM_INFINITE,   /* added 2.10.0 */
  TM_SQUARE,     /* added 2.12.6 */
  TM_CUBE,       /* added 2.13.0 */
  TM_REAL,       /* added 2.14.2 */
  TM_IMAG,       /* added 2.14.2 */
  TM_ZERO,       /* added 2.16.0 */
  TM_NONZERO,    /* added 2.16.0 */
  TM_INVSQRT,    /* added 2.16.13 */
  TM_SQUAREADD,  /* added 2.16.13 */
  TM_INTEGRAL,   /* added 2.34.7 */
  TM_FRACTIONAL, /* added 2.34.7 */
  TM_LOG,        /* added 2.34.7 */
  TM_MULUP,      /* added 3.5.6 */
  TM_BAND,       /* added 4.8.0 */
  TM_BOR,        /* added 4.8.0 */
  TM_BXOR,       /* added 4.8.0 */
  TM_BNOT,       /* added 4.8.0 */
  TM_BSHL,       /* added 4.8.0 */
  TM_BSHR,       /* added 4.8.0 */
  TM_BROTL,      /* added 6.1.4 */
  TM_BROTR,      /* added 6.1.4 */
  TM_LEFT,       /* added 5.0.0 */
  TM_RIGHT,      /* added 5.0.0 */
  TM_ITER,       /* added 7.1.0 */
  TM_N           /* number of elements in the TMS enum */
} TMS;


#define gfasttm(g,et,e) ((et) == NULL ? NULL : \
  ((et)->flags & (1u<<(e))) ? NULL : luaT_gettm(et, e, (g)->tmname[e]))

#define fasttm(l,et,e)   gfasttm(G(l), et, e)

LUAI_DATA const char *const luaT_typenames[];


LUAI_FUNC const TValue *luaT_gettm (Table *events, TMS event, TString *ename);
LUAI_FUNC const TValue *luaT_gettmbyobj (lua_State *L, const TValue *o,
                                                       TMS event);
LUAI_FUNC void luaT_init (lua_State *L);

#endif

