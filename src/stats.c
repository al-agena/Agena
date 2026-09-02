/*
** $Id: lstatlib.c, v 1.0.1 by Alexander Walz - initiated June 19, 2007
** Statistics library
** See Copyright Notice in agena.h
*/

#include <stdlib.h>
#include <math.h>
#include <string.h>

/* the following package ini declarations must be included after `#include <` and before `include #` ! */

#define stats_c
#define LUA_LIB

#include "agena.h"
#include "agenalib.h"
#include "agncmpt.h"  /* for TRUNC */
#include "agnhlps.h"  /* for tools_intpow, isnan, PI2, quicksort, k-th smallest */
#include "agnxlib.h"
#include "cephes.h"
#include "dblvec.h"
#include "jbmath.h"
#include "long.h"
#include "numarray.h"

/* Computation of the arithmetic mean using Kahan-Babuška summation, see:
   `A generalized Kahan-Babuška Summation-Algorithm` by Andreas Klein, April 21, 2005.

   rc must have been initialised at the time of call !
   rc = 1: there are all numbers in the distributions, 0 = at least one non-number found, e.g. a complex number. */

static lua_Number KahanBabuskaMean (lua_State *L, int idx, lua_Number (*f)(lua_State *, int, int, int *), size_t ii, int *rc) {  /* 2.4.3/3.18.8 */
  size_t i;
  volatile lua_Number s, cs, ccs, t, c, cc, x;
  s = cs = ccs = 0.0;
  for (i=0; rc && i < ii; i++) {
    x = (*f)(L, idx, i + 1, rc)/ii;  /* for accuracy, we deliberately divide instead of multiply by a quotient */
    t = s + x;
    c = (fabs(s) >= fabs(x)) ? (s - t) + x : (x - t) + s;
    s = t;
    t = cs + c;
    cc = (fabs(cs) >= fabs(c)) ? (cs - t) + c : (c - t) + cs;
    cs = t;
    ccs += cc;
  }
  return s + cs + ccs;  /* no rounding in between */
}

/* The function is at stack index idx, the structure at idx + 1. */
static lua_Number KahanBabuskaMeanFn (lua_State *L, int idx, lua_Number (*f)(lua_State *, int, int, int *), size_t ii, int *rc) {  /* 4.6.1 */
  size_t i;
  volatile lua_Number s, cs, ccs, t, c, cc, x;
  s = cs = ccs = 0.0;
  for (i=0; rc && i < ii; i++) {
    x = (*f)(L, idx + 1, i + 1, rc);  /* for accuracy, we deliberately divide instead of multiply by a quotient */
    luaL_checkstack(L, 2, "not enough stack space");
    lua_pushvalue(L, idx);
    lua_pushnumber(L, x);
    lua_call(L, 1, 1);
    if (lua_istrue(L, -1)) {  /* 5.0.1 fix */
      x /= (lua_Number)ii;
      t = s + x;
      c = (fabs(s) >= fabs(x)) ? (s - t) + x : (x - t) + s;
      s = t;
      t = cs + c;
      cc = (fabs(cs) >= fabs(c)) ? (cs - t) + c : (c - t) + cs;
      cs = t;
      ccs += cc;
    }
    agn_poptop(L);
  }
  return s + cs + ccs;  /* no rounding in between */
}

static lua_Number KahanBabuskaMeanNumArray (lua_Number *a, size_t ii) {  /* 2.4.3 */
  size_t i;
  volatile lua_Number s, cs, ccs, t, c, cc, x;
  s = cs = ccs = 0.0;
  for (i=0; i < ii; i++) {
    x = a[i]/ii;  /* for accuracy, we deliberately divide instead of multiply by a quotient */
    t = s + x;
    c = (fabs(s) >= fabs(x)) ? (s - t) + x : (x - t) + s;
    s = t;
    t = cs + c;
    cc = (fabs(cs) >= fabs(c)) ? (cs - t) + c : (c - t) + cs;
    cs = t;
    ccs += cc;
  }
  return s + cs + ccs;  /* 2.4.4, no rounding in between */
}

/* The function is at stack index 1. */
static lua_Number KahanBabuskaMeanNumArrayFn (lua_State *L, lua_Number *a, size_t ii) {  /* 4.6.1 */
  size_t i;
  volatile lua_Number s, cs, ccs, t, c, cc, x;
  s = cs = ccs = 0.0;
  for (i=0; i < ii; i++) {
    x = a[i];  /* for accuracy, we deliberately divide instead of multiply by a quotient */
    luaL_checkstack(L, 2, "not enough stack space");
    lua_pushvalue(L, 1);
    lua_pushnumber(L, x);
    lua_call(L, 1, 1);
    if (lua_istrue(L, -1)) {  /* 5.0.1 fix */
      x /= (lua_Number)ii;
      t = s + x;
      c = (fabs(s) >= fabs(x)) ? (s - t) + x : (x - t) + s;
      s = t;
      t = cs + c;
      cc = (fabs(cs) >= fabs(c)) ? (cs - t) + c : (c - t) + cs;
      cs = t;
      ccs += cc;
    }
    agn_poptop(L);
  }
  return s + cs + ccs;  /* 2.4.4, no rounding in between */
}

/* Harmonic mean, based on KahanBabuskaMean; 2.12.0 RC 3/3.18.8; rewritten 4.5.0 */
static int KahanBabuskaHMean (lua_State *L, void (*f)(lua_State *, int, int, lua_Number *, int *rc), size_t ii) {
  int rc, flag;
  size_t i;
  lua_Number x, z[2];
  volatile lua_Number s[2], s0[2], cs[2], ccs[2], t[2], c[2], cc[2], d;
  z[0] = s[0] = t[0] = c[0] = cc[0] = cs[0] = ccs[0] = 0.0;
  z[1] = s[1] = t[1] = c[1] = cc[1] = cs[1] = ccs[1] = 0.0;
  flag = LUA_TNIL;
  for (i=0; i < ii; i++) {
    (*f)(L, 1, i + 1, z, &rc);
    if (rc == LUA_TNUMBER) {
      if (flag == LUA_TNIL) flag = LUA_TNUMBER;
      x = (z[0] == 0.0) ? AGN_NAN : 1/z[0];  /* 3.18.8 security fix */
      t[0] = s[0] + x;
      c[0] = (fabs(s[0]) >= fabs(x)) ? (s[0] - t[0]) + x : (x - t[0]) + s[0];
      s[0] = t[0];
      t[0] = cs[0] + c[0];
      cc[0] = (fabs(cs[0]) >= fabs(c[0])) ? (cs[0] - t[0]) + c[0] : (c[0] - t[0]) + cs[0];
      cs[0] = t[0];
      ccs[0] = ccs[0] + cc[0];
    } else if (rc == LUA_TCOMPLEX) {
      if (flag == LUA_TNIL || flag == LUA_TNUMBER) flag = LUA_TCOMPLEX;
      d = z[0]*z[0] + z[1]*z[1];
      x = (d == 0.0) ? AGN_NAN : z[0]/d;
      t[0] = s[0] + x;
      c[0] = (fabs(s[0]) >= fabs(x)) ? (s[0] - t[0]) + x : (x - t[0]) + s[0];
      s[0] = t[0];
      t[0] = cs[0] + c[0];
      cc[0] = (fabs(cs[0]) >= fabs(c[0])) ? (cs[0] - t[0]) + c[0] : (c[0] - t[0]) + cs[0];
      cs[0] = t[0];
      ccs[0] = ccs[0] + cc[0];
      x = (d == 0.0) ? AGN_NAN : z[1]/d;
      t[1] = s[1] - x;
      c[1] = (fabs(s[1]) >= fabs(x)) ? (s[1] - t[1]) + x : (x - t[1]) + s[1];
      s[1] = t[1];
      t[1] = cs[1] + c[1];
      cc[1] = (fabs(cs[1]) >= fabs(c[1])) ? (cs[1] - t[1]) + c[1] : (c[1] - t[1]) + cs[1];
      cs[1] = t[1];
      ccs[1] = ccs[1] + cc[1];
    }
  }
  s0[0] = s[0] + cs[0] + ccs[0];
  s0[1] = s[1] + cs[1] + ccs[1];
  switch (flag) {
    case LUA_TNUMBER: {
      lua_pushnumber(L, (lua_Number)ii/s0[0]);  /* no rounding in between */
      break;
    }
    case LUA_TCOMPLEX: {
      lua_Number t2, t3, t5;
      t2 = s0[0]*s0[0];
      t3 = s0[1]*s0[1];
      t5 = 1/(t2 + t3);
      agn_pushcomplex(L, (lua_Number)ii*s0[0]*t5, (lua_Number)ii*s0[1]*t5);
      break;
    }
    default: {
      lua_pushfail(L);
    }
  }
  return 1;
}


static lua_Number KahanBabuskaHMeanNumArray (lua_Number *a, size_t ii) {
  size_t i;
  volatile lua_Number s, cs, ccs, t, c, cc, x, y;
  s = cs = ccs = 0;
  for (i=0; i < ii; i++) {
    y = a[i];
    x = (y == 0.0) ? AGN_NAN : 1/y;  /* 3.18.8 security fix */
    t = s + x;
    c = (fabs(s) >= fabs(x)) ? (s - t) + x : (x - t) + s;
    s = t;
    t = cs + c;
    cc = (fabs(cs) >= fabs(c)) ? (cs - t) + c : (c - t) + cs;
    cs = t;
    ccs += cc;
  }
  return (lua_Number)ii/(s + cs + ccs);  /* no rounding in between */
}


static lua_Number KahanBabuskaSumdata (lua_Number *a, size_t ii, lua_Number p, lua_Number xm) {  /* 2.4.3 */
  size_t i;
  int isint;
  volatile lua_Number s, cs, ccs, t, c, cc, x, y;
  isint = tools_isint(p);  /* 2.14.13 change */
  s = cs = ccs = 0;
  for (i=0; i < ii; i++) {
    if (isint) {
      if (p == 1) x = a[i] - xm;
      else if (p == 2) {
        y = a[i];  /* 2.12.6 */
        x = y*y - 2*y*xm + xm*xm;
      } else x = tools_intpow(a[i] - xm, p);
    } else
      x = sun_pow(a[i] - xm, p, 0);  /* 2.14.13 change from pow to sun_pow */
    t = s + x;
    c = (fabs(s) >= fabs(x)) ? (s - t) + x : (x - t) + s;
    s = t;
    t = cs + c;
    cc = (fabs(cs) >= fabs(c)) ? (cs - t) + c : (c - t) + cs;
    cs = t;
    ccs += cc;
  }
  return (ii == 0) ? AGN_NAN : s + cs + ccs;  /* 2.4.4, no rounding in between */
}


static lua_Number KahanBabuskaSumdataLog (lua_Number *a, size_t ii, lua_Number p, lua_Number xm, int *sign) {  /* 2.5.2, idea taken from the COLT package published by CERN, Geneva */
  size_t i;
  int isint;
  volatile lua_Number s, cs, ccs, t, c, cc, x, y;
  isint = tools_isint(p);  /* 2.14.13 change */
  s = cs = ccs = 0;
  for (i=0; i < ii; i++) {
    if (isint) {
      if (p == 1) x = a[i] - xm;
      else if (p == 2) {
        y = a[i];  /* 2.12.6 */
        x = y*y - 2*y*xm + xm*xm;
      } else x = tools_intpow(a[i] - xm, p);
    } else
      x = sun_pow(a[i] - xm, p, 0);  /* 2.14.13 change from pow to sun_pow */
    if (x < 0) {
      *sign = *sign * (-1);
      x = -x;
    }
    x = luai_numln(x);  /* 2.5.3 fix, 2.14.13 change from log to luai_numln */
    t = s + x;
    c = (fabs(s) >= fabs(x)) ? (s - t) + x : (x - t) + s;
    s = t;
    t = cs + c;
    cc = (fabs(cs) >= fabs(c)) ? (cs - t) + c : (c - t) + cs;
    cs = t;
    ccs += cc;
  }
  return (ii == 0) ? AGN_NAN : s + cs + ccs;
}


static int aux_getsize (lua_State *L, int idx, int *type, const char *procname) {  /* 3.18.8 */
  *type = lua_type(L, idx);
  switch (*type) {
    case LUA_TTABLE: {
      return agn_asize(L, idx);
    }
    case LUA_TSEQ: {
      return agn_seqsize(L, idx);
    }
    case LUA_TREG: {
      return agn_regsize(L, idx);
    }
    case LUA_TUSERDATA: {
      NumArray *b = checknumarray(L, idx);
      if (b->datatype != NADOUBLE)
        luaL_error(L, "Error in " LUA_QS ": numarray must contain Agena numbers (C doubles).", procname);
      return b->size;
    }
    default: {
      luaL_error(L, "Error in " LUA_QS ": table, sequence, register or userdata expected, got %s.", procname, luaL_typename(L, idx));
    }
  }
  return 0;
}


/* Takes a structure of n elements at stack index idx and returns the arithmetic mean without duplicating data; 3.18.8 */
static lua_Number FORCE_INLINE aux_kbamean (lua_State *L, int idx, int n, const char *procname) {
  int rc, isfn;
  lua_Number val = 0.0;
  isfn = lua_isfunction(L, idx);
  switch (lua_type(L, idx + isfn)) {
    case LUA_TTABLE: {
      val = (isfn) ? KahanBabuskaMeanFn(L, idx, agn_rawgetinumber, n, &rc) :
                     KahanBabuskaMean(L, idx, agn_rawgetinumber, n, &rc);
      break;
    }
    case LUA_TSEQ: {
      val = (isfn) ? KahanBabuskaMeanFn(L, idx, agn_seqrawgetinumber, n, &rc) :
                     KahanBabuskaMean(L, idx, agn_seqrawgetinumber, n, &rc);
      break;
    }
    case LUA_TREG: {
      val = (isfn) ? KahanBabuskaMeanFn(L, idx, agn_regrawgetinumber, n, &rc) :
                     KahanBabuskaMean(L, idx, agn_regrawgetinumber, n, &rc);
      break;
    }
    case LUA_TUSERDATA: {
      NumArray *a = checknumarray(L, 1);
      val = (isfn) ? KahanBabuskaMeanNumArrayFn(L, a->data.n, n) :
                     KahanBabuskaMeanNumArray(a->data.n, n);
      break;
    }
    default:
      luaL_error(L, "Error in " LUA_QS ": table, sequence, register or userdata expected, got %s.", procname, luaL_typename(L, idx));
  }
  return val;
}

static int stats_amean (lua_State *L) {
  int n, type, isfn;
  isfn = lua_isfunction(L, 1);
  luaL_checkany(L, 1 + isfn);
  n = aux_getsize(L, 1 + isfn, &type, "stats.amean");
  if (n < 2) {  /* 2.2.3 */
    lua_pushfail(L);
  } else if (type == LUA_TUSERDATA || isfn) {
    /* let's assume the distribution is a userdata containing numbers only, 4.4.6 */
    /* aux_kbamean can handle userdata, agn_sumupdiv does not. rc will always be initialised during this call */
    lua_pushnumber(L, aux_kbamean(L, 1, n, "stats.amean"));  /* 4.6.1 change */
  } else {
    /* with tables, sequences and registers, agn_sumupdiv is 25 percent faster than aux_kbamean */
    agn_sumupdiv(L, 1, (lua_Number)n, type, "stats.amean");
  }
  return 1;
}


/* The function computes the harmonic mean of the values in the table or sequence o. It is useful with rates
   and ratios, as it provides the truest average.
   1.6.0, March 31, 2012. ported to C 2.12.0 RC (based on stats_amean), changed to use Kahan-Babuška summation

   stats.hmean := proc(o :: {table, sequence}) is  # changed 2.12.0 RC 3
      local r, n, f := 0, size o, math.accu();
      if n < 2 then return fail fi;  # 2.2.3 change
      for i in o do
         if i :: number then
            f(i)  # r := r + 1/i
         else
            argerror(i, 'stats.hmean', 'number expected in structure')
         fi
      od;
      return n/f(0)
   end; */
static int stats_hmean (lua_State *L) {
  int n, type;
  luaL_checkany(L, 1);
  n = aux_getsize(L, 1, &type, "stats.hmean");
  if (n < 2) {  /* 2.2.3 */
    lua_pushfail(L);
  } else {
    switch (type) {
      case LUA_TTABLE: {
        KahanBabuskaHMean(L, agn_rawgeticomplex, n);
        break;
      }
      case LUA_TSEQ: {
        KahanBabuskaHMean(L, agn_seqrawgeticomplex, n);
        break;
      }
      case LUA_TREG: {
        KahanBabuskaHMean(L, agn_regrawgeticomplex, n);
        break;
      }
      case LUA_TUSERDATA: {
        NumArray *a = checknumarray(L, 1);
        lua_pushnumber(L, KahanBabuskaHMeanNumArray(a->data.n, n));
        break;
      }
      default: {
        luaL_error(L, "Error in " LUA_QS ": table, sequence, register or userdata expected, got %s.", "stats.hmean", lua_typename(L, type));
      }
    }
  }
  return 1;
}


/* median of all values in an _already sorted table array or sequence_, June 18, 2007; tweaked Jan 13, 2008;
   faster than an Agena implementation; patched 0.25.4, August 01, 2009; patched 0.26.1, 11.08.2009;
   extended 1.6.0, April 31, 2012, extended 1.8.2, October 07, 2012; rewritten December 30, 2012 */
static int stats_median (lua_State *L) {
  size_t ii;
  lua_Number *a;
  luaL_checkany(L, 1);
  ii = 0;
  a = agnL_tonumarray(L, 1, &ii, "stats.median", 1, 0);  /* 2.18.1 extension */
  if (a == NULL) return 1;  /* issue the error */
  if (ii < 2)
    lua_pushfail(L);
  else
    lua_pushnumber(L, ii&1 ? tools_kth_smallest(a, ii, ii/2) :
      (tools_kth_smallest(a, ii, ii/2 - 1) + tools_kth_smallest(a, ii, ii/2))*0.5  /* 2.17.7 tweak */
    );
  xfree(a);  /* for all structures, free a */
  return 1;
}


/* Absolute deviation of all the values in a structure (the mean of the equally likely absolute deviations
   from the mean); absolute deviation is more robust as it is less sensitive to outliers. Also called `mean
   deviation`. 1.12.8, September 13, 2013, improved 2.2.0 RC 2, May 20, 2014, improved 2.2.3, 30.06.2014;
   rewritten 4.4.8 to handle complex values, as well. */

static void checkadoptions (lua_State *L, int *nargs, int *duplicate, int *coeff, const char *procname) {
  int checkoptions;
  *duplicate = 1;
  *coeff = 0;
  /* check for options */
  checkoptions = 2;  /* check n options; CHANGE THIS if you add/delete options */
  /* check for properly allocated slots will be done by agn_pairgetiall() */
  while (checkoptions-- && *nargs > 1 && lua_ispair(L, *nargs)) {
    agn_pairgetiall(L, *nargs);  /* get left, right value, set to stack index -2, -1, 7.7.8 */
    if (agn_isstring(L, -2)) {
      const char *option = agn_tostring(L, -2);
      if (tools_streqx(option, "dupli", "duplicate", NULL)) {
        *duplicate = agn_checkboolean(L, -1);
      } else if (tools_streq(option, "coeff")) {
        *coeff = agn_checkboolean(L, -1);
      } else {
        agn_poptoptwo(L);
        luaL_error(L, "Error in " LUA_QS ": unknown option " LUA_QS ".", procname, option);
      }
    }
    /* do not call lua_settop as it would corrupt the argument stack since we have already pushed values */
    (*nargs)--;
    agn_poptoptwo(L);
  }
  /* lhf, May 29, 2011 at 12:20 at
     https://stackoverflow.com/questions/6167555/how-can-i-safely-iterate-a-lua-table-while-keys-are-being-removed
     "You can safely remove entries while traversing a table but you cannot create new entries, that is, new keys.
      You can modify the values of existing entries, though. (Removing an entry being a special case of that rule.)" */
}

#define ad_finalise_real(L,s,cs,ccs,n,mean) { \
  s += cs + ccs; \
  if (!coeff) \
    lua_pushnumber(L, s/n); \
  else \
    lua_pushnumber(L, (mean == 0.0) ? AGN_NAN : s/(n*fabs(mean))); \
}

static int stats_ad (lua_State *L) {
  volatile double s, cs, ccs;
  int i, n, rc, type, nargs, duplicate, coeff;
  luaL_checkany(L, 1);
  nargs = lua_gettop(L);
  checkadoptions(L, &nargs, &duplicate, &coeff, "stats.ad");
  n = aux_getsize(L, 1, &type, "stats.ad");
  if (n < 2) {
    lua_pushfail(L);
    return 1;
  }
  s = cs = ccs = 0;
  /* let's assume the distribution is a userdata containing numbers only */
  if (type == LUA_TUSERDATA) {
    /* aux_kbamean can handle userdata, agn_sumupdiv does not. rc will always be initialised during this call */
    size_t i, ii;
    lua_Number *a, mean;
    a = agnL_tonumarray(L, 1, &ii, "stats.ad", 0, 0);
    if (a == NULL) return 1;  /* issue the error */
    mean = aux_kbamean(L, 1, ii, "stats.ad");
    for (i=0; i < ii; i++)
      s = tools_kbadd(s, fabs(a[i] - mean), &cs, &ccs);  /* avoid dividing each difference by the number of observations separately */
    ad_finalise_real(L, s, cs, ccs, n, mean);
    return 1;
  }
  /* with tables, sequences and registers, agn_sumupdiv is 25 percent faster than aux_kbamean */
  agn_sumupdiv(L, 1, (lua_Number)n, type, "stats.ad");
  if (agn_isnumber(L, -1)) {
    volatile double x, mean;
    mean = agn_tonumber(L, -1);
    agn_poptop(L);
    switch (type) {
      case LUA_TTABLE: {
        for (i=1; i <= n; i++) {
          x = agn_rawgetinumber(L, 1, i, &rc);
          s = tools_kbadd(s, fabs(x - mean), &cs, &ccs);
        }
        break;
      }
      case LUA_TSEQ: {
        for (i=1; i <= n; i++) {
          x = agn_seqrawgetinumber(L, 1, i, &rc);
          s = tools_kbadd(s, fabs(x - mean), &cs, &ccs);
        }
        break;
      }
      case LUA_TREG: {
        for (i=1; i <= n; i++) {
          x = agn_regrawgetinumber(L, 1, i, &rc);
          s = tools_kbadd(s, fabs(x - mean), &cs, &ccs);
        }
        break;
      }
    }
    ad_finalise_real(L, s, cs, ccs, n, mean);
  } else if (lua_iscomplex(L, -1)) {
    double x[2], mean[2];
    mean[0] = agn_complexreal(L, -1); mean[1] = agn_compleximag(L, -1);
    agn_poptop(L);
    if (coeff)
      luaL_error(L, "Error in " LUA_QS ": cannot compute coefficient with complex numbers.", "stats.ad");
    switch (type) {
      case LUA_TTABLE: {
        for (i=1; i <= n; i++) {
          agn_rawgeticomplex(L, 1, i, x, &rc);
          s = tools_kbadd(s, sun_hypot(x[0] - mean[0], x[1] - mean[1]), &cs, &ccs);
        }
        break;
      }
      case LUA_TSEQ: {
        for (i=1; i <= n; i++) {
          agn_seqrawgeticomplex(L, 1, i, x, &rc);
          s = tools_kbadd(s, sun_hypot(x[0] - mean[0], x[1] - mean[1]), &cs, &ccs);
        }
        break;
      }
      case LUA_TREG: {
        for (i=1; i <= n; i++) {
          agn_regrawgeticomplex(L, 1, i, x, &rc);
          s = tools_kbadd(s, sun_hypot(x[0] - mean[0], x[1] - mean[1]), &cs, &ccs);
        }
        break;
      }
    }
    s += cs + ccs;
    if (!coeff)
      lua_pushnumber(L, s/n);
    else
      lua_pushnumber(L, (mean[0] == 0.0 && mean[1] == 0.0) ? AGN_NAN : s/(n*sun_hypot(mean[0], mean[1])));
  } else {
    agn_poptop(L);
    luaL_error(L, "Error in " LUA_QS ": structure must include (complex) numbers.", "stats.ad");
  }
  return 1;
}


static int stats_mad (lua_State *L) {  /* 1.12.8, September 13, 2013, improved 2.2.0 RC 2, May 20, 2014 */
  size_t i, ii;
  lua_Number *a;
  luaL_checkany(L, 1);
  ii = 0;
  a = agnL_tonumarray(L, 1, &ii, "stats.mad", 1, 0);  /* 2.18.1 extension */
  if (a == NULL) return 1;  /* issue the error */
  if (ii < 2)  /* 2.2.3 */
    lua_pushfail(L);
  else {
    lua_Number median;
    median = ii&1 ? tools_kth_smallest(a, ii, ii/2) :
      (tools_kth_smallest(a, ii, ii/2 - 1) + tools_kth_smallest(a, ii, ii/2))*0.5;  /* 2.17.7 tweak */
    for (i=0; i < ii; i++)
      a[i] = fabs(a[i] - median);
    if (lua_isnoneornil(L, 2)) {
      lua_pushnumber(L, ii&1 ? tools_kth_smallest(a, ii, ii/2) :
        (tools_kth_smallest(a, ii, ii/2 - 1) + tools_kth_smallest(a, ii, ii/2))*0.5  /* 2.17.7 tweak */
      );
    } else {
      if (median == 0.0)  /* 3.18.8 fix */
        lua_pushundefined(L);
      else
        lua_pushnumber(
          L, ii&1 ? tools_kth_smallest(a, ii, ii/2) / median :
          (tools_kth_smallest(a, ii, ii/2 - 1) + tools_kth_smallest(a, ii, ii/2))*0.5 / median);  /* 2.17.7 tweak */
    }
  }
  xfree(a);  /* for all structures, free a */
  return 1;
}


static int stats_md (lua_State *L) {  /* 2.4.1 */
  size_t i, ii;
  lua_Number *a;
  luaL_checkany(L, 1);
  ii = 0;
  a = agnL_tonumarray(L, 1, &ii, "stats.md", 1, 0);  /* 2.18.1 extension */
  if (a == NULL) return 1;  /* issue the error */
  if (ii < 2)
    lua_pushfail(L);
  else {  /* 2.2.3 speed optimisation */
    lua_Number median;
    volatile double sum, cs, ccs;
    sum = cs = ccs = 0;
    median = (ii & 1) ? tools_kth_smallest(a, ii, ii/2) :
      (tools_kth_smallest(a, ii, ii/2 - 1) + tools_kth_smallest(a, ii, ii/2))*0.5;  /* 2.17.7 tweak */
    for (i=0; i < ii; i++)  /* avoid dividing each difference by the number of observations separately */
      sum = tools_kbadd(sum, fabs(a[i] - median), &cs, &ccs);
    sum += cs + ccs;  /* 3.7.2 change to Kahan-Babuska summation */
    if (lua_isnoneornil(L, 2))
      lua_pushnumber(L, sum/ii);
    else if (median == 0.0)  /* 3.18.8 fix */
      lua_pushundefined(L);
    else
      lua_pushnumber(L, sum/ii/fabs(median));
  }
  xfree(a);  /* for all structures, free a */
  return 1;
}


static int stats_sorted (lua_State *L) {  /* 3.5.4: can now also process numarrays */
  int mode, type;
  size_t ii;
  lua_Number *a;
  static const char *const modenames[] = {"quicksort", "pixelsort", "heapsort", "introsort", "nrquicksort", NULL};
  type = lua_type(L, 1);
  luaL_typecheck(L, type == LUA_TTABLE || type == LUA_TSEQ || type == LUA_TREG || type == LUA_TUSERDATA, 1,
    "table, sequence, register or numarray expected", type);
  ii = 0;
  mode = 3;  /* 0 = recursive quicksort
                1 = pixel quicksort
                2 = heapsort
                3 = introsort (default)
                4 = non-recursive quicksort ala Niklaus Wirth */
  if (lua_gettop(L) == 2)  /* option given */
    mode = (lua_isboolean(L, 2)) ? agn_istrue(L, 2) : luaL_checkoption(L, 2, "introsort", modenames);
  a = agnL_tonumarray(L, 1, &ii, "stats.sorted", 1, 0);  /* 2.18.1 extension, 3.5.4 for numarrays create a duplicate */
  if (a == NULL) return 1;  /* issue the error */
  if (ii < 2)
    lua_pushfail(L);
  else {
    size_t i;
    if (tools_sort(a, ii, mode)) {
      xfree(a);  /* 1.12.4 */
      luaL_error(L, "Error in " LUA_QS ": internal memory allocation failed or other error.", "stats.sorted");
    }
    if (type == LUA_TTABLE) {
      lua_createtable(L, ii, 0);
      for (i=0; i < ii; i++)
        agn_setinumber(L, -1, i + 1, a[i]);
    } else if (type == LUA_TSEQ) {
      agn_createseq(L, ii);
      for (i=0; i < ii; i++)
        agn_seqsetinumber(L, -1, i + 1, a[i]);
    } else if (type == LUA_TREG) {
      agn_createreg(L, ii);
      for (i=0; i < ii; i++)
        agn_regsetinumber(L, -1, i + 1, a[i]);
    } else {  /* 3.5.4 */
      NumArray *b = NULL;
      numarray_createdouble(L, b, ii);
      for (i=0; i < ii; i++)
        b->data.n[i] = a[i];
    }
  }
  xfree(a);  /* for all structures, free a */
  return 1;
}


/* stats.smallest

   Returns the k-th smallest element in the numeric table or sequence a. If k is not given, it is set to 1.
   Code tweaking 2.2.0 RC 2, May 20, 2014 */

static int stats_smallest (lua_State *L) {
  size_t ii, k;
  lua_Number *a;
  luaL_checkany(L, 1);
  k = luaL_optinteger(L, 2, 1);
  ii = 0;
  a = agnL_tonumarray(L, 1, &ii, "stats.smallest", 1, 0);  /* 2.18.1 extension */
  if (a == NULL) return 1;  /* issue the error */
  if (ii < 2) {
    lua_pushfail(L);
  } else if (k < 1 || k > ii) {
    xfree(a);
    luaL_error(L, "Error in " LUA_QS ": second argument too small or too large.", "stats.smallest");
  } else {
    lua_pushnumber(L, tools_kth_smallest(a, ii, k - 1));
  }
  xfree(a);  /* for all structures, free a */
  return 1;
}


/* tbl_minmax: returns the smallest and largest value in a list. If the option
   'sorted' is given, the list is not traversed, instead the first and the last entry
   in the list is returned.
   If the list is empty or has only one element, fail is returned. June 20, 2007;
   modified August 01, 2009, 0.25.4; modified 0.26.1, 11.08.2009, extended 1.3.3, 30.01.2011;
   patched 1.6.0, April 01, 2012; patched 2.41.4 */

static int stats_minmax (lua_State *L) {
  int i, n, type, issorted;
  size_t minpos, maxpos;
  lua_Number min, max, number;
  const char *option;
  NumArray *a = NULL;  /* 2.18.1 extension */
  type = lua_type(L, 1);
  luaL_typecheck(L, type == LUA_TTABLE || type == LUA_TSEQ || type == LUA_TREG || type == LUA_TUSERDATA, 1, "table, sequence, register or numarray expected", type);
  option = luaL_optstring(L, 2, "default");
  n = 0;
  if (type == LUA_TUSERDATA) {  /* 2.18.1 extension */
    a = checknumarray(L, 1);
    if (a->datatype != NADOUBLE)
      luaL_error(L, "Error in " LUA_QS ": numarray must contain Agena numbers (C doubles).", "stats.minmax");
    n = a->size;
    if (n < 2) {
      lua_pushfail(L);
      return 1;
    }
  }
  issorted = tools_streq(option, "sorted");
  if (issorted && type != LUA_TUSERDATA) {
    n = agn_nops(L, 1);
    if (n < 2) {
      lua_pushfail(L);
      return 1;
    }
  }
  luaL_checkstack(L, 3, "not enough stack space");  /* 2.31.7 */
  switch (type) {
    case LUA_TTABLE: {
      lua_newtable(L);
      if (issorted) {  /* 2.16.12 tweak */
        lua_pushinteger(L, 1);
        lua_rawgeti(L, 1, 1);
        lua_rawset(L, -3);
        lua_pushinteger(L, 2);
        lua_rawgeti(L, 1, n);
        lua_rawset(L, -3);
        return 1;
      }
      if (!agn_minmax(L, 1, 0, &min, &minpos, &max, &maxpos, &n)) {  /* 4.9.2 change: 5x faster */
        lua_pushfail(L);
        return 1;
      }
      agn_setinumber(L, -1, 1, min);
      agn_setinumber(L, -1, 2, max);
      break;
    }
    case LUA_TSEQ: {
      agn_createseq(L, 2);
      if (issorted) {  /* 2.16.12 tweak */
        lua_seqrawgeti(L, 1, 1);
        lua_seqrawseti(L, -2, 1);
        lua_seqrawgeti(L, 1, n);
        lua_seqrawseti(L, -2, 2);
        return 1;
      }
      if (!agn_minmax(L, 1, 0, &min, &minpos, &max, &maxpos, &n)) {  /* 4.9.2 change: 4x faster */
        lua_pushfail(L);
        return 1;
      }
      agn_seqsetinumber(L, -1, 1, min);
      agn_seqsetinumber(L, -1, 2, max);
      break;
    }
    case LUA_TREG: {  /* 3.18.8 */
      agn_createreg(L, 2);
      if (issorted) {
        agn_regrawgeti(L, 1, 1);
        agn_regrawseti(L, -2, 1);
        agn_regrawgeti(L, 1, n);
        agn_regrawseti(L, -2, 2);
        return 1;
      }
      if (!agn_minmax(L, 1, 0, &min, &minpos, &max, &maxpos, &n)) {  /* 4.9.2 change: 4x faster */
        lua_pushfail(L);
        return 1;
      }
      agn_regsetinumber(L, -1, 1, min);
      agn_regsetinumber(L, -1, 2, max);
      break;
    }
    case LUA_TUSERDATA: {  /* 2.18.1 */
      agn_createseq(L, 2);
      if (issorted) {
        agn_seqsetinumber(L, -1, 1, a->data.n[0]);
        agn_seqsetinumber(L, -1, 2, a->data.n[n - 1]);
        return 1;
      }
      min = a->data.n[0];
      max = min;
      for (i=1; i < n; i++) {
        number = a->data.n[i];
        if (number < min)
          min = number;
        if (number > max)
          max = number;
      }
      agn_seqsetinumber(L, -1, 1, min);
      agn_seqsetinumber(L, -1, 2, max);
      break;
    }
    default: lua_assert(0);  /* should not happen */
  }
  return 1;
}


/* Returns minimum value in an observation (a table, sequence, register or numarray) and its index position, in this order.
   If the observation is empty, returns `fail` twice. Based on luaB_min. 4.1.2 */
static int stats_min (lua_State *L) {
  int i, n, type, isnumarraygiven, issorted;
  size_t minpos, maxpos;
  lua_Number min, max, v;
  const char *option;
  type = lua_type(L, 1);
  isnumarraygiven = isnumarray(L, 1);
  luaL_typecheck(L, type == LUA_TTABLE || type == LUA_TSEQ || type == LUA_TREG || isnumarraygiven, 1, "table, sequence, register or numarray expected", type);
  min = n = minpos = 0;  /* to prevent compiler warnings */
  luaL_checkstack(L, 2, "not enough stack space");
  option = luaL_optstring(L, 2, "default");
  if (isnumarraygiven) {
    NumArray *ud = lua_touserdata(L, 1);
    if (ud->datatype != NADOUBLE)  /* 3.5.4*/
      luaL_error(L, "Error in " LUA_QS ": numarray must contain Agena numbers (C doubles).", "stats.min");
    n = ud->size;
    if (n < 2) {
      lua_pushfail(L);
      lua_pushfail(L);
      return 2;
    }
  }
  issorted = tools_streq(option, "sorted");
  if (issorted && type != LUA_TUSERDATA) {
    n = agn_nops(L, 1);
    if (n < 2) {
      lua_pushfail(L);
      lua_pushfail(L);
      return 2;
    }
  }
  if (issorted) {
    if (type == LUA_TTABLE)
      lua_pushnumber(L, agn_getinumber(L, 1, 1));
    else if (type == LUA_TSEQ)
      lua_pushnumber(L, lua_seqrawgetinumber(L, 1, 1));
    else if (type == LUA_TREG)
      lua_pushnumber(L, agn_reggetinumber(L, 1, 1));
    else {  /* numarray */
      NumArray *ud = lua_touserdata(L, 1);
      lua_Number *arr = ud->data.n;
      lua_pushnumber(L, arr[0]);
    }
    lua_pushinteger(L, 1);
    return 2;
  }
  switch (type) {
    case LUA_TTABLE: case LUA_TSEQ: case LUA_TREG: {
      if (!agn_minmax(L, 1, 0, &min, &minpos, &max, &maxpos, &n)) {  /* 4.9.2 change: 4x faster */
        lua_pushfail(L);
        lua_pushfail(L);
        return 2;
      }
      break;
    }
    case LUA_TUSERDATA: {
      NumArray *ud = lua_touserdata(L, 1);
      lua_Number *arr = ud->data.n;
      min = arr[0];
      for (i=1; i < n; i++) {
        v = arr[i];
        if (v < min) { min = v; minpos = i + 1; }
      }
      break;
    }
    default: lua_assert(0);  /* should not happen */
  }
  lua_pushnumber(L, min);
  lua_pushinteger(L, minpos);
  return 2;
}


/* Returns maximum value in an observation (a table, sequence, register or numarray) and its index position, in this order.
   If the observation is empty, returns `fail` twice. Based on luaB_min. 4.1.2 */
static int stats_max (lua_State *L) {
  int i, n, type, isnumarraygiven, issorted;
  size_t minpos, maxpos;
  lua_Number max, min, v;
  const char *option;
  type = lua_type(L, 1);
  isnumarraygiven = isnumarray(L, 1);
  luaL_typecheck(L, type == LUA_TTABLE || type == LUA_TSEQ || type == LUA_TREG || isnumarraygiven, 1, "table, sequence, register or numarray expected", type);
  max = n = maxpos = 0;  /* to prevent compiler warnings */
  luaL_checkstack(L, 2, "not enough stack space");
  option = luaL_optstring(L, 2, "default");
  if (isnumarraygiven) {
    NumArray *ud = lua_touserdata(L, 1);
    if (ud->datatype != NADOUBLE)  /* 3.5.4*/
      luaL_error(L, "Error in " LUA_QS ": numarray must contain Agena numbers (C doubles).", "stats.max");
    n = ud->size;
    if (n < 2) {
      lua_pushfail(L);
      lua_pushfail(L);
      return 2;
    }
  }
  issorted = tools_streq(option, "sorted");
  if (issorted && type != LUA_TUSERDATA) {
    n = agn_nops(L, 1);
    if (n < 2) {
      lua_pushfail(L);
      lua_pushfail(L);
      return 2;
    }
  }
  if (issorted) {
    if (type == LUA_TTABLE)
      lua_pushnumber(L, agn_getinumber(L, 1, n));
    else if (type == LUA_TSEQ)
      lua_pushnumber(L, lua_seqrawgetinumber(L, 1, n));
    else if (type == LUA_TREG)
      lua_pushnumber(L, agn_reggetinumber(L, 1, n));
    else {  /* numarray */
      NumArray *ud = lua_touserdata(L, 1);
      lua_Number *arr = ud->data.n;
      lua_pushnumber(L, arr[n - 1]);
    }
    lua_pushinteger(L, n);
    return 2;
  }
  switch (type) {
    case LUA_TTABLE: case LUA_TSEQ: case LUA_TREG: {
      if (!agn_minmax(L, 1, 0, &min, &minpos, &max, &maxpos, &n)) {  /* 4.9.2 change: 4x faster */
        lua_pushfail(L);
        lua_pushfail(L);
        return 2;
      }
      break;
    }
    case LUA_TUSERDATA: {
      NumArray *ud = lua_touserdata(L, 1);
      lua_Number *arr = ud->data.n;
      max = arr[0];
      for (i=1; i < n; i++) {
        v = arr[i];
        if (v > max) { max = v; maxpos = i + 1; }
      }
      break;
    }
    default: lua_assert(0);  /* should not happen */
  }
  lua_pushnumber(L, max);
  lua_pushinteger(L, maxpos);
  return 2;
}


/* stats_midrange: Computes the sum of the minimum and maximum value of a distribution, divided by two.

   If the option 'sorted' is given, the list is not traversed; instead the first and the last entry
   is taken to compute the mean. If the list is empty or has only one element, fail is returned.
   2.9.4 */

static int stats_midrange (lua_State *L) {
  int i, n, type, issorted;
  size_t minpos, maxpos;
  lua_Number min, max, number;
  const char *option;
  NumArray *a = NULL;  /* 2.18.1 extension */
  type = lua_type(L, 1);
  luaL_typecheck(L, type == LUA_TTABLE || type == LUA_TSEQ || type == LUA_TREG || type == LUA_TUSERDATA, 1, "table, sequence, register or numarray expected", type);
  option = luaL_optstring(L, 2, "default");
  min = max = n = 0;
  if (type == LUA_TUSERDATA) {  /* 2.18.1 extension */
    a = checknumarray(L, 1);
    if (a->datatype != NADOUBLE)
      luaL_error(L, "Error in " LUA_QS ": numarray must contain Agena numbers (C doubles).", "stats.midrange");
    n = a->size;
    if (n < 2) {
      lua_pushfail(L);
      lua_pushfail(L);
      return 2;
    }
  }
  issorted = tools_streq(option, "sorted");
  if (issorted && type != LUA_TUSERDATA) {
    n = agn_nops(L, 1);
    if (n < 2) {
      lua_pushfail(L);
      lua_pushfail(L);
      return 2;
    }
  }
  switch (type) {
    case LUA_TTABLE: {
      if (issorted) {  /* 2.16.12 tweak */
        min = agn_getinumber(L, 1, 1);
        max = agn_getinumber(L, 1, n);
      } else if (!agn_minmax(L, 1, 0, &min, &minpos, &max, &maxpos, &n)) {  /* 4.9.2 change: 5x faster */
        lua_pushfail(L);
        lua_pushfail(L);
        return 2;
      }
      break;
    }
    case LUA_TSEQ: {
      if (issorted) {  /* 2.16.12 tweak */
        min = lua_seqrawgetinumber(L, 1, 1);
        max = lua_seqrawgetinumber(L, 1, n);
      } else if (!agn_minmax(L, 1, 0, &min, &minpos, &max, &maxpos, &n)) {  /* 4.9.2 change: 4x faster */
        lua_pushfail(L);
        lua_pushfail(L);
        return 2;
      }
      break;
    }
    case LUA_TREG: {  /* 3.18.8 */
      int rc;
      if (issorted) {
        min = agn_regrawgetinumber(L, 1, 1, &rc);
        if (!rc) min = HUGE_VAL;  /* 4.4.7 change */
        max = agn_regrawgetinumber(L, 1, n, &rc);
        if (!rc) max = HUGE_VAL;  /* ditto */
      } else if (!agn_minmax(L, 1, 0, &min, &minpos, &max, &maxpos, &n)) {  /* 4.9.2 change: 4x faster */
        lua_pushfail(L);
        lua_pushfail(L);
        return 2;
      }
      break;
    }
    case LUA_TUSERDATA: {  /* 2.18.1 */
      if (issorted) {
        min = a->data.n[0];
        max = a->data.n[n - 1];
      } else {
        min = a->data.n[0];
        max = min;
        for (i=1; i < n; i++) {
          number = a->data.n[i];
          if (number < min)
            min = number;
          if (number > max)
            max = number;
        }
      }
      break;
    }
    default: lua_assert(0);  /* should not happen */
  }
  lua_pushnumber(L, (min + max)*0.5);  /* 2.17.7 tweak */
  return 1;
}


#define setinumber(L,idx,t,d,ak,vec,pn) { \
  switch (t) { \
    case LUA_TTABLE: \
      agn_setinumber(L, idx, d, ak); \
      break; \
    case LUA_TSEQ: \
      agn_seqsetinumber(L, idx, d, ak); \
      break; \
    case LUA_TREG: { \
      if (d > agn_regsize(L, idx)) { \
        agn_regresize(L, idx, agn_newsize(NULL, d - 1), 1); \
      } \
      agn_regsetinumber(L, idx, d, ak); \
      break; \
    } \
    case LUA_TUSERDATA: \
      if (dblvec_append(vec, ak)) { \
        xfree(a); \
        dblvec_free(v); \
        luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", pn); \
      } \
      break; \
    default: { \
      xfree(a); \
      dblvec_free(v); \
      luaL_error(L, "Error in " LUA_QS ": this should not happen.", pn); \
    } \
  } \
}

/* Based on stats_sorted, 4.10.3 */
static int stats_mode (lua_State *L) {
  int mode, type;
  size_t ii;
  lua_Number *a;
  static const char *const modenames[] = {"quicksort", "pixelsort", "heapsort", "introsort", "nrquicksort", NULL};
  type = lua_type(L, 1);
  luaL_typecheck(L, type == LUA_TTABLE || type == LUA_TSEQ || type == LUA_TREG || type == LUA_TUSERDATA, 1, "table, sequence, register or numarray expected", type);
  ii = 0;
  mode = 3;  /* 0 = recursive quicksort
                1 = pixel quicksort
                2 = heapsort
                3 = introsort (default)
                4 = non-recursive quicksort ala Niklaus Wirth */
  if (lua_gettop(L) == 2)  /* option given */
    mode = (lua_isboolean(L, 2)) ? agn_istrue(L, 2) : luaL_checkoption(L, 2, "introsort", modenames);
  a = agnL_tonumarray(L, 1, &ii, "stats.mode", 1, 0);
  if (a == NULL) return 1;  /* issue the error */
  if (ii < 2)
    lua_pushfail(L);
  else {
    int flag;
    size_t i, c, maxc, d, n;
    lua_Number modal;
    dblvec v;
    if (dblvec_init(v, 0)) {
      xfree(a);
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "stats.mode");
    }
    c = maxc = 1;
    if (tools_sort(a, ii, mode)) {
      xfree(a);
      dblvec_free(v);
      luaL_error(L, "Error in " LUA_QS ": internal memory allocation failed or other error.", "stats.mode");
    }
    switch (type) {
      case LUA_TTABLE:
        lua_createtable(L, sun_ilogb(ii), 0);
        break;
      case LUA_TSEQ:
        agn_createseq(L, sun_ilogb(ii));
        break;
      case LUA_TREG:
        agn_createreg(L, sun_ilogb(ii));
        break;
      default: {
        /* numarray, new numarray will be created below */
      }
    }
    modal = a[0];
    for (i=1; i < ii; i++) {
      if (a[i] == a[i - 1]) c++;
      else {
        if (c > maxc) { maxc = c; modal = a[i - 1]; }
        c = 1;
      }
    }
    /* is last sample a modal value ? */
    if ( (flag = (c >= maxc)) ) { modal = a[ii - 1]; maxc = c; }
    /* now find all the samples with the largest count */
    c = d = 1;
    for (i=1; i < ii; i++) {
      if (a[i] == a[i - 1]) c++;
      else {
        if (c == maxc) {
          setinumber(L, -1, type, d, a[i - 1], v, "stats.mode");
          d++;  /* do NOT put `d++` into the macro call, it won't work with numarrays ! */
        }
        c = 1;
      }
    }
    if (flag) {  /* we must take care for the last sample as it also a modal value */
      setinumber(L, -1, type, d, modal, v, "stats.mode");
    }
    n = d - 1 + flag;  /* actual number of modal values */
    switch (type) {
      case LUA_TTABLE:  /* input is a table */
        agn_resize(L, -1, n);
        break;
      case LUA_TSEQ:  /* or sequence */
        agn_seqresize(L, -1, n);
        break;
      case LUA_TREG:  /* or register, new 4.11.7 */
        agn_regresize(L, -1, n, 0);
        break;
      default: {  /* or a numarray, OPTIMISE ! */
        NumArray *b = NULL;
        numarray_createdouble(L, b, n);
        for (i=0; i < n; i++) {
          b->data.n[i] = dblvec_get(v, i);
        }
      }
    }
    lua_pushnumber(L, maxc);
    dblvec_free(v);  /* 4.10.3a fix: always free double buffer for it is initialised even for non-numarrays. */
  }
  xfree(a);  /* for all structures, free a */
  return 1 + (ii > 1);
}


/* IOS: Index On Stability
   operates on sorted and unsorted list but with different results ! Sum up absolute differences
   between neighbouring points and divide by the number of differences. This indicator is quite
   useful to find out how stable or volatile a distribution is.

   This C implementation is at least 3 to five times faster than the almost equivalent versions:

   stats.ios := proc(x :: table) is
      local n, result := size(x), 0;       # n: size of table, result: intermediate results
      for i from 2 to n do
         inc result, abs(x[i] - x[i - 1])  # add absolute distances of neighboring values
      od;
      return result/(n - 1)                # divide by number of entries - 1 and return result
   end;

   or for short:

   stats.ios := << x :: table -> sadd(stats.deltalist(x, true))/(size x - 1) >> */

static int stats_ios (lua_State *L) {  /* 2.2.1 change */
  size_t i, n;
  int type, option;
  lua_Number x1, ios2, max;
  volatile double result, x2, cs, ccs;
  NumArray *a = NULL;  /* 2.18.1 extension */
  type = lua_type(L, 1);
  luaL_typecheck(L, type == LUA_TTABLE || type == LUA_TSEQ || type == LUA_TREG || type == LUA_TUSERDATA, 1, "table, sequence, register or numarray expected", type);
  if (type == LUA_TUSERDATA) {  /* 2.18.1 extension */
    a = checknumarray(L, 1);
    if (a->datatype != NADOUBLE)
      luaL_error(L, "Error in " LUA_QS ": numarray must contain Agena numbers (C doubles).", "stats.ios");
    n = a->size;
  } else
    n = agn_nops(L, 1);
  if (n < 2) {
    lua_pushfail(L);
    return 1;
  }
  option = !(lua_isnoneornil(L, 2));
  result = ios2 = cs = ccs = 0;
  max = 1;
  switch (type) {
    case LUA_TTABLE: {
      if (option) {  /* scale distribution to [-infinity, 1], 2.2.1 */
        max = 0;
        for (i=1; i <= n; i++) {
          x1 = agn_getinumber(L, 1, i);
          if (fabs(x1) > fabs(max)) max = x1;
        }
      }
      x1 = agn_getinumber(L, 1, 1);
      for (i=2; i <= n; i++) {
        x2 = agn_getinumber(L, 1, i);
        result = (max == 0.0) ? AGN_NAN : tools_kbadd(result, fabs(x2 - x1)/max, &cs, &ccs);  /* 3.7.2 change to Kahan-Babuska summation, 3.18.8 fix */
        x1 = x2;
      }
      break;
    }
    case LUA_TSEQ: {
      if (option) {  /* scale distribution to [-infinity, 1], 2.2.1 */
        max = 0;
        for (i=1; i <= n; i++) {
          x1 = lua_seqrawgetinumber(L, 1, i);
          if (fabs(x1) > fabs(max)) max = x1;
        }
      }
      x1 = lua_seqrawgetinumber(L, 1, 1);
      for (i=2; i <= n; i++) {
        x2 = lua_seqrawgetinumber(L, 1, i);
        result = (max == 0.0) ? AGN_NAN : tools_kbadd(result, fabs(x2 - x1)/max, &cs, &ccs);  /* 3.7.2 change to Kahan-Babuska summation */
        x1 = x2;
      }
      break;
    }
    case LUA_TREG: {  /* 3.18.8 */
      if (option) {  /* scale distribution to [-infinity, 1] */
        max = 0;
        for (i=1; i <= n; i++) {
          x1 = agn_reggetinumber(L, 1, i);
          if (fabs(x1) > fabs(max)) max = x1;
        }
      }
      x1 = agn_reggetinumber(L, 1, 1);
      for (i=2; i <= n; i++) {
        x2 = agn_reggetinumber(L, 1, i);
        result = (max == 0.0) ? AGN_NAN : tools_kbadd(result, fabs(x2 - x1)/max, &cs, &ccs);
        x1 = x2;
      }
      break;
    }
    case LUA_TUSERDATA: {  /* 2.18.1 extension */
      if (option) {
        max = 0;
        for (i=0; i < n; i++) {
          x1 = a->data.n[i];
          if (fabs(x1) > fabs(max)) max = x1;
        }
      }
      x1 = a->data.n[0];
      for (i=1; i < n; i++) {
        x2 = a->data.n[i];
        result = (max == 0.0) ? AGN_NAN : tools_kbadd(result, fabs(x2 - x1)/max, &cs, &ccs);  /* 3.7.2 change to Kahan-Babuska summation */
        x1 = x2;
      }
      break;
    }
    default: lua_assert(0);  /* should not happen */
  }
  result += cs + ccs;  /* 3.7.2 change to Kahan-Babuska summation */
  lua_pushnumber(L, result/((lua_Number)n - 1));  /* n is always > 1 */
  return 1;
}


/* sum, Agena 1.8.2, 05.10.2012; extended 1.8.3, 08.10.2012; extended 2.5.2, 06/04/2015

   Computes the sums of all numbers in a table or sequence. Contrary to the `sadd` operator, it prevents round-off
   errors during summation. Inspired by the TI-Nspire(tm) CX CAS.

   The function also accepts structures including the value `undefined`. In this case, all `undefined's` are simply
   ignored and skipped in the computation of the sums. Thus the function can be used with incomplete observations. */


/* Fills the given array a, malloced before, with a maximum of n values of the object at stack position 1 (no function given)
   or 2 (function given), of type objtype. The actual number of values filled, is returned in *size. */
void aux_fillarraysumdatax (lua_State *L, lua_Number *a, size_t *size, size_t n, size_t nargs, int fngiven, int objtype,
     const char *procname) {
  size_t i, ii, j;
  lua_Number x2;
  ii = 0;
  switch (objtype) {
    case LUA_TTABLE: {
      if (!fngiven) {
        for (i=1; i <= n; i++) {
          x2 = agn_getinumber(L, 1, i);
          if (!tools_isnan(x2)) a[ii++] = x2;  /* 2.10.1, changed to isnan instead of tools_isnan */
        }
      } else {  /* procedure passed */
        size_t slots = 2 + (nargs > 4)*(nargs - 4);  /* 3.15.3 fix */
        for (i=1; i <= n; i++) {
          lua_checkstack(L, slots);  /* 3.5.4 fix */
          lua_pushvalue(L, 1);  /* push function */
          x2 = agn_getinumber(L, 2, i);
          lua_pushnumber(L, x2);
          for (j=5; j <= nargs; j++) {
            lua_pushvalue(L, j);
          }
          lua_call(L, slots - 1, 1);  /* call function with the given arguments and one result, 1.8.3, 3.15.3 fix */
          if (lua_istrue(L, -1)) { if (!tools_isnan(x2)) a[ii++] = x2; }  /* 2.10.1, changed to isnan instead of tools_isnan, 5.0.1 fix */
          agn_poptop(L);
        }
      }
      break;
    }
    case LUA_TSEQ: {
      if (!fngiven) {
        for (i=1; i <= n; i++) {
          x2 = lua_seqrawgetinumber(L, 1, i);
          if (!tools_isnan(x2)) a[ii++] = x2;  /* 2.10.1, changed to isnan instead of tools_isnan */
        }
      } else {
        size_t slots = 2 + (nargs > 4)*(nargs - 4);  /* 3.15.3 fix */
        for (i=1; i <= n; i++) {
          lua_checkstack(L, slots);  /* 3.5.4 fix */
          lua_pushvalue(L, 1);  /* push function */
          x2 = lua_seqrawgetinumber(L, 2, i);
          lua_pushnumber(L, x2);
          for (j=5; j <= nargs; j++) {
            lua_pushvalue(L, j);
          }
          lua_call(L, slots - 1, 1);  /* call function with the given arguments and one result, 1.8.3 */
          if (lua_istrue(L, -1)) { if (!tools_isnan(x2)) a[ii++] = x2; }  /* 2.10.1, changed to isnan instead of tools_isnan, 5.0.1 fix */
          agn_poptop(L);
        }
      }
      break;
    }
    case LUA_TREG: {  /* 3.18.8 */
      int rc;
      if (!fngiven) {
        for (i=1; i <= n; i++) {
          x2 = agn_regrawgetinumber(L, 1, i, &rc);
          if (rc) a[ii++] = x2;  /* 4.4.7 change */
        }
      } else {
        size_t slots = 2 + (nargs > 4)*(nargs - 4);
        for (i=1; i <= n; i++) {
          lua_checkstack(L, slots);
          lua_pushvalue(L, 1);  /* push function */
          x2 = agn_regrawgetinumber(L, 2, i, &rc);
          lua_pushnumber(L, rc ? x2 : HUGE_VAL);  /* 4.4.7 change */
          for (j=5; j <= nargs; j++) {
            lua_pushvalue(L, j);
          }
          lua_call(L, slots - 1, 1);  /* call function with the given arguments and one result */
          if (lua_istrue(L, -1)) { if (!tools_isnan(x2)) a[ii++] = x2; }  /* 5.0.1 fix */
          agn_poptop(L);
        }
      }
      break;
    }
    case LUA_TUSERDATA: {  /* 2.18.1 extension */
      lua_Number *arr;
      NumArray *ud = lua_touserdata(L, 1 + fngiven);
      if (ud->datatype != NADOUBLE)
        luaL_error(L, "Error in " LUA_QS ": numarray must contain Agena numbers (C doubles).", procname);  /* 3.5.4 fix */
      arr = ud->data.n;
      if (!fngiven) {
        for (i=0; i < n; i++) {
          x2 = arr[i];
          if (!tools_isnan(x2)) a[ii++] = x2;
        }
      } else {
        size_t slots = 2 + (nargs > 4)*(nargs - 4);  /* 3.15.3 fix */
        for (i=0; i < n; i++) {
          lua_checkstack(L, slots);  /* 3.5.4 fix */
          lua_pushvalue(L, 1);  /* push function */
          x2 = arr[i];
          lua_pushnumber(L, x2);
          for (j=5; j <= nargs; j++) {
            lua_pushvalue(L, j);
          }
          lua_call(L, slots - 1, 1);  /* call function with the given arguments and one result */
          if (lua_istrue(L, -1) && !tools_isnan(x2)) a[ii++] = x2;  /* 5.0.1 fix */
          agn_poptop(L);
        }
      }
      break;
    }
    default: lua_assert(0);  /* should not happen */
  }
  *size = ii;
}

static int stats_sumdata (lua_State *L) {
  size_t ii, n, nargs;
  int objtype, fngiven, isnumarraygiven;
  lua_Number *a, p, xm;
  luaL_checkany(L, 1);
  ii = 0;
  nargs = lua_gettop(L);
  if (nargs < 1)
    luaL_error(L, "Error in " LUA_QS ": expected one or more arguments, got none.", "stats.sumdata");
  luaL_checkstack(L, nargs, "too many arguments");  /* 2.14.0 fix */
  fngiven = lua_type(L, 1) == LUA_TFUNCTION;
  objtype = lua_type(L, 1 + fngiven);
  isnumarraygiven = isnumarray(L, 1 + fngiven);  /* 2.18.1 extension */
  luaL_typecheck(L, objtype == LUA_TTABLE || objtype == LUA_TSEQ || objtype == LUA_TREG || isnumarraygiven,
    1 + fngiven, "table, sequence, register or numarray expected", objtype);
  p = agnL_optnumber(L, 2 + fngiven, 1);   /* the moment (power p) */
  xm = agnL_optnumber(L, 3 + fngiven, 0);  /* origin, quantity about which the moment is computed (optional value to be subtracted from x[i]) */
  if (isnumarraygiven) {  /* 2.18.1 extension */
    NumArray *ud = lua_touserdata(L, 1 + fngiven);
    n = ud->size;
  } else
    n = agn_nops(L, 1 + fngiven);  /* number of elements in structure */
  agn_createarray(a, n, "stats.sumdata");
  aux_fillarraysumdatax(L, a, &ii, n, nargs, fngiven, objtype, "stats.sumdata");
  if (ii < 2)
    lua_pushfail(L);
  else
    lua_pushnumber(L, KahanBabuskaSumdata(a, ii, p, xm));  /* 2.4.3, 2.5.2 improvement */
  xfree(a);
  return 1;
}


/* Sums up the natural logarithms of all the observations in a distribution using Kahan-Babuska round-off prevention. Idea taken
   from the COLT package published by CERN. */

static int stats_sumdataln (lua_State *L) {
  size_t ii, n, nargs;
  int objtype, fngiven, sign, isnumarraygiven;
  lua_Number *a, p, xm;
  luaL_checkany(L, 1);
  ii = 0;
  sign = 1;
  nargs = lua_gettop(L);
  if (nargs < 1)
    luaL_error(L, "Error in " LUA_QS ": expected one or more arguments, got none.", "stats.sumdataln");
  fngiven = lua_type(L, 1) == LUA_TFUNCTION;
  objtype = lua_type(L, 1 + fngiven);
  isnumarraygiven = isnumarray(L, 1 + fngiven);  /* 2.18.1 extension */
  luaL_typecheck(L, objtype == LUA_TTABLE || objtype == LUA_TSEQ || objtype == LUA_TSEQ || isnumarraygiven,
    1 + fngiven, "table, sequence, register or numarray expected", objtype);
  p = agnL_optnumber(L, 2 + fngiven, 1);   /* the moment (power p) */
  xm = agnL_optnumber(L, 3 + fngiven, 0);  /* origin, quantity about which the moment is computed (optional value to be subtracted from x[i]) */
  if (isnumarraygiven) {  /* 2.18.1 extension */
    NumArray *ud = lua_touserdata(L, 1 + fngiven);
    n = ud->size;
  } else
    n = agn_nops(L, 1 + fngiven);  /* number of elements in structure */
  agn_createarray(a, n, "stats.sumdataln");
  aux_fillarraysumdatax(L, a, &ii, n, nargs, fngiven, objtype, "stats.sumdataln");
  if (ii < 2)
    lua_pushfail(L);
  else {
    lua_Number r;
    r = KahanBabuskaSumdataLog(a, ii, p, xm, &sign);
    lua_pushnumber(L, (sign == -1) ? AGN_NAN : r); /* 2.4.3, 2.5.2 improvement */
  }
  xfree(a);
  return 1;
}


/* Cumulative sum, Agena 1.7.9a, 11.09.2012, extended Agena 1.8.2, 05.10.2012

   Uses a modified Kahan algorithm developed by Kazufumi Ozawa published in his paper `Analysis and Improvement of
   Kahan's Summation Algorithm` to prevent round-off errors during summation. Inspired by the cumulativeSum function
   available on the TI-Nspire(tm) CX CAS.

   The function also accepts structures including the value `undefined`. In this case, all `undefined's` are simply
   included in the resulting structure and are ignored in the computation of the sums. Thus the function can be used
   with incomplete observations. */

static int stats_cumsum (lua_State *L) {
  size_t i, n, isnumarraygiven;
  int type;
  volatile lua_Number x, s, s0, c, cc, cs, ccs, t;
  type = lua_type(L, 1);
  isnumarraygiven = isnumarray(L, 1);  /* 2.18.1 extension */
  luaL_typecheck(L, type == LUA_TTABLE || type == LUA_TSEQ || type == LUA_TREG || isnumarraygiven, 1, "table, sequence, register or numarray expected", type);
  if (isnumarraygiven) {
    NumArray *ud = lua_touserdata(L, 1);
    if (ud->datatype != NADOUBLE)  /* 3.5.4*/
      luaL_error(L, "Error in " LUA_QS ": numarray must contain Agena numbers (C doubles).", "stats.cumsum");
    n = ud->size;
  } else
    n = agn_nops(L, 1);  /* number of elements in structure */
  if (n < 1) {
    lua_pushfail(L);
    return 1;
  }
  s = c = cs = ccs = 0;
  switch (type) {
    case LUA_TTABLE: {
      lua_createtable(L, n, 0);
      for (i=1; i <= n; i++) {
        x = agn_getinumber(L, 1, i);
        if (tools_isnan(x)) {
          agn_setinumber(L, -1, i, x);
          continue;
        }
        t = s + x;  /* 2.31.3, changed to Kahan-Babuška */
        c = (fabs(s) >= fabs(x)) ? (s - t) + x : (x - t) + s;
        s = t;
        t = cs + c;
        cc = (fabs(cs) >= fabs(c)) ? (cs - t) + c : (c - t) + cs;
        cs = t;
        ccs += cc;
        s0 = s + cs + ccs;
        agn_setinumber(L, -1, i, (fabs(s0) < AGN_HEPSILON) ? 0 : s0);
      }
      break;
    }
    case LUA_TSEQ: {
      agn_createseq(L, n);
      for (i=1; i <= n; i++) {
        x = lua_seqrawgetinumber(L, 1, i);
        if (tools_isnan(x)) {
          agn_seqsetinumber(L, -1, i, x);
          continue;
        }
        t = s + x;  /* 2.31.3, changed to Kahan-Babuška */
        c = (fabs(s) >= fabs(x)) ? (s - t) + x : (x - t) + s;
        s = t;
        t = cs + c;
        cc = (fabs(cs) >= fabs(c)) ? (cs - t) + c : (c - t) + cs;
        cs = t;
        ccs += cc;
        s0 = s + cs + ccs;
        agn_seqsetinumber(L, -1, i, (fabs(s0) < AGN_HEPSILON) ? 0 : s0);
      }
      break;
    }
    case LUA_TREG: {  /* 3.18.8 */
      int rc;
      agn_createreg(L, n);
      for (i=1; i <= n; i++) {
        x = agn_regrawgetinumber(L, 1, i, &rc);
        if (tools_isnan(x)) {
          agn_regsetinumber(L, -1, i, x);
          continue;
        }
        t = s + x;  /* 2.31.3, changed to Kahan-Babuška */
        c = (fabs(s) >= fabs(x)) ? (s - t) + x : (x - t) + s;
        s = t;
        t = cs + c;
        cc = (fabs(cs) >= fabs(c)) ? (cs - t) + c : (c - t) + cs;
        cs = t;
        ccs += cc;
        s0 = s + cs + ccs;
        agn_regsetinumber(L, -1, i, (fabs(s0) < AGN_HEPSILON) ? 0 : s0);
      }
      break;
    }
    case LUA_TUSERDATA: {  /* 2.18.1 extension */
      NumArray *ud = lua_touserdata(L, 1);
      lua_Number *arr = ud->data.n;
      agn_createseq(L, n);
      for (i=0; i < n; i++) {
        x = arr[i];
        if (tools_isnan(x)) {
          agn_seqsetinumber(L, -1, i + 1, x);
          continue;
        }
        t = s + x;  /* 2.31.3, changed to Kahan-Babuška */
        c = (fabs(s) >= fabs(x)) ? (s - t) + x : (x - t) + s;
        s = t;
        t = cs + c;
        cc = (fabs(cs) >= fabs(c)) ? (cs - t) + c : (c - t) + cs;
        cs = t;
        ccs += cc;
        s0 = s + cs + ccs;
        agn_seqsetinumber(L, -1, i + 1, (fabs(s0) < AGN_HEPSILON) ? 0 : s0);
      }
      break;
    }
    default: lua_assert(0);  /* should not happen */
  }
  return 1;
}


/* checks whether all numbers in a table or sequence obj are stored in ascending order. If a value in obj is
   not a number, it is ignored. If obj is a table, you have to make sure that it does not contain holes.
   If it contains holes, apply tables.entries on obj. See also: sort. March 31, 2012, extended November 17, 2012 */

static int stats_issorted (lua_State *L) {
  size_t i, n, nargs;
  int type, isf, isnumarraygiven;
  lua_Number x1, x2;
  type = lua_type(L, 1);
  isnumarraygiven = isnumarray(L, 1);  /* 2.18.1 extension */
  luaL_typecheck(L, type == LUA_TTABLE || type == LUA_TSEQ || type == LUA_TREG || isnumarraygiven, 1, "table, sequence, register or numarray expected", type);
  if ((isf = !lua_isnoneornil(L, 2)))  /* is there a 2nd argument?  Agena 1.8.9 */
    luaL_checktype(L, 2, LUA_TFUNCTION);
  nargs = lua_gettop(L);
  if (isnumarraygiven) {  /* 2.18.1 extension */
    NumArray *ud = lua_touserdata(L, 1);
    if (ud->datatype != NADOUBLE)  /* 3.5.4*/
      luaL_error(L, "Error in " LUA_QS ": numarray must contain Agena numbers (C doubles).", "stats.issorted");
    n = ud->size;
  } else
    n = agn_nops(L, 1);  /* number of elements in structure */
  if (n < 2) {
    lua_pushfail(L);
    return 1;
  }
  if (nargs > 2) {
    luaL_error(L, "Error in " LUA_QS ": expected one or two arguments, got %d.", "stats.issorted", nargs);
  }
  switch (type) {
    case LUA_TTABLE: {
      x1 = agn_getinumber(L, 1, 1);
      if (isf) {  /* function passed as second argument ?, Agena 1.8.9 */
        size_t slots = 3;  /* 3.15.3 fix */
        for (i=2; i <= n; i++) {
          luaL_checkstack(L, slots, "too many arguments");  /* 2.14.0, 2.31.7/3.5.4 fix */
          lua_pushvalue(L, 2);  /* push function */
          lua_pushnumber(L, x1);
          lua_rawgeti(L, 1, i);
          x2 = lua_tonumber(L, -1);
          lua_call(L, slots - 1, 1);
          if (lua_isfalse(L, -1))  /* 2.36.2 optimisation */
            return 1;
          agn_poptop(L);  /* pop result of lua_call */
          x1 = x2;
        }
        lua_pushtrue(L);
      } else {
        for (i=2; i <= n; i++) {
          x2 = agn_getinumber(L, 1, i);
          if (x1 > x2) {
            lua_pushfalse(L);
            return 1;
          }
          x1 = x2;
        }
        lua_pushtrue(L);
      }
      break;
    }
    case LUA_TSEQ: {
      x1 = lua_seqrawgetinumber(L, 1, 1);
      if (isf) {  /* function passed as second argument ?, Agena 1.8.9 */
        size_t slots = 3;  /* 3.15.3 fix */
        for (i=2; i <= n; i++) {
          luaL_checkstack(L, slots, "too many arguments");  /* 2.14.0, 2.31.7/3.5.4 fix */
          lua_pushvalue(L, 2);  /* push function */
          lua_pushnumber(L, x1);
          lua_seqrawgeti(L, 1, i);
          x2 = lua_tonumber(L, -1);
          lua_call(L, slots - 1, 1);
          if (lua_isfalse(L, -1))  /* 2.36.2 optimisation */
            return 1;
          agn_poptop(L);  /* pop result of lua_call */
          x1 = x2;
        }
        lua_pushtrue(L);
      } else {
        for (i=2; i <= n; i++) {
          x2 = lua_seqrawgetinumber(L, 1, i);
          if (x1 > x2) {
            lua_pushfalse(L);
            return 1;
          }
          x1 = x2;
        }
        lua_pushtrue(L);
      }
      break;
    }
    case LUA_TREG: {  /* 3.18.8 */
      int rc;
      x1 = agn_regrawgetinumber(L, 1, 1, &rc);
      if (isf) {  /* function passed as second argument ? */
        size_t slots = 3;
        for (i=2; i <= n; i++) {
          luaL_checkstack(L, slots, "too many arguments");
          lua_pushvalue(L, 2);  /* push function */
          lua_pushnumber(L, x1);
          agn_regrawgeti(L, 1, i);
          x2 = lua_tonumber(L, -1);
          lua_call(L, slots - 1, 1);
          if (lua_isfalse(L, -1))
            return 1;
          agn_poptop(L);  /* pop result of lua_call */
          x1 = x2;
        }
        lua_pushtrue(L);
      } else {
        for (i=2; i <= n; i++) {
          x2 = agn_regrawgetinumber(L, 1, i, &rc);
          if (x1 > x2) {
            lua_pushfalse(L);
            return 1;
          }
          x1 = x2;
        }
        lua_pushtrue(L);
      }
      break;
    }
    case LUA_TUSERDATA: {  /* 2.18.1 extension */
      NumArray *ud = lua_touserdata(L, 1);
      lua_Number *arr = ud->data.n;
      x1 = arr[0];
      if (isf) {  /* function passed as second argument ?, Agena 1.8.9 */
        size_t slots = 3;  /* 3.15.3 fix */
        for (i=1; i < n; i++) {
          luaL_checkstack(L, slots, "too many arguments");  /* 2.14.0, 2.31.7/3.5.4 fix */
          lua_pushvalue(L, 2);  /* push function */
          lua_pushnumber(L, x1);
          x2 = arr[i];
          lua_pushnumber(L, x2);
          lua_call(L, slots - 1, 1);
          if (lua_isfalse(L, -1))  /* 2.36.2 optimisation */
            return 1;
          agn_poptop(L);  /* pop result of lua_call */
          x1 = x2;
        }
        lua_pushtrue(L);
      } else {
        for (i=1; i < n; i++) {
          x2 = arr[i];
          if (x1 > x2) {
            lua_pushfalse(L);
            return 1;
          }
          x1 = x2;
        }
        lua_pushtrue(L);
      }
      break;
    }
    default: lua_assert(0);  /* should not happen */
  }
  return 1;
}


/* `stats.deltalist` returns a structure of the deltas of neighbouring elements. It is an extended version of
   the TI-Nspire(tm) CX CAS version of `deltaList`. If obj is a table, you have to make sure that it does not
   contain holes. If it contains holes, apply tables.entries on obj. 12.03.2012 */

static int stats_deltalist (lua_State *L) {
  size_t i, n, isnumarraygiven;
  int type, applyabs;
  lua_Number x1, x2;
  type = lua_type(L, 1);
  isnumarraygiven = isnumarray(L, 1);  /* 2.18.1 extension */
  luaL_typecheck(L, type == LUA_TTABLE || type == LUA_TSEQ || type == LUA_TREG || isnumarraygiven, 1, "table, sequence, register or numarray expected", type);
  if (isnumarraygiven) {
    NumArray *ud = lua_touserdata(L, 1);
    if (ud->datatype != NADOUBLE)  /* 3.5.4*/
      luaL_error(L, "Error in " LUA_QS ": numarray must contain Agena numbers (C doubles).", "stats.deltalist");
    n = ud->size;
  } else
    n = agn_nops(L, 1);  /* number of elements in structure */
  applyabs = lua_toboolean(L, 2);  /* compute absolute differences ? */
  if (n < 2) {
    lua_pushfail(L);
    return 1;
  }
  switch (type) {
    case LUA_TTABLE: {
      lua_createtable(L, n - 1, 0);  /* Agena 1.7.10 */
      x1 = agn_getinumber(L, 1, 1);
      if (applyabs) {
        for (i=2; i <= n; i++) {
          x2 = agn_getinumber(L, 1, i);
          agn_setinumber(L, -1, i - 1, fabs(x2 - x1));
          x1 = x2;
        }
      } else {
        for (i=2; i <= n; i++) {
          x2 = agn_getinumber(L, 1, i);
          agn_setinumber(L, -1, i - 1, x2 - x1);
          x1 = x2;
        }
      }
      break;
    }
    case LUA_TSEQ: {
      agn_createseq(L, n - 1);  /* Agena 1.7.10 */
      x1 = lua_seqrawgetinumber(L, 1, 1);
      if (applyabs) {
        for (i=2; i <= n; i++) {
          x2 = lua_seqrawgetinumber(L, 1, i);
          agn_seqsetinumber(L, -1, i - 1, fabs(x2 - x1));
          x1 = x2;
        }
      } else {
        for (i=2; i <= n; i++) {
          x2 = lua_seqrawgetinumber(L, 1, i);
          agn_seqsetinumber(L, -1, i - 1, x2 - x1);
          x1 = x2;
        }
      }
      break;
    }
    case LUA_TREG: {  /* 3.18.8 */
      int rc;
      agn_createreg(L, n - 1);
      x1 = agn_regrawgetinumber(L, 1, 1, &rc);
      if (applyabs) {
        for (i=2; i <= n; i++) {
          x2 = agn_regrawgetinumber(L, 1, i, &rc);
          agn_regsetinumber(L, -1, i - 1, fabs(x2 - x1));
          x1 = x2;
        }
      } else {
        for (i=2; i <= n; i++) {
          x2 = agn_regrawgetinumber(L, 1, i, &rc);
          agn_regsetinumber(L, -1, i - 1, x2 - x1);
          x1 = x2;
        }
      }
      break;
    }
    case LUA_TUSERDATA: {  /* 2.18.1 extension */
      NumArray *ud = lua_touserdata(L, 1);
      lua_Number *arr = ud->data.n;
      agn_createseq(L, n - 1);  /* Agena 1.7.10 */
      x1 = arr[0];
      if (applyabs) {
        for (i=1; i < n; i++) {
          x2 = arr[i];
          agn_seqsetinumber(L, -1, i, fabs(x2 - x1));
          x1 = x2;
        }
      } else {
        for (i=1; i < n; i++) {
          x2 = arr[i];
          agn_seqsetinumber(L, -1, i, x2 - x1);
          x1 = x2;
        }
      }
      break;
    }
    default: lua_assert(0);  /* should not happen */
  }
  return 1;
}


/* stats.tovals: convert strings in a table or sequence to numbers. If a string cannot be converted,
   it is returned unchanged. Equivalent to
      stats.tovals := << (o) -> map( << (x) -> tonumber(x) >>, o) >>,
   but up to 40 % faster. */

static int tonumber (lua_State *L, int idx) {
  lua_Number n;
  int exception, type;
  luaL_checkany(L, idx);
  type = lua_type(L, idx);  /* 2.8.1 */
  if (type == LUA_TSTRING || type == LUA_TNUMBER) {
    n = agn_tonumberx(L, idx, &exception);
    if (exception) { /* conversion failed ? Check whether string contains a complex value */
      int cexception;
#ifndef PROPCMPLX
      agn_Complex c;
      c = agn_tocomplexx(L, idx, &cexception);
#else
      lua_Number c[2];
      agn_tocomplexx(L, idx, &cexception, c);
#endif
      if (cexception == 0)
#ifndef PROPCMPLX
        agn_createcomplex(L, c);
#else
        agn_createcomplex(L, c[0], c[1]);
#endif
      else
        lua_pushvalue(L, idx);
    }
    else
      lua_pushnumber(L, n);
  } else if (type == LUA_TCOMPLEX) {
    lua_pushvalue(L, idx);
  } else {
    lua_pushfail(L);
  }
  lua_remove(L, idx - 1);  /* remove original value, 2.8.1 */
  return 1;
}

static int stats_tovals (lua_State *L) {
  switch (lua_type(L, 1)) {
    case LUA_TTABLE: {
      /* very rough guess whether table is an array or dictionary */
      size_t n = lua_objlen(L, 1);
      if (n == 0)  /* assume table is a dictionary */
        lua_createtable(L, 0, agn_size(L, 1));
      else
        lua_createtable(L, n, 0);  /* lua_objlen not always returns correct results ! */
      lua_pushnil(L);
      while (lua_next(L, 1)) {  /* push the table key and the table value */
        tonumber(L, -1);
        lua_rawset2(L, -3);  /* store value in a table, leave key on stack */
      }
      break;
    }
    case LUA_TSET: {
      agn_createset(L, agn_ssize(L, 1));
      lua_pushnil(L);
      while (lua_usnext(L, 1)) {  /* push item twice */
        tonumber(L, -1);
        lua_srawset(L, -3);  /* store result to new set */
      }
      break;
    }
    case LUA_TSEQ: {
      size_t i, nops;
      nops = agn_seqsize(L, 1);
      agn_createseq(L, nops);
      for (i=0; i < nops; i++) {
        lua_seqrawgeti(L, 1, i + 1);  /* push item */
        tonumber(L, -1);
        lua_seqinsert(L, -2);  /* store result to new sequence */
      }
      break;
    }
    case LUA_TREG: {  /* 3.18.8 */
      size_t i, nops;
      nops = agn_regsize(L, 1);
      agn_createreg(L, nops);
      for (i=0; i < nops; i++) {
        agn_regrawgeti(L, 1, i + 1);  /* push item */
        tonumber(L, -1);
        lua_reginsert(L, -2);  /* store result to new register */
      }
      break;
    }
    default:
      luaL_error(L, "Error in " LUA_QS ": table, set, sequence or register expected, got %s.", "stats.tovals",
        luaL_typename(L, 1));
  }  /* end of switch */
  return 1;
}


/*

Credit: Agena's `stats.scale` is a port of the ALGOL 60 function REASCL, being part of the NUMAL package,
originally published by The Stichting Centrum Wiskunde & Informatica, Amsterdam, The Netherlands.

The Stichting Centrum Wiskunde & Informatica (Stichting CWI) (legal successor of Stichting Mathematisch
Centrum) at Amsterdam has granted permission to Paul McJones to attach the integral NUMAL library manual
to his software preservation project web page.

( URL: http://www.softwarepreservation.org/projects/ALGOL/applications/ )

It may be freely used. It may be copied provided that the name NUMAL and the attribution to the Stichting
CWI are retained.

Original ALGOL 60 credits to REASCL:

AUTHORS  : T.J. DEKKER, W. HOFFMANN.
CONTRIBUTORS: W. HOFFMANN, S.P.N. VAN KAMPEN.
INSTITUTE: MATHEMATICAL CENTRE.
RECEIVED: 731030.

BRIEF DESCRIPTION:

The procedure REASCL (scale) normalises the the numbers in a table or sequence in such a way that
an element of maximum absolute value equals 1. The normalised numbers are returned in a new
table or sequence, where the type of return is defined by the type of the input.

RUNNING TIME: PROPORTIONAL TO N.

LANGUAGE:   ALGOL 60.

METHOD AND PERFORMANCE: SEE REF [1].

REFERENCES:
     [1].T.J. DEKKER AND W. HOFFMANN.
         ALGOL 60 PROCEDURES IN NUMERICAL ALGEBRA, PART 2.
         MC TRACT 23, 1968, MATH. CENTR., AMSTERDAM.

SOURCE TEXT(S):

 CODE 34183;
     COMMENT MCA 2413;
     PROCEDURE REASCL(A, N, N1, N2); VALUE N, N1, N2;
     INTEGER N, N1, N2; ARRAY A;
     BEGIN INTEGER I, J; REAL S;
         FOR J:= N1 STEP 1 UNTIL N2 DO
         BEGIN S:= 0;
             FOR I:= 1 STEP 1 UNTIL N DO
             IF ABS(A[I,J]) > ABS(S) THEN S:= A[I,J];
             IF S ^= 0 THEN
             FOR I:= 1 STEP 1 UNTIL N DO A[I,J]:= A[I,J] / S
         END
     END REASCL;
         EOP */

static int stats_scale (lua_State *L) {  /* optimised 2.2.0 RC 2, May 20, 2014, tuned 4.9.3 */
  int type, nooption, c;
  size_t minpos, maxpos;
  lua_Number min, max, x;
  max = min = 0.0;
  type = lua_type(L, 1);
  luaL_typecheck(L, type == LUA_TTABLE || type == LUA_TSEQ || type == LUA_TREG || type == LUA_TUSERDATA, 1, "table, sequence, register or numarray expected", type);
  nooption = lua_isnoneornil(L, 2);
  switch (type) {
    case LUA_TTABLE: {
      if (nooption) {
        if (!agn_minmax(L, 1, 1, &min, &minpos, &max, &maxpos, &c) || (max == 0.0)) {
          lua_pushfail(L);
          return 1;
        }
        lua_createtable(L, c, 0);
        c = 0;
        lua_pushnil(L);
        while (lua_next(L, 1)) {
          agn_setinumber(L, -3, ++c, agn_tonumber(L, -1)/max);
          agn_poptop(L);  /* pop the number */
        }
      } else {  /* normalise to the range [0, 1], 2.2.1 */
        if (!agn_minmax(L, 1, 0, &min, &minpos, &max, &maxpos, &c) || (fabs(max - min) == 0.0)) {
          lua_pushfail(L);
          return 1;
        }
        lua_createtable(L, c, 0);
        c = 0;
        lua_pushnil(L);
        while (lua_next(L, 1)) {
          agn_setinumber(L, -3, ++c, (agn_tonumber(L, -1) - min)/(max - min));
          agn_poptop(L);  /* pop the number */
        }
      }
      break;
    }
    case LUA_TSEQ: {
      size_t i;
      if (nooption) {
        if (!agn_minmax(L, 1, 1, &min, &minpos, &max, &maxpos, &c) || (max == 0.0)) {
          lua_pushfail(L);
          return 1;
        }
        agn_createseq(L, c);
        for (i=1; i <= c; i++) {
          agn_seqsetinumber(L, -1, i, lua_seqrawgetinumber(L, 1, i)/max);  /* internal division by zero cannot happen here */
        }
      } else {  /* normalise to the range [0, 1], 2.2.1 */
        if (!agn_minmax(L, 1, 0, &min, &minpos, &max, &maxpos, &c) || (fabs(max - min) == 0.0)) {
          lua_pushfail(L);
          return 1;
        }
        agn_createseq(L, c);
        for (i=1; i <= c; i++) {
          agn_seqsetinumber(L, -1, i, (lua_seqrawgetinumber(L, 1, i) - min)/(max - min));
        }
      }
      break;
    }
    case LUA_TREG: {  /* 3.18.8 */
      size_t i;
      if (nooption) {
        if (!agn_minmax(L, 1, 1, &min, &minpos, &max, &maxpos, &c) || (max == 0.0)) {
          lua_pushfail(L);
          return 1;
        }
        agn_createreg(L, c);
        for (i=1; i <= c; i++) {
          agn_regsetinumber(L, -1, i, agn_reggetinumber(L, 1, i)/max);  /* internal dicion by zero cannot happem here */
        }
      } else {  /* normalise to the range [0, 1] */
        if (!agn_minmax(L, 1, 0, &min, &minpos, &max, &maxpos, &c) || (fabs(max - min) == 0.0)) {
          lua_pushfail(L);
          return 1;
        }
        agn_createreg(L, c);
        for (i=1; i <= c; i++) {
          agn_regsetinumber(L, -1, i, (agn_reggetinumber(L, 1, i) - min)/(max - min));
        }
      }
      break;
    }
    case LUA_TUSERDATA: {  /* 3.5.4 */
      size_t i, ii;
      lua_Number *a;
      NumArray *b = NULL;
      a = agnL_tonumarray(L, 1, &ii, "stats.scale", 0, 0);
      if (a == NULL) return 1;  /* issue the error, 3.18.8 */
      if (ii == 0) {
        lua_pushfail(L);
        return 1;
      }
      if (nooption) {
        for (i=0; i < ii; i++) {
          x = a[i];
          if (fabs(x) > fabs(max)) max = x;
        }
        if (max == 0) {
          lua_pushfail(L);
          return 1;
        }
        numarray_createdouble(L, b, ii);
        for (i=0; i < ii; i++) {
          b->data.n[i] = a[i]/max;  /* internal dicion by zero cannot happem here */
        }
      } else {  /* normalise to the range [0, 1] */
        min = HUGE_VAL;
        max = -HUGE_VAL;
        for (i=0; i < ii; i++) {
          x = a[i];
          if (x > max) max = x;
          if (x < min) min = x;
        }
        if (fabs(max - min) == 0) {
          lua_pushfail(L);
          return 1;
        }
        numarray_createdouble(L, b, ii);
        for (i=0; i < ii; i++) {
          b->data.n[i] = (a[i] - min)/(max - min);
        }
      }
      /* nothing to be freed for we requested just a reference with the call to agnL_tonumarray */
      break;
    }
    default: lua_assert(0);  /* should not happen */
  }
  return 1;
}


/* stats.colnorm: Returns the largest absolute value of the numbers in a table or sequence, and the original
   value with the largest absolute magnitude. If the structure consists entirely of one or more 'undefined's,
   then the function returns the value undefined twice. If the structure is empty, 'fail' is returned.

   This is an extended version of the TI-Nspire's colNorm function when passed a one-dimensional matrix. See also:
   `stats.scale`, `stats.rownorm`.

   Agena 1.8.2, 03.10.2012 */

static int stats_colnorm (lua_State *L) {
  int type, onlyundefs, isnumarraygiven;
  size_t n;
  lua_Number s, x;
  isnumarraygiven = isnumarray(L, 1);  /* 2.18.1 extension */
  type = lua_type(L, 1);  /* 2.5.0 */
  luaL_typecheck(L, type == LUA_TTABLE || type == LUA_TSEQ || type == LUA_TREG || isnumarraygiven, 1, "table, sequence, register or numarray expected", type);
  if (isnumarraygiven) {  /* 2.18.1 extension */
    NumArray *ud = lua_touserdata(L, 1);
    if (ud->datatype != NADOUBLE)  /* 3.5.4*/
      luaL_error(L, "Error in " LUA_QS ": numarray must contain Agena numbers (C doubles).", "stats.colnorm");
    n = ud->size;
  } else
    n = agn_nops(L, 1);  /* number of elements in structure */
  if (n == 0) {
    lua_pushfail(L);
    return 1;
  }
  s = 0;  /* largest magnitude */
  onlyundefs = 1;   /* structure contains `undefined` only */
  switch (type) {
    case LUA_TTABLE: {
      lua_pushnil(L);
      while (lua_next(L, 1)) {
        x = agn_checknumber(L, -1);
        if (!tools_isnan(x)) {  /* 2.10.1, changed to isnan instead of tools_isnan */
          onlyundefs = 0;
          if (fabs(x) > fabs(s)) s = x;
        }
        agn_poptop(L);  /* pop the number */
      }
      break;
    }
    case LUA_TSEQ: {
      size_t i;
      for (i=1; i <= n; i++) {  /* 2.18.1 improvement */
        x = lua_seqrawgetinumber(L, 1, i);
        if (!tools_isnan(x)) {  /* 2.10.1, changed to isnan instead of tools_isnan */
          onlyundefs = 0;
          if (fabs(x) > fabs(s)) s = x;
        }
      }
      break;
    }
    case LUA_TREG: {  /* 3.18.8 */
      int rc;
      size_t i;
      for (i=1; i <= n; i++) {  /* 2.18.1 improvement */
        x = agn_regrawgetinumber(L, 1, i, &rc);
        if (!tools_isnan(x)) {  /* 2.10.1, changed to isnan instead of tools_isnan */
          onlyundefs = 0;
          if (fabs(x) > fabs(s)) s = x;
        }
      }
      break;
    }
    case LUA_TUSERDATA: {  /* 2.18.1 extension */
      size_t i;
      NumArray *ud = lua_touserdata(L, 1);
      lua_Number *arr = ud->data.n;
      for (i=0; i < n; i++) {
        x = arr[i];
        if (!tools_isnan(x)) {  /* 2.10.1, changed to isnan instead of tools_isnan */
          onlyundefs = 0;
          if (fabs(x) > fabs(s)) s = x;
        }
      }
      break;
    }
    default: lua_assert(0);  /* should not happen */
  }
  luaL_checkstack(L, 2, "not enough stack space");  /* 3.15.4 fix */
  if (onlyundefs) {
    lua_pushundefined(L); lua_pushundefined(L);
  } else {
    lua_pushnumber(L, fabs(s)); lua_pushnumber(L, s);
  }
  return 2;
}


/* stats.rownorm: Returns the sum of the absolute values of the numbers in a table or sequence. If the structure
   consists entirely of one or more 'undefined's, then the function returns 'undefined'. If the structure is empty,
   'fail' is returned.

   This is a version of the TI-Nspire's rowNorm function when passed a one-dimensional matrix. See also:
   `stats.scale`, `stats.colnorm`.

   Agena 1.8.2, 04.10.2012 */

static int stats_rownorm (lua_State *L) {
  int type, onlyundefs, isnumarraygiven;
  size_t n;
  volatile double s, x, cs, ccs;
  s = cs = ccs = 0;  /* largest magnitude */
  onlyundefs = 1;   /* structure contains `undefined` only */
  isnumarraygiven = isnumarray(L, 1);
  type = lua_type(L, 1);
  luaL_typecheck(L, type == LUA_TTABLE || type == LUA_TSEQ || type == LUA_TREG || isnumarraygiven, 1, "table, sequence, register or numarray expected", type);
  if (isnumarraygiven) {  /* 2.18.1 extension */
    NumArray *ud = lua_touserdata(L, 1);
    if (ud->datatype != NADOUBLE)  /* 3.5.4*/
      luaL_error(L, "Error in " LUA_QS ": numarray must contain Agena numbers (C doubles).", "stats.rownorm");
    n = ud->size;
  } else
    n = agn_nops(L, 1);  /* number of elements in structure */
  if (n == 0) {
    lua_pushfail(L);
    return 1;
  }
  switch (type) {
    case LUA_TTABLE: {
      lua_pushnil(L);
      while (lua_next(L, 1)) {
        x = agn_checknumber(L, -1);
        if (!tools_isnan(x)) {  /* 2.10.1, changed to isnan instead of tools_isnan */
          onlyundefs = 0;
          s = tools_kbadd(s, fabs(x), &cs, &ccs);  /* 3.7.2 change to Kahan-Babuska summation */
        }
        agn_poptop(L);  /* pop the number */
      }
      break;
    }
    case LUA_TSEQ: {
      size_t i;
      for (i=1; i <= n; i++) {  /* 2.18.1 improvement */
        x = lua_seqrawgetinumber(L, 1, i);
        if (!tools_isnan(x)) {  /* 2.10.1, changed to isnan instead of tools_isnan */
          onlyundefs = 0;
          s = tools_kbadd(s, fabs(x), &cs, &ccs);  /* 3.7.2 change to Kahan-Babuska summation */
        }
      }
      break;
    }
    case LUA_TREG: {  /* 3.18.8 */
      int rc;
      size_t i;
      for (i=1; i <= n; i++) {
        x = agn_regrawgetinumber(L, 1, i, &rc);
        if (!tools_isnan(x)) {
          onlyundefs = 0;
          s = tools_kbadd(s, fabs(x), &cs, &ccs);
        }
      }
      break;
    }
    case LUA_TUSERDATA: {  /* 2.18.1 extension */
      size_t i;
      NumArray *ud = lua_touserdata(L, 1);
      lua_Number *arr = ud->data.n;
      for (i=0; i < n; i++) {
        x = arr[i];
        if (!tools_isnan(x)) {  /* 2.10.1, changed to isnan instead of tools_isnan */
          onlyundefs = 0;
          s = tools_kbadd(s, fabs(x), &cs, &ccs);  /* 3.7.2 change to Kahan-Babuska summation */
        }
      }
      break;
    }
    default: lua_assert(0);  /* should not happen */
  }
  if (onlyundefs)
    lua_pushundefined(L);
  else
    lua_pushnumber(L, s + cs + ccs);  /* 3.7.2 change to Kahan-Babuska summation */
  return 1;
}


/* Computes the probability density function for the normal distribution at the numeric value x. The defaults are
   mu = 0, with standard deviation sigma = 1; 1.10.6 */

static int stats_pdf (lua_State *L) {
  lua_Number x, mu, si, p, q;
  x = agn_checknumber(L, 1);
  mu = agnL_optnumber(L, 2, 0);
  si = agnL_optpositive(L, 3, 1);  /* 2.21.5 change */
  p = 1/(si*sqrt(PI2));
  q = -(x - mu)*(x - mu)/(2*si*si);
  lua_pushnumber(L, p * sun_pow(EXP1, q, 0));  /* 2.14.13 change from pow to sun_pow */
  return 1;
}


static int stats_ndf (lua_State *L) {
  lua_Number si = agnL_optpositive(L, 1, 1);  /* 2.5.0, 2.21.5 change */
  lua_pushnumber(L, 1/(si*sqrt(PI2)));
  return 1;
}


static int stats_nde (lua_State *L) {
  lua_Number x, mu, si;
  x = agn_checknumber(L, 1);
  mu = agnL_optnumber(L, 2, 0);
  si = agnL_optpositive(L, 3, 1);  /* 2.21.5 change */
  lua_pushnumber(L, sun_exp(-(x - mu)*(x - mu)/(2*si*si)));  /* 2.14.13 change from exp to sun_exp */
  return 1;
}


static int checkstructuresxx (lua_State *L, int *type, size_t *n, int *k, int *p, int *scientific,
    int *left, int *right, int outofrange, const char *procname) {  /* 2.1 RC 1 */
  luaL_checkany(L, 1);
  *type = lua_type(L, 1);
  luaL_typecheck(L, *type == LUA_TTABLE || *type == LUA_TSEQ || *type == LUA_TREG, 1, "table, sequence or register expected", *type);
  luaL_typecheck(L, lua_type(L, 2) == LUA_TNUMBER, 2, "posint expected", lua_type(L, 2));
  luaL_typecheck(L, lua_type(L, 3) == LUA_TNUMBER, 3, "posint expected", lua_type(L, 3));
  *n = agn_nops(L, 1);
  if (*n < 1) {
    lua_pushfail(L);
    return 1;
  }
  *k = lua_tonumber(L, 2);  /* index of sample point */
  if (*k < 1) luaL_error(L, "Error in " LUA_QS ": second argument must be a posint.", procname);
  else if (*k > *n) luaL_error(L, "Error in " LUA_QS ": second argument too large.", procname);
  *p = lua_tonumber(L, 3);  /* period */
  if (*p < 2) luaL_error(L, "Error in " LUA_QS ": third argument must be greater than 1.", procname);
  else if (*n < *p) luaL_error(L, "Error in " LUA_QS ": too few elements in structure.", procname);
  *scientific = agnL_optboolean(L, 4, 0);  /* sample point at centre ? */
  if (*scientific) {  /* k is in the center of a period, period must be odd */
    lua_Number d, id;
    d = *p*0.5;  /* 2.17.7 tweak */
    id = sun_trunc(d);
    if (d == id) luaL_error(L, "Error in " LUA_QS ": third argument must be an odd number.", procname);
    *left = *k - id;
    *right = *k + id;
  } else {
    *left = *k - *p + 1;
    *right = *k;
  }
  if (outofrange && (*left < 1 || *right > *n)) {
    lua_pushundefined(L);
    return 1;
  }
  return 0;
}

static int stats_sma (lua_State *L) {  /* based on stats.amean */
  size_t ii, n;
  int left, right, type, scientific, k, p;  /* 1.12.3 */
  lua_Number *a, s;
  type = k = p = n = scientific = 0;  /* avoid compiler warnings */
  if (checkstructuresxx(L, &type, &n, &k, &p, &scientific, &left, &right, 1, "stats.sma")) return 1;
  agn_createarray(a, p, "stats.sma");
  ii = 0;
  if (agnL_fillarray(L, 1, type, a, &ii, left, right, 0)) {
    xfree(a);  /* 1.12.4 */
    luaL_error(L, "Error in " LUA_QS ": structure includes `undefined`.", "stats.sma");
  }
  s = KahanBabuskaMeanNumArray(a, ii);  /* 2.4.3 improvement */
  xfree(a);
  lua_pushnumber(L, s);
  return 1;
}


static int stats_smm (lua_State *L) {  /* based on stats.sma */
  size_t ii, n;
  int left, right, type, scientific, k, p;  /* 1.12.3 */
  lua_Number *a;
  type = k = p = n = scientific = left = right = ii = 0;  /* avoid compiler warnings */
  if (checkstructuresxx(L, &type, &n, &k, &p, &scientific, &left, &right, 1, "stats.smm")) return 1;
  agn_createarray(a, p, "stats.smm");
  if (agnL_fillarray(L, 1, type, a, &ii, left, right, 0)) {
    xfree(a);  /* 1.12.4 */
    luaL_error(L, "Error in " LUA_QS ": structure includes `undefined`.", "stats.smm");
  }
  /* it does not make much sense to check for small unsorted ranges */
  /* tools_dintrosort(a, 0, ii - 1, 0, 2*sun_log2(ii)); */ /* 2.8.4 speed-up */
  klib_dintrosort(a, ii);  /* 7.7.4 10 % speed-up */
  lua_pushnumber(L, ii&1 ? tools_kth_smallest(a, ii, ii/2) :
    (tools_kth_smallest(a, ii, ii/2 - 1) + tools_kth_smallest(a, ii, ii/2))*0.5);  /* 2.17.7 tweak */
  xfree(a);
  return 1;
}


static int gsmm_iterator (lua_State *L) {
  lua_Number *a;
  size_t p, n, ii;
  int k, structure, type;
  k = lua_tonumber(L, lua_upvalueindex(2));  /* index of first sample point */
  p = lua_tonumber(L, lua_upvalueindex(3));  /* period */
  n = lua_tonumber(L, lua_upvalueindex(4));  /* size of structure */
  if (k < 1 || (k + p - 1 > n && k <= n)) {
    lua_pushnumber(L, k + 1);
    lua_replace(L, lua_upvalueindex(2));  /* update index */
    lua_pushundefined(L);
    return 1;
  } else if (k > n) {
    lua_pushnil(L);
    return 1;
  }
  agn_createarray(a, p, "stats.gsmm");
  ii = 0;
  structure = lua_upvalueindex(1);
  type = lua_type(L, structure);
  if (agnL_fillarray(L, structure, type, a, &ii, k, k + p - 1, 0)) {
    xfree(a);  /* 1.12.4 */
    luaL_error(L, "Error in " LUA_QS ": structure includes `undefined`.", "stats.gsmm");
  }
  /* It does not make much sense to search for small unsorted ranges */
  /* tools_dintrosort(a, 0, ii - 1, 0, 2*sun_log2(ii)); */ /* 2.8.4 speed-up */
  klib_dintrosort(a, ii);  /* 7.7.4 10 % speed-up */
  lua_pushnumber(L, k + 1);
  lua_replace(L, lua_upvalueindex(2));  /* update index k */
  lua_pushnumber(L, ii&1 ? tools_kth_smallest(a, ii, ii/2) :
    (tools_kth_smallest(a, ii, ii/2 - 1) + tools_kth_smallest(a, ii, ii/2))*0.5);  /* 2.17.7 tweak */
  xfree(a);
  return 1;  /* return median */
}

static int stats_gsmm (lua_State *L) {
  int left, right, type, k, p, scientific;
  size_t n;
  type = k = p = n = scientific = left = right = 0;  /* avoid compiler warnings */
  if (checkstructuresxx(L, &type, &n, &k, &p, &scientific, &left, &right, 0, "stats.gsmm")) return 1;
  luaL_checkstack(L, 4, "not enough stack space");  /* 3.18.4 fix */
  lua_pushvalue(L, 1);      /* push structure, converting each number in the structure to an upvalue would speed up the function
                               even further, but in this case the number of samples would be limited to the maximum stack size. */
  lua_pushnumber(L, left);  /* push first sample point */
  lua_pushnumber(L, p);     /* push period */
  lua_pushnumber(L, n);     /* push size of structure */
  lua_pushcclosure(L, &gsmm_iterator, 4);  /* converts the values on the stack into upvalues and pops these three values from the stack */
  return 1;
}


static int gsma_iterator (lua_State *L) {
  lua_Number *a, s;
  size_t p, n, ii;
  int k, structure, type;
  k = lua_tonumber(L, lua_upvalueindex(2));
  p = lua_tonumber(L, lua_upvalueindex(3));
  n = lua_tonumber(L, lua_upvalueindex(4));
  if (k < 1 || (k + p - 1 > n && k <= n)) {
    lua_pushnumber(L, k + 1);
    lua_replace(L, lua_upvalueindex(2));  /* update index */
    lua_pushundefined(L);
    return 1;
  } else if (k > n) {
    lua_pushnil(L);
    return 1;
  }
  agn_createarray(a, p, "stats.gsma");
  ii = 0;
  structure = lua_upvalueindex(1);
  type = lua_type(L, structure);
  if (agnL_fillarray(L, structure, type, a, &ii, k, k + p - 1, 0)) {
    xfree(a);  /* 1.12.4 */
    luaL_error(L, "Error in " LUA_QS ": structure includes `undefined`.", "stats.gsma");
  }
  s = KahanBabuskaMeanNumArray(a, ii);  /* 2.4.3 improvement, we deliberately do not remove the first average and add the last from the sum */
  lua_pushnumber(L, k + 1);
  lua_replace(L, lua_upvalueindex(2));  /* update index k */
  lua_pushnumber(L, s);
  xfree(a);
  return 1;  /* return moving mean */
}

static int stats_gsma (lua_State *L) {
  int left, right, type, scientific, k, p;
  size_t n;
  type = k = p = n = scientific = left = right = 0;  /* avoid compiler warnings */
  if (checkstructuresxx(L, &type, &n, &k, &p, &scientific, &left, &right, 0, "stats.gsma")) return 1;
  luaL_checkstack(L, 4, "not enough stack space");  /* 3.18.4 fix */
  lua_pushvalue(L, 1);      /* push structure, converting each number in the structure to an upvalue would speed up the function
                               even further, but in this case the number of samples would be limited to the maximum stack size. */
  lua_pushnumber(L, left);  /* push first sample point */
  lua_pushnumber(L, p);     /* push period */
  lua_pushnumber(L, n);     /* push size of structure */
  lua_pushcclosure(L, &gsma_iterator, 4);  /* converts the values on the stack into upvalues and pops these three values from the stack */
  return 1;
}


/* stats.ema (obj, k, alpha [, mode [, y0star]])

Computes the exponential moving average of a table or sequence obj up to and including its k-th element.

The smoothing factor alpha is a rational number in the range [0, 1].

The function supports two algorithms: If mode is 1 (the default), then the algorithm

      r := alpha * obj[k];
      s := 1 - alpha;
      for i from k - 1 to 1 by -1 do
         r := r + alpha * s ^ i * obj[i];
      od;
      r := r + s ^ k * y0star;

is used to compute the result r. In mode 1, you can pass an explicit first estimate y0*, otherwise
the first value y0* is equal to the sample moving average of obj.

If mode is 2, then the formula

      r := obj[k];
      for i from k - 1 to 1 by -1 do
         r := r + alpha * (obj[i] - r)
      od;

is applied.

The result is a number. */

static int checkstructureema (lua_State *L, int *type, size_t *n, int *k, lua_Number *alpha, const char *procname) {  /* 2.1 RC 1
  nok: no `k` value given;
  !!! do not delete the nok parameter for the future implementation of stats.fema !!! */
  luaL_checkany(L, 1);
  *type = lua_type(L, 1);
  luaL_typecheck(L, *type == LUA_TTABLE || *type == LUA_TSEQ || *type == LUA_TREG, 1, "table, sequence or register expected", *type);
  luaL_typecheck(L, lua_type(L, 2) == LUA_TNUMBER, 2, "posint expected", lua_type(L, 2));
  *n = agn_nops(L, 1);
  if (*n < 1) {
    lua_pushfail(L);
    return 1;
  }
  *k = lua_tonumber(L, 2);  /* index of sample point */
  if (*k < 1) luaL_error(L, "Error in " LUA_QS ": second argument must be a posint.", procname);
  else if (*k > *n) luaL_error(L, "Error in " LUA_QS ": second argument too large.", procname);
  *alpha = agn_checknumber(L, 3);  /* smoothing factor */
  if (*alpha < 0 || *alpha > 1)
    luaL_error(L, "Error in " LUA_QS ": smoothing factor must be in the range [0, 1].", procname);
  return 0;
}


static int aux_ema (lua_State *L, lua_Number *a,  size_t ii, lua_Number alpha, int mode, lua_Number y0star, lua_Number *r) {  /* 2.1 RC 1 */
  size_t i;
  switch (mode) {
    case 1: {  /* see http://de.wikipedia.org/wiki/Exponentielle_Gl%C3%A4ttung */
      lua_Number s;
      if (tools_isnan(y0star)) y0star = KahanBabuskaMeanNumArray(a, ii);  /* 2.4.3 improvement */
      *r = alpha * a[0];
      s = 1 - alpha;
      for (i=1; i < ii; i++) *r += alpha * tools_intpow(s, i) * a[i];
      *r += tools_intpow(s, ii) * y0star;
      break;
    }
    case 2: {  /* see: http://stackoverflow.com/questions/7947352/exponential-moving-average */
      *r = a[0];
      for (i=1; i < ii; i++) *r += alpha * (a[i] - *r);
      break;
    }
    default:
      return 1;
  }
  return 0;
}

static int stats_ema (lua_State *L) {  /* 2.1 RC 1 */
  size_t ii, n;
  int type, k, mode;
  lua_Number *a, alpha, r, y0star;
  ii = type = k = n = r = 0;  /* avoid compiler warnings */
  if (checkstructureema(L, &type, &n, &k, &alpha, "stats.ema")) return 1;
  mode = agnL_optinteger(L, 4, 1);
  y0star = agnL_optnumber(L, 5, AGN_NAN);
  agn_createarray(a, k, "stats.ema");
  /* fill array in the opposite direction to allow hardware prefetching */
  if (agnL_fillarray(L, 1, type, a, &ii, 1, k, 1)) {
    xfree(a);
    luaL_error(L, "Error in " LUA_QS ": structure includes `undefined`.", "stats.ema");
  }
  if (aux_ema(L, a, ii, alpha, mode, y0star, &r)) {
    xfree(a);
    luaL_error(L, "Error in " LUA_QS ": unknown mode %d.", "stats.ema", mode);
  }
  xfree(a);
  lua_pushnumber(L, r);
  return 1;
}


static int gema_iterator (lua_State *L) {  /* 2.1 RC 1 */
  lua_Number *a;
  size_t n, ii;
  int k, structure, type, mode;
  lua_Number alpha, y0star, r;
  r = 0;
  if (lua_gettop(L) != 0)
    luaL_error(L, "Error in " LUA_QS ": iterator requires no arguments.", "stats.gema");
  k = lua_tonumber(L, lua_upvalueindex(2));
  n = lua_tonumber(L, lua_upvalueindex(3));
  alpha = lua_tonumber(L, lua_upvalueindex(4));
  mode = lua_tointeger(L, lua_upvalueindex(5));
  y0star = lua_tonumber(L, lua_upvalueindex(6));
  if (k < 1 || k > n) {
    lua_pushnil(L);
    return 1;
  }
  agn_createarray(a, k, "stats.gema");
  ii = 0;
  structure = lua_upvalueindex(1);
  type = lua_type(L, structure);
  if (agnL_fillarray(L, structure, type, a, &ii, 1, k, 1)) {
    xfree(a);  /* 1.12.4 */
    luaL_error(L, "Error in " LUA_QS ": structure includes `undefined`.", "stats.gema");
  }
  if (aux_ema(L, a, ii, alpha, mode, y0star, &r)) {
    xfree(a);
    luaL_error(L, "Error in " LUA_QS ": unknown mode %d.", "stats.gema", mode);
  }
  lua_pushnumber(L, k + 1);
  lua_replace(L, lua_upvalueindex(2));  /* update index k */
  lua_pushnumber(L, r);
  xfree(a);
  return 1;  /* return median */
}

/*

stats.gema (obj, alpha [, mode [, y0*]])

Like stats.ema, but returns a function that, each time it is called, returns the exponential
moving average, starting with sample obj[1], and progressing with sample obj[2], obj[3], etc.
with subsequent calls. It return `null` if there are no more samples in obj. It is much faster
than `stats.ema` with large observations.

The smoothing factor alpha is a rational number in the range [0, 1].

The function supports two algorithms: If mode is 1 (the default), then the algorithm

   ...

is used to compute the result. In mode 1, you can pass an explicit first estimate y0*, otherwise
the first value y0* is equal to the sample moving average of obj.

If mode is 2, then the formula

   ...

is applied to the period.

The result is a number. */

static int stats_gema (lua_State *L) {  /* 2.1 RC 1 */
  int type, k, mode;
  lua_Number alpha, y0star;
  size_t n;
  type = k = n = 0;  /* avoid compiler warnings */
  if (checkstructureema(L, &type, &n, &k, &alpha, "stats.gema")) return 1;
  mode = agnL_optinteger(L, 4, 1);
  y0star = agnL_optnumber(L, 5, AGN_NAN);
  luaL_checkstack(L, 6, "not enough stack space");  /* 3.18.4 fix */
  lua_pushvalue(L, 1);        /* push structure, converting each number in the structure to an upvalue would speed up the function
                                 even further, but in this case the number of samples would be limited to the maximum stack size. */
  lua_pushnumber(L, k);       /* push first sample point */
  lua_pushnumber(L, n);       /* push _total_ size of structure */
  lua_pushnumber(L, alpha);   /* push smoothing factor alpha */
  lua_pushnumber(L, mode);    /* push mode */
  lua_pushnumber(L, y0star);  /* push y0star */
  lua_pushcclosure(L, &gema_iterator, 6);  /* converts the values on the stack into upvalues and pops these three values from the stack */
  return 1;
}


/* returns all elements in a table or sequence obj from the p-th percentile rank up but not including
   the q-th percentile rank. p and q must be positive integers in the range (0 .. [100. If p and q
   are not given, p is set to 25, and q to 75. If q is not given, it is set to 100 - p. The type of
   return is determined by the type of obj. */

#define FLOOR(x) sun_floor((x) + 0.5)

static int stats_prange (lua_State *L) {  /* 1.12.9 */
  size_t i, nargs, p, q, c, rankp, rankq, ii;
  int type;
  lua_Number x1, x2, *a;
  luaL_checkany(L, 1);
  type = lua_type(L, 1);
  luaL_typecheck(L, type == LUA_TTABLE || type == LUA_TSEQ || type == LUA_TREG || isnumarray(L, 1), 1, "table, sequence, register or numarray expected", type);
  nargs = lua_gettop(L);
  if (nargs > 1) {
    p = agn_checkinteger(L, 2);
    if (p > 99)  /* 7.9.5 change, no check for negative value any longer */
      luaL_error(L, "Error in " LUA_QS ": second argument must be in the range [0, 100).", "stats.prange");
  } else
    p = 25;
  if (nargs > 2) {
    q = agn_checkinteger(L, 3);
    if (q > 99)  /* 7.9.5 change, no check for negative value any longer */
      luaL_error(L, "Error in " LUA_QS ": third argument must be in the range [0, 100).", "stats.prange");
  } else
    q = 100 - p;
  if (p > q)
    luaL_error(L, "Error in " LUA_QS ": second argument must be less than third argument.", "stats.prange");
  /* create an array and fill it */
  a = agnL_tonumarray(L, 1, &ii, "stats.prange", 1, 0);  /* 2.18.1 extension */
  if (a == NULL) return 1;  /* issue the error, 2.4.4 fix */
  if (ii < 2)
    lua_pushfail(L);
  else {
    x1 = a[0];
    for (i=1; i < ii; i++) {  /* 2.4.3 fix */
      x2 = a[i];
      if (x1 > x2) {  /* array unsorted ? -> sort it in ascending order */
        /* tools_dintrosort(a, 0, ii - 1, 0, 2*sun_log2(ii)); */ /* 2.8.4 speed-up */
        klib_dintrosort(a, ii);  /* 7.7.4 10 % speed-up */
        break;
      }
      x1 = x2;
    }
    rankp = FLOOR((lua_Number)p/100*(ii + 1));  /* 2.4.2 change to NIST standard */
    rankq = FLOOR((lua_Number)q/100*(ii + 1));  /* 2.4.2 change to NIST standard */
    c = 1 - (type == LUA_TUSERDATA);
    switch (type) {
      /* #slots: rankq - 1 - rankp != formerly ii*(q - p)/100 + 1, 4.11.7 */
      case LUA_TTABLE: {
        lua_createtable(L, rankq - 1 - rankp, 0);
        for (i=rankp; i < rankq - 1; i++)  /* 2.4.3 fix */
          agn_setinumber(L, -1, c++, a[i]);
        break;
      }
      case LUA_TSEQ: {
        agn_createseq(L, rankq - 1 - rankp);
        for (i=rankp; i < rankq - 1; i++)  /* 2.4.3 fix */
          agn_seqsetinumber(L, -1, c++, a[i]);
        break;
      }
      case LUA_TREG: {  /* new 4.11.7 */
        agn_createreg(L, rankq - 1 - rankp);
        for (i=rankp; i < rankq - 1; i++)
          agn_regsetinumber(L, -1, c++, a[i]);
        break;
      }
      case LUA_TUSERDATA: {  /* new 4.11.7 */
        NumArray *b = NULL;
        numarray_createdouble(L, b, rankq - 1 - rankp);
        for (i=rankp; i < rankq - 1; i++)
          b->data.n[c++] = a[i];
        break;
      }
      default: lua_assert(0);  /* should not happen */
    }
  }
  xfree(a);  /* for all structures, free a */
  return 1;
}


/* Returns the arithmetic mean of the interquartile range of a distribution using Kahan-Babuska round-off error prevention.
   If a distribution is unsorted, the function automatically sorts it non-destructively, and any non-numeric
   observations are converted to zeros. The return is a number. The interquartile range comprises all observations
   that reside between the first and third quartiles. */

static int stats_iqmean (lua_State *L) {  /* 2.4.2 */
  size_t i, ii, p, q, c, rankp, rankq;
  lua_Number x1, x2, *a;
  luaL_checkany(L, 1);
  a = agnL_tonumarray(L, 1, &ii, "stats.iqmean", 1, 0);  /* 2.4.4 change, 2.18.1 extension */
  if (a == NULL) return 1;  /* issue the error */
  if (ii < 2)
    lua_pushfail(L);
  else {
    p = 25; q = 100 - p;
    x1 = a[0];
    for (i=1; i < ii; i++) {
      x2 = a[i];
      if (x1 > x2) {  /* array unsorted ? -> sort it in ascending order */
        /* tools_dintrosort(a, 0, ii - 1, 0, 2*sun_log2(ii)); */ /* 2.8.4 speed-up */
        klib_dintrosort(a, ii);  /* 7.7.4 10 % speed-up */
        break;
      }
      x1 = x2;
    }
    rankp = FLOOR((lua_Number)p/100*(ii + 1));  /* 2.4.2 change to NIST standard */
    rankq = FLOOR((lua_Number)q/100*(ii + 1));  /* 2.4.2 change to NIST standard */
    c = 0;
    /* shift interquartile range to the beginning of a */
    for (i=rankp - 1; i < rankq - 1; i++) a[c++] = a[i];
    lua_pushnumber(L, KahanBabuskaMeanNumArray(a, c));  /* 2.4.3 improvement */
  }
  xfree(a);  /* for all structures, free a */
  return 1;
}


/* The function determines the 1st quartile Q1 and the 3rd quartile Q3 along with the median Q2 of a
   distribution obj and returns the trimean (Q1 + 2*Q2 + Q3)/4 along with the median. When compared
   to the median, the trimean is a means to determine whether a distribution is biased in its
   first or second half. If the distribution is not sorted, it automatically sorts it non-destructively.

   From https://explorable.com/trimean:
   `A Sample Example

   For example, consider the heights of students in a class, in cm, to be 155, 158, 161, 162, 166, 170, 171, 174 and 179.
   It is easy to see the median of this data is 166 cm.

   Now consider another class where the heights of the students, again in cm, are 162, 162, 163, 165, 166, 175, 181, 186, and 192.
   It can be seen that the median height of the class is again 166 cm. However, a look at the two data distributions
   tells us that the distributions are quite different in both these cases, even though they have the same median.

   Now let us compute the trimean for the first case. The median as we saw was 166, the first quartile is 161 and
   the third quartile is 171. Using the formula given above, the trimean is computed as (161 + 2(166) + 171)/4 = 166.

   In the second example, the median is the same 166, but the first quartile is 163 and the second quartile is 181.
   Now the trimean is computed as (163 + 2(166) + 181)/4 = 169.

   Interpreting the Results

   In the first case, we see that the trimean is the same as the median. What this essentially means is that the distribution
   is very even from the median, which means there are about as many data points at a given distance from the median on
   either side (only on an average case of course).

   In the second case, the trimean is bigger than the mean. As you can see, the third quartile is farther away from
   the median than the first quartile, which essentially means that the data is biased in the second half of the distribution.
   Thus the trimean reflects this bias in data away from the median. Thus the effect of quartiles appears on the definition
   of trimean.` */

static int stats_trimean (lua_State *L) {  /* 2.4.2, patched 2.4.4 */
  size_t i, ii, rankp, rankq, p, q;
  lua_Number x1, x2, *a, m;
  luaL_checkany(L, 1);
  a = agnL_tonumarray(L, 1, &ii, "stats.trimean", 1, 0);  /* 2.4.4 change, 2.18.1 extension */
  if (a == NULL) return 1;  /* issue the error */
  p = agnL_optinteger(L, 2, 25);  /* 2.8.1 */
  if (p > 99) {  /* 7.9.5 change, no check for negative value any longer */
    xfree(a);
    luaL_error(L, "Error in " LUA_QS ": second argument must be in the range [0, 100).", "stats.trimean");
  }
  q = 100 - p;
  if (ii < 2)
    lua_pushfail(L);
  else {
    x1 = a[0];
    for (i=1; i < ii; i++) {
      x2 = a[i];
      if (x1 > x2) {  /* array unsorted ? -> sort it in ascending order */
        /* tools_dintrosort(a, 0, ii - 1, 0, 2*sun_log2(ii));  */ /* 2.8.4 speed-up */
        klib_dintrosort(a, ii);  /* 7.7.4 10 % speed-up */
        break;
      }
      x1 = x2;
    }
    rankp = FLOOR((lua_Number)p/100*(ii + 1)) - 1;  /* position of the 1. quartile */
    m = (ii % 2 == 0) ? (a[ii/2 - 1] + a[ii/2])*0.5 : a[ii/2];  /* median */
    rankq = FLOOR((lua_Number)q/100*(ii + 1)) - 1;  /* position of the 3. quartile */
    lua_pushnumber(L, (a[rankp] + 2*m + a[rankq])*0.25);  /* 2.17.7 tweak */
    lua_pushnumber(L, m);
  }
  xfree(a);  /* for all structures, free a */
  return 1 + (ii > 1);
}


/* Returns the first, second, and third quartile of a _sorted_ table or sequence, to the NIST rule.
   Agena 1.3.3, January 30, 2011. Ported to C with 2.4.2
   See also: http://www.vias.org/tmdatanaleng/cc_quartile.html. Also returns the lower and upper
   outlier limit and the interquartile range.
   stats.quartiles([-20, -4, -1, -1, 0, 1, 2, 3, 4, 4, 7, 7, 8, 11, 11, 12, 12, 15, 18, 22])
   -> [0, 5.5, 12, -4, 18, 12] */

static int stats_quartiles (lua_State *L) {  /* 2.4.2, patched 2.4.4 */
  size_t i, rankp, rankq, ii, p, q, nargs;
  int type, option;
  lua_Number x1, x2, *a, *r, m;
  NumArray *b = NULL;
  luaL_checkany(L, 1);
  type = lua_type(L, 1);
  a = agnL_tonumarray(L, 1, &ii, "stats.quartiles", 1, 0);  /* 2.18.1 extension */
  if (a == NULL) return 1;  /* issue the error */
  if (ii < 2) {
    lua_pushfail(L);
    xfree(a);
    return 1;
  }
  nargs = lua_gettop(L);
  option = luaL_optinteger(L, 2, 0);
  x1 = a[0];
  for (i=1; i < ii; i++) {
    x2 = a[i];
    if (x1 > x2) {  /* array unsorted ? -> sort it in ascending order */
      /* tools_dintrosort(a, 0, ii - 1, 0, 2*sun_log2(ii));  */ /* 2.8.4 speed-up */
      klib_dintrosort(a, ii);  /* 7.7.4 10 % speed-up */
      break;
    }
    x1 = x2;
  }
  p = 25; q = 75;
  rankp = FLOOR((lua_Number)p/100*(ii + 1)) - 1; /* position of the 1. quartile */
  m = (ii % 2 == 0) ? (a[ii/2 - 1] + a[ii/2])/2 : a[ii/2];  /* median */
  rankq = FLOOR((lua_Number)q/100*(ii + 1)) - 1;  /* position of the 3. quartile */
  r = agn_malloc(L, 6 * sizeof(lua_Number), "stats.quartiles", a, NULL);  /* 2.9.8, 7.3.5 fix */
  r[0] = a[rankp]; r[1] = m; r[2] = a[rankq];  /* 1st quartile, median, third quartile */
  if (nargs > 1) {
    switch (option) {
      case 1: lua_pushnumber(L, r[0]); break;  /* return 1st quartile, only */
      case 2: lua_pushnumber(L, r[1]); break;  /* return median, only */
      case 3: lua_pushnumber(L, r[2]); break;  /* return 3rd quartile, only */
      default: {
        xfreeall(a, r);
        luaL_error(L, "Error in " LUA_QS ": invalid option %d.", "stats.quartiles", option);
      }
    }
    xfreeall(a, r);  /* 2.9.8 */
    return 1;
  }
  if (type == LUA_TTABLE) {
    lua_createtable(L, 6, 0);
    for (i=0; i < 3; i++) agn_setinumber(L, -1, i + 1, r[i]);
  } else if (type == LUA_TREG) {
    agn_createreg(L, 6);
    for (i=0; i < 3; i++) agn_regsetinumber(L, -1, i + 1, r[i]);
  } else if (type == LUA_TSEQ) {
    agn_createseq(L, 6);
    for (i=0; i < 3; i++) agn_seqsetinumber(L, -1, i + 1, r[i]);
  } else {  /* new 4.11.7 */
    numarray_createdouble(L, b, 6);
    for (i=0; i < 3; i++) b->data.n[i] = r[i];
  }
  if (ii > 4 && rankp != 1) {
    lua_Number iqr, l1, u1;
    int M, A, B;
    M = -1;  /* to prevent compiler warnings */
    /* determine interquartile range */
    iqr = r[2] - r[0];
    /* compute lower outlier limit L1 */
    l1 = r[0] - 1.5*iqr;
    A = 0; B = rankp - 1;
    while (A < B) {
      M = tools_midpoint(A, B);  /* 2.38.2 patch */
      if (l1 > a[M])
        A = M + 1;
      else
        B = M;
    }
    while (a[M] <= l1) M++;  /* evade round-off errors */
    r[3] = a[M];  /* use index `m` instead of a or b ! */
    /* compute upper outlier limit U1 */
    u1 = r[2] + 1.5*iqr;
    A = rankq + 1; B = ii - 1;
    while (A < B) {
      M = tools_midpoint(A, B);  /* 2.38.2 patch */
      if (u1 > a[M])
        A = M + 1;
      else
        B = M;
    }
    while (a[M] >= u1) M--;  /* evade round-off errors */
    r[4] = a[M];  /* use index `m` instead of a or b ! */
    r[5] = iqr;
    if (type == LUA_TTABLE) {  /* insert `L1`,`U1`, and the interquartile range */
      for (i=3; i < 6; i++) agn_setinumber(L, -1, i + 1, r[i]);
    } else if (type == LUA_TREG) {
      for (i=3; i < 6; i++) agn_regsetinumber(L, -1, i + 1, r[i]);
    } else if (type == LUA_TSEQ) {
      for (i=3; i < 6; i++) agn_seqsetinumber(L, -1, i + 1, r[i]);
    } else {
      for (i=3; i < 6; i++) b->data.n[i] = r[i];
    }
  }
  xfreeall(a, r);  /* 2.9.8 */
  return 1;
}


/* Returns the first quartile, the median, and the third quartile of a distribution, in this order. If the number of
   observations is five or more, it also returns the minimum and the maximum observation, along with the arithmetic
   mean. The first and third quartiles are computed according to the NIST rule, see stats.percentile for further
   information. If the elements in obj are not sorted in ascending order, the function automatically sorts them
   non-destructively, and any non-numeric values are converted to zeros.

   See also: stats.quartiles. */
static lua_Number aux_fivenum (lua_Number *a, lua_Number rank, size_t n) {
  if (rank == 1) return a[0];
  else if (rank == 2) return a[n - 1];
  else {
    size_t k = luai_numentier(rank) - 1;
    return a[k] + luai_numfrac(rank)*(a[k + 1] - a[k]);
  }
}

static int stats_fivenum (lua_State *L) {  /* 2.8.1, extended 3.5.4 */
  size_t i, ii, rankp, rankq;
  int type, mode;
  lua_Number x1, x2, *a, *r, p, q;
  static const char *const modenames[] = {"nist", "excel", "wikipedia", NULL};
  luaL_checkany(L, 1);
  type = lua_type(L, 1);
  luaL_typecheck(L, type == LUA_TTABLE || type == LUA_TSEQ || type == LUA_TREG || type == LUA_TUSERDATA, 1,
    "table, sequence, register or numarray expected", type);
  mode = luaL_checkoption(L, 2, "nist", modenames);
  a = agnL_tonumarray(L, 1, &ii, "stats.fivenum", 1, 0);  /* 2.18.1 extension */
  if (a == NULL) return 1;  /* issue the error */
  if (ii < 2) {
    lua_pushfail(L);
    xfree(a);  /* for all structures, free a */
    return 1;
  }
  x1 = a[0];
  for (i=1; i < ii; i++) {
    x2 = a[i];
    if (x1 > x2) {  /* array unsorted ? -> sort it in ascending order */
      /* tools_dintrosort(a, 0, ii - 1, 0, 2*sun_log2(ii)); */ /* 2.8.4 speed-up */
      klib_dintrosort(a, ii);  /* 7.7.4 10 % speed-up */
      break;
    }
    x1 = x2;
  }
  p = 25.0; q = 75.0;
  r = agn_malloc(L, 6 * sizeof(lua_Number), "stats.fivenum", a, NULL);  /* 2.9.8, 7.3.5 fix */
  switch (mode) {
    case 0: { /* NIST */
      /* rank = p/100*(n + 1) */
      r[0] = aux_fivenum(a, p/100*(ii + 1), ii);  /* position of the 1. quartile */
      r[2] = aux_fivenum(a, q/100*(ii + 1), ii);  /* position of the 3. quartile */
      break;
    }
    case 1: { /* Excel, 3.5.4 */
      /* rank = p/100*(n - 1) + 1 */
      r[0] = aux_fivenum(a, p/100*(ii - 1) + 1, ii);  /* position of the 1. quartile */
      r[2] = aux_fivenum(a, q/100*(ii - 1) + 1, ii);  /* position of the 3. quartile */
      break;
    }
    case 2: {  /* Wikipedia, 3.5.4 */
      /* rank = sun_round(p/100*n + 0.5) */
      rankp = (size_t)sun_round(p/100*ii + 0.5);  /* round rank to the nearest integer */
      rankq = (size_t)sun_round(q/100*ii + 0.5);  /* ditto */
      r[0] = a[rankp - 1];
      r[2] = a[rankq - 1];
      break;
    }
    default: {
      xfreeall(a, r);
      luaL_error(L, "Error in " LUA_QS ": unknown option.", "stats.fivenum");
    }
  }
  r[1] = (ii % 2 == 0) ? (a[ii/2 - 1] + a[ii/2])*0.5 : a[ii/2];  /* median, 2.17.7 tweak */
  r[3] = a[0];       /* minimum */
  r[4] = a[ii - 1];  /* maximum */
  r[5] = KahanBabuskaMeanNumArray(a, ii);  /* arithmetic mean */
  if (type == LUA_TTABLE) {
    lua_createtable(L, 6, 0);
    for (i=0; i < 6; i++) agn_setinumber(L, -1, i + 1, r[i]);
  } else if (type == LUA_TREG) {
    agn_createreg(L, 6);
    for (i=0; i < 6; i++) agn_regsetinumber(L, -1, i + 1, r[i]);
  } else if (type == LUA_TSEQ) {
    agn_createseq(L, 6);
    for (i=0; i < 6; i++) agn_seqsetinumber(L, -1, i + 1, r[i]);
  } else {  /* new 4.11.7 */
    NumArray *b = NULL;
    numarray_createdouble(L, b, 6);
    for (i=0; i < 6; i++) b->data.n[i] = r[i];
  }
  xfreeall(a, r);  /* 2.9.8, for all structures, free a */
  return 1;
}


static int stats_percentile (lua_State *L) {  /* 3.5.4 */
  size_t i, ii;
  int type, mode;
  lua_Number x1, x2, *a, r, p;
  static const char *const modenames[] = {"nist", "excel", "wikipedia", NULL};
  r = 0;
  type = lua_type(L, 1);
  luaL_typecheck(L, type == LUA_TTABLE || type == LUA_TSEQ || type == LUA_TREG || type == LUA_TUSERDATA, 1,
    "table, sequence, register or numarray expected", type);
  p = agn_checknonnegint(L, 2);
  if (p > 99)
    luaL_error(L, "Error in " LUA_QS ": second argument must be in the range [0, 100).", "stats.percentile");
  mode = luaL_checkoption(L, 3, "wikipedia", modenames);
  a = agnL_tonumarray(L, 1, &ii, "stats.percentile", 1, 0);
  if (a == NULL) return 1;  /* issue the error */
  if (ii < 2) {
    lua_pushfail(L);
    xfree(a);
    return 1;
  }
  x1 = a[0];
  for (i=1; i < ii; i++) {
    x2 = a[i];
    if (x1 > x2) {  /* array unsorted ? -> sort it in ascending order */
      /* tools_dintrosort(a, 0, ii - 1, 0, 2*sun_log2(ii)); */
      klib_dintrosort(a, ii);  /* 7.7.4 10 % speed-up */
      break;
    }
    x1 = x2;
  }
  switch (mode) {
    case 0: { /* NIST */
      /* rank = p/100*(n + 1) */
      r = aux_fivenum(a, p/100*(ii + 1), ii);  /* position of the percentile */
      break;
    }
    case 1: { /* Excel */
      /* rank = p/100*(n - 1) + 1 */
      r = aux_fivenum(a, p/100*(ii - 1) + 1, ii);  /* position of the percentile */
      break;
    }
    case 2: {  /* Wikipedia */
      /* rank = sun_round(p/100*n + 0.5) */
      size_t rank = (size_t)sun_round(p/100*ii + 0.5);  /* round rank to the nearest integer */
      r = a[rank - 1];
      break;
    }
    default: {
      xfree(a);
      luaL_error(L, "Error in " LUA_QS ": unknown option.", "stats.percentile");
    }
  }
  lua_pushnumber(L, r);
  xfree(a);  /* for all structures, free a */
  return 1;
}


static int stats_fsum (lua_State *L) {  /* based on calc.fsum; 2.2.0 RC 2, May 19, 2014 */
  size_t n, a, b, slots;
  volatile lua_Number s, cs, ccs, t, c, cc, x;
  int nargs, i, offset, type, error, isnumarraygiven;
  NumArray *ud = NULL;  /* 2.18.1 extension */
  s = cs = ccs = 0;
  luaL_checktype(L, 1, LUA_TFUNCTION);
  nargs = lua_gettop(L);  /* 2.1.4 */
  isnumarraygiven = isnumarray(L, 2);
  type = lua_type(L, 2);
  luaL_typecheck(L, type == LUA_TTABLE || type == LUA_TSEQ || type == LUA_TREG || isnumarraygiven, 2,
    "table, sequence, register or numarray expected", type);
  if (isnumarraygiven) {  /* 2.18.1 extension */
    ud = lua_touserdata(L, 2);
    n = ud->size;
  } else
    n = agn_nops(L, 2);  /* number of elements in structure */
  a = agnL_optposint(L, 3, 1);  /* start value */
  b = agnL_optposint(L, 4, n);  /* stop value */
  if (a > n || b > n || a > b)
    luaL_error(L, "Error in " LUA_QS ": invalid start or stop values.", "stats.fsum");
  offset = (nargs > 4) * (nargs - 4);  /* multivariate function ? */
  slots = 1 + 1 + offset;
  while (a <= b) {
    luaL_checkstack(L, slots, "not enough stack space");  /* 2.31.7/3.5.4 fix */
    lua_pushvalue(L, 1);  /* push function */
    if (type == LUA_TSEQ) {  /* 2.18.1 change */
      lua_pushnumber(L, agn_seqgetinumber(L, 2, a));
    } else if (type == LUA_TREG) {  /* 3.18.8 */
      lua_pushnumber(L, agn_reggetinumber(L, 2, a));
    } else if (type == LUA_TTABLE) {
      lua_pushnumber(L, agn_getinumber(L, 2, a));
    } else {
      lua_pushnumber(L, ud->data.n[a - 1]);
    }
    for (i=5; i <= nargs; i++) lua_pushvalue(L, i);
    x = agn_ncall(L, 1 + offset, &error, 1);
    t = s + x;
    c = (fabs(s) >= fabs(x)) ? (s - t) + x : (x - t) + s;
    s = t;
    t = cs + c;
    cc = (fabs(cs) >= fabs(c)) ? (cs - t) + c : (c - t) + cs;
    cs = t;
    ccs = ccs + cc;
    a++;
  }
  lua_pushnumber(L, s + cs + ccs);  /* no rounding in between */
  return 1;
}


static int stats_fprod (lua_State *L) {  /* based on stats.fsum; 2.2.0 RC 2, May 22, 2014 */
  size_t n, a, b, slots;
  lua_Number p;
  int nargs, i, offset, type, error, isnumarraygiven;
  NumArray *ud = NULL;
  p = 1;
  luaL_checktype(L, 1, LUA_TFUNCTION);
  nargs = lua_gettop(L);  /* 2.1.4 */
  luaL_checkstack(L, nargs, "too many arguments");
  isnumarraygiven = isnumarray(L, 2);
  type = lua_type(L, 2);
  luaL_typecheck(L, type == LUA_TTABLE || type == LUA_TSEQ || type == LUA_TREG || isnumarraygiven, 2,
    "table, sequence, register or numarray expected", type);
  if (isnumarraygiven) {  /* 2.18.1 extension */
    ud = lua_touserdata(L, 2);
    n = ud->size;
  } else
    n = agn_nops(L, 2);  /* number of elements in structure */
  a = agnL_optposint(L, 3, 1);  /* start value */
  b = agnL_optposint(L, 4, n);  /* stop value */
  if (a > n || b > n || a > b)
    luaL_error(L, "Error in " LUA_QS ": invalid start or stop values.", "stats.fprod");
  offset = (nargs > 4) * (nargs - 4);  /* multivariate function ? */
  slots = 1 + 1 + offset;
  while (a <= b) {
    luaL_checkstack(L, slots, "not enough stack space");  /* 2.31.7/3.5.4 fix */
    lua_pushvalue(L, 1);  /* push function */
    if (type == LUA_TSEQ) {  /* 2.18.1 change */
      lua_pushnumber(L, agn_seqgetinumber(L, 2, a));
    } else if (type == LUA_TREG) {
      lua_pushnumber(L, agn_reggetinumber(L, 2, a));
    } else if (type == LUA_TTABLE) {
      lua_pushnumber(L, agn_getinumber(L, 2, a));
    } else {
      lua_pushnumber(L, ud->data.n[a - 1]);
    }
    for (i=5; i <= nargs; i++) lua_pushvalue(L, i);
    p *= agn_ncall(L, 1 + offset, &error, 1);
    a++;
  }
  lua_pushnumber(L, p);
  return 1;
}


/* Gini Coefficient, 2.3.0 RC 1

Measures the inequality in a distribution given by the table, sequence, register or numarray obj
by applying Gini's formula.

All samples should be numbers. `infinity's` or `undefined's` will be ignored.

It returns a number r indicating the absolute mean of the difference between every pair of observations,
divided by the arithmetic mean of the population, with 0 <= r <= 1, where 0 indicates that all
observations are equal, and (a theoretical value of) 1 indicates complete inequality. It is assumed
that all observations are non-negative.

If the option "sorted" is given then the function assumes that all elements in
obj are already sorted in ascending order - thus computing the result much faster.

See also: http://www.statsdirect.com/help/default.htm#nonparametric_methods/gini.htm */

static int stats_gini (lua_State *L) {
  size_t ii, i, j;
  lua_Number *a;
  const char *option;
  lua_Number km;
  volatile double r, cs, ccs;
  luaL_checkany(L, 1);
  ii = r = cs = ccs = 0;
  a = agnL_tonumarray(L, 1, &ii, "stats.gini", 0, 0);  /* 2.18.1 extension; with numarrays, work in-place */
  if (a == NULL) return 1;  /* issue the error */
  option = luaL_optstring(L, 2, "default");
  if (ii < 2)
    lua_pushfail(L);
  else if (tools_strneq(option, "sorted")) {  /* 2.16.12 tweak */
    km = KahanBabuskaMeanNumArray(a, ii);  /* 2.4.3 improvement */
    for (i=0; i < ii; i++) {
      for (j=0; j < ii; j++) {
        r = tools_kbadd(r, fabs(a[i] - a[j]), &cs, &ccs);  /* 3.7.2 change to Kahan-Babuska summation */
      }
    }
    r += cs + ccs;
    lua_pushnumber(L, (km == 0.0) ? AGN_NAN : r/(2*ii*ii*km));  /* 3.18.8 fix */
  } else {
    km = KahanBabuskaMeanNumArray(a, ii);  /* 2.4.3 improvement */
    for (i=0; i < ii; i++) {
      r = tools_kbadd(r, (i + 1)*(a[i] - km), &cs, &ccs);  /* 3.7.2 change to Kahan-Babuska summation */
    }
    r += cs + ccs;
    lua_pushnumber(L, (km == 0.0) ? AGN_NAN : r*2/(ii*ii*km));  /* 3.18.8 fix */
  }
  if (!lua_isuserdata(L, 1)) { xfree(a); }  /* don't free numarray ! */
  return 1;
}


/* stats.moment: computes the various moments p of the given data x about any origin xm for a full population.
   It is equivalent to: sum((x[i] - xm)^p, i=1 .. n)/n.

   If only the table or sequence x is given, the moment p defaults to 1, and the origin xm defaults to 0. If
   given, the moment p and the origin xm must be numbers. Beginning with 2.4.5, the function can also return the
   sample moment. */

static int stats_moment (lua_State *L) {
  size_t ii;
  int sample;
  lua_Number *a, p, xm;
  luaL_checkany(L, 1);
  p = luaL_optnumber(L, 2, 1);  /* the moment (power p) */
  xm = luaL_optnumber(L, 3, 0);  /* origin, quantity about which the moment is computed (optional value to be
                                    subtracted from x[i]) */
  sample = agnL_optboolean(L, 4, 0);  /* compute sample moment instead of population moment, 2.4.5 */
  ii = 0;
  a = agnL_tonumarray(L, 1, &ii, "stats.moment", 0, 0);  /* 2.18.1 extension */
  if (a == NULL) return 1;  /* issue the error */
  if (ii < 2)
    lua_pushfail(L);
  else
    lua_pushnumber(L, KahanBabuskaSumdata(a, ii, p, xm) / ((sample) ? ii - 1 : ii ));  /* 2.4.4 & 2.4.5 change */
  if (!lua_isuserdata(L, 1)) { xfree(a); }
  return 1;
}


static int stats_meanmed (lua_State *L) {  /* 2.2.3 / 2.3.0 RC 1 */
  size_t ii, nres;
  lua_Number *a;
  luaL_checkany(L, 1);
  ii = 0;
  a = agnL_tonumarray(L, 1, &ii, "stats.meanmed", 1, 0);  /* 2.18.1 extension */
  if (a == NULL) return 1;  /* issue the error */
  nres = 1;
  if (ii < 2)
    lua_pushfail(L);
  else {
    lua_Number mean, median;
    mean = KahanBabuskaMeanNumArray(a, ii);  /* 2.4.3 improvement */
    median = ii&1 ? tools_kth_smallest(a, ii, ii/2) :
      (tools_kth_smallest(a, ii, ii/2 - 1) + tools_kth_smallest(a, ii, ii/2))*0.5;  /* 2.17.7 tweak */
    if (lua_gettop(L) == 2)
      lua_pushnumber(L, mean/median);
    else {
      lua_pushnumber(L, mean);
      lua_pushnumber(L, median);
      nres++;
    }
  }
  xfree(a);  /* for all structures, free a */
  return nres;
}


/* Returns both the artithmetic mean and the variance of a distribution using an algorithm
   developed by B. P. Welford to prevent round-off errors. By default, the population variance
   is returned unless you pass the Boolean value `true` to compute the sample variance. 2.5.3,
   rewritten 4.6.0 avoiding item duplication. 4.6.1 Increase in precision especially with large
   distributions, by switching to 80-bit.
   The structure is at stack index 1. */
static lua_Number Welford (lua_State *L, lua_Number (*f)(lua_State *, int, int, int *),
  size_t n, int optimised, lua_Number *variance) {  /* 4.6.1 */
  int i, rc;
  volatile longdouble mean, delta, x, var, rho;  /* 4.6.1 */
  mean = 0.0L;
  var = 0.0L;
  for (i=0; i < n; i++) {
    x = (*f)(L, 1, i + 1, &rc);
    delta = x - mean;
    rho = 1.0L/(i + 1.0L);
    mean += rho*delta;
    /* See: https://stackoverflow.com/questions/1174984/how-to-efficiently-calculate-a-running-standard-deviation
       answer by Dave: "Unlike the [non-optimised version], the variable, var, that is tracking the running variance
       does not grow in proportion to the number of samples." 4.11.6 */
    var += optimised ? rho*((1 - rho)*delta*delta - var) : delta*(x - mean);
  }
  *variance = (lua_Number)var;
  return (lua_Number)mean;
}

static lua_Number WelfordNumArray (lua_Number *a, size_t n, int optimised, lua_Number *variance) {  /* 4.6.1 */
  int i;
  volatile longdouble mean, delta, x, var, rho;  /* 4.6.1 */
  mean = 0.0L;
  var = 0.0L;
  for (i=0; i < n; i++) {
    x = a[i];
    delta = x - mean;
    rho = 1.0L/(i + 1.0L);
    mean += rho*delta;
    /* See: https://stackoverflow.com/questions/1174984/how-to-efficiently-calculate-a-running-standard-deviation
       answer by Dave: "Unlike the [non-optimised version], the variable, var, that is tracking the running variance
       does not grow in proportion to the number of samples." 4.11.6 */
    var += optimised ? rho*((1 - rho)*delta*delta - var) : delta*(x - mean);
  }
  *variance = (lua_Number)var;
  return mean;
}

/* The function is at stack index fidx, the structure at idx. */
static lua_Number WelfordFn (lua_State *L, int fidx, int idx, lua_Number (*f)(lua_State *, int, int, int *),
    size_t n, int pos, int nargs, int optimised, lua_Number *variance, int *c) {  /* 4.6.1/4.11.6 */
  int rc, i, j, surplus;
  volatile longdouble mean, delta, x, var, rho;
  mean = 0.0L;
  var = 0.0L;
  *c = 1;
  surplus = (pos <= nargs)*(nargs - pos + 1);
  for (i=0; i < n; i++) {
    x = (*f)(L, idx, i + 1, &rc);
    luaL_checkstack(L, 2 + surplus, "not enough stack space");
    lua_pushvalue(L, fidx);
    lua_pushnumber(L, x);
    for (j=pos; j <= nargs; j++) lua_pushvalue(L, j);
    lua_call(L, 1 + surplus, 1);
    if (lua_istrue(L, -1)) {  /* 5.0.1 fix */
      delta = x - mean;
      rho = 1.0L/(*c)++;
      mean += rho*delta;
      /* See: https://stackoverflow.com/questions/1174984/how-to-efficiently-calculate-a-running-standard-deviation
         answer by Dave: "Unlike the [non-optimised version], the variable, var, that is tracking the running variance
         does not grow in proportion to the number of samples." 4.11.6 */
      var += optimised ? rho*((1 - rho)*delta*delta - var) : delta*(x - mean);
    }
    agn_poptop(L);
  }
  *variance = (lua_Number)var;
  (*c)--;
  return mean;
}

/* The function is at stack index fidx. */
static lua_Number WelfordNumArrayFn (lua_State *L, int fidx, lua_Number *a, size_t n, int pos, int nargs,
    int optimised, lua_Number *variance, int *c) {  /* 4.6.1/4.11.6 */
  int i, j, surplus;
  volatile longdouble mean, delta, x, var, rho;
  mean = 0.0L;
  var = 0.0L;
  *c = 1;
  surplus = (pos <= nargs)*(nargs - pos + 1);
  for (i=0; i < n; i++) {
    x = a[i];
    luaL_checkstack(L, 2 + surplus, "not enough stack space");
    lua_pushvalue(L, fidx);
    lua_pushnumber(L, x);
    for (j=pos; j <= nargs; j++) lua_pushvalue(L, j);
    lua_call(L, 1 + surplus, 1);
    if (lua_istrue(L, -1)) {  /* 5.0.1 fix */
      delta = x - mean;
      rho = 1.0L/(*c)++;
      mean += rho*delta;
      /* See: https://stackoverflow.com/questions/1174984/how-to-efficiently-calculate-a-running-standard-deviation
         answer by Dave: "Unlike the [non-optimised version], the variable, var, that is tracking the running variance
         does not grow in proportion to the number of samples." 4.11.6 */
      var += optimised ? rho*((1 - rho)*delta*delta - var) : delta*(x - mean);
    }
    agn_poptop(L);
  }
  *variance = (lua_Number)var;
  (*c)--;
  return mean;
}

/* what = 0: variance, what = 1: qmdev */
static int aux_checkcardoptions (lua_State *L, int what, int pos,
  int *nargs, int *meanvar, int *optimised, int *sample, const char *procname) {
  int checkoptions, rc;
  rc = 0;
  *meanvar = 0;      /* 1 = compute arithmetic mean and variance along with number of elements, 0 = just count */
  *optimised = what; /* 1 = running variance does not grow in proportion to the number of samples */
  if (lua_isnoneornil(L, pos)) {  /* 1 = sample, 0 = population, backward-compatibility for `stats.meanvar` */
    *sample = 0;
  } else {
    *sample = lua_istrue(L, pos);
  }
  /* check for options, here `map in-place` */
  checkoptions = 3;  /* check n options; CHANGE THIS if you add/delete options */
  /* check for properly allocated slots will be done by agn_pairgetiall() */
  while (checkoptions-- && *nargs >= pos && lua_ispair(L, *nargs)) {
    agn_pairgetiall(L, *nargs);  /* get left, right value, set to stack index -2, -1, 7.7.8 */
    if (agn_isstring(L, -2)) {
      const char *option = agn_tostring(L, -2);
      if (tools_streq(option, "meanvar")) {
        *meanvar = agn_checkboolean(L, -1);
      } else if (tools_streq(option, "optimised")) {
        *optimised = agn_checkboolean(L, -1); rc = 1;
      } else if (tools_streq(option, "sample")) {
        *sample = agn_checkboolean(L, -1);
      } else {
        agn_poptoptwo(L);
        luaL_error(L, "Error in " LUA_QS ": unknown option " LUA_QS ".", procname, option);
      }
    }
    /* do not call lua_settop as it would corrupt the argument stack since we have already pushed values */
    (*nargs)--;
    agn_poptoptwo(L);
  }
  return rc;
}

/* what = 0: compute variance, what = 1: compute quadratic mean deviation; externalised 4.11.6 */
static int aux_meanvarqmd (lua_State *L, int what, const char *procname, lua_Number *mean, lua_Number *var) {
  int n, type, isfn, c, nargs, meanvar, optimised, sample;
  nargs = lua_gettop(L);
  luaL_checkany(L, 1);
  isfn = lua_isfunction(L, 1);
  aux_checkcardoptions(L, what, 2 + isfn, &nargs, &meanvar, &optimised, &sample, procname);
  if (what == 1 && sample)
    luaL_error(L, "Error in " LUA_QS ": the sample option cannot be given.", procname);
  n = aux_getsize(L, 1 + isfn, &type, procname);
  if (n < 2) return 1;
  *mean = 0.0; *var = 0.0; c = 0;
  switch (type) {
    case LUA_TTABLE: {
      *mean = (isfn) ? WelfordFn(L, 1, 2, agn_rawgetinumber, n, 3, nargs, optimised, var, &c) :
                       Welford(L, agn_rawgetinumber, n, optimised, var);
      break;
    }
    case LUA_TSEQ: {
      *mean = (isfn) ? WelfordFn(L, 1, 2, agn_seqrawgetinumber, n, 3, nargs, optimised, var, &c) :
                       Welford(L, agn_seqrawgetinumber, n, optimised, var);
      break;
    }
    case LUA_TREG: {
      *mean = (isfn) ? WelfordFn(L, 1, 2, agn_regrawgetinumber, n, 3, nargs, optimised, var, &c) :
                       Welford(L, agn_regrawgetinumber, n, optimised, var);
      break;
    }
    case LUA_TUSERDATA: {
      NumArray *a = checknumarray(L, 1 + isfn);
      *mean = (isfn) ? WelfordNumArrayFn(L, 1, a->data.n, n, 3, nargs, optimised, var, &c) :
                       WelfordNumArray(a->data.n, n, optimised, var);
      break;
    }
    default: {
      luaL_error(L, "Error in " LUA_QS ": table, sequence, register or userdata expected, got %s.",
        procname, luaL_typename(L, 1));
    }
  }
  if (isfn) n = c;
  switch(what) {
    case 0: {  /* return population or sample variance */
      if (optimised) {
        *var *= (lua_Number)n/(lua_Number)(n - sample);
      } else {
        *var /= (n - sample);
      }
      break;
    }
    case 1: {  /* return quadratic mean deviation */
      /* in optimised mode, *var has the variance, so we must multiply by cardinality */
      if (optimised) *var *= n;
      /* otherwise *var already has the quatratic mean deviation */
      break;
    }
    default:
      luaL_error(L, "Error in " LUA_QS ": this should not happen.", procname);
  }
  return 0;
}

static int stats_meanvar (lua_State *L) {
  lua_Number mean, var;
  if (aux_meanvarqmd(L, 0, "stats.meanvar", &mean, &var)) {  /* too few samples */
    lua_pushfail(L);
    return 1;
  }
  lua_pushnumber(L, mean);
  lua_pushnumber(L, var);
  return 2;
}


/* Returns both the arithmetic mean and the quadratic mean deviation of distribution o, represented as a table,
   sequence, register or double numeric array. Mathemetically, the quadratic mean deviation is the population
   variance multiplied by the number of samples in a distribution. Note that with very large samples, the
   quadratic mean deviation might overflow. 4.11.6 */
static int stats_meanqmdev (lua_State *L) {
  lua_Number mean, var;
  if (aux_meanvarqmd(L, 1, "stats.meanqmdev", &mean, &var)) {  /* too few samples */
    lua_pushfail(L);
    return 1;
  }
  lua_pushnumber(L, mean);
  lua_pushnumber(L, var);
  return 2;
}


/* In the first form, `stats.card` counts the samples in the table, sequence, register or numarray s.
   In the second form, counts all the samples in s that satisfy the Boolean function f. With multivariate f,
   its second, third, etc. argument may be given right after f.
   By default, the return is a non-negative integer, the cardinality.

   By passing one or more of the following option, you can control how the result is computed and what to return:

   * When given the `optimised=true' option, the function more strictly tries to avoid numeric overflow with very
     large values. The default is `false`.
   * With the `meanvar=true' option the function returns the cardinality, the arithmetic mean and the population
     variance (see next). The default is `false`.
   * The `sample=true' option, given along the `meanvar=true' option in any order, forces the function to return
     the sample variance instead of the population variance.

   With the `meanvar` option given, you may improve the accuracy of the result by sorting the distribution before
   calling the call.

   Based on stats_meanvar, 4.11.6

   The function probably is at stack index 2, the structure at 1. */

/* Check options for linalg.vzero, linalg.unitvector, linalg.inverse 4.1.1 */

/* The function is at stack index fidx, the structure at idx. */
static int CountFn (lua_State *L, int fidx, int idx, lua_Number (*f)(lua_State *, int, int, int *),
  size_t n, int pos, int nargs) {
  int rc, i, j, c, surplus;
  lua_Number x;
  c = 0;
  surplus = (pos <= nargs)*(nargs - pos + 1);
  for (i=0; i < n; i++) {
    x = (*f)(L, idx, i + 1, &rc);
    luaL_checkstack(L, 2 + surplus, "not enough stack space");
    lua_pushvalue(L, fidx);
    lua_pushnumber(L, x);
    for (j=pos; j <= nargs; j++) lua_pushvalue(L, j);
    lua_call(L, 1 + surplus, 1);
    if (lua_istrue(L, -1)) c++;  /* 5.0.1 fix */
    agn_poptop(L);
  }
  return c;
}

/* The function is at stack index fidx. */
static int CountNumArrayFn (lua_State *L, int fidx, lua_Number *a, size_t n, int pos, int nargs) {
  int i, j, c, surplus;
  lua_Number x;
  c = 0;
  surplus = (pos <= nargs)*(nargs - pos + 1);
  for (i=0; i < n; i++) {
    x = a[i];
    luaL_checkstack(L, 2 + surplus, "not enough stack space");
    lua_pushvalue(L, fidx);
    lua_pushnumber(L, x);
    for (j=pos; j <= nargs; j++) lua_pushvalue(L, j);
    lua_call(L, 1 + surplus, 1);
    if (lua_istrue(L, -1)) c++;  /* 5.0.1 fix */
    agn_poptop(L);
  }
  return c;
}

static int stats_card (lua_State *L) {
  int n, c, type, isfn, nargs, meanvar, optimised, sample;
  lua_Number mean, var;
  luaL_checkany(L, 1);
  nargs = lua_gettop(L);
  isfn = lua_isfunction(L, 2);
  aux_checkcardoptions(L, 0, 2 + isfn, &nargs, &meanvar, &optimised, &sample, "stats.card");
  n = aux_getsize(L, 1, &type, "stats.card");
  if (sample) meanvar = 1;
  mean = 0.0; var = 0.0;
  c = 0;
  if (!meanvar && !isfn) {
    lua_pushinteger(L, n);
    return 1;
  } else if (!meanvar && isfn) {
    switch (type) {
      case LUA_TTABLE: {
        c = CountFn(L, 2, 1, agn_rawgetinumber, n, 3, nargs);
        break;
      }
      case LUA_TSEQ: {
        c = CountFn(L, 2, 1, agn_seqrawgetinumber, n, 3, nargs);
        break;
      }
      case LUA_TREG: {
        c = CountFn(L, 2, 1, agn_regrawgetinumber, n, 3, nargs);
        break;
      }
      case LUA_TUSERDATA: {
        NumArray *a = checknumarray(L, 1);
        c = CountNumArrayFn(L, 2, a->data.n, n, 3, nargs);
        break;
      }
      default: {
        luaL_error(L, "Error in " LUA_QS ": table, sequence, register or userdata expected, got %s.",
          "stats.card", luaL_typename(L, 1));
      }
    }
    lua_pushinteger(L, c);
    return 1;
  }
  /* meanvar = true option given, return number of samples, arithmetic mean and variance */
  switch (type) {
    case LUA_TTABLE: {
      mean = (isfn) ? WelfordFn(L, 2, 1, agn_rawgetinumber, n, 3, nargs, optimised, &var, &c) :
                      Welford(L, agn_rawgetinumber, n, optimised, &var);
      break;
    }
    case LUA_TSEQ: {
      mean = (isfn) ? WelfordFn(L, 2, 1, agn_seqrawgetinumber, n, 3, nargs, optimised, &var, &c) :
                      Welford(L, agn_seqrawgetinumber, n, optimised, &var);
      break;
    }
    case LUA_TREG: {
      mean = (isfn) ? WelfordFn(L, 2, 1, agn_regrawgetinumber, n, 3, nargs, optimised, &var, &c) :
                      Welford(L, agn_regrawgetinumber, n, optimised, &var);
      break;
    }
    case LUA_TUSERDATA: {
      NumArray *a = checknumarray(L, 1);
      mean = (isfn) ? WelfordNumArrayFn(L, 2, a->data.n, n, 3, nargs, optimised, &var, &c) :
                      WelfordNumArray(a->data.n, n, optimised, &var);
      break;
    }
    default: {
      luaL_error(L, "Error in " LUA_QS ": table, sequence, register or userdata expected, got %s.",
        "stats.card", luaL_typename(L, 1));
    }
  }
  if (isfn) n = c;
  lua_pushinteger(L, n);
  lua_pushnumber(L, mean);
  /* if n < 2 then we have to avoid division by zero as stats.card does not just bail out before */
  if (optimised) {
    lua_pushnumber(L, (n != sample)*(lua_Number)n/(lua_Number)(n - sample)*var);
  } else {
    lua_pushnumber(L, (n != sample)*var/(n - sample));
  }
  return 3;
}


/* Standardises a distribution by subtracting the arithmetic mean from each observation and then dividing by the
   population standard deviation (default) of the distribution. Depending on the type of its argument,
   the return is either a new table or sequence of the respective quotients, preserving the original order of the
   observations. You may alternatively divide by the sample standard deviation by passing the optional value `true`
   as the second argument. 2.5.3; rewritten to reduce memory consumption 4.6.0. */

static void checkstdoptions (lua_State *L, int *nargs, int *sample, int *inplace, const char *procname) {
  int checkoptions;
  *inplace = 0;
  *sample = *nargs > 1 && !lua_ispair(L, 2) && !lua_isnoneornil(L, 2);
  /* check for options */
  checkoptions = 2;  /* check n options; CHANGE THIS if you add/delete options */
  /* check for properly allocated slots will be done by agn_pairgetiall() */
  while (checkoptions-- && *nargs > 1 && lua_ispair(L, *nargs)) {
    agn_pairgetiall(L, *nargs);  /* get left, right value, set to stack index -2, -1, 7.7.8 */
    if (agn_isstring(L, -2)) {
      const char *option = agn_tostring(L, -2);
      if (tools_streq(option, "sample")) {
        *sample = agn_checkboolean(L, -1);
      } else if (tools_streq(option, "inplace")) {
        *inplace = agn_checkboolean(L, -1);
      } else {
        agn_poptoptwo(L);
        luaL_error(L, "Error in " LUA_QS ": unknown option " LUA_QS ".", procname, option);
      }
    }
    /* do not call lua_settop as it would corrupt the argument stack since we have already pushed values */
    (*nargs)--;
    agn_poptoptwo(L);
  }
}

static int stats_standardise (lua_State *L) {
  int j, n, sample, inplace, type, rc, nargs, idx;
  lua_Number mean, var, sd, x;
  luaL_checkany(L, 1);
  nargs = agnL_gettop(L, "need at least one argument", "stats.standardise");
  checkstdoptions(L, &nargs, &sample, &inplace, "stats.standardise");
  n = aux_getsize(L, 1, &type, "stats.standardise");
  if (n < 2) {
    lua_pushfail(L);
    return 1;
  }
  mean = 0.0; var = 0.0;
  idx = (inplace) ? 1 : -1;
  if (inplace) lua_settop(L, 1);  /* return modified input structure */
  switch (type) {
    case LUA_TTABLE: {
      mean = Welford(L, agn_rawgetinumber, n, 0, &var);
      sd = sqrt(var/(n - sample));
      if (!inplace) lua_createtable(L, n, 0);
      for (j=1; j <= n; j++) {
        x = agn_rawgetinumber(L, 1, j, &rc);
        agn_setinumber(L, idx, j, (x - mean)/sd);
      }
      break;
    }
    case LUA_TSEQ: {
      mean = Welford(L, agn_seqrawgetinumber, n, 0, &var);
      sd = sqrt(var/(n - sample));
      if (!inplace) agn_createseq(L, n);
      for (j=1; j <= n; j++) {
        x = agn_seqrawgetinumber(L, 1, j, &rc);
        agn_seqsetinumber(L, idx, j, (x - mean)/sd);
      }
      break;
    }
    case LUA_TREG: {
      mean = Welford(L, agn_regrawgetinumber, n, 0, &var);
      sd = sqrt(var/(n - sample));
      if (!inplace) agn_createreg(L, n);
      for (j=1; j <= n; j++) {
        x = agn_regrawgetinumber(L, 1, j, &rc);
        agn_regsetinumber(L, idx, j, (x - mean)/sd);
      }
      break;
    }
    case LUA_TUSERDATA: {
      NumArray *a = checknumarray(L, 1);
      mean = WelfordNumArray(a->data.n, n, 0, &var);
      sd = sqrt(var/(n - sample));
      if (!inplace) {
        agn_createseq(L, n);
        for (j=1; j <= n; j++) {
          agn_seqsetinumber(L, -1, j, (a->data.n[j - 1] - mean)/sd);
        }
      } else {
        for (j=0; j < n; j++) {
          a->data.n[j] = (a->data.n[j] - mean)/sd;
        }
      }
      break;
    }
    default: {
      luaL_error(L, "Error in " LUA_QS ": table, sequence, register or userdata expected, got %s.",
        "stats.standardise", luaL_typename(L, 1));
    }
  }
  return 1;
}


/* Determines all neighbours of a given n-dimensional point in a distribution obj that lie in a
   certain Eucledian distance eps. idx is the position of the point of interest in the distribution
   - a positive integer -, and not the point itself. eps is any positive number, power is a positive
   integer with which the respective Euclidean distances and eps shall be raised before a comparison
   is conducted, its default is 2.

   The return is a sequence with the nearby points. If the fifth argument `indices` is `true`, however,
   then not the points but their positions in the distribution are returned.

   The points may be represented either as pairs (2-dimensional space), sequences of coordinates
   (n-dimensional space), or any n-dimensional vectors created by the linalg.vector function.

   See also: linalg.norm, stats.dbscan. */

static int stats_neighbours (lua_State *L) {  /* 2.3.0 RC 4 */
  lua_Number eps, power, erp, x0, x1, y0, y1, indices, *p, *q;
  size_t c, s, i, n, dim;
  int idx, type, rc;
  p = q = NULL;  /* avoid compiler warnings */
  type = lua_type(L, 1);
  luaL_typecheck(L, type == LUA_TSEQ, 1, "sequence expected", type);  /* 3.5.4 fix */
  s = agn_seqsize(L, 1);
  idx = agn_checkinteger(L, 2);
  if (idx < 1 || idx > s)
    luaL_error(L, "Error in " LUA_QS ": index %d out of range.", "stats.neighbours", idx);
  eps = agn_checkpositive(L, 3);  /* 2.21.5 change */
  power = agnL_optposint(L, 4, 2);
  indices = agnL_optboolean(L, 5, 0);
  erp = tools_intpow(eps, power);
  c = dim = n = 0;
  lua_seqrawgeti(L, 1, idx);  /* push point */
  /* determine dimension of point */
  if (lua_ispair(L, -1)) {
    dim = 2;
    type = 1;
    agnL_pairgetinumbers(L, "stats.neighbours", -1, &x0, &y0);
  } else if (lua_isseq(L, -1)) {
    dim = agn_seqsize(L, -1);
    type = 0;
    if (dim < 1)
      luaL_error(L, "Error in " LUA_QS ": dimension of point at index %d is non-positive.",
        idx, "stats.neighbours");
    p = agnL_tonumarray(L, -1, &n, "stats.neighbours", 0, 0);  /* 2.18.1 extension */
    if (p == NULL) return 1;  /* issue the error, 2.4.4 */
    agn_poptop(L);
    if (n != dim) {
      xfree(p);
      luaL_error(L, "Error in " LUA_QS ": point at index %d includes non-finite numbers.",
        "stats.neighbours", idx);
    }
  } else if (agnL_islinalgvector(L, -1, &dim)) {  /* linalg.vector ? */
    type = 2;
    p = agnL_tonumarray(L, -1, &n, "stats.neighbours", 0, 0);  /* 2.18.1 extension */
    if (p == NULL) return 1;  /* issue the error, 2.4.4 */
    agn_poptop(L);
  } else
    luaL_error(L, "Error in " LUA_QS ": wrong kind of point at index %d, got " LUA_QS ".",
      "stats.neighbours", idx, luaL_typename(L, -1));
  agn_createseq(L, 0);
  if (type == 1) {  /* check pairs ? */
    for (i=1; i <= s; i++) {
      lua_seqrawgeti(L, 1, i);
      rc = agn_pairgetinumbers(L, -1, &x1, &y1);  /* does not change the stack, 4.11.2 change */
      agn_poptop(L);  /* remove pair */
      if (!rc)
        luaL_error(L, "Error in " LUA_QS ": non-pair at position %d or pair with non-numbers.",
          "stats.neighbours", i);
      if (i != idx &&
          tools_intpow(sun_hypot(x0 - x1, y0 - y1), power) < erp) {  /* 2.9.8 improvement */
        c++;
        if (indices) {  /* agn_seqsetinumber is a macro, so use curly brackets */
          agn_seqsetinumber(L, -1, c, i);
        } else {
          lua_seqrawgeti(L, 1, i);
          lua_seqrawseti(L, -2, c);
        }
      }
    }
  } else if (type == 0) {  /* else check sequences of coordinates */
    lua_Number sum;
    size_t j;
    for (i=1; i <= s; i++) {
      lua_seqrawgeti(L, 1, i);
      if (!lua_isseq(L, -1)) {
        xfree(p);  /* 2.8.2 patch */
        luaL_error(L, "Error in " LUA_QS ": encountered wrong kind of point at index %d, got " LUA_QS ".",
          "stats.neighbours", i, luaL_typename(L, -1));
      }
      if (agn_seqsize(L, -1) != dim) {
        xfree(p);  /* 2.8.2 patch */
        luaL_error(L, "Error in " LUA_QS ": encountered point of different dimension at index %d.",
          "stats.neighbours", i);
      }
      q = agnL_tonumarray(L, -1, &n, "stats.neighbours", 0, 0);  /* 2.18.1 extension */
      if (q == NULL) return 1;  /* issue the error, 2.4.4 */
      agn_poptop(L);  /* pop point */
      if (n != dim) {
        xfreeall(p, q);  /* 2.8.2 patch, 2.9.8 */
        luaL_error(L, "Error in " LUA_QS ": point at index %d includes non-finite numbers.",
          "stats.neighbours", i);
      }
      if (i != idx) {
        sum = 0;  /* now determine Euclidean distance */
        for (j=0; j < n; j++)
          sum += (q[j] - p[j])*(q[j] - p[j]);
        if (tools_intpow(sqrt(sum), power) < erp) {
          if (indices) {  /* agn_seqsetinumber is a macro, so use curly brackets */
            agn_seqsetinumber(L, -1, ++c, i);
          } else {
            lua_seqrawgeti(L, 1, i);
            lua_seqrawseti(L, -2, ++c);
          }
        }
      }
      xfree(q);
    }
    xfree(p);
  } else {  /* else check linalg.vectors */
    lua_Number sum;
    size_t j;
    for (i=1; i <= s; i++) {
      lua_seqrawgeti(L, 1, i);
      if (!agnL_islinalgvector(L, -1, &n)) {
        xfree(p);  /* 2.8.2 patch */
        luaL_error(L, "Error in " LUA_QS ": wrong kind of point at index %d, got " LUA_QS ".",
          "stats.neighbours", i, luaL_typename(L, -1));
      }
      if (n != dim) {
        xfree(p);  /* 2.8.2 patch */
        luaL_error(L, "Error in " LUA_QS ": encountered vector of different dimension at index %d.",
          "stats.neighbours", luaL_typename(L, -1), i);
      }
      q = agnL_tonumarray(L, -1, &n, "stats.neighbours", 0, 0);  /* 2.18.1 extension */
      if (q == NULL) return 1;  /* issue the error, 2.4.4 */
      if (n != dim) {
        agn_poptop(L);
        xfreeall(p, q);  /* 2.9.8 */
        luaL_error(L, "Error in " LUA_QS ": encountered malformed vector at index %d.",
          "stats.neighbours", i);
      }
      agn_poptop(L);  /* pop point */
      if (i != idx) {
        sum = 0;  /* now determine Euclidean distance */
        for (j=0; j < n; j++)
          sum += (q[j] - p[j])*(q[j] - p[j]);
        if (tools_intpow(sqrt(sum), power) < erp) {
          if (indices) {  /* agn_seqsetinumber is a macro, so use curly brackets */
            agn_seqsetinumber(L, -1, ++c, i);
          } else {
            lua_seqrawgeti(L, 1, i);
            lua_seqrawseti(L, -2, ++c);
          }
        }
      }
      xfree(q);
    }
    xfree(p);
  }
  agn_seqresize(L, -1, c);
  return 1;
}


/* Returns all points in a distribution o (a sequence) that are in a residing vicinity eps (a positive number)
   of each other and returns them in a new sequence.

   Contrary to `stats.neighbours`, the function does not compute in Euclidean space, but in the space
   of a single dimension dim, which is 1 by default. It is assumed that the points in o are in the correct
   order. */

/*  indices = set only the index but not the point */
#define insertcoordinate(indices, c, i) { \
  if (indices) { \
    agn_seqsetinumber(L, -1, c, i); \
  } else { \
    lua_seqrawgeti(L, 1, i); \
    lua_seqrawseti(L, -2, c); \
  } \
}

static int stats_nearby (lua_State *L) {  /* 2.4.1 */
  lua_Number eps, epsilon, x0, x1, y0, y1, indices, *p, *q;
  size_t c, s, i, n, dim, dimspace;
  int type, rc;
  p = q = NULL;  /* avoid compiler warnings */
  type = lua_type(L, 1);
  luaL_typecheck(L, type == LUA_TSEQ, 1, "sequence expected", type);  /* 3.5.4 fix */
  s = agn_seqsize(L, 1);
  epsilon = agn_getepsilon(L);
  eps = agn_checkpositive(L, 2);  /* 2.21.5 change */
  dimspace = agnL_optposint(L, 3, 1);
  indices = agnL_optboolean(L, 4, 0);
  c = dim = n = 0;
  lua_seqrawgeti(L, 1, 1);  /* push point */
  /* determine dimension of point */
  if (lua_ispair(L, -1)) {
    dim = 2;
    type = 1;
    rc = agn_pairgetinumbers(L, -1, &x0, &y0);  /* 4.11.2 change */
    agn_poptop(L);  /* remove pair */
    if (!rc)
      luaL_error(L, "Error in " LUA_QS ": argument #1 is a pair with non-numbers.", "stats.nearby");
  } else if (lua_isseq(L, -1)) {
    dim = agn_seqsize(L, -1);
    type = 0;
    if (dim < 1)
      luaL_error(L, "Error in " LUA_QS ": dimension of point at index 1 is non-positive.", "stats.nearby");
    p = agnL_tonumarray(L, -1, &n, "stats.nearby", 0, 0);  /* 2.18.1 extension */
    if (p == NULL) return 1;  /* issue the error, 2.4.4 */
    agn_poptop(L);
    if (n != dim) {
      xfree(p);
      luaL_error(L, "Error in " LUA_QS ": point at index 1 includes non-numbers.", "stats.nearby");
    }
  } else if (agnL_islinalgvector(L, -1, &dim)) {  /* linalg.vector ? */
    type = 2;
    p = agnL_tonumarray(L, -1, &n, "stats.nearby", 0, 0);  /* 2.18.1 extension */
    if (p == NULL) return 1;  /* issue the error, 2.4.4 */
    agn_poptop(L);
  } else
    luaL_error(L, "Error in " LUA_QS ": wrong kind of point at index 1, got " LUA_QS ".",
      "stats.nearby", luaL_typename(L, -1));
  if (dimspace > dim)
    luaL_error(L, "Error in " LUA_QS ": dimension is invalid, maximum of %d allowed.", "stats.nearby", dim);
  agn_createseq(L, 0);
  dimspace--;  /* now go down to zero-based C arrays */
  if (type == 1) {  /* check pairs ? (no p) */
    for (i=2; i <= s; i++) {
      lua_seqrawgeti(L, 1, i);
      rc = agn_pairgetinumbers(L, -1, &x1, &y1);  /* 4.11.2 change */
      agn_poptop(L);  /* pop sequence value */
      if (!rc)
        luaL_error(L, "Error in " LUA_QS ": pair with non-numbers in sequence at position %d.", "stats.nearby", i);
      if ((dimspace == 0 && tools_approx(x1 - x0, eps, epsilon)) ||
          (dimspace == 1 && tools_approx(y1 - y0, eps, epsilon))) {
        if (i == 2) { insertcoordinate(indices, ++c, 1); }
        insertcoordinate(indices, ++c, i);
      }
      x0 = x1;
      y0 = y1;
    }
  } else if (type == 0) {  /* else check sequences of coordinates (p) */
    size_t j;
    for (i=2; i <= s; i++) {
      lua_seqrawgeti(L, 1, i);
      if (!lua_isseq(L, -1)) {
        xfree(p);  /* 2.8.2 patch */
        luaL_error(L, "Error in " LUA_QS ": encountered wrong kind of point at index %d, got " LUA_QS ".",
          "stats.nearby", i, luaL_typename(L, -1));
      }
      if (agn_seqsize(L, -1) != dim) {
        xfree(p);  /* 2.8.2 patch */
        luaL_error(L, "Error in " LUA_QS ": encountered point of different dimension at index %d.", "stats.nearby", i);
      }
      q = agnL_tonumarray(L, -1, &n, "stats.nearby", 0, 0);  /* 2.18.1 extension */
      if (q == NULL) return 1;  /* issue the error, 2.4.4 */
      agn_poptop(L);  /* pop point */
      if (n != dim) {
        xfreeall(p, q);  /* 2.8.2 patch, 2.9.8 */
        luaL_error(L, "Error in " LUA_QS ": point at index %d includes non-numbers.", "stats.nearby", i);
      }
      if (tools_approx(q[dimspace] - p[dimspace], eps, epsilon)) {
        if (i == 2) { insertcoordinate(indices, ++c, 1); }  /* determine distance in the dimension given by dimspace */
        insertcoordinate(indices, ++c, i);
      }
      for (j=0; j < dim; j++) p[j] = q[j];
      xfree(q);
    }
    xfree(p);
  } else {  /* else check linalg.vectors (p) */
    size_t j;
    for (i=1; i <= s; i++) {
      lua_seqrawgeti(L, 1, i);
      if (!agnL_islinalgvector(L, -1, &n)) {
        xfree(p);  /* 2.8.2 patch */
        luaL_error(L, "Error in " LUA_QS ": wrong kind of point at index %d, got " LUA_QS ".",
          "stats.nearby", i, luaL_typename(L, -1));
        }
      if (n != dim) {
        xfree(p);  /* 2.8.2 patch */
        luaL_error(L, "Error in " LUA_QS ": encountered vector of different dimension at index %d.",
          "stats.nearby", luaL_typename(L, -1), i);
      }
      q = agnL_tonumarray(L, -1, &n, "stats.nearby", 0, 0);  /* 2.18.1 extension */
      if (q == NULL) return 1;  /* issue the error, 2.4.4 */
      if (n != dim) {
        agn_poptop(L);
        xfreeall(p, q);  /* 2.8.2 patch, 2.9.8 */
        luaL_error(L, "Error in " LUA_QS ": encountered malformed vector at index %d.", "stats.nearby", i);
      }
      agn_poptop(L);  /* pop point */
      if (tools_approx(q[dimspace] - p[dimspace], eps, epsilon)) {
        if (i == 2) { insertcoordinate(indices, ++c, 1); }
        insertcoordinate(indices, ++c, i);
      }
      for (j=0; j < dim; j++) p[j] = q[j];
      xfree(q);
    }
    xfree(p);
  }
  agn_seqresize(L, -1, c);
  return 1;
}


/* Computes the normalised autocorrelation of a distribution (a table or sequence) of numbers at a given lag.
   If any third argument is passed, then the un-normalised autocorrelation is returned.

   It may be used to detect periodicy in a time series.

   A distribution is autocorrelated if stats.acf returns a negative or positive value significantly different
   from zero.

   Formula as defined in the NIST Engineering Statistics Handbook
   http://www.itl.nist.gov/div898/handbook/eda/section3/eda35c.htm */

static int stats_acf (lua_State *L) {
  size_t n, i, size;
  int option, lag;
  lua_Number *a, m, sum, udmean;
  volatile double ac, cs, ccs;
  udmean = sum = HUGE_VAL;
  luaL_checkany(L, 1);  /* agnL_tonumarray throws errors if argument is not a table or sequence */
  size = lua_gettop(L);
  lag = agn_checkinteger(L, 2);  /* the lag */
  if (lag < 0)
    luaL_error(L, "Error in " LUA_QS ": lag must be non-negative.", "stats.acf");
  option = !lua_isnoneornil(L, 3);
  if (size >= 4 && agn_isnumber(L, 4))  /* 2.8.2 */
    udmean = agn_tonumber(L, 4);  /* pre-comuted mean */
  if (size > 4 && agn_isnumber(L, 5))  /* 2.8.2 */
    sum = agn_tonumber(L, 5);  /* pre-computed sum of all values */
  n = ac = cs = ccs = 0;
  a = agnL_tonumarray(L, 1, &n, "stats.acf", 0, 0);  /* 2.18.1 extension */
  if (a == NULL) return 1;  /* issue the error */
  if (lag >= n) {
    if (!lua_isuserdata(L, 1)) { xfree(a); }  /* 2.8.2 patch */
    luaL_error(L, "Error in " LUA_QS ": lag out of upper bound.", "stats.acf");
  }
  m = (udmean == HUGE_VAL) ? KahanBabuskaMeanNumArray(a, n) : udmean;  /* 2.8.2 improvement */
  sum = (sum == HUGE_VAL) ? KahanBabuskaSumdata(a, n, 2, m) : sum;  /* 2.8.2 improvement */
  for (i=0; i < n - lag; i++)
    ac = tools_kbadd(ac, (a[i] - m) * (a[i + lag] - m), &cs, &ccs);  /* 3.7.2 change to Kahan-Babuska summation */
  ac += cs + ccs;
  /* no option given ? return ac / (n * variance), i.e. a normalised result */
  lua_pushnumber(L, (option) ? ac : ac / sum);
  if (!lua_isuserdata(L, 1)) { xfree(a); }
  return 1;
}


/* Returns a sequence of autocorrelations through the given number of lags.
   If any third argument is passed, then the un-normalised autocorrelations are returned.

   Formula as defined in the NIST Engineering Statistics Handbook
   http://www.itl.nist.gov/div898/handbook/eda/section3/eda35c.htm

import stats
a := utils.readcsv('../../Downloads/lew.dat', header=true)
stats.acv(a, 0):
*/
static int stats_acv (lua_State *L) {
  size_t n, i, j;
  int option, lag, type;
  lua_Number *a, m, secondmoment;
  volatile double s, cs, ccs;
  type = lua_type(L, 1);
  luaL_typecheck(L, type == LUA_TTABLE || type == LUA_TSEQ || type == LUA_TREG || type == LUA_TUSERDATA, 1,
    "table, sequence, register or numarray expected", type);
  lag = agn_checkinteger(L, 2);  /* the maximum lag */
  if (lag < 0)
    luaL_error(L, "Error in " LUA_QS ": lag must be non-negative.", "stats.acv");  /* corrected 2.4.4 */
  option = !lua_isnoneornil(L, 3);
  n = 0;
  a = agnL_tonumarray(L, 1, &n, "stats.acv", 0, 0);  /* corrected 2.4.4 */
  if (a == NULL) return 1;  /* issue the error */
  if (lag >= n) {
    if (!lua_isuserdata(L, 1)) { xfree(a); }  /* 2.8.2 patch */
    luaL_error(L, "Error in " LUA_QS ": lag out of upper bound.", "stats.acv");  /* corrected 2.4.4 */
  }
  m = KahanBabuskaMeanNumArray(a, n);  /* 2.4.3 improvement */
  secondmoment = (option) ? 1 : KahanBabuskaSumdata(a, n, 2, m);  /* (n * variance) */
  switch (type) {
    case LUA_TTABLE: {  /* new 4.11.7 */
      lua_createtable(L, n - lag, 0);
      for (j=0; j <= lag; j++) {
        s = cs = ccs = 0;
        for (i=0; i < n - j; i++)
          s = tools_kbadd(s, (a[i] - m) * (a[i + j] - m), &cs, &ccs);  /* 3.7.2 change to Kahan-Babuska summation */
        s += cs + ccs;
        agn_setinumber(L, -1, j + 1, (option) ? s : s / secondmoment);
      }
      break;
    }
    case LUA_TSEQ: {
      agn_createseq(L, n - lag);
      for (j=0; j <= lag; j++) {
        s = cs = ccs = 0;
        for (i=0; i < n - j; i++)
          s = tools_kbadd(s, (a[i] - m) * (a[i + j] - m), &cs, &ccs);  /* 3.7.2 change to Kahan-Babuska summation */
        s += cs + ccs;
        agn_seqsetinumber(L, -1, j + 1, (option) ? s : s / secondmoment);
      }
      break;
    }
    case LUA_TREG: {  /* new 4.11.7 */
      agn_createreg(L, n - lag);
      for (j=0; j <= lag; j++) {
        s = cs = ccs = 0;
        for (i=0; i < n - j; i++)
          s = tools_kbadd(s, (a[i] - m) * (a[i + j] - m), &cs, &ccs);  /* 3.7.2 change to Kahan-Babuska summation */
        s += cs + ccs;
        agn_regsetinumber(L, -1, j + 1, (option) ? s : s / secondmoment);
      }
      break;
    }
    case LUA_TUSERDATA: {  /* new 4.11.7 */
      int ii = n - lag;
      NumArray *b = NULL;
      numarray_createdouble(L, b, ii);
      for (j=0; j <= lag; j++) {
        s = cs = ccs = 0;
        for (i=0; i < n - j; i++)
          s = tools_kbadd(s, (a[i] - m) * (a[i + j] - m), &cs, &ccs);  /* 3.7.2 change to Kahan-Babuska summation */
        s += cs + ccs;
        b->data.n[j] = (option) ? s : s / secondmoment;
      }
      break;
    }
    default: lua_assert(0);  /* should not happen */
  }
  if (!lua_isuserdata(L, 1)) { xfree(a); }
  return 1;
}


/* Checks whether at least one element in a table or sequence obj is non-zero and returns `true` or `false`. If the
   second argument eps, a non-negative number, is passed, the function returns `true` if at least one observation
   x in o satisfies the condition abs(x) > eps. By default eps is 0.

   Idea taken frm Matlab's any function, see: http://de.mathworks.com/help/matlab/ref/any.html. 2.4.1 */
static int stats_isany (lua_State *L) {
  size_t i, n;
  int type;
  lua_Number eps;
  type = lua_type(L, 1);
  luaL_typecheck(L, type == LUA_TTABLE || type == LUA_TSEQ || type == LUA_TREG || type == LUA_TUSERDATA, 1,
    "table, sequence, register or numarray expected", type);
  eps = agnL_optnumber(L, 2, 0);
  if (eps < 0)
    luaL_error(L, "Error in " LUA_QS ": threshold must be non-negative.", "stats.isany");
  if (isnumarray(L, 1)) {  /* 3.5.4 */
    NumArray *ud = lua_touserdata(L, 1);
    if (ud->datatype != NADOUBLE)
      luaL_error(L, "Error in " LUA_QS ": numarray must contain Agena numbers (C doubles).", "stats.isany");
    n = ud->size;
  } else
    n = agn_nops(L, 1);  /* number of elements in structure */
  if (n < 2) {
    lua_pushfail(L);
    return 1;
  }
  switch (type) {
    case LUA_TTABLE: {
      for (i=1; i <= n; i++) {
        if (fabs(agn_getinumber(L, 1, i)) > eps) {
          lua_pushtrue(L);
          return 1;
        }
      }
      break;
    }
    case LUA_TSEQ: {
      for (i=1; i <= n; i++) {
        if (fabs(agn_seqgetinumber(L, 1, i)) > eps) {  /* 2.4.2 */
          lua_pushtrue(L);
          return 1;
        }
      }
      break;
    }
    case LUA_TREG: {  /* 3.18.8 */
      for (i=1; i <= n; i++) {
        if (fabs(agn_reggetinumber(L, 1, i)) > eps) {
          lua_pushtrue(L);
          return 1;
        }
      }
      break;
    }
    case LUA_TUSERDATA: {  /* 3.5.4 */
      lua_Number *a;
      NumArray *ud;
      if (!isnumarray(L, 1))
        luaL_error(L, "Error in " LUA_QS ": structure is not a numarray.", "stats.isany");
      ud = lua_touserdata(L, 1);
      a = ud->data.n;
      for (i=0; i < n; i++) {
        if (fabs(a[i]) > eps) {
          lua_pushtrue(L);
          return 1;
        }
      }
      break;
    }
    default: lua_assert(0);  /* should not happen */
  }
  lua_pushfalse(L);
  return 1;
}


/* Checks whether all elements in a table or sequence o are non-zero and returns `true` or `false`. If the
   second argument eps, a non-negative number, is passed, the function returns `true` if all observations
   x in o satisfies the condition abs(x) > eps. By default eps is 0.

   Idea taken frm Matlab's all function, see: http://de.mathworks.com/help/matlab/ref/all.html. 2.4.1 */
static int stats_isall (lua_State *L) {
  size_t i, n;
  int type;
  lua_Number eps;
  type = lua_type(L, 1);
  luaL_typecheck(L, type == LUA_TTABLE || type == LUA_TSEQ || type == LUA_TREG || type == LUA_TUSERDATA, 1,
    "table, sequence, register or numarray expected", type);
  eps = agnL_optnumber(L, 2, 0);
  if (eps < 0)
    luaL_error(L, "Error in " LUA_QS ": threshold must be non-negative.", "stats.isall");
  if (isnumarray(L, 1)) {  /* 3.5.4 */
    NumArray *ud = lua_touserdata(L, 1);
    if (ud->datatype != NADOUBLE)
      luaL_error(L, "Error in " LUA_QS ": numarray must contain Agena numbers (C doubles).", "stats.isany");
    n = ud->size;
  } else
    n = agn_nops(L, 1);  /* number of elements in structure */
  if (n < 2) {
    lua_pushfail(L);
    return 1;
  }
  switch (type) {
    case LUA_TTABLE: {
      for (i=1; i <= n; i++) {
        if (fabs(agn_getinumber(L, 1, i)) <= eps) {
          lua_pushfalse(L);
          return 1;
        }
      }
      break;
    }
    case LUA_TSEQ: {
      for (i=1; i <= n; i++) {
        if (fabs(agn_seqgetinumber(L, 1, i)) <= eps) {  /* 2.4.2 fix */
          lua_pushfalse(L);
          return 1;
        }
      }
      break;
    }
    case LUA_TREG: {  /* 3.18.8 */
      for (i=1; i <= n; i++) {
        if (fabs(agn_reggetinumber(L, 1, i)) <= eps) {
          lua_pushfalse(L);
          return 1;
        }
      }
      break;
    }
    case LUA_TUSERDATA: {  /* 3.5.4 */
      lua_Number *a;
      NumArray *ud;
      if (!isnumarray(L, 1))
        luaL_error(L, "Error in " LUA_QS ": structure is not a numarray.", "stats.isall");
      ud = lua_touserdata(L, 1);
      a = ud->data.n;
      for (i=0; i < n; i++) {
        if (fabs(a[i]) <= eps) {
          lua_pushtrue(L);
          return 1;
        }
      }
      break;
    }
    default: lua_assert(0);  /* should not happen */
  }
  lua_pushtrue(L);
  return 1;
}


/* The function checks whether the given coordinate c is a pair with both its left-hand and right-hand side being numbers.
   If a second argument, a string, is given, then error messages of `stats.checkcoordinate` refer to the given procedure
   procname as function issuing the error. Otherwise the error message includes a reference to `stats.checkcoordinate`
   itself. */
static int stats_checkcoordinate (lua_State *L) {  /* 2.4.1, code-optimised 2.10.1 */
  lua_Number x, y;
  size_t nargs;
  const char *procname;
  nargs = lua_gettop(L);
  procname = luaL_optstring(L, 2, "stats.checkcoordinate");
  if (nargs < 1)  /* 2.4.5 fix */
    luaL_error(L, "Error in " LUA_QS ": expected at least one argument.", procname);
  if (!agn_pairgetinumbers(L, 1, &x, &y))  /* 4.11.2 change */
    luaL_error(L, "Error in " LUA_QS ": expected a pair with numbers.", procname);
  luaL_checkstack(L, 2, "not enough stack space");  /* 3.15.4 fix */
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  return 2;
}


/* The studentst[nu] distribution has the probability density function
   GAMMA( (nu+1)/2 )/GAMMA(nu/2)/sqrt(nu*Pi)/(1+t^2/nu)^((nu+1)/2), with nu a positive integer. */
static int stats_studentst (lua_State *L) {  /* suggested by Slobodan, 2.5.1, equivalent to
  Maple's stats[statevalf,pdf,studentst[v]](t); */
  lua_Number t, nu, n, d;
  t = agn_checknumber(L, 1);
  nu = agn_checkposint(L, 2);  /* changed 2.21.5, behave like in Maple */
  n = tools_gamma((nu + 1)*0.5);  /* 2.13.0 change; 2.17.4/7 tweak */
  d = sqrt(nu*PI) * tools_gamma(nu*0.5);  /* 2.13.0 change; 2.17.4/7 tweak */
  lua_pushnumber(L, n/d * sun_pow(1 + t*t/nu, -(nu + 1)*0.5, 0));  /* 2.14.13 change from pow to sun_pow, 2.17.7 tweak */
  return 1;
}


/* The normald[mu, sigma] distribution has the probability density function:
   exp( -(x-mu)^2/2/sigma^2 ) / sqrt(2*Pi*sigma^2). positive si is the standard deviation.

   Example in Maple:

   > with(stats,statevalf):
   > with(statevalf):
   > pdf[normald[10.,10]](9);
   .03969525473 */
static int stats_normald (lua_State *L) {  /* suggested by Slobodan, 2.5.1, equivalent to
  Maple's stats[statevalf,pdf,normald[mu, sigma]](x); 1.5 % tweak 7.3.5 by Gemini AI */
  lua_Number t, mu, diff, sigma, sigma2;
  t = agn_checknumber(L, 1);
  mu = agnL_optnumber(L, 2, 0);  /* 3.7.4 change */
  sigma = agnL_optpositive(L, 3, 1);
  sigma2 = tools_square(sigma);  /* changed 2.21.5, 3.7.4 tweak */
  diff = t - mu;
  lua_pushnumber(L, sun_exp(-tools_square(diff)/(2.0*sigma2))/(sigma*SQRTPI2));
  return 1;
}


static int stats_lognormald (lua_State *L) {  /* Maple's stats[statevalf,pdf,lognormald[mu, sigma]](x); 3.7.4 */
  lua_Number x, mu, sigma;
  x = agn_checkpositive(L, 1);
  mu = agnL_optnumber(L, 2, 0);
  sigma = agnL_optpositive(L, 3, 1);
  lua_pushnumber(L, 1/(x*SQRTPI2*sigma) * sun_exp(-0.5*(tools_square((sun_log(x) - mu)/sigma))));
  return 1;
}


/* Implements the cumulative density function for the standard normal distribution, works like statevalf[cdf,normald]()
   in Maple, i.e. evalf(1/sqrt(2*Pi)*int(exp(-t^2/2), t=-infinity .. x));  see also: stats.cdf */
static int stats_cdfnormald (lua_State *L) {  /* 2.21.5 */
  lua_pushnumber(L, 0.5*sun_erfc(-agn_checknumber(L, 1)/SQRT2));  /* 3.1.3 tweak */
  return 1;
}


/* Inverse of Normal distribution function
 *
 * SYNOPSIS:
 *
 * double x, y, ndtri();
 *
 * x = ndtri( y );
 *
 * DESCRIPTION:
 *
 * Returns the argument, x, for which the area under the
 * Gaussian probability density function (integrated from
 * minus infinity to x) is equal to y.
 *
 * SEE ALSO: stats.normald */

static int stats_invnormald (lua_State *L) {  /* 2.8.2 */
  lua_pushnumber(L, ndtri(agn_checknumber(L, 1)));
  return 1;
}


/* The chisquare[nu] distribution has the probability density function
   x^((mu-2)/2) exp(-x/2)/2^(mu/2)/GAMMA(mu/2), x>0, mu a posint.
   suggested by Slobodan, 2.5.1, equivalent to Maple's stats[statevalf,pdf,chisquare[v]](x);
   see ?stats,distributions. The long double version is much slower but not more accurate. */
static int stats_chisquare (lua_State *L) {
  lua_Number x, mu, t1, t2;
  x = agn_checkpositive(L, 1);  /* changed 2.21.5 */
  mu = agn_checkposint(L, 2);  /* changed 2.21.5, behave like in Maple */
  t1 = sun_pow(x, (mu - 2)*0.5, 0);  /* 2.14.13 change from pow to sun_pow */
  /* 2.13.0 change, 2.14.13 change from pow/exp to sun_pow/sun_exp; 2.17.4/7 tweak */
  t2 = sun_exp(-0.5*x)/sun_pow(2, 0.5*mu, 0)/tools_gamma(0.5*mu);
  lua_pushnumber(L, t1*t2);
  return 1;
}


/* The fratio[nu1, nu2] distribution (also known as Fischer f distribution) has the probability density function
      GAMMA( (nu1+nu2)/2)/GAMMA(nu1/2)/GAMMA(nu2/2)*(nu1/nu2)^(nu1/2)*
      x^((nu1-2)/2) / ( 1+ (nu1/nu2)*x) ^ ((nu1+nu2)/2), x>0, with nu1 and nu2 positive integers.
   Constraints: nu1, nu2 are positive integers. */
static int stats_fratio (lua_State *L) {  /* suggested by Slobodan, 2.5.1, equivalent to Maple's
  stats[statevalf,pdf,fratio[nu1, nu2]](x); */
  lua_Number x, nu1, nu2, t1, t2;
  x = agn_checkpositive(L, 1);  /* changed 2.21.5 */
  nu1 = agn_checkposint(L, 2);  /* changed 2.21.5 */
  nu2 = agn_checkposint(L, 3);  /* changed 2.21.5 */
  /* 2.13.0 change, 2.14.13 change from pow to sun_pow; 2.17.7 tweak */
  t1 = tools_gamma((nu1 + nu2)*0.5)/tools_gamma(nu1*0.5)/tools_gamma(nu2*0.5)*sun_pow(nu1/nu2, nu1*0.5, 0);
  /* 2.14.13 change from pow to sun_pow, 2.17.7 tweak */
  t2 = sun_pow(x, (nu1 - 2)*0.5, 0) / sun_pow(1 + (nu1/nu2)*x, (nu1 + nu2)*0.5, 0);
  lua_pushnumber(L, t1 * t2);
  return 1;
}


/* The cauchy[a, b] distribution has the probability density function 1/(Pi*b*(1+((x-a)/b)^2)), b>0.
   Maple V R4: stats[statevalf,pdf,cauchy[2, 3]](4); -> .73456127580874770356e-1 */
static int stats_cauchy (lua_State *L) {  /* 2.5.2, equivalent to Maple's stats[statevalf,pdf,fratio[nu1, nu2]](x); */
  lua_Number x, a, b, t;
  x = agn_checknumber(L, 1);
  a = agn_checknumber(L, 2);
  b = agn_checkpositive(L, 3);  /* changed 2.21.5 */
  t = (x - a)/b;
  lua_pushnumber(L, 1/(PI*b*(1 + t*t)));
  return 1;
}


/* The Durbin-Watson test detects the autocorrelation in the residuals from a linear regression and returns

d = sum((obj[i] - obj[i - 1])^2, i=2 .. n)/sum(obj[i]^2, i=1 .. n);

If d is equal to 2, it indicated the absence of autocorrelation. If d is less than 2, it indicates positive
autocorrelation; if d is greater than 2 it indicates negative autocorrelation and that the observations are
very different from each other. If d is less than 1, the regression should be checked. The function uses
Kahan-Babuska roundoff prevention. */
static int stats_durbinwatson (lua_State *L) {
  size_t i, ii;
  lua_Number *a;
  luaL_checkany(L, 1);
  ii = 0;
  a = agnL_tonumarray(L, 1, &ii, "stats.durbinwatson", 0, 0);  /* 2.18.1 extension */
  if (a == NULL) return 1;  /* issue the error */
  if (ii < 2)
    lua_pushfail(L);
  else {
    volatile lua_Number s, cs, ccs, t, c, cc, x;
    s = cs = ccs = 0;
    for (i=1; i < ii; i++) {
      x = a[i] - a[i - 1];
      x *= x;
      t = s + x;
      c = (fabs(s) >= fabs(x)) ? (s - t) + x : (x - t) + s;
      s = t;
      t = cs + c;
      cc = (fabs(cs) >= fabs(c)) ? (cs - t) + c : (c - t) + cs;
      cs = t;
      ccs = ccs + cc;
    }
    lua_pushnumber(L, (s + cs + ccs)/KahanBabuskaSumdata(a, ii, 2, 0));
  }
  if (!lua_isuserdata(L, 1)) { xfree(a); }
  return 1;
}


/* Computes the covariance of distributions x and y, which is
   cov(x,y) = [1/n or 1/(n-1)] * sum((x[i] - mean(x)) * (y[i]-mean(y)), i = 1 .. n).
   See the math definition at http://www.cquest.utoronto.ca/geog/ggr270y/notes/not05efg.html.
   By default, the population covariance
      cov(x,y) = 1/n * sum((x[i] - mean(x)) * (y[i] - mean(y)), i = 1 .. n)
   is returned. If you pass any third argument then the sample covariance
      cov(x,y) = 1/(n-1) * sum((x[i] - mean(x)) * (y[i] - mean(y)), i = 1 .. n)
   is computed.

   The covariance indicates the tendency in the linear relationship between two distributions: if it is
   positive, the distributions are similar, if it is negative, they are not.

   This is a port of the Java function `covariance` written by Peter Gedeck and Wolfgang Hoschek in 1999,
   included in the COLT package published by CERN Geneva, see file /Colt-master/src/cern/jet/stat/Descriptive.java.
   The algorithm used is faster than the formula above. 2.5.3 */
static int stats_covar (lua_State *L) {
  size_t i, ii, jj;
  int sample;
  lua_Number *a, *b, x, y, sx, sy, sxy;
  luaL_checkany(L, 2);
  ii = jj = 0;
  a = agnL_tonumarray(L, 1, &ii, "stats.covar", 0, 0);  /* 2.18.1 extension */
  if (a == NULL) return 1;  /* issue the error */
  b = agnL_tonumarray(L, 2, &jj, "stats.covar", 0, 0);  /* 2.18.1 extension */
  if (b == NULL) return 1;  /* issue the error */
  sample = !lua_isnoneornil(L, 3);
  if (ii != jj || ii < 2)  /* in this order */
    lua_pushfail(L);
  else {
    sx = a[0]; sy = b[0]; sxy = 0;
    for (i=1; i < ii; i++) {
      x = a[i]; y = b[i];
      sx += x;
      sxy += (x - sx/(i + 1)) * (y - sy/i);
      sy += y;
    }
    lua_pushnumber(L, sxy/(ii - sample));
  }
  if (!lua_isuserdata(L, 1)) { xfree(a); }
  if (!lua_isuserdata(L, 2)) { xfree(b); }
  return 1;
}


/*              gdtr.c
 *
 *  Gamma distribution function
 *
 *
 *
 * SYNOPSIS:
 *
 * double a, b, x, y, gdtr();
 *
 * y = gdtr( x, a, b );
 *
 *
 *
 * DESCRIPTION:
 *
 * Returns the integral from zero to x of the gamma probability
 * density function:
 *
 *                x
 *        b       -
 *       a       | |   b-1  -at
 * y =  -----    |    t    e    dt
 *       -     | |
 *      | (b)   -
 *               0
 *
 *  The incomplete gamma integral is used, according to the
 * relation
 *
 * y = igam( b, ax ).
 *
 *
 * ACCURACY:
 *
 * See igam().
 *
 * ERROR MESSAGES:
 *
 *   message         condition      value returned
 * gdtr domain         x < 0            0.0
 *
 */

static int stats_gammad (lua_State *L) {  /* Gamma distribution function, 2.8.1 */
  lua_Number x, a, b;
  x = agn_checknumber(L, 1);
  a = agn_checknumber(L, 2);
  b = agn_checknumber(L, 3);
  if (x < 0)
    lua_pushundefined(L);
  else
    lua_pushnumber(L, igam(b, a*x));
  return 1;
}


/*              gdtrc.c
 *
 *  Complemented gamma distribution function
 *
 *
 *
 * SYNOPSIS:
 *
 * double a, b, x, y, gdtrc();
 *
 * y = gdtrc( x, a, b );
 *
 *
 *
 * DESCRIPTION:
 *
 * Returns the integral from x to infinity of the gamma
 * probability density function:
 *
 *               inf.
 *        b       -
 *       a       | |   b-1  -at
 * y =  -----    |    t    e    dt
 *       -     | |
 *      | (b)   -
 *               x
 *
 *  The incomplete gamma integral is used, according to the
 * relation
 *
 * y = igamc( b, ax ).
 *
 *
 * ACCURACY:
 *
 * See igamc().
 *
 * ERROR MESSAGES:
 *
 *   message         condition      value returned
 * gdtrc domain         x < 0            0.0
 *
 */
/*              gdtr()  */
/*
Cephes Math Library Release 2.8:  June, 2000
Copyright 1984, 1987, 1995, 2000 by Stephen L. Moshier
*/

static int stats_gammadc (lua_State *L) {  /* complemented Gamma distribution function, 2.8.1 */
  lua_Number x, a, b;
  x = agn_checknumber(L, 1);
  a = agn_checknumber(L, 2);
  b = agn_checknumber(L, 3);
  if (x < 0)
    lua_pushundefined(L);
  else
    lua_pushnumber(L, igamc(b, a*x));  /* 2.8.2 fix */
  return 1;
}


/* Returns the winsorised mean. The function first replaces the first n percent of a distribution at the low
   and high end with the most extreme remaining values, and then calculates the arithmetic mean of the entire
   modified distribution. By default, n is 10.

   The winsorised mean is more resistant to outliers than the traditional arithmetic mean. See also: stats.trimean,
   stats.iqr. */
static int stats_winsor (lua_State *L) {  /* 2.10.0, based on stats.trimean */
  size_t i, ii, p, q;
  int64_t rankp, rankq;
  lua_Number x1, x2, *a;
  luaL_checkany(L, 1);
  a = agnL_tonumarray(L, 1, &ii, "stats.winsor", 1, 0);  /* 2.18.1 extension */
  if (a == NULL) return 1;  /* issue the error */
  p = agnL_optinteger(L, 2, 10);
  if (p > 99) {  /* 7.9.5 change, no check for negative value any longer */
    xfree(a);
    luaL_error(L, "Error in " LUA_QS ": second argument must be in the range [0, 100).", "stats.winsor");
  }
  q = 100 - p;
  if (ii < 2)
    lua_pushfail(L);
  else {
    x1 = a[0];
    for (i=1; i < ii; i++) {
      x2 = a[i];
      if (x1 > x2) {  /* array unsorted ? -> sort it in ascending order */
        /* tools_dintrosort(a, 0, ii - 1, 0, 2*sun_log2(ii)); */
        klib_dintrosort(a, ii);  /* 7.7.4 10 % speed-up */
        break;
      }
      x1 = x2;
    }
    rankp = FLOOR((lua_Number)p/100*(ii + 1)) - 1;  /* position of the 1. quartile */
    rankq = FLOOR((lua_Number)q/100*(ii + 1)) - 1;  /* position of the 3. quartile */
    if (rankp < 0) rankp = 0;
    if (rankq >= ii) rankq = ii - 1;
    for (i=0; i < rankp; i++) a[i] = a[rankp];
    for (i=rankq + 1; i < ii; i++) a[i] = a[rankq];
    lua_pushnumber(L, KahanBabuskaMeanNumArray(a, ii));
  }
  xfree(a);
  return 1;
}


/* In the first form, the function inserts all the elements in sequence obj into a new sequence and returns it.
   In the second form, it inserts all the given arguments into a new sequence and returns it.

   In both forms, only numbers and pairs are accepted in the sequence or argument list. In case of a pair x:n,
   x denotes an observation and n, a non-negative integer, denotes the number of occurrence of x, so x is inserted
   n times into the new sequence. */
static int stats_weights (lua_State *L) {  /* 2.10.0, code-optimised 2.10.1 */
  size_t n, i, c;
  int isseq;
  n = lua_gettop(L);
  agn_createseq(L, n);
  c = 0;
  isseq = lua_isseq(L, 1);
  if (isseq) n = agn_seqsize(L, 1);
  for (i=1; i <= n; i++) {
    if (isseq)
      lua_seqrawgeti(L, 1, i);
    else
      lua_pushvalue(L, i);
    if (agn_isnumber(L, -1)) {
      lua_seqrawseti(L, -2, ++c);  /* pops value */
    } else if (lua_ispair(L, -1)) {
      lua_Number x, p;
      size_t j;
      agnL_pairgetinumbers(L, "stats.weights", -1, &x, &p);  /* pops pair */
      if (!tools_isnonnegint(p)) {  /* 2.14.13 change */
        luaL_error(L, "Error in " LUA_QS ": right-hand side must be a non-negative integer.", "stats.weights");
      }
      for (j=0; j < (size_t)p; j++) {
        agn_seqsetinumber(L, -1, ++c, x);
      }
    } else {
      /* do not pop value */
      luaL_error(L, "Error in " LUA_QS ": expected either a number or pair.",
        "stats.weights", luaL_typename(L, -1));
    }
  }
  return 1;
}


static int stats_rank (lua_State *L) {  /* 2.12.5 UNDOC */
  size_t n;
  int type, l, r;
  lua_Number x, yl, yr;
  type = lua_type(L, 1);
  luaL_typecheck(L, type == LUA_TTABLE || type == LUA_TSEQ || type == LUA_TREG, 1,
                 "table, sequence or register expected", type);
  n = agn_nops(L, 1);
  x = agn_checknumber(L, 2);
  l = agnL_optposint(L, 3, 1);
  r = agnL_optposint(L, 4, n);
  yl = yr = 0;
  if (l >= r || n < 2 || n < r || l < 1) {
    lua_pushfail(L);
    return 1;
  }
  switch (type) {
    case LUA_TTABLE: {
      yl = agn_getinumber(L, 1, l);
      yr = agn_getinumber(L, 1, r);
      break;
    }
    case LUA_TSEQ: {
      yl = lua_seqrawgetinumber(L, 1, l);
      yr = lua_seqrawgetinumber(L, 1, r);
      break;
    }
    case LUA_TREG: {  /* 3.18.8 */
      int rc;
      yl = agn_regrawgetinumber(L, 1, l, &rc);
      if (!rc) yl = HUGE_VAL;  /* 4.4.7 change */
      yr = agn_regrawgetinumber(L, 1, r, &rc);
      if (!rc) yr = HUGE_VAL;  /* ditto */
      break;
    }
    default: lua_assert(0);  /* should not happen */
  }
  lua_pushnumber(L, yr - yl == 0.0 ? AGN_NAN : l + (r - l)*(x - yl)/(yr - yl));  /* 6.1.2 fix */
  return 1;
}


/* Implements the quantile function associated with the standard normal distribution, or in other words:
   the inverse of the cumulative distribution function of the standard normal distribution for number x.
   It returns the number sqrt(2)*inverf(2*p - 1). 2.21.5 */
static int stats_probit (lua_State *L) {
  lua_pushnumber(L, SQRT2*tools_inverf(2*agn_checknumber(L, 1) - 1));
  return 1;
}


/* Geometric Cumulative (Probability Point) Distribution, 2.41.0
   taken from: http://www.mymathlib.com/functions/probability/geometric_distribution.html
   Copyright © 2004 RLH. All rights reserved. */
static int stats_geometric (lua_State *L) {
  lua_Number p, r;
  int k, point;
  k = agn_checknonnegint(L, 1);
  p = agn_checknumber(L, 2);
  point = agnL_optboolean(L, 3, 0);
  if (k < 0) r = 0.0;
  else if (p == 0.0) r = 0.0;
  else if (p == 1.0) r = (point) ? ((k == 0) ? 1.0 : 0.0) : 1.0;
  else r = (point) ? p*sun_pow(1.0 - p, (double)k, 1) : 1.0 - sun_pow(1.0 - p, (double)(k + 1), 1);
  lua_pushnumber(L, r);
  return 1;
}


/* Logistic Density Distribution & its PDF, 2.41.0
   F(x) = 1/(1 + exp(-x))
   f(x) = exp(-x)/(1 + exp(-x))^2
   taken from: http://www.mymathlib.com/functions/probability/logistic_distribution.html
   Copyright © 2004 RLH. All rights reserved.
   See also: https://www.statisticshowto.com/logistic-distribution/
   Example for Maple V R4:
   > with(stats);
   > with(statevalf);
   > pdf[logistic[9, 5]](-8.5);
   .005690604778 */
static int stats_logistic (lua_State *L) {
  long double x = agn_checknumber(L, 1);
  int onearg = (lua_gettop(L) == 1);
  luaL_checkstack(L, 1 + onearg, "not enough stack space");  /* 3.15.4 fix / 3.18.4 fix */
  if (onearg) {
    long double t, u;
    t = tools_expl(-x);
    u = 1.0L + t;
    lua_pushnumber(L, 1.0L/(1.0L + t));  /* Logistic distribution ... */
    lua_pushnumber(L, t/u);  /* ... and its corresponding probability density function */
  } else {
    /* 3.7.5, computes the probability density function of the Logistic[a, b] distribution,
       as implemented in Maple V Release 4:
    > e := exp( - (x-a)/b ) / b / (1 + exp(-(x-a)/b))^2:
    > assume(x, real, a, real, b, real);
    > simplify(ln(e), ln); */
    long double a, b;
    a = agn_checknumber(L, 2);
    b = agn_checknumber(L, 3);
    if (b == 0)  /* 3.7.7 fix */
      lua_pushundefined(L);
    else if (b > 0)
      lua_pushnumber(L, tools_expl(-(x - a)/b - 2*tools_logl(1 + tools_expl(-(x - a)/b)) + tools_logl(1/b)) );
    else  /* 3.7.7 fix */
      lua_pushnumber(L, sun_exp(-(x - a)/b ) / b / tools_square(1 + sun_exp(-(x - a)/b)));
  }
  return 1 + onearg;
}


/* Logarithmic Series Cumulative Distribution, 2.41.0
   f(k, p) = -p^k/[k ln(1 - p)], k = 1, ...
   k: argument of the logarithmic distribution, k >= 1.
   p: shape parameter of a logarithmic distribution, 0 < p < 1.
   taken from: http://www.mymathlib.com/functions/probability/log_series_distribution.html
   Copyright © 2004 RLH. All rights reserved. */
static int stats_logseries (lua_State *L) {
  int k, i;
  long double p, summand, sum, r;
  k = agn_checkinteger(L, 1);
  p = agn_checknumber(L, 2);
  summand = p;
  sum = summand;
  if (k <= 0) r = 0.0;
  else if (p <= 0 || p >= 1) r = AGN_NAN;
  else {
    for (i=2; i <= k; i++) {
      summand *= p;
      sum += summand/(long double)i;
    }
    r = -sum/tools_logl(1.0L - p);
  }
  lua_pushnumber(L, r);
  return 1;
}


/* Laplace Distribution and Laplace Density, 2.41.1
   taken from: http://www.mymathlib.com/functions/probability/laplace_distribution.html
   Copyright © 2004 RLH. All rights reserved. */
#define LN2L   6.931471805599453094172321214581e-1L
static int stats_laplace (lua_State *L) {
  long double x = agn_checknumber(L, 1);
  int onearg = (lua_gettop(L) == 1);
  luaL_checkstack(L, 1 + onearg, "not enough stack space");  /* 3.15.4 fix / 3.18.4 fix */
  if (onearg) {
    long double t;
    t = 0.5L*tools_expl(-tools_fabsl(x));
    lua_pushnumber(L, (x <= 0.0L) ? t : 1.0L - t);  /* Laplace Distribution */
    lua_pushnumber(L, t);  /* probability density function */
  } else {
    /* 3.7.5, computes the probability density function of the Logistic[a, b] distribution
       as implemented in Maple V Release 4:
    > f := 1/(2*b) * exp(-abs(x-a)/b);
    > simplify(ln(f), ln); */
    long double a, b;
    a = agn_checknumber(L, 2);
    b = agn_checknumber(L, 3);
    lua_pushnumber(L, (b <= 0.0) ? AGN_NAN : tools_expl(-LN2L - tools_fabsl(x - a)/b + tools_logl(1/b)));
  }
  return 1 + onearg;
}


/* Implements the gamma distribution probability density function as listed on Wikipedia.
   See: https://en.wikipedia.org/wiki/Gamma_distribution; 3.1.4
   See: https://stackoverflow.com/questions/55404723/how-do-i-output-a-probability-density-for-gamma-distribution-at-a-specific-value */
static double gammapdf (double x, double alpha, double beta) {
  double gam = tools_gamma(alpha);
  return gam == tools_isnan(gam) ? gam : (sun_pow(beta, alpha, 1)*sun_pow(x, alpha - 1, 1)*sun_exp(-1*beta*x))/gam;
}

static int stats_gammapdf (lua_State *L) {
  lua_Number x, alpha, beta;
  x = agn_checknumber(L, 1);
  alpha = agn_checknumber(L, 2);
  beta = agn_checknumber(L, 3);
  lua_pushnumber(L, gammapdf(x, alpha, beta));
  return 1;
}


/* Implements the gamma cumulative distribution function as listed on Wikipedia.
   See: https://en.wikipedia.org/wiki/Gamma_distribution, 3.1.4 */
static int stats_gammacdf (lua_State *L) {
  lua_Number alpha, beta, x, gam;
  x = agn_checknumber(L, 1);
  alpha = agn_checknumber(L, 2);
  beta = agn_checknumber(L, 3);
  gam = tools_gamma(alpha);
  lua_pushnumber(L, tools_isnan(gam) ? gam : tools_lowerincompletegamma(alpha, beta*x)/gam);
  return 1;
}


static int stats_F (lua_State *L) {
  lua_pushnumber(L, fdtr(agn_checkinteger(L, 1), agn_checkinteger(L, 2), agn_checknumber(L, 3)));
  return 1;
}


static int stats_invF (lua_State *L) {
  lua_pushnumber(L, fdtri(agn_checkinteger(L, 1), agn_checkinteger(L, 2), agn_checknumber(L, 3)));
  return 1;
}


static int stats_Fc (lua_State *L) {
  lua_pushnumber(L, fdtrc(agn_checkinteger(L, 1), agn_checkinteger(L, 2), agn_checknumber(L, 3)));
  return 1;
}


/* binompdf(k, n, p) simplifies to the binomial probability density defined as:

	                  k         n - k
	binomial(n, k) * p * (1 - p)      */

#define lnbinompdf(x,n,p)  (tools_lnbinomial(n, x) + x*sun_log(p) + (n - x)*sun_log(1 - p))
#define MIN(a,b)           ((a) < (b) ? (a) : (b))

static lua_Number binompdf (lua_Number x, lua_Number n, lua_Number p) {
  lua_Number r = tools_binomial(n, x)*sun_pow(p, x, 1)*sun_pow(1 - p, n - x, 1);
  if (tools_isnan(r)) r = sun_exp(lnbinompdf(x, n, p));  /* overflow case */
  return tools_isnan(r) && tools_isint(x) ? 0 : r;  /* 3.7.3 patch to Derive 6.1 implementation */
}

static int stats_binompdf (lua_State *L) {
  lua_pushnumber(L, binompdf(agn_checknumber(L, 1), agn_checknumber(L, 2), agn_checknumber(L, 3)));
  return 1;
}


/* stats.binomd(k, n, p) simplifies to the cumulative probability binomial distribution function defined as:
	 Sum stats.binompdf(j, n, p), j=0 .. min(k, n)); 3.5.3 */
static int stats_binomd (lua_State *L) {
  volatile double s, cs, ccs;
  lua_Number x, n, p, min;
  int j;
  x = agn_checknumber(L, 1);
  n = agn_checknumber(L, 2);
  p = agn_checknumber(L, 3);
  s = cs = ccs = 0;
  min = MIN(x, n);
  for (j=0; j <= min; j++) {
    s = tools_kbadd(s, binompdf(j, n, p), &cs, &ccs);  /* 3.7.2 change to Kahan-Babuska */
  }
  lua_pushnumber(L, s + cs + ccs);
  return 1;
}


/* `stats.negbinompdf` implements Maple's negativebinomial function and computes the probability
    density function equal to binomial(n + x - 1, x) * p^n * (1 - p)^x, with x a non-negative
    integer no greater than n, n a positive integer and p a number between 0 (inclusive) and 1 (inclusive).
   > with(stats):
   > statevalf[pf, negativebinomial[10, 0.5]](5); -> .06109619141
   > statevalf[pf, negativebinomial[10, 1]](5);   -> 0 */
#define neglnbinompdf(x,n,p)  (tools_lnbinomial(n + x - 1, x) + n*sun_log(p) + x*sun_log(1 - p))

static lua_Number negbinompdf (lua_Number x, lua_Number n, lua_Number p) {  /* 3.7.5 */
  lua_Number r = tools_binomial(n + x - 1, x)*sun_pow(p, n, 1)*sun_pow(1 - p, x, 1);
  if (tools_isnan(r)) r = sun_exp(neglnbinompdf(x, n, p));  /* overflow case */
  return tools_isnan(r) && tools_isint(x) ? AGN_NAN : r;  /* 3.7.3 patch to Derive 6.1 implementation */
}

static int stats_negbinompdf (lua_State *L) {
  lua_Number x, n, p;
  x = agn_checknonnegint(L, 1);
  n = agn_checkposint(L, 2);
  p = agn_checknumber(L, 3);
  if (x > n)
    luaL_error(L, "Error in " LUA_QS ": first argument is greater than second argument.", "stats.negbinompdf");
  if (p < 0 || p > 1)
    luaL_error(L, "Error in " LUA_QS ": third argument must bei in [0, 1].", "stats.negbinompdf");
  lua_pushnumber(L, (lua_Number)negbinompdf(x, n, p));  /* 6.5.0 tweak */
  return 1;
}


/* Inverse binomial distribution, 6.5.0 */
static int stats_invbinomd (lua_State *L) {
  lua_Number k, n, x;
  k = agn_checknonnegint(L, 1);
  n = agn_checkinteger(L, 2);
  x = agn_checknumber(L, 3);
  if (x < 0 || x > 1)
    luaL_error(L, "Error in " LUA_QS ": third argument must bei in [0, 1].", "stats.invbinomd");
  if (k >= n)
    luaL_error(L, "Error in " LUA_QS ": first argument is greater than or equal second argument.", "stats.invbinomd");
  /* double bdtri(int k, int n, double y); */
  lua_pushnumber(L, bdtri(k, n, x));
  return 1;
}


static lua_Number poisson (long double k, long double t) {
  if (t == 0)
    return (k == 0) ? AGN_NAN : 0;  /* 3.7.7 fix */
  else if (k < 0)  /* 3.7.7 fix */
    return AGN_NAN;
  else {
    long double x;
    int flag = fabsl(k*t) < 700;  /* this is a fairly good guess to try direct or logarithmic computation, 3.7.3 */
    if (flag) {  /* direct computation */
      x = tools_expl(-t)*tools_powl(t, k)/cephes_factoriall(k);  /* changed 3.16.2 */
      if (tools_fpisnanl(x)) flag = 0;  /* some cases will overflow, e.g. k=1 && t ~= -709, 3.18.4 fix */
    }
    if (!flag) {  /* logarithmic computation, protection against overflow, 3.7.3 */
      x = tools_expl(-t + k*tools_logl(t) - tools_lnfactoriall(k));
    }
    return x;  /* exp(-t) * t^k/k! */
  }
}

/* Simplifies to the Poisson probability density defined as exp(-t) * t^k/k!, 3.5.3 */
static int stats_poisson (lua_State *L) {
  lua_pushnumber(L, poisson((long double)agn_checknonnegint(L, 1), (long double)agn_checknumber(L, 2)));  /* 3.7.7 fix */
  return 1;
}


/* Simplifies to the cumulative probability Poisson distribution function defined as the sum of exp(-t)·t^j/j!
   from j=0 to k, using Kahan-Babuska summation to minimise round-off errors. 3.5.3 */
static int stats_poissond (lua_State *L) {
  volatile double x, s, t, cs, ccs;
  int k, j;
  k = agn_checkinteger(L, 1);
  t = agn_checknumber(L, 2);
  s = cs = ccs = 0;
  for (j=0; j <= k; j++) {
    x = (double)poisson(j, t);
    s = tools_kbadd(s, x, &cs, &ccs);  /* 3.7.2 change to Kahan-Babuska summation */
  }
  lua_pushnumber(L, s + cs + ccs);
  return 1;
}


/* Computes the hypergeometric probability density function

   binomial(N1, x)*binomial(N2, n - x)/binomial(N1 + N2, n),

   avoiding numeric overflow.
   Non-negative N1 is the size of the success population, non-negative N2 the size of the failure population,
   and n is the sample size. If n > N1 + N2, the function returns `fail`.
   This is the Maple V R4 emulation with additional fallback at overflow, 3.7.4, 3.16.4 changed to 80-bit precision
   > with(stats);
   > statevalf[pf, hypergeometric[10, 20, 15]](0);  -> .00009995002499 */
static int stats_hypergeom (lua_State *L) {
  long double x, N1, N2, n;
  x  = agn_checknonnegint(L, 1);
  N1 = agn_checknonnegint(L, 2);
  N2 = agn_checknonnegint(L, 3);
  n  = agn_checknonnegint(L, 4);
  if (n > N1 + N2)
    luaL_error(L, "Error in " LUA_QS ": fourth argument is greater than second plus third argument.", "stats.hypergeom");
  if (x > n)
    luaL_error(L, "Error in " LUA_QS ": first argument is greater than fourth argument.", "stats.hypergeom");
  else {
    long double r = tools_binomiall(N1, x) * tools_binomiall(N2, n - x) / tools_binomiall(N1 + N2, n);  /* to avoid `undefined` with n = N1 + N2 */
    if (tools_isnanorinf(r))  /* fallback */
      r = tools_expl((long double)(tools_lnbinomiall(N1, x) + tools_lnbinomiall(N2, n - x) - tools_lnbinomiall(N1 + N2, n)));
    lua_pushnumber(L, (lua_Number)r);
  }
  return 1;
}


/* The beta[nu1, nu2] distribution has the probability density function 1/Beta(nu1, nu2) * x^(nu1-1) * (1-x)^(nu2-1).
   Constraints: nu1, nu2 are positive integers. Works like in Maple. 3.7.5
   > with(stats):
   > statevalf[pdf, beta[1, 2]](0.9);  -> .2 */
static int stats_beta (lua_State *L) {
  lua_Number x, nu1, nu2;
  x  = agn_checknumber(L, 1);
  nu1 = agn_checkposint(L, 2);
  nu2 = agn_checkposint(L, 3);
  lua_pushnumber(L, 1/tools_beta(nu1, nu2)*sun_pow(x, nu1 - 1, 1)*sun_pow(1 - x, nu2 - 1, 1));
  return 1;
}


/* Computes the log-sum-exp function a + ln(Sum(e^(X[i] - a), i=1 .. n) with a = max(X) and also returns a.

   To compute the softmax function, that is the gradient vector of the log-sum-exp function, issue for example:

   > X := [-1, 2]

   > lse := stats.lse(X):
   2.0485873515737

   > map(<< x, lse -> exp(x)/exp(lse) >>, X, lse):
   [0.047425873177567, 0.95257412682243] */
static int stats_lse (lua_State *L) {
  lua_Number lse, max;
  if (!agn_lse(L, 1, &lse, &max, "stats.lse")) {
    luaL_error(L, "Error in " LUA_QS ": too few samples or table, sequence, register expected.", "stats.lse");
  }
  lua_pushnumber(L, lse);
  lua_pushnumber(L, max);
  return 2;
}


/* Correlation section *********************************************************************************/

/* corr_unitemplate is based on stats_sorted, 4.11.6 */
static int corr_unitemplate (lua_State *L, void (*f)(int, double [], double), const char *procname) {
  int type;
  size_t ii;
  lua_Number *a;
  type = lua_type(L, 1);
  luaL_typecheck(L, type == LUA_TTABLE || type == LUA_TSEQ || type == LUA_TREG || type == LUA_TUSERDATA, 1,
    "table, sequence, register or numarray expected", type);
  ii = 0;
  a = agnL_tonumarray(L, 1, &ii, procname, 1, 0);
  if (a == NULL) return 1;  /* issue the error */
  if (ii < 2)
    lua_pushfail(L);
  else {
    size_t i;
    (*f)(ii, a, agn_checknumber(L, 2));  /* change a in-place */
    if (type == LUA_TTABLE) {
      lua_createtable(L, ii, 0);
      for (i=0; i < ii; i++)
        agn_setinumber(L, -1, i + 1, a[i]);
    } else if (type == LUA_TSEQ) {
      agn_createseq(L, ii);
      for (i=0; i < ii; i++)
        agn_seqsetinumber(L, -1, i + 1, a[i]);
    } else if (type == LUA_TREG) {
      agn_createreg(L, ii);
      for (i=0; i < ii; i++)
        agn_regsetinumber(L, -1, i + 1, a[i]);
    } else {  /* 3.5.4 */
      NumArray *b = NULL;
      numarray_createdouble(L, b, ii);
      for (i=0; i < ii; i++)
        b->data.n[i] = a[i];
    }
  }
  xfree(a);  /* for all structures, free a */
  return 1;
}

/*
  Purpose:

    CORRELATION_CIRCULAR evaluates the circular correlation function.

  Discussion:

    This correlation is based on the area of overlap of two circles
    of radius RHO0 and separation RHO.

  Licensing:

    This code is distributed under the MIT license.

  Modified:

    10 November 2012

  Author:

    John Burkardt

  Reference:

    Petter Abrahamsen,
    A Review of Gaussian Random Fields and Correlation Functions,
    Norwegian Computing Center, 1997.

  Parameters:

    Input, int N, the number of arguments.

    Input, double RHO[N], the arguments.

    Input, double RHO0, the correlation length.

    Output, double C[N], the correlations.
*/

static void correlation_circular (int n, double rho[], double rho0) {
  int i;
  double rhohat;
  for (i=0; i < n; i++) {
    rhohat = fMin(fabs(rho[i])/rho0, 1.0);
    rho[i] = (1.0 - TWOOPI*(rhohat*sqrt(1.0 - rhohat*rhohat) + sun_asin(rhohat)));
  }
}

static int stats_circular (lua_State *L) {
  corr_unitemplate(L, correlation_circular, "stats.circular");
  return 1;
}


/******************************************************************************/
/*
  Purpose:

    CORRELATION_BESSELJ evaluates the Bessel J correlation function.

  Licensing:

    This code is distributed under the MIT license.

  Modified:

    10 November 2012

  Author:

    John Burkardt

  Reference:

    Petter Abrahamsen,
    A Review of Gaussian Random Fields and Correlation Functions,
    Norwegian Computing Center, 1997.

  Parameters:

    Input, int N, the number of arguments.

    Input, double RHO[N], the arguments.

    Input, double RHO0, the correlation length.

    Output, double C[N], the correlations.
*/

static void correlation_besselj (int n, double rho[], double rho0) {
  int i;
  double rhohat;
  for (i=0; i < n; i++) {
    rhohat = fabs(rho[i])/rho0;
    rho[i] = r8_besj0(rhohat);
  }
}

static int stats_besselj (lua_State *L) {
  corr_unitemplate(L, correlation_besselj, "stats.besselj");
  return 1;
}

/******************************************************************************/
/*
  Purpose:

    CORRELATION_BESSELK evaluates the Bessel K correlation function.

  Licensing:

    This code is distributed under the MIT license.

  Modified:

    10 November 2012

  Author:

    John Burkardt

  Reference:

    Petter Abrahamsen,
    A Review of Gaussian Random Fields and Correlation Functions,
    Norwegian Computing Center, 1997.

  Parameters:

    Input, int N, the number of arguments.

    Input, double RHO[N], the arguments.

    Input, double RHO0, the correlation length.

    Output, double C[N], the correlations.
*/

static void correlation_besselk (int n, double rho[], double rho0) {
  int i;
  double rhohat;
  for (i=0; i < n; i++) {
    if (rho[i] == 0.0) {
      rho[i] = 1.0;
    } else {
      rhohat = fabs(rho[i])/rho0;
      rho[i] = rhohat*r8_besk1(rhohat);
    }
  }
}

static int stats_besselk (lua_State *L) {
  corr_unitemplate(L, correlation_besselk, "stats.besselk");
  return 1;
}

/******************************************************************************/
/*
  Purpose:

    CORRELATION_CONSTANT evaluates the constant correlation function.

  Licensing:

    This code is distributed under the MIT license.

  Modified:

    10 November 2012

  Author:

    John Burkardt

  Reference:

    Petter Abrahamsen,
    A Review of Gaussian Random Fields and Correlation Functions,
    Norwegian Computing Center, 1997.

  Parameters:

    Input, int N, the number of arguments.

    Input, double RHO[N], the arguments.

    Input, double RHO0, the correlation length.

    Output, double C[N], the correlations.
*/
static void correlation_constant (int n, double rho[], double rho0) {
  int i;
  for (i=0; i < n; i++) {
    rho[i] = 1.0;
  }
}

static int stats_constant (lua_State *L) {
  corr_unitemplate(L, correlation_constant, "stats.constant");
  return 1;
}

/******************************************************************************/
/*
  Purpose:

    CORRELATION_CUBIC evaluates the cubic correlation function.

  Licensing:

    This code is distributed under the MIT license.

  Modified:

    10 November 2012

  Author:

    John Burkardt

  Reference:

    Petter Abrahamsen,
    A Review of Gaussian Random Fields and Correlation Functions,
    Norwegian Computing Center, 1997.

  Parameters:

    Input, int N, the number of arguments.

    Input, double RHO[N], the arguments.

    Input, double RHO0, the correlation length.

    Output, double C[N], the correlations.
*/
static void correlation_cubic (int n, double rho[], double rho0) {
  int i;
  double rhohat;
  for (i=0; i < n; i++) {
    rhohat = fMin(fabs(rho[i])/rho0, 1.0);
    rho[i] = 1.0
         - 7.0  * tools_intpow(rhohat, 2)
         + 8.75 * tools_intpow(rhohat, 3)
         - 3.5  * tools_intpow(rhohat, 5)
         + 0.75 * tools_intpow(rhohat, 7);
  }
}

static int stats_cubic (lua_State *L) {
  corr_unitemplate(L, correlation_cubic, "stats.cubic");
  return 1;
}

/******************************************************************************/
/*
  Purpose:

    CORRELATION_DAMPED_COSINE evaluates the damped cosine correlation function.

  Licensing:

    This code is distributed under the MIT license.

  Modified:

    10 November 2012

  Author:

    John Burkardt

  Reference:

    Petter Abrahamsen,
    A Review of Gaussian Random Fields and Correlation Functions,
    Norwegian Computing Center, 1997.

  Parameters:

    Input, int N, the number of arguments.

    Input, double RHO[N], the arguments.

    Input, double RHO0, the correlation length.

    Output, double C[N], the correlations.
*/
static void correlation_damped_cosine (int n, double rho[], double rho0) {
  int i;
  for (i=0; i < n; i++) {
    rho[i] = sun_exp(-fabs(rho[i])/rho0)*sun_cos(fabs(rho[i])/rho0);
  }
}

static int stats_dampedcos (lua_State *L) {
  corr_unitemplate(L, correlation_damped_cosine, "stats.dampedcos");
  return 1;
}

/******************************************************************************/
/*
  Purpose:

    CORRELATION_DAMPED_SINE evaluates the damped sine correlation function.

  Licensing:

    This code is distributed under the MIT license.

  Modified:

    10 November 2012

  Author:

    John Burkardt

  Reference:

    Petter Abrahamsen,
    A Review of Gaussian Random Fields and Correlation Functions,
    Norwegian Computing Center, 1997.

  Parameters:

    Input, int N, the number of arguments.

    Input, double RHO[N], the arguments.

    Input, double RHO0, the correlation length.

    Output, double C[N], the correlations.
*/
static void correlation_damped_sine (int n, double rho[], double rho0) {
  int i;
  double rhohat;
  for (i=0; i < n; i++) {
    if (rho[i] == 0.0) {
      rho[i] = 1.0;
    } else {
      rhohat = fabs(rho[i])/rho0;
      rho[i] = sun_sin(rhohat)/rhohat;
    }
  }
}

static int stats_dampedsin (lua_State *L) {
  corr_unitemplate(L, correlation_damped_sine, "stats.dampedsin");
  return 1;
}

/******************************************************************************/
/*
  Purpose:

    CORRELATION_EXPONENTIAL evaluates the exponential correlation function.

  Licensing:

    This code is distributed under the MIT license.

  Modified:

    10 November 2012

  Author:

    John Burkardt

  Reference:

    Petter Abrahamsen,
    A Review of Gaussian Random Fields and Correlation Functions,
    Norwegian Computing Center, 1997.

  Parameters:

    Input, int N, the number of arguments.

    Input, double RHO[N], the arguments.

    Input, double RHO0, the correlation length.

    Output, double C[N], the correlations.
*/
static void correlation_exponential (int n, double rho[], double rho0) {
  int i;
  for (i=0; i < n; i++) {
    rho[i] = sun_exp(-fabs(rho[i])/rho0);
  }
}

static int stats_exponential (lua_State *L) {
  corr_unitemplate(L, correlation_exponential, "stats.exponential");
  return 1;
}

/******************************************************************************/
/*
  Purpose:

    CORRELATION_GAUSSIAN evaluates the Gaussian correlation function.

  Licensing:

    This code is distributed under the MIT license.

  Modified:

    10 November 2012

  Author:

    John Burkardt

  Reference:

    Petter Abrahamsen,
    A Review of Gaussian Random Fields and Correlation Functions,
    Norwegian Computing Center, 1997.

  Parameters:

    Input, int N, the number of arguments.

    Input, double RHO[N], the arguments.

    Input, double RHO0, the correlation length.

    Output, double C[N], the correlations.
*/
static void correlation_gaussian (int n, double rho[], double rho0){
  int i;
  for (i=0; i < n; i++ ) {
    rho[i] = sun_exp(-tools_intpow(rho[i]/rho0, 2));
  }
}

static int stats_gaussian (lua_State *L) {
  corr_unitemplate(L, correlation_gaussian, "stats.gaussian");
  return 1;
}

/******************************************************************************/
/*
  Purpose:

    CORRELATION_HOLE evaluates the hole correlation function.

  Licensing:

    This code is distributed under the MIT license.

  Modified:

    10 November 2012

  Author:

    John Burkardt

  Parameters:

    Input, int N, the number of arguments.

    Input, double RHO[N], the arguments.

    Input, double RHO0, the correlation length.

    Output, double C[N], the correlations.
*/
static void correlation_hole (int n, double rho[], double rho0) {
  int i;
  for (i=0; i < n; i++) {
    rho[i] = (1.0 - fabs(rho[i])/rho0)*sun_exp(-fabs(rho[i])/rho0);
  }
}

static int stats_hole (lua_State *L) {
  corr_unitemplate(L, correlation_hole, "stats.hole");
  return 1;
}

/******************************************************************************/
/*
  Purpose:

    CORRELATION_LINEAR evaluates the linear correlation function.

  Licensing:

    This code is distributed under the MIT license.

  Modified:

    10 November 2012

  Author:

    John Burkardt

  Reference:

    Petter Abrahamsen,
    A Review of Gaussian Random Fields and Correlation Functions,
    Norwegian Computing Center, 1997.

  Parameters:

    Input, int N, the number of arguments.

    Input, double RHO[N], the arguments.

    Input, double RHO0, the correlation length.

    Output, double C[N], the correlations.
*/
static void correlation_linear (int n, double rho[], double rho0) {
  int i;
  for (i=0; i < n; i++) {
    if (rho0 < fabs(rho[i])) {
      rho[i] = 0.0;
    } else {
      rho[i] = (rho0 - fabs(rho[i]))/rho0;
    }
  }
}

static int stats_linear (lua_State *L) {
  corr_unitemplate(L, correlation_linear, "stats.linear");
  return 1;
}

/******************************************************************************/
/*
  Purpose:

    CORRELATION_MATERN evaluates the Matern correlation function.

  Discussion:

    In order to call this routine under a dummy name, I had to drop NU from
    the parameter list.

    The Matern correlation is

      rho1 = 2 * sqrt ( nu ) * rho / rho0

      c(rho) = ( rho1 )^nu * BesselK ( nu, rho1 )
               / gamma ( nu ) / 2 ^ ( nu - 1 )

    The Matern covariance has the form:

      K(rho) = sigma^2 * c(rho)

    A Gaussian process with Matern covariance has sample paths that are
    differentiable (nu - 1) times.

    When nu = 0.5, the Matern covariance is the exponential covariance.

    As nu goes to +oo, the correlation converges to exp ( - (rho/rho0)^2 ).

  Licensing:

    This code is distributed under the MIT license.

  Modified:

    03 November 2012

  Author:

    John Burkardt

  Parameters:

    Input, int N, the number of arguments.

    Input, double RHO[N], the arguments.
    0.0 <= RHO.

    Input, double RHO0, the correlation length.
    0.0 < RHO0.

    Output, double C[N], the correlations.
*/
static void correlation_matern (int n, double rho[], double rho0) {
  int i;
  double nu, rho1;
  nu = 2.5;
  for (i=0; i < n; i++) {
    rho1 = 2.0*sqrt(nu)*fabs(rho[i])/rho0;
    if (rho1 == 0.0) {
      rho[i] = 1.0;
    } else {
      rho[i] = sun_pow(rho1, nu, 1)*r8_besk(nu, rho1)/tgamma(nu)/sun_pow(2.0, nu - 1.0, 1);
    }
  }
}

static int stats_matern (lua_State *L) {
  corr_unitemplate(L, correlation_matern, "stats.matern");
  return 1;
}

/******************************************************************************/

/******************************************************************************/
/*
  Purpose:

    CORRELATION_PENTASPHERICAL evaluates the pentaspherical correlation function.

  Discussion:

    This correlation is based on the volume of overlap of two spheres
    of radius RHO0 and separation RHO.

  Licensing:

    This code is distributed under the MIT license.

  Modified:

    10 November 2012

  Author:

    John Burkardt

  Reference:

    Petter Abrahamsen,
    A Review of Gaussian Random Fields and Correlation Functions,
    Norwegian Computing Center, 1997.

  Parameters:

    Input, int N, the number of arguments.

    Input, double RHO[N], the arguments.

    Input, double RHO0, the correlation length.

    Output, double C[N], the correlations.
*/
static void correlation_pentaspherical (int n, double rho[], double rho0) {
  double rhohat;
  int i;
  for (i=0; i < n; i++) {
    rhohat = fMin(fabs(rho[i])/rho0, 1.0);
    rho[i] = 1.0 - 1.875*rhohat + 1.25*tools_intpow(rhohat, 3) - 0.375*tools_intpow(rhohat, 5);
  }
}

static int stats_penta (lua_State *L) {
  corr_unitemplate(L, correlation_pentaspherical, "stats.penta");
  return 1;
}


/******************************************************************************/
/*
  Purpose:

    CORRELATION_POWER evaluates the power correlation function.

  Discussion:

    In order to be able to call this routine under a dummy name, I had
    to drop E from the argument list.

    The power correlation is

      C(rho) = ( 1 - |rho| )^e  if 0 <= |rho| <= 1
             = 0                otherwise

      The constraint on the exponent is 2 <= e.

  Licensing:

    This code is distributed under the MIT license.

  Modified:

    10 November 2012

  Author:

    John Burkardt

  Parameters:

    Input, int N, the number of arguments.

    Input, double RHO[N], the arguments.
    0.0 <= RHO.

    Input, double RHO0, the correlation length.
    0.0 < RHO0.

    Input, double E, the exponent.
    E has a default value of 2.0;
    2.0 <= E.

    Output, double C[N], the correlations.
*/
static void correlation_power (int n, double rho[], double rho0) {
  double rhohat;
  int i;
  for (i=0; i < n; i++ ) {
    rhohat = fabs(rho[i])/rho0;
    if (rhohat <= 1.0) {
      rho[i] = tools_intpow(1.0 - rhohat, 2);
    } else {
      rho[i] = 0.0;
    }
  }
}

static int stats_power (lua_State *L) {
  corr_unitemplate(L, correlation_power, "stats.power");
  return 1;
}

/******************************************************************************/
/*
  Purpose:

    CORRELATION_RATIONAL_QUADRATIC: rational quadratic correlation function.

  Licensing:

    This code is distributed under the MIT license.

  Modified:

    10 November 2012

  Author:

    John Burkardt

  Reference:

    Petter Abrahamsen,
    A Review of Gaussian Random Fields and Correlation Functions,
    Norwegian Computing Center, 1997.

  Parameters:

    Input, int N, the number of arguments.

    Input, double RHO[N], the arguments.

    Input, double RHO0, the correlation length.

    Output, double C[N], the correlations.
*/
static void correlation_rational_quadratic (int n, double rho[], double rho0) {
  int i;
  for (i=0; i < n; i++) {
    rho[i] = 1.0/(1.0 + tools_intpow(rho[i]/rho0, 2));
  }
}

static int stats_ratquad (lua_State *L) {
  corr_unitemplate(L, correlation_rational_quadratic, "stats.ratquad");
  return 1;
}

/******************************************************************************/
/*
  Purpose:

    CORRELATION_SPHERICAL evaluates the spherical correlation function.

  Discussion:

    This correlation is based on the volume of overlap of two spheres
    of radius RHO0 and separation RHO.

  Licensing:

    This code is distributed under the MIT license.

  Modified:

    10 November 2012

  Author:

    John Burkardt

  Reference:

    Petter Abrahamsen,
    A Review of Gaussian Random Fields and Correlation Functions,
    Norwegian Computing Center, 1997.

  Parameters:

    Input, int N, the number of arguments.

    Input, double RHO[N], the arguments.

    Input, double RHO0, the correlation length.

    Output, double C[N], the correlations.
*/
static void correlation_spherical (int n, double rho[], double rho0) {
  int i;
  double rhohat;
  for ( i = 0; i < n; i++ ) {
    rhohat = fMin(fabs(rho[i])/rho0, 1.0);
    rho[i] = 1.0 - 1.5*rhohat + 0.5*tools_intpow(rhohat, 3);
  }
}

static int stats_spherical (lua_State *L) {
  corr_unitemplate(L, correlation_spherical, "stats.spherical");
  return 1;
}

/******************************************************************************/
/*
  Purpose:

    CORRELATION_WHITE_NOISE evaluates the white noise correlation function.

  Licensing:

    This code is distributed under the MIT license.

  Modified:

    03 November 2012

  Author:

    John Burkardt

  Reference:

    Petter Abrahamsen,
    A Review of Gaussian Random Fields and Correlation Functions,
    Norwegian Computing Center, 1997.

  Parameters:

    Input, int N, the number of arguments.

    Input, double RHO[N], the arguments.

    Input, double RHO0, the correlation length.

    Output, double C[N], the correlations.
*/
static void correlation_white_noise (int n, double rho[], double rho0) {
  int i;
  for (i=0; i < n; i++) {
    rho[i] = (rho[i] == 0.0) ? 1.0 : 0.0;
  }
}

static int stats_white (lua_State *L) {
  corr_unitemplate(L, correlation_white_noise, "stats.white");
  return 1;
}


static const luaL_Reg statslib[] = {
  {"acf", stats_acf},                       /* added on November 04, 2014 */
  {"acv", stats_acv},                       /* added on November 04, 2014 */
  {"amean", stats_amean},                   /* added on August 26, 2012 */
  {"ad", stats_ad},                         /* added on March 11, 2012 */
  {"beta", stats_beta},                     /* added on December 19, 2023 */
  {"binomd", stats_binomd},                 /* added on December 05, 2023 */
  {"binompdf", stats_binompdf},             /* added on December 05, 2023 */
  {"card", stats_card},                     /* added on April 22, 2025 */
  {"cauchy", stats_cauchy},                 /* added on March 30, 2015 */
  {"cdfnormald", stats_cdfnormald},         /* added on July 08, 2020 */
  {"checkcoordinate", stats_checkcoordinate},  /* added January 21, 2015 */
  {"chisquare", stats_chisquare},           /* added on March 29, 2015 */
  {"colnorm", stats_colnorm},               /* added on October 03, 2012 */
  {"covar", stats_covar},                   /* added on April 08, 2015 */
  {"cumsum", stats_cumsum},                 /* added on September 10, 2012 */
  {"deltalist", stats_deltalist},           /* added on March 12, 2012 */
  {"durbinwatson", stats_durbinwatson},     /* added on April 06, 2015 */
  {"ema", stats_ema},                       /* added on December 06, 2013 */
  {"F", stats_F},                           /* added on July 30, 2023 */
  {"Fc", stats_Fc},                         /* added on July 30, 2023 */
  {"fivenum", stats_fivenum},               /* added on June 29, 2015 */
  {"fratio", stats_fratio},                 /* added on March 29, 2015 */
  {"fsum", stats_fsum},                     /* added on May 19, 2014 */
  {"fprod", stats_fprod},                   /* added on May 19, 2014 */
  {"gammacdf", stats_gammacdf},             /* added on July 27, 2023 */
  {"gammad", stats_gammad},                 /* added on July 07, 2015 */
  {"gammadc", stats_gammadc},               /* added on July 07, 2015 */
  {"gammapdf", stats_gammapdf},             /* added on July 27, 2023 */
  {"gema", stats_gema},                     /* added on December 09, 2013 */
  {"geometric", stats_geometric},           /* added on June 24, 2023 */
  {"gini", stats_gini},                     /* added on July 28, 2014 */
  {"gsma", stats_gsma},                     /* added on July 21, 2013 */
  {"gsmm", stats_gsmm},                     /* added on July 20, 2013 */
  {"hmean", stats_hmean},                   /* added on May 27, 2018 */
  {"hypergeom", stats_hypergeom},           /* added on December 16, 2023 */
  {"invbinomd", stats_invbinomd},           /* added on December 06, 2025 */
  {"invF", stats_invF},                     /* added on July 30, 2023 */
  {"invnormald", stats_invnormald},         /* added on July 09, 2015 */
  {"ios", stats_ios},                       /* added on March 11, 2012 */
  {"iqmean", stats_iqmean},                 /* added on February 11, 2015 */
  {"isall", stats_isall},                   /* added on January 15, 2015 */
  {"isany", stats_isany},                   /* added on January 15, 2015 */
  {"issorted", stats_issorted},             /* added on March 31, 2012 */
  {"laplace", stats_laplace},               /* added on June 25, 2023 */
  {"logistic", stats_logistic},             /* added on June 24, 2023 */
  {"lognormald", stats_lognormald},         /* added on December 16, 2023 */
  {"logseries", stats_logseries},           /* added on June 24, 2023 */
  {"lse", stats_lse},                       /* added on Fenruary 16, 2025 */
  {"mad", stats_mad},                       /* added on September 11, 2013 */
  {"max", stats_max},                       /* added on September 09, 2024 */
  {"md", stats_md},                         /* added on January 24, 2015 */
  {"meanmed", stats_meanmed},               /* added on June 30, 2014 */
  {"meanqmdev", stats_meanqmdev},           /* added on November 30, 2024 */
  {"meanvar", stats_meanvar},               /* added on April 07, 2015 */
  {"median", stats_median},                 /* added on June 19, 2007 */
  {"midrange", stats_midrange},             /* added on November 18, 2015 */
  {"min", stats_min},                       /* added on September 09, 2024 */
  {"minmax", stats_minmax},                 /* added on June 20, 2007 */
  {"mode", stats_mode},                     /* added on March 08, 2025 */
  {"moment", stats_moment},                 /* added on March 11, 2012 */
  {"nde", stats_nde},                       /* added on April 14, 2013 */
  {"ndf", stats_ndf},                       /* added on April 13, 2013 */
  {"negbinompdf", stats_negbinompdf},       /* added on December 19, 2023 */
  {"neighbours", stats_neighbours},         /* added on October 31, 2014 */
  {"nearby", stats_nearby},                 /* added on January 15, 2015 */
  {"normald", stats_normald},               /* added on March 29, 2015 */
  {"pdf", stats_pdf},                       /* added on April 11, 2013 */
  {"percentile", stats_percentile},         /* added on November 21, 2023 */
  {"poissond", stats_poissond},             /* added on December 05, 2023 */
  {"poisson", stats_poisson},               /* added on December 05, 2023 */
  {"prange", stats_prange},                 /* added on November 04, 2013 */
  {"probit", stats_probit},                 /* added on July 08, 2020 */
  {"quartiles", stats_quartiles},           /* added on February 12, 2015 */
  {"rank", stats_rank},                     /* added on July 27, 2018 */
  {"rownorm", stats_rownorm},               /* added on October 04, 2012 */
  {"scale", stats_scale},                   /* added on October 01, 2012 */
  {"sma", stats_sma},                       /* added on July 11, 2013 */
  {"smallest", stats_smallest},             /* added on December 30, 2012 */
  {"smm", stats_smm},                       /* added on July 15, 2013 */
  {"sorted", stats_sorted},                 /* added on October 07, 2012 */
  {"standardise", stats_standardise},       /* added on April 07, 2015 */
  {"studentst", stats_studentst},           /* added on March 29, 2015 */
  {"sumdata", stats_sumdata},               /* added on March 11, 2012 */
  {"sumdataln", stats_sumdataln},           /* added on April 06, 2015 */
  {"tovals", stats_tovals},                 /* added on March 11, 2012 */
  {"trimean", stats_trimean},               /* added on February 12, 2015 */
  {"weights", stats_weights},               /* added on April 26, 2017 */
  {"winsor", stats_winsor},                 /* added on March 24, 2017 */
  /* correlation functions */
  {"besselj", stats_besselj},               /* added on April 23, 2025 */
  {"besselk", stats_besselk},               /* added on April 23, 2025 */
  {"circular", stats_circular},             /* added on April 23, 2025 */
  {"constant", stats_constant},             /* added on April 23, 2025 */
  {"cubic", stats_cubic},                   /* added on April 23, 2025 */
  {"dampedcos", stats_dampedcos},           /* added on April 23, 2025 */
  {"dampedsin", stats_dampedsin},           /* added on April 23, 2025 */
  {"exponential", stats_exponential},       /* added on April 23, 2025 */
  {"gaussian", stats_gaussian},             /* added on April 23, 2025 */
  {"hole", stats_hole},                     /* added on April 23, 2025 */
  {"linear", stats_linear},                 /* added on April 23, 2025 */
  {"matern", stats_matern},                 /* added on April 23, 2025 */
  {"penta", stats_penta},                   /* added on April 23, 2025 */
  {"power", stats_power},                   /* added on April 23, 2025 */
  {"ratquad", stats_ratquad},               /* added on April 23, 2025 */
  {"spherical", stats_spherical},           /* added on April 23, 2025 */
  {"white", stats_white},                   /* added on April 23, 2025 */
  {NULL, NULL}
};


/*
** Open stats library
*/
LUALIB_API int luaopen_stats (lua_State *L) {
  luaL_register(L, AGENA_STATSLIBNAME, statslib);
  return 1;
}

