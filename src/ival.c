/*
* ival.c
* interval arithmetic library for Lua and Agena based on fi_lib
* Luiz Henrique de Figueiredo <lhf@tecgraf.puc-rio.br>
* 24 Jul 2018 23:30:19
* This code is hereby placed in the public domain and also under the MIT license.
* For Lua, pass the -DLUA switch to GCC.
*/

#ifndef LUA

#include "agena.h"
#include "agnxlib.h"
#include "agnhlps.h"

#else

#include "lua.h"
#include "lauxlib.h"

#define agn_getepsilon(L)  (1.4901161193847656e-08)
#define sun_pow            pow
#define sun_cbrt           cbrt
#define sun_trunc          trunc
#define agnL_optstring     luaL_optstring
#define lua_istrue(L,idx)  (lua_isboolean(L, (idx)) && bvalue(index2adr(L, (idx))) == 1)
#define agn_checknumber    luaL_checknumber

static int agnL_gettablefield (lua_State *L, const char *table, const char *field, const char *procname, int issueerror) {
  luaL_checkstack(L, 2, "not enough stack space");
  lua_getglobal(L, table);
  if (!lua_istable(L, -1)) {
    if (issueerror) {
      lua_pop(L, 1);  /* in case of an error, remove object on the top */
      luaL_error(L, "Error in " LUA_QS ": table " LUA_QS " does not exist.", procname, table);
    }
    return LUA_TNONE - 1;  /* = -3 */
  }
  lua_getfield(L, -1, field);
  lua_remove(L, -2);  /* remove table */
  return (lua_type(L, -1));
}

#endif

#include "fi_lib.h"

#define AGENA_IVALLIBNAME		"ival"
#define MYVERSION	AGENA_IVALLIBNAME " library for " AGENA_VERSION " / Jul 2018 / "\
			"using fi_lib"
#define MYTYPE		AGENA_IVALLIBNAME

/* compatibility macros */
#if LUA_VERSION_NUM <= 501
#define luaL_setmetatable(L,t) \
  luaL_getmetatable(L,t); \
  lua_setmetatable(L,-2)
#define luaL_setfuncs(L,r,n) \
  luaL_register(L,NULL,r)
#endif

#ifdef LUA
#define fMax(a,b) ({ typeof (a) _a = (a); typeof (b) _b = (b); _a > _b ? _a : _b; })
static int tools_approx (double x, double y, double eps) {
  if ((x == y) || (isnan(x) && isnan(y))) {
    return 1;
  } else {
    double dist = fabs(x - y);
    /* HUGE_VAL is defined in math.h */
    return (dist != HUGE_VAL) && (dist <= eps || dist <= (eps*fMax(fabs(x), fabs(y))));
  }
}
#endif

typedef interval *Interval;

#define checkival(L,n) (Interval)(luaL_checkudata(L, n, "ival"))
#define isival(L,n)    luaL_isudata(L, n, "ival")

static lua_State *LL = NULL;

/* qerr_m.c ************************************************************************** */

#define NAN_ERR -1

static void fi_abort (int n, int fctn) {
	(void)(fctn);
	const char *message;
	if (n == INV_ARG)
		message = "invalid argument or domain error";
	else if (n == OVER_FLOW)
		message = "overflow";
	else if (n == DIV_ZERO)
		message = "division by zero";
	else if (n == NAN_ERR)
		message = "undefined";
	else
		message = "cannot happen";
	luaL_error(LL, "Error in fi_lib: %s.", message);
}

static double q_abort (int n, double *x, int fctn) {
	fi_abort(n, fctn);
	return *x;
}

static interval j_abort (int n, double *x1, double *x2, int fctn) {
	interval res;
	fi_abort(n, fctn);
	res.INF = *x1;
	res.SUP = *x2;
	return res;
}

double q_abortnan (int n, double *x, int fctn) {
	(void)(n);
	return q_abort(NAN_ERR, x, fctn);
}

double q_abortr1 (int n, double *x, int fctn) {
	return q_abort(n, x, fctn);
}

interval q_abortr2 (int n, double *x1, double *x2, int fctn) {
	return j_abort(n, x1, x2, fctn);
}

double q_abortdivd (int n, double *x) {
	return q_abort(n, x, -1);
}

interval q_abortdivi (int n, double *x1, double *x2) {
	return j_abort(n, x1, x2, -1);
}

/* ********************************************************************************** */

static double q_ipow (double x, int n) {
  double r;
  for (r=1.0; n > 0; n >>= 1) {
    if (n & 1) r *= x;
    x *= x;
  }
  return r;
}

static double q_cbrt (double x) {  /* added 5.0.1 */
  double res;
  if (isnan(x))  /* Test: x = NaN */
    res = q_abortnan(INV_ARG, &x, 0);
  else  /* cbrt over the reals is defined only in the non-negative domain */
    res = (x < 0) ? q_abortr1(INV_ARG, &x, 0) : sun_cbrt(x);
  return(res);
}

static interval j_cbrt (interval x) {  /* added 5.0.1 */
  interval res;
  if (x.INF == x.SUP) {  /* point interval */
    if (x.INF == 0)
      res.INF = res.SUP = 0;
    else {
      res.INF = q_cbrt(x.INF);
      res.SUP = r_succ(res.INF);
      res.INF = r_pred(res.INF);
    }
  } else {
    if (x.INF == 0) res.INF = 0;
    else res.INF = r_pred(q_cbrt(x.INF));
    res.SUP = r_succ(q_cbrt(x.SUP));
  }
  return res;
}

static void Pset (Interval x, double a, double b) {
  x->INF = a;
  x->SUP = b;
}

static void Pout (Interval x, double a, double b) {
  x->INF = q_pred(a);   /* next possible number on the machine less than b */
  x->SUP = q_succ(b);   /* next possible number on the machine greater than b (nextafter) */
}

static Interval Pnew (lua_State *L) {
  Interval x = lua_newuserdata(L, sizeof(interval));
  luaL_setmetatable(L, MYTYPE);
  return x;
}

static Interval Pget (lua_State *L, int i, Interval x) {
  LL = L;
  switch (lua_type(L, i)) {
    case LUA_TNUMBER: {
      lua_Number a = lua_tonumber(L, i);
      Pset(x, a, a);
      return x;
    }
    case LUA_TSTRING: {
      lua_Number a = lua_tonumber(L, i);
      Pout(x, a, a);
      return x;
    }
    default:
      return luaL_checkudata(L, i, MYTYPE);
  }
  return NULL;
}

static int Pdo1 (lua_State *L, interval (*f)(interval a)) {
  interval A;
  Interval a, r;
  a = Pget(L, 1, &A);
  r = Pnew(L);
  *r = f(*a);
  return 1;
}

static int Pdo2 (lua_State *L, interval (*f)(interval a, interval b)) {
  interval A, B;
  Interval a, b, r;
  a = Pget(L, 1, &A);
  b = Pget(L, 2, &B);
  r = Pnew(L);
  *r = f(*a, *b);
 return 1;
}

#define DO(d,f,F)	static int L##f(lua_State *L) { return d(L,F); }
#define DO1(f,F)	DO(Pdo1,f,F)
#define DO2(f,F)	DO(Pdo2,f,F)

DO1(xabs,     j_abs)   /* abs(x) */
DO1(xarccos,  j_acos)  /* acos(x) */
DO1(xarccosh, j_acsh)  /* acosh(x) */
DO1(xarccot,  j_acot)  /* acot(x) */
DO1(xarccoth, j_acth)  /* acoth(x) */
DO1(xarcsin,  j_asin)  /* asin(x) */
DO1(xarcsinh, j_asnh)  /* asinh(x) */
DO1(xarctan,  j_atan)  /* atan(x) */
DO1(xarctanh, j_atnh)  /* atanh(x) */
DO1(xcbrt,    j_cbrt)  /* cbrt(x) */
DO1(xcos,     j_cos)   /* cos(x) */
DO1(xcosh,    j_cosh)  /* cosh(x) */
DO1(xcot,     j_cot)   /* cot(x) */
DO1(xcoth,    j_coth)  /* coth(x) */
DO1(xerf,     j_erf)   /* erf(x) */
DO1(xerfc,    j_erfc)  /* erfc(x) */
DO1(xexp,     j_exp)   /* exp(x) */
DO1(xexp10,   j_ex10)  /* exp10(x) */
DO1(xexp2,    j_exp2)  /* exp2(x) */
DO1(xexpm1,   j_expm)  /* expm1(x) */
DO1(xlog,     j_log)   /* log(x) */
DO1(xlog10,   j_lg10)  /* log10(x) */
DO1(xlog1p,   j_lg1p)  /* log1p(x) */
DO1(xlog2,    j_log2)  /* log2(x) */
DO1(xsin,     j_sin)   /* sin(x) */
DO1(xsinh,    j_sinh)  /* sinh(x) */
DO1(xsquare,  j_sqr)   /* sqr(x) */
DO1(xsqrt,    j_sqrt)  /* sqrt(x) */
DO1(xtan,     j_tan)   /* tan(x) */
DO1(xtanh,    j_tanh)  /* tanh(x) */
DO2(add,      add_ii)  /* add(x,y) */
DO2(div,      div_ii)  /* div(x,y) */
DO2(join,     hull)    /* join(x,y) */
DO2(sub,      sub_ii)  /* sub(x,y) */

static int Ltostring (lua_State *L){  /* tostring(x) */
  interval A;
  Interval a = Pget(L, 1, &A);
  const char *format = agnL_optstring(L, 2, NULL);  /* In Lua/Agena, %f denotes a double ! */
  if (!format) {
    lua_pushfstring(L, "[%f .. %f]", a->INF, a->SUP);
  } else {
    if (agnL_gettablefield(L, "strings", "format", "ival.tostring", 1) != LUA_TFUNCTION) {
      luaL_error(L, "Error in " LUA_QS ": could not find `strings.format`.", "ival.tostring");
    }
    lua_pushstring(L, format);
    lua_pushnumber(L, a->INF);
    lua_pushnumber(L, a->SUP);
    lua_call(L, 3, 1);
    if (!lua_isstring(L, -1)) {
      lua_pop(L, 1);
      luaL_error(L, "Error in " LUA_QS ": `strings.format` did not return a string.", "ival.tostring");
    }
  }
  return 1;
}

static int Lgc (lua_State *L) {  /* added 5.0.0 although not necessarily needed */
  luaL_checkudata(L, 1, MYTYPE);
#ifndef LUA
  lua_setmetatabletoobject(L, 1, NULL, 0);
#else
  lua_pushnil(L);
  lua_setmetatable(L, 1);
#endif
  return 0;
}

static int Lnew (lua_State *L)	{  /* new(a,b,[outward]) */
  lua_Number a, b;
  int outward, offset;
#ifndef LUA
  if ( (offset = lua_ispair(L, 1)) ) {
    agnL_pairgetinumbers(L, "interval.new", -1, &a, &b);
  } else {
    a = luaL_checknumber(L, 1);
    b = luaL_optnumber(L, 2, a);
  }
#else
  offset = 0;
  a = luaL_checknumber(L, 1);
  b = luaL_optnumber(L, 2, a);
#endif
  outward = lua_toboolean(L, 3 - offset);
  Interval x = Pnew(L);
  if (outward) Pout(x, a, b); else Pset(x, a, b);
  return 1;
}

static int Lextremes (lua_State *L){  /* extremes(x) */
  interval A;
  Interval a = Pget(L, 1, &A);
  luaL_checkstack(L, 2, "not enough stack space");  /* added 5.0.0 */
  lua_pushnumber(L, a->INF);
  lua_pushnumber(L, a->SUP);
  return 2;
}

static int Ldiam (lua_State *L) {  /* diam(x) */
  interval A;
  Interval a = Pget(L, 1, &A);
  lua_pushnumber(L, q_diam(*a));
  return 1;
}

static int Linf (lua_State *L)	{  /* inf(x) */
  interval A;
  Interval a = Pget(L, 1, &A);
  lua_pushnumber(L, a->INF);
  return 1;
}

static int Lsup (lua_State *L) {  /* sup(x) */
  interval A;
  Interval a = Pget(L, 1, &A);
  lua_pushnumber(L, a->SUP);
  return 1;
}

static int Lmid (lua_State *L) {  /* mid(x) */
  interval A;
  Interval a = Pget(L, 1, &A);
  lua_pushnumber(L, q_mid(*a));
  return 1;
}

static int Lneg (lua_State *L) {  /* neg(x) */
  interval A;
  Interval a = Pget(L, 1, &A);
  double p = a->INF;
  double q = a->SUP;
  Interval r = Pnew(L);
  Pset(r, -q, -p);
  return 1;
}

/* call this way: recip(*a) with Interval a */
#define recip_i(a)   (div_di(1.0, (a)))

static int Lrecip (lua_State *L) {  /* added 5.0.1 */
  interval A;
  Interval a, r;
  a = Pget(L, 1, &A);
  r = Pnew(L);
  *r = recip_i(*a);
  return 1;
}

static int Lpow (lua_State *L) {  /* pow(x,n) */
  int n;
  double P, Q, p, q, nn;
  interval A;
  Interval r, a = Pget(L, 1, &A);
  nn = agn_checknumber(L, 2);
#if defined(LUA) || !defined(__MINGW32__)
  n = (int)(sun_trunc(nn));  /* make this absolutely portable, especially for negative nn, by always rounding towards zero */
#else
  n = (int)nn;
#endif
  p = a->INF;
  q = a->SUP;
  r = Pnew(L);
#ifdef LUA
  if (tools_isfrac(nn)) {  /* added 5.0.1 */
#else
  if (nn != n) {
#endif
    P = sun_pow(p, nn, 1);
    Q = sun_pow(q, nn, 1);
    if (nn > 0.0) {  /* monotonically increasing */
      Pset(r, P, Q);
    } else {  /* monotonically decreasing */
      Pset(r, Q, P);
    }
    return 1;
  }
  /* from here on raise to the power of any integer */
  if (n < 0) {  /* 5.0.1 extension */
    luaL_checkstack(L, 1, "not enough stack space");
    lua_pushnumber(L, -n);
    lua_replace(L, 2);  /* replace pops the number automatically */
    Lpow(L);
    a = Pget(L, -1, &A);
    *r = recip_i(*a);
    lua_pop(L, 1);  /* pop result of exponentiation with -n, leave r on the stack top and return it */
    return 1;
  } else if (n == 0) {
    Pset(r, 1.0, 1.0);
  } else if (n == 1) {
    lua_settop(L, 1);
    return 1;
  } else if (n == 2) {
    *r = j_sqr(*a);
    return 1;
  }
  P = q_ipow(p, n);
  Q = q_ipow(q, n);
  if (n % 2 == 1 || p >= 0.0) {
    Pout(r, P, Q);
  } else if (q < 0.0) {
    Pout(r, Q, P);
  } else {
    P = q_succ(P);  /* next possible number on the machine (nextafter) */
    Q = q_succ(Q);  /* ditto */
    Pset(r, 0.0, q_max(P, Q));
  }
  return 1;
}

static int Lmul (lua_State *L) {  /* added 5.0.1 */
  interval A, B;
  Interval a, b, r;
  a = Pget(L, 1, &A);
  b = Pget(L, 2, &B);
  if (a->INF == b->INF && a->SUP == b->SUP) {  /* with intervals a, b, if a = b then circumvent bug in mul_ii */
    lua_settop(L, 1);
    lua_pushinteger(L, 2);
    Lpow(L);
  } else {
    r = Pnew(L);
    *r = mul_ii(*a, *b);
  }
  return 1;
}

static int Lcube (lua_State *L) {  /* added 5.0.1 */
  lua_settop(L, 1);
  lua_pushinteger(L, 3);
  Lpow(L);
  return 1;
}

static int Lintsec (lua_State *L) {  /* interval.meet(x, y), intersect operator */
  interval A, B;
  Interval a, b, r;
  a = Pget(L, 1, &A);
  b = Pget(L, 2, &B);
  r = Pnew(L);
  *r = intsec(*a, *b);
  if (r->INF > r->SUP) {  /* added 5.0.0 */
    lua_pop(L, 1);
    lua_pushnil(L);
  }
  return 1;
}

interval iunion (interval x, interval y) {  /* added 5.0.0 */
  interval res;
  res.INF = (x.INF >= y.INF) ? y.INF : x.INF;
  res.SUP = (x.SUP <= y.SUP) ? y.SUP : x.SUP;
  return res;
}

static int Lunion (lua_State *L) {  /* union operator, added 5.0.0 */
  interval A, B;
  Interval a, b, r;
  a = Pget(L, 1, &A);
  b = Pget(L, 2, &B);
  r = Pnew(L);
  *r = intsec(*a, *b);
  if (r->INF <= r->SUP) {  /* intersection ? -> compute union or return `null` otherwise */
    *r = iunion(*a, *b);
  }
  if (r->INF > r->SUP) {
    lua_pop(L, 1);
    lua_pushnil(L);
  }
  return 1;
}

static int Lcontained (lua_State *L) {  /* contained(x, y) */
  interval A, B;
  Interval a, b;
  a = Pget(L, 1, &A);
  b = Pget(L, 2, &B);
  lua_pushboolean(L, in_ii(*a, *b));
  return 1;
}

static int Ldisjoint (lua_State *L) {  /* disjoint(x, y) */
  interval A, B;
  Interval a, b;
  a = Pget(L, 1, &A);
  b = Pget(L, 2, &B);
  lua_pushboolean(L, dis_ii(*a, *b));
  return 1;
}

static int Lin (lua_State *L) {  /* t in x or x in y */
  if (lua_isnumber(L, 1)) {
    interval A;
    lua_Number t = lua_tonumber(L, 1);
    Interval a = Pget(L, 2, &A);
    lua_pushboolean(L, in_di(t, *a));
  } else {
    Lcontained(L);
  }
  return 1;
}

static int Lnotin (lua_State *L) {  /* t notin x or x notin y */
  interval A, B;
  Interval a, b;
  if (lua_isnumber(L, 1)) {
    lua_Number t = lua_tonumber(L, 1);
    a = Pget(L, 2, &A);
    lua_pushboolean(L, !in_di(t, *a));
  } else {
    /* Ldisjoint(L); works differently */
    a = Pget(L, 1, &A);
    b = Pget(L, 2, &B);
    lua_pushboolean(L, !in_ii(*a, *b));
  }
  return 1;
}

static int Leq (lua_State *L) {
  interval A, B;
  Interval a, b;
  a = Pget(L, 1, &A);
  b = Pget(L, 2, &B);
  lua_pushboolean(L, ieq_ii(*a, *b));
  return 1;
}

static int Laeq (lua_State *L) {
  interval A, B;
  Interval a, b;
  int r;
  a = Pget(L, 1, &A);
  b = Pget(L, 2, &B);
  r = tools_approx(a->INF, b->INF, agn_getepsilon(L)) &&
      tools_approx(a->SUP, b->SUP, agn_getepsilon(L));
  lua_pushboolean(L, r ? 1 : ieq_ii(*a, *b));
  return 1;
}

static int Llt (lua_State *L) {
  interval A, B;
  Interval a, b;
  a = Pget(L, 1, &A);
  b = Pget(L, 2, &B);
  lua_pushboolean(L, a->SUP < b->INF);
  return 1;
}

static int Lle (lua_State *L) {
  interval A, B;
  Interval a, b;
  a = Pget(L, 1, &A);
  b = Pget(L, 2, &B);
  lua_pushboolean(L, a->SUP <= b->INF);
  return 1;
}

/* hypot(a, b) = sqrt(a^2+b^2), 3 times faster than the Agena version */
static int Lhypot (lua_State *L) {  /* added 5.0.0 */
  interval A, B, X, Y, Z, S;
  Interval a, b, r;
  a = Pget(L, 1, &A);
  b = Pget(L, 2, &B);
  r = Pnew(L);
  X = mul_ii(*a, *a);
  Y = mul_ii(*b, *b);
  /* printf("X.inf=%lf X.sup=%lf\n", X.INF, Y.SUP); */
  Z = add_ii(X, Y);
  S = j_sqrt(Z);
  *r = S;
  return 1;
}

static int Lpytha (lua_State *L) {  /* added 5.0.0 */
  interval A, B, X, Y;
  Interval a, b, r;
  a = Pget(L, 1, &A);
  b = Pget(L, 2, &B);
  r = Pnew(L);
  X = mul_ii(*a, *a);
  Y = mul_ii(*b, *b);
  *r = add_ii(X, Y);
  return 1;
}

static int ival_checkival (lua_State *L) {  /* added 5.0.1 */
  int i, rc = 0, nargs = lua_gettop(L);
  if (nargs > 1 && lua_istrue(L, nargs)) {
    nargs--; rc = 1;
  }
  for (i=1; i <= nargs; i++) {
    checkival(L, i);
    if (rc) {
      interval A;
      Interval a;
      a = Pget(L, i, &A);
      /* printf("%lf %lf\n", a->INF, a->SUP); */
      if (a->INF > a->SUP)
        luaL_error(L, "Error in " LUA_QS " with argument #%d: left border > right border.", "ival.checkival", i);
    }
  }
  return 0;
}

static int ival_isival (lua_State *L) {  /* added 5.0.1 */
  int i, r = 1, rc = 0, nargs = lua_gettop(L);
  if (nargs > 1 && lua_istrue(L, nargs)) {
    nargs--; rc = 1;
  }
  for (i=1; r && i <= nargs; i++) {
    r = isival(L, i);
    if (rc) {
      interval A;
      Interval a;
      a = Pget(L, i, &A);
      r = a->INF <= a->SUP;
    }
  }
  lua_pushboolean(L, r);
  return 1;
}


static const struct luaL_Reg ival_metalib [] = {
  {"__add",       Ladd},   /* __add(x,y) */
  {"__sub",       Lsub},   /* __s ub(x,y) */
  {"__mul",       Lmul},   /* __mul(x,y) */
  {"__div",       Ldiv},   /* __div(x,y) */
  {"__recip",     Lrecip}, /* __div(1,y) */
  {"__pow",       Lpow},   /* __pow(x,n) */
  {"__eq",        Leq},    /* __eq(x,y) */
  {"__eeq",       Leq},    /* __eeq(x,y) */
  {"__aeq",       Laeq},   /* __aeq(x,y) */
  {"__le",        Lle},    /* __le(x,y) */
  {"__lt",        Llt},    /* __lt(x,y) */
  {"__abs",       Lxabs},
  {"__arcsin",    Lxarcsin},
  {"__arccos",    Lxarccos},
  {"__arctan",    Lxarctan},
  {"__cos",       Lxcos},
  {"__cosh",      Lxcosh},
  {"__cube",      Lcube},  /* __cube(x) */
  {"__exp",       Lxexp},
  {"__ipow",      Lpow},   /* __pow(x,n) */
  {"__ln",        Lxlog},
  {"__sin",       Lxsin},
  {"__sinh",      Lxsinh},
  {"__square",    Lxsquare},
  {"__sqrt",      Lxsqrt},
  {"__tan",       Lxtan},
  {"__tanh",      Lxtanh},
  {"__unm",       Lneg},   /* __unm(x) */
  {"__in",        Lin},
  {"__notin",     Lnotin},
  {"__intersect", Lintsec},
  {"__union",     Lunion},
  {"__left",      Linf},
  {"__right",     Lsup},
  {"__size",      Ldiam},
  {"__tostring",  Ltostring},  /* __tostring(x) */
  {"extremes",    Lextremes},
  {"contained",   Lcontained},
  {"disjoint",    Ldisjoint},
  {"member",      Lin},
  {"diam",        Ldiam},
  {"inf",         Linf},
  {"mean",        Lmid},
  {"sup",         Lsup},
  {"__gc",        Lgc},
  {NULL, NULL}
};

static const luaL_Reg ivalnumlib[] = {
  {"xabs",      Lxabs},
  {"xarccos",   Lxarccos},
  {"xarccosh",  Lxarccosh},
  {"xarccot",   Lxarccot},
  {"xarccoth",  Lxarccoth},
  {"xadd",      Ladd},
  {"xarcsin",   Lxarcsin},
  {"xarcsinh",  Lxarcsinh},
  {"xarctan",   Lxarctan},
  {"xarctanh",  Lxarctanh},
  {"xcbrt",     Lxcbrt},
  {"xcos",      Lxcos},
  {"xcosh",     Lxcosh},
  {"xcot",      Lxcot},
  {"xcoth",     Lxcoth},
  {"xcube",     Lcube},
  {"xdiv",      Ldiv},
  {"xerf",      Lxerf},
  {"xerfc",     Lxerfc},
  {"xexp",      Lxexp},
  {"xexp10",    Lxexp10},
  {"xexp2",     Lxexp2},
  {"xexpm1",    Lxexpm1},
  {"xhypot",    Lhypot},
  {"xpytha",    Lpytha},
  {"xln",       Lxlog},
  {"xlnp1",     Lxlog1p},
  {"xlog10",    Lxlog10},
  {"xlog2",     Lxlog2},
  {"xmul",      Lmul},
  {"xneg",      Lneg},
  {"xpow",      Lpow},
  {"xrecip",    Lrecip},
  {"xsin",      Lxsin},
  {"xsinh",     Lxsinh},
  {"xsquare",   Lxsquare},
  {"xsqrt",     Lxsqrt},
  {"xsub",      Lsub},
  {"xtan",      Lxtan},
  {"xtanh",     Lxtanh},
  {"contained", Lcontained},
  {"disjoint",  Ldisjoint},
  {"diam",      Ldiam},
  {"extremes",  Lextremes},
  {"inf",       Linf},
  {"join",      Ljoin},
  {"meet",      Lintsec},
  {"member",    Lin},
  {"mean",      Lmid},
  {"new",       Lnew},
  {"sup",       Lsup},
  {"tostring",  Ltostring},
  {"checkival", ival_checkival},
  {"isival",    ival_isival},
  {"aeq",       Laeq},
  {"eq",        Leq},
  {"eeq",       Leq},
  {"lt",        Llt},
  {"le",        Lle},
	{NULL, NULL}
};

static void createmeta (lua_State *L) {
  luaL_newmetatable(L, AGENA_IVALLIBNAME);  /* create metatable for intervals */
  lua_pushvalue(L, -1);  /* push metatable */
  lua_setfield(L, -2, "__index");  /* metatable.__index = metatable */
  luaL_register(L, NULL, ival_metalib);
}


LUALIB_API int luaopen_ival (lua_State *L) {
  createmeta(L);
  luaL_register(L, AGENA_IVALLIBNAME, ivalnumlib);
  return 1;
}

