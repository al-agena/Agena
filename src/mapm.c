/*
* lmapm.c
*
* Big-number library for Lua 5.1 based on the MAPM library originally written by Michael C. Ring.

* Luiz Henrique de Figueiredo <lhf@tecgraf.puc-rio.br>
* 03 Apr 2009 00:11:32
*
* This code is hereby placed in the public domain.
*
* MAPM binding for Agena initiated January 17, 2010
*
* For Michael C. Ring's MAPM library see: https://github.com/achan001/MAPM-5 et al.
* For the Lua binding see: https://github.com/LuaDist/mapm/
*/

#include <errno.h>      /* for errno */
#include <stdlib.h>

#if !defined(__DJGPP__)
#include <fenv.h>       /* for rounding functions */
#endif

#define mapm_c
#define LUA_LIB

#include "agena.h"

#include "agenalib.h"
#include "agnxlib.h"
#include "mapm.h"
#include "m_apm.h"
#include "m_apm_lc.h"
#include "prepdefs.h"  /* FORCE_INLINE */

/* #define AGENA_LIBVERSION	"MAPM 4.9.5a binding, version 2.0.2 as of November 05, 2023\n" */

#if !(defined(__DJGPP__) || defined(__OS2__) || defined(__ANSI__))
#define AGENA_MAPMLIBNAME "mapm"
LUALIB_API int (luaopen_mapm) (lua_State *L);
#endif

#define mfreeall(...) Fn_freeall(void, m_apm_free, __VA_ARGS__);

/* Converts an object that has already been instantiated to userdata and pushes it onto the stack */
#ifndef lua_boxpointer
#define lua_boxpointer(L, u) \
	(*(void **)(lua_newuserdata(L, sizeof(void *))) = (u))
#endif

#define MYNAME		"mapm"
#define MYVERSION	MYNAME " library for " AGENA_VERSION " / Apr 2009 / "\
			"using MAPM " MAPM_LIB_SHORT_VERSION

static int DIGITS = 17;
#define MYDIGITS   (DIGITS + 2)
#define IEEEPRECISION  21

static lua_State *LL = NULL;

#define M_set_to_one(z) (m_apm_copy(z, MM_One))
#define M_iszero(z)     (z->m_apm_sign == 0)
#define M_isone(z)      (m_apm_compare(z, MM_One) == 0)
#define M_isneg(z)      (z->m_apm_sign == -1)
#define M_ispos(z)      (z->m_apm_sign == +1)
#define M_isnonpos(z)   (z->m_apm_sign == -1 || z->m_apm_sign == 0)
#define M_isnonneg(z)   (z->m_apm_sign == 0 || z->m_apm_sign == +1)
#define M_sign(z)       (z->m_apm_sign)
#define M_isint(z)      (m_apm_is_integer(z))

#define M_int(r,z) { \
  if (z->m_apm_sign < 0) \
    m_apm_ceil(r, z); \
  else \
    m_apm_floor(r, z); \
}

/* Check options for linalg.extend and linalg.countitems */
static void aux_newoptions (lua_State *L, int pos, int *nargs, int *digits, const char *procname) {
  int checkoptions;
  *digits = -1;  /* 14 fractional digits, the default of the MAPM library: "%.14E" */
  checkoptions = 1;  /* check n options; CHANGE THIS if you add/delete options */
  if (*nargs >= pos && lua_ispair(L, *nargs))  /* 6.7.8 change */
    luaL_checkstack(L, 2, "not enough stack space");
  while (checkoptions-- && *nargs >= pos && lua_ispair(L, *nargs)) {
    agn_pairgeti(L, *nargs, 1);  /* get left value, set to stack index -2 */
    agn_pairgeti(L, *nargs, 2);  /* get right value, set to stack index  -1 */
    if (agn_isstring(L, -2)) {
      const char *option = agn_tostring(L, -2);
      if (tools_streq("digits", option)) {
        *digits = agn_checknonnegint(L, -1);
      } else {
        agn_poptoptwo(L);
        luaL_error(L, "Error in " LUA_QS ": unknown option " LUA_QS ".", procname, option);
      }
    }
    /* do not call lua_settop as it would corrupt the argument stack since we have already pushed values */
    (*nargs)--;
    agn_poptoptwo(L);
  }
  if (*digits > 21) {
    luaL_error(L, "Error in " LUA_QS ": option " LUA_QS " must be in [0, 21].", procname, "digits");
  }
}


/* fmt must be a format in scientific notation, that is "%.14E" or similar; 6.5.7.
   Based on m_apm_set_double() in mapm_4.9.5/mapmgues.c  */
static void	m_apm_set_double14 (M_APM atmp, double dd, int digits) {
  char	*cp, *p, *ps, buf[128];
  if (dd == 0.0)  /* special case for 0 exactly */
    M_set_to_zero(atmp);
  else {
    /* cut off fractional part after 14 digits as m_apm_set_double() does, but use safer snprintf(). Make sure
       rounding will be the same on all platforms, independent of the current setting, 6.5.8 */
    char *fmt;
    int generated, original_mode;
#if !defined(__DJGPP__)
    original_mode = fegetround();
    if (original_mode != FE_TONEAREST) { fesetround(FE_TONEAREST); }
#endif
    fmt = str_concat("%.", tools_itoa(digits, 10), "E", NULL);
    generated = snprintf(buf, sizeof(buf), (const char *)fmt, dd);
    xfree(fmt);
#if !defined(__DJGPP__)
    if (original_mode != FE_TONEAREST) { fesetround(original_mode); }
#endif
    if ((generated >= sizeof(buf)) || (cp = strstr(buf, "E")) == NULL) {
      M_apm_log_error_msg(M_APM_RETURN,
        /* 7.9.5 change to prevent compiler warnings, cast to (char *) */
        (char *)"\'m_apm_set_double14\', Invalid double input (likely a NAN or +/- INF)");
      M_set_to_zero(atmp);
      return;
    }
    if (atoi(cp + sizeof(char)) == 0) *cp = '\0';
    p = cp;
    while (1) {
      p--;
      if (*p == '0' || *p == '.') *p = ' ';
      else break;
    }
    ps = buf;
    p  = buf;
    while (1) {
      if ((*p = *ps) == '\0') break;
      if (*ps++ != ' ') p++;
    }
    m_apm_set_string(atmp, buf);
  }
}


/* Instantiates a new M_APM object, creates a userdata from it, pushes it onto the stack and assigns the mt */
static M_APM Bnew (lua_State *L) {
  M_APM x = m_apm_init();
  lua_boxpointer(L, x);
  lua_setmetatabletoobject(L, -1, MAPMXTYPE, 1);  /* 3.5.0/1 change */
  return x;
}


static M_APM Bzero (lua_State *L) {
  M_APM x = m_apm_init();
  M_set_to_zero(x);
  lua_boxpointer(L, x);
  lua_setmetatabletoobject(L, -1, MAPMXTYPE, 1);
  return x;
}


/* - With a number or string at stack position i, creates a new M_APM userdata object and replaces the value at stack index i
     with this userdata object. The stack is left unchanged. The address of the M_APM ud object is returned.
   - With M_APM userdata, the address of the ud object is returned. */
static M_APM Bget (lua_State *L, int i) {
  M_APM x;
  LL = L;
  switch (lua_type(L, i)) {
    case LUA_TNUMBER: {
      x = Bnew(L);
      m_apm_set_double(x, lua_tonumber(L, i));  /* Agena 1.4.3/1.5.0, 2.9.4 change */
      lua_replace(L, i);
      return x;
    }
    case LUA_TSTRING: {
      size_t j, l, flag;
      char *str;
      flag = 0;
      str = (char *)lua_tolstring(L, i, &l);
      if (l > 2) {  /* 6.3.7 */
        if ((l > 3 && str[2] == 'x' && tools_strncmp(str, "-0x", 3) == 0) ||
            (str[1] == 'x' && tools_strncmp(str, "0x", 2) == 0)) {
          int rc;
          str = str_hextodec(str, l, &rc);
          switch (rc) {
            case 1:
              luaL_error(L, "Error: memory allocation failed.");
              break;
            case 2:
              luaL_error(L, "Error: invalid character in input string.");
              break;
            default:
              flag = 1;
          }
          l = tools_strlen(str);
        }
      }
      for (j=0; j < l; j++) {  /* 6.3.7 */
        unsigned char c = uchar(str[j]);
        struct lconv *cv = localeconv();
        unsigned char decpoint = (cv ? cv->decimal_point[0] : '.');
        if (((c < 48 && (c != decpoint && c != 45)) || (c > 57 && c < 65) || (c > 70 && c < 97) || c > 102))
          luaL_error(L, "Error: invalid character `%c` in input string.", c);
      }
      x = Bnew(L);
      m_apm_set_string(x, (char*)str);
      lua_replace(L, i);
      if (flag) { xfree(str); }
      return x;
    }
    default: {
      return *((void**)luaL_checkudata(L, i, MAPMXTYPE));
    }
  }
  return NULL;
}


static M_APM Bgetx (lua_State *L, int i, int digits) {
  if (agn_isnumber(L, i)) {
    M_APM x;
    x = Bnew(L);
    m_apm_set_double14(x, lua_tonumber(L, i), (digits == -1) ? 14 : digits);  /* Agena 1.4.3/1.5.0, 2.9.4/6.5.7 change */
    lua_replace(L, i);
    return x;
  }
  if (digits != -1) {  /* 6.5.8 */
    luaL_error(L, "Error: " LUA_QS " option works with (complex) numbers only.", "digits");
  }
  return Bget(L, i);
}


/* Applies function f onto the value at stack position 1, and returns the result as a new M_APM userdata object */
static int Bdo0 (lua_State *L, void (*f)(M_APM y, M_APM x)) {
  M_APM a, c;
  a = Bget(L, 1);
  c = Bnew(L);
  f(c, a);
  return 1;
}


static int Bdo1 (lua_State *L, void (*f)(M_APM y, int n, M_APM x)) {
  M_APM a, c;
  int n = agnL_optnonnegint(L, 2, MYDIGITS);
  a = Bget(L, 1);
  c = Bnew(L);
  f(c, n, a);
  return 1;
}


static int Bdo2 (lua_State *L, void (*f)(M_APM z, M_APM x, M_APM y)) {
  M_APM a, b, c;
  a = Bget(L, 1);
  b = Bget(L, 2);
  c = Bnew(L);
  f(c, a, b);
  return 1;
}


static int Bdo3 (lua_State *L, void (*f)(M_APM z, int n, M_APM x, M_APM y)) {
  M_APM a, b, c;
  int n;
  n = agnL_optnonnegint(L, 3, MYDIGITS);  /* 6.3.6 change from DIGITS to MYDIGITS */
  a = Bget(L, 1);
  b = Bget(L, 2);
  c = Bnew(L);
  f(c, n, a, b);
  return 1;
}


static int Bdigits (lua_State *L) {  /* digits([n]) */
  DIGITS = agnL_optnonnegint(L, 1, DIGITS);
  lua_pushinteger(L, DIGITS);  /* changed Agena 0.30.3 */
  return 1;
}


static int Btostring (lua_State *L) {  /* tostring(x,[n,exp]) */
  char *s;
  int n;
  M_APM a = Bget(L, 1);
  n = agnL_optinteger(L, 2, DIGITS);
  if (lua_toboolean(L, 3)) {
    int m = (n < 0) ? m_apm_significant_digits(a) : n;
    s = malloc((m + 16)*sizeof(char));
    if (s != NULL) {
      m_apm_to_string(s, n, a);
    } else {  /* 4.11.5 */
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "mapm tostring mt");
    }
  } else {
    s = m_apm_to_fixpt_stringexp(n, a, '.', 0, 0);
  }
  lua_pushstring(L, s);
  xfree(s);
  return 1;
}


static int mt_tostring (lua_State *L) {
  Btostring(L);
  return 1;
}


static lua_Number aux_m2dbl (M_APM a, int n, int *rc) {  /* 3.6.1, rewritten 6.1.5 */
  char *s, *endptr;
  int m = (n < 0) ? m_apm_significant_digits(a) : n;
  lua_Number x = 0.0;
  *rc = 0;
  s = malloc((m + 16)*sizeof(char));
  if (s != NULL) {
    m_apm_to_string(s, n, a);
    /* Reset errno before the call to distinguish between no error and an error */
    set_errno(0);  /* Windows 2000 seems susceptible to uncleared errno's */
    x = strtod(s, &endptr);
    if (errno == ERANGE || endptr == s || *endptr != '\0') x = 0.0;
    else *rc = 1;
    xfree(s);  /* 3.6.1 change: call free only _after_ conversion to a number */
  }
  return x;
}


static int Btonumber (lua_State *L) {  /* mapm.xtonumber(x), extended 2.31.12, changed 3.6.1 */
  int rc;
  lua_Number x = aux_m2dbl(Bget(L, 1), agnL_optposint(L, 2, 20), &rc);
  if (!rc)
    luaL_error(L, "Error in " LUA_QS ": conversion failed.", "mapm.xtonumber");
  lua_pushnumber(L, x);
  return 1;
}


static int Bnumber (lua_State *L) {  /* mapm.xnumber(x) */
  int nargs, digits;
  nargs = lua_gettop(L);
  aux_newoptions(L, 2, &nargs, &digits, "mapm.xnumber");
  Bgetx(L, 1, digits);
  lua_settop(L, 1);
  return 1;
}


static int Bround (lua_State *L) {   /* round(x) */
  return Bdo1(L, m_apm_round);
}


static int Binv (lua_State *L) {     /* inv(x) */
  M_APM a, c;
  a = Bget(L, 1);
  if (M_iszero(a)) {  /* 6.3.9 change */
    lua_pushundefined(L);
  } else {
    int n = agnL_optnonnegint(L, 2, MYDIGITS);
    c = Bnew(L);
    m_apm_reciprocal(c, n, a);
  }
  return 1;
}

#define mt_recip Binv


static int Bsqrt (lua_State *L) {  /* sqrt(x) */
  return Bdo1(L, m_apm_sqrt);
}

#define mt_sqrt Bsqrt


static int mt_invsqrt (lua_State *L) {  /* invsqrt(x), 6.4.0 */
  M_APM a, c, t;
  a = Bget(L, 1);
  if (M_isnonpos(a)) {
    lua_pushundefined(L);
  } else {
    int places = agnL_optnonnegint(L, 2, MYDIGITS);
    c = Bnew(L);
    t = m_apm_init();
    m_apm_sqrt(t, places, a);
    m_apm_reciprocal(c, places, t);
    m_apm_free(t);
  }
  return 1;
}


static int Bcbrt (lua_State *L) {  /* cbrt(x) */
  return Bdo1(L, m_apm_cbrt);
}


static int Blog (lua_State *L) {  /* ln(x) */
  return Bdo1(L, m_apm_log);
}

#define mt_ln Blog


static int Blog10 (lua_State *L) {  /* log10(x) */
  return Bdo1(L, m_apm_log10);
}


static int Bexp (lua_State *L) {  /* exp(x) */
  return Bdo1(L, m_apm_exp);
}

#define mt_exp Bexp


static int Bsin (lua_State *L) {  /* sin(x) */
  return Bdo1(L, m_apm_sin);
}

#define mt_sin Bsin


static int Bcos (lua_State *L) {  /* cos(x) */
  return Bdo1(L, m_apm_cos);
}

#define mt_cos Bcos


static int Btan (lua_State *L) {  /* tan(x) */
  return Bdo1(L, m_apm_tan);
}

#define mt_tan Btan


static int Basin (lua_State *L) {  /* asin(x) */
  return Bdo1(L, m_apm_asin);
}

#define mt_arcsin Basin


static int Bacos (lua_State *L) {    /* acos(x) */
  return Bdo1(L, m_apm_acos);
}

#define mt_arccos Bacos


static int Batan (lua_State *L) {    /* atan(x) */
  return Bdo1(L, m_apm_atan);
}

#define mt_arctan Batan


/* arcsec, arcsecant */
static int Basec (lua_State *L) {   /* arcsec(x), 3.3.3 */
  M_APM a, r;
  a = Bget(L, 1);
  if (m_apm_compare(a, MM_One) < 0) {
    lua_pushundefined(L);
  } else {
    r = Bnew(L);
    m_apm_reciprocal(r, MYDIGITS, a);
    m_apm_arccos(r, MYDIGITS, r);
  }
  return 1;
}

#define mt_arcsec Basec


/* arccsc, arccosecant */
static int Bacsc (lua_State *L) {  /* arccsc(x), 3.3.3 */
  M_APM a, r;
  a = Bget(L, 1);
  if (m_apm_compare(a, MM_One) < 0) {  /* |x| < 1 */
    lua_pushundefined(L);  /* 6.4.2 change */
  } else {
    r = Bnew(L);
    m_apm_reciprocal(r, MYDIGITS, a);
    m_apm_arcsin(r, DIGITS, r);
  }
  return 1;
}


static int Bsincos (lua_State *L) {  /* sincos(x) */
  M_APM a, s, c;
  int n = agnL_optinteger(L, 2, MYDIGITS);
  a = Bget(L, 1);
  luaL_checkstack(L, 2, "not enough stack space");  /* 6.3.9 fix */
  s = Bnew(L);
  c = Bnew(L);
  m_apm_sin_cos(s, c, n, a);
  return 2;
}


/* sinhcosh(x), 3.5.2; rewritten 6.6.4, merged from the original source code of m_apm_sinh, m_apm_cosh */
static void m_apm_sinh_cosh (M_APM sih, M_APM coh, int places, M_APM aa) {
  M_APM tmp1, tmp3, expaa, rexpaa;
  int local_precision;
  tmp1   = M_get_stack_var();
  tmp3   = M_get_stack_var();
  expaa  = M_get_stack_var();
  rexpaa = M_get_stack_var();
  local_precision = places + 4;
  m_apm_exp(expaa, local_precision, aa);
  m_apm_reciprocal(rexpaa, local_precision, expaa);
  /* sinh(aa) */
  m_apm_subtract(tmp3, expaa, rexpaa);
  m_apm_multiply(tmp1, tmp3, MM_0_5);
  m_apm_round(sih, places, tmp1);
  /* cosh(aa) */
  m_apm_add(tmp3, expaa, rexpaa);
  m_apm_multiply(tmp1, tmp3, MM_0_5);
  m_apm_round(coh, places, tmp1);
  M_restore_stack(4);
}

static int Bsinhcosh (lua_State *L) {  /* sinhcosh(x), 3.5.2 */
  M_APM a, s, c;
  int n = agnL_optinteger(L, 2, MYDIGITS);
  a = Bget(L, 1);
  luaL_checkstack(L, 2, "not enough stack space");  /* 6.3.9 fix */
  s = Bnew(L);
  c = Bnew(L);
  m_apm_sinh_cosh(s, c, n, a);
  return 2;
}


static int Batan2 (lua_State *L) {  /* atan2(y, x) */
  return Bdo3(L, m_apm_atan2);
}


static int Bsinh (lua_State *L) {   /* sinh(x) */
  return Bdo1(L, m_apm_sinh);
}

static int mt_sinh (lua_State *L) {   /* sinh(x) */
  return Bdo1(L, m_apm_sinh);
}


static int Bcosh (lua_State *L) {   /* cosh(x) */
  return Bdo1(L, m_apm_cosh);
}

static int mt_cosh (lua_State *L) {   /* cosh(x) */
  return Bdo1(L, m_apm_cosh);
}


static int Btanh (lua_State *L) {   /* tanh(x) */
  return Bdo1(L, m_apm_tanh);
}

static int mt_tanh (lua_State *L) {   /* tanh(x) */
  return Bdo1(L, m_apm_tanh);
}


static int Basinh (lua_State *L) {  /* asinh(x) */
  return Bdo1(L, m_apm_asinh);
}


static int Bacosh (lua_State *L) {  /* acosh(x) */
  return Bdo1(L, m_apm_acosh);
}


static int Batanh (lua_State *L) {  /* atanh(x) */
  return Bdo1(L, m_apm_atanh);
}


static int Bsec (lua_State *L) {  /* xsec(x) = 1/cos(x), 3.5.2 */
  M_APM a, r, co;
  int n = agnL_optinteger(L, 2, MYDIGITS);
  a = Bget(L, 1);
  co = m_apm_init();
  m_apm_cos(co, n, a);
  if (M_iszero(co)) {
    lua_pushundefined(L);  /* 6.3.9 */
  } else {
    r = Bnew(L);
    m_apm_reciprocal(r, n + 2, co);
  }
  m_apm_free(co);
  return 1;
}


static int Bsech (lua_State *L) {  /* xsech(x) = 1/cosh(x), 3.5.2 */
  M_APM a, r, coh;
  int n = agnL_optinteger(L, 2, MYDIGITS);
  a = Bget(L, 1);
  coh = m_apm_init();
  m_apm_cosh(coh, n, a);
  if (M_iszero(coh)) {
    lua_pushundefined(L);  /* 6.3.9 */
  } else {
    r = Bnew(L);
    m_apm_reciprocal(r, n + 2, coh);
  }
  m_apm_free(coh);
  return 1;
}


static int Bcsc (lua_State *L) {  /* xcsc(x) = 1/sin(x), 3.5.2 */
  M_APM a, r, si;
  int n = agnL_optinteger(L, 2, MYDIGITS);
  a = Bget(L, 1);
  si = m_apm_init();
  m_apm_sin(si, n, a);
  if (M_iszero(si)) {
    lua_pushundefined(L);  /* 6.3.9 */
  } else {
    r = Bnew(L);
    m_apm_reciprocal(r, n + 2, si);
  }
  m_apm_free(si);
  return 1;
}


static int Bcsch (lua_State *L) {  /* xcsch(x) = 1/sinh(x), 3.5.2 */
  M_APM a, r, sih;
  int n = agnL_optinteger(L, 2, MYDIGITS);
  a = Bget(L, 1);
  sih = m_apm_init();
  m_apm_sinh(sih, n, a);
  if (M_iszero(sih)) {
    lua_pushundefined(L);  /* 6.3.9 */
  } else {
    r = Bnew(L);
    m_apm_reciprocal(r, n + 2, sih);
  }
  m_apm_free(sih);
  return 1;
}


static int Bcot (lua_State *L) {  /* xcot(x) = -tan(Pi/2 + x), 3.5.2 */
  M_APM a, r, t, ta;
  int n = agnL_optinteger(L, 2, MYDIGITS);
  a = Bget(L, 1);
  r = Bnew(L);
  t = m_apm_init();
  ta = m_apm_init();
  m_apm_add(t, MM_lc_HALF_PI, a);
  m_apm_tan(ta, n, t);
  m_apm_negate(r, ta);
  m_apm_free(t); m_apm_free(ta);
  return 1;
}


static int Bcoth (lua_State *L) {  /* xcoth(x) = 1/tanh(x), 3.5.2 */
  M_APM a, r, tah;
  int n = agnL_optinteger(L, 2, MYDIGITS);
  a = Bget(L, 1);
  tah = m_apm_init();
  m_apm_tanh(tah, n, a);
  if (M_iszero(tah)) {
    lua_pushundefined(L);  /* 6.3.9 */
  } else {
    r = Bnew(L);
    m_apm_reciprocal(r, n + 2, tah);
  }
  m_apm_free(tah);
  return 1;
}


static int Bsinc (lua_State *L) {  /* xsinc(x), 3.5.2 */
  M_APM a, r, si;
  int n = agnL_optinteger(L, 2, MYDIGITS);
  a = Bget(L, 1);
  if (M_iszero(a)) {
    lua_pushundefined(L);  /* 6.3.9 */
  } else {
    r = Bnew(L);
    si = m_apm_init();
    m_apm_sin(si, n, a);
    m_apm_divide(r, n, si, a);
    m_apm_free(si);
  }
  return 1;
}


static int mt_sinc (lua_State *L) {  /* 3.5.2 */
  Bsinc(L);
  return 1;
}


static int Bcosc (lua_State *L) {  /* xcosc(x), 3.5.2 */
  M_APM a, r, co;
  int n = agnL_optinteger(L, 2, MYDIGITS);
  a = Bget(L, 1);
  if (M_iszero(a)) {
    lua_pushundefined(L);  /* 6.3.9 */
  } else {
    r = Bnew(L);
    co = m_apm_init();
    m_apm_cos(co, n, a);
    m_apm_divide(r, n, co, a);
    m_apm_free(co);
  }
  return 1;
}


static int Btanc (lua_State *L) {  /* xtanc(x), 3.5.2 */
  M_APM a, r, ta;
  int n = agnL_optinteger(L, 2, MYDIGITS);
  a = Bget(L, 1);
  if (M_iszero(a)) {
    lua_pushundefined(L);  /* 6.3.9 */
  } else {
    r = Bnew(L);
    ta = m_apm_init();
    m_apm_tan(ta, n, a);
    m_apm_divide(r, n, ta, a);
    m_apm_free(ta);
  }
  return 1;
}


static int Babs (lua_State *L) {   /* abs(x) */
  return Bdo0(L, m_apm_absolute_value);
}


static int Bneg (lua_State *L) {   /* neg(x) */
  return Bdo0(L, m_apm_negate);
}

static int mt_unm (lua_State *L) {   /* -(x), 3.5.2 fix */
  return Bneg(L);
}


/* factorial(x) for integral MAPM number x. See mapm.agn for a redefinition of the function
   to support fractional MAPM numbers, too. */
static int Bfactorial (lua_State *L) {  /* factorial(x) */
  M_APM a, c;
  a = Bget(L, 1);
  if (!m_apm_is_integer(a))  /* 6.4.6 */
    luaL_error(L, "Error: argument must be a non-negative integral.");
  c = Bnew(L);
  m_apm_factorial(c, a);
  return 1;
}


static int Bfloor (lua_State *L) {  /* floor(x) */
  return Bdo0(L, m_apm_floor);
}

#define mt_entier Bfloor


static int Bceil (lua_State *L) {   /* ceil(x) */
  return Bdo0(L, m_apm_ceil);
}


/* C trunc, round towards the next integer towards zero */
static int Bint (lua_State *L) {   /* int(x), 3.3.3 */
  M_APM a, r;
  a = Bget(L, 1);
  r = Bnew(L);
  M_int(r, a);
  return 1;
}


static int Bfrac (lua_State *L) {  /* 6.1.4 */
  M_APM r, a, x;
  a = m_apm_init();  /* don't use M_get_stack_var or Agena will crash */
  x = Bget(L, 1);
  r = Bnew(L);
  M_int(a, x);
  m_apm_subtract(r, x, a);
  m_apm_free(a);
  return 1;
}


static int Bintfrac (lua_State *L) {  /* 6.1.4 */
  M_APM q, r, x;
  x = Bget(L, 1);
  luaL_checkstack(L, 2, "not enough stack space");  /* 6.3.9 */
  q = Bnew(L);
  r = Bnew(L);
  if (x->m_apm_sign < 0)
    m_apm_ceil(q, x);
  else
    m_apm_floor(q, x);
  m_apm_subtract(r, x, q);
  return 2;
}


static int Badd (lua_State *L) {  /* add(x, y) */
  return Bdo2(L, m_apm_add);
}

static int mt_add (lua_State *L) {  /* add(x, y), 3.5.2 fix */
  return Bdo2(L, m_apm_add);
}


static int Bsub (lua_State *L) {  /* sub(x, y) */
  return Bdo2(L, m_apm_subtract);
}

static int mt_sub (lua_State *L) {  /* sub(x, y), 3.5.2 fix */
  return Bdo2(L, m_apm_subtract);
}


static int Bmul (lua_State *L) {  /* mul(x, y) */
  return Bdo2(L, m_apm_multiply);
}

static int mt_mul (lua_State *L) {  /* mul(x, y), 3.5.2 fix */
  return Bdo2(L, m_apm_multiply);
}


static int Bdiv (lua_State *L) {  /* div(x, y) */
  M_APM a, b, c;
  a = Bget(L, 1);
  b = Bget(L, 2);
  if (M_iszero(b)) {  /* 6.3.9 change */
    lua_pushundefined(L);
  } else {
    int n = agnL_optnonnegint(L, 3, MYDIGITS);  /* 6.3.6 change from DIGITS to MYDIGITS */
    c = Bnew(L);
    m_apm_divide(c, n, a, b);
  }
  return 1;
}


static int mt_div (lua_State *L) {  /* div(x, y), 6.3.9 change */
  M_APM a, b, c;
  a = Bget(L, 1);
  b = Bget(L, 2);
  if (M_iszero(b)) {  /* 6.3.9 change */
    lua_pushundefined(L);
  } else {
    c = Bnew(L);
    m_apm_divide(c, MYDIGITS, a, b);
  }
  return 1;
}


static int Bidiv (lua_State *L) {  /* idiv(x, y) */
  M_APM a, b, p, q;
  a = Bget(L, 1);
  b = Bget(L, 2);
  luaL_checkstack(L, 2, "not enough stack space");
  if (M_iszero(b)) {  /* 6.3.9 change */
    lua_pushundefined(L);
    lua_pushundefined(L);
  } else {
    p = Bnew(L);
    q = Bnew(L);
    m_apm_integer_div_rem(p, q, a, b);
  }
  return 2;
}


static int Bmod (lua_State *L) {  /* mod(x, y); rewritten/tuned by 25 6.3.6 */
  M_APM a, b, p, q;
  a = Bget(L, 1);
  b = Bget(L, 2);
  if (M_iszero(b)) {  /* 6.3.9 change */
    lua_pushundefined(L);
  } else {
    p = m_apm_init();
    q = Bnew(L);
    m_apm_integer_div_rem(p, q, a, b);
    m_apm_free(p);
  }
  return 1;
}


#define mt_mod Bmod  /* __mod(x, y), 3.5.2 fix */


/* The following macro conducts exponentiation with automatic precision adaption for integral b, 6.3.6 */
#define m_apm_pow_adaptive(r,n,a,b) { \
  if (m_apm_is_integer(b) && M_ispos(b)) { \
    int __rc, e; \
    e = (int)aux_m2dbl(b, b->m_apm_datalength + 2, &__rc); \
    m_apm_integer_pow_nr(r, a, e); \
  } else { \
    m_apm_pow(r, n, a, b); \
  } \
}

static int Bpow (lua_State *L) {  /* pow(x, y) */
  M_APM a, b, r;
  int n;
  n = agnL_optnonnegint(L, 3, DIGITS);
  a = Bget(L, 1);
  b = Bget(L, 2);
  if (M_iszero(a) && M_iszero(b)) {  /* 6.3.9 change */
    lua_pushundefined(L);
  } else {
    r = Bnew(L);
    m_apm_pow_adaptive(r, n, a, b);
  }
  return 1;
}


static int mt_pow (lua_State *L) {  /* pow(x, y), 3.5.2 fix, 6.3.6 improvement */
  return Bpow(L);  /* Bdo3(L, m_apm_pow); */
}


static int Bipow (lua_State *L) {  /* x ** n, 2.33.1, 3.5.2 fix */
  M_APM x, r;
  int n;
  x = Bget(L, 1);
  n = agn_checkinteger(L, 2);
  if (M_iszero(x) && n == 0) {  /* 6.3.9 change */
    lua_pushundefined(L);
  } else {
    r = Bnew(L);
    m_apm_integer_pow_nr(r, x, n);  /* 6.3.6 fix */
  }
  return 1;
}


#define ISPOW2PREC 256  /* 6.3.9 change, better, faster than 512 */
static int mapm_xispow2 (lua_State *L) {  /* 6.3.7 */
  int r, rc, flag;
  lua_Number x;
  M_APM a = Bget(L, 1);
  r = flag = 0;
  if (M_isnonpos(a) || (m_apm_compare(a, MM_One) > 0 && !m_apm_is_integer(a)) ) {
    /* do nothing */
  } else {
    int isint;
    M_APM b, c, d;
    b = m_apm_init();
    c = m_apm_init();
    d = m_apm_init();
    if (m_apm_compare(a, MM_One) < 0) {  /* 0 < a < 1 ? check for 1/2, 1/4, 1/8, 1/16, etc. */
      m_apm_reciprocal(b, ISPOW2PREC, a);
      m_apm_round(c, ISPOW2PREC, b);  /* do not use m_apm_floor ! */
      m_apm_subtract(d, c, b);
      x = aux_m2dbl(d, IEEEPRECISION, &rc);
      if ( ( flag = tools_approx(x, 0, agn_getepsilon(L)) ) ) {
        m_apm_copy(b, c);
      }
    } else {
      m_apm_copy(b, a);
    }
    isint = m_apm_is_integer(b);
    if (isint || flag) {  /* a >= 1 or (a < 1 and candidate) ? */
      m_apm_log(c, isint ? b->m_apm_datalength + 2 : ISPOW2PREC, b);
      m_apm_divide(d, isint ? c->m_apm_datalength + 2 : ISPOW2PREC, c, MM_lc_log2);
      m_apm_round(b, d->m_apm_datalength, d);  /* do not use m_apm_floor ! */
      m_apm_subtract(c, d, b);
      x = aux_m2dbl(c, IEEEPRECISION, &rc);  /* c is now equals or near zero, so we can safely compare */
      r = tools_approx(x, 0, agn_getepsilon(L));
    }
    mfreeall(b, c, d);
  }
  lua_pushboolean(L, r);
  return 1;
}


static int Bcompare (lua_State *L) {  /* compare(x, y) */
  M_APM a, b;
  a = Bget(L, 1);
  b = Bget(L, 2);
  lua_pushinteger(L, m_apm_compare(a, b));
  return 1;
}


static int Beq (lua_State *L) {
  M_APM a, b;
  a = Bget(L, 1);
  b = Bget(L, 2);
  lua_pushboolean(L, m_apm_compare(a, b) == 0);
  return 1;
}

static int mt_eq (lua_State *L) {  /* 3.5.2 fix */
  return Beq(L);
}


static int Blt (lua_State *L) {
  M_APM a, b;
  a = Bget(L, 1);
  b = Bget(L, 2);
  lua_pushboolean(L, m_apm_compare(a, b) < 0);
  return 1;
}

static int mt_lt (lua_State *L) {  /* 3.5.2 fix */
  return Blt(L);
}


static int mt_le (lua_State *L) {  /* new 3.5.2 */
  M_APM a, b;
  a = Bget(L, 1);
  b = Bget(L, 2);
  lua_pushboolean(L, m_apm_compare(a, b) <= 0);
  return 1;
}


static int Bsign (lua_State *L) {  /* sign(x) */
  M_APM a = Bget(L, 1);
  lua_pushinteger(L, m_apm_sign(a));
  return 1;
}

static int mt_sign (lua_State *L) {  /* sign mt, 3.5.2 fix */
  M_APM a, r;
  a = Bget(L, 1);
  r = Bnew(L);
  m_apm_set_double(r, (double)m_apm_sign(a));
  return 1;
}


static int mt_abs (lua_State *L) {  /* abs mt */
  return Bdo0(L, m_apm_absolute_value);
}


static int mt_absdiff (lua_State *L) {  /* `|-` operator, 7.5.8 */
  M_APM a, b, c, d;
  a = Bget(L, 1);
  b = Bget(L, 2);
  d = m_apm_init();
  m_apm_subtract(d, a, b);
  c = Bnew(L);
  m_apm_absolute_value(c, d);
  m_apm_free(d);
  return 1;
}


static int Bexponent (lua_State *L) {  /* exponent(x) */
  M_APM a = Bget(L, 1);
  lua_pushinteger(L, m_apm_exponent(a));
  return 1;
}


static int Bisint (lua_State *L) {  /* __integral */
  M_APM a = Bget(L, 1);
  lua_pushboolean(L, m_apm_is_integer(a));
  return 1;
}


static int Bisfrac (lua_State *L) {  /* __fractional */
  M_APM a = Bget(L, 1);
  lua_pushboolean(L, !m_apm_is_integer(a));
  return 1;
}


static int mapm_checkxnumber (lua_State *L) {  /* 6.4.6 */
  int i, nargs = lua_gettop(L);
  for (i=0; i < nargs; i++) {
    if (!ismapm(L, i + 1)) {
      luaL_error(L, "Error: value #%d must be a real MAPM number, got %s.", i + 1, luaL_typename(L, i + 1));
    }
  }
  return 0;
}


static int mapm_checkcnumber (lua_State *L) {  /* 6.4.6 */
  int i, nargs = lua_gettop(L);
  for (i=0; i < nargs; i++) {
    if (!iscmapm(L, i + 1)) {
      luaL_error(L, "Error: value #%d must be a complex MAPM number, got %s.", i + 1, luaL_typename(L, i + 1));
    }
  }
  return 0;
}


static int mapm_isxnumber (lua_State *L) {  /* 6.4.6 */
  int i, r = 1, nargs = lua_gettop(L);
  for (i=0; i < nargs && r; i++) {  /* 6.6.5 change */
    r = ismapm(L, i + 1);
  }
  lua_pushboolean(L, r);
  return 1;
}


static int mapm_iscnumber (lua_State *L) {  /* 6.4.6 */
  int i, r = 1, nargs = lua_gettop(L);
  for (i=0; i < nargs && r; i++) {  /* 6.6.5 change */
    r = iscmapm(L, i + 1);
  }
  lua_pushboolean(L, r);
  return 1;
}


static int mapm_checkinteger (lua_State *L) {
  int i, nargs = lua_gettop(L);
  for (i=0; i < nargs; i++) {
    if (!ismapm(L, i + 1)) {  /* 6.4.6 extension */
      luaL_error(L, "Error: argument #%d must be a real MAPM number.", i + 1);
    }
    if (!m_apm_is_integer(Bget(L, i + 1))) {
      luaL_error(L, "Error: argument #%d must be integral.", i + 1);
    }
  }
  return 0;
}


static int mapm_checknonnegint (lua_State *L) {
  M_APM a;
  int i, nargs = lua_gettop(L);
  if (nargs > 1 && lua_isboolean(L, nargs)) {
    if (agn_getbitwise(L))  /* 6.1.5 */
      luaL_error(L, "Error: cannot compute with signed integers.");
    nargs--;
  }
  for (i=0; i < nargs; i++) {
    if (!ismapm(L, i + 1)) {  /* 6.4.6 extension */
      luaL_error(L, "Error: argument #%d must be a real MAPM number.", i + 1);
    }
    a = Bget(L, i + 1);
    if (!m_apm_is_integer(a) || (m_apm_compare(a, MM_Zero) < 0)) {
      luaL_error(L, "Error: argument #%d must be a non-negative integral.", i + 1);
    }
  }
  return 0;
}


static int mapm_checknonnegative (lua_State *L) {  /* 6.4.10 */
  M_APM a;
  int i, nargs = lua_gettop(L);
  if (nargs > 1 && lua_isboolean(L, nargs)) {
    if (agn_getbitwise(L))  /* 6.1.5 */
      luaL_error(L, "Error: cannot compute with signed integers.");
    nargs--;
  }
  for (i=0; i < nargs; i++) {
    if (!ismapm(L, i + 1)) {  /* 6.4.6 extension */
      luaL_error(L, "Error: argument #%d must be a real MAPM number.", i + 1);
    }
    a = Bget(L, i + 1);
    if (m_apm_compare(a, MM_Zero) < 0) {
      luaL_error(L, "Error: argument #%d must be a non-negative MAPM number.", i + 1);
    }
  }
  return 0;
}


static int mapm_checkposint (lua_State *L) {  /* 6.4.10 */
  M_APM a;
  int i, nargs = lua_gettop(L);
  if (nargs > 1 && lua_isboolean(L, nargs)) {
    if (agn_getbitwise(L))  /* 6.1.5 */
      luaL_error(L, "Error: cannot compute with signed integers.");
    nargs--;
  }
  for (i=0; i < nargs; i++) {
    if (!ismapm(L, i + 1)) {  /* 6.4.6 extension */
      luaL_error(L, "Error: argument #%d must be a real MAPM number.", i + 1);
    }
    a = Bget(L, i + 1);
    if (!m_apm_is_integer(a) || (m_apm_compare(a, MM_Zero) <= 0)) {
      luaL_error(L, "Error: argument #%d must be a positive integral.", i + 1);
    }
  }
  return 0;
}


static int mapm_checkpositive (lua_State *L) {  /* 6.4.10 */
  M_APM a;
  int i, nargs = lua_gettop(L);
  if (nargs > 1 && lua_isboolean(L, nargs)) {
    if (agn_getbitwise(L))  /* 6.1.5 */
      luaL_error(L, "Error: cannot compute with signed integers.");
    nargs--;
  }
  for (i=0; i < nargs; i++) {
    if (!ismapm(L, i + 1)) {  /* 6.4.6 extension */
      luaL_error(L, "Error: argument #%d must be a real MAPM number.", i + 1);
    }
    a = Bget(L, i + 1);
    if (m_apm_compare(a, MM_Zero) <= 0) {
      luaL_error(L, "Error: argument #%d must be a positive MAPM number.", i + 1);
    }
  }
  return 0;
}


static int Biseven (lua_State *L) {  /* iseven(x) */
  M_APM a = Bget(L, 1);
  lua_pushboolean(L, m_apm_is_integer(a) && m_apm_is_even(a));
  return 1;
}

static int mt_even (lua_State *L) {  /* even(x), new 3.5.2 */
  M_APM a;
  a = Bget(L, 1);
  lua_pushboolean(L, m_apm_is_integer(a) && m_apm_is_even(a));
  return 1;
}


static int Bisodd (lua_State *L) {  /* isodd(x) */
  M_APM a = Bget(L, 1);
  lua_pushboolean(L, m_apm_is_integer(a) && m_apm_is_odd(a));
  return 1;
}

static int mt_odd (lua_State *L) {  /* odd(x), new 3.5.2 */
  M_APM a;
  a = Bget(L, 1);
  lua_pushboolean(L, m_apm_is_integer(a) && m_apm_is_odd(a));
  return 1;
}


static int mt_zero (lua_State *L) {  /* zero(x), new 3.6.9 */
  M_APM a = Bget(L, 1);
  lua_pushboolean(L, M_iszero(a));
  return 1;
}


static int mt_nonzero (lua_State *L) {  /* nonzero(x), new 3.6.9 */
  M_APM a = Bget(L, 1);
  lua_pushboolean(L, !M_iszero(a));
  return 1;
}


static int Bispositive (lua_State *L) {  /* 6.4.10 */
  int i, r = 1, nargs = lua_gettop(L);
  if (nargs == 0) {  /* 6.5.3 */
    luaL_error(L, "Error in " LUA_QS ": need at least one value.", "mapm.ispositive");
  }
  for (i=0; i < nargs && r; i++) {  /* 6.5.0 extension */
    r = M_ispos(Bget(L, i + 1));
  }
  lua_pushboolean(L, r);
  return 1;
}


static int Bisnonnegative (lua_State *L) {  /* 6.4.10 */
  int i, r = 1, nargs = lua_gettop(L);
  if (nargs == 0) {  /* 6.5.3 */
    luaL_error(L, "Error in " LUA_QS ": need at least one value.", "mapm.isnonnegative");
  }
  for (i=0; i < nargs && r; i++) {  /* 6.5.0 extension */
    r = M_isnonneg(Bget(L, i + 1));
  }
  lua_pushboolean(L, r);
  return 1;
}


static int Bisnegative (lua_State *L) {  /* 6.5.1 */
  int i, r = 1, nargs = lua_gettop(L);
  if (nargs == 0) {  /* 6.5.3 */
    luaL_error(L, "Error in " LUA_QS ": need at least one value.", "mapm.isnegative");
  }
  for (i=0; i < nargs && r; i++) {
    r = !(M_isnonneg(Bget(L, i + 1)));
  }
  lua_pushboolean(L, r);
  return 1;
}


static int Bisposint (lua_State *L) {  /* 6.5.1 */
  M_APM a;
  int i, r = 1, nargs = lua_gettop(L);
  if (nargs == 0) {  /* 6.5.3 */
    luaL_error(L, "Error in " LUA_QS ": need at least one value.", "mapm.isposint");
  }
  for (i=0; i < nargs && r; i++) {
    a = Bget(L, i + 1);
    r = M_isint(a) && M_ispos(a);
  }
  lua_pushboolean(L, r);
  return 1;
}


static int Bisnonnegint (lua_State *L) {  /* 6.5.1 */
  M_APM a;
  int i, r = 1, nargs = lua_gettop(L);
  if (nargs == 0) {  /* 6.5.3 */
    luaL_error(L, "Error in " LUA_QS ": need at least one value.", "mapm.isnonnegint");
  }
  for (i=0; i < nargs && r; i++) {
    a = Bget(L, i + 1);
    r = M_isint(a) && !M_isneg(a);
  }
  lua_pushboolean(L, r);
  return 1;
}


static int Bisnegint (lua_State *L) {  /* 6.5.1 */
  M_APM a;
  int i, r = 1, nargs = lua_gettop(L);
  if (nargs == 0) {  /* 6.5.3 */
    luaL_error(L, "Error in " LUA_QS ": need at least one value.", "mapm.isnegint");
  }
  for (i=0; i < nargs && r; i++) {
    a = Bget(L, i + 1);
    r = M_isint(a) && M_isneg(a);
  }
  lua_pushboolean(L, r);
  return 1;
}


static int Bdigitsin (lua_State *L) {  /* digitsin(x) */
  M_APM a = Bget(L, 1);
  lua_pushinteger(L, m_apm_significant_digits(a));
  return 1;
}


static int mt_gc (lua_State *L) {  /* this is the better-be-sure-than-sorry GC metamethod */
  if (ismapm(L, 1)) {
    M_APM a = Bget(L, 1);
    if (a) {  /* added 3.5.4 */
      lua_setmetatabletoobject(L, 1, NULL, 1);
      m_apm_free(a);
      /* 6.6.3: Do not call this now:
      m_apm_trim_mem_usage();
      as this will crash Agena especially on Linux. */
    }
  }
  return 0;
}


/* Computes the n-th Chebyshev polynomial of the first kind, evaluated at x, with n a non-negative integer and x a number. 2.33.1
   42 percent faster than the Agena implementation which returns cos(n*arccos(x)):
   mapm.xchebyt := proc(n, x) is
     local t1, t4, t6, t9, t15, t17, t20
     if n :: nonnegint then
        n := mapm.xnumber(n)
     fi;
     if x :: number then
        x := mapm.xnumber(x)
     fi
     t1  := x*x;
     t4  := sqrt(t1 + mapm.one + mapm.two*x);
     t6  := sqrt(t1 + mapm.one - mapm.two*x);
     t9  := n*arccos(mapm.half*(t4 - t6));
     t15 := (mapm.half*(t4 + t6))^mapm.two;
     t17 := sqrt(t15 - mapm.one);
     t20 := n*sign(x)*ln(mapm.half*(t4 + t6) + t17);
     return cos(t9)*cosh(t20)
  end; */
static void m_apm_chebyt (M_APM rr, int places, M_APM n, M_APM x) {  /* 2.33.1, result is in rr */
  M_APM a, b, c, d, t4, t6, t9, t15, t17;
  a   = M_get_stack_var();
  b   = M_get_stack_var();
  c   = M_get_stack_var();
  d   = M_get_stack_var();
  t4  = M_get_stack_var();
  t6  = M_get_stack_var();
  t9  = M_get_stack_var();
  t15 = M_get_stack_var();
  t17 = M_get_stack_var();
  /* t1(=a) = x*x; */
  m_apm_multiply(a, x, x);
  /* t4 = sqrt(t1 + 1.0 + 2.0*x);
     t6 = sqrt(t1 + 1.0 - 2.0*x); */
  m_apm_add(c, a, MM_One);
  m_apm_multiply(d, x, MM_Two);
  m_apm_add(a, c, d);
  m_apm_subtract(b, c, d);
  m_apm_sqrt(t4, places, a);
  m_apm_sqrt(t6, places, b);
  /* t9 = n*sun_acos(0.5*(t4 - t6)); */
  m_apm_subtract(a, t4, t6);
  m_apm_multiply(b, a, MM_0_5);  /* 6.6.5 change */
  m_apm_arccos(c, places, b);
  m_apm_multiply(t9, n, c);
  /* t15 = sun_pow(0.5*(t4 + t6), 2.0, 1); */
  m_apm_add(a, t4, t6);
  m_apm_multiply(b, a, MM_0_5);  /* 6.6.5 change */
  m_apm_multiply(t15, b, b);
  /* t17 = sqrt(t15 - 1.0); */
  m_apm_subtract(a, t15, MM_One);
  m_apm_sqrt(t17, places, a);
  /* t20 = n*tools_sign(a)*sun_log(0.5*(t4 + t6) + t17); */
  m_apm_set_double(a, (double)m_apm_sign(x));
  m_apm_multiply(b, n, a);
  m_apm_add(c, t4, t6);
  m_apm_multiply(d, c, MM_0_5);  /* 6.6.5 change */
  m_apm_add(d, d, t17);
  m_apm_log(d, places, d);
  m_apm_multiply(rr, b, d);
  /* sun_cos(t9)*sun_cosh(t20=rr) */
  m_apm_cos(a, places, t9);
  m_apm_cosh(b, places, rr);
  m_apm_multiply(rr, a, b);  /* result is in rr */
  M_restore_stack(9);  /* restore the 9 locals we used here */
}

static int Bchebyt (lua_State *L) {
  return Bdo3(L, m_apm_chebyt);
}


static void m_apm_hypot (M_APM rr, int places, M_APM x, M_APM y) {  /* 2.33.1 */
  M_APM a, b, c;
  a = m_apm_init();  /* don't use M_get_stack_var or Agena will crash */
  b = m_apm_init();
  c = m_apm_init();
  m_apm_multiply(a, x, x);
  m_apm_multiply(b, y, y);
  m_apm_add(c, a, b);
  m_apm_sqrt(rr, places + 2, c);
  mfreeall(a, b, c);
}

static int Bhypot (lua_State *L) {
  return Bdo3(L, m_apm_hypot);
}


static int m_apm_cathet (M_APM rr, int places, M_APM x, M_APM y) {  /* 3.3.3, 6.6.3 change */
  M_APM a, b, c;
  int rc = 0;
  a = m_apm_init();  /* don't use M_get_stack_var or Agena will crash */
  b = m_apm_init();
  c = m_apm_init();
  m_apm_multiply(a, x, x);
  m_apm_multiply(b, y, y);
  m_apm_subtract(c, a, b);
  if (m_apm_compare(c, MM_Zero) < 0) {
    M_set_to_zero(rr);
    rc = 1;
  } else {
    m_apm_sqrt(rr, places + 2, c);
  }
  mfreeall(a, b, c);
  return rc;
}

static int Bcathet (lua_State *L) {
  M_APM a, b, c;
  int n;
  n = agnL_optnonnegint(L, 3, MYDIGITS);  /* 6.3.6 change from DIGITS to MYDIGITS */
  a = Bget(L, 1);
  b = Bget(L, 2);
  c = Bnew(L);
  if (m_apm_cathet(c, n, a, b)) {  /* |difference| < 0, 6.6.3 change */
    lua_pop(L, 1);
    lua_pushundefined(L);
  }
  return 1;
}


static void m_apm_square (M_APM rr, M_APM x) {  /* 2.33.1, changed 6.3.6 */
  m_apm_multiply(rr, x, x);
}

static int mt_square (lua_State *L) {  /* square mt, 2.33.1 */
  return Bdo0(L, m_apm_square);
}

static int Bsquare (lua_State *L) {  /* square mt, 3.3.3 */
  return Bdo0(L, m_apm_square);
}


static void m_apm_cube (M_APM rr, M_APM x) {  /* 2.33.1, 6.3.6 improvement */
  m_apm_integer_pow_nr(rr, x, 3);
}

static int mt_cube (lua_State *L) {  /* cube mt, 2.33.1 */
  return Bdo0(L, m_apm_cube);
}

static int Bcube (lua_State *L) {  /* cube mt, 3.3.3 */
  return Bdo0(L, m_apm_cube);
}


static int mapm_xfma (lua_State *L) {  /* x*y + z, 2.33.1 */
  M_APM a, x, y, z, r;
  a = m_apm_init();  /* don't use M_get_stack_var or Agena will crash */
  x = Bget(L, 1);
  y = Bget(L, 2);
  z = Bget(L, 3);
  r = Bnew(L);
  m_apm_multiply(a, x, y);
  m_apm_add(r, a, z);
  m_apm_free(a);
  return 1;
}


static int mapm_xterm (lua_State *L) {  /* c*x**n, 2.33.1 */
  M_APM r, a, c, x, n;
  a = m_apm_init();  /* don't use M_get_stack_var or Agena will crash */
  c = Bget(L, 1);
  x = Bget(L, 2);
  n = Bget(L, 3);
  r = Bnew(L);
  m_apm_pow_adaptive(a, MYDIGITS, x, n);  /* 6.3.6 improvement */
  m_apm_multiply(r, c, a);
  m_apm_free(a);
  return 1;
}


static int mapm_xlog (lua_State *L) {  /* 2.33.2 */
  int places;
  M_APM r, a, b, x, n;
  a = m_apm_init();  /* don't use M_get_stack_var or Agena will crash */
  b = m_apm_init();
  x = Bget(L, 1);
  n = Bget(L, 2);
  places = agnL_optinteger(L, 3, MYDIGITS);
  r = Bnew(L);
  m_apm_log(a, places + 2, x);
  m_apm_log(b, places + 2, n);
  m_apm_divide(r, places, a, b);
  m_apm_free(a); m_apm_free(b);
  return 1;
}


static int mapm_xlog2 (lua_State *L) {  /* 2.33.2 */
  int places;
  M_APM r, a, x;
  a = m_apm_init();  /* don't use M_get_stack_var or Agena will crash */
  x = Bget(L, 1);
  places = agnL_optinteger(L, 2, MYDIGITS);
  r = Bnew(L);
  m_apm_log(a, places + 2, x);
  m_apm_divide(r, places, a, MM_lc_log2);  /* 3.5.2 tuning */
  m_apm_free(a);
  return 1;
}


static int mapm_xexp2 (lua_State *L) {  /* 2.33.2 */
  int places;
  M_APM r, x;
  x = Bget(L, 1);
  places = agnL_optinteger(L, 2, MYDIGITS);
  r = Bnew(L);
  m_apm_pow_adaptive(r, places, MM_Two, x);  /* 6.3.6 improvement */
  return 1;
}


static int mt_antilog2 (lua_State *L) {  /* 6.4.0 */
  M_APM r, x;
  x = Bget(L, 1);
  r = Bnew(L);
  m_apm_pow_adaptive(r, MYDIGITS, MM_Two, x);
  return 1;
}


static int mapm_xexp10 (lua_State *L) {  /* 2.33.2 */
  int places;
  M_APM r, x;
  x = Bget(L, 1);
  places = agnL_optinteger(L, 2, MYDIGITS);
  r = Bnew(L);
  m_apm_pow_adaptive(r, places, MM_Ten, x);  /* 6.3.6 improvement */
  return 1;
}


static int mt_antilog10 (lua_State *L) {  /* 6.4.0 */
  M_APM r, x;
  x = Bget(L, 1);
  r = Bnew(L);
  m_apm_pow_adaptive(r, MYDIGITS, MM_Ten, x);
  return 1;
}


static int mapm_xrandom (lua_State *L) {  /* new 3.5.2 */
  M_APM r = Bnew(L);
  m_apm_get_random(r);
  return 1;
}

/* Example: m_apm_set_random_seed("12345678");

  This function will set the random number generator to a known starting value.

  The char string argument should correspond to any *integer* value between 0 and (1.0E+15 - 1).

  This function can be called at any time, either before or anytime after 'm_apm_get_random'. */
static int mapm_xrandomseed (lua_State *L) {  /* 3.5.2 */
  int i;
  const char *str = agn_checkstring(L, 1);
  for (i=0; i < tools_strlen(str); i++) {
    if (!isdigit(str[i])) luaL_error(L, "Error in " LUA_QS ": character is non-numeric.", "mapm.xrandomseed");
  }
  m_apm_set_random_seed((char *)str);
  return 0;
}


#define aux_swap(x,y,a,swap) { \
  if ( (swap = m_apm_compare(x, y) < 0) ) { \
    m_apm_copy(a, x); \
    m_apm_copy(x, y); \
    m_apm_copy(y, a); \
  } \
}

#define aux_swapback(x,y,a,swap) { \
  if (swap) { \
    m_apm_copy(a, x); \
    m_apm_copy(x, y); \
    m_apm_copy(y, a); \
  } \
}

/* The function computes intermediate results for the bitwise and, or, xor operations:
   p := mapm.two**n;
   a := (x\p) % mapm.two;  -> either 0 or 1
   b := (y\p) % mapm.two;  -> either 0 or 1
   The function just pushes p as an MAPM xnumber, and a, b as ordinary Agena numbers to spare the
   garbage collector and the VM from too much work. */
static void m_apm_bprep (M_APM rr, M_APM x, M_APM y, int n, int nargs, int swap, lua_Number *s, lua_Number *t, int *rc) {  /* 6.1.5 */
  M_APM a, b, c;
  a = M_get_stack_var();
  b = M_get_stack_var();
  c = M_get_stack_var();
  if (nargs == 3 && swap) { aux_swap(x, y, a, swap); }
  m_apm_integer_pow_nr(rr, MM_Two, n);  /* 6.3.6 improvement */
  m_apm_integer_divide(a, x, rr);
  m_apm_integer_div_rem(b, c, a, MM_Two);
  *s = aux_m2dbl(c, 20, rc);
  if (nargs == 3) {
    m_apm_integer_divide(a, y, rr);
    m_apm_integer_div_rem(b, c, a, MM_Two);
    *t = aux_m2dbl(c, 20, rc);
  } else
    *t = AGN_NAN;
  if (nargs == 3) { aux_swapback(x, y, a, swap); }
  M_restore_stack(3);  /* restore the 3 locals we used here */
}

static int mapm_bprep (lua_State *L) {  /* 6.1.4 */
  lua_Number s, t;
  int n, nargs, rc;
  M_APM x, y, p;
  nargs = lua_gettop(L);
  if (nargs < 2 || nargs > 3) {
    luaL_error(L, "Error in " LUA_QS ": need two or three arguments.", "mapm.bprep");
  }
  x = Bget(L, 1);
  y = (nargs == 3) ? Bget(L, 2) : MM_Zero;
  n = agn_checknonnegint(L, nargs);
  luaL_checkstack(L, nargs, "not enough stack space");
  p = Bnew(L);
  m_apm_bprep(p, x, y, n, nargs, 1, &s, &t, &rc);
  if (!rc) {
    luaL_error(L, "Error in " LUA_QS ": internal conversion failed.", "mapm.bprep");
  }
  lua_pushnumber(L, s);
  if (nargs == 3) {
    lua_pushnumber(L, t);
  }
  return nargs;
}

static void aux_checknonnegint (lua_State *L, M_APM x, M_APM y, const char *procname) {
  if (agn_getbitwise(L)) {
    luaL_error(L, "Error in " LUA_QS ": cannot compute with signed integers.", procname);
  }
  if (!m_apm_is_integer(x) || m_apm_compare(x, MM_Zero) < 0 ||
      !m_apm_is_integer(y) || m_apm_compare(y, MM_Zero) < 0) {
    luaL_error(L, "Error in " LUA_QS ": values must be a non-negative integral.", procname);
  }
}

static void aux_issueerror (lua_State *L, M_APM x, M_APM y, M_APM z, const char *procname) {
  mfreeall(x, y, z);
  luaL_error(L, "Error in " LUA_QS ": internal conversion failed.", procname);
}

static int aux_log2x (M_APM x, M_APM a, M_APM b, int *rc) {
  if (M_iszero(x)) { *rc = 1; return -1; }
  m_apm_log(a, DIGITS + 2, x);
  m_apm_divide(b, DIGITS, a, MM_lc_log2);
  return (int)aux_m2dbl(b, 20, rc);
}

/* mapm.xband := proc(x :: xnumber, y :: xnumber) is
   local p, a, b, r;
   mapm.checknonnegint(x, y);
   if environ.kernel('signedbits') then
      error('Error in `mapm.xband`: cannot compute with signed integers.')
   fi;
   if x < y then x, y := y, x fi;
   r := mapm.naught;
   for n from 0 to mapm.xtonumber(mapm.xlog2(x)) do
      p, a, b := mapm.bprep(x, y, n);
      r +:= p*(a*b)
   od;
   return r
end; */

static int mapm_xband (lua_State *L) {
  int n, rc, ln2x;
  lua_Number s, t;
  M_APM x, y, a, b, p, r;
  x = Bget(L, 1);
  y = Bget(L, 2);
  aux_checknonnegint(L, x, y, "mapm.xband");
  a = m_apm_init();
  b = m_apm_init();
  p = m_apm_init();
  ln2x = aux_log2x(m_apm_compare(x, y) < 0 ? y : x, a, b, &rc);
  if (!rc) { aux_issueerror(L, a, b, p, "mapm.xband"); }
  r = Bzero(L);  /* the accumulator */
  for (n=0; n <= ln2x; n++) {
    m_apm_bprep(p, x, y, n, 3, 0, &s, &t, &rc);
    if (!rc) { aux_issueerror(L, a, b, p, "mapm.xband"); }
    m_apm_set_double(a, s*t);
    m_apm_multiply(b, p, a);
    m_apm_add(a, b, r);
    m_apm_copy(r, a);
  }
  mfreeall(a, b, p);
  return 1;
}


/* mapm.xbor := proc(x :: xnumber, y :: xnumber) is
   local p, a, b, r;
   mapm.checknonnegint(x, y);
   if environ.kernel('signedbits') then
      error('Error in `mapm.xbor`: cannot compute with signed integers.')
   fi;
   if x < y then x, y := y, x fi;
   r := mapm.naught;
   for n from 0 to mapm.xtonumber(mapm.xlog2(x)) do
      p, a, b := mapm.bprep(x, y, n);
      r +:= p*(a + b - a*b)
   od;
   return r
end; */

static int mapm_xbor (lua_State *L) {
  int n, rc, ln2x;
  lua_Number s, t;
  M_APM x, y, a, b, p, r;
  x = Bget(L, 1);
  y = Bget(L, 2);
  aux_checknonnegint(L, x, y, "mapm.xbor");
  a = m_apm_init();
  b = m_apm_init();
  p = m_apm_init();
  ln2x = aux_log2x(m_apm_compare(x, y) < 0 ? y : x, a, b, &rc);
  if (!rc) { aux_issueerror(L, a, b, p, "mapm.xbor"); }
  r = Bzero(L);  /* the accumulator */
  for (n=0; n <= ln2x; n++) {
    m_apm_bprep(p, x, y, n, 3, 0, &s, &t, &rc);
    if (!rc) { aux_issueerror(L, a, b, p, "mapm.xbor"); }
    m_apm_set_double(a, s + t - s*t);
    m_apm_multiply(b, p, a);
    m_apm_add(a, b, r);
    m_apm_copy(r, a);
  }
  mfreeall(a, b, p);
  return 1;
}


/* mapm.xbxor2 := proc(x :: xnumber, y :: xnumber) is
   local p, a, b, r;
   mapm.checknonnegint(x, y);
   if environ.kernel('signedbits') then
      error('Error in `mapm.xbxor`: cannot compute with signed integers.')
   fi;
   if x < y then x, y := y, x fi;
   r := mapm.naught;
   for n from 0 to mapm.xtonumber(mapm.xlog2(x)) do
      p, a, b := mapm.bprep(x, y, n);
      r +:= p*((a + b) % 2)
   od;
   return r
end; */

static int mapm_xbxor (lua_State *L) {
  int n, rc, ln2x;
  lua_Number s, t;
  M_APM x, y, a, b, p, r;
  x = Bget(L, 1);
  y = Bget(L, 2);
  aux_checknonnegint(L, x, y, "mapm.xbxor");
  a = m_apm_init();
  b = m_apm_init();
  p = m_apm_init();
  ln2x = aux_log2x(m_apm_compare(x, y) < 0 ? y : x, a, b, &rc);
  if (!rc) { aux_issueerror(L, a, b, p, "mapm.xbxor"); }
  r = Bzero(L);  /* the accumulator */
  for (n=0; n <= ln2x; n++) {
    m_apm_bprep(p, x, y, n, 3, 0, &s, &t, &rc);
    if (!rc) { aux_issueerror(L, a, b, p, "mapm.xbxor"); }
    m_apm_set_double(a, (int)(s + t) % 2);
    m_apm_multiply(b, p, a);
    m_apm_add(a, b, r);
    m_apm_copy(r, a);
  }
  mfreeall(a, b, p);
  return 1;
}


/****************************************************************************/

/* Taken from mapm 4.9.5/... primenum.c */

/*
 *  M_APM  -  primenum.c
 *
 *  Copyright (C) 1999 - 2007   Michael C. Ring
 *
 *  Permission to use, copy, and distribute this software and its
 *  documentation for any purpose with or without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and
 *  that both that copyright notice and this permission notice appear
 *  in supporting documentation.
 *
 *  Permission to modify the software is granted. Permission to distribute
 *  the modified code is granted. Modifications are to be distributed by
 *  using the file 'license.txt' as a template to modify the file header.
 *  'license.txt' is available in the official MAPM distribution.
 *
 *  This software is provided "as is" without express or implied warranty.
 *
 *  $Id: primenum.c,v 1.10 2007/12/03 02:05:01 mike Exp $
 *
 *  PRIME Number Generator using the MAPM Library
 *
 *	$Log: primenum.c,v $
 *	Revision 1.10  2007/12/03 02:05:01  mike
 *	update version */

static int mapm_is_number_prime (M_APM input) {
  int ii, ret, index;
  /* since the real algorithm starts at 11 (to synchronize with the increment table), we will cheat for numbers < 10. */
  if (input->m_apm_sign < 0 || !m_apm_is_integer(input)) {
    ret = 0;
  } else if (m_apm_compare(input, MM_Ten) <= 0) {
    int sigdigs = m_apm_significant_digits(input);
    char *sbuf = malloc( (sigdigs < 32 ? 32 : 1 + sigdigs) * sizeof(char) );
    if (sbuf == NULL) return -1;
    m_apm_to_integer_string(sbuf, input);
    ii = atoi(sbuf);
    ret = (ii == 2 || ii == 3 || ii == 5 || ii == 7);
    xfree(sbuf);
  } else {
    M_APM M_limit, M_digit, M_quot, M_rem, M_tmp0, M_tmp1;
    /* for reference:
     * table size of 2 to filter multiples of 2 and 3
     * table size of 8 to filter multiples of 2, 3 and 5
     * table size of 480 to filter multiples of 2, 3, 5, 7, and 11.
     * This increment table will filter out all numbers that are multiples of 2, 3, 5 and 7. */
    static char incr_table[48] = {
      2, 4, 2, 4, 6, 2, 6, 4, 2, 4, 6, 6, 2, 6,  4, 2,
      6, 4, 6, 8, 4, 2, 4, 2, 4, 8, 6, 4, 6, 2,  4, 6,
      2, 6, 6, 4, 2, 4, 6, 2, 6, 4, 2, 4, 2, 10, 2, 10 };
    M_limit = m_apm_init();
    M_digit = m_apm_init();
    M_quot = m_apm_init();
    M_rem = m_apm_init();
    M_tmp0 = m_apm_init();
    M_tmp1 = m_apm_init();
    ret = index = 0;
    /* see if the input number is a multiple of 3, 5, or 7. */
    m_apm_integer_div_rem(M_quot, M_rem, input, MM_Three);
    if (m_apm_sign(M_rem) == 0) goto endofit;  /* remainder == 0 */
    m_apm_integer_div_rem(M_quot, M_rem, input, MM_Five);
    if (m_apm_sign(M_rem) == 0) goto endofit;
    m_apm_set_long(M_digit, 7L);
    m_apm_integer_div_rem(M_quot, M_rem, input, M_digit);
    if (m_apm_sign(M_rem) == 0) goto endofit;
    ii = m_apm_exponent(input) + 16;
    m_apm_sqrt(M_tmp1, ii, input);
    m_apm_add(M_limit, MM_Two, M_tmp1);
    m_apm_set_long(M_digit, 11L);  /* now start at '11' to check */
    while (1) {
      if (m_apm_compare(M_digit, M_limit) >= 0) { ret = 1; break; }
      m_apm_integer_div_rem(M_quot, M_rem, input, M_digit);
      if (m_apm_sign(M_rem) == 0) break;  /* remainder == 0 */
      m_apm_set_long(M_tmp1, (long)incr_table[index]);
      m_apm_add(M_tmp0, M_digit, M_tmp1);
      m_apm_copy(M_digit, M_tmp0);
      if (++index == 48) index = 0;
    }
endofit:
    m_apm_free(M_limit);
    m_apm_free(M_digit);
    m_apm_free(M_quot);
    m_apm_free(M_rem);
    m_apm_free(M_tmp0);
    m_apm_free(M_tmp1);
  }
  return ret;
}

static int Bisprime (lua_State *L) {  /* 6.1.4 */
  M_APM a = Bget(L, 1);
  int rc = mapm_is_number_prime(a);
  if (rc == -1)
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "mapm.xisprime");
  lua_pushboolean(L, rc);
  return 1;
}


/* Calculates the Greatest Common Divicor (GCD) of two integers a, b, according to the formula:
tools_gcdu64 := proc (a, b) {
  local t;
  if fractional a or fractional b then return mapm.one fi;
  while b <> 0 do
    t := b;
    b := a % b;
    a := t
  od;
  return a;
end

MAPM has a built-in function m_apm_gcd() which is as fast is this implementation. 6.3.6 */

/* See mapm_4.9.5/mapmutl2.c for M_set_to_zero */

static void my_m_apm_gcd (M_APM rr, M_APM x, M_APM y) {
  M_APM a, b, t, q, r;
  if (!m_apm_is_integer(x) || !m_apm_is_integer(y)) {
    M_set_to_one(rr);
    return;
  }
  a = m_apm_init();
  b = m_apm_init();
  t = m_apm_init();
  q = m_apm_init();
  r = m_apm_init();
  m_apm_absolute_value(a, x);   /* take the abs and do not let the input be changed in-place */
  m_apm_absolute_value(b, y);   /* ditto */
  while (!M_iszero(b)) {  /* b <> 0 ? */
    m_apm_copy(t, b);
    m_apm_integer_div_rem(q, r, a, b);
    m_apm_copy(b, r);
    m_apm_copy(a, t);
  }
  m_apm_copy(rr, a);
  m_apm_free(a);
  m_apm_free(b);
  m_apm_free(t);
  m_apm_free(q);
  m_apm_free(r);
}

static int mapm_xgcd (lua_State *L) {  /* 6.3.6 */
  M_APM x, y, r;
  x = Bget(L, 1);
  y = Bget(L, 2);
  r = Bnew(L);
  my_m_apm_gcd(r, x, y);
  return 1;
}

static int mapm_xlcm (lua_State *L) {  /* 6.3.6 */
  M_APM x, y, r;
  x = Bget(L, 1);
  y = Bget(L, 2);
  r = Bnew(L);
  m_apm_lcm(r, x, y);
  return 1;
}


/* Performs modular multiplication: a*b % n = ((a % n)*(b % n)) % n */

static int m_apm_mulmod (M_APM r, M_APM a, M_APM b, M_APM m) {
  int rc = 0;
  if (M_iszero(m)) {
    /* M_apm_log_error_msg(M_APM_RETURN, (char *)"\'mapm.xmulmod\', modulus is zero"); */
    M_set_to_zero(r);
    rc = 1;
  } else {
    M_APM t0, t1, t2, M;
    t0 = m_apm_init();
    t1 = m_apm_init();
    t2 = m_apm_init();
    M = m_apm_init();
    m_apm_absolute_value(M, m);
    m_apm_integer_div_rem(t0, t1, a, M);
    m_apm_integer_div_rem(t0, t2, b, M);
    m_apm_multiply(t0, t1, t2);
    m_apm_integer_div_rem(t1, r, t0, M);
    if (!M_iszero(r) && M_isneg(m)) {
      m_apm_subtract(t0, r, M);
      m_apm_copy(r, t0);
    }
    if (!M_iszero(r) && (M_sign(a)*M_sign(b) == -1)) {
      m_apm_add(t0, r, M);
      m_apm_copy(r, t0);
    }
    m_apm_free(M);
    m_apm_free(t0);
    m_apm_free(t1);
    m_apm_free(t2);
  }
  return rc;
}

static int mapm_xmulmod (lua_State *L) {  /* 6.3.6 */
  M_APM a, b, m, r;
  a = Bget(L, 1);
  b = Bget(L, 2);
  m = Bget(L, 3);
  r = Bnew(L);
  if (m_apm_mulmod(r, a, b, m)) {  /* 6.6.3 */
    lua_pop(L, 1);
    lua_pushundefined(L);
  }
  return 1;
}


static void m_apm_invmod (M_APM rr, M_APM a, M_APM b) {
  M_APM A, B, t, t0, t1, nt, r, nr, q;
  if (M_iszero(b)) {
    m_apm_set_double(rr, -1.0);
    return;
  }
  A = m_apm_init();
  B = m_apm_init();
  t0 = m_apm_init();
  t1 = m_apm_init();
  m_apm_absolute_value(A, a);
  m_apm_absolute_value(B, b);
  if (M_isneg(a)) {  /* if (a < 0) a = b - (-a % b); */
    m_apm_integer_div_rem(t0, t1, A, B);
    m_apm_subtract(t0, B, t1);
    m_apm_copy(A, t0);
  }
  t = m_apm_init();
  M_set_to_zero(t);  /* t = 0 */
  nt = m_apm_init();
  M_set_to_one(nt);  /* nt = 1 */
  r = m_apm_init();
  m_apm_copy(r, B);  /* r = b */
  nr = m_apm_init();
  q = m_apm_init();
  m_apm_integer_div_rem(q, nr, A, B);  /* nr = a % b */
  while (!M_iszero(nr)) {
    m_apm_integer_div_rem(q, t0, r, nr);  /* q = r/nr */
    m_apm_copy(t0, nt);  /* t0 = nt */
    m_apm_multiply(t1, q, nt);
    m_apm_subtract(nt, t, t1);  /* nt = t - q*nt */
    m_apm_copy(t, t0);  /* t = t0 */
    m_apm_copy(t0, nr);  /* t0 = nr */
    m_apm_multiply(t1, q, nr);
    m_apm_subtract(nr, r, t1);  /* nr = r - q*nr */
    m_apm_copy(r, t0);  /* r = t0 */
  }
  if (m_apm_compare(r, MM_One) > 0) {  /* if (r > 1) return -1 -> no inverse */
    m_apm_set_double(t, -1.0);
  } else if (M_isneg(t)) {  /* if (t < 0) t += b */
    m_apm_add(t0, t, B);
    m_apm_copy(t, t0);
  }
  m_apm_copy(rr, t);
  m_apm_free(t);
  m_apm_free(nt);
  m_apm_free(r);
  m_apm_free(nr);
  m_apm_free(q);
  m_apm_free(A);
  m_apm_free(B);
  m_apm_free(t0);
  m_apm_free(t1);
}

static int mapm_xinvmod (lua_State *L) {  /* 6.3.6 */
  M_APM a, b, r;
  a = Bget(L, 1);
  b = Bget(L, 2);
  r = Bnew(L);
  m_apm_invmod(r, a, b);
  return 1;
}


static int m_apm_powmod (M_APM r, M_APM a, M_APM b, M_APM m) {
  int rc = 0;
  if (M_iszero(m) || (M_iszero(a) && !M_ispos(b))) {
    rc = 1;
    /* M_apm_log_error_msg(M_APM_RETURN, (char *)"\'mapm.xpowmod\', modulus is zero or operation undefined"); */
    M_set_to_zero(r);
  } else if (M_isone(m)) {
    M_set_to_zero(r);
  } else if (M_iszero(b)) {
    M_set_to_one(r);
  } else if (M_isneg(b)) {
    /* double r = tools_intpow(x, -b); */
    M_APM t = m_apm_init();
    m_apm_negate(t, b);
    m_apm_pow_adaptive(r, MYDIGITS, a, t);
    if (M_iszero(r)) {
      m_apm_set_double(r, -1.0);
    } else {
      m_apm_invmod(t, r, m);
      m_apm_copy(r, t);
    }
    m_apm_free(t);
  } else {
    int flag;
    M_APM t = m_apm_init();
    m_apm_pow_adaptive(t, MYDIGITS, a, b);
    if ( (flag = m_apm_compare(t, m) < 0) ) {
      m_apm_copy(r, t);
    } else {
      M_APM q = m_apm_init();
      m_apm_integer_div_rem(q, r, t, m);
      m_apm_free(q);
    }
    if (!flag && M_isneg(r)) {
      M_APM p = m_apm_init();
      M_APM q = m_apm_init();
      M_APM R = m_apm_init();
      M_APM M = m_apm_init();
      m_apm_absolute_value(R, r);
      m_apm_absolute_value(M, m);
      m_apm_integer_div_rem(q, t, R, M);
      m_apm_multiply(p, q, M);
      m_apm_add(t, r, p);  /* we are still negative */
      if (M_isneg(t) && M_ispos(m))
        m_apm_add(r, t, M);  /* now we are not */
      else
        m_apm_copy(r, t);
      m_apm_free(p);
      m_apm_free(q);
      m_apm_free(R);
      m_apm_free(M);
    }
    m_apm_free(t);
  }
  return rc;
}

static int mapm_xpowmod (lua_State *L) {  /* 6.3.6 */
  M_APM a, b, m, r;
  a = Bget(L, 1);
  b = Bget(L, 2);
  m = Bget(L, 3);
  r = Bnew(L);
  if (m_apm_powmod(r, a, b, m)) {  /* 6.6.3 */
    lua_pop(L, 1);
    lua_pushundefined(L);
  }
  return 1;
}

/* From mapm_4.9.5/DOCS/struct.ref:

   'm_apm_datalength' : The number of base 10 digits in the number. In other
			words, the number 5678 will have a datalength of 4 (which will fit into 2 bytes).
			The number 1234567 will have a datalength of 7 (which requires 4 bytes to store).

   'm_apm_exponent'   : The exponent of the number, can be up to sizeof(int)

   'm_apm_sign'       : The sign of the number. sign = -1 is a negative number.
      sign = +1 is a positive number. sign = 0 is a number that is exactly 0.
      This feature is used extensively in the library for fast comparisons to 0. */
static int mapm_xattrib (lua_State *L) {  /* 6.3.6, extended 6.5.7 */
  int i, len;
  M_APM a = Bget(L, 1);
  lua_createtable(L, 0, 5);
  lua_rawsetstringinteger(L, -1, "m_apm_datalength", a->m_apm_datalength);
  len = (a->m_apm_datalength + 1) >> 1;
  lua_rawsetstringinteger(L, -1, "m_apm_datalength_aligned", len);
  lua_rawsetstringinteger(L, -1, "m_apm_exponent",   a->m_apm_exponent);
  lua_rawsetstringinteger(L, -1, "m_apm_sign",       a->m_apm_sign);
  lua_pushliteral(L, "m_apm_data");
  lua_createtable(L, len, 0);
  for (i=0; i < len; i++) {
    lua_rawsetiinteger(L, -1, i + 1, a->m_apm_data[i]);
  }
  lua_settable(L, -3);
  return 1;
}


static int mapm_xmin (lua_State *L) {  /* 6.5.6 */
  M_APM a, b;
  a = Bget(L, 1);
  b = Bget(L, 2);
  if (m_apm_compare(a, b) < 0) {
    lua_settop(L, 1);  /* return original a */
  }  /* else return original b */
  return 1;
}


static int mapm_xmax (lua_State *L) {  /* 6.5.6 */
  M_APM a, b;
  a = Bget(L, 1);
  b = Bget(L, 2);
  if (m_apm_compare(a, b) > 0) {
    lua_settop(L, 1);  /* return original a */
  }  /* else return original b */
  return 1;
}


/*** COMPLEX compartment *************************************************************************************/

typedef struct {
  M_APM real;
  M_APM imag;
} M_CAPM;


/* auxiliary functions **********************************************************************/

static FORCE_INLINE void m_apm_copysign (M_APM rr, M_APM a, M_APM b) {
  int acomp = m_apm_compare(a, MM_Zero);
  int bcomp = m_apm_compare(b, MM_Zero);
  if ((acomp > 0 && bcomp < 0) || (acomp < 0 && bcomp > 0))
    m_apm_negate(rr, a);
  else
    m_apm_copy(rr, a);
}


static void m_apm_csgn (M_APM rr, M_APM a, M_APM b) {
  int acomp = m_apm_compare(a, MM_Zero);
  int bcomp = m_apm_compare(b, MM_Zero);
  if (acomp > 0 || (acomp == 0 && bcomp > 0))
    m_apm_set_double(rr, 1.0);
  else if (acomp < 0 || (acomp == 0 && bcomp < 0))
    m_apm_set_double(rr, -1.0);
  else
    m_apm_set_double(rr, 0.0);
}

/********************************************************************************************/


static M_CAPM *Cnew (lua_State *L) {
  M_CAPM *x = lua_newuserdata(L, sizeof(M_CAPM) + 2*sizeof(M_APM));
  x->real = m_apm_init();
  x->imag = m_apm_init();
  lua_setmetatabletoobject(L, -1, MAPMCTYPE, 1);  /* 3.5.1 change */
  return x;
}


#define Cgetrealimag(L,idx,re,im) { \
  M_CAPM *_x = (M_CAPM *)luaL_checkudata(L, idx, MAPMCTYPE); \
  (re) = _x->real; \
  (im) = _x->imag; \
}

#define Cgetreal(L,idx,re) { \
  M_CAPM *_x = (M_CAPM *)luaL_checkudata(L, idx, MAPMCTYPE); \
  (re) = _x->real; \
}

#define Cgetimag(L,idx,im) { \
  M_CAPM *_x = (M_CAPM *)luaL_checkudata(L, idx, MAPMCTYPE); \
  (im) = _x->imag; \
}

#define setcomponent(L,x,idx,digits) { \
  switch (lua_type(L, idx)) { \
    case LUA_TNUMBER: { \
      m_apm_set_double14((x), lua_tonumber(L, idx), (digits == -1) ? 14 : digits); \
      break; \
    } \
    case LUA_TSTRING: { \
      if (digits != -1) { \
        luaL_error(L, "Error: " LUA_QS " option works with (complex) numbers only.", "digits"); \
      } \
      m_apm_set_string((x), (char*)lua_tostring(L, idx)); \
      break; \
    } \
    default: { \
      if (digits != -1) { \
        luaL_error(L, "Error: " LUA_QS " option works with (complex) numbers only.", "digits"); \
      } \
      if (luaL_isudata(L, idx, MAPMXTYPE)) { \
        m_apm_copy((x), *((void**)lua_touserdata(L, idx))); \
      } else { \
        luaL_argerror(L, idx, "wrong type of argument"); \
      } \
    } \
  } \
}


static int aux_approx (M_APM a, M_APM b, M_APM eps) {
  M_APM d, dist;
  int rc;
  d = m_apm_init();
  dist = m_apm_init();
  m_apm_subtract(d, a, b);
  m_apm_absolute_value(dist, d);
  if (m_apm_compare(dist, eps) < 0) {  /* dist < eps ? */
    rc = 1;
  } else {  /* dist <= (eps * fMax(|a|, |b|)) */
    M_APM x, y, p;
    x = m_apm_init();
    y = m_apm_init();
    p = m_apm_init();
    m_apm_absolute_value(x, a);
    m_apm_absolute_value(y, b);
    m_apm_multiply(p, (m_apm_compare(x, y) > 0) ? x : y, eps);
    rc = m_apm_compare(dist, p) <= 0;
    mfreeall(x, y, p);
  }
  mfreeall(d, dist);
  return rc;
}

static int mapm_approx (lua_State *L) {  /* 6.5.6 */
  M_APM a, b, eps;
  if (lua_gettop(L) == 2) {
    M_APM t;
    /* first increase stack top, then replace, otherwise it won't work without calling the function once again */
    lua_settop(L, 3);
    t = Bnew(L);
    m_apm_set_double(t, agn_getepsilon(L));
    lua_replace(L, 3);
  }
  eps = Bget(L, 3);
  if (ismapm(L, 1)) {
    a = Bget(L, 1);
    b = Bget(L, 2);
    lua_pushboolean(L, aux_approx(a, b, eps));
  } else if (iscmapm(L, 1)) {
    M_APM c, d;
    Cgetrealimag(L, 1, a, b);
    Cgetrealimag(L, 2, c, d);
    lua_pushboolean(L, aux_approx(a, c, eps) && aux_approx(b, d, eps));
  } else {
    luaL_error(L, "Error in " LUA_QS ": expected mapm input.", "mapm.approx");
  }
  return 1;
}

static int mt_aeq (lua_State *L) {  /* 7.5.2 */
  M_APM a, b, eps;
  eps  = M_get_stack_var();
  m_apm_set_double(eps, agn_getepsilon(L));
  if (ismapm(L, 1)) {
    a = Bget(L, 1);
    b = Bget(L, 2);
    lua_pushboolean(L, aux_approx(a, b, eps));
  } else if (iscmapm(L, 1)) {
    M_APM c, d;
    Cgetrealimag(L, 1, a, b);
    Cgetrealimag(L, 2, c, d);
    lua_pushboolean(L, aux_approx(a, c, eps) && aux_approx(b, d, eps));
  } else {
    M_restore_stack(1);
    luaL_error(L, "Error in " LUA_QS ": expected mapm input.", "mapm aeq mt");
  }
  M_restore_stack(1);
  return 1;
}


static int Cnumber (lua_State *L) {  /* mapm.cnumber(x, y) */
  LL = L;
  int digits, nargs = lua_gettop(L);
  aux_newoptions(L, 3, &nargs, &digits, "mapm.cnumber");
  M_CAPM *x = Cnew(L);  /* pushes new M_CAPM userdata onto the stack top */
  setcomponent(L, x->real, 1, digits);
  setcomponent(L, x->imag, 2, digits);
  return 1;
}


/* mode = 1: one string, mode = 2 two strings (real and imaginary parts) */
static int Ctostring (lua_State *L, int idx, int mode) {  /* tostring(x,[n,exp]) */
  char *s;
  int n, bool;
  M_APM re, im;
  Cgetrealimag(L, idx, re, im);  /* 3.5.1 fix */
  if (idx < 0) {
    n = DIGITS; bool = 0;
  } else {
    n = agnL_optinteger(L, idx + 1, DIGITS);
    bool = lua_toboolean(L, idx + 2);
  }
  luaL_checkstack(L, mode == 1 ? 4 : 2, "not enough stack space");  /* 3.5.3 fix */
  /* real part */
  if (bool) {
    int m = (n < 0) ? m_apm_significant_digits(re) : n;
    s = malloc((m + 16)*sizeof(char));
    if (s != NULL) {
      m_apm_to_string(s, n, re);
    } else {  /* 4.11.5 fix */
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "mapm tostring mt");
    }
  } else
      s = m_apm_to_fixpt_stringexp(n, re, '.', 0, 0);
  lua_pushstring(L, s);
  xfree(s);
  /* imaginary part */
  if (mode == 1) lua_pushstring(L, (m_apm_compare(im, MM_Zero) < 0) ? "" : "+");  /* 3.5.2 fix for negative imags */
  if (bool) {
    int m = (n < 0) ? m_apm_significant_digits(im) : n;
    s = malloc((m + 16)*sizeof(char));
    if (s != NULL) {
      m_apm_to_string(s, n, im);
    } else {
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "mapm tostring mt");
    }
  } else
    s = m_apm_to_fixpt_stringexp(n, im, '.', 0, 0);
  if (mode == 1) {
    lua_pushstring(L, s);
    lua_pushstring(L, "*I");
    lua_concat(L, 4);
  } else
    lua_pushstring(L, s);
  xfree(s);
  return mode;
}


static int mapm_ctostring (lua_State *L) {
  int mode = (lua_gettop(L) == 1) ? 1 : 2;
  Ctostring(L, 1, mode);
  return mode;
}


static FORCE_INLINE void Ctorealnumbers (lua_State *L, int idx, lua_Number *real, lua_Number *imag) {
  char *s;
  int overflow;
  M_APM re, im;
  Cgetrealimag(L, idx, re, im);  /* 3.5.1 fix */
  /* real part */
  s = m_apm_to_fixpt_stringexp(IEEEPRECISION, re, '.', 0, 0);
  *real = luaL_str2d(L, s, &overflow);
  xfree(s);
  /* imaginary part */
  s = m_apm_to_fixpt_stringexp(IEEEPRECISION, im, '.', 0, 0);
  *imag = luaL_str2d(L, s, &overflow);
  xfree(s);
}


static int Ctonumber (lua_State *L) {  /* ctonumber(x) */
  lua_Number re, im;
  Ctorealnumbers(L, 1, &re, &im);
  luaL_checkstack(L, 2, "not enough stack space");  /* 6.3.9 fix */
  lua_pushnumber(L, re);
  lua_pushnumber(L, im);
  return 2;
}


static int Ctocomplex (lua_State *L) {  /* mapm.ctocomplex(x) */
  lua_Number re, im;
  Ctorealnumbers(L, 1, &re, &im);
  agn_pushcomplex(L, re, im);
  return 1;
}


static int mt_ctostring (lua_State *L) {
  Ctostring(L, 1, 1);
  return 1;
}


static int mt_creal (lua_State *L) {  /* real(x) */
  M_APM a, r;
  Cgetreal(L, 1, a);
  r = Bnew(L);
  /* we duplicate explicitly for the return not to be GC'd when the original value is being GC'd */
  m_apm_copy(r, a);
  return 1;
}


static int mt_cimag (lua_State *L) {  /* imag(x) */
  M_APM b, r;
  Cgetimag(L, 1, b);
  r = Bnew(L);
  /* we duplicate explicitly for the return not to be GC'd when the original value is being GC'd */
  m_apm_copy(r, b);
  return 1;
}


static int mt_cunm (lua_State *L) {  /* 3.5.2 fix */
  M_APM a, b;
  M_CAPM *z;
  z = Cnew(L);
  Cgetrealimag(L, 1, a, b);
  m_apm_negate(z->real, a);
  m_apm_negate(z->imag, b);
  return 1;
}


static int mt_cadd (lua_State *L) {  /* 3.5.2 fix */
  M_APM a, b, c, d;
  M_CAPM *z;
  z = Cnew(L);
  Cgetrealimag(L, 1, a, b);
  Cgetrealimag(L, 2, c, d);
  m_apm_add(z->real, a, c);
  m_apm_add(z->imag, b, d);
  return 1;
}


static int mt_csub (lua_State *L) {  /* 3.5.2 fix */
  M_APM a, b, c, d;
  M_CAPM *z;
  z = Cnew(L);
  Cgetrealimag(L, 1, a, b);
  Cgetrealimag(L, 2, c, d);
  m_apm_subtract(z->real, a, c);
  m_apm_subtract(z->imag, b, d);
  return 1;
}


static FORCE_INLINE int Cmul (lua_State *L) {
  M_APM a, b, c, d, t0, t1;
  M_CAPM *z = Cnew(L);
  t0 = M_get_stack_var();
  t1 = M_get_stack_var();
  Cgetrealimag(L, 1, a, b);
  Cgetrealimag(L, 2, c, d);
  m_apm_multiply(t0, a, c);
  m_apm_multiply(t1, b, d);
  m_apm_subtract(z->real, t0, t1);
  m_apm_multiply(t0, a, d);
  m_apm_multiply(t1, b, c);
  m_apm_add(z->imag, t0, t1);
  M_restore_stack(2);  /* restore the locals we used here */
  return 1;
}

static int mt_cmul (lua_State *L) {  /* 3.5.2 fix */
  return Cmul(L);
}


static FORCE_INLINE int Cdiv (lua_State *L) {
  M_APM a, b, c, d;
  Cgetrealimag(L, 1, a, b);
  Cgetrealimag(L, 2, c, d);
  if (M_iszero(c) && M_iszero(d)) {
    lua_pushundefined(L);  /* 6.3.9 */
  } else {
    M_APM t2, t3, t5, t6, t7, t8;
    M_CAPM *z = Cnew(L);
    t2 = M_get_stack_var();
    t3 = M_get_stack_var();
    t5 = M_get_stack_var();
    t6 = M_get_stack_var();
    t7 = M_get_stack_var();
    t8 = M_get_stack_var();
    m_apm_multiply(t2, c, c);
    m_apm_multiply(t3, d, d);
    m_apm_add(t6, t2, t3);
    m_apm_divide(t5, MYDIGITS, MM_One, t6);
    m_apm_multiply(t2, a, c);
    m_apm_multiply(t3, b, d);
    m_apm_multiply(t7, t2, t5);
    m_apm_multiply(t8, t3, t5);
    m_apm_add(z->real, t7, t8);
    m_apm_multiply(t2, b, c);
    m_apm_multiply(t3, a, d);
    m_apm_multiply(t7, t2, t5);
    m_apm_multiply(t8, t3, t5);
    m_apm_subtract(z->imag, t7, t8);
    M_restore_stack(6);
  }
  return 1;
}

static int mt_cdiv (lua_State *L) {  /* 3.5.2 fix */
  return Cdiv(L);
}


static FORCE_INLINE int Cpow (lua_State *L) {
  M_APM x0, y0, x, y;
  M_CAPM *z = Cnew(L);
  Cgetrealimag(L, 1, x0, y0);
  Cgetrealimag(L, 2, x, y);
  if (M_iszero(x0) && M_iszero(y0) && m_apm_compare(x, MM_Zero) <= 0) {
    if (M_iszero(y)) {
      lua_pop(L, 1);  /* 6.4.2 change */
      lua_pushundefined(L);
    } else {
      M_set_to_zero(z->real);
      M_set_to_zero(z->imag);
    }
  } else {
    M_APM absa = M_get_stack_var();
    /* absa = sun_hypot(x0, y0); */
    m_apm_hypot(absa, MYDIGITS, x0, y0);
    /* if (absa == 0.0) { */
    if (M_iszero(absa)) {
      M_set_to_zero(z->real);
      M_set_to_zero(z->imag);
      M_restore_stack(1);
    } else {
      M_APM arga, r, r0, theta, t0, si, co;
      arga  = M_get_stack_var();
      r     = M_get_stack_var();
      r0    = M_get_stack_var();
      theta = M_get_stack_var();
      t0    = M_get_stack_var();
      si    = M_get_stack_var();
      co    = M_get_stack_var();
      /* arga = sun_atan2(y0, x0); */
      m_apm_arctan2(arga, MYDIGITS, y0, x0);
      /* r = sun_pow(absa, x, 0); */
      m_apm_pow_adaptive(r, MYDIGITS, absa, x);  /* 6.3.6 improvement */
      /* theta = x*arga; */
      m_apm_multiply(theta, x, arga);
      /* if (y != 0.0) { */
      if (!M_iszero(y)) {
        /* r = r*sun_exp(-y*arga); */
        m_apm_multiply(t0, y, arga);
        m_apm_negate(r0, t0);
        m_apm_exp(t0, MYDIGITS, r0);
        /* r = r*t0; */
        m_apm_multiply(r0, r, t0);
        m_apm_copy(r, r0);
        /* theta = theta + y*sun_log(absa); */
        m_apm_log(t0, MYDIGITS, absa);
        m_apm_multiply(r0, y, t0);
        m_apm_add(t0, theta, r0);
        m_apm_copy(theta, t0);
      }
      /* sun_sincos(theta, &si, &co);  r*co, r*si; */
      m_apm_sin_cos(si, co, MYDIGITS, theta);
      m_apm_multiply(z->real, r, co);
      m_apm_multiply(z->imag, r, si);
      M_restore_stack(8);
    }
  }
  return 1;
}

static int mt_cpow (lua_State *L) {  /* 3.5.2 fix */
  return Cpow(L);
}


static FORCE_INLINE int Cipow (lua_State *L) {
  M_APM a, b, x, y, n, t1, t2, t5, t6, t7, t8, t9;
  M_CAPM *z = Cnew(L);
  t1 = M_get_stack_var();
  t2 = M_get_stack_var();
  t5 = M_get_stack_var();
  t6 = M_get_stack_var();
  t7 = M_get_stack_var();
  t8 = M_get_stack_var();
  t9 = M_get_stack_var();
  x  = M_get_stack_var();
  y  = M_get_stack_var();
  n  = M_get_stack_var();
  Cgetrealimag(L, 1, a, b);
  m_apm_multiply(t1, a, a);
  m_apm_multiply(t2, b, b);
  m_apm_add(x, t1, t2);
  m_apm_set_double(n, agn_checknumber(L, 2));
  m_apm_multiply(y, n, MM_0_5);  /* 6.6.5 change */
  m_apm_pow_adaptive(t5, MYDIGITS, x, y);  /* 6.3.6 improvement */
  m_apm_arctan2(t6, MYDIGITS, b, a);
  m_apm_multiply(t7, t6, n);
  m_apm_sin_cos(t9, t8, MYDIGITS, t7);
  m_apm_multiply(z->real, t5, t8);
  m_apm_multiply(z->imag, t5, t9);
  M_restore_stack(10);
  return 1;
}

static int mt_cipow (lua_State *L) {  /* 3.5.2 fix */
  return Cipow(L);
}


static int mt_crecip (lua_State *L) {
  M_APM c, d;
  Cgetrealimag(L, 1, c, d);
  if (M_iszero(c) && M_iszero(d)) {
    lua_pushundefined(L);  /* 6.3.9 */
  } else {
    M_APM t2, t3, t5, t6;
    M_CAPM *z = Cnew(L);
    t2 = M_get_stack_var();
    t3 = M_get_stack_var();
    t5 = M_get_stack_var();
    t6 = M_get_stack_var();
    m_apm_multiply(t2, c, c);
    m_apm_multiply(t3, d, d);
    m_apm_add(t6, t2, t3);
    m_apm_divide(t5, MYDIGITS, MM_One, t6);
    m_apm_multiply(z->real, c, t5);
    m_apm_multiply(t6, d, t5);
    m_apm_negate(z->imag, t6);
    M_restore_stack(4);
  }
  return 1;
}


static int Cfma (lua_State *L) {
  M_APM a, b, c, d, e, f, t0, t1, t2;
  M_CAPM *z = Cnew(L);
  Cgetrealimag(L, 1, a, b);
  Cgetrealimag(L, 2, c, d);
  Cgetrealimag(L, 3, e, f);
  t0 = M_get_stack_var();
  t1 = M_get_stack_var();
  t2 = M_get_stack_var();
  /* real: a*c - b*d + e */
  m_apm_multiply(t0, a, c);
  m_apm_multiply(t1, b, d);
  m_apm_subtract(t2, t0, t1);
  m_apm_add(z->real, t2, e);
  /* imag: a*d + b*c + f */
  m_apm_multiply(t0, a, d);
  m_apm_multiply(t1, b, c);
  m_apm_add(t2, t0, t1);
  m_apm_add(z->imag, t2, f);
  M_restore_stack(3);
  return 1;
}


static int mt_cabs (lua_State *L) {
  M_APM a, b, r;
  r = Bnew(L);
  Cgetrealimag(L, 1, a, b);
  m_apm_hypot(r, DIGITS, a, b);
  return 1;
}


static int Cargument (lua_State *L) {
  M_APM a, b, r;
  r = Bnew(L);
  Cgetrealimag(L, 1, a, b);
  m_apm_atan2(r, DIGITS, b, a);
  return 1;
}


static int mt_csgn (lua_State *L) {
  M_APM a, b, r;
  r = Bnew(L);
  Cgetrealimag(L, 1, a, b);
  m_apm_csgn(r, a, b);
  return 1;
}


static int mt_csquare (lua_State *L) {
  M_APM a, b, t0, t1;
  M_CAPM *z;
  z = Cnew(L);
  t0 = M_get_stack_var();
  t1 = M_get_stack_var();
  Cgetrealimag(L, 1, a, b);
  m_apm_multiply(t0, a, a);
  m_apm_multiply(t1, b, b);
  m_apm_subtract(z->real, t0, t1);
  m_apm_multiply(t0, a, b);
  m_apm_multiply(t1, b, a);
  m_apm_add(z->imag, t0, t1);
  M_restore_stack(2);  /* restore the locals we used here */
  return 1;
}


static int mt_ccube (lua_State *L) {
  M_APM a, b, t0, t1, x, y;
  M_CAPM *z;
  z = Cnew(L);
  t0 = M_get_stack_var();
  t1 = M_get_stack_var();
  x = M_get_stack_var();
  y = M_get_stack_var();
  Cgetrealimag(L, 1, a, b);
  m_apm_multiply(t0, a, a);
  m_apm_multiply(t1, b, b);
  m_apm_subtract(x, t0, t1);
  m_apm_multiply(t0, a, b);
  m_apm_multiply(t1, b, a);
  m_apm_add(y, t0, t1);
  m_apm_multiply(t0, x, a);
  m_apm_multiply(t1, y, b);
  m_apm_subtract(z->real, t0, t1);
  m_apm_multiply(t0, x, b);
  m_apm_multiply(t1, y, a);
  m_apm_add(z->imag, t0, t1);
  M_restore_stack(4);  /* restore the locals we used here */
  return 1;
}


static int mt_cln (lua_State *L) {
  M_APM a, b;
  Cgetrealimag(L, 1, a, b);
  if (M_iszero(a) && M_iszero(b)) {
    lua_pushundefined(L);  /* 6.6.3 change */
  } else {
    M_APM t, t0, t1;
    M_CAPM *z = Cnew(L);
    t0 = M_get_stack_var();
    t1 = M_get_stack_var();
    t  = M_get_stack_var();
    m_apm_multiply(t0, a, a);
    m_apm_multiply(t1, b, b);
    m_apm_add(t, t0, t1);
    m_apm_log(t, MYDIGITS, t);
    m_apm_multiply(z->real, t, MM_0_5);  /* 6.6.5 change */
    m_apm_arctan2(z->imag, MYDIGITS, b, a);
    M_restore_stack(3);
  }
  return 1;
}


static int mt_cexp (lua_State *L) {
  M_APM a, b, t, si, co;
  M_CAPM *z;
  z = Cnew(L);
  Cgetrealimag(L, 1, a, b);
  si = M_get_stack_var();
  co = M_get_stack_var();
  t  = M_get_stack_var();
  m_apm_exp(t, MYDIGITS, a);
  m_apm_sin_cos(si, co, MYDIGITS, b);
  m_apm_multiply(z->real, t, co);
  m_apm_multiply(z->imag, t, si);
  M_restore_stack(3);
  return 1;
}


static int mt_csqrt (lua_State *L) {
  M_APM a, b;
  M_CAPM *z;
  z = Cnew(L);
  Cgetrealimag(L, 1, a, b);
  if (M_iszero(b)) {
    if (m_apm_compare(a, MM_Zero) < 0) {
      M_APM ta;
      M_set_to_zero(z->real);
      ta = M_get_stack_var();
      m_apm_negate(ta, a);
      m_apm_sqrt(z->imag, MYDIGITS, ta);
      M_restore_stack(1);  /* 6.5.15 fix */
    } else {
      m_apm_sqrt(z->real, MYDIGITS, a);
      M_set_to_zero(z->imag);
    }
  } else {
    M_APM ta, t1, t2, t4, t6, t10, t, x, y;
    ta  = M_get_stack_var();
    t1  = M_get_stack_var();
    t2  = M_get_stack_var();
    t4  = M_get_stack_var();
    t6  = M_get_stack_var();
    t10 = M_get_stack_var();
    t   = M_get_stack_var();
    x   = M_get_stack_var();
    y   = M_get_stack_var();
    m_apm_multiply(t1, a, a);
    m_apm_multiply(t2, b, b);
    m_apm_add(t, t1, t2);
    m_apm_sqrt(t4, MYDIGITS, t);
    m_apm_multiply(x, t4, MM_Two);
    m_apm_multiply(y, a, MM_Two);
    m_apm_add(t, x, y);
    m_apm_sqrt(t6, MYDIGITS, t);
    m_apm_subtract(t, x, y);
    m_apm_sqrt(t10, MYDIGITS, t);
    /* tools_csgn(b, -a)*t10/2; */
    m_apm_negate(ta, a);
    m_apm_csgn(t, b, ta);
    m_apm_multiply(t1, t, t10);
    m_apm_multiply(z->real, t6, MM_0_5);  /* 6.6.5 change */
    m_apm_multiply(z->imag, t1, MM_0_5);  /* 6.6.5 change */
    M_restore_stack(9);  /* restore the locals we used here */
  }
  return 1;
}


static int mt_csin (lua_State *L) {
  M_APM a, b, si, co, sih, coh;
  M_CAPM *z;
  z = Cnew(L);
  Cgetrealimag(L, 1, a, b);
  si  = M_get_stack_var();
  co  = M_get_stack_var();
  sih = M_get_stack_var();
  coh = M_get_stack_var();
  m_apm_sin_cos(si, co, MYDIGITS, a);
  m_apm_sinh(sih, MYDIGITS, b);
  m_apm_cosh(coh, MYDIGITS, b);
  m_apm_multiply(z->real, si, coh);
  m_apm_multiply(z->imag, co, sih);
  M_restore_stack(4);
  return 1;
}


static int mt_ccos (lua_State *L) {
  M_APM a, b, sir, si, co, sih, coh;
  M_CAPM *z;
  z = Cnew(L);
  Cgetrealimag(L, 1, a, b);
  si  = M_get_stack_var();
  co  = M_get_stack_var();
  sih = M_get_stack_var();
  coh = M_get_stack_var();
  sir = M_get_stack_var();
  m_apm_sin_cos(si, co, MYDIGITS, a);
  m_apm_sinh(sih, MYDIGITS, b);
  m_apm_cosh(coh, MYDIGITS, b);
  m_apm_multiply(z->real, co, coh);
  m_apm_negate(sir, si);
  m_apm_multiply(z->imag, sir, sih);
  M_restore_stack(5);
  return 1;
}


static int mt_ctan (lua_State *L) {
  M_APM a, b, re, im, si, co, sih, coh, den;
  Cgetrealimag(L, 1, a, b);
  re  = M_get_stack_var();
  im  = M_get_stack_var();
  si  = M_get_stack_var();
  co  = M_get_stack_var();
  sih = M_get_stack_var();
  coh = M_get_stack_var();
  den = M_get_stack_var();
  m_apm_multiply(re, a, MM_Two);
  m_apm_multiply(im, b, MM_Two);
  m_apm_sin_cos(si, co, MYDIGITS, re);
  m_apm_sinh(sih, MYDIGITS, im);
  m_apm_cosh(coh, MYDIGITS, im);
  m_apm_add(den, co, coh);
  if (M_iszero(den)) {
    /* M_apm_log_error_msg(M_APM_RETURN, (char *)"\'tan\', argument is out-of-range"); */ /* 3.5.2 fix */
    lua_pushundefined(L);
  } else {
    M_CAPM *z = Cnew(L);
    m_apm_divide(z->real, MYDIGITS, si, den);
    m_apm_divide(z->imag, MYDIGITS, sih, den);
  }
  M_restore_stack(7);
  return 1;
}


static int mt_csinh (lua_State *L) {
  M_APM a, b, si, co, sih, coh;
  M_CAPM *z;
  z = Cnew(L);
  Cgetrealimag(L, 1, a, b);
  si  = M_get_stack_var();
  co  = M_get_stack_var();
  sih = M_get_stack_var();
  coh = M_get_stack_var();
  m_apm_sin_cos(si, co, MYDIGITS, b);
  m_apm_sinh(sih, MYDIGITS, a);
  m_apm_cosh(coh, MYDIGITS, a);
  m_apm_multiply(z->real, sih, co);
  m_apm_multiply(z->imag, coh, si);
  M_restore_stack(4);
  return 1;
}


static int mt_ccosh (lua_State *L) {
  M_APM a, b, si, co, sih, coh;
  M_CAPM *z;
  z = Cnew(L);
  Cgetrealimag(L, 1, a, b);
  si  = M_get_stack_var();
  co  = M_get_stack_var();
  sih = M_get_stack_var();
  coh = M_get_stack_var();
  m_apm_sin_cos(si, co, MYDIGITS, b);
  m_apm_sinh(sih, MYDIGITS, a);
  m_apm_cosh(coh, MYDIGITS, a);
  m_apm_multiply(z->real, coh, co);
  m_apm_multiply(z->imag, sih, si);
  M_restore_stack(4);
  return 1;
}


static int mt_ctanh (lua_State *L) {
  M_APM a, b, si, co, sih, coh, t4, t6, t8, t9;
  M_CAPM *z;
  z = Cnew(L);
  Cgetrealimag(L, 1, a, b);
  si  = M_get_stack_var();
  co  = M_get_stack_var();
  sih = M_get_stack_var();
  coh = M_get_stack_var();
  t4  = M_get_stack_var();
  t6  = M_get_stack_var();
  t8  = M_get_stack_var();
  t9  = M_get_stack_var();
  m_apm_sin_cos(si, co, MYDIGITS, b);
  m_apm_sinh(sih, MYDIGITS, a);
  m_apm_cosh(coh, MYDIGITS, a);
  m_apm_multiply(t4, sih, sih);
  m_apm_multiply(t6, co, co);
  m_apm_add(t8, t4, t6);
  m_apm_divide(t9, MYDIGITS, MM_One, t8);
  m_apm_multiply(t4, sih, coh);
  m_apm_multiply(t6, si, co);
  m_apm_multiply(z->real, t9, t4);
  m_apm_multiply(z->imag, t9, t6);
  M_restore_stack(8);
  return 1;
}


/* -I*arcsin(I*z) */
static void m_apm_carcsinh (M_APM rx, M_APM ry, M_APM a, M_APM b) {
  if (M_iszero(b)) {
    m_apm_asinh(rx, MYDIGITS, a);
    M_set_to_zero(ry);
  } else {
    M_APM b2, t3, t4, t5, t6, t8, t10, t12, z0, z1, z2;
    t3  = M_get_stack_var();
    t4  = M_get_stack_var();
    t5  = M_get_stack_var();
    t6  = M_get_stack_var();
    t8  = M_get_stack_var();
    t10 = M_get_stack_var();
    t12 = M_get_stack_var();
    z0  = M_get_stack_var();
    z1  = M_get_stack_var();
    z2  = M_get_stack_var();
    b2  = M_get_stack_var();
    m_apm_multiply(t3, a, a);
    m_apm_multiply(t4, b, b);
    /* z1 = t3 + t4 + 1.0; */
    m_apm_add(z0, t3, t4);
    m_apm_add(z1, MM_One, z0);
    /* t6 = sqrt(z1 + 2.0*b); */
    m_apm_multiply(b2, b, MM_Two);
    m_apm_add(t3, z1, b2);
    m_apm_sqrt(t6, MYDIGITS, t3);
    /* t8 = sqrt(z1 - 2.0*b); */
    m_apm_subtract(z0, z1, b2);
    m_apm_sqrt(t8, MYDIGITS, z0);
    /* z2 = 0.5*t6 + 0.5*t8; */
    m_apm_multiply(z0, t6, MM_0_5);  /* 6.6.5 change */
    m_apm_multiply(z1, t8, MM_0_5);  /* 6.6.5 change */
    m_apm_add(z2, z0, z1);
    /* t10 = tools_square(z2); */
    m_apm_multiply(t10, z2, z2);
    /* t12 = sqrt(t10 - 1.0); */
    m_apm_subtract(t3, t10, MM_One);
    m_apm_sqrt(t12, MYDIGITS, t3);
    /* rx = tools_csgn(a, b)*sun_log(z2 + t12); */
    m_apm_csgn(t3, a, b);
    m_apm_add(t4, z2, t12);
    m_apm_log(t5, MYDIGITS, t4);
    m_apm_multiply(t10, t3, t5);  /* rx */
    /* z2 = 0.5*t6 - 0.5*t8; */
    m_apm_subtract(z2, z0, z1);
    /* tools_adjust(t18, 1, AGN_EPSILON, -1); */
    /* ry = sun_asin(z2); */
    m_apm_asin(t12, MYDIGITS, z2);  /* ry */
    /* z->real = copysign(rx, a); */
    m_apm_copysign(rx, t10, a);
    /* z->imag = copysign(ry, b); */
    m_apm_copysign(ry, t12, b);
    M_restore_stack(11);
  }
}

static int Carcsinh (lua_State *L) {
  M_APM a, b;
  M_CAPM *z = Cnew(L);
  Cgetrealimag(L, 1, a, b);
  m_apm_carcsinh(z->real, z->imag, a, b);
  return 1;
}


static int Carccosh (lua_State *L) {
  M_APM a, b;
  M_CAPM *z = Cnew(L);
  Cgetrealimag(L, 1, a, b);
  if (M_iszero(b) && m_apm_compare(a, MM_One) >= 0) {
    m_apm_acosh(z->real, MYDIGITS, a);
    M_set_to_zero(z->imag);
  } else {
    M_APM u, v, t2, t6, t7, t8, t9, t11, t13, t15, t9h, t11h, t16;
    t2  = M_get_stack_var();
    t6  = M_get_stack_var();
    t7  = M_get_stack_var();
    t8  = M_get_stack_var();
    t9  = M_get_stack_var();
    t11 = M_get_stack_var();
    t13 = M_get_stack_var();
    t15 = M_get_stack_var();
    t9h = M_get_stack_var();
    t11h = M_get_stack_var();
    t16 = M_get_stack_var();
    u   = M_get_stack_var();
    v   = M_get_stack_var();
    /* t2 = tools_csgn(b, 1 - a); */
    m_apm_subtract(u, MM_One, a);
    m_apm_csgn(t2, b, u);
    /* t6 = a*a; */
    m_apm_multiply(t6, a, a);
    /* t7 = b*b; */
    m_apm_multiply(t7, b, b);
    /* t8 = t6 + t7 + 1.0; */
    m_apm_add(u, t6, t7);
    m_apm_add(t8, u, MM_One);
    /* t9 = sqrt(2.0*a + t8); */
    m_apm_multiply(u, MM_Two, a);
    m_apm_add(v, u, t8);
    m_apm_sqrt(t9, MYDIGITS, v);
    /* t11 = sqrt(-2.0*a + t8); */
    m_apm_negate(v, u);
    m_apm_add(u, v, t8);
    m_apm_sqrt(t11, MYDIGITS, u);
    /* t13 = sun_pow(0.5*(t9 + t11), 2.0, 1); */
    m_apm_add(u, t9, t11);
    m_apm_multiply(v, u, MM_0_5);  /* 6.6.5 change */
    m_apm_multiply(t13, v, v);
    /* t15 = sqrt(t13 - 1.0); */
    m_apm_subtract(u, t13, MM_One);
    m_apm_sqrt(t15, MYDIGITS, u);
    /* t9h = 0.5*t9; */
    m_apm_multiply(t9h, t9, MM_0_5);  /* 6.6.5 change */
    /* t11h = 0.5*t11; */
    m_apm_multiply(t11h, t11, MM_0_5);  /* 6.6.5 change */
    /* t16 = t9h - t11h; */
    m_apm_subtract(t16, t9h, t11h);
    /* real -t2*tools_csgn(-b, a)*sun_log(t9h + t11h + t15) */
    m_apm_negate(u, b);
    m_apm_csgn(v, u, a);
    m_apm_multiply(t7, t2, v);
    m_apm_negate(t6, t7);  /* t6 = -t2*tools_csgn(-b, a) */
    /* sun_log(t9h + t11h + t15) */
    m_apm_add(u, t9h, t11h);
    m_apm_add(v, u, t15);
    m_apm_log(t7, MYDIGITS, v);
    m_apm_multiply(z->real, t6, t7);
    /* imag: t2*sun_acos(t16)); */
    m_apm_acos(u, MYDIGITS, t16);
    m_apm_multiply(z->imag, t2, u);
    M_restore_stack(13);
  }
  return 1;
}


static int Carctanh (lua_State *L) {
  M_APM a, b, absa;
  M_CAPM *z = Cnew(L);
  Cgetrealimag(L, 1, a, b);
  absa = m_apm_init();
  m_apm_absolute_value(absa, a);
  if (M_iszero(b) && m_apm_compare(absa, MM_One) <= 0) {
    m_apm_atanh(z->real, MYDIGITS, a);
    M_set_to_zero(z->imag);
    m_apm_free(absa);
  } else {
    M_APM t1, t2, t3, t4, t6, u, v, w;
    m_apm_free(absa);
    t1 = M_get_stack_var();
    t2 = M_get_stack_var();
    t3 = M_get_stack_var();
    t4 = M_get_stack_var();
    t6 = M_get_stack_var();
    u  = M_get_stack_var();
    v  = M_get_stack_var();
    w  = M_get_stack_var();
    /* t1 = a + 1.0; */
    m_apm_add(t1, a, MM_One);
    /* t2 = t1*t1; */
    m_apm_multiply(t2, t1, t1);
    /* t3 = b*b; */
    m_apm_multiply(t3, b, b);
    /* t4 = a - 1.0; */
    m_apm_subtract(t4, a, MM_One);
    /* t6 = t4*t4; */
    m_apm_multiply(t6, t4, t4);
    /* im = 0.5*(sun_atan2(b, t1) - sun_atan2(-b, 1.0 - a)); */
    /* u = sun_atan2(b, t1) */
    m_apm_atan2(u, MYDIGITS, b, t1);
    /* v = sun_atan2(-b, 1.0 - a) */
    m_apm_negate(t1, b);
    m_apm_subtract(w, MM_One, a);
    m_apm_atan2(v, MYDIGITS, t1, w);
    m_apm_subtract(w, u, v);
    m_apm_multiply(u, w, MM_0_5);  /* 6.6.5 change */
    if (m_apm_compare(a, MM_Zero) > 0 && M_iszero(b)) {
      m_apm_negate(v, u);
      m_apm_copy(z->imag, v);
    } else
      m_apm_copy(z->imag, u);
    /* 0.25*sun_log((t2 + t3)/(t6 + t3)) */
    m_apm_add(u, t2, t3);
    m_apm_add(v, t6, t3);
    m_apm_divide(w, MYDIGITS, u, v);
    m_apm_log(u, MYDIGITS, w);
    m_apm_divide(z->real, MYDIGITS, u, MM_Four);
    M_restore_stack(8);
  }
  return 1;
}


static int mt_carcsin (lua_State *L) {
  M_APM a, b, x, y;
  M_CAPM *z;
  z = Cnew(L);
  Cgetrealimag(L, 1, a, b);
  x = m_apm_init();
  y = m_apm_init();
  m_apm_carcsinh(x, y, b, a);
  if (M_iszero(b) && m_apm_compare(a, MM_One) >= 0) {
    M_APM t = m_apm_init();
    m_apm_negate(t, x);
    m_apm_copy(x, t);
    m_apm_free(t);
  }
  m_apm_copy(z->real, y);
  m_apm_copy(z->imag, x);
  m_apm_free(x); m_apm_free(y);
  return 1;
}


static int mt_carccos (lua_State *L) {
  M_APM a, b, t, x, y;
  M_CAPM *z;
  z = Cnew(L);
  Cgetrealimag(L, 1, a, b);
  x = m_apm_init();
  y = m_apm_init();
  t = m_apm_init();
  m_apm_carcsinh(x, y, b, a);
  if (!M_iszero(b) || m_apm_compare(a, MM_One) <= 0) {
    m_apm_negate(t, x);
    m_apm_copy(x, t);
  }
  /* y = PIO2 - y; */
  m_apm_subtract(z->real, MM_HALF_PI, y);
  m_apm_copy(z->imag, x);
  m_apm_free(x); m_apm_free(y); m_apm_free(t);
  return 1;
}


static int mt_carctan (lua_State *L) {
  M_APM a, b, t3, t5, t6, t9, u, v, w, m1;
  M_CAPM *z;
  z = Cnew(L);
  Cgetrealimag(L, 1, a, b);
  t3 = M_get_stack_var();
  t5 = M_get_stack_var();
  t6 = M_get_stack_var();
  t9 = M_get_stack_var();
  u  = M_get_stack_var();
  v  = M_get_stack_var();
  w  = M_get_stack_var();
  m1 = M_get_stack_var();
  m_apm_negate(m1, MM_One);
  /* t3 = b + 1.0; */
  m_apm_add(t3, b, MM_One);
  /* t5 = a*a; */
  m_apm_multiply(t5, a, a);
  /* t6 = t3*t3; */
  m_apm_multiply(t6, t3, t3);
  /* t9 = tools_square(b - 1.0); */
  m_apm_subtract(u, b, MM_One);
  m_apm_multiply(t9, u, u);
  /* if (a == 0 || a == -0) { */
  if (M_iszero(a)) {
    /* if ((b == 1 || b == -1)) { */
    m_apm_absolute_value(v, b);
    if (m_apm_compare(v, MM_One) == 0) {  /* undefined */
      /* M_apm_log_error_msg(M_APM_RETURN, (char *)"\'arctan\', |imag| is one"); */
      lua_pop(L, 1);
      lua_pushundefined(L);
      goto freestat;
    /* else if (b < -1) */
    } else if (m_apm_compare(b, m1) < 0) {
      /* r = -PI*0.5; */
      m_apm_negate(z->real, MM_HALF_PI);
      goto lcatan1;
    }
  }
  /* r = sun_atan2(a, 1.0 - b)*0.5 - sun_atan2(-a, t3)*0.5; */
  m_apm_subtract(u, MM_One, b);
  m_apm_atan2(v, MYDIGITS, a, u);
  m_apm_multiply(u, v, MM_0_5);  /* 6.6.5 change */
  m_apm_negate(v, a);
  m_apm_atan2(w, MYDIGITS, v, t3);
  m_apm_multiply(v, w, MM_0_5);  /* 6.6.5 change */
  m_apm_subtract(z->real, u, v);
lcatan1:
  /* i = sun_log((t5 + t6)/(t5 + t9))*0.25; */
  m_apm_add(u, t5, t6);
  m_apm_add(v, t5, t9);
  m_apm_divide(w, MYDIGITS, u, v);
  m_apm_log(u, MYDIGITS, w);
  m_apm_divide(z->imag, MYDIGITS, u, MM_Four);
freestat:
  M_restore_stack(8);
  return 1;
}


static int mt_carcsec (lua_State *L) {
  M_APM a, b, e, t, u, v, x, y;
  M_CAPM *z;
  z = Cnew(L);
  Cgetrealimag(L, 1, a, b);
  x = M_get_stack_var();
  y = M_get_stack_var();
  e = M_get_stack_var();
  t = M_get_stack_var();
  u = M_get_stack_var();
  v = M_get_stack_var();
  /* e = x*x + y*y; */
  m_apm_multiply(t, a, a);
  m_apm_multiply(u, b, b);
  m_apm_add(e, t, u);
  /* t = a/e; u = (-b)/e; */
  if (m_apm_sign(e) == 0) {
    lua_pop(L, 1);
    lua_pushundefined(L);
  } else {
    m_apm_divide(t, MYDIGITS, a, e);
    m_apm_divide(u, MYDIGITS, b, e);
    m_apm_negate(v, u);
    m_apm_copy(u, v);
    /* tools_casinh(u, t, &x, &y); */
    m_apm_carcsinh(x, y, u, t);
    if (!M_iszero(u) || m_apm_compare(t, MM_One) > 0) {
      m_apm_negate(t, x);
      m_apm_copy(x, t);
    }
    /* y = PIO2 - y; */
    m_apm_subtract(z->real, MM_HALF_PI, y);
    m_apm_copy(z->imag, x);
  }
  M_restore_stack(6);
  return 1;
}


static int mt_ceq (lua_State *L) {  /* 3.5.2 fix */
  M_APM a, b, c, d;
  Cgetrealimag(L, 1, a, b);
  Cgetrealimag(L, 2, c, d);
  lua_pushboolean(L, m_apm_compare(a, c) == 0 && m_apm_compare(b, d) == 0);
  return 1;
}


static int mt_czero (lua_State *L) {  /* 3.6.9 */
  M_APM a, b;
  Cgetrealimag(L, 1, a, b);
  lua_pushboolean(L, M_iszero(a) && M_iszero(b));
  return 1;
}


static int mt_cnonzero (lua_State *L) {  /* 3.6.9 */
  M_APM a, b;
  Cgetrealimag(L, 1, a, b);
  lua_pushboolean(L, !(M_iszero(a) && M_iszero(b)));
  return 1;
}


static int mt_cgc (lua_State *L) {  /* this is the better-be-sure-than-sorry GC metamethod */
  if (iscmapm(L, 1)) {
    M_CAPM *a = (M_CAPM *)lua_touserdata(L, 1);
    if (a) {
      lua_setmetatabletoobject(L, 1, NULL, 1);  /* 3.5.1 change */
      /* The source code of m_apm_free() is a bit mysterious, so we will explicitly check
         for non-NULL a->real, a->imag. 6.6.3 */
      if (a->real) { m_apm_free(a->real); a->real = NULL; }
      if (a->imag) { m_apm_free(a->imag); a->imag = NULL; }
      /* 6.6.3: Do not call this now:
      m_apm_trim_mem_usage();
      as this will crash Agena especially on Linux. */
    }
  }
  return 0;
}


static int Csincos (lua_State *L) {  /* 6.6.4 */
  M_APM a, b, si, sir, co, sih, coh;
  M_CAPM *z1, *z2;
  luaL_checkstack(L, 2, "not enough stack space");
  z1 = Cnew(L);
  z2 = Cnew(L);
  Cgetrealimag(L, 1, a, b);
  si  = M_get_stack_var();
  co  = M_get_stack_var();
  sih = M_get_stack_var();
  coh = M_get_stack_var();
  sir = M_get_stack_var();
  m_apm_sin_cos(si, co, MYDIGITS, a);
  m_apm_sinh(sih, MYDIGITS, b);
  m_apm_cosh(coh, MYDIGITS, b);
  m_apm_multiply(z1->real, si, coh);
  m_apm_multiply(z1->imag, co, sih);
  m_apm_multiply(z2->real, co, coh);
  m_apm_negate(sir, si);
  m_apm_multiply(z2->imag, sir, sih);
  M_restore_stack(5);
  return 2;
}


static int Csinhcosh (lua_State *L) {  /* 6.6.4 */
  M_APM a, b, si, co, sih, coh;
  M_CAPM *z1, *z2;
  luaL_checkstack(L, 2, "not enough stack space");
  z1 = Cnew(L);
  z2 = Cnew(L);
  Cgetrealimag(L, 1, a, b);
  si  = M_get_stack_var();
  co  = M_get_stack_var();
  sih = M_get_stack_var();
  coh = M_get_stack_var();
  m_apm_sin_cos(si, co, MYDIGITS, b);
  m_apm_sinh(sih, MYDIGITS, a);
  m_apm_cosh(coh, MYDIGITS, a);
  m_apm_multiply(z1->real, sih, co);
  m_apm_multiply(z1->imag, coh, si);
  m_apm_multiply(z2->real, coh, co);
  m_apm_multiply(z2->imag, sih, si);
  M_restore_stack(4);
  return 2;
}


static int Csinc (lua_State *L) {  /* 3.5.2 */
  M_APM a, b;
  M_CAPM *z = Cnew(L);
  Cgetrealimag(L, 1, a, b);
  /* if ((a) == 0 && (b) == 0) */
  if (M_iszero(a) && M_iszero(b)) {
    m_apm_copy(z->real, MM_One);  /* this is the limit */
    M_set_to_zero(z->imag);
  } else {
    M_APM t3, t4, t5, t7, t8, t12, t13, si, co, sih, coh;
    t3  = M_get_stack_var();
    t4  = M_get_stack_var();
    t5  = M_get_stack_var();
    t7  = M_get_stack_var();
    t8  = M_get_stack_var();
    t12 = M_get_stack_var();
    t13 = M_get_stack_var();
    si  = M_get_stack_var();
    co  = M_get_stack_var();
    sih = M_get_stack_var();
    coh = M_get_stack_var();
    /* sun_sincos(a, &si, &co); */
    m_apm_sin_cos(si, co, MYDIGITS, a);
    /* luai_numsinhcosh(b, &sih, &coh); */
    m_apm_sinh(sih, MYDIGITS, b);
    m_apm_cosh(coh, MYDIGITS, b);
    /* t3 = si*coh; */
    m_apm_multiply(t3, si, coh);
    /* t12 = co*sih; */
    m_apm_multiply(t12, co, sih);
    /* t4 = a*a; */
    m_apm_multiply(t4, a, a);
    /* t5 = b*b; */
    m_apm_multiply(t5, b, b);
    /* t7 = 1/(t4 + t5); */
    m_apm_add(si, t4, t5);
    m_apm_reciprocal(t7, MYDIGITS, si);
    /* t8 = a*t7; */
    m_apm_multiply(t8, a, t7);
    /* t13 = b*t7; */
    m_apm_multiply(t13, b, t7);
    /* im = t12*t8 - t3*t13; */
    m_apm_multiply(si, t12, t8);
    m_apm_multiply(co, t3, t13);
    m_apm_subtract(z->imag, si, co);
    /* re = t3*t8 + t12*t13 */
    m_apm_multiply(si, t3, t8);
    m_apm_multiply(co, t12, t13);
    m_apm_add(z->real, si, co);
    M_restore_stack(11);
  }
  return 1;
}


static int mt_csinc (lua_State *L) {  /* 3.5.2 */
  Csinc(L);
  return 1;
}


static int Ccosc (lua_State *L) {  /* 3.5.2 */
  M_APM a, b;
  M_CAPM *z = Cnew(L);
  Cgetrealimag(L, 1, a, b);
  /* if ((a) == 0 && (b) == 0) */
  if (M_iszero(a) && M_iszero(b)) {
    M_set_to_zero(z->real);
    M_set_to_zero(z->imag);
  } else {
    M_APM t3, t4, t5, t7, t8, t12, t13, si, co, sih, coh;
    t3  = M_get_stack_var();
    t4  = M_get_stack_var();
    t5  = M_get_stack_var();
    t7  = M_get_stack_var();
    t8  = M_get_stack_var();
    t12 = M_get_stack_var();
    t13 = M_get_stack_var();
    si  = M_get_stack_var();
    co  = M_get_stack_var();
    sih = M_get_stack_var();
    coh = M_get_stack_var();
    /* sun_sincos(a, &si, &co); */
    m_apm_sin_cos(si, co, MYDIGITS, a);
    /* luai_numsinhcosh(b, &sih, &coh); */
    m_apm_sinh(sih, MYDIGITS, b);
    m_apm_cosh(coh, MYDIGITS, b);
    /* t3 = co*coh; */
    m_apm_multiply(t3, co, coh);
    /* t12 = si*sih; */
    m_apm_multiply(t12, si, sih);
    /* t4 = a*a; */
    m_apm_multiply(t4, a, a);
    /* t5 = b*b; */
    m_apm_multiply(t5, b, b);
    /* t7 = 1/(t4 + t5); */
    m_apm_add(si, t4, t5);
    m_apm_reciprocal(t7, MYDIGITS, si);
    /* t8 = a*t7; */
    m_apm_multiply(t8, a, t7);
    /* t13 = b*t7; */
    m_apm_multiply(t13, b, t7);
    /* im = -t12*t8 - t3*t13; */
    m_apm_multiply(si, t12, t8);
    m_apm_negate(t4, si);
    m_apm_multiply(co, t3, t13);
    m_apm_subtract(z->imag, t4, co);
    /* re = t3*t8 + t12*t13 */
    m_apm_multiply(si, t3, t8);
    m_apm_multiply(co, t12, t13);
    m_apm_subtract(z->real, si, co);
    M_restore_stack(11);
  }
  return 1;
}


static int Ctanc (lua_State *L) {  /* 3.5.2 */
  M_APM a, b;
  M_CAPM *z = Cnew(L);
  Cgetrealimag(L, 1, a, b);
  /* if ((a) == 0 && (b) == 0) */
  if (M_iszero(b)) {
    if (M_iszero(a)) {
      m_apm_copy(z->real, MM_One);  /* this is the limit, 6.5.15 */
    } else {
      M_APM u = M_get_stack_var();
      m_apm_tan(u, MYDIGITS, a);
      m_apm_divide(z->real, MYDIGITS, u, a);
      M_restore_stack(1);
    }
    M_set_to_zero(z->imag);
  } else {
    M_APM t2, t3, t4, t5, t6, t8, t10, t11, t13, t14, t17, t19, u, v;
    t2  = M_get_stack_var();
    t3  = M_get_stack_var();
    t4  = M_get_stack_var();
    t5  = M_get_stack_var();
    t6  = M_get_stack_var();
    t8  = M_get_stack_var();
    t10 = M_get_stack_var();
    t11 = M_get_stack_var();
    t13 = M_get_stack_var();
    t14 = M_get_stack_var();
    t17 = M_get_stack_var();
    t19 = M_get_stack_var();
    u   = M_get_stack_var();
    v   = M_get_stack_var();
    /* t2 = cos(a); */
    m_apm_sin_cos(u, t2, MYDIGITS, a);
    /* t3 = sin(a)*t2; */
    m_apm_multiply(t3, u, t2);
    /* t4 = t2*t2; */
    m_apm_multiply(t4, t2, t2);
    /* t5 = sinh(b); */
    m_apm_sinh(t5, MYDIGITS, b);
    /* t6 = t5*t5; */
    m_apm_multiply(t6, t5, t5);
    /* t8 = 1/(t4+t6); */
    m_apm_add(u, t4, t6);
    m_apm_reciprocal(t8, MYDIGITS, u);
    /* t10 = a*a; */
    m_apm_multiply(t10, a, a);
    /* t11 = b*b; */
    m_apm_multiply(t11, b, b);
    /* t13 = 1/(t10+t11); */
    m_apm_add(u, t10, t11);
    m_apm_reciprocal(t13, MYDIGITS, u);
    /* t14 = t8*a*t13; */
    m_apm_multiply(u, t8, a);
    m_apm_multiply(t14, u, t13);
    /* t17 = t5*cosh(b); */
    m_apm_cosh(u, MYDIGITS, b);
    m_apm_multiply(t17, t5, u);
    /* t19 = t8*b*t13; */
    m_apm_multiply(u, t8, b);
    m_apm_multiply(t19, u, t13);
    /* re = t3*t14+t17*t19 */
    m_apm_multiply(u, t3, t14);
    m_apm_multiply(v, t17, t19);
    m_apm_add(z->real, u, v);
    /* im = t17*t14-t3*t19 */
    m_apm_multiply(u, t17, t14);
    m_apm_multiply(v, t3, t19);
    m_apm_subtract(z->imag, u, v);
    M_restore_stack(14);
  }
  return 1;
}


static int Ccsc (lua_State *L) {
  M_APM a, b;
  M_CAPM *z = Cnew(L);
  Cgetrealimag(L, 1, a, b);
  /* if (b == 0) */
  if (M_iszero(b)) {
    /* agn_pushcomplex(L, 1/sun_sin(x), 0); */
    M_APM si = M_get_stack_var();
    m_apm_sin(si, MYDIGITS, a);
    if (M_iszero(si)) {
      lua_pop(L, 1);
      lua_pushundefined(L);  /* 6.3.9 */
    } else {
      m_apm_reciprocal(z->real, MYDIGITS, si);
      M_set_to_zero(z->imag);
    }
    M_restore_stack(1);
  } else {
    M_APM t1, t2, t4, t5, t7, t8, t9, t10, t13, u, v, w;
    t1  = M_get_stack_var();
    t2  = M_get_stack_var();
    t4  = M_get_stack_var();
    t5  = M_get_stack_var();
    t7  = M_get_stack_var();
    t8  = M_get_stack_var();
    t9  = M_get_stack_var();
    t10 = M_get_stack_var();
    t13 = M_get_stack_var();
    u   = M_get_stack_var();
    v   = M_get_stack_var();
    w   = M_get_stack_var();
    /* sun_sincos(a, &t1, &t7); */
    m_apm_sin_cos(t1, t7, MYDIGITS, a);
    /* luai_numsinhcosh(b, &t9, &t2); */
    m_apm_sinh(t9, MYDIGITS, b);
    m_apm_cosh(t2, MYDIGITS, b);
    /* t4 = t1*t1; */
    m_apm_multiply(t4, t1, t1);
    /* t5 = t2*t2; */
    m_apm_multiply(t5, t2, t2);
    /* t8 = t7*t7; */
    m_apm_multiply(t8, t7, t7);
    /* t10 = t9*t9; */
    m_apm_multiply(t10, t9, t9);
    /* t13 = 1/(t4*t5 + t8*t10); */
    m_apm_multiply(u, t4, t5);
    m_apm_multiply(v, t8, t10);
    m_apm_add(w, u, v);
    m_apm_reciprocal(t13, MYDIGITS, w);
    /* re = t1*t2*t13 */
    m_apm_multiply(u, t1, t2);
    m_apm_multiply(z->real, u, t13);
    /* im = -t7*t9*t13 */
    m_apm_multiply(u, t7, t9);
    m_apm_multiply(v, u, t13);
    m_apm_negate(z->imag, v);
    M_restore_stack(12);
  }
  return 1;
}


static int Csec (lua_State *L) {  /* 3.5.2 */
  M_APM a, b;
  M_CAPM *z = Cnew(L);
  Cgetrealimag(L, 1, a, b);
  /* if (b == 0) */
  if (M_iszero(b)) {
    /* agn_pushcomplex(L, 1/sun_sin(x), 0); */
    M_APM co = M_get_stack_var();
    m_apm_cos(co, MYDIGITS, a);
    if (M_iszero(co)) {
      lua_pop(L, 1);
      lua_pushundefined(L);  /* 6.3.9 */
    } else {
      m_apm_reciprocal(z->real, MYDIGITS, co);
      M_set_to_zero(z->imag);
    }
    M_restore_stack(1);
  } else {
    M_APM t1, t2, t4, t5, t7, t8, t9, t10, t13, u, v, w;
    t1  = M_get_stack_var();
    t2  = M_get_stack_var();
    t4  = M_get_stack_var();
    t5  = M_get_stack_var();
    t7  = M_get_stack_var();
    t8  = M_get_stack_var();
    t9  = M_get_stack_var();
    t10 = M_get_stack_var();
    t13 = M_get_stack_var();
    u   = M_get_stack_var();
    v   = M_get_stack_var();
    w   = M_get_stack_var();
    /* sun_sincos(a, &t7, &t1); */
    m_apm_sin_cos(t7, t1, MYDIGITS, a);
    /* luai_numsinhcosh(b, &t9, &t2); */
    m_apm_sinh(t9, MYDIGITS, b);
    m_apm_cosh(t2, MYDIGITS, b);
    /* t4 = t1*t1; */
    m_apm_multiply(t4, t1, t1);
    /* t5 = t2*t2; */
    m_apm_multiply(t5, t2, t2);
    /* t8 = t7*t7; */
    m_apm_multiply(t8, t7, t7);
    /* t10 = t9*t9; */
    m_apm_multiply(t10, t9, t9);
    /* t13 = 1/(t4*t5 + t8*t10); */
    m_apm_multiply(u, t4, t5);
    m_apm_multiply(v, t8, t10);
    m_apm_add(w, u, v);
    m_apm_reciprocal(t13, MYDIGITS, w);
    /* re = t1*t2*t13 */
    m_apm_multiply(u, t1, t2);
    m_apm_multiply(z->real, u, t13);
    /* im = t7*t9*t13 */
    m_apm_multiply(u, t7, t9);
    m_apm_multiply(z->imag, u, t13);
    M_restore_stack(12);
  }
  return 1;
}


static int Ccot (lua_State *L) {  /* 3.5.2 */
  M_APM a, b;
  M_CAPM *z = Cnew(L);
  Cgetrealimag(L, 1, a, b);
  /* if (b == 0) */
  if (M_iszero(b)) {
    /* agn_pushcomplex(L, -sun_tan(PIO2 + x), 0); */
    M_APM u, v;
    u = M_get_stack_var();
    v = M_get_stack_var();
    m_apm_add(u, MM_lc_HALF_PI, a);
    m_apm_tan(v, MYDIGITS, u);
    m_apm_negate(z->real, v);
    M_set_to_zero(z->imag);
    M_restore_stack(2);
  } else {
    M_APM t1, t4, t5, t6, t8, co, coh, u, v;
    t1  = M_get_stack_var();
    t4  = M_get_stack_var();
    t5  = M_get_stack_var();
    t6  = M_get_stack_var();
    t8  = M_get_stack_var();
    co  = M_get_stack_var();
    coh = M_get_stack_var();
    u   = M_get_stack_var();
    v   = M_get_stack_var();
    /* sun_sincos(a, &t1, &co); */
    m_apm_sin_cos(t1, co, MYDIGITS, a);
    /* luai_numsinhcosh(b, &t5, &coh); */
    m_apm_sinh(t5, MYDIGITS, b);
    m_apm_cosh(coh, MYDIGITS, b);
    /* t4 = t1*t1; */
    m_apm_multiply(t4, t1, t1);
    /* t6 = t5*t5; */
    m_apm_multiply(t6, t5, t5);
    /* t8 = 1/(t4 + t6); */
    m_apm_add(u, t4, t6);
    m_apm_reciprocal(t8, MYDIGITS, u);
    /* re = t1*co*t8 */
    m_apm_multiply(u, t1, co);
    m_apm_multiply(z->real, u, t8);
    /* im = -t5*coh*t8 */
    m_apm_multiply(u, t5, coh);
    m_apm_multiply(v, u, t8);
    m_apm_negate(z->imag, v);
    M_restore_stack(9);
  }
  return 1;
}


static int Ccoth (lua_State *L) {  /* 3.5.2 */
  M_APM a, b;
  M_CAPM *z = Cnew(L);
  Cgetrealimag(L, 1, a, b);
  /* if (b == 0) */
  if (M_iszero(b)) {
    /* agn_pushcomplex(L, 1/luai_numtanh(x), 0); */
    M_APM u = M_get_stack_var();
    m_apm_tanh(u, MYDIGITS, a);
    if (M_iszero(u)) {
      lua_pop(L, 1);
      lua_pushundefined(L);  /* 6.3.9 */
    } else {
      m_apm_reciprocal(z->real, MYDIGITS, u);
      M_set_to_zero(z->imag);
    }
    M_restore_stack(1);
  } else {
    M_APM t1, t2, t4, t5, t6, t7, t9, t11, t12, t14, t15, t20, u, v, w;
    t1  = M_get_stack_var();
    t2  = M_get_stack_var();
    t4  = M_get_stack_var();
    t5  = M_get_stack_var();
    t6  = M_get_stack_var();
    t7  = M_get_stack_var();
    t9  = M_get_stack_var();
    t11 = M_get_stack_var();
    t12 = M_get_stack_var();
    t14 = M_get_stack_var();
    t15 = M_get_stack_var();
    t20 = M_get_stack_var();
    u   = M_get_stack_var();
    v   = M_get_stack_var();
    w   = M_get_stack_var();
    /* sun_sincos(b, &t14, &t5); */
    m_apm_sin_cos(t14, t5, MYDIGITS, b);
    /* luai_numsinhcosh(a, &t1, &t2); */
    m_apm_sinh(t1, MYDIGITS, a);
    m_apm_cosh(t2, MYDIGITS, a);
    /* t4 = t1*t1; */
    m_apm_multiply(t4, t1, t1);
    /* t6 = t5*t5; */
    m_apm_multiply(t6, t5, t5);
    /* t7 = t4+t6; */
    m_apm_add(t7, t4, t6);
    /* t9 = t2*t2; */
    m_apm_multiply(t9, t2, t2);
    /* t11 = t7*t7; */
    m_apm_multiply(t11, t7, t7);
    /* t12 = 1/t11; */
    m_apm_reciprocal(t12, MYDIGITS, t11);
    /* t15 = t14*t14; */
    m_apm_multiply(t15, t14, t14);
    /* t20 = 1/t7/(t4*t9*t12 + t15*t6*t12); */
    /* v = t4*t9*t12 */
    m_apm_multiply(u, t4, t9);
    m_apm_multiply(v, u, t12);
    /* w = t15*t6*t12 */
    m_apm_multiply(u, t15, t6);
    m_apm_multiply(w, u, t12);
    /* u = (t4*t9*t12 + t15*t6*t12) */
    m_apm_add(u, v, w);
    /* v = 1/t7 */
    m_apm_reciprocal(v, MYDIGITS, t7);
    /* t20 = v/u */
    m_apm_divide(t20, MYDIGITS, v, u);
    /* re = t1*t2*t20 */
    m_apm_multiply(u, t1, t2);
    m_apm_multiply(z->real, u, t20);
    /* im = -t14*t5*t20 */
    m_apm_multiply(u, t14, t5);
    m_apm_multiply(v, u, t20);
    m_apm_negate(z->imag, v);
    M_restore_stack(15);
  }
  return 1;
}


static int Ccsch (lua_State *L) {
  M_APM a, b, t1, t2, t4, t5, t7, t8, t9, t10, t13, u, v, w;
  M_CAPM *z;
  z = Cnew(L);
  Cgetrealimag(L, 1, a, b);
  /* if (b == 0) */
  if (M_iszero(b)) {
    u = M_get_stack_var();
    m_apm_sinh(u, MYDIGITS, a);
    if (M_iszero(u)) {
      lua_pop(L, 1);
      lua_pushundefined(L);  /* 6.3.9, 6.6.3 fix */
    } else {
      m_apm_reciprocal(z->real, MYDIGITS, u);
      M_set_to_zero(z->imag);
    }
    M_restore_stack(1);
  } else {
    t1  = M_get_stack_var();
    t2  = M_get_stack_var();
    t4  = M_get_stack_var();
    t5  = M_get_stack_var();
    t7  = M_get_stack_var();
    t8  = M_get_stack_var();
    t9  = M_get_stack_var();
    t10 = M_get_stack_var();
    t13 = M_get_stack_var();
    u   = M_get_stack_var();
    v   = M_get_stack_var();
    w   = M_get_stack_var();
    /* t1 = sinh(a); t2 = cos(b); t7 = cosh(a); t9 = sin(b); */
    m_apm_sin_cos(t9, t2, MYDIGITS, b);
    m_apm_sinh(t1, MYDIGITS, a);
    m_apm_cosh(t7, MYDIGITS, a);
    /* t4 = t1*t1; */
    m_apm_multiply(t4, t1, t1);
    /* t5 = t2*t2; */
    m_apm_multiply(t5, t2, t2);
    /* t8 = t7*t7; */
    m_apm_multiply(t8, t7, t7);
    /* t10 = t9*t9; */
    m_apm_multiply(t10, t9, t9);
    /* t13 = 1/(t4*t5+t8*t10); */
    m_apm_multiply(u, t4, t5);
    m_apm_multiply(v, t8, t10);
    m_apm_add(w, u, v);
    m_apm_reciprocal(t13, MYDIGITS, w);
    /* re = t1*t2*t13 */
    m_apm_multiply(u, t1, t2);
    m_apm_multiply(z->real, u, t13);
    /* im = -t7*t9*t13; */
    m_apm_multiply(u, t7, t9);
    m_apm_multiply(v, u, t13);
    m_apm_negate(z->imag, v);
    M_restore_stack(12);
  }
  return 1;
}


static int Csech (lua_State *L) {
  M_APM a, b, u;
  M_CAPM *z;
  z = Cnew(L);
  Cgetrealimag(L, 1, a, b);
  /* if (b == 0) */
  if (M_iszero(b)) {
    u = M_get_stack_var();
    m_apm_cosh(u, MYDIGITS, a);
    if (m_apm_compare(u, MM_Zero) == 0) {
      lua_pop(L, 1);
      lua_pushundefined(L);  /* 6.3.9, 6.6.3 fix */
    } else {
      m_apm_reciprocal(z->real, MYDIGITS, u);
      M_set_to_zero(z->imag);
    }
    M_restore_stack(1);
  } else {
    M_APM t1, t2, t4, t5, t7, t8, t9, t10, t13, v, w;
    t1  = M_get_stack_var();
    t2  = M_get_stack_var();
    t4  = M_get_stack_var();
    t5  = M_get_stack_var();
    t7  = M_get_stack_var();
    t8  = M_get_stack_var();
    t9  = M_get_stack_var();
    t10 = M_get_stack_var();
    t13 = M_get_stack_var();
    u   = M_get_stack_var();
    v   = M_get_stack_var();
    w   = M_get_stack_var();
    /* t1 = cosh(a); t2 = cos(b); t7 = sinh(a); t9 = sin(b); */
    m_apm_sin_cos(t9, t2, MYDIGITS, b);
    m_apm_sinh(t7, MYDIGITS, a);
    m_apm_cosh(t1, MYDIGITS, a);
    /* t4 = t1*t1; */
    m_apm_multiply(t4, t1, t1);
    /* t5 = t2*t2; */
    m_apm_multiply(t5, t2, t2);
    /* t8 = t7*t7; */
    m_apm_multiply(t8, t7, t7);
    /* t10 = t9*t9; */
    m_apm_multiply(t10, t9, t9);
    /* t13 = 1/(t4*t5+t8*t10); */
    m_apm_multiply(u, t4, t5);
    m_apm_multiply(v, t8, t10);
    m_apm_add(w, u, v);
    m_apm_reciprocal(t13, MYDIGITS, w);
    /* re = t1*t2*t13 */
    m_apm_multiply(u, t1, t2);
    m_apm_multiply(z->real, u, t13);
    /* im = -t7*t9*t13; */
    m_apm_multiply(u, t7, t9);
    m_apm_multiply(v, u, t13);
    m_apm_negate(z->imag, v);
    M_restore_stack(12);
  }
  return 1;
}


static int Cexp2 (lua_State *L) {
  M_APM z_re, z_im, exponent_im, magnitude, sin_val, cos_val;
  M_CAPM *z = Cnew(L);
  Cgetrealimag(L, 1, z_re, z_im);
  exponent_im = M_get_stack_var();
  magnitude   = M_get_stack_var();
  sin_val     = M_get_stack_var();
  cos_val     = M_get_stack_var();
  m_apm_pow_adaptive(magnitude, MYDIGITS, MM_Two, z_re);
  m_apm_multiply(exponent_im, z_im, MM_lc_log2);
  m_apm_sin_cos(sin_val, cos_val, MYDIGITS, exponent_im);
  m_apm_multiply(z->real, magnitude, cos_val);
  m_apm_multiply(z->imag, magnitude, sin_val);
  M_restore_stack(4);
  return 1;
}


static int Cexp10 (lua_State *L) {
  M_APM z_re, z_im, exponent_im, magnitude, sin_val, cos_val;
  M_CAPM *z = Cnew(L);
  Cgetrealimag(L, 1, z_re, z_im);
  exponent_im = M_get_stack_var();
  magnitude   = M_get_stack_var();
  sin_val     = M_get_stack_var();
  cos_val     = M_get_stack_var();
  m_apm_pow_adaptive(magnitude, MYDIGITS, MM_Ten, z_re);
  m_apm_multiply(exponent_im, z_im, MM_lc_log10);
  m_apm_sin_cos(sin_val, cos_val, MYDIGITS, exponent_im);
  m_apm_multiply(z->real, magnitude, cos_val);
  m_apm_multiply(z->imag, magnitude, sin_val);
  M_restore_stack(4);
  return 1;
}


/* 6.6.4, based on mpfr.c/mpfr_clog2() */
static int Clog2 (lua_State *L) {
  M_APM z_re, z_im, r1, r2, t, angle;
  M_CAPM *z = Cnew(L);
  Cgetrealimag(L, 1, z_re, z_im);
  r1    = M_get_stack_var();
  r2    = M_get_stack_var();
  t     = M_get_stack_var();
  angle = M_get_stack_var();
  m_apm_square(r1, z_re);
  m_apm_square(t, z_im);
  m_apm_add(r2, r1, t);
  m_apm_log(t, MYDIGITS, r2);
  m_apm_divide(r1, MYDIGITS, t, MM_lc_log2);
  m_apm_multiply(z->real, r1, MM_0_5);
  m_apm_atan2(angle, MYDIGITS, z_im, z_re);
  m_apm_divide(z->imag, MYDIGITS, angle, MM_lc_log2);
  M_restore_stack(4);
  return 1;
}


/* 6.6.4, based on mpfr.c/mpfr_clog10() */
static int Clog10 (lua_State *L) {
  M_APM z_re, z_im, r1, r2, t, angle;
  M_CAPM *z = Cnew(L);
  Cgetrealimag(L, 1, z_re, z_im);
  r1    = M_get_stack_var();
  r2    = M_get_stack_var();
  t     = M_get_stack_var();
  angle = M_get_stack_var();
  m_apm_square(r1, z_re);
  m_apm_square(t, z_im);
  m_apm_add(r2, r1, t);
  m_apm_log(t, MYDIGITS, r2);
  m_apm_divide(r1, MYDIGITS, t, MM_lc_log10);
  m_apm_multiply(z->real, r1, MM_0_5);
  m_apm_atan2(angle, MYDIGITS, z_im, z_re);
  m_apm_divide(z->imag, MYDIGITS, angle, MM_lc_log10);
  M_restore_stack(4);
  return 1;
}


static int Ccbrt (lua_State *L) {  /* 6.6.5 */
  M_APM z_re, z_im, r, theta, s, c, t;
  M_CAPM *z = Cnew(L);
  Cgetrealimag(L, 1, z_re, z_im);
  r     = M_get_stack_var();
  theta = M_get_stack_var();
  s     = M_get_stack_var();
  c     = M_get_stack_var();
  t     = M_get_stack_var();
  /* 1. Calculate magnitude r = sqrt(x^2 + y^2) */
  m_apm_square(r, z_re);
  m_apm_square(t, z_im);
  m_apm_add(s, r, t);
  m_apm_sqrt(r, MYDIGITS,s);
  /* 2. Calculate the cubic root of the magnitude: r^(1/3) */
  m_apm_cbrt(t, MYDIGITS, r);
  /* 3. Calculate the angle theta = atan2(y, x) */
  m_apm_atan2(theta, MYDIGITS, z_im, z_re);
  /* 4. Divide the angle by 3 */
  m_apm_divide(theta, MYDIGITS, theta, MM_Three);
  /* 5. Simultaneous sine and cosine of (theta/3) */
  m_apm_sin_cos(s, c, MYDIGITS, theta);
  /* 6. Finalize: res = r_cbrt * (cos + i*sin) */
  m_apm_multiply(z->real, t, c);
  m_apm_multiply(z->imag, t, s);
  M_restore_stack(5);
  return 1;
}


static int mt_index (lua_State *L) {  /* 6.1.4 */
  if (ismapm(L, 1) || iscmapm(L, 1)) {
    if (lua_gettop(L) == 2 && agn_isstring(L, 2)) {
      /* sizeof(<string constant>) == strlen(<string constant>) + 1 */
      return agn_initmethodcall(L, AGENA_MAPMLIBNAME, sizeof(AGENA_MAPMLIBNAME) - 1);
    }
  }
  luaL_error(L, "Error in " LUA_QS " package: illegal call.", AGENA_MAPMLIBNAME);
  return 0;
}


static int mapm_new (lua_State *L) {  /* 6.3.9 */
  LL = L;
  int digits, nargs = lua_gettop(L);
  aux_newoptions(L, 2, &nargs, &digits, "mapm.new");
  switch (nargs) {
    case 1: {
      Bgetx(L, 1, digits);
      lua_settop(L, 1);
      break;
    }
    case 2: {
      M_CAPM *x = Cnew(L);  /* pushes new M_CAPM userdata onto the stack top */
      setcomponent(L, x->real, 1, digits);
      setcomponent(L, x->imag, 2, digits);
      break;
    }
    default:
      luaL_error(L, "Error in " LUA_QS ": expected one or two arguments.", "mapm.new");
  }
  return 1;
}


static int mapm_tonumber (lua_State *L) {  /* 6.4.0 */
  if (ismapm(L, 1)) {
    return Btonumber(L);
  } else if (agn_isnumber(L, 1) && sun_isirregular(agn_tonumber(L, 1))) {  /* undefined, +/-infinity, 6.4.7 */
    lua_pushnumber(L, agn_tonumber(L, 1));
    return 1;
  } else if (iscmapm(L, 1)) {
    return Ctonumber(L);
  } else {
    luaL_error(L, "Error in " LUA_QS ": expected mapm input.", "mapm.tonumber");
  }
  return 0;
}


static int mapm_swap (lua_State *L) {  /* 6.4.2 */
  if (ismapm(L, 1)) {
    M_APM a, b, t;
    a = Bget(L, 1);
    b = Bget(L, 2);
    t = M_get_stack_var();
    m_apm_copy(t, a);
    m_apm_copy(a, b);
    m_apm_copy(b, t);
    M_restore_stack(1);
  } else if (iscmapm(L, 1)) {
    M_APM a, b, c, d, t;
    Cgetrealimag(L, 1, a, b);
    Cgetrealimag(L, 2, c, d);
    t = M_get_stack_var();
    m_apm_copy(t, a);
    m_apm_copy(a, c);
    m_apm_copy(c, t);
    m_apm_copy(t, b);
    m_apm_copy(b, d);
    m_apm_copy(d, t);
    M_restore_stack(1);
  } else {
    luaL_error(L, "Error in " LUA_QS ": expected either real or complex mapm input.", "mapm.swap");
  }
  return 0;
}


static int mapm_min (lua_State *L) {  /* 6.4.2 */
  int lt = 0;
  if (ismapm(L, 1) && ismapm(L, 2)) {
    lt = m_apm_compare(Bget(L, 1), Bget(L, 2)) < 0;
  } else if (iscmapm(L, 1) && iscmapm(L, 2)) {
    M_APM a, b, c, d, abs1, abs2;
    Cgetrealimag(L, 1, a, b);
    Cgetrealimag(L, 2, c, d);
    abs1 = M_get_stack_var();
    abs2 = M_get_stack_var();
    m_apm_hypot(abs1, MYDIGITS, a, b);
    m_apm_hypot(abs2, MYDIGITS, c, d);
    lt = m_apm_compare(abs1, abs2) < 0;
    M_restore_stack(2);
  } else {
    luaL_error(L, "Error in " LUA_QS ": expected real mapm input.", "mapm.min");
  }
  lua_pushvalue(L, 2 - lt);
  return 1;
}


static int mapm_max (lua_State *L) {  /* 6.4.2 */
  int gt = 0;
  if (ismapm(L, 1) && ismapm(L, 2)) {
    gt = m_apm_compare(Bget(L, 1), Bget(L, 2)) > 0;
  } else if (iscmapm(L, 1) && iscmapm(L, 2)) {
    M_APM a, b, c, d, abs1, abs2;
    Cgetrealimag(L, 1, a, b);
    Cgetrealimag(L, 2, c, d);
    abs1 = M_get_stack_var();
    abs2 = M_get_stack_var();
    m_apm_hypot(abs1, MYDIGITS, a, b);
    m_apm_hypot(abs2, MYDIGITS, c, d);
    gt = m_apm_compare(abs1, abs2) > 0;
    M_restore_stack(2);
  } else {
    luaL_error(L, "Error in " LUA_QS ": expected real mapm input.", "mapm.max");
  }
  lua_pushvalue(L, 2 - gt);
  return 1;
}


static int mapm_minmax (lua_State *L) {  /* 6.4.2 */
  int lt = 0;
  if (ismapm(L, 1) && ismapm(L, 2)) {
    lt = m_apm_compare(Bget(L, 1), Bget(L, 2)) < 0;
  } else if (iscmapm(L, 1) && iscmapm(L, 2)) {
    M_APM a, b, c, d, abs1, abs2;
    Cgetrealimag(L, 1, a, b);
    Cgetrealimag(L, 2, c, d);
    abs1 = M_get_stack_var();
    abs2 = M_get_stack_var();
    m_apm_hypot(abs1, MYDIGITS, a, b);
    m_apm_hypot(abs2, MYDIGITS, c, d);
    lt = m_apm_compare(abs1, abs2) < 0;
    M_restore_stack(2);
  } else {
    luaL_error(L, "Error in " LUA_QS ": expected real mapm input.", "mapm.minmax");
  }
  luaL_checkstack(L, 2, "not enough stack space");
  lua_pushvalue(L, 2 - lt);
  lua_pushvalue(L, 1 + lt);
  return 2;
}


static int mapm_trim (lua_State *L) {  /* 6.6.3 */
  m_apm_trim_mem_usage();
  return 0;
}


/* The metemethods ************************************************************************/

static const struct luaL_Reg mapm_lib [] = {  /* metamethods for real mapm numbers */
  {"__abs",         mt_abs},
  {"__absdiff",     mt_absdiff},
  {"__sign",        mt_sign},
  {"__unm",         mt_unm},
  {"__add",         mt_add},
  {"__sub",         mt_sub},
  {"__mul",         mt_mul},
  {"__div",         mt_div},
  {"__intdiv",      Bidiv},
  {"__mod",         mt_mod},
  {"__eq",          mt_eq},
  {"__eeq",         mt_eq},
  {"__aeq",         mt_aeq},
  {"__lt",          mt_lt},
  {"__le",          mt_le},
  {"__pow",         mt_pow},
  {"__ipow",        Bipow},
  {"__square",      mt_square},
  {"__cube",        mt_cube},
  {"__antilog2",    mt_antilog2},
  {"__antilog10",   mt_antilog10},
  {"__recip",       mt_recip},
  {"__sqrt",        mt_sqrt},
  {"__invsqrt",     mt_invsqrt},
  {"__ln",          mt_ln},
  {"__exp",         mt_exp},
  {"__sin",         mt_sin},
  {"__cos",         mt_cos},
  {"__tan",         mt_tan},
  {"__arcsec",      mt_arcsec},
  {"__arcsin",      mt_arcsin},
  {"__arccos",      mt_arccos},
  {"__arctan",      mt_arctan},
  {"__sinc",        mt_sinc},
  {"__sinh",        mt_sinh},
  {"__cosh",        mt_cosh},
  {"__tanh",        mt_tanh},
  {"__entier",      mt_entier},
  {"__int",         Bint},
  {"__frac",        Bfrac},
  {"__integral",    Bisint},
  {"__fractional",  Bisfrac},
  {"__even",        mt_even},     /* __even(x) */
  {"__odd",         mt_odd},      /* __odd(x) */
  {"__zero",        mt_zero},     /* __zero(x) */
  {"__nonzero",     mt_nonzero},  /* __nonzero(x) */
  {"__band",        mapm_xband},
  {"__bor",         mapm_xbor},
  {"__bxor",        mapm_xbxor},
  {"__gc",          mt_gc},
  {"__tostring",    mt_tostring},  /* __tostring(x) */
  {"__index",       mt_index},  /* OOP-style calls */
  {NULL, NULL}
};


static const struct luaL_Reg mapm_clib [] = {  /* metamethods for complex mapm numbers */
  {"__abs",         mt_cabs},
  {"__sign",        mt_csgn},
  {"__add",         mt_cadd},  /* __add(x, y) */
  {"__sub",         mt_csub},  /* __sub(x, y) */
  {"__mul",         mt_cmul},  /* __mul(x, y) */
  {"__div",         mt_cdiv},  /* __div(x, y) */
  {"__recip",       mt_crecip},
  {"__pow",         mt_cpow},  /* __pow(x, y) */
  {"__ipow",        mt_cipow}, /* __ipow(x, y) */
  {"__square",      mt_csquare},
  {"__cube",        mt_ccube},
  {"__sqrt",        mt_csqrt},
  {"__ln",          mt_cln},
  {"__exp",         mt_cexp},
  {"__sin",         mt_csin},
  {"__cos",         mt_ccos},
  {"__tan",         mt_ctan},
  {"__sinc",        mt_csinc},
  {"__sinh",        mt_csinh},
  {"__cosh",        mt_ccosh},
  {"__tanh",        mt_ctanh},
  {"__arcsin",      mt_carcsin},
  {"__arccos",      mt_carccos},
  {"__arctan",      mt_carctan},
  {"__arcsec",      mt_carcsec},
  {"__real",        mt_creal},
  {"__imag",        mt_cimag},
  {"__unm",         mt_cunm},  /* __unm(x) */
  {"__eq",          mt_ceq},   /* __eq(x, y) */
  {"__aeq",         mt_aeq},
  {"__zero",        mt_czero},
  {"__nonzero",     mt_cnonzero},
  {"__gc",          mt_cgc},
  {"__tostring",    mt_ctostring},
  {"__index",       mt_index},  /* OOP-style calls */
  {NULL, NULL}
};


static const luaL_Reg mapmlib[] = {
  /* real compartment */
  {"approx",         mapm_approx},
  {"bprep",          mapm_bprep},
  {"checkxnumber",   mapm_checkxnumber},
  {"checkcnumber",   mapm_checkcnumber},
  {"checkinteger",   mapm_checkinteger},
  {"checknonnegint", mapm_checknonnegint},
  {"checknonnegative", mapm_checknonnegative},
  {"checkposint",    mapm_checkposint},
  {"checkpositive",  mapm_checkpositive},
  {"isxnumber",      mapm_isxnumber},
  {"iscnumber",      mapm_iscnumber},
  {"max",            mapm_max},
  {"min",            mapm_min},
  {"minmax",         mapm_minmax},
  {"new",            mapm_new},
  {"swap",           mapm_swap},
  {"tonumber",       mapm_tonumber},
  {"trim",           mapm_trim},
  {"xabs",           Babs},
  {"xarccos",        Bacos},
  {"xarccosh",       Bacosh},
  {"xadd",           Badd},
  {"xarccsc",        Bacsc},
  {"xarcsec",        Basec},
  {"xarcsin",        Basin},
  {"xarcsinh",       Basinh},
  {"xarctan",        Batan},
  {"xarctan2",       Batan2},
  {"xarctanh",       Batanh},
  {"xattrib",        mapm_xattrib},
  {"xband",          mapm_xband},
  {"xbor",           mapm_xbor},
  {"xbxor",          mapm_xbxor},
  {"xcathet",        Bcathet},
  {"xcbrt",          Bcbrt},
  {"xceil",          Bceil},
  {"xchebyt",        Bchebyt},
  {"xcompare",       Bcompare},
  {"xcos",           Bcos},
  {"xcosc",          Bcosc},
  {"xcosh",          Bcosh},
  {"xcot",           Bcot},
  {"xcoth",          Bcoth},
  {"xcsc",           Bcsc},
  {"xcsch",          Bcsch},
  {"xcube",          Bcube},
  {"xdigits",        Bdigits},
  {"xdigitsin",      Bdigitsin},
  {"xdiv",           Bdiv},
  {"xexp",           Bexp},
  {"xexp2",          mapm_xexp2},
  {"xexp10",         mapm_xexp10},
  {"xexponent",      Bexponent},
  {"xfactorial",     Bfactorial},
  {"xfloor",         Bfloor},
  {"xfma",           mapm_xfma},
  {"xfrac",          Bfrac},
  {"xgcd",           mapm_xgcd},
  {"xhypot",         Bhypot},
  {"xidiv",          Bidiv},
  {"xint",           Bint},
  {"xintfrac",       Bintfrac},
  {"xinv",           Binv},
  {"xinvmod",        mapm_xinvmod},
  {"xipow",          Bipow},
  {"xiseven",        Biseven},
  {"xisint",         Bisint},
  {"xisnegative",    Bisnegative},
  {"xisnegint",      Bisnegint},
  {"xisnonnegative", Bisnonnegative},
  {"xisnonnegint",   Bisnonnegint},
  {"xisodd",         Bisodd},
  {"xisposint",      Bisposint},
  {"xispositive",    Bispositive},
  {"xispow2",        mapm_xispow2},
  {"xisprime",       Bisprime},
  {"xlcm",           mapm_xlcm},
  {"xln",            Blog},  /* changed 0.30.3 */
  {"xlog",           mapm_xlog},
  {"xlog2",          mapm_xlog2},
  {"xlog10",         Blog10},
  {"xmax",           mapm_xmax},
  {"xmin",           mapm_xmin},
  {"xmod",           Bmod},
  {"xmul",           Bmul},
  {"xmulmod",        mapm_xmulmod},
  {"xneg",           Bneg},
  {"xnumber",        Bnumber},
  {"xpow",           Bpow},
  {"xpowmod",        mapm_xpowmod},
  {"xrandom",        mapm_xrandom},
  {"xrandomseed",    mapm_xrandomseed},
  {"xrecip",         Binv},
  {"xround",         Bround},
  {"xsec",           Bsec},
  {"xsech",          Bsech},
  {"xsign",          Bsign},
  {"xsin",           Bsin},
  {"xsinc",          Bsinc},
  {"xsincos",        Bsincos},
  {"xsinhcosh",      Bsinhcosh},
  {"xsinh",          Bsinh},
  {"xsquare",        Bsquare},
  {"xsqrt",          Bsqrt},
  {"xsub",           Bsub},
  {"xtan",           Btan},
  {"xtanc",          Btanc},
  {"xtanh",          Btanh},
  {"xterm",          mapm_xterm},
  {"xtonumber",      Btonumber},
  {"xtostring",      Btostring},
  /* complex compartment */
  {"carccosh",       Carccosh},
  {"carcsinh",       Carcsinh},
  {"carctanh",       Carctanh},
  {"cargument",      Cargument},
  {"ccbrt",          Ccbrt},
  {"ccosc",          Ccosc},
  {"ccot",           Ccot},
  {"ccoth",          Ccoth},
  {"ccsc",           Ccsc},
  {"ccsch",          Ccsch},
  {"cexp10",         Cexp10},
  {"cexp2",          Cexp2},
  {"cfma",           Cfma},
  {"clog10",         Clog10},
  {"clog2",          Clog2},
  {"cnumber",        Cnumber},
  {"csec",           Csec},
  {"csech",          Csech},
  {"csinc",          Csinc},
  {"csincos",        Csincos},
  {"csinhcosh",      Csinhcosh},
  {"ctanc",          Ctanc},
  {"ctocomplex",     Ctocomplex},
  {"ctonumber",      Ctonumber},
  {"ctostring",      mapm_ctostring},
  {NULL, NULL}
};

static void clearmapm (void) {  /* 2.9.6 */
  m_apm_free_all_mem();
}

static int nopened = 0;

LUALIB_API int luaopen_mapm (lua_State *L) {
  if (nopened++ == 0) atexit(clearmapm);  /* only register once, 3.6.1 */
  luaL_newmetatable(L, MAPMXTYPE);      /* metatable for userdata, adds it to the registry with key 'mapm' */
  luaL_register(L, NULL, mapm_lib);     /* assign C metamethods to this metatable */
  luaL_newmetatable(L, MAPMCTYPE);      /* metatable for long userdata, adds it to the registry with key 'mapm' */
  luaL_register(L, NULL, mapm_clib);    /* assign C metamethods to this metatable */
  luaL_register(L, AGENA_MAPMLIBNAME, mapmlib);  /* leaves the package table on the top of the stack */
  lua_pushliteral(L, "version");
  lua_pushliteral(L, MYVERSION);
  lua_settable(L, -3);
  lua_pushliteral(L, "__index");
  lua_pushvalue(L, -2);
  lua_settable(L, -3);
  return 1;
}

