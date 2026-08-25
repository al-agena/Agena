/*
** $Id: clock.c, initiated May 09, 2024 $
** Sexagesimal time library library
** See Copyright Notice in agena.h
*/

#define clock_c
#define LUA_LIB

#include <stdlib.h>

#include "agena.h"
#include "agenalib.h"
#include "agnhlps.h"
#include "agnxlib.h"
#include "sofa.h"

#define TMTYPENAME       "tm"

#define isclocktuple(L, idx) (lua_isfunction(L, idx) && agn_isutype(L, idx, TMTYPENAME))

static FORCE_INLINE void gettm (lua_State *L, int idx, double *s, const char *procname) {
  int i;
  lu_byte nupvals;
  nupvals = lua_nupvalues(L, idx);
  if (nupvals != 3)
    luaL_error(L, "Error in " LUA_QS ": `tm` value is invalid.", procname);
  for (i=0; i < 3; i++) {
    lua_getupvalue(L, idx, i + 1);
    s[i] = agn_checknumber(L, -1);
    agn_poptop(L);
  }
}

static int clock_adjust (lua_State *L) {
  int i, nargs, wraphours;
  lua_Number s[3], inseconds, sgn, tmod;
  nargs = lua_gettop(L);
  wraphours = 0;
  if (nargs > 1 && lua_istrue(L, nargs)) {  /* 7.3.1 extension */
    wraphours = 1; nargs--;
  }
  if (!(isclocktuple(L, 1))) {
    s[0] = (lua_Number)agn_checkinteger(L, 1);
    s[1] = (lua_Number)agn_checkinteger(L, 2);
    s[2] = agnL_optnumber(L, 3, 0.0);
  } else {
    gettm(L, 1, s, "clock.adjust");
  }
  inseconds = s[0]*3600.0 + s[1]*60.0 + sun_trunc(s[2]);
  sgn = tools_sign(inseconds);  /* remember sign */
  inseconds = fabs(inseconds);  /* proceed calculation with absolute values */
  tmod = luai_nummod(inseconds, 3600);
  s[0] = luai_numintdiv(inseconds, 3600);  /* hours */
  if (wraphours) {  /* 7.3.1 extension, Gemini AI: to handle both positive overflows and negative underflows,
     the most robust way in C is the "Double Modulo" trick. This is much safer than the standard % operator,
     which can behave inconsistently with negative numbers in different C compilers. */
    s[0] = ((((int)s[0]) % 24) + 24) % 24;
  }
  s[1] = luai_numintdiv(tmod, 60);  /* minutes */
  s[2] = luai_nummod(tmod, 60) + sun_frac(s[2]);  /* seconds, prevent round-off errors if seconds component includes milliseconds */
  if (nargs == 1) {  /* tm data structure given ? */
    luaL_checkstack(L, 2, "not enough stack space");
    for (i=0; i < 3; i++) {  /* in-place operation */
      lua_getupvalue(L, 1, i + 1);
      lua_pushnumber(L, sgn*s[i]);
      lua_setupvalue(L, 1, i + 1);
    }
  } else {  /* 7.3.1 extension */
    double djt;
    luaL_checkstack(L, 3, "not enough stack space");
    for (i=0; i < 3; i++) {
      lua_pushnumber(L, sgn*s[i]);
    }
    iauTf2d(s[0] >= 0.0 ? '+' : '-', s[0], s[1], s[2], &djt);  /* the fraction of day */
    lua_pushnumber(L, djt);
  }
  return 0 + 4*(nargs > 1) ;  /* return nothing when tm data structure given, else return h, m, s */
}


static const luaL_Reg clocklib[] = {
  {"adjust", clock_adjust},              /* added on May 09, 2024 */
  {NULL, NULL}
};


/*
** Open clock library
*/
LUALIB_API int luaopen_clock (lua_State *L) {
  luaL_register(L, AGENA_CLOCKLIBNAME, clocklib);
  return 1;
}

