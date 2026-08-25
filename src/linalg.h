#ifndef linalg_h
#define linalg_h

/* creates an array a with size n, FREE it ! */
#define la_createarray(L, a, n, procname) { \
  if ((n) < 1) \
    luaL_error(L, "Error in " LUA_QS ": table or sequence with at least one entry expected.", (procname)); \
  (a) = malloc((n)*sizeof(lua_Number)); \
  if ((a) == NULL) \
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", (procname)); \
}

void la_getrange  (lua_State *L, int idx, int nargs, int *a, int *b, int m, const char *procname);
void la_transpose (lua_Number *a, int n);
void numal_ichcol (lua_State *L, lua_Number *a, int n, int l, int u, int i, int j);
void numal_ichrow (lua_State *L, lua_Number *a, int n, int l, int u, int i, int j);
int  fillmatrixld (lua_State *L, int idx, long double *a, int m, int n, int gaps, const char *procname);

#endif