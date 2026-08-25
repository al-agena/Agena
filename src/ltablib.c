/*
** $Id: ltablib.c,v 1.38 2005/10/23 17:38:15 roberto Exp $
** Library for Table Manipulation
** See Copyright Notice in agena.h
*/


#include <stddef.h>
#include <string.h>

#define ltablib_c
#define LUA_LIB

#include "agena.h"

#include "agenalib.h"
#include "agnxlib.h"
#include "linalg.h"
#include "lopcodes.h"


#define aux_getn(L,n)     (luaL_checktype(L, n, LUA_TTABLE), luaL_getn(L, n))
#define aux_getnx(L,n,w)  (checktab(L, n, (w) | TAB_L), luaL_len(L, n))


/* Returns the largest positive numerical index of the given table t, or zero if the table has no positive
   numerical indices. (To do its job, this function does a linear traversal of the whole table.) */
static int tbl_maxn (lua_State *L) {
  lua_Number max = 0;
  luaL_checktype(L, 1, LUA_TTABLE);
  lua_pushnil(L);  /* first key */
  while (lua_next(L, 1)) {
    agn_poptop(L);  /* remove value */
    if (lua_type(L, -1) == LUA_TNUMBER) {
      lua_Number v = agn_tonumber(L, -1);  /* Agena 1.4.3/1.5.0 */
      if (v > max) max = v;
    }
  }
  lua_pushnumber(L, max);
  return 1;
}


static int tbl_allocate (lua_State *L) {  /* 22.12.2009 - 0.29.3 */
  int i, nargs;
  luaL_checktype(L, 1, LUA_TTABLE);
  nargs = lua_gettop(L);
  luaL_checkstack(L, nargs, "too many arguments");
  if (nargs == 1)
    luaL_error(L, "Error in " LUA_QS ": at least three arguments expected.", "tables.allocate");
  if ((nargs & 1) == 0)  /* 2.9.0 */
    luaL_error(L, "Error in " LUA_QS ": odd number of arguments expected.", "tables.allocate");
  lua_settop(L, nargs);
  for (i=3; i <= nargs; i = i + 2) {
    lua_settable(L, 1);
  }
  return 0;
}


/*************************************************************************/
/* functions added for agena 0.5.3                                       */
/*************************************************************************/

/* tbl_indices: returns all keys of a table in a new table */

static int tbl_indices (lua_State *L) {
  int i, nrets;
  i = 0;
  luaL_checktype(L, 1, LUA_TTABLE);
  if (lua_gettop(L) == 1) {
    lua_createtable(L, agn_size(L, 1), 0);
    lua_pushnil(L);
    while (lua_next(L, 1) != 0)
      lua_rawsetikey(L, -3, ++i);
    nrets = 1;
  } else {
    int flag;
    agn_intindices(L, 1, &flag);
    lua_pushboolean(L, flag);
    nrets = 2;
  }
  return nrets;
}


static int tbl_entries (lua_State *L) {  /* 0.30.1/2.30.1 */
  int flag;
  luaL_checktype(L, 1, LUA_TTABLE);
  if (lua_gettop(L) == 1) {
    agn_entries(L, 1, &flag);
  } else {  /* return all entries that have integral keys */
    agn_intentries(L, 1, &flag);
  }
  lua_pushboolean(L, flag);
  return 2;
}


static int tbl_array (lua_State *L) {  /* 2.30.1 */
  luaL_checktype(L, 1, LUA_TTABLE);
  agn_arraypart(L, 1);
  return 1;
}


static int tbl_hash (lua_State *L) {  /* 2.30.1 */
  luaL_checktype(L, 1, LUA_TTABLE);
  agn_hashpart(L, 1);
  return 1;
}


static int tbl_parts (lua_State *L) {  /* 2.30.1 */
  luaL_checktype(L, 1, LUA_TTABLE);
  agn_parts(L, 1);
  return 2;
}


static int tbl_reshuffle (lua_State *L) {  /* 2.30.1 */
  luaL_checktype(L, 1, LUA_TTABLE);
  agn_reorder(L, 1, lua_gettop(L) == 2);  /* 3.9.0 extension */
  lua_gc(L, LUA_GCCOLLECT, 0);  /* 4.6.3 */
  return 0;
}


/* Resizes the size of the array part of table t to `newsize` allocated slots. `newsize` must be a non-negative
   integer; if not given the function counts the number of elements in the array part and proceeds.

   If `newsize` is less than the number of currently stored values in the array part, surplus values are cut off.
   If the array part includes embedded `null`'s, they are removed shifting down the other elements to close the space.
   `newsize` may be zero, and in this case the function removes the array part of t completely.

   The function always leaves the hash part unchanged. 4.6.3 */
static int tbl_resize (lua_State *L) {  /* 4.6.3 */
  luaL_checktype(L, 1, LUA_TTABLE);
  agn_resize(L, 1, agnL_optnonnegint(L, 2, agn_asize(L, 1)));
  lua_gc(L, LUA_GCCOLLECT, 0);  /* 4.6.3 */
  return 0;
}


/* Clears the entire array and hash parts of a table, leaves possible metatables untouched. 4.9.0 */
static int tbl_cleanse (lua_State *L) {  /* 4.9.0 */
  luaL_checktype(L, 1, LUA_TTABLE);
  agn_clear(L, 1);
  lua_gc(L, LUA_GCCOLLECT, 0);
  return 0;
}


static int tbl_getsize (lua_State *L) {  /* 2.3.0 RC 4 */
  int option;
  size_t a[3];
  luaL_checktype(L, 1, LUA_TTABLE);
  option = (lua_gettop(L) != 1);
  agn_tablesize(L, 1, a);
  /* push rough guess on the size of a table using a logarithmic method */
  lua_pushnumber(L, a[0]);
  if (option) {
    lua_pushboolean(L, a[1]);       /* hash part empty ? `guess` */
    lua_pushboolean(L, a[2] != 0);  /* hash part filled, no guess */
  }
  return 1 + 2*option;
}


/* Checks whether the given table is a pure array, i.e. only contains one or more elements in the array part
   of the table but none in the hash part, and returns `true` or `false`. The second Boolean return indicates
   whether the array has holes. */
static int tbl_isarray (lua_State *L) {  /* added 3.1.0 */
  size_t a[12];
  luaL_checktype(L, 1, LUA_TTABLE);
  agn_tablestate(L, 1, a, 1);
  lua_pushboolean(L, a[0] != 0 && a[1] == 0);  /* a[0]: elems in array part, a[1]: elems in hash part */
  lua_pushboolean(L, a[2]);  /* a[2] array has holes ? */
  return 2;
}


/* Checks whether the array part of a table contains at least one `null` value, i.e. a hole. The table may may
   also have a hash part, but this does not influence the result. 3.9.0 */
static int tbl_hashole (lua_State *L) {
  size_t a[12];
  luaL_checktype(L, 1, LUA_TTABLE);
  agn_tablestate(L, 1, a, 1);
  lua_pushboolean(L, a[2]);  /* a[2] array has holes ? */
  return 1;
}


/* Checks whether a table consists of an array part only and whether at least one value is null. 3.9.0 */
static int tbl_isnullarray (lua_State *L) {
  size_t a[12];
  luaL_checktype(L, 1, LUA_TTABLE);
  agn_tablestate(L, 1, a, 1);
  /* a[0]: elems in array part, a[1]: elems in hash part; a[2] array has holes ? */
  lua_pushboolean(L, a[0] != 0 && a[1] == 0 && a[2]);
  return 1;
}


/* Checks whether the given table is a pure dictionary, i.e. only contains one or more elements in the hash part
   of the table but none in the array part, and returns `true` or `false`. */
static int tbl_ishash (lua_State *L) {  /* added 3.1.0 */
  luaL_checktype(L, 1, LUA_TTABLE);
  lua_pushboolean(L, !agn_hasarraypart(L, 1) && agn_hashashpart(L, 1));  /* 3.9.7 speed-up */
  return 1;
}


static int tbl_isall (lua_State *L) {  /* 3.10.2 */
  agn_tblisall(L, 1, "tables.isall");
  return 1;
}


/* If any `option` is given, returns the actual number of elements currently stored in the array and hash part. If no option
   is given, then an estimate of the number of elements in the array part is returned, and 0 for the hash part as this
   cannot be estimated.
   Returns two integers: the first for the array part, the second for the hash part.
   See also: `size`, `tables.getsize`. */
static int tbl_getsizes (lua_State *L) {  /* 2.21.4 RC 4 */
  luaL_checktype(L, 1, LUA_TTABLE);
  if (lua_gettop(L) != 1) {
    size_t a[12];
    agn_tablestate(L, 1, a, 1);
    lua_pushnumber(L, a[0]);  /* actual number of elements currently stored in array part */
    lua_pushnumber(L, a[1]);  /* actual number of elements currently stored in hash part */
  } else {
    size_t a[3];
    agn_tablesize(L, 1, a);
    lua_pushnumber(L, a[0]);  /* estimated number of elements currently stored in array part */
    lua_pushnumber(L, a[2]);  /* actual number or hash elements */
  }
  return 2;
}


/* Returns the smallest and largest assigned index - in this order - in the array part of a table (default)
   or in the array and hash part if any option has been given. If zeros are returned, the array part or
   the array and hash part of the table are empty. */
static int tbl_borders (lua_State *L) {  /* 2.14.10, lowest and highest index in array part */
  size_t a[2];
  luaL_checktype(L, 1, LUA_TTABLE);
  if (lua_gettop(L) == 1)  /* only traverse array part ? */
    agn_arrayborders(L, 1, a);  /* if zeros are returned, the array part is empty */
  else
    agn_borders(L, 1, a);  /* traverse both array and hash part, slower; 2.30.1 */
  lua_pushnumber(L, a[0]);
  lua_pushnumber(L, a[1]);
  return 2;
}


/* Inserts values into a table t of tables. If t[key] represents a table, value is added to the end of its array part. If t[key] is
   unassigned, then t[key] := [value]. */
static int tbl_include (lua_State *L) {  /* 2.11.4 */
  int i, nargs;
  luaL_checktype(L, 1, LUA_TTABLE);
  luaL_checkany(L, 3);
  nargs = lua_gettop(L);
  luaL_checkstack(L, nargs, "too many arguments");
  lua_pushvalue(L, 2);  /* 2.20.3 change */
  lua_rawget(L, 1);     /* ditto */
  if (lua_isnil(L, -1)) {
    agn_poptop(L);
    lua_pushvalue(L, 2);  /* push key */
    lua_createtable(L, 1, 1);
    for (i=3; i <= nargs; i++) {
      lua_pushvalue(L, i);  /* push value */
      lua_rawseti(L, -2, i - 2);  /* insert value into new subtable, new subtable at the stack top */
    }
    lua_rawset(L, 1);  /* assign new subtable to table */
  } else if (lua_istable(L, -1)) {
    for (i=3; i <= nargs; i++)
      agn_rawinsertfrom(L, -1, i);
    agn_poptop(L);  /* pop subtable */
  } else
    luaL_error(L, "Error in " LUA_QS ": wrong kind of table.", "tables.include");
  return 0;
}


/* Checks whether a table includes a given field (a string key) amd returns true or false. Metamethods are ignored.
   50 % slower than the expression `(assigned) tbl[field]`. UNDOC, 2.21.0 */
static int tbl_hasfield (lua_State *L) {
  luaL_checktype(L, 1, LUA_TTABLE);
  luaL_checktype(L, 2, LUA_TSTRING);
  lua_pushboolean(L, lua_hasfield(L, 1, agn_tostring(L, 2)));
  return 1;
}


/*
** Operations that an object must define to mimic a table
** (some functions only need some of them)
*/
#define TAB_R  1      /* read */
#define TAB_W  2      /* write */
#define TAB_L  4      /* length */
#define TAB_RW  (TAB_R | TAB_W)    /* read/write */

static int checkfield (lua_State *L, const char *key, int n) {
  lua_pushstring(L, key);
  return (lua_rawget(L, -n) != LUA_TNIL);
}

/*
** Check that 'arg' either is a table or can behave like one (that is,
** has a metatable with the required metamethods)
*/
static void checktab (lua_State *L, int arg, int what) {
  if (lua_type(L, arg) != LUA_TTABLE) {  /* is it not a table? */
    int n = 1;  /* number of elements to pop */
    if (lua_getmetatable(L, arg) &&  /* must have metatable */
        (!(what & TAB_R) || checkfield(L, "__index", ++n)) &&
        (!(what & TAB_W) || checkfield(L, "__writeindex", ++n)) &&
        (!(what & TAB_L) || checkfield(L, "__size", ++n))) {
      lua_pop(L, n);  /* pop metatable and tested metamethods */
    } else
      luaL_checktype(L, arg, LUA_TTABLE);  /* force an error */
  }
}

/* Copies elements from the table a1 to the table a2, performing the equivalent to the following multiple assignment:
   a2[t],··· = a1[f],···,a1[e]. The default for a2 is a1. The destination range can overlap with the source range.

   Returns the destination table a2.

   Example: The following statement copies four elements in table a from position 3 up to and including 6 to a new table b,
   starting with index 1:

   > a := ['a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'];

   > b := tables.move(a, 3, 6, 1, []);

   > b:
   [c, d, e, f]

   The next statement copies four elements in a to its beginning:

   > tables.move(a, 3, 6, 1);

   > a:
   [c, d, e, f, e, f, g, h]

** Copy elements (1[f], ..., 1[e]) into (tt[t], tt[t+1], ...). Whenever possible, copy in increasing order,
** which is better for rehashing. "possible" means destination after original range, or smaller than origin,
** or copying to another table. 2.21.1; taken from Lua 5.4.0. RC 4/5.4.4. */
static int tbl_move (lua_State *L) {
  lua_Integer f = luaL_checkinteger(L, 2);  /* source start position */
  lua_Integer e = luaL_checkinteger(L, 3);  /* source end position */
  lua_Integer t = luaL_checkinteger(L, 4);  /* destination start position */
  int tt = !lua_isnoneornil(L, 5) ? 5 : 1;  /* index of destination structure */
  checktab(L, 1, TAB_R);
  checktab(L, tt, TAB_W);
  if (e >= f) {  /* otherwise, nothing to move */
    lua_Integer n, i;
    luaL_argcheck(L, f > 0 || e < LUA_MAXINTEGER + f, 3, "too many elements to move");
    n = e - f + 1;  /* number of elements to move */
    luaL_argcheck(L, t <= LUA_MAXINTEGER - n + 1, 4, "destination wrap around");
    /* if APPEND: source end pos < dest start pos ||
          SHIFT LEFT: source start pos >= dest start pos ||
          source structure different from destination structure */
    if (t > e || t <= f || (tt != 1 && !lua_compare(L, 1, tt, LUA_OPEQ))) {
      for (i=0; i < n; i++) {  /* from left to right, optionally using metamethods */
        lua_geti(L, 1, f + i);
        lua_seti(L, tt, t + i);
      }
    } else {  /* SHIFT RIGHT in same structure */
      for (i=n - 1; i >= 0; i--) {  /* from right to left, optionally using metamethods */
        lua_geti(L, 1, f + i);
        lua_seti(L, tt, t + i);
      }
    }
  }
  lua_pushvalue(L, tt);  /* return destination table */
  return 1;
}


/* tables.remove (t [, pos])

Removes from table t the element at position pos, returning the value of the removed element. When pos is
an integer between 1 and size t, it shifts down the elements t[pos+1], t[pos+2], ···, t[size list]
and erases element t[size t]; The index pos can also be 0 when size t is 0, or size t + 1.

The default value for pos is size t, so that a call tables.remove(t) removes the last element of t.

Cross-ref ! See also: `purge`.

Taken from Lua 5.4.8 */
static int tbl_remove (lua_State *L) {
  lua_Integer size = aux_getn(L, 1);
  lua_Integer pos = luaL_optinteger(L, 2, size);
  if (pos != size)  /* validate 'pos' if given */
    /* check whether 'pos' is in [1, size + 1] */
    luaL_argcheck(L, (lua_Unsigned)pos - 1u <= (lua_Unsigned)size, 2,
                     "position out of bounds");
  luaL_checkstack(L, 2, "not enough stack space");  /* 5.1.3 fix */
  lua_geti(L, 1, pos);  /* result = t[pos] */
  for ( ; pos < size; pos++) {
    lua_geti(L, 1, pos + 1);
    lua_seti(L, 1, pos);  /* t[pos] = t[pos + 1] */
  }
  lua_pushnil(L);
  lua_seti(L, 1, pos);  /* remove entry t[pos] (pos is now set to size) */
  return 1;
}


/* In table t, the function replaces every occurrence of the value old with value new, in-place, and
   returns the number of substitutions done. If you are substituting with `null`, you may call
   `tables.reshuffle` afterwards to remove the holes created in the table, or call `tables.remove`
   instead of `tables.subs`.
   The function is four times faster than `subs`, avoiding most of all checks and conversions that
   `subs` is performing. 5.1.3 */
static int tbl_subs (lua_State *L) {
  luaL_checktype(L, 1, LUA_TTABLE);
  luaL_checkany(L, 3);
  lua_pushinteger(L, agn_tabsubs(L, 1, 2, 3));
  return 1;
}


static int tbl_new (lua_State *L) {
  /* use `volatile` so that the compiler does not render the Kahan code effectless */
  /* volatile lua_Number idx, step, c, y, t; */
  int isfunc, isint, isdefault;
  size_t counter, i, nargs, total, offset;
  lua_Number a, b, eps;
  volatile lua_Number s, c, cs, ccs, cc, t, idx, step, x;
  if (lua_ispair(L, 1)) {  /* new 6.1.2 */
    int x, y, nargs;
    nargs = lua_gettop(L);
    if (agn_pairgetiintegers(L, 1, &x, &y)) {  /* 6.1.2 */
      if (x < 0 || y < 0)
        luaL_error(L, "Error in " LUA_QS ": expected a pair of non-negative integers for argument #1.", "tables.new");
      lua_createtable(L, x, y);
      if (nargs == 2) {
        if (agnL_gettablefield(L, LUA_TABLIBNAME, "mt", "tables.new", 1) != LUA_TTABLE) {
          agn_poptop(L);
          luaL_error(L, "Error in " LUA_QS ": could not find table " LUA_QS ".", "tables.new", "tables.mt");
        }
        lua_setmetatable(L, -2);
      }
    } else {
      luaL_error(L, "Error in " LUA_QS ": expected a pair of numbers for argument #1.", "tables.new");
    }
  } else {
    luaL_aux_nstructure(L, "table.new", &nargs, &offset, &a, &b, &step, &eps, &total, &isfunc, &isdefault, &isint);
    luaL_checkstack(L, 1 + isdefault, "not enough stack space");  /* 3.15.4 fix */
    lua_createtable(L, total, 0);
    counter = 0;
    cs = ccs = 0;
    s = idx = a;
    if (isdefault) {  /* create a sequence of n slots and fill it with one and the same default of any type */
      agn_pairgeti(L, 2, 2);
    }
    /* total > counter: prevents that the last element is inserted even if a roundoff error occurred. */
    if (isfunc) {  /* function passed ? */
      int slots = 2 + (nargs >= 4 + offset)*(nargs - 4 - (int)offset + 1);
      while (idx <= b || tools_approx(idx, b, eps)) {
        luaL_checkstack(L, slots, "not enough stack space");  /* 3.5.5/3.15.4 fix */
        lua_pushvalue(L, offset);  /* push function */
        lua_pushnumber(L, (fabs(idx) < AGN_HEPSILON) ? 0 : idx);  /* quite dirty hack to avoid roundoff errors with 0 */
        for (i=4 + offset; i <= nargs; i++) lua_pushvalue(L, i);
        lua_call(L, slots - 1, 1);
        lua_seti(L, -2, ++counter);
        if (isint) {
          idx += step;
        } else {
          x = step;  /* Kahan-Babuska */
          t = s + x;
          c = (fabs(s) >= fabs(x)) ? (s - t) + x : (x - t) + s;
          s = t;
          t = cs + c;
          cc = (fabs(cs) >= fabs(c)) ? (cs - t) + c : (c - t) + cs;
          cs = t;
          ccs += cc;
          idx = s + cs + ccs;
        }
      }
    } else {
      while (idx <= b || tools_approx(idx, b, eps)) {
        if (isdefault) {  /* fill with default value  */
          lua_pushvalue(L, -1);
          lua_seti(L, -3, idx);
        } else
          agn_setinumber(L, -1, ++counter, (fabs(idx) < AGN_HEPSILON) ? 0 : idx);
        if (isint) {  /* 2.12.2 */
          idx += step;
        } else {
          x = step;  /* Kahan-Babuska */
          t = s + x;
          c = (fabs(s) >= fabs(x)) ? (s - t) + x : (x - t) + s;
          s = t;
          t = cs + c;
          cc = (fabs(cs) >= fabs(c)) ? (cs - t) + c : (c - t) + cs;
          cs = t;
          ccs += cc;
          idx = s + cs + ccs;
        }
      }
    }
    if (isdefault) agn_poptop(L);
  }
  return 1;
}


/* Creates a table of n subtables, each with narray preallocated array slots and nhash preallocated hash slots.
   You can fill the array part of each subtable with a default, narray times, by passing the `init=default` option. 3.17.8 */
static int tbl_tableoftables (lua_State *L) {
  int i, j, narray, nhash, nops, isdefault;
  luaL_checkstack(L, 2, "not enough stack space");
  nops = agn_checknonnegint(L, 1);
  narray = agn_checknonnegint(L, 2);  /* 3.18.1 fix */
  nhash = agn_checknonnegint(L, 3);   /* ditto */
  isdefault = lua_ispair(L, 4);
  lua_createtable(L, nops, 0);
  if (isdefault) {  /* create a sequence of n slots and fill it with one and the same default of any type */
    agn_pairgeti(L, 4, 2);
  }
  for (i=0; i < nops; i++) {
    lua_createtable(L, narray, nhash);
    if (isdefault) {
      for (j=0; j < narray; j++) {
        lua_pushvalue(L, -2);
        lua_seti(L, -2, j + 1);
      }
    }
    lua_seti(L, -2 - isdefault, i + 1);
  }
  if (isdefault) agn_poptop(L);
  return 1;
}


/* Checks table A for subtables of the same size. Only elements in the array part of the subtables are taken into account.
   Returns both the number of rows and the number of columns found if all subtables have the same size or issues an error
   otherwise. 3.17.8 */
static int aux_getdim (lua_State *L, int idx, int *rows, int *cols) {
  int i, c, rc;
  luaL_checktype(L, idx, LUA_TTABLE);
  *cols = c = 0;
  *rows = agn_asize(L, idx);
  rc = 0;
  for (i=0; i < *rows; i++) {
    lua_rawgeti(L, idx, i + 1);  /* push item */
    if (lua_istable(L, -1)) {  /* a row */
      *cols = agn_asize(L, -1);
      if (i == 0) c = *cols;
      rc = (c != *cols);
    } else {  /* not a row */
      rc = 1;
    }
    agn_poptop(L);
    if (rc) break;
  }
  return rc;
}

static int tbl_getdim (lua_State *L) {  /* 3.17.8 */
  int rows, cols;
  if (aux_getdim(L, 1, &rows, &cols)) {
    luaL_error(L, "Error in " LUA_QS ": table is not 2-dimensional.", "tables.getdim");
  }
  luaL_checkstack(L, 2, "not enough stack space");
  lua_pushinteger(L, rows);
  lua_pushinteger(L, cols);
  return 2;
}


/* Checks whether table A has the same number of rows as there are elements in each of its subtables (`columns`).
   The subtables must be of the same size. Only elements in the array part of the subtables are taken into account.
   Returns `true` or `false` and in case of `true`, also returns the number of rows and the number of columns
   found. 3.17.8 */
static int tbl_issquare (lua_State *L) {
  int rows, cols, rc, nrets;
  rc = aux_getdim(L, 1, &rows, &cols);
  nrets = 1 + 2*(rc == 0);
  luaL_checkstack(L, nrets, "not enough stack space");
  lua_pushboolean(L, rc == 0 && rows == cols);
  if (!rc) {
    lua_pushinteger(L, rows);
    lua_pushinteger(L, cols);
  }
  return nrets;
}


/* Checks table A for subtables of the same size. Only elements in the array part of the subtables are taken into account.
   Returns `true` or `false` and in case of `true`, also returns the number of rows and the number of columns found.
   With square tables, also returns `true`. 3.17.8 */
static int tbl_isrectangular (lua_State *L) {
  int rows, cols, rc, nrets;
  rc = aux_getdim(L, 1, &rows, &cols);
  nrets = 1 + 2*(rc == 0);
  luaL_checkstack(L, nrets, "not enough stack space");
  lua_pushboolean(L, rc == 0);
  if (!rc) {
    lua_pushinteger(L, rows);
    lua_pushinteger(L, cols);
  }
  return nrets;
}


/* Check options for tables.extend, tables.transpose;
   You must pass a decent value for nargs ! */
void tables_checkoptions (lua_State *L, int from, int *nargs, int *inplace, int *init, int *sparse, const char *procname) {
  int checkoptions, p, q, gotrange;
  gotrange = p = q = 0;  /* 0 = no integer range p:q given */
  *init = 0;     /* index position of default value residing at rhs of the pair */
  *inplace = 0;  /* 0 = return a new structure, do not work in-place */
  *sparse = 0;   /* 0 = return dense structure, 1 = return sparse structure */
  checkoptions = 3;  /* check n options; CHANGE THIS if you add/delete options */
  if (*nargs >= from && lua_ispair(L, *nargs))  /* 6.7.8 change */
    luaL_checkstack(L, 2, "not enough stack space");
  while (checkoptions-- && *nargs >= from && lua_ispair(L, *nargs)) {
    agn_pairgeti(L, *nargs, 1);  /* get left value, set to stack index -2 */
    agn_pairgeti(L, *nargs, 2);  /* get right value, set to stack index  -1 */
    if (agn_isstring(L, -2)) {
      const char *option = agn_tostring(L, -2);
      if (tools_streq("inplace", option)) {
        *inplace = agn_checkboolean(L, -1);
      } else if (tools_streq("init", option)) {
        *init = *nargs;
      } else if (tools_streq("sparse", option)) {
        *sparse = agn_checkboolean(L, -1);
      } else {
        agn_poptoptwo(L);
        luaL_error(L, "Error in " LUA_QS ": unknown option " LUA_QS ".", procname, option);
      }
    } else if (agn_isinteger(L, -2) && agn_isinteger(L, -1)) {
      p = agn_tointeger(L, -2); q = agn_tointeger(L, -1); gotrange = 1;
    }
    /* do not call lua_settop as it would corrupt the argument stack since we have already pushed values */
    (*nargs)--;
    agn_poptoptwo(L);
  }
  if (gotrange) {  /* 7.3.7 fix for subranges */
    lua_settop(L, ++(*nargs));
    agn_createpairnumbers(L, p, q);
    lua_replace(L, *nargs);
  }
}

/* create new 2-dimensional table with the elements of C `matrix` a, based on linalg/creatematrix, 3.18.9 */
static int creatematrix (lua_State *L, lua_Number *a, int m, int n, int sparse) {
  int i, j;
  lua_Number item;
  luaL_checkstack(L, 2, "not enough stack space");
  lua_createtable(L, m, 0);
  for (i=0; i < m; i++) {
    lua_createtable(L, n, 0);  /* create new vector */
    for (j=0; j < n; j++) {
      item = a[i*n + j];
      if (!sparse || (sparse && item != 0))
        agn_setinumber(L, -1, j + 1, item);  /* create sparse matrix if requested */
    }
    lua_rawseti(L, -2, i + 1);
  }
  return 1;
}

static FORCE_INLINE int checkvector (lua_State *L, int idx, const char *procname) {
  if (!(lua_istable(L, idx)))
    luaL_error(L, "Error in " LUA_QS ": table expected, got %s.", procname, luaL_typename(L, idx));
  return agn_asize(L, idx);
}

static void fillmatrix (lua_State *L, int idx, lua_Number *a, int m, int n, const char *procname) {
  int i, j;
  for (i=0; i < m; i++) {
    lua_rawgeti(L, idx, i + 1);  /* push row vector on stack */
    if (checkvector(L, -1, procname) != n)
      luaL_error(L, "Error in " LUA_QS ": row table has wrong dimension.", procname);
    for (j=0; j < n; j++)
      a[i*n + j] = agn_getinumber(L, -1, j + 1);  /* with non-numbers, sets zero */
    agn_poptop(L);  /* pop row vector */
  }
}

static int FORCE_INLINE aux_checkrectangular (lua_State *L, int idx, int from,
  int *m, int *n, int *inplace, int *init, int *sparse, const char *procname) {
  int nargs;
  nargs = lua_gettop(L);
  *m = *n = *inplace = *init = *sparse = 0;
  if (aux_getdim(L, idx, m, n))
    luaL_error(L, "Error in " LUA_QS ": table is not two-dimensional or not a table at all.", procname);
  tables_checkoptions(L, from, &nargs, inplace, init, sparse, procname);
  return nargs;
}

static int tbl_transpose (lua_State *L) {  /* 3.18.9 */
  lua_Number *a;
  int m, n, inplace, init, sparse;
  m = n = inplace = init = sparse = 0;
  aux_checkrectangular(L, 1, 2, &m, &n, &inplace, &init, &sparse, "tables.transpose");
  la_createarray(L, a, m*n, "tables.transpose");
  fillmatrix(L, 1, a, m, n, "tables.transpose");
  if (m == n) {
    la_transpose(a, n);
    creatematrix(L, a, n, n, sparse);
  } else {
    lua_Number *b;
    int i, j, c;
    la_createarray(L, b, m*n, "tables.transpose");
    c = 0;
    for (j=0; j < n; j++) {
      for (i=0; i < m; i++) {
        b[c++] = a[i*n + j];
      }
    }
    creatematrix(L, b, n, m, sparse);
    xfree(b);
  }
  xfree(a);
  return 1;
}


/* By default, swaps row p in matrix A with row q. p, q must be positive integers. The result is a new matrix.

   If the very last argument is the Boolean `true`, then the operation is done in-place, modifying A. In this
   mode, the modified matrix A is returned, as well.

   Whether in-place or not, you may limit the exchange of the matrix elements to columns s to t by passing the
   respective index range s:t as an optional fourth argument. Bases on linalg_swaprow, 3.18.9 */
static int tbl_swaprow (lua_State *L) {
  int i, j, l, u, m, n, nargs, inplace, init, sparse;
  luaL_checkany(L, 3);
  nargs = lua_gettop(L);
  i = agn_checkposint(L, 2);
  j = agn_checkposint(L, 3);
  m = n = inplace = init = sparse = 0;
  nargs = aux_checkrectangular(L, 1, 4, &m, &n, &inplace, &init, &sparse, "tables.swaprow");
  if (i > m)
    luaL_error(L, "Error in " LUA_QS ": row index %d out of range.", "tables.swaprow", i);
  if (j > m)
    luaL_error(L, "Error in " LUA_QS ": row index %d out of range.", "tables.swaprow", j);
  l = 1; u = n;
  if (lua_istrue(L, nargs)) {
    inplace = 1;
    nargs--;
  }
  la_getrange(L, 4, nargs, &l, &u, n, "tables.swaprow");
  if (inplace) {
    luaL_checkstack(L, 2, "not enough stack space");
    if (l == 1 && u == n) {
      lua_rawgeti(L, 1, i);  /* push i-th row vector in matrix at idx */
      checkvector(L, -1, "tables.swaprow");
      lua_rawgeti(L, 1, j);  /* push j-th row vector in matrix at idx */
      checkvector(L, -1, "tables.swaprow");
      lua_rawseti(L, 1, i);  /* set j-th row to index i */
      lua_rawseti(L, 1, j);  /* set i-th row to index j */
    } else {
      lua_Number x, y;
      lua_rawgeti(L, 1, i);  /* push i-th row vector in matrix at idx */
      checkvector(L, -1, "tables.swaprow");
      lua_rawgeti(L, 1, j);  /* push j-th row vector in matrix at idx */
      checkvector(L, -1, "tables.swaprow");
      for (; l <= u; l++) {
        x = agn_getinumber(L, -2, l);
        y = agn_getinumber(L, -1, l);
        lua_rawsetinumber(L, -2, l, y);
        lua_rawsetinumber(L, -1, l, x);
      }
      lua_rawseti(L, 1, j);
      lua_rawseti(L, 1, i);
    }
    lua_settop(L, 1);  /* return modified matrix */
  } else {
    lua_Number *a;
    la_createarray(L, a, m*n, "tables.swaprow");
    fillmatrix(L, 1, a, m, n, "tables.swaprow");
    numal_ichrow(L, a, n,  l - 1, u - 1, i - 1, j - 1);
    creatematrix(L, a, m, n, sparse);
    xfree(a);
  }
  return 1;
}


/* based on linalg.swapcol, 3.18.9 */
static int tbl_swapcol (lua_State *L) {
  int i, j, l, u, m, n, nargs, inplace, init, sparse;
  luaL_checkany(L, 3);
  nargs = lua_gettop(L);
  m = n = inplace = init = sparse = 0;
  i = agn_checkposint(L, 2);
  j = agn_checkposint(L, 3);
  nargs = aux_checkrectangular(L, 1, 4, &m, &n, &inplace, &init, &sparse, "tables.swapcol");
  if (i > n)
    luaL_error(L, "Error in " LUA_QS ": column index %d out of range.", "tables.swapcol", i);
  if (j > n)
    luaL_error(L, "Error in " LUA_QS ": column index %d out of range.", "tables.swapcol", j);
  l = 1; u = m;
  if (lua_istrue(L, nargs)) {
    inplace = 1;
    nargs--;
  }
  la_getrange(L, 4, nargs, &l, &u, m, "tables.swapcol");
  if (inplace) {
    lua_Number x, y;
    luaL_checkstack(L, 2, "not enough stack space");
    for (; l <= u; l++) {
      lua_rawgeti(L, 1, l);  /* push l-th row vector in matrix at idx */
      checkvector(L, -1, "tables.swapcol");
      x = agn_getinumber(L, -1, i);
      y = agn_getinumber(L, -1, j);
      lua_rawsetinumber(L, -1, i, y);
      lua_rawsetinumber(L, -1, j, x);
      lua_rawseti(L, 1, l);
    }
    lua_settop(L, 1);
  } else {
    lua_Number *a;
    la_createarray(L, a, m*n, "tables.swapcol");
    fillmatrix(L, 1, a, m, n, "tables.swapcol");
    numal_ichcol(L, a, n,  l - 1, u - 1, i - 1, j - 1);
    creatematrix(L, a, m, n, sparse);
    xfree(a);
  }
  return 1;
}


static int tbl_col (lua_State *L) {  /* 7.3.7 */
  int i, j, c, l, u, m, n, idx, nargs, ncols;
  luaL_checkany(L, 2);
  nargs = lua_gettop(L);
  m = n = ncols = 0;
  if (aux_getdim(L, 1, &m, &n))  /* checks input structure for correct type, too */
    luaL_error(L, "Error in " LUA_QS ": table is not two-dimensional.", "tables.col");
  for (i=2; i <= nargs && agn_isinteger(L, i); i++, ncols++) {
    idx = tools_posrelat(agn_tointeger(L, i), n);
    if (idx < 1 || idx > n)
      luaL_error(L, "Error in " LUA_QS ": column %d does not exist.", "tables.col", agn_tointeger(L, i));
  }
  if (ncols == 0)
    luaL_error(L, "Error in " LUA_QS ": need at least one index.", "tables.col");
  l = 1; u = m;
  la_getrange(L, 2 + ncols, nargs, &l, &u, m, "tables.col");
  c = 0;
  luaL_checkstack(L, 3 + (ncols != 1), "not enough stack space");
  lua_createtable(L, u - l + 1, 0);
  if (ncols == 1) {
    for (; l <= u; l++) {
      lua_rawgeti(L, 1, l);  /* push l-th row vector in matrix at idx */
      checkvector(L, -1, "tables.col");
      lua_rawgeti(L, -1, tools_posrelat(agn_tointeger(L, 2), n));
      lua_rawseti(L, -3, ++c);
      agn_poptop(L);
    }
  } else {
    for (i=l; i <= u; i++) {   /* for each row vector i in range */
      lua_createtable(L, ncols, 0);
      lua_rawgeti(L, 1, i);  /* push l-th row vector in matrix at idx */
      checkvector(L, -1, "tables.col");
      for (j=2; j < 2 + ncols; j++) {  /* for each column j */
        lua_rawgeti(L, -1, tools_posrelat(agn_tointeger(L, j), n));  /* get value at column j */
        lua_rawseti(L, -3, j - 1);  /* set it into substructure */
      }
      agn_poptop(L);  /* drop row vector, result substructure is on top */
      lua_rawseti(L, -2, ++c);
    }
  }
  return 1;
}


/* Tries to find a value with integral index k in the array part of table t and returns it, otherwise returns null. 2.37.3 */
static int tbl_getarray (lua_State *L) {
  luaL_checktype(L, 1, LUA_TTABLE);
  agn_arrayorhashgeti(L, 1, luaL_checkinteger(L, 2), 1) ;
  return 1;
}


/* Tries to find a value with integral index k in the hash part of table t and returns it, otherwise returns null. 2.37.3 */
static int tbl_gethash (lua_State *L) {
  luaL_checktype(L, 1, LUA_TTABLE);
  agn_arrayorhashgeti(L, 1, luaL_checkinteger(L, 2), 0) ;
  return 1;
}


static int tbl_numunion (lua_State *L) {  /* 3.10.0 */
  luaL_argcheck(L, lua_type(L, 1) == LUA_TTABLE, 1, "argument is not a table");
  luaL_argcheck(L, lua_type(L, 2) == LUA_TTABLE, 2, "argument is not a table");
  lua_pushinteger(L, agn_numunion(L, 1, 2));
  return 1;
}


static int tbl_numintersect (lua_State *L) {  /* 3.10.0 */
  luaL_argcheck(L, lua_type(L, 1) == LUA_TTABLE, 1, "argument is not a table");
  luaL_argcheck(L, lua_type(L, 2) == LUA_TTABLE, 2, "argument is not a table");
  lua_pushinteger(L, agn_numintersect(L, 1, 2));
  return 1;
}


static int tbl_numminus (lua_State *L) {  /* 3.10.0 */
  luaL_argcheck(L, lua_type(L, 1) == LUA_TTABLE, 1, "argument is not a table");
  luaL_argcheck(L, lua_type(L, 2) == LUA_TTABLE, 2, "argument is not a table");
  lua_pushinteger(L, agn_numminus(L, 1, 2));
  return 1;
}


/* Based on linalg.extend, 3.17.9 */
static void aux_extend (lua_State *L, int addrows, int addcols, int inplace, int idxdef, const char *procname) {
  int i, j, k, m, n;
  if (addrows == 0 && addcols == 0) return;
  m = n = 0;  /* just to prevent compiler warnings */
  if (aux_getdim(L, 1, &m, &n))
    luaL_error(L, "Error in " LUA_QS ": got malformed table.", procname);
  luaL_checkstack(L, 1, "not enough stack space");
  if (idxdef) {
    lua_pushvalue(L, idxdef);
    if (!lua_ispair(L, -1))  /* better be sure than sorry */
      luaL_error(L, "Error in " LUA_QS ": stack corruption with default value." LUA_QS ".", procname);
    agn_pairgeti(L, -1, 2);
    lua_replace(L, idxdef);
    agn_poptop(L);
  }
  if (inplace)
    lua_pushvalue(L, 1);
  else
    lua_createtable(L, m + addrows, 1);
  for (i=0; i < m; i++) {
    luaL_checkstack(L, 2 + (inplace == 0), "not enough stack space");  /* reserve space for new and existing column vector */
    if (!inplace) {
      lua_createtable(L, n + addcols, 0);  /* create new row vector */
      lua_rawgeti(L, 1, i + 1);  /* push row vector on stack (for reading) */
      luaL_checktype(L, -1, LUA_TTABLE);
      for (j=0; j < n; j++) {
        lua_rawgeti(L, -1, j + 1);
        lua_rawseti(L, -3, j + 1);
      }
    } else {
      lua_rawgeti(L, 1, i + 1);  /* push row vector on stack (for writing) */
      luaL_checktype(L, -1, LUA_TTABLE);
      j = n;
    }
    /* add further elements to end of extended row vector (linalg.extend only) */
    for (k=j; idxdef && k < j + addcols; k++) {
      lua_pushvalue(L, idxdef);
      lua_rawseti(L, -2 - !inplace, k + 1);
    }
    if (!inplace) agn_poptop(L);  /* pop read-only row vector */
    lua_rawseti(L, -2, i + 1);  /* set row vector into resulting table */
  }
  for (i=0; i < addrows; i++) {  /* add further row vector(s) */
    /* we still have enough reserved stack space */
    lua_createtable(L, n + addcols, 0);  /* push new row vector */
    for (j=0; idxdef && j < n + addcols; j++) {
      lua_pushvalue(L, idxdef);
      lua_rawseti(L, -2, j + 1);
    }
    lua_rawseti(L, -2, m + i + 1);  /* drop new row vector */
  }
  /* resulting table is on the stack top */
}

/* Creates a new 2-dimensional table which is a copy of the input 2D table with addrows additional rows and addcols
   additional columns. You can also optionally initialise new entries by passing the 'init = <def>' option where `def`
   may be of any type. Also, the 'inplace = true'  option allows to work in-place, altering the input table and
   saving memory. If addrows and addcols are both zero, a deep copy of the input table is returned. 3.17.9, based
   on linalg.extend. */
static int tbl_extend (lua_State *L) {
  int addrows, addcols, inplace, init, sparse, nargs;
  inplace = init = sparse = 0;
  nargs = lua_gettop(L);
  addrows = agn_checknonnegint(L, 2);
  addcols = agn_checknonnegint(L, 3);
  tables_checkoptions(L, 4, &nargs, &inplace, &init, &sparse, "tables.extend");
  aux_extend(L, addrows, addcols, inplace, init, "tables.extend");
  return 1;
}


/* Checks whether table t has only zero elements in its array part and returns `true` or `false`. It also returns the
   index of the first non-zero element in t as a second result, or 0 if there is none. The non-zero element is returned
   as a third result, too.

   The function regards any non-numeric values in t as non-zero. An empty table is non-zero, too. The function checks
   the array part only.

   By default, the check is done against strict zero. You can change this by passing a positive epsilon value as an
   optional second argument so that all elements x with |x| <= epsilon will be considered zero.

   See also: `linalg.viszero`, `sequences.iszero`. 3.20.1 */
static int tbl_iszero (lua_State *L) {
  int i, n, r, rc;
  lua_Number x, eps;
  luaL_checktype(L, 1, LUA_TTABLE);
  eps = agnL_optnonnegative(L, 2, 0.0);
  r = 1; x = AGN_NAN;
  n = luaL_getn(L, 1);
  for (i=1; i <= n && r; i++) {  /* from left to right ! */
    x = agn_rawgetinumber(L, 1, i, &rc);
    r = rc && (fabs(x) <= eps);
  }
  rc = r*(n != 0);
  lua_pushboolean(L, rc);
  lua_pushinteger(L, (!r)*(i - 1));
  if (!rc) lua_pushnumber(L, x);
  return 2 + !rc;
}


static int tbl_isone (lua_State *L) {
  int i, n, r, rc;
  lua_Number x, eps;
  luaL_checktype(L, 1, LUA_TTABLE);
  eps = agnL_optnonnegative(L, 2, 0.0);
  r = 1; x = AGN_NAN;
  n = luaL_getn(L, 1);
  for (i=1; i <= n && r; i++) {  /* from left to right ! */
    x = agn_rawgetinumber(L, 1, i, &rc);
    r = rc && (fabs(x - 1.0) <= eps);
  }
  rc = r*(n != 0);
  lua_pushboolean(L, rc);
  lua_pushinteger(L, (!r)*(i - 1));
  if (!rc) lua_pushnumber(L, x);
  return 2 + !rc;
}


/* Code taken from Lua 5.4.x *********************************************************************************************** */

/* Taken from Lua 5.4.6, Agena 3.10.1

   tables.concat (tbl [, sep [, i [, j]]])

   Given a table array where all elements are strings or numbers, returns the string tbl[i] & sep & tbl[i+1] ··· sep & tbl[j].
   The default value for sep is the empty string, the default for i is 1, and the default for j is size(tbl). If i is greater
   than j, returns the empty string. */

static void addfield (lua_State *L, luaL_Buffer *b, lua_Integer i) {
  lua_geti(L, 1, i);
  if (l_unlikely(!lua_isstring(L, -1)))
    luaL_error(L, "invalid value (%s) at index %I in table for 'concat'",
                  luaL_typename(L, -1), (LUAI_UACINT)i);
  luaL_addvalue(b);
}

static int tbl_concat (lua_State *L) {
  luaL_Buffer b;
  lua_Integer last = aux_getnx(L, 1, TAB_R);
  size_t lsep;
  const char *sep = luaL_optlstring(L, 2, "", &lsep);
  lua_Integer i = luaL_optinteger(L, 3, 1);
  last = luaL_optinteger(L, 4, last);
  luaL_buffinit(L, &b);
  for (; i < last; i++) {
    addfield(L, &b, i);
    luaL_addlstring(&b, sep, lsep);
  }
  if (i == last)  /* add last value (if interval was not empty) */
    addfield(L, &b, i);
  luaL_pushresult(&b);
  return 1;
}


/*
** {======================================================
** Pack/unpack, taken from Lua 5.4.6
** =======================================================
*/

/* tables.pack (···)
   Returns a new table with all arguments stored into keys 1, 2, etc. and with a field "n" with the total number
   of arguments. Note that the resulting table may not be a table array, if some arguments are null.  */
static int tbl_pack (lua_State *L) {
  int i;
  int n = lua_gettop(L);  /* number of elements to pack */
  lua_createtable(L, n, 1);  /* create result table */
  lua_insert(L, 1);  /* put it at index 1 */
  for (i = n; i >= 1; i--)  /* assign elements */
    lua_seti(L, 1, i);
  lua_pushinteger(L, n);
  lua_setfield(L, 1, "n");  /* t.n = number of elements */
  return 1;  /* return table */
}


/* tables.unpack (tbl [, i [, j]])
   Returns the elements from the given table array. This function is equivalent to
     return tbl[i], tbl[i+1], ···, tbl[j].
   By default, i is 1 and j is size(tbl). */
static int tbl_unpack (lua_State *L) {
  lua_Unsigned n;
  lua_Integer i = luaL_optinteger(L, 2, 1);
  lua_Integer e = luaL_opt(L, luaL_checkinteger, 3, luaL_len(L, 1));
  if (i > e) return 0;  /* empty range */
  n = (lua_Unsigned)e - i;  /* number of elements minus 1 (avoid overflows) */
  if (l_unlikely(n >= (unsigned int)INT_MAX  ||
                 !lua_checkstack(L, (int)(++n))))
    return luaL_error(L, "too many results to unpack");
  for (; i < e; i++) {  /* push arg[i..e - 1] (to avoid overflows) */
    lua_geti(L, 1, i);
  }
  lua_geti(L, 1, e);  /* push last element */
  return (int)n;
}


static int tbl_getitem (lua_State *L) {  /* 6.1.2 */
  int nargs = lua_gettop(L);
  luaL_checktype(L, 1, LUA_TTABLE);
  if (nargs == 2 && !agn_isinteger(L, 2)) {  /* OOP method */
    return agn_initmethodcall(L, LUA_TABLIBNAME, 6);
  }
  luaL_checkany(L, 2);
  lua_settop(L, 2);
  lua_rawget(L, 1);
  return 1;
}


/* }====================================================== */

static const luaL_Reg tab_funcs[] = {
  {"allocate", tbl_allocate},   /* added 0.29.2, 22.12.2009 */
  {"array", tbl_array},         /* added 2.30.1, July 30, 2022 */
  {"borders", tbl_borders},     /* added 2.14.10, 22/04/2019 */
  {"cleanse", tbl_cleanse},     /* added 4.9.0, February 08, 2025 */
  {"col", tbl_col},             /* added April 05, 2026 */
  {"concat", tbl_concat},       /* added 3.10.1, 29/01/2024 */
  {"entries", tbl_entries},     /* added 0.30.1, 03.01.2010 */
  {"extend", tbl_extend},       /* added 3.17.9, June 30, 2024 */
  {"getarray", tbl_getarray},   /* added 2.37.3, February 22, 2023 */
  {"getdim", tbl_getdim},       /* added 3.17.8, June 29, 2024 */
  {"getitem", tbl_getitem},     /* added 6.1.2, October 04, 2025 */
  {"gethash", tbl_gethash},     /* added 2.37.3, February 22, 2023 */
  {"getsize", tbl_getsize},     /* added 2.3.0 RC 4, November 08, 2014 */
  {"getsizes", tbl_getsizes},   /* added 2.21.4, July 02, 2020 */
  {"hasfield", tbl_hasfield},   /* added 2.21.0, June 04, 2020 */
  {"hash", tbl_hash},           /* added 2.30.1, July 30, 2022 */
  {"hashole", tbl_hashole},     /* added 3.9.0, January 02, 2024 */
  {"include", tbl_include},
  {"indices", tbl_indices},
  {"isall", tbl_isall},         /* added 3.10.2, February 03, 2024 */
  {"isarray", tbl_isarray},     /* added 3.1.0, July 12, 2023 */
  {"ishash", tbl_ishash},       /* added 3.1.0, July 12, 2023 */
  {"isnullarray", tbl_isnullarray},  /* added 3.9.0, January 02, 2024 */
  {"isone", tbl_isone},         /* added December 19, 2025 */
  {"isrectangular", tbl_isrectangular},  /* added 3.17.8, June 29, 2024 */
  {"issquare", tbl_issquare},   /* added 3.17.8, June 29, 2024 */
  {"iszero", tbl_iszero},       /* added August 12, 2024 */
  {"maxn", tbl_maxn},
  {"move", tbl_move},           /* added 2.21.1, June 12, 2020 */
  {"new", tbl_new},             /* added 2.28.1, May 30, 2022 */
  {"numunion", tbl_numunion},   /* added January 26, 2024 */
  {"numintersect", tbl_numintersect},  /* added January 26, 2024 */
  {"numminus", tbl_numminus},   /* added January 26, 2024 */
  {"pack", tbl_pack},           /* added 3.10.1, 29/01/2024 */
  {"parts", tbl_parts},         /* added 2.30.1, July 30, 2022 */
  {"remove", tbl_remove},       /* added 5.0.2, June 26, 2025 */
  {"reshuffle", tbl_reshuffle}, /* added 2.30.1, July 30, 2022 */
  {"resize", tbl_resize},       /* added 4.6.3, December 13, 2024 */
  {"subs", tbl_subs},           /* added 5.1.3, July 11, 2025 */
  {"swapcol", tbl_swapcol},     /* added on July 28, 2024 */
  {"swaprow", tbl_swaprow},     /* added on July 28, 2024 */
  {"tableoftables", tbl_tableoftables},  /* added 3.17.8, June 29, 2024 */
  {"transpose", tbl_transpose},  /* added 3.18.9, July 28, 2024 */
  {"unpack", tbl_unpack},       /* added 3.10.1, 29/01/2024 */
  {NULL, NULL}
};

LUALIB_API int luaopen_tables (lua_State *L) {
  /* register library */
  luaL_register(L, LUA_TABLIBNAME, tab_funcs);
  /* register tables.mt table with function to facilitate OOP-style calls */
  luaL_checkstack(L, 4, "not enough stack space");
  lua_pushstring(L, "mt");
  lua_createtable(L, 0, 1);
  lua_pushstring(L, "__index");
  lua_pushcfunction(L, tbl_getitem);
  lua_rawset(L, -3);
  lua_rawset(L, -3);
  return 1;
}

