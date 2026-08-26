/*
** $Id: registry.c, initiated September 04, 2008 $
** Registry Access Library
** See Copyright Notice in agena.h
*/


/*
The function provides limited access to the registry, an interface between Agena and its C virtual machine which
mainly stores values used by userdata (see llist and numarray libraries), metatables of libraries written in C,
open files, and loaded libraries.

The package allows to read and write registry data in a limited way. It is useful if the debug library is not part
of a sandbox, see Chapter 6.15 Sandboxing.

The procedures provided by this package should be used instead of the debug.getregistry function which allows
to modify or even delete the registry which would jeopardise Agena integrity and stability. */

#include <stdlib.h>
#include <string.h>

#define registry_c
#define LUA_LIB

#include "agena.h"

#include "agenalib.h"
#include "agnhlps.h"
#include "agnxlib.h"

#if !(defined(__DJGPP__) || defined(__OS2__) || defined(LUA_ANSI))
#define AGENA_REGISTRYLIBNAME "registry"
LUALIB_API int (luaopen_registry) (lua_State *L);
#endif


/* This function is OBSOLETE and now just returns the string given. Use the string as the identifier required by registry.anchor and registry.get.
   See also: utils.uuid. */
static int registry_anyid (lua_State *L) {  /* 2.9.2 */
  /* Lightuserdata is not garbaged collected and causes invalid memory reads, so don't use: lua_pushlightuserdata(L, (void *)agn_checkstring(L, 1)); */
  luaL_checkany(L, 1);
  lua_settop(L, 1);
  /* just return the string; we use strings as integers will cause luaL_ref (used by numarrays, llists, ulists, etc.) to return non-unique ids. */
  return 1;
}


/* 2.16.7 conversion to macro */
#define aux_checkid(L, idx, procname) { \
  if (!agn_isstring(L, idx)) { \
    luaL_error(L, "Error in " LUA_QS ": string expected, got %s.", procname, luaL_typename(L, (idx))); \
  } \
}


static const char *const exclude[] = {"FILE*", "BINIOFILE*", "_LOADED", "_LOADLIB", NULL};  /* do not return the corresponding entries */

/* Inserts a new key ~ value pair into the registry, where the key is a light userdata object returned by
   register.anyid, and the value the corresponding data. If the key already exists in the registry, the
   function leaves the registry unchanged. The function returns nothing. */
static int registry_anchor (lua_State *L) {  /* 2.9.2, rewritten 2.16.6 = thrice as fast */
  int i;
  const char *needle;
  luaL_checkany(L, 2);
  aux_checkid(L, 1, "registry.anchor");
  needle = agn_tostring(L, 1);
  for (i=0; exclude[i]; i++) {
    if ((tools_streq(needle, exclude[i])) || (strstr(needle, "LOADLIB:") == needle)) {  /* argument starts with one of the keys in exclude ?  2.16.12 tweak */
      luaL_error(L, "Error in " LUA_QS ": invalid key %s.", "registry.anchor", needle);
      return 1;
    }
  }
  lua_pushvalue(L, LUA_REGISTRYINDEX);
  lua_pushvalue(L, 1);
  lua_rawget(L, -2);
  lua_pushvalue(L, 1);  /* push lud again */
  lua_pushvalue(L, 2);  /* push value to be entered */
  lua_settable(L, LUA_REGISTRYINDEX);
  agn_poptoptwo(L); /* leave nothing on stack: drop (registry value or null) and registry */
  return 0;  /* return nothing, 2.12.3 fix */
}


/* The function returns the registry value indexed by key, which may be of any type. If the registry entry is used by userdata,
   refers to loaded libraries or open files, the function just returns `null`. Otherwise, the entry is simply returned.
   If a C library metatable contains the __metatable read-only `metamethod`, `null` is returned, as well.

   With metatables defined by C libraries, it is still possible to delete or change metamethods, so extreme care should be
   taken when referencing to metatables returned by registry.get. Especially, the __gc metamethod must not be deleted or changed. */

static int registry_get (lua_State *L) {
  int i, nargs;
  nargs = lua_gettop(L);
  if (nargs == 0) {  /* changed back to `safety`, 5.4.1 */
    luaL_error(L, "Error in " LUA_QS ": use " LUA_QS ".", "registry.get", "debug.getregistry");
    return 1;
  }
  if (agn_isinteger(L, 1)) {  /* 2.16.6 change */
    lua_rawgeti(L, LUA_REGISTRYINDEX, agn_tointeger(L, 1));  /* push registry entry */
    if (lua_isuserdata(L, -1)) {  /* 5.4.1 change */
      agn_poptop(L);
      lua_pushnil(L);  /* do not return internal values occupied by userdata */
    }
    return 1;
  } else if (agn_isstring(L, 1)) {  /* do not return certain registry values */
    const char *needle = agn_tostring(L, 1);
    for (i=0; exclude[i]; i++) {
      if ((tools_streq(needle, exclude[i])) || (strstr(needle, "LOADLIB:") == needle)) {  /* argument starts with one of the keys in exclude ?  2.16.12 tweak */
        lua_pushnil(L);
        return 1;
      }
    }
  }
  /* get a specific field from registry */
  lua_pushvalue(L, LUA_REGISTRYINDEX);
  lua_pushvalue(L, 1);
  lua_rawget(L, -2);
  lua_remove(L, -2);  /* delete registry */
  /* is return a table and does it include a __metamethod read-only entry ? */
  if (lua_istable(L, -1)) {
    lua_pushnil(L);
    while (lua_next(L, -2)) {
      if ((agn_isstring(L, -2) && tools_streq(agn_tostring(L, -2), "__metatable")) || agn_freeze(L, -1, -1)) {  /* 2.16.12 tweak, 3.7.6 fix, 5.4.0 extension */
        lua_pop(L, 3);   /* delete value, key, and table */
        lua_pushnil(L);  /* push null instead */
        break;
      }
      agn_poptop(L);  /* delete value, leave key on stack */
    }
  }
  return 1;
}


/* Inserts a value into the Agena registry and returns a reference to it, an integer. See also: `registry.unref`. 5.4.0 */
static int registry_ref (lua_State *L) {
  int ref;
  luaL_checkany(L, 1);
  lua_settop(L, 1);
  ref = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_pushinteger(L, ref);
  return 1;
}


/* Returns the registry value with reference ref, an integer, and deletes it from the registry.
   See also: `registry.ref`. */
static int registry_unref (lua_State *L) {  /* 5.4.0 */
  int ref = luaL_checkinteger(L, 1);
  lua_rawgeti(L, LUA_REGISTRYINDEX, ref);  /* push registry entry */
  luaL_unref(L, LUA_REGISTRYINDEX, ref);  /* and purge it from the registry */
  return 1;  /* return purged entry */
}


static const luaL_Reg registrylib[] = {
  {"anchor", registry_anchor},        /* added October 31, 2015 */
  {"anyid", registry_anyid},          /* added October 31, 2015 */
  {"get", registry_get},              /* added October 31, 2015 */
  {"ref", registry_ref},              /* added August 09, 2025 */
  {"unref", registry_unref},          /* added August 09, 2025 */
  {NULL, NULL}
};


/*
** Open registry library
*/
LUALIB_API int luaopen_registry (lua_State *L) {
  luaL_register(L, AGENA_REGISTRYLIBNAME, registrylib);
  return 1;
}

