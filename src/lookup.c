/*
** $Id: lookup.c,v 0.1 21/01/2024 $
** Lookup Library
** See Copyright Notice in agena.h
*/

#include <stdio.h>
#include <string.h>

#define lookup_c
#define LUA_LIB

#include "agena.h"
#include "agnxlib.h"
#include "agenalib.h"
#include "agncmpt.h"

typedef struct {
  /* We use an environment table for the lookup data. Directly operating with a Table* object unfortunately does not work as it
     often gets destroyed in a session. */
  int size;         /* number of all the keys */
  int sizeentries;  /* sum of all the elements */
  int type;         /* subtable or subset */
} Lookup;

#define checklookup(L, n)   (Lookup *)luaL_checkudata(L, n, "lookup")
#define islookup(L,n)       (luaL_isudata(L, n, "lookup") && agn_isutypeset(L, n))

/* The function creates an empty lookup table and returns it. */
static int lookup_new (lua_State *L) {
  Lookup *a;
  int la, lh;
  const char *type;
  la = agnL_optnonnegint(L, 1, 0);
  lh = agnL_optnonnegint(L, 2, 0);
  type = agnL_optstring(L, 3, "table");
  a = (Lookup *)lua_newuserdata(L, sizeof(Lookup));
  if (!a)
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "lookup.new");
  lua_setmetatabletoobject(L, -1, "lookup", 0);
  agn_setutypestring(L, -1, "lookup");
  lua_createtable(L, la, lh);
  lua_setfenv(L, -2);
  a->size = 0;
  a->sizeentries = 0;
  a->type = tools_streq(type, "table") ? LUA_TTABLE : LUA_TSET;
  return 1;
}


/* Inserts values into a table t of tables. If t[key] represents a table, value is added to the end of its array part. If t[key] is
   unassigned, then t[key] := [value]. */
static int aux_getfromfenvbykey (lua_State *L, int idx) {
  lua_getfenv(L, idx);
  lua_pushvalue(L, idx + 1);
  lua_gettable(L, -2);
  return lua_type(L, -1);
}

static int lookup_include (lua_State *L) {
  int i, nargs, type;
  Lookup *a = checklookup(L, 1);
  luaL_checkany(L, 3);
  nargs = lua_gettop(L);
  luaL_checkstack(L, nargs, "too many arguments");
  type = aux_getfromfenvbykey(L, 1);
  switch (type) {
    case LUA_TNIL: {
      agn_poptop(L);
      lua_getfenv(L, 1);
      lua_pushvalue(L, 2);
      if (a->type == LUA_TTABLE) {
        lua_createtable(L, nargs - 3 + 1, 0);
        for (i=3; i <= nargs; i++) {
          lua_pushvalue(L, i);  /* push value */
          lua_rawseti(L, -2, i - 2);  /* insert value into new subtable */
          a->sizeentries++;
        }
      } else {
        agn_createset(L, nargs - 3 + 1);
        for (i=3; i <= nargs; i++) {
          lua_pushvalue(L, i);  /* push value */
          lua_srawset(L, -2);
        }
        a->sizeentries += agn_ssize(L, -1);
      }
      /* new subtable is at the stack top, set it to lookup table */
      lua_settable(L, -3);
      a->size++;
      break;
    }
    case LUA_TTABLE: {
      for (i=3; i <= nargs; i++) {
        agn_rawinsertfrom(L, -1, i);
        a->sizeentries++;
      }
      break;
    }
    case LUA_TSET: {  /* 5.1.2 */
      size_t oldsize = agn_ssize(L, -1);
      for (i=3; i <= nargs; i++) {
        lua_pushvalue(L, i);  /* push value */
        lua_srawset(L, -2);
      }
      a->sizeentries += agn_ssize(L, -1) - oldsize;
      break;
    }
    default:
      luaL_error(L, "Error in " LUA_QS ": malformed lookup table.", "lookup.include");
  }
  agn_poptop(L);  /* pop subtable or nil */
  return 0;
}

static int aux_dropentry (lua_State *L, Lookup *a, int size) {  /* 5.1.2 */
  agn_poptop(L);  /* drop subtable */
  lua_getfenv(L, 1);  /* push entire lookup table t */
  lua_pushvalue(L, 2);
  lua_pushnil(L);
  lua_rawset(L, -3);
  if (a->size > 0) a->size--;
  if (a->sizeentries > 0) a->sizeentries -= size;
  return 1;
}

static int lookup_purge (lua_State *L) {
  int type, nargs, rc;
  Lookup *a = checklookup(L, 1);
  luaL_checkany(L, 2);
  nargs = lua_gettop(L);
  luaL_checkstack(L, 3, "not enough stack space");
  type = aux_getfromfenvbykey(L, 1);  /* the subtable or nil is now on the stack top */
  rc = 0;
  switch (type) {
    case LUA_TNIL: {  /* do nothing, return null */
      agn_poptop(L);  /* drop null, return false */
      break;
    }
    case LUA_TTABLE: {
      int size = agn_size(L, -1);  /* number of entries for the key */
      if (nargs == 3) {  /* delete the value for the given key; the subtable is on the stack top */
        int cnt = agn_deletefrom(L, 3, -1);
        a->sizeentries -= cnt;
        size -= cnt;
        /* modified subtable is on stack top */
        if (size != 0) {  /* not all values have been deleted: register new entry */
          agn_reorder(L, -1, 0); /* reshuffle the subtable to remove all holes */
          agn_poptop(L);  /* pop subtable */
          rc = 1;
        }
      }
      /* rc = 0: either two arguments have been given or the subtable has been completely emptied after
         the traversal above: delete it from the lookup table */
      if (!rc) {
        rc = aux_dropentry(L, a, size);
      }
      agn_poptop(L);  /* drop lookup table */
      break;
    }
    case LUA_TSET: {  /* 5.1.2 extension */
      size_t size = agn_ssize(L, -1);  /* number of entries for the key */
      if (nargs == 3) {  /* delete the value for the given key; the subset is on the stack top */
        size_t sizediff;
        lua_pushvalue(L, 3);
        lua_sdelete(L, -2);
        sizediff = size - agn_ssize(L, -1);
        a->sizeentries -= sizediff;
        size -= sizediff;
        if (size != 0) {  /* not all values have been deleted: register new entry */
          agn_poptop(L);  /* pop subtable */
          rc = 1;
        }
      }
      /* rc = 0: either two arguments have been given or the subset has been completely emptied after
         the traversal above: delete it from the lookup table */
      if (!rc) {
        rc = aux_dropentry(L, a, size);
      }
      agn_poptop(L);
      break;
    }
    default: {
      agn_poptop(L);
      luaL_error(L, "Error in " LUA_QS ": malformed lookup table.", "lookup.purge");
    }
  }
  lua_pushboolean(L, rc);
  return 1;
}


static int lookup_gettable (lua_State *L) {
  (void)checklookup(L, 1);
  if (lua_gettop(L) == 1) {
    lua_getfenv(L, 1);
  } else {
    luaL_checkany(L, 2);
    aux_getfromfenvbykey(L, 1);
  }
  return 1;
}


static int lookup_indices (lua_State *L) {
  Lookup *a = checklookup(L, 1);
  int nargs = lua_gettop(L);
  lua_getfenv(L, 1);
  lua_replace(L, 1);
  if (nargs == 1) {
    int i = 0;
    luaL_checkstack(L, 3, "not enough stack space");
    lua_createtable(L, a->size, 0);
    lua_pushnil(L);
    while (lua_next(L, 1) != 0) {
      lua_rawsetikey(L, -3, ++i);
    }
  } else {
    int flag;
    agn_intindices(L, 1, &flag);
  }
  return 1;
}


static int lookup_getsizes (lua_State *L) {
  Lookup *a = checklookup(L, 1);
  luaL_checkstack(L, 2, "not enough stack space");  /* 5.1.2 fix */
  lua_pushinteger(L, a->size);         /* number of keys */
  lua_pushinteger(L, a->sizeentries);  /* number of all the elements in the lookup table */
  return 2;
}


static int lookup_setsizes (lua_State *L) {
  Lookup *a = checklookup(L, 1);
  a->size = agn_checknonnegint(L, 2);
  a->sizeentries = agnL_optnonnegint(L, 3, a->sizeentries);
  return 0;
}


#define testsentinel(L, hassentinel) { \
  if ((hassentinel)) { \
    if (!lua_equal(L, -1, 1)) return 2; \
    else { \
      lua_pushnil(L); \
      return 1; \
    } \
  } \
}

/* based on luaB_nextone */
static int lookup_nextone (lua_State *L) {
  int hassentinel, offset;
  (void)checklookup(L, 1);
  luaL_checkstack(L, 2, "not enough stack space");  /* 5.1.2 fix */
  lua_getfenv(L, 1);
  lua_replace(L, 1);
  hassentinel = 0;
  offset = 1;
  if ( ( hassentinel = (lua_gettop(L) == 3) ) ) {  /* save sentinel to position 1, shifting up all other arguments */
    lua_pushvalue(L, 3);
    lua_insert(L, 1);
    offset++;
  }
  switch (lua_type(L, offset)) {
    case LUA_TTABLE: {
      lua_settop(L, offset + 1);  /* create a 2nd/3rd argument if there isn't one */
      if (lua_next(L, offset)) {
        testsentinel(L, hassentinel);
        return 2;
      } else {
        lua_pushnil(L);
        return 1;
      }
    }
    default: {
      luaL_error(L, "Error in " LUA_QS ": malformed lookup table.", "lookup.nextone");
    }
  }
  return 1;
}


/* Returns an iterator function that when called returns one element after another from lookup table a. If there are no more elements left, the iterator function returns `null`. Example usage:

a := lookup.new();
lookup.include(a, 'abc', 1, 2, 3, 4)
lookup.include(a, 'xyz', -1, -2, -3, -4)
f := lookup.iterate(a);
while x := [f()] do
   if empty x then break fi;
   print(x)
od;

> f():  # traversal complete, no more element left */
static int iterate (lua_State *L) {
  luaL_checkstack(L, 2, "not enough stack space");  /* 5.1.2 fix */
  lua_pushvalue(L, lua_upvalueindex(2));
  if (lua_next(L, lua_upvalueindex(1))) {
    lua_pushvalue(L, -2);
    lua_replace(L, lua_upvalueindex(2));
    return 2;  /* return key and value */
  }
  lua_pushnil(L);
  return 1;
}

static int lookup_iterate (lua_State *L) {  /* 3.9.6 */
  int nargs = lua_gettop(L);
  (void)checklookup(L, 1);
  luaL_checkstack(L, 2, "not enough stack space");  /* 3.18.4 fix */
  lua_getfenv(L, 1);
  if (nargs == 1)
    lua_pushnil(L);
  else
    lua_pushvalue(L, 2);
  lua_pushcclosure(L, &iterate, 2);
  return 1;
}


static int lookup_checktable (lua_State *L) {  /* 5.1.2 */
  int i, nargs;
  nargs = lua_gettop(L);
  for (i=1; i <= nargs; i++) {
    if (!islookup(L, i)) {
      lua_pop(L, i - 1);  /* pop previously pushed types */
      luaL_error(L, "Error in " LUA_QS ": argument #%d is not a lookup table.", "lookup.checktable", i);
    }
  }
  return 0;
}


static int lookup_istable (lua_State *L) {  /* 5.1.2 */
  int i, nargs;
  nargs = lua_gettop(L);
  luaL_checkstack(L, nargs, "not enough stack space");
  for (i=1; i <= nargs; i++) {
    lua_pushboolean(L, islookup(L, i));
  }
  return nargs;
}


/* Metamethods *******************************************************************************/

static int mt_empty (lua_State *L) {
  Lookup *a = checklookup(L, 1);
  lua_pushboolean(L, a->size == 0);
  return 1;
}


static int mt_filled (lua_State *L) {
  Lookup *a = checklookup(L, 1);
  lua_pushboolean(L, a->size != 0);
  return 1;
}


static int mt_tostring (lua_State *L) {
  Lookup *a = checklookup(L, 1);
  if (agn_getutype(L, 1)) {
    lua_pushfstring(L, "(%u, %u, %f, sub%s)", a->size, a->sizeentries,  /* 5.1.2 extension */
      a->size == 0 ? AGN_NAN : a->sizeentries/a->size, a->type == LUA_TTABLE ? "table" : "set");
    lua_concat(L, 2);
  } else
    luaL_error(L, "Error in " LUA_QS ": malformed lookup table.", "lookup.__tostring");
  return 1;
}


static int mt_in (lua_State *L) {  /* 3.9.6 */
  (void)checklookup(L, 2);
  lua_getfenv(L, 2);
  lua_pushnil(L);
  while (lua_next(L, -2)) {
    if (agn_in(L, 1, -1, 0)) {
      lua_pop(L, 3);
      lua_pushtrue(L);
      return 1;
    }
    agn_poptop(L);
  }
  agn_poptop(L);
  lua_pushfalse(L);
  return 1;
}


static int mt_notin (lua_State *L) {  /* 3.9.6 */
  (void)checklookup(L, 2);
  lua_getfenv(L, 2);
  lua_pushnil(L);
  while (lua_next(L, -2)) {
    if (agn_in(L, 1, -1, 0)) {
      lua_pop(L, 3);
      lua_pushfalse(L);
      return 1;
    }
    agn_poptop(L);
  }
  agn_poptop(L);
  lua_pushtrue(L);
  return 1;
}


static int mt_gc (lua_State *L) {  /* garbage collect deletes userdata automatically */
  lua_lock(L);
  (void)checklookup(L, 1);
  lua_setmetatabletoobject(L, 1, NULL, 1);
  lua_pushnil(L);  /* new 5.1.2, better sure than sorry */
  lua_setfenv(L, 1);
  lua_unlock(L);
  return 0;
}


static const struct luaL_Reg lookup_lib [] = {  /* metamethods for lookup tables */
  {"__index",  lookup_gettable},  /* read access, 3.9.6 */
  {"__size",   lookup_getsizes},  /* retrieve the number of unique elements */
  {"__in",     mt_in},            /* metamethod for `in` operator */
  {"__notin",  mt_notin},         /* metamethod for `notin` operator */
  {"__empty",  mt_empty},         /* metamethod for `empty` operator */
  {"__filled", mt_filled},        /* metamethod for `filled` operator */
  {"__tostring", mt_tostring},    /* metamethod for `filled` operator */
  {"__gc", mt_gc},                /* metamethod for garbage collection */
  {NULL, NULL}
};


static const luaL_Reg lookuplib[] = {
  {"checktable", lookup_checktable},
  {"getsizes",   lookup_getsizes},
  {"gettable",   lookup_gettable},
  {"include",    lookup_include},
  {"indices",    lookup_indices},
  {"istable",    lookup_istable},
  {"iterate",    lookup_iterate},
  {"new",        lookup_new},
  {"nextone",    lookup_nextone},
  {"purge",      lookup_purge},
  {"setsizes",   lookup_setsizes},
  {NULL, NULL}
};


/*
** Open lookup library
*/

LUALIB_API int luaopen_lookup (lua_State *L) {
  /* metamethods */
  luaL_newmetatable(L, "lookup");
  luaL_register(L, NULL, lookup_lib);
  /* register library */
  luaL_register(L, AGENA_LOOKUPLIBNAME, lookuplib);
  return 1;
}

