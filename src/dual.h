#ifndef dual_h
#define dual_h

#define agn_isdual(L,idx)  (lua_isreg(L, idx) && agn_isutype(L, idx, AGENA_DUALLIBNAME))
#define isdual(L,idx)      (lua_isreg(L, idx) && agn_isutype(L, idx, AGENA_DUALLIBNAME) && agn_regsize(L, idx) == 2)
#define ishyperdual(L,idx) (lua_isreg(L, idx) && agn_isutype(L, idx, AGENA_DUALLIBNAME) && agn_regsize(L, idx) == 4)
#define iseightdual(L,idx) (lua_isreg(L, idx) && agn_isutype(L, idx, AGENA_DUALLIBNAME) && agn_regsize(L, idx) == 8)

#define createdual(L,x,y) { \
  if (isnan((x)) || isnan((y))) { \
    lua_pushundefined(L); \
  } else if (isinf((x))) { \
    lua_pushnumber(L, (x)); \
  } else if (isinf((y))) { \
    lua_pushnumber(L, (y)); \
  } else { \
    agn_createreg(L, 2); \
    agn_regsetinumber(L, -1, 1, (lua_Number)(x)); \
    agn_regsetinumber(L, -1, 2, (lua_Number)(y)); \
    lua_setmetatabletoobject(L, -1, AGENA_DUALLIBNAME, 1); \
  } \
}

#define createhyperdual(L,a,b,c,d) { \
  if (isnan((a)) || isnan((b)) || isnan((c)) || isnan((d))) { \
    lua_pushundefined(L); \
  } else if (isinf((a))) { \
    lua_pushnumber(L, (a)); \
  } else if (isinf((b))) { \
    lua_pushnumber(L, (b)); \
  } else if (isinf((c))) { \
    lua_pushnumber(L, (c)); \
  } else if (isinf((d))) { \
    lua_pushnumber(L, (d)); \
  } else { \
    agn_createreg(L, 4); \
    agn_regsetinumber(L, -1, 1, (lua_Number)(a)); \
    agn_regsetinumber(L, -1, 2, (lua_Number)(b)); \
    agn_regsetinumber(L, -1, 3, (lua_Number)(c)); \
    agn_regsetinumber(L, -1, 4, (lua_Number)(d)); \
    lua_setmetatabletoobject(L, -1, AGENA_DUALLIBNAME, 1); \
  } \
}

#define createeightdual(L,a,b,c,d,e,f,g,h) { \
  if (isnan((a)) || isnan((b)) || isnan((c)) || isnan((d)) || isnan((e)) || isnan((f)) || isnan((g)) || isnan((h))) { \
    lua_pushundefined(L); \
  } else if (isinf((a))) { \
    lua_pushnumber(L, (a)); \
  } else if (isinf((b))) { \
    lua_pushnumber(L, (b)); \
  } else if (isinf((c))) { \
    lua_pushnumber(L, (c)); \
  } else if (isinf((d))) { \
    lua_pushnumber(L, (d)); \
  } else if (isinf((e))) { \
    lua_pushnumber(L, (e)); \
  } else if (isinf((f))) { \
    lua_pushnumber(L, (f)); \
  } else if (isinf((g))) { \
    lua_pushnumber(L, (g)); \
  } else if (isinf((h))) { \
    lua_pushnumber(L, (h)); \
  } else { \
    agn_createreg(L, 8); \
    agn_regsetinumber(L, -1, 1, (lua_Number)(a)); \
    agn_regsetinumber(L, -1, 2, (lua_Number)(b)); \
    agn_regsetinumber(L, -1, 3, (lua_Number)(c)); \
    agn_regsetinumber(L, -1, 4, (lua_Number)(d)); \
    agn_regsetinumber(L, -1, 5, (lua_Number)(e)); \
    agn_regsetinumber(L, -1, 6, (lua_Number)(f)); \
    agn_regsetinumber(L, -1, 7, (lua_Number)(g)); \
    agn_regsetinumber(L, -1, 8, (lua_Number)(h)); \
    lua_setmetatabletoobject(L, -1, AGENA_DUALLIBNAME, 1); \
  } \
}

#define pushzero(L,s) { \
  if (s == 2) { \
    createdual(L, 0.0, 0.0); \
  } else if (s == 4) { \
    createhyperdual(L, 0.0, 0.0, 0.0, 0.0); \
  } else { \
    createeightdual(L, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0); \
  } \
}

#define checkdualorelse(L,f,fn) { \
  if (agn_isdual(L, 1)) { \
    f(L); \
  } else { \
    luaL_nonumorcmplxordual(L, 1, fn); \
  } \
}

/* This macro is actually a hotfix to deal with faulty dual-number C implementation of
   inverse trigonometric and inverse hyperbolic functions which unfortunately return a
   faulty third derivative while all the other functions are working correctly.
   The macro tries to read the respective dual package function, probably overloaded
   via lib/library.agn. The call to agnL_gettablefield() has reserved two slots before
   and pushes null in case of failure. */
#define trydualfix(L,fn,nargs) { \
  if (agn_isdual(L, 1) || ((nargs) == 2 && agn_isdual(L, 2)) ) { \
    if (agnL_gettablefield(L, AGENA_DUALLIBNAME, fn, NULL, 0) == LUA_TFUNCTION) { \
      int i; \
      luaL_checkstack(L, nargs, "not enough stack space"); \
      for (i=0; i < nargs; i++) { \
        lua_pushvalue(L, i + 1); \
      } \
      lua_call(L, nargs, 1); \
      return 1; \
    } else { \
      agn_poptop(L); \
      luaL_error(L, "Error in " LUA_QS ": `dual.%s` is not available.", fn); \
    } \
  } \
}


int INLINE checkdual (lua_State *L, int idx, const char *procname, int toconstant);

int dual_cbrt (lua_State *L);
int dual_pytha (lua_State *L);
int dual_pytha4 (lua_State *L);
int dual_hypot (lua_State *L);
int dual_hypot2 (lua_State *L);
int dual_hypot3 (lua_State *L);
int dual_hypot4 (lua_State *L);

int dual_exp2 (lua_State *L);
int dual_exp10 (lua_State *L);
int dual_expm1 (lua_State *L);
int dual_expx2 (lua_State *L);
int dual_log2 (lua_State *L);
int dual_log10 (lua_State *L);
int dual_lnp1 (lua_State *L);

int dual_csc (lua_State *L);
int dual_sec (lua_State *L);
int dual_cot (lua_State *L);

int dual_csch (lua_State *L);
int dual_sech (lua_State *L);
int dual_coth (lua_State *L);

int dual_cas (lua_State *L);

int dual_arctan2 (lua_State *L);
int dual_arccot (lua_State *L);
int dual_arccsc (lua_State *L);

int dual_arcsinh (lua_State *L);
int dual_arccosh (lua_State *L);
int dual_arctanh (lua_State *L);
int dual_arcsech (lua_State *L);
int dual_arccoth (lua_State *L);
int dual_arccsch (lua_State *L);

int dual_cosc (lua_State *L);
int dual_tanc (lua_State *L);

int dual_erf (lua_State *L);
int dual_erfc (lua_State *L);
int dual_erfcx (lua_State *L);
int dual_inverf (lua_State *L);
int dual_inverfc (lua_State *L);

int dual_gamma (lua_State *L);
int dual_beta (lua_State *L);
int dual_psi (lua_State *L);

#endif

