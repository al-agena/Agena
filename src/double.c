/*
** $Id: double.c,v 1.67 15/10/2022 roberto Exp $
** Double Arithmetic Library
** Just to play around with userdata.
** See Copyright Notice in agena.h
**
** This package is of no practical use - it just demonstrates how to create userdata in C and how to extend the
** functionality by procedures written in the Agena language, see file `double.agn`.
*/

#include <math.h>

#include <string.h>

#define double_c
#define LUA_LIB

#include "agena.h"

#include "agenalib.h"
#include "agnconf.h"
#include "agnxlib.h"
#include "double.h"
#include "long.h"

#if !(defined(__DJGPP__) || defined(__OS2__) || defined(LUA_ANSI))
#define AGENA_DOUBLELIBNAME "double"
LUALIB_API int (luaopen_double) (lua_State *L);
#endif

#define AGENA_LIBVERSION	"double 0.0.1 for Agena as of December 09, 2023\n"

static int double_new (lua_State *L) {
  DLong *d = (DLong *)lua_newuserdata(L, sizeof(DLong));
  lua_setmetatabletoobject(L, -1, "doublevalue", 1);
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER:
      d->value = agn_tonumber(L, 1);
      break;
    case LUA_TSTRING:
#ifndef __ARMCPU
      d->value = str_strtold(agn_tostring(L, 1), NULL);
#else
      d->value = strtod(agn_tostring(L, 1), NULL);
#endif
      break;
    case LUA_TUSERDATA:
      if (luaL_isudata(L, 1, "doublevalue")) {
        d->value = getdoublevalue(L, 1);
      } else
        luaL_error(L, "Error in " LUA_QS ": wrong userdata.", "double.new");
      break;
    default:
      luaL_error(L, "Error in " LUA_QS ": expected number, a string or long userdata.", "double.new");
  }
  return 1;
}


static int dmt_gc (lua_State *L) {  /* please do not forget to garbage collect deleted userdata */
  (void)checkdouble(L, 1);
  lua_setmetatabletoobject(L, 1, NULL, 1);  /* delete metattable and user-defined type */
  return 0;
}


static int double_tostring (lua_State *L) {  /* 2.34.10 */
  DLong *d = checkdouble(L, 1);
  const char *format = agnL_optstring(L, 2, NULL);
  if (!format) {
    longdouble absx = fabs(d->value);
    lua_pushfstring(L, absx < 1e-10 || absx > 1e20 ? "%e" : "%f", d->value);  /* 19 fractional digits */
  } else {  /* 2.34.10 extension */
    if (agnL_gettablefield(L, "strings", "format", "double.tostring", 1) != LUA_TFUNCTION) {
      luaL_error(L, "Error in " LUA_QS ": could not find `strings.format`.", "double.tostring");
    }
    lua_pushstring(L, format);
    createdouble(L, d->value);
    lua_call(L, 2, 1);
    if (!lua_isstring(L, -1)) {
      agn_poptop(L);
      luaL_error(L, "Error in " LUA_QS ": `strings.format` did not return a string.", "double.tostring");
    }
  }
  return 1;
}


static int double_tonumber (lua_State *L) {
  lua_pushnumber(L, (double)checkandgetdouble(L, 1));
  return 1;
}


static const struct luaL_Reg double_lib [] = {  /* metamethods for userdata `n` */
  {"__gc",         dmt_gc},        /* do not forget garbage collection */
  {NULL, NULL}
};

static const luaL_Reg doublelib[] = {
  {"new",          double_new},
  {"tonumber",     double_tonumber},
  {"tostring",     double_tostring},
  {NULL, NULL}
};


/*
** Open double library
*/
LUALIB_API int luaopen_double (lua_State *L) {
  luaL_newmetatable(L, "doublevalue");   /* metatable for double userdata, adds it to the registry with key 'doublevalue' */
  luaL_register(L, NULL, double_lib);    /* assign C metamethods to this metatable */
  luaL_register(L, AGENA_DOUBLELIBNAME, doublelib);
  lua_pushinteger(L, FP_NAN);
  lua_setfield(L, -2, "fp_nan");         /* for FP_NAN */
  lua_pushinteger(L, FP_INFINITE);
  lua_setfield(L, -2, "fp_infinite");    /* for FP_INFINITE */
  lua_pushinteger(L, FP_SUBNORMAL);
  lua_setfield(L, -2, "fp_subnormal");   /* for FP_SUBNORMAL */
  lua_pushinteger(L, FP_ZERO);
  lua_setfield(L, -2, "fp_zero");        /* for FP_ZERO */
  lua_pushinteger(L, FP_NORMAL);
  lua_setfield(L, -2, "fp_normal");      /* for FP_NORMAL */
  createdouble(L, DBL_MIN);              /* minimum positive _normalised_ value */
  lua_setfield(L, -2, "MinDouble");
  return 1;
}

