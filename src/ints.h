#ifndef ints_h
#define ints_h

#define checkints(L,n)        (Ints *)luaL_checkudata(L, n, AGENA_INTSLIBNAME)
#define toints(L,n)           (Ints *)lua_touserdata(L, n)
#define isints(L,n)           (luaL_isudata(L, n, AGENA_INTSLIBNAME) && agn_isutypeset(L, n))

typedef struct Ints {
  union {
    int64_t i;      /* signed 64-bit integer */
    uint64_t ui;    /* unsigned 64-bit integer */
  } data;
  char datatype;    /* 0 = uint64_t, 1 = int64_t */
  char padding[7];  /* explicit padding (peclaiming the 7-byte tail) */
} Ints;

/*               0       1   */
typedef enum { UINT64T, INT64T } inttype;

#define getuint64value(L,idx) (((Ints *)lua_touserdata(L, (idx)))->data.ui)
#define getint64value(L,idx)  (((Ints *)lua_touserdata(L, (idx)))->data.i)

void createuint64 (lua_State *L, uint64_t value);
void createint64 (lua_State *L, int64_t value);

#define agnL_checkuint64(L,idx,procname) ({ \
  uint64_t __x = 0ULL; \
  if (agn_isnonnegint(L, (idx))) { \
    __x = agn_tonumber(L, (idx)); \
  } else if (luaL_isudata(L, idx, "ints")) { \
    Ints *__b = (Ints *)lua_touserdata(L, idx); \
    if (__b->datatype != UINT64T) { \
      if (idx < 0) agn_poptop(L); \
      luaL_error(L, "Error in " LUA_QS ": unsigned 64-bit integer expected.", procname); \
    } \
    __x = __b->data.ui;\
  } else { \
    if (idx < 0) lua_remove(L, idx); \
    luaL_error(L, "Error in " LUA_QS ": unsigned 64-bit integer or number expected, got %s.", procname, luaL_typename(L, idx)); \
  } \
  (__x); \
})

#define agnL_checkint64(L,idx,procname) ({ \
  int64_t __x = 0LL; \
  if (agn_isinteger(L, (idx))) { \
    __x = agn_tonumber(L, (idx)); \
  } else if (luaL_isudata(L, idx, "ints")) { \
    Ints *__b = (Ints *)lua_touserdata(L, idx); \
    if (__b->datatype != INT64T) { \
      if (idx < 0) agn_poptop(L); \
      luaL_error(L, "Error in " LUA_QS ": signed 64-bit integer expected.", procname); \
    } \
    __x = __b->data.i;\
  } else { \
    if (idx < 0) lua_remove(L, idx); \
    luaL_error(L, "Error in " LUA_QS ": signed 64-bit integer or number expected, got %s.", procname, luaL_typename(L, idx)); \
  } \
  (__x); \
})

#endif
