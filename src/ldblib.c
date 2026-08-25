/*
** $Id: ldblib.c,v 1.104 2005/12/29 15:32:11 roberto Exp $
** Interface from Lua/Agena to its debug API
** See Copyright Notice in agena.h
*/


#include <stdio.h>
#include <string.h>

#define ldblib_c
#define LUA_LIB

#include "agena.h"

#include "agenalib.h"
#include "agncmpt.h"
#include "agnconf.h"
#include "agnhlps.h"
#include "agnxlib.h"
#include "lapi.h"
#include "lcode.h"
#include "lfunc.h"
#include "lopcodes.h"
#include "lstate.h"


/* taken from Lua 5.2.4 for probable future use, 2.27.10 */
void checkstack (lua_State *L, lua_State *L1, int n) {
  if (L != L1 && !lua_checkstack(L1, n))
    luaL_error(L, "stack overflow");
}


static int db_getregistry (lua_State *L) {
  if (lua_gettop(L) == 0) {
    lua_pushvalue(L, LUA_REGISTRYINDEX);
  } else {  /* 5.0.1 extension */
    lua_pushvalue(L, 1);
    lua_gettable(L, LUA_REGISTRYINDEX);
  }
  return 1;
}


static int db_getconstants (lua_State *L) {  /* 2.20.0 */
  lua_pushvalue(L, LUA_CONSTANTSINDEX);
  return 1;
}


static int db_getmetatable (lua_State *L) {
  luaL_checkany(L, 1);
  if (!lua_getmetatable(L, 1)) {
    lua_pushnil(L);  /* no metatable */
  }
  return 1;
}


static int db_setmetatable (lua_State *L) {
  int t = lua_type(L, 2);
  luaL_typecheck(L, t == LUA_TNIL || t == LUA_TTABLE, 2,
                    "null or table expected", t);
  lua_settop(L, 2);
  lua_pushboolean(L, lua_setmetatable(L, 1));
  return 1;
}


static int db_getstore (lua_State *L) {  /* 2.33.1 */
  luaL_checktype(L, 1, LUA_TFUNCTION);  /* 5.1.6 change */
  if (!agn_getstorage(L, 1)) lua_pushnil(L);
  return 1;
}


static int db_setstore (lua_State *L) {  /* 3.5.3 */
  int t;
  luaL_checktype(L, 1, LUA_TFUNCTION);  /* 5.1.6 change */
  t = lua_type(L, 2);
  luaL_typecheck(L, t == LUA_TTABLE || t == LUA_TNIL, 2, "table or `null` expected", t);
  agn_setstorage(L, 1);
  if (!agn_getstorage(L, 1)) lua_pushnil(L);
  return 1;
}


static int db_getrtable (lua_State *L) {  /* 2.33.1 */
  int t = lua_type(L, 1);
  luaL_typecheck(L, t == LUA_TFUNCTION, 1, "procedure expected", t);
  if (!agn_getrtable(L, 1)) lua_pushnil(L);
  return 1;
}


static int db_getfenv (lua_State *L) {
  luaL_checkany(L, 1);  /* Lua 5.1.4 patch 5 */
  lua_getfenv(L, 1);
  return 1;
}


static int db_setfenv (lua_State *L) {
  luaL_checktype(L, 2, LUA_TTABLE);
  lua_settop(L, 2);
  if (lua_setfenv(L, 1) == 0)
    luaL_error(L, LUA_QL("setfenv")
                  " cannot change environment of given object.");
  return 1;
}


static void settabss (lua_State *L, const char *i, const char *v) {
  lua_pushstring(L, v);
  lua_setfield(L, -2, i);
}


static void settabsi (lua_State *L, const char *i, int v) {
  lua_pushinteger(L, v);
  lua_setfield(L, -2, i);
}


static void settabsb (lua_State *L, const char *i, int v) {  /* 2.10.4 */
  lua_pushboolean(L, v);
  lua_setfield(L, -2, i);
}


static lua_State *getthread (lua_State *L, int *arg) {
  *arg = lua_isthread(L, 1);
  return (*arg) ? lua_tothread(L, 1) : L;
}


static void treatstackoption (lua_State *L, lua_State *L1, const char *fname) {
  if (L == L1) {
    lua_pushvalue(L, -2);
    lua_remove(L, -3);
  } else
    lua_xmove(L1, L, 1);
  lua_setfield(L, -2, fname);
}


static int db_getinfo (lua_State *L) {
  lua_Debug ar;
  int arg, levelgiven;
  lua_State *L1 = getthread(L, &arg);
  const char *options = luaL_optstring(L, arg + 2, "flnSuav");  /* added 'a' 2.1.1 */
  levelgiven = 0;
  if (agn_isnumber(L, arg + 1)) {
    if (!lua_getstack(L1, (int)lua_tointeger(L, arg + 1), &ar)) {  /* only called to check whether level is okay */
      lua_pushnil(L);  /* level out of range */
      return 1;
    }
    levelgiven = 1;
  } else if (lua_isfunction(L, arg + 1)) {
    lua_pushfstring(L, ">%s", options);
    options = lua_tostring(L, -1);
    lua_pushvalue(L, arg + 1);  /* push function */
    lua_xmove(L, L1, 1);        /* move it onto stack L1 */
  } else
    return luaL_argerror(L, arg + 1, "procedure or level expected");
  if (!lua_getinfo(L1, options, &ar))  /* assigns selected fields in `ar' structure */
    return luaL_argerror(L, arg + 2, "invalid option");
  lua_createtable(L, 0, 2);
  if (strchr(options, 'S')) {
    settabss(L, "source", ar.source);
    settabss(L, "short_src", ar.short_src);
    settabsi(L, "linedefined", ar.linedefined);
    settabsi(L, "lastlinedefined", ar.lastlinedefined);
    settabss(L, "what", ar.what);
  }
  if (strchr(options, 'l'))
    settabsi(L, "currentline", ar.currentline);
  if (strchr(options, 'u'))
    settabsi(L, "nups", ar.nups);
  if (strchr(options, 'n')) {
    settabss(L, "name", ar.name);
    settabss(L, "namewhat", ar.namewhat);
  }
  if (strchr(options, 'a'))  /* number of arguments expected, 2.1.1 Rob Hoelz' LuaPowerPatch */
    settabsi(L, "arity", ar.arity);
  if (strchr(options, 'v'))  /* variable arguments mode, 2.1.1 Rob Hoelz' LuaPowerPatch */
    settabsb(L, "vararg", ar.hasvararg);
  if (strchr(options, 'c')) {  /* ar.i_ci info, 2.20.2 */
    if (!levelgiven)  /* function instead of level given ? */
      luaL_argerror(L, arg + 1, "level expected with `c' option");
    settabsi(L, "ci", ar.i_ci);
  }
  if (strchr(options, 'L'))
    treatstackoption(L, L1, "activelines");
  if (strchr(options, 'f'))
    treatstackoption(L, L1, "func");
  /* PATCH - checkglobals, by David Manura, 2008. */
  if (strchr(options, 'g'))
    treatstackoption(L, L1, "globals");
  if (strchr(options, 'k'))  /* new 7.0.1, get values internally treated as `constants` */
    treatstackoption(L, L1, "konstants");
  if (strchr(options, 'V')) {  /* return table with parameters and locals of an Agena function; 2.20.2 */
    const char *name;
    int i = 0;
    if (!levelgiven)  /* function instead of level given ? */
      luaL_argerror(L, arg + 1, "level expected with `V' option");
    lua_pushstring(L, "locals");
    lua_createtable(L, 8, 8);  /* array part will contain varnames, hash part varname ~ value pairs */
    do {
      name = lua_getlocal(L1, &ar, ++i);
      if (name) {
        if (tools_streq(name, "(*temporary)")) {  /* ignore */
          agn_poptop(L);
        } else {
          lua_xmove(L1, L, 1);
          agn_rawsetfield(L, -2, name);
          lua_pushstring(L, name);  /* put name into array part, regardless if assigned or unassigned */
          lua_rawseti(L, -2, i);
        }
      }
    } while(name);
    lua_rawset(L, -3);
  }
  return 1;  /* return table */
}


/* Returns the name of the function in which it (i.e. `debug.funcname`) has been called. It is a wrapper for
   "debug.getinfo(level, "n").name". By default, level 1 is used, but you may pass another level. If level is
   out of range, then fail is returned. The function may be useful to create more flexible error messages. */
static int db_funcname (lua_State *L) {  /* 2.11.4 */
  lua_Debug ar;
  int arg, level, status;
  lua_State *L1 = getthread(L, &arg);
  level = agnL_optinteger(L, arg + 1, 1);
  if (!lua_getstack(L1, level, &ar)) {
    lua_pushfail(L);  /* level out of range */
    return 1;
  }
  status = lua_getinfo(L1, "n", &ar);
  if (status == 0 || ar.name == NULL)
    lua_pushnil(L);
  else
    lua_pushstring(L, ar.name);
  return 1;
}


/* taken from https://github.com/stevenjohnstone/afl-lua/blob/v5.1/ltests.c,
   $Id: ltests.c,v 2.35 2006/01/10 12:50:00 roberto Exp roberto $

   Returns the values internally treated as `constants` of the function in which it (i.e. `debug.getk`)
   has been called. It is a wrapper for "debug.getinfo(level, "k")". By default, level 1 is used, but
   you may pass another level. If level is out of range, then fail is returned.
   Instead of a level, you may pass the function of interest. 7.0.1 */
static int db_getk (lua_State *L) {
  if (lua_isfunction(L, 1)) {
    Proto *p;
    int i;
    if (lua_iscfunction(L, 1))
      luaL_error(L, "Error in " LUA_QS ": C functions are not accepted.", "db.getk");
    p = clvalue(obj_at(L, 1))->l.p;
    lua_createtable(L, p->sizek, 0);
    for (i=0; i < p->sizek; i++) {
      luaA_pushobject(L, p->k + i);
      lua_rawseti(L, -2, i + 1);
    }
  } else {
    int arg, level, status;
    lua_Debug ar;
    lua_State *L1 = getthread(L, &arg);
    level = agnL_optinteger(L, arg + 1, 1);
    if (!lua_getstack(L1, level, &ar)) {  /* out of range? */
      lua_pushfail(L);  /* level out of range */
    } else {
      status = lua_getinfo(L1, "k", &ar);
      if (status == 0) {
        lua_pushnil(L);
      }
    }
  }
  return 1;
}


static int db_getlocal (lua_State *L) {
  int arg;
  lua_Debug ar;
  lua_State *L1 = getthread(L, &arg);
  const char *name;
  if (!lua_getstack(L1, agnL_checkint(L, arg + 1), &ar))  /* out of range? */
    return luaL_argerror(L, arg + 1, "level out of range");
  name = lua_getlocal(L1, &ar, agnL_checkint(L, arg + 2));
  if (name) {
    lua_xmove(L1, L, 1);
    lua_pushstring(L, name);
    lua_pushvalue(L, -2);
    return 2;
  }
  else {
    lua_pushnil(L);
    return 1;
  }
}


/* taken from https://github.com/stevenjohnstone/afl-lua/blob/v5.1/ltests.c,
   $Id: ltests.c,v 2.35 2006/01/10 12:50:00 roberto Exp roberto $ */
static int listlocals (lua_State *L) {
  Proto *p;
  int pc = luaL_optint(L, 2, 1) - 1;  /* 7.5.3 fix */
  int i = 0;
  const char *name;
  luaL_argcheck(L, lua_isfunction(L, 1) && !lua_iscfunction(L, 1),
                1, "Agena function expected");
  p = clvalue(obj_at(L, 1))->l.p;
  luaL_checkstack(L, 2, "not enough stack space");
  lua_createtable(L, 8, 0);  /* 7.6.4 change */
  while ((name = luaF_getlocalname(p, ++i, pc)) != NULL) {
    lua_pushstring(L, name);
    lua_rawseti(L, -2, i);
  }
  return 1;
}

static int db_getlocals (lua_State *L) {  /* 2.12.1, fixed 2.14.10 */
  int arg, i, j, minus, nparams, hasvarargs, extended;
  lua_Debug ar;
  const char *name;
  if (lua_isfunction(L, 1)) return listlocals(L);  /* 7.0.1 extension */
  /* level given */
  lua_State *L1 = getthread(L, &arg);
  extended = (lua_gettop(L) <= arg + 1);
  if (!lua_getstack(L1, agnL_checkint(L, arg + 1), &ar))  /* out of range? */
    return luaL_argerror(L, arg + 1, "level out of range");
  i = j = minus = 0;
  nparams = lua_getarity(L1, &ar, &hasvarargs);  /* ar->i_ci was allocated by calling lua_getstack before */
  if (nparams == -1)  /* 2.14.10 fix */
    return luaL_argerror(L, arg + 1, "level out of range");
  luaL_checkstack(L, 2 + 3*extended, "not enough stack space");  /* 4.7.1 */
  if (extended) lua_createtable(L, 8, 0);  /* table with unassigned local names, 2.14.10 */
  lua_createtable(L, 8, extended ? 8 : 0);  /* array part will contain varnames, hash part varname ~ value pairs */
  do {
    name = lua_getlocal(L1, &ar, ++i);
    if (name) {
      if (tools_streq(name, "(*temporary)")) {  /* ignore */
        agn_poptop(L); minus++;
      } else {
        lua_xmove(L1, L, 1);
        if (extended) {
          if (lua_isnil(L, -1)) {  /* put name into table of unassigned locals, 2.14.10 */
            agn_poptop(L);
            lua_pushstring(L, name);
            lua_rawseti(L, -3, ++j);
          } else  /* set name ~ value pair into hash part & pop value */
            agn_rawsetfield(L, -2, name);
        } else
          agn_poptop(L);  /* remove value, popped before if extended = 1 */
        lua_pushstring(L, name);  /* put name into array part, regardless if assigned or unassigned */
        lua_rawseti(L, -2, i);
      }
    }
  } while(name);
printf("extended=%d", extended);
  if (extended) {
    lua_pushinteger(L, i - 1 - minus);    /* number of local vars including params */
    lua_pushinteger(L, nparams);          /* number of params */
    lua_pushboolean(L, hasvarargs != 0);  /* varargs ? */
    lua_pushvalue(L, -5);                 /* table of unassigned local variables, 2.14.10 */
    lua_remove(L, -6);
  } else {
    lua_pushinteger(L, nparams);
  }
  return 2 + 3*extended;
}


static int db_setlocal (lua_State *L) {
  int arg;
  lua_State *L1 = getthread(L, &arg);
  lua_Debug ar;
  if (!lua_getstack(L1, agnL_checkint(L, arg + 1), &ar))  /* out of range? */
    return luaL_argerror(L, arg + 1, "level out of range");
  luaL_checkany(L, arg + 3);
  lua_settop(L, arg + 3);
  lua_xmove(L, L1, 1);
  lua_pushstring(L, lua_setlocal(L1, &ar, agnL_checkint(L, arg + 2)));
  return 1;
}


static int auxupvalue (lua_State *L, int get) {
  const char *name;
  int n = agnL_checkint(L, 2);
  luaL_checktype(L, 1, LUA_TFUNCTION);
  if (lua_iscfunction(L, 1)) return 0;  /* cannot touch C upvalues from Lua */
  name = get ? lua_getupvalue(L, 1, n) : lua_setupvalue(L, 1, n);
  if (name == NULL) return 0;
  lua_pushstring(L, name);
  lua_insert(L, -(get + 1));
  return get + 1;
}


static int db_getupvalue (lua_State *L) {
  return auxupvalue(L, 1);
}


static int db_setupvalue (lua_State *L) {
  luaL_checkany(L, 3);
  return auxupvalue(L, 0);
}


/* Returns all upvalues of an Agena or C closure in a table, plus the number of upvalues. With C functions, the first entry in the table
   array depicts the first upvalue, and so on. With Agena closures, the upvalue names and associated values are returned in a hash table.

   If there are no upvalues, the return is `null` plus zero. */
static int db_getupvalues (lua_State *L) {  /* 2.27.5/4.7.1 */
  int nupvals, option;
  nupvals = lua_nupvalues(L, 1);
  option = !agnL_optboolean(L, 2, 0);
  if (nupvals == -1)
    luaL_argerror(L, 1, "procedure expected");
  if (nupvals == 0)
    lua_pushnil(L);
  else {
    int i, iscfn;
    const char *varname;
    iscfn = lua_iscfunction(L, 1);
    luaL_checkstack(L, 3 - iscfn, "not enough stack space");
    lua_createtable(L, iscfn*nupvals, (!iscfn)*nupvals);
    for (i=1; i <= nupvals; i++) {
      varname = lua_getupvalue(L, 1, i);  /* does not push anything if `varname' is NULL */
      if (varname) {
        if (option) {
          if (tools_streq(varname, "")) {  /* C closure */
            lua_rawseti(L, -2, i);
          } else {  /* Agena function */
            lua_pushstring(L, varname);
            lua_insert(L, -2);
            lua_rawset(L, -3);
          }
        } else {  /* determine upvalue names for Agena closures only */
          agn_poptop(L);  /* drop value */
          if (tools_strneq(varname, "")) {
            lua_pushstring(L, varname);
            lua_rawseti(L, -2, i);
          }
        }
      }
    }
  }
  lua_pushinteger(L, nupvals);
  return 2;
}


/* Returns the number of upvalues in an Agena closure. The function does not accept closures written in C. 2.27.5 */
static int db_nupvalues (lua_State *L) {
  int nupvals = lua_nupvalues(L, 1);
  if (nupvals == -1)  /* 4.7.1 change */
    luaL_argerror(L, 1, "procedure expected");
  lua_pushinteger(L, nupvals);
  return 1;
}


static const char KEY_HOOK = 'h';


static void hookf (lua_State *L, lua_Debug *ar) {
  static const char *const hooknames[] =
    {"call", "return", "line", "count", "tail return"};
  lua_pushlightuserdata(L, (void *)&KEY_HOOK);
  lua_rawget(L, LUA_REGISTRYINDEX);
  lua_pushlightuserdata(L, L);
  lua_rawget(L, -2);
  if (lua_isfunction(L, -1)) {
    lua_pushstring(L, hooknames[(int)ar->event]);
    if (ar->currentline >= 0)
      lua_pushinteger(L, ar->currentline);
    else lua_pushnil(L);
    lua_assert(lua_getinfo(L, "lS", ar));
    lua_call(L, 2, 0);
  }
}


static int makemask (const char *smask, int count) {
  int mask = 0;
  if (strchr(smask, 'c')) mask |= LUA_MASKCALL;
  if (strchr(smask, 'r')) mask |= LUA_MASKRET;
  if (strchr(smask, 'l')) mask |= LUA_MASKLINE;
  if (count > 0) mask |= LUA_MASKCOUNT;
  return mask;
}


static char *unmakemask (int mask, char *smask) {
  int i = 0;
  if (mask & LUA_MASKCALL) smask[i++] = 'c';
  if (mask & LUA_MASKRET) smask[i++] = 'r';
  if (mask & LUA_MASKLINE) smask[i++] = 'l';
  smask[i] = '\0';
  return smask;
}


static void gethooktable (lua_State *L) {
  lua_pushlightuserdata(L, (void *)&KEY_HOOK);
  lua_rawget(L, LUA_REGISTRYINDEX);
  if (!lua_istable(L, -1)) {
    agn_poptop(L);
    lua_createtable(L, 0, 1);
    lua_pushlightuserdata(L, (void *)&KEY_HOOK);
    lua_pushvalue(L, -2);
    lua_rawset(L, LUA_REGISTRYINDEX);
  }
}


static int db_sethook (lua_State *L) {
  int arg, mask, count;
  lua_Hook func;
  lua_State *L1 = getthread(L, &arg);
  if (lua_isnoneornil(L, arg + 1)) {
    lua_settop(L, arg + 1);
    func = NULL; mask = 0; count = 0;  /* turn off hooks */
  }
  else {
    const char *smask = luaL_checkstring(L, arg + 2);
    luaL_checktype(L, arg + 1, LUA_TFUNCTION);
    count = agnL_optinteger(L, arg + 3, 0);
    func = hookf; mask = makemask(smask, count);
  }
  gethooktable(L);
  lua_pushlightuserdata(L, L1);
  lua_pushvalue(L, arg + 1);
  lua_rawset(L, -3);  /* set new hook */
  agn_poptop(L);  /* remove hook table */
  lua_sethook(L1, func, mask, count);  /* set hooks */
  return 0;
}


static int db_gethook (lua_State *L) {
  int arg;
  lua_State *L1 = getthread(L, &arg);
  char buff[5];
  int mask = lua_gethookmask(L1);
  lua_Hook hook = lua_gethook(L1);
  if (hook != NULL && hook != hookf)  /* external hook? */
    lua_pushliteral(L, "external hook");
  else {
    gethooktable(L);
    lua_pushlightuserdata(L, L1);
    lua_rawget(L, -2);   /* get hook */
    lua_remove(L, -2);  /* remove hook table */
  }
  lua_pushstring(L, unmakemask(mask, buff));
  lua_pushinteger(L, lua_gethookcount(L1));
  return 3;
}


static int db_debug (lua_State *L) {
  for (;;) {
    char buffer[250];
    fputs("agn_debug> ", stderr);
    if (fgets(buffer, sizeof(buffer), stdin) == 0 ||
        tools_streq(buffer, "cont\n") ||
        tools_streq(buffer, "cont\n"))  /* 7.0.2 change */
      return 0;
    if (luaL_loadbuffer(L, buffer, tools_strlen(buffer), "=(debug command)") ||  /* 2.25.1 tweak */
        lua_pcall(L, 0, 0, 0)) {
      fputs(lua_tostring(L, -1), stderr);
      fputs("\n", stderr);
    }
    lua_settop(L, 0);  /* remove eventual returns */
  }
}


#define LEVELS1	12	/* size of the first part of the stack */
#define LEVELS2	10	/* size of the second part of the stack */

/* additional error information: modified 0.5.2; patched 0.13.0, Febraury 24, 2009; patched 2.11.4 */

static int db_errorfb (lua_State *L) {
  int level, noqmark, prnl;
  int firstpart = 1;  /* still before eventual `...' */
  int arg;
  lua_State *L1 = getthread(L, &arg);
  lua_Debug ar;
  if (agn_isnumber(L, arg + 2)) {
    level = (int)lua_tointeger(L, arg + 2);
    agn_poptop(L);
  }
  else
    level = (L == L1) ? 1 : 0;  /* level 0 may be this own function */
  if (lua_gettop(L) == arg)
    lua_pushliteral(L, "");
  else if (!agn_isstring(L, arg + 1)) return 1;  /* message is not a string */
  else lua_pushliteral(L, "\n");
  lua_pushliteral(L, "\nStack traceback:");
  while (lua_getstack(L1, level++, &ar)) {
    if (level > LEVELS1 && firstpart) {
      /* no more than `LEVELS2' more levels? */
      if (!lua_getstack(L1, level + LEVELS2, &ar))
        level--;  /* keep going */
      else {
        lua_pushliteral(L, "\n\t...");  /* too many levels */
        while (lua_getstack(L1, level + LEVELS2, &ar))  /* find last levels */
          level++;
      }
      firstpart = 0;
      continue;
    }
    lua_getinfo(L1, "Snl", &ar);
    noqmark = !((tools_streq(ar.short_src, "[C]") || tools_streq(ar.short_src, "(tail call)"))  /* 2.25.1 tweak */
      && (*ar.what == 'C' || *ar.what == 't'));
    if (noqmark)
      lua_pushfstring(L, "\n   %s,", ar.short_src);  /* avoid printing '[C]: ?' */
    if ((prnl = (ar.currentline > 0)))
      lua_pushfstring(L, " at line %d", ar.currentline);
    /* if (*ar.namewhat != '\0')   is there a name?
        lua_pushfstring(L, " in function " LUA_QS, ar.name); */
    if (*ar.namewhat != '\0' && ar.name != NULL)  /* is there a name?  2.11.4 change */
      lua_pushfstring(L, " in " LUA_QS, ar.name);
    else {
      if (*ar.what == 'm')  /* main? */
        lua_pushfstring(L, " in main chunk");
      else if (*ar.what == 'C' || *ar.what == 't') {
        if (noqmark) lua_pushliteral(L, " ?");  /* C function or tail call */
      } else {
        lua_pushfstring(L, " at line %d", ar.linedefined);
      }
    }
    lua_concat(L, lua_gettop(L) - arg);
  }
  lua_concat(L, lua_gettop(L) - arg);
  return 1;
}


/*
** {=====================================================================================
** Disassembler, taken from https://github.com/stevenjohnstone/afl-lua/blob/v5.1/ltests.c
** $Id: ltests.h,v 2.16 2005/09/14 17:48:57 roberto Exp roberto $
** ======================================================================================
*/

static void setnameval (lua_State *L, const char *name, int val) {
  lua_pushstring(L, name);
  lua_pushinteger(L, val);
  lua_settable(L, -3);
}

#define GETLINE(f,pc)	(((f)->lineinfo) ? (f)->lineinfo[pc] : 0)
static char *buildop (Proto *p, int pc, char *buff) {
  Instruction i = p->code[pc];
  OpCode o = GET_OPCODE(i);
  const char *name = luaP_opnames[o];
  int line = GETLINE(p, pc);
  sprintf(buff, "(%4d) %4d - ", line, pc);
  switch (getOpMode(o)) {
    case iABC:
      sprintf(buff + tools_strlen(buff), "%-12s%4d %4d %4d", name,
              GETARG_A(i), GETARG_B(i), GETARG_C(i));
      break;
    case iABx:
      sprintf(buff + tools_strlen(buff), "%-12s%4d %4d", name, GETARG_A(i), GETARG_Bx(i));
      break;
    case iAsBx:
      sprintf(buff + tools_strlen(buff), "%-12s%4d %4d", name, GETARG_A(i), GETARG_sBx(i));
      break;
  }
  return buff;
}


/* Breaks down an Agena function f into its opcode instructions and returns them in a table.
   Each line in it starts with the respective line number in brackets. 7.0.1 */
static int db_listcode (lua_State *L) {
  int pc;
  Proto *p;
  luaL_argcheck(L, lua_isfunction(L, 1) && !lua_iscfunction(L, 1),
                 1, "Agena function expected");
  p = clvalue(obj_at(L, 1))->l.p;
  lua_newtable(L);
  setnameval(L, "maxstack", p->maxstacksize);
  setnameval(L, "numparams", p->numparams);
  for (pc=0; pc < p->sizecode; pc++) {
    char buff[100];
    lua_pushinteger(L, pc+1);
    lua_pushstring(L, buildop(p, pc, buff));
    lua_settable(L, -3);
  }
  return 1;
}

/* }====================================================== */


static int is_register_used (Instruction i, int reg) {
  OpCode op = GET_OPCODE(i);
  int a = GETARG_A(i);
  int b = GETARG_B(i);
  int c = GETARG_C(i);
  switch (op) {
    /* Special handling for your custom high-precision loops.
       Covers: index, limit, step, downto, correction, and accumulators. */
    case OP_FORLOOP:
    case OP_FORPREP:
    case OP_TFORLOOP:
    case OP_TFORPREP: {
      if (reg >= a && reg <= a + 6) return 1;
      break;
    }
    /* Table operations (B is the index) */
    case OP_SETTABLE:
    case OP_GETTABLE: {
      if (b == reg) return 1;
      if (!(c & 0x100) && (c & 0xFF) == reg) return 1; /* Check if C is a register and matches */
      break;
    }
    /* Standard Arithmetic and Logical (B and C are inputs) */
    case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV:
    case OP_POW: case OP_IPOW: case OP_MOD:
    case OP_EQ: case OP_LT: case OP_LE: {
      /* if (b == reg || c == reg) return 1; */
      if (!(b & 0x100) && (b & 0xFF) == reg) return 1;
      if (!(c & 0x100) && (c & 0xFF) == reg) return 1;
      break;
    }
    /* Move and Unary operations (B is the source) */
    case OP_MOVE: case OP_UNM: case OP_NOT: case OP_LEN: {
      if (b == reg) return 1;
      break;
    }
    /* Boolean and Nil loads (Ignore target A, check if used later) */
    case OP_LOADBOOL:
    case OP_LOADNIL: {
      break;
    }
    /* Tests and Conditional Jumps (A is the register being tested) */
    case OP_TEST: case OP_TESTSET: {
      if (a == reg) return 1;
      break;
    }
    case OP_RETURN: {
    /* RETURN A B: returns R(A) ... R(A + B - 2) */
      if (b > 1) { /* if b == 1, it's a 'return' with no values */
        if (reg >= a && reg <= (a + b - 2)) return 1;
      } else if (b == 0) {
        /* b == 0 means returns all results from a previous function call
       (e.g., return some_func()). We treat reg A as used. */
        if (reg >= a) return 1;
      }
      break;
    }
    /* Also add the Call opcodes, as they use registers for arguments */
    case OP_CALL:
    case OP_TAILCALL: {
    /* CALL A B C: R(A) is the function, arguments are R(A+1) ... R(A+B-1) */
      if (b > 1) {
        if (reg >= a && reg <= (a + b - 1)) return 1;
      } else if (b == 0) {
        if (reg >= a) return 1;
      }
      break;
    }
    case OP_FN: {  /* extended 7.5.3 */
      switch (c) {
        case OPR_ABS: case OPR_ARCTAN: case OPR_COS: case OPR_ENTIER: case OPR_EXP:
        case OPR_LNGAMMA: case OPR_INT: case OPR_FRAC: case OPR_LN: case OPR_SIGN: case OPR_SIN:
        case OPR_SQRT: case OPR_TAN: case OPR_ARCSIN: case OPR_ARCCOS: case OPR_ARCSEC:
        case OPR_SINH: case OPR_COSH: case OPR_TANH: case OPR_BNOT: case OPR_RECIP: case OPR_COSXX:
        case OPR_BEA: case OPR_FLIP: case OPR_CONJUGATE: case OPR_ANTILOG2: case OPR_ANTILOG10:
        case OPR_SIGNUM: case OPR_SINC: case OPR_CIS: case OPR_PEPS: case OPR_MEPS: case OPR_CELL:
        case OPR_SQUARE: case OPR_CUBE: case OPR_INVSQRT: case OPR_UNITY:
        case OPR_ASSIGNED: case OPR_TSUMUP: case OPR_EVEN: case OPR_ODD:
        case OPR_FINITE: case OPR_INFINITE: case OPR_IMAG: case OPR_LEFT: case OPR_BOTTOM:
        case OPR_NAN: case OPR_NEWSEQ: case OPR_NEWREG: case OPR_NEWUSET: case OPR_REAL: case OPR_RIGHT:
        case OPR_TYPEOF: case OPR_TEMPTY: case OPR_TFILLED: case OPR_TOP:
        case OPR_TYPE: case OPR_UNASSIGNED: case OPR_FRACTIONAL: case OPR_POP:
        case OPR_NUMERIC: case OPR_TMULUP: case OPR_QMDEV: case OPR_INTEGRAL: case OPR_ZERO: case OPR_NONZERO: {
          if (!(b & 0x100) && (b & 0xFF) == reg) return 1;
        }
      }
    }
    default:
      break;
  }
  return 0;
}

#define aux_issuecandidate(L,c,name,line,i,start,end,first,printonly) { \
  if (printonly) { \
    printf("Candidate '%s' around line %d, register [%d] (PC %d-%d)\n", \
      name, line, i, start, end); \
  } else { \
    lua_pushstring(L, name); \
    lua_pushinteger(L, line); \
    agn_createpair(L, -2, -1); \
    lua_rawseti(L, -4, c); \
    lua_pop(L, 2); \
  } \
}

static void analyze_proto (lua_State *L, Proto* f, int first, int printonly) {
  int i, j, c, pc;
  if (first && !printonly) {
    luaL_checkstack(L, 4, "not enough stack space");
    lua_createtable(L, 0, 0);
  }
  c = 0;
  for (i=0; i < f->sizelocvars; i++) {
    int unused = 1;
    int start = f->locvars[i].startpc;
    int end = f->locvars[i].endpc;
    const char* name = getstr(f->locvars[i].varname);
    if (name[0] == '(' || tools_streq(name, "lasterror")) continue;  /* 7.5.3 extension */
    /* 1. Only skip if the range is physically impossible (stripped by compiler) */
    if (start >= end && start > 0) {
      aux_issuecandidate(L, ++c, name, f->lineinfo[start], i, start, end, first, printonly);
      continue;
    }
    /* 2. Calculate the Actual Register Index */
    int reg_index = 0;
    for (j=0; j < i; j++) {
      /* If a variable started before or at the same time as this one,
         and is still alive when this one starts, it occupies a register. */
      if (f->locvars[j].startpc <= start && f->locvars[j].endpc > start) {
        reg_index++;
      }
    }
    /* 3. Loop Machinery Heuristic (OP_FORPREP at startpc) */
    if (start < f->sizecode) {
      OpCode op = GET_OPCODE(f->code[start]);
      if (op == OP_FORPREP || op == OP_TFORLOOP) {
        unused = 0;
      }
    }
    /* 4. The Usage Scan */
    if (unused) {
      for (pc = start; pc < end; pc++) {
        if (pc < f->sizecode && is_register_used(f->code[pc], reg_index)) {
          unused = 0;
          break;
        }
      }
    }
    if (unused) {
      aux_issuecandidate(L, ++c, name, f->lineinfo[start], i, start, end, first, printonly);
    }
  }
  for (i=0; i < f->sizep; i++) {
    analyze_proto(L, f->p[i], 0, printonly);
  }
}

/* Created by Gemini AI, April 29, 2026, 7.5.2 */
static int db_unused (lua_State* L) {
  int printonly = lua_gettop(L) == 2;
  if (lua_type(L, 1) != LUA_TFUNCTION) return 0;
  Closure* cl = (Closure*)lua_topointer(L, 1);
  if (cl->c.isC) return 0;
  analyze_proto(L, cl->l.p, 1, printonly);
  return !printonly;
}


static const luaL_Reg dblib[] = {
  {"debug",        db_debug},
  {"getfenv",      db_getfenv},
  {"funcname",     db_funcname},
  {"getconstants", db_getconstants},
  {"gethook",      db_gethook},
  {"getinfo",      db_getinfo},
  {"getlocal",     db_getlocal},
  {"getlocals",    db_getlocals},    /* 2.12.1, 07/07/2018 */
  {"getk",         db_getk},         /* 7.0.1, 02/03/2026 */
  {"getregistry",  db_getregistry},
  {"getrtable",    db_getrtable},    /* 2.33.1, 18/10/2022 */
  {"getstore",     db_getstore},     /* 2.33.1, 18/10/2022 */
  {"getmetatable", db_getmetatable},
  {"getupvalue",   db_getupvalue},
  {"getupvalues",  db_getupvalues},  /* 2.27.5, 24/04/2022 */
  {"listcode",     db_listcode},     /* 7.0.1, 02/03/2026 */
  {"nupvalues",    db_nupvalues},    /* 2.27.5, 24/04/2022 */
  {"setfenv",      db_setfenv},
  {"sethook",      db_sethook},
  {"setlocal",     db_setlocal},
  {"setmetatable", db_setmetatable},
  {"setstore",     db_setstore},     /* 3.5.3, 11/11/2023 */
  {"setupvalue",   db_setupvalue},
  {"traceback",    db_errorfb},
  {"unused",       db_unused},       /* 7.5.2, 29/04/2026 */
  {NULL, NULL}
};


LUALIB_API int luaopen_debug (lua_State *L) {
  luaL_register(L, LUA_DBLIBNAME, dblib);
  return 1;
}

