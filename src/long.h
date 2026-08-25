#ifndef long_h
#define long_h

#include "lstate.h"
#include "sunpro.h"
#include "ints.h"

#ifndef longdouble
#define longdouble long double
#endif

#ifndef __ARMCPU
#ifndef DLong
#define DLong ieee_long_double_shape_type
#endif
#else
#ifndef DLong
#define DLong ieee_double_shape_type
#endif
#endif

#define checkdlong(L,n)   (DLong *)luaL_checkudata(L, n, "longdouble")
#define isdlong(L,n)      (luaL_isudata(L, n, "longdouble"))

#define getdlongvalue(L,idx) (((DLong *)lua_touserdata(L, (idx)))->value)

#define checkandgetdlong(L,idx) (((DLong *)luaL_checkudata(L, (idx), "longdouble"))->value)

#define checkandgetdlongnum(L,idx) ({ \
  longdouble __x = (agn_isnumber(L, (idx))) ? agn_tonumber(L, (idx)) : (checkandgetdlong(L, (idx))); \
  (__x); \
})

#define agnL_checkdlongnum(L,idx,procname) ({ \
  longdouble __x; \
  if (agn_isnumber(L, (idx))) { \
    __x = agn_tonumber(L, (idx)); \
  } else if (luaL_isudata(L, idx, "longdouble")) { \
    __x = getdlongvalue(L, (idx)); \
  } else { \
    int _typename; \
    __x = 0.0L; \
    _typename = lua_type(L, idx); \
    if (idx < 0) lua_remove(L, idx); \
    luaL_error(L, "Error in " LUA_QS ": number or longdouble expected, got %s.", procname, lua_typename(L, _typename)); \
  } \
  (__x); \
})

#define createdlong(L,x) { \
  DLong *__d = (DLong *)lua_newuserdata(L, sizeof(DLong)); \
  lua_setmetatabletoobject(L, -1, "longdouble", 1); \
  __d->value = (x); \
}


#ifndef __ARMCPU

#define tolong(L,idx,ds)  (lua_isnumber(L, (idx)) ? ds.d : ds.f)

#define checkandgetdlongint(L,idx) ({ \
  longdouble __x = (agn_isnumber(L, (idx))) ? agn_tonumber(L, (idx)) : (checkandgetdlong(L, (idx))); \
  (sun_intl(__x)); \
})

LUALIB_API long double str_strtold (const char *s, char **p);

#ifndef PROPCMPLX
#define getanynumber(L,d,idx,procname) { \
  d.i64 = 0; \
  d.ui64 = 0; \
  d.d = AGN_NAN; \
  d.z = AGN_NAN; \
  d.f = AGN_NAN; \
  if (agn_isnumber(L, (idx))) { \
    lua_Number x = agn_tonumber(L, (idx)); \
    d.z = x + 0*I; \
    d.d = x; \
  } else if (lua_iscomplex(L, (idx))) { \
    d.z = agn_tocomplex(L, (idx)); \
  } else if (luaL_isudata(L, idx, "longdouble")) { \
    d.f = getdlongvalue(L, (idx)); \
  } else if (luaL_isudata(L, idx, "ints")) { \
    Ints *a = lua_touserdata(L, idx); \
    if (a->datatype == INT64T) { \
      d.i64 = a->data.i; \
    } else { \
      d.ui64 = a->data.ui; \
    } \
  } else { \
    luaL_error(L, "Error in " LUA_QS ": need (complex) number, longdouble, 64-bit integer, got %s.", procname, luaL_typename(L, idx)); \
  } \
}

#else

#define tolong(L,idx,ds)  (lua_isnumber(L, (idx)) ? ds.d : ds.f)

#define checkandgetdlongint(L,idx) ({ \
  longdouble __x = (agn_isnumber(L, (idx))) ? agn_tonumber(L, (idx)) : (checkandgetdlong(L, (idx))); \
  (sun_intl(__x)); \
})

#define getanynumber(L,d,idx,procname) { \
  d.i64 = 0; \
  d.ui64 = 0; \
  d.d = AGN_NAN; \
  d.f = AGN_NAN; \
  d.z[0] = AGN_NAN; \
  d.z[1] = AGN_NAN; \
  if (agn_isnumber(L, (idx))) { \
    lua_Number x = agn_tonumber(L, (idx)); \
    d.z[0] = x; \
    d.z[1] = 0; \
    d.d = x; \
  } else if (agn_iscomplex(L, (idx))) { \
    int rc; \
    lua_Number z[2]; \
    agn_tocomplexx(L, idx, &rc, z); \
    d.z[0] = z[0]; \
    d.z[1] = z[1]; \
  } else if (luaL_isudata(L, idx, "ints")) { \
    Ints *a = lua_touserdata(L, idx); \
    if (a->datatype == INT64T) { \
      d.i64 = a->data.i; \
    } else { \
      d.ui64 = a->data.ui; \
    } \
  } else { \
    luaL_error(L, "Error in " LUA_QS ": (complex) number or 64-bit integer expected, got %s.", procname, luaL_typename(L, idx)); \
  } \
}
#endif  /* of PROPCMPLX */

#else  /* now ARM part */

#define tolong(L,idx,ds)  (lua_isnumber(L, (idx)) ? ds.d : ds.f)

#define checkandgetdlongint(L,idx) ({ \
  longdouble __x = (agn_isnumber(L, (idx))) ? agn_tonumber(L, (idx)) : (checkandgetdlong(L, (idx))); \
  (sun_intl(__x)); \
})

#ifndef PROPCMPLX
#define getanynumber(L,d,idx,procname) { \
  d.d = AGN_NAN; \
  d.z = AGN_NAN; \
  d.i64 = 0; \
  d.ui64 = 0; \
  if (agn_isnumber(L, (idx))) { \
    lua_Number x = agn_tonumber(L, (idx)); \
    d.z = x + 0*I; \
    d.d = x; \
  } else if (agn_iscomplex(L, (idx))) { \
    d.z = agn_tocomplex(L, (idx)); \
  } else if (luaL_isudata(L, idx, "ints")) { \
    Ints *a = lua_touserdata(L, idx); \
    if (a->datatype == INT64T) { \
      d.i64 = a->data.i; \
    } else { \
      d.ui64 = a->data.ui; \
    } \
  } else { \
    luaL_error(L, "Error in " LUA_QS ": (complex) number expected, got %s.", procname, luaL_typename(L, idx)); \
  } \
}
#else
#define getanynumber(L,d,idx,procname) { \
  d.i64 = 0; \
  d.ui64 = 0; \
  d.d = AGN_NAN; \
  d.z[0] = AGN_NAN; \
  d.z[1] = AGN_NAN; \
  if (agn_isnumber(L, (idx))) { \
    lua_Number x = agn_tonumber(L, (idx)); \
    d.z[0] = x; \
    d.z[1] = 0; \
    d.d = x; \
  } else if (agn_iscomplex(L, (idx))) { \
    int rc; \
    lua_Number z[2]; \
    agn_tocomplexx(L, idx, &rc, z); \
    d.z[0] = z[0]; \
    d.z[1] = z[1]; \
  } else if (luaL_isudata(L, idx, "ints")) { \
    Ints *a = lua_touserdata(L, idx); \
    d.z[0] = AGN_NAN; \
    d.z[1] = AGN_NAN; \
    if (a->datatype == INT64T) { \
      d.i64 = a->data.i; \
    } else { \
      d.ui64 = a->data.ui; \
    } \
  } else { \
    luaL_error(L, "Error in " LUA_QS ": (complex) number expected, got %s.", procname, luaL_typename(L, idx)); \
}
#endif  /* of PROPCMPLX */

#endif  /* of __ARMCPU */

#endif
