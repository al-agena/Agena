/* Tuple Library, based on code written by Roberto Ierusalimschy, and published on Lua-list on March 25, 2004. Initiated 16/11/2022.
   See: http://lua-users.org/lists/lua-l/2004-03/msg00353.html - `Re: cons cells for Lua`
   "To add more fun to the discussion, follows a small C module that adds tuples to Lua. In some quick tests, they outperform tables
   both in space and creation time, but are slower to access.

   Manual:

   a := tuples.new(v1, v2, ..., vn) --> creates a new tuple
   a(i)                            --> returns vi
   a()                             --> returns v1, v2, ... vn

   -- Roberto" */

#include <stdio.h>
#include <string.h>

#define tuples_c
#define LUA_LIB

#include "agena.h"
#include "agnxlib.h"
#include "agenalib.h"
#include "agncmpt.h"
#include "agnhlps.h"
#include "prepdefs.h"  /* FORCE_INLINE */
#include "llex.h"      /* llex_token2str */

#if !(defined(__DJGPP__) || defined(__OS2__) || defined(LUA_ANSI))
#define AGENA_TUPLESLIBNAME "tuples"
LUALIB_API int (luaopen_tuples) (lua_State *L);
#endif

#define AGENA_TUPLESLIBNAMELEN  6
#define TUPLETYPENAME       "tuple"

#define istuple(L, idx) (lua_isfunction(L, idx) && agn_isutypeset(L, idx))

static FORCE_INLINE lu_byte checktuple (lua_State *L, int idx, const char *procname, int strict) {
  lu_byte nupvals;
  if (!(istuple(L, idx))) {
    luaL_error(L, "Error in " LUA_QS ": expected a tuple, got %s.",
      procname, luaL_typename(L, idx));
  }
  nupvals = lua_nupvalues(L, idx);
  if (strict && nupvals == 0)
    luaL_error(L, "Error in " LUA_QS ": tuple is empty.", procname);
  return nupvals;
}


static int tuple (lua_State *L) {
  int a = luaL_optint(L, 1, 0);
  if (a == 0) {  /* return all elements? */
    int i;
    for (i=1; lua_type(L, lua_upvalueindex(i)) != LUA_TNONE; i++)
      lua_pushvalue(L, lua_upvalueindex(i));
    return i - 1;
  } else {
    if (a < 0) {  /* return |a|-th field from the right */
      int i;
      for (i=1; lua_type(L, lua_upvalueindex(i)) != LUA_TNONE; i++);
      a = tools_posrelat(a, i - 1);
    }
    luaL_argcheck(L, 1 <= a && lua_type(L, lua_upvalueindex(a)) != LUA_TNONE,
                     1, "invalid index");
    lua_pushvalue(L, lua_upvalueindex(a));
    return 1;
  }
}


/* Creates a new tuple. It receives none, one or more values and returns a factory that when called returns the given values. e.g.

   > t := tuples.tuple(10, 20, 30);
   t():  # return all values
   10      20      30

   > t(1), t(2), t(3):  # return the values at indices 1, 2 and 3
   10      20      30

   The latter is equivalent to:

   > t[1], t[2], t[3]:
   10      20      30

   See also: tuples.getitem, tuples.setitem. */

/* Expects tuple at the stack top */

static int tuples_iterate (lua_State *L);

/* the tuple must be on the stack top */
static void aux_additerator (lua_State *L) {
  luaL_checkstack(L, 7, "not enough stack space");
  lua_getmetatable(L, -1);
  lua_pushcfunction(L, &tuples_iterate);  /* push tuples.iterate */
  lua_pushvalue(L, -3);   /* push tuple */
  lua_pushinteger(L, 1);  /* starting position */
  lua_pushinteger(L, 1);  /* step size */
  lua_pushtrue(L);        /* retrieve both index and value */
  lua_call(L, 4, 1);      /* create iterator */
  lua_setfield(L, -2, "__iter");  /* assign to metatable */
  agn_poptop(L);  /* pop metatable and leave tuple on the stack top, to returned */
}

static int tuples_new (lua_State *L) {
  int n = lua_gettop(L);
  if (n > LUAI_MAXUPVALUES || (n > 0 && !lua_checkstack(L, n)))  /* 2.39.6 change, allow empty tuples */
    luaL_error(L, "Error in " LUA_QS ": too many or too few arguments.", "tuples.tuple");
  lua_pushcclosure(L, tuple, n);
  lua_setmetatabletoobject(L, -1, AGENA_TUPLESLIBNAME, 0);  /* 2.39.6 fix */
  agn_setutypestring(L, -1, TUPLETYPENAME);  /* 2.39.6 fix */
  aux_additerator(L);  /* add iterator, 7.1.0 */
  return 1;
}


static int tuples_getsize (lua_State *L) {
  lua_pushinteger(L, checktuple(L, 1, "tuples.getsize", 0));
  return 1;
}


static int tuples_tostring (lua_State *L) {  /* at the console, the tuple is formatted as follows: */
  int nops = checktuple(L, 1, "tuples.tostring", 0);
  if (agn_getutype(L, 1)) {
    lua_pushfstring(L, "(%u)", nops);
    lua_concat(L, 2);
  } else
    luaL_error(L, "Error in " LUA_QS ": invalid tuple.", "tuples.tostring");
  return 1;
}


/* tuples.getitem (t, i)
With any tuple, returns the value at a[i], where i, the index, is an integer counting from 1. The function has been provided
to avoid the index metamethod overhead.

See also: tuples.setitem. */

static int tuples_getitem (lua_State *L) {
  int a, nops, nargs = lua_gettop(L);
  nops = checktuple(L, 1, "tuples.getitem", 1);
  if (nargs == 2 && !agn_isinteger(L, 2)) {  /* OOP method, 5.5.15 */
    return agn_initmethodcall(L, AGENA_TUPLESLIBNAME, AGENA_TUPLESLIBNAMELEN);
  }
  a = agn_checkinteger(L, 2);
  if (a == 0) {  /* return all elements? */
    luaL_error(L, "Error in " LUA_QS ": index must be non-zero.", "tuples.getitem");
    return 0;
  } else {
    a = tools_posrelat(a, nops);
    luaL_argcheck(L, a > 0 && a <= nops, 1, "invalid index");
    lua_getupvalue(L, 1, a);
    return 1;
  }
}


/* tuples.setitem (t, i, v)
With any tuple, sets value v to a[i], where i, the index, is an integer counting from 1. The function has been provided
to avoid the index metamethod overhead.

See also: tuples.getitem. */

static int tuples_setitem (lua_State *L) {
  int a, nops;
  if (lua_gettop(L) != 3)
    luaL_error(L, "Error in " LUA_QS ": expected three arguments.", "tuples.setitem");
  nops = checktuple(L, 1, "tuples.setitem", 1);
  a = tools_posrelat(agn_checkinteger(L, 2), nops);
  luaL_argcheck(L, a > 0 && a <= nops, 1, "invalid index");
  lua_getupvalue(L, 1, a);
  lua_pushvalue(L, 3);
  lua_setupvalue(L, 1, a);
  return 1;
}


/* Taken from lbaselib.c; check options for map, select, remove, subs; externalised 2.31.5 */
static void aux_fcheckoptions (lua_State *L, int *nargs, int *newstruct, int *multipass, int *equality, int *reshuffle, const char *procname) {
  int checkoptions;
  *newstruct = 1;  /* returns a new structure, do not work in-place */
  *multipass = 1;  /* 1 = always substitute at each pass, 0 = substitute a position only once */
  *equality = 1;   /* 1 = strict equality, 0 = approximate equality */
  *reshuffle = 0;   /* 0 = leave holes in return as they are, 1 = remove holes and return an array */
  /* check for options, here `map in-place` */
  if (*nargs > 2 && lua_istrue(L, *nargs)) {
    (*nargs)--; *newstruct = 0;
  }
  checkoptions = 4;  /* check n options; CHANGE THIS if you add/delete options, 2.29.0 */
  if (*nargs > 2 && lua_ispair(L, *nargs))  /* 3.15.2 fix */
    luaL_checkstack(L, 2, "not enough stack space");
  while (checkoptions-- && *nargs > 2 && lua_ispair(L, *nargs)) {
    agn_pairgeti(L, *nargs, 1);  /* get left value, set to stack index -2 */
    agn_pairgeti(L, *nargs, 2);  /* get right value, set to stack index  -1 */
    if (agn_isstring(L, -2)) {
      const char *option = agn_tostring(L, -2);
      if (tools_streq("inplace", option)) {
        *newstruct = !agn_checkboolean(L, -1);
      } else if (tools_streq("multipass", option)) {
        *multipass = agn_checkboolean(L, -1);
      } else if (tools_streq("strict", option)) {
        *equality = agn_checkboolean(L, -1);
      } else if (tools_streq("reshuffle", option)) {
        *reshuffle = agn_checkboolean(L, -1);
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

static int tuples_map (lua_State *L) {
  int i, j, nargs, nops, newstruct, multipass, equality, reshuffle;
  nargs = lua_gettop(L);
  if (nargs < 2)
    luaL_error(L, "Error in " LUA_QS ": expected at least two arguments.", "tuples.map");
  luaL_checktype(L, 1, LUA_TFUNCTION);
  nops = checktuple(L, 2, "tuples.map", 1);
  aux_fcheckoptions(L, &nargs, &newstruct, &multipass, &equality, &reshuffle, "tuples.map");
  if (newstruct) {
    luaL_checkstack(L, nops > nargs ? nops : nargs, "not enough stack space");
    for (i=1; i <= nops; i++) lua_getupvalue(L, 2, i);
    lua_pushcclosure(L, tuple, nops);  /* create new tuple */
    luaL_setmetatype(L, 2);  /* 3.15.5 change */
    aux_additerator(L);  /* 7.1.1 fix */
  } else {
    luaL_checkstack(L, 1, "too many arguments");  /* 3.15.5 fix */
    lua_pushvalue(L, 2);  /* push existing tuple */
  }
  for (i=1; i <= nops; i++) {
    luaL_checkstack(L, nargs, "not enough stack space");  /* 3.5.5 fix */
    lua_pushvalue(L, 1);        /* push function */
    lua_getupvalue(L, -2, i);   /* push i-th upvalue */
    for (j=3; j <= nargs; j++)  /* push optional function arguments */
      lua_pushvalue(L, j);
    lua_call(L, nargs - 1, 1);
    lua_setupvalue(L, -2, i);   /* pops result of the function call */
  }
  return 1;
}


/* tuples.subs (x:v [, ···], t [, true])
Substitutes all occurrences of the value x in tuple t with value v. Multiple substitution pairs can be given. The substitutions are performed sequentially and by default simultaneously starting with the first pair.

> t := tuples.subs(1:3, 2:4, tuples.new(1, 2, -1)):
returns a tuple with values 3, 4, -1.

If the last argument is the option inplace=true, or the Boolean true, then the operation will be done in-place, modifying the original tuple, but saving memory. After completion, the function returns the modified tuple.

By default, tuples.subs is replacing the elements in a structure with _all_ the replacements given, including intermediate substitutions. So if we have an expression like

> t := tuples.subs(1:2, 2:3, 3:4, tuples.new(1, 2, 3))

we will get a tuple with values 4, 4, 4.

By passing the multipass=false option, the rest of the replacement list will be skipped as soon as a substitution has been done:

> t := tuples.subs(1:2, 2:3, 3:4, tuples.new(1, 2, 3), multipass=false):

returns a new tuple with values 2, 3, 4.

You can check numbers for approximate instead of strict equality by passing the strict=false option.

See also: tuples.map, tuples.remove, tuples.select. */

static int tuples_subs (lua_State *L) {
  int i, k, flag, nargs, nops, newstruct, multipass, equality, reshuffle;
  nargs = lua_gettop(L);
  aux_fcheckoptions(L, &nargs, &newstruct, &multipass, &equality, &reshuffle, "tuples.subs");
  for (i=1; i < nargs; i++)
    luaL_checktype(L, i, LUA_TPAIR);
  nops = checktuple(L, nargs, "tuples.subs", 1);
  if (newstruct) {
     luaL_checkstack(L, nops + 1 + 2, "not enough stack space");  /* 3.15.5 change */
     for (i=1; i <= nops; i++) lua_getupvalue(L, nargs, i);
     lua_pushcclosure(L, tuple, nops);  /* create new tuple */
     luaL_setmetatype(L, 2);  /* 3.15.5 change */
     aux_additerator(L);  /* 7.1.1 fix */
  } else {
    luaL_checkstack(L, 1 + 2, "not enough stack space");  /* 3.15.5 change */
    lua_pushvalue(L, nargs);
  }
  /* start traversal */
  for (k=1; k <= nops; k++) {
    lua_getupvalue(L, nargs, k);
    flag = 0;
    for (i=1; i < nargs; i++) {
      agn_pairgeti(L, i, 1);
      if (equality ? lua_equal(L, -1, -2) : lua_rawaequal(L, -1, -2)) {
        agn_poptoptwo(L);  /* delete value and lhs */
        agn_pairgeti(L, i, 2);  /* push rhs */
        flag = 1;
        if (!multipass) break;
      } else {
        agn_poptop(L);  /* delete lhs */
      }
    }
    if (!flag && !newstruct)  /* replace only if needed */
      agn_poptop(L);
    else
      lua_setupvalue(L, -2, k);  /* store result in tuple */
  }
  return 1;
}


/* tuples.select (f, t [, ··· ])
Returns all values in tuple t that satisfy a condition determined by function f in a new tuple.

If f has only one argument, then only the function and the tuple are passed to tuples.select.

> t := tuples.select(<< x -> x > 1 >>, tuples.new(1, 2, 3)):
returns a new tuple including the value 2 and 3.

If the function has more than one argument, then all arguments except the first are passed right after the name of t.

> t := tuples.select(<< x, y -> x > y >>, tuples.new(1, 2, 3), 1):   # 1 for parameter y
returns a new tuple including the value 2 and 3.

See also: tuples.map, tuples.remove, tuples.subs. */

static int tuples_select (lua_State *L) {
  size_t i, j, c;
  int nargs, nops;
  luaL_checktype(L, 1, LUA_TFUNCTION);
  nargs = lua_gettop(L);
  nops = checktuple(L, 2, "tuples.select", 1);
  /* very rough guess whether table is an array or dictionary */
  c = 0;
  /* start traversal */
  for (i=1; i <= nops; i++) {
    luaL_checkstack(L, nargs, "not enough stack space");  /* 3.15.4 fix */
    lua_pushvalue(L, 1);
    lua_getupvalue(L, 2, i);
    for (j=3; j <= nargs; j++)
      lua_pushvalue(L, j);
    lua_call(L, nargs - 1, 1);  /* call function with nargs-1 argument(s) and one result */
    if (lua_istrue(L, -1)) {  /* 5.0.1 fix */
      agn_poptop(L);
      lua_getupvalue(L, 2, i);
      c++;
    } else {  /* in-place operation: delete non-satisfying value from table */
      agn_poptop(L);  /* delete result */
    }
  }
  luaL_checkstack(L, 1, "not enough stack space");
  lua_pushcclosure(L, tuple, c);  /* create new tuple */
  luaL_setmetatype(L, 2);  /* 3.15.5 change */
  aux_additerator(L);  /* 7.1.1 fix */
  return 1;
}


/* tuples.remove (f, t [, ··· ])
Returns all values in tuple t that do not satisfy a condition determined by function f, as a new tuple.

If the function has only one argument, then only the function and the tuple are passed to tuples.remove.

> tuples.remove(<< x -> x > 1 >>, tuples.new(1, 2, 3));
returns a new tuple including the value 1 only.

If the function has more than one argument, then all arguments except the first are
passed right after the name of t.

> tuples.remove(<< x, y -> x > y >>, tuples.new(1, 2, 3), 1): # 1 for parameter y
returns a new tuple including the value 1 only.

See also: tuples.map, tuples.select, tuples.subs. */

static int tuples_remove (lua_State *L) {
  size_t i, j, c;
  int nargs, nops;
  luaL_checktype(L, 1, LUA_TFUNCTION);
  nargs = lua_gettop(L);
  nops = checktuple(L, 2, "tuples.remove", 1);
  /* very rough guess whether table is an array or dictionary */
  c = 0;
  /* start traversal */
  for (i=1; i <= nops; i++) {
    luaL_checkstack(L, nargs, "not enough stack space");  /* 3.15.4 fix */
    lua_pushvalue(L, 1);
    lua_getupvalue(L, 2, i);
    for (j=3; j <= nargs; j++)
      lua_pushvalue(L, j);
    lua_call(L, nargs - 1, 1);  /* call function with nargs-1 argument(s) and one result */
    if (lua_istrue(L, -1)) {  /* 5.0.1 fix */
      agn_poptop(L);  /* delete result */
    } else {  /* in-place operation: delete non-satisfying value from table */
      agn_poptop(L);
      lua_getupvalue(L, 2, i);
      c++;
    }
  }
  luaL_checkstack(L, 1, "not enough stack space");  /* 3.15.4 fix */
  lua_pushcclosure(L, tuple, c);  /* create new tuple */
  luaL_setmetatype(L, 2);  /* 3.15.5 change */
  aux_additerator(L);  /* 7.1.1 fix */
  return 1;
}


/* Receives tuple t and puts its contents into a table, the return. */
static int tuples_totable (lua_State *L) {
  int i, nops;
  nops = checktuple(L, 1, "tuples.totable", 0);
  lua_createtable(L, nops, 0);
  for (i=1; i <= nops; i++) {
    lua_getupvalue(L, 1, i);
    lua_rawseti(L, -2, i);
  }
  return 1;
}


/* Receives tuple t and puts its contents into a sequence, the return. */
static int tuples_toseq (lua_State *L) {
  int i, nops;
  nops = checktuple(L, 1, "tuples.toseq", 0);
  agn_createseq(L, nops);
  for (i=1; i <= nops; i++) {
    lua_getupvalue(L, 1, i);
    lua_seqrawseti(L, -2, i);
  }
  return 1;
}


/* Receives tuple t and puts its contents into a register, the return. */
static int tuples_toreg (lua_State *L) {
  int i, nops;
  nops = checktuple(L, 1, "tuples.toreg", 0);
  agn_createreg(L, nops);
  for (i=1; i <= nops; i++) {
    lua_getupvalue(L, 1, i);
    agn_regrawseti(L, -2, i);
  }
  return 1;
}


/* Receives tuple t and dumps its contents. 2.39.6 */
static int tuples_unpack (lua_State *L) {
  int i, nops;
  nops = checktuple(L, 1, "tuples.unpack", 0);
  luaL_checkstack(L, nops, "not enough stack space");
  for (i=1; i <= nops; i++) {
    lua_getupvalue(L, 1, i);
  }
  return nops;
}


#define aux_isofbasictype(L,nops,fn) { \
  int i, rc; \
  rc = 1; \
  for (i=1; i <= nops && rc; i++) { \
    lua_getupvalue(L, 1, i); \
    rc = fn(L, -1); \
    agn_poptop(L); \
  } \
  lua_pushboolean(L, nops && rc); \
}

static FORCE_INLINE int aux_isofnumtype (lua_State *L, int nops, int (*fn)(double)) {
  int i, rc;
  rc = 1;
  for (i=1; i <= nops && rc; i++) {
    lua_getupvalue(L, 1, i);
    rc = agn_isnumber(L, -1) && fn(agn_tonumber(L, -1));
    agn_poptop(L);
  }
  lua_pushboolean(L, nops && rc);
  return 1;
}

static int aux_isint (double x) {
  return tools_isint(x);
}

static int tuples_isall (lua_State *L) {
  int nops;
  const char *type;
  nops = checktuple(L, 1, "tuples.isall", 0);
  type = agn_checkstring(L, 2);
  if (tools_streq(type, llex_token2str(TK_TNUMBER))) {
    aux_isofbasictype(L, nops, agn_isnumber);
  } else if (tools_streq(type, llex_token2str(TK_INTEGER))) {
    aux_isofnumtype(L, nops, aux_isint);
  } else if (tools_streq(type, llex_token2str(TK_TCOMPLEX))) {
    aux_isofbasictype(L, nops, lua_iscomplex);
  } else if (tools_streq(type, llex_token2str(TK_TSTRING))) {
    aux_isofbasictype(L, nops, agn_isstring);
  } else if (tools_streq(type, llex_token2str(TK_TBOOLEAN))) {
    aux_isofbasictype(L, nops, lua_isboolean);
  } else if (tools_streq(type, llex_token2str(TK_POSINT))) {
    aux_isofnumtype(L, nops, tools_isposint);
  } else if (tools_streq(type, llex_token2str(TK_POSITIVE))) {
    aux_isofnumtype(L, nops, tools_ispositive);
  } else if (tools_streq(type, llex_token2str(TK_NONNEGINT))) {
    aux_isofnumtype(L, nops, tools_isnonnegint);
  } else if (tools_streq(type, llex_token2str(TK_NONZEROINT))) {  /* 4.11.0 */
    aux_isofnumtype(L, nops, tools_isnonzeroint);
  } else if (tools_streq(type, llex_token2str(TK_NONNEGATIVE))) {
    aux_isofnumtype(L, nops, tools_isnonnegative);
  } else
    luaL_error(L, "Error in " LUA_QS ": unsupported type " LUA_QS ".", "tuples.isall", type);
  return 1;
}


static int tuples_member (lua_State *L) {  /* 7.0.3 */
  int i, rc, nops, approximate, isnum;
  lua_Number x, y, eps;
  nops = checktuple(L, 2, "tuples.member", 0);
  eps = agnL_optpositive(L, 3, 0.0);
  approximate = eps != 0.0;
  rc = 0;
  isnum = agn_isnumber(L, 1);
  x = (isnum) ? agn_tonumber(L, 1) : 0.0;
  for (i=1; (i <= nops) && !rc; i++) {
    lua_getupvalue(L, 2, i);
    if (isnum && agn_isnumber(L, -1)) {
      y = agn_tonumber(L, -1);
      rc = (approximate) ? tools_approx(x, y, eps) : x == y;
    } else {
      rc = lua_equal(L, -1, 1);
    }
    agn_poptop(L);
  }
  if (!rc)
    lua_pushnil(L);
  else
    lua_pushinteger(L, i - 1);
  return 1;
}


static int iterate (lua_State *L) {
  size_t pos, step, max, pushidx;
  /* 7.1.1 fix: `dynamically` retrieve the number of elements in the tuple at `run-time`. */
  max = lua_nupvalues(L, lua_upvalueindex(1));
  pos = lua_tointeger(L, lua_upvalueindex(2));
  step = lua_tointeger(L, lua_upvalueindex(3));
  pushidx = lua_tointeger(L, lua_upvalueindex(4));
  if (pushidx != 0) {
    luaL_checkstack(L, 1, "not enough stack space");
    lua_pushinteger(L, pos);
  }
  if (pos > max) {
    if (pushidx != 0) {
      lua_pushinteger(L, pushidx);
      lua_replace(L, lua_upvalueindex(2));
      pushidx = 0;
    }
    lua_pushnil(L);
  }
  else {
    lua_getupvalue(L, lua_upvalueindex(1), pos);
    lua_pushinteger(L, pos + step);
    lua_replace(L, lua_upvalueindex(2));
  }
  return 1 + (pushidx != 0);
}

static int tuples_iterate (lua_State *L) {
  int64_t pos, step;
  int nops, pushidx;
  nops = checktuple(L, 1, "tuples.iterate", 0);
  pos = tools_posrelat(agnL_optinteger(L, 2, 1), nops);
  step = agnL_optposint(L, 3, 1);
  pushidx = agnL_optboolean(L, 4, 0);  /* 7.1.0 */
  luaL_checkstack(L, 3, "not enough stack space");
  lua_pushvalue(L, 1);
  lua_pushinteger(L, pos);  /* we iterate from index position 1, not 0 */
  lua_pushinteger(L, step);
  lua_pushinteger(L, pushidx ? pos : 0);
  lua_pushcclosure(L, &iterate, 4);
  return 1;
}


static void tupleeq (lua_State *L, int (*fn)(lua_Number, lua_Number, lua_Number), const char *procname) {  /* based on the linalg package (function meq) */
  int i, flag, xn, yn;
  lua_Number eps;
  xn = checktuple(L, 1, procname, 1);
  yn = checktuple(L, 2, procname, 1);
  eps = agn_getepsilon(L);
  flag = xn == yn;
  luaL_checkstack(L, 2, "not enough stack space");
  for (i=1; i <= xn && flag; i++) {
    lua_getupvalue(L, 1, i);
    lua_getupvalue(L, 2, i);
    if (agn_isnumber(L, -1) && agn_isnumber(L, -2))
      flag = fn(agn_tonumber(L, -1), agn_tonumber(L, -2), eps);
    else
      flag = lua_equal(L, -1, -2);
    agn_poptoptwo(L);
  }
  lua_pushboolean(L, flag);
}

static int equal (lua_Number x, lua_Number y, lua_Number eps) {  /* strict equality, eps does not matter */
  return x == y;
}

static int mt_aeq (lua_State *L) {
  tupleeq(L, tools_approx, "tuples.__aeq");
  return 1;
}


static int mt_eeq (lua_State *L) {
  tupleeq(L, equal, "tuples.__(e)eq");
  return 1;
}


static int mt_empty (lua_State *L) {
  lua_pushboolean(L, checktuple(L, 1, "empty", 0) == 0);
  return 1;
}


static int mt_filled (lua_State *L) {
  lua_pushboolean(L, checktuple(L, 1, "filled", 0) != 0);
  return 1;
}


/* Checks whether all elements in the tuple are zeros and returns `true` or `false`. */
static int mt_zero (lua_State *L) {
  int i, rc, nops;
  nops = checktuple(L, 1, "zero", 1);
  rc = 1;
  for (i=1; (i <= nops) && rc; i++) {
    lua_getupvalue(L, 1, i);
    rc = agn_isnumber(L, -1) && agn_tonumber(L, -1) == 0;
    agn_poptop(L);
  }
  lua_pushboolean(L, nops && rc);
  return 1;
}


/* Checks whether at least one element in the tuple is non-zero and returns `true` or `false`. */
static int mt_nonzero (lua_State *L) {
  int i, rc, nops;
  nops = checktuple(L, 1, "nonzero", 1);
  rc = 0;
  for (i=1; (i <= nops) && !rc; i++) {
    lua_getupvalue(L, 1, i);
    rc = agn_isnumber(L, -1) && agn_tonumber(L, -1) == 0;
    agn_poptop(L);
  }
  lua_pushboolean(L, !rc);
  return 1;
}


static int mt_in (lua_State *L) {
  int i, rc, nops;
  nops = checktuple(L, 2, "in", 1);
  rc = 0;
  for (i=1; (i <= nops) && !rc; i++) {
    lua_getupvalue(L, 2, i);
    rc = lua_equal(L, -1, 1);
    agn_poptop(L);
  }
  lua_pushboolean(L, rc);
  return 1;
}


static int mt_notin (lua_State *L) {
  int i, rc, nops;
  nops = checktuple(L, 2, "notin", 1);
  rc = 1;
  for (i=1; (i <= nops) && rc; i++) {
    lua_getupvalue(L, 2, i);
    rc = !lua_equal(L, -1, 1);
    agn_poptop(L);
  }
  lua_pushboolean(L, rc);
  return 1;
}


static int mt_union (lua_State *L) {
  int i, xn, yn, nops, mtidx;
  if (istuple(L, 1) && istuple(L, 2)) {
    xn = lua_nupvalues(L, 1);
    yn = lua_nupvalues(L, 2);
    if (xn*yn == 0)
      luaL_error(L, "Error in " LUA_QS ": at least one tuple is empty.", "union");
    nops = xn + yn;
    if (nops > LUAI_MAXUPVALUES)
      luaL_error(L, "Error in " LUA_QS ": too many elements in result: %d instead of max. %d.", "union", nops, LUAI_MAXUPVALUES);
    luaL_checkstack(L, nops, "not enough stack space");
    for (i=1; i <= xn; i++) {
      lua_getupvalue(L, 1, i);
    }
    for (i=1; i <= yn; i++) {
      lua_getupvalue(L, 2, i);
    }
    mtidx = 1;
  } else if (!istuple(L, 1) && istuple(L, 2)) {  /* prepend, 7.0.3 extension */
    xn = 1;
    yn = lua_nupvalues(L, 2);
    if (yn == 0)
      luaL_error(L, "Error in " LUA_QS ": tuple is empty.", "union");
    nops = xn + yn;
    if (nops > LUAI_MAXUPVALUES)
      luaL_error(L, "Error in " LUA_QS ": too many elements in result: %d instead of max. %d.", "union", nops, LUAI_MAXUPVALUES);
    luaL_checkstack(L, nops, "not enough stack space");
    lua_pushvalue(L, 1);
    for (i=1; i <= yn; i++) {
      lua_getupvalue(L, 2, i);
    }
    mtidx = 2;
  } else if (istuple(L, 1) && !istuple(L, 2)) {  /* append, 7.0.3 extension */
    xn = lua_nupvalues(L, 1);
    yn = 1;
    if (xn == 0)
      luaL_error(L, "Error in " LUA_QS ": tuple is empty.", "union");
    nops = xn + yn;
    if (nops > LUAI_MAXUPVALUES)
      luaL_error(L, "Error in " LUA_QS ": too many elements in result: %d instead of max. %d.", "union", nops, LUAI_MAXUPVALUES);
    luaL_checkstack(L, nops, "not enough stack space");
    for (i=1; i <= xn; i++) {
      lua_getupvalue(L, 1, i);
    }
    lua_pushvalue(L, 2);
    mtidx = 1;
  } else {
    nops = mtidx = 0;
    luaL_error(L, "Error in " LUA_QS ": this should not happen.", "union");
  }
  lua_pushcclosure(L, tuple, nops);
  luaL_setmetatype(L, mtidx);  /* 3.15.5 change */
  aux_additerator(L);  /* 7.1.1 fix */
  return 1;
}


static int mt_intersect (lua_State *L) {
  int i, j, c, xn, yn, rc;
  xn = checktuple(L, 1, "intersect", 1);
  yn = checktuple(L, 2, "intersect", 1);
  luaL_checkstack(L, 2 + ((xn > yn) ? xn : yn), "not enough stack space");
  c = 0;
  for (i=1; i <= xn; i++) {
    rc = 0;
    lua_getupvalue(L, 1, i);
    for (j=1; j <= yn; j++) {
      lua_getupvalue(L, 2, j);
      rc = lua_equal(L, -2, -1);
      agn_poptop(L);
      if (rc) {  /* leave value on stack for later inclusion */
        c++;
        break;
      }
    }
    if (!rc) agn_poptop(L);
  }
  lua_pushcclosure(L, tuple, c);
  luaL_setmetatype(L, 1);  /* 3.15.5 change */
  aux_additerator(L);  /* 7.1.1 fix */
  return 1;
}


static int mt_minus (lua_State *L) {
  int i, j, c, xn, yn, rc;
  c = 0;
  if (istuple(L, 1) && istuple(L, 2)) {
    xn = lua_nupvalues(L, 1);
    yn = lua_nupvalues(L, 2);
    luaL_checkstack(L, xn + 2, "not enough stack space");
    for (i=1; i <= xn; i++) {
      rc = 0;
      lua_getupvalue(L, 1, i);
      for (j=1; j <= yn; j++) {
        lua_getupvalue(L, 2, j);
        rc = lua_equal(L, -2, -1);
        agn_poptop(L);
        if (rc) break;
      }
      if (!rc) c++;
      else agn_poptop(L);
    }
  } else if (istuple(L, 1)) {  /* 7.0.3 extension */
    xn = lua_nupvalues(L, 1);
    luaL_checkstack(L, xn + 1, "not enough stack space");
    c = 0;
    for (i=1; i <= xn; i++) {
      rc = 0;
      lua_getupvalue(L, 1, i);
      rc = lua_equal(L, -1, 2);
      if (!rc) c++;
      else agn_poptop(L);
    }
  } else {
    luaL_error(L, "Error in " LUA_QS ": wrong order of types given.", "minus");
  }
  lua_pushcclosure(L, tuple, c);
  luaL_setmetatype(L, 1);  /* 3.15.5 change */
  aux_additerator(L);  /* 7.1.1 fix */
  return 1;
}


static const struct luaL_Reg mt_tupleslib [] = {  /* metamethods for numeric userdata `n` */
  {"__index",      tuples_getitem},   /* n[p], with p the index, counting from 1, NO support of n[p to q] indexing since the VM can only return one value */
  {"__writeindex", tuples_setitem},   /* n[p] := value, with p the index, counting from 1 */
  {"__tostring",   tuples_tostring},  /* for output at the console, e.g. print(n) */
  {"__size",       tuples_getsize},   /* metamethod for `size` operator */
  {"__empty",      mt_empty},        /* metamethod for `empty` operator */
  {"__filled",     mt_filled},       /* metamethod for `filled` operator */
  {"__zero",       mt_zero},         /* metamethod for `zero` operator */
  {"__nonzero",    mt_nonzero},      /* metamethod for `nonzero` operator */
  {"__in",         mt_in},           /* metamethod for `in` operator */
  {"__notin",      mt_notin},        /* metamethod for `notin` operator */
  {"__intersect",  mt_intersect},    /* metamethod for `intersect` operator */
  {"__minus",      mt_minus},        /* metamethod for `minus` operator */
  {"__union",      mt_union},        /* metamethod for `union` operator */
  {"__eeq",        mt_eeq},          /* equality mt */
  {"__eq",         mt_eeq},          /* equality mt */
  {"__aeq",        mt_aeq},          /* approximate equality mt */
  {NULL, NULL}
};


static const luaL_Reg tupleslib[] = {
  {"getitem",    tuples_getitem},
  {"getsize",    tuples_getsize},
  {"isall",      tuples_isall},
  {"iterate",    tuples_iterate},
  {"map",        tuples_map},
  {"member",     tuples_member},
  {"remove",     tuples_remove},
  {"select",     tuples_select},
  {"setitem",    tuples_setitem},
  {"subs",       tuples_subs},
  {"toreg",      tuples_toreg},
  {"toseq",      tuples_toseq},
  {"tostring",   tuples_tostring},
  {"totable",    tuples_totable},
  {"new",        tuples_new},
  {"unpack",     tuples_unpack},
  {NULL, NULL}
};


/*
** Open tuples library
*/
LUALIB_API int luaopen_tuples (lua_State *L) {
  /* metamethods */
  luaL_newmetatable(L, AGENA_TUPLESLIBNAME);
  luaL_register(L, NULL, mt_tupleslib);
  /* register library */
  luaL_register(L, AGENA_TUPLESLIBNAME, tupleslib);
  return 1;
}

