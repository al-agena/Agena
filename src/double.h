#ifndef double_h
#define double_h

#include "sunpro.h"
#include "long.h"

#define checkdouble(L, n) (DLong *)luaL_checkudata(L, n, "doublevalue")

#define getdoublevalue(L,idx) (((DLong *)lua_touserdata(L, (idx)))->value)

#define checkandgetdouble(L,idx) (((DLong *)luaL_checkudata(L, (idx), "doublevalue"))->value)

#define checkandgetdoublenum(L,idx) ({ \
  longdouble __x = (agn_isnumber(L, (idx))) ? agn_tonumber(L, (idx)) : (checkandgetdouble(L, (idx))); \
  (__x); \
})

#define createdouble(L,x) { \
  DLong *__d = (DLong *)lua_newuserdata(L, sizeof(DLong)); \
  lua_setmetatabletoobject(L, -1, "doublevalue", 1); \
  __d->value = (x); \
}

#endif
