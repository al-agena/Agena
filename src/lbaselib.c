/* $Id: lbaselib.c,v 1.191 15.03.2009 12:38:02 roberto Exp $
** Basic library
   Revision 25.12.2009 07:53:37
** See Copyright Notice in agena.h
** In OS/2, you need Paul Smedley's GCC 8.3.0 to get correct results with arctan2.
*/

#include <ctype.h>
#include <math.h>    /* fabs, isnan, fmod, HUGE_VAL */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <conio.h>   /* getch */
#include <io.h>      /* access in MinGW */
#endif

#if defined(__unix__) || defined(__APPLE__) || defined(__HAIKU__)
#include <unistd.h>  /* access in UNIX */
#endif

#if defined(__OS2__)
#include <conio.h>   /* getch */
#include <unistd.h>  /* access */
#endif

#define lbaselib_c
#define LUA_LIB

#include "agena.h"

#include "agenalib.h"
#include "agncmpt.h"   /* for fmax */
#include "agnconf.h"
#include "agnhlps.h"   /* for Pi and Exp */
#include "agnxlib.h"
#include "cephes.h"    /* for gamma, PIO2 */
#include "charbuf.h"
#include "dblvec.h"
#include "dual.h"
#include "lbaselib.h"
#include "lcomplex.h"  /* agnCmplx_create */
#include "llex.h"      /* for type names */
#include "llimits.h"   /* for cast */
#include "loadlib.h"   /* for require subfunction fastload* */
#include "lopcodes.h"
#include "prepdefs.h"  /* FORCE_INLINE, etc. */


/*
** If your system does not support `stdout', you can just remove this function.
** If you need, you can define your own `print' function, following this
** model but changing `fputs' to put the strings at a proper place
** (a console window or a log file, for instance).
*/

/* this macro checks whether there is a user defined printing function for the requested object
   and if so calls this function; otherwise the macro is `skipped`. 3.17.5/6 security fix */

static int aux_userprint (lua_State *L, const char *fn, const char *enclose) {  /* 5.3.2 change */
  int rc;
  luaL_checkstack(L, 2, "not enough stack space");
  lua_getfield(L, -1, fn);
  if ( (rc = lua_isfunction(L, -1)) ) {
    if (enclose) fputs(enclose, stdout);
    lua_pushvalue(L, -3);
    lua_call(L, 1, 1);
    fputs(lua_tostring(L, -1), stdout);
    lua_pop(L, 3);
    if (enclose) fputs(enclose, stdout);
  } else {
    agn_poptop(L);
  }
  return rc;
}

static void aux_enclose (lua_State *L, const char *what, char *enclose) {  /* 5.3.2 change */
  luaL_checkstack(L, 3, "not enough stack space");
  lua_pushstring(L, enclose);
  lua_pushstring(L, lua_isnil(L, -2) ? "null" : what);
  lua_pushstring(L, enclose);
  lua_concat(L, 3);
  lua_remove(L, -2);
}

static int luaB_print (lua_State *L) {
  int i, pprinter, nonewline, nargs, checkoptions;
  char *delim, *encloseby;
  delim = tools_strdup("\t");
  if (!delim) luaL_error(L, "Error: memory allocation failed.");  /* 7.7.7 fix */
  encloseby = NULL;
  nargs = lua_gettop(L);
  nonewline = 0;
  checkoptions = 3;
  /* check for properly allocated slots will be done by agn_pairgetiall() */
  while (checkoptions-- && nargs != 0 && lua_ispair(L, nargs)) {  /* 0.30.4, 2.11.0 RC3 fix, 2.27.0 improvement */
    agn_pairgetiall(L, nargs);  /* get left and right value, set to stack index -2, -1, 7.7.8 */
    if (agn_isstring(L, -2)) {
      const char *option = agn_tostring(L, -2);
      if (tools_streq("nonewline", option)) {  /* 2.16.12/2.27.0 tweak */
        nargs--;
        nonewline = lua_toboolean(L, -1);  /* if -1 is not a boolean, returns 1, i.e. do not print nl */
      } else if (tools_streq("delim", option)) {  /* 2.16.12/2.27.0 tweak */
        nargs--;
        xfree(delim);  /* 5.3.1 fix */
        delim = tools_strdup(agn_checkstring(L, -1));  /* 2.27.0 tweak */
        if (!delim) luaL_error(L, "Error: memory allocation failed.");  /* 7.7.7 fix */
      } else if (tools_streq("enclose", option)) {  /* new 2.27.0 */
        nargs--;
        xfree(encloseby);  /* 5.3.1 fix */
        encloseby = tools_strdup(agn_checkstring(L, -1));
        if (!encloseby) luaL_error(L, "Error: memory allocation failed.");  /* 7.7.7 fix */
      }
    }
    agn_poptoptwo(L);  /* 0.30.4 */
  }
  for (i=1; i <= nargs; i++) {
    luaL_checkstack(L, 1, "not enough stack space");  /* 3.17.5 security fix */
    if (i > 1) fputs(delim, stdout);
    if ((pprinter = luaL_callmeta(L, i, "__tostring"))) {  /* is there a metamethod ?  Agena 1.3.2 fix */
      agn_poptop(L);  /* pop pretty printer */
    }
    lua_pushvalue(L, i);
    if (!pprinter) {  /* no __string metamethod ? */
      int type;
      luaL_checkstack(L, 1, "not enough stack space");  /* 3.17.5 security fix */
      type = agnL_gettablefield(L, "environ", "aux", "print", 1);  /* get environ.aux (print) function container, Agena 1.7.0 */
      if (type == LUA_TTABLE) {  /* environ.aux is assigned and a table ? */
        switch (lua_type(L, -2)) {
          case LUA_TTABLE:
            if (aux_userprint(L, "printtable", encloseby)) continue;
            break;
          case LUA_TSET:
            if (aux_userprint(L, "printset", encloseby)) continue;
            break;
          case LUA_TSEQ:
            if (aux_userprint(L, "printsequence", encloseby)) continue;
            break;
          case LUA_TREG:
            if (aux_userprint(L, "printregister", encloseby)) continue;
            break;
          case LUA_TPAIR:
            if (aux_userprint(L, "printpair", encloseby)) continue;
            break;
          case LUA_TCOMPLEX:
            if (aux_userprint(L, "printcomplex", encloseby)) continue;
            break;
          case LUA_TFUNCTION:  /* 0.28.0 */
            if (aux_userprint(L, "printprocedure", encloseby)) continue;
            break;
          default: break;
        }
      }
      agn_poptop(L);  /* remove environ.aux.<field> */
    }
    /* if no user-defined formatting function for the given structure could be found or if there is a `__string` metamethod
       for the object, then the next lines will be executed, otherwise they will be skipped. */
    if (encloseby && !agn_getenclose(L)) {  /* substitute value with enclosed string and leave on the stack; 7.8.10 fix */
      aux_enclose(L, lua_tostring(L, -1), encloseby);
    } else if (agn_isstring(L, -1)) {  /* 5.3.2 change */
      if (agn_getenclose(L)) {  /* enclose in single quotes, 3.10.2; 7.8.10 fix */
        aux_enclose(L, lua_tostring(L, -1), "\'");
      } else if (agn_getenclosedouble(L)) {  /* enclose in double quotes, 3.10.2; 7.8.10 fix */
        aux_enclose(L, lua_tostring(L, -1), "\"");
      } else if (agn_getenclosebackquotes(L)) {  /* enclose in backquotes, 3.10.2; 7.8.10 fix */
        aux_enclose(L, lua_tostring(L, -1), "`");
      }
    }
    agnL_printnonstruct(L, -1);  /* aux_printobject can only be called with a negative index */
    agn_poptop(L);
  }
  if (!nonewline) fputs("\n", stdout);
  fflush(stdout);
  xfreeall(delim, encloseby);
  return 0;
}


static int luaB_mprint (lua_State *L) {
  char *delim, *encloseby, *s;
  int i, nargs, width, height, nonewline, simple, checkoptions, iscmplx;
  size_t l, l0, l1;
  nargs = lua_gettop(L);
  delim = tools_strdup("    ");  /* sometimes \t is one space, suddenly 4 spaces or more in Windows */
  if (!delim) luaL_error(L, "Error: memory allocation failed.");  /* 7.7.7 fix */
  encloseby = NULL;
  simple = 0;
  nonewline = 0;
  nargs = lua_gettop(L);
  checkoptions = 4;
  /* check for properly allocated slots will be done by agn_pairgetiall() */
  while (checkoptions-- && nargs != 0 && lua_ispair(L, nargs)) {  /* 0.30.4, 2.11.0 RC3 fix, 2.27.0 improvement */
    agn_pairgetiall(L, nargs);  /* get left, right value, set to stack index -2, -1, 7.7.8 */
    if (agn_isstring(L, -2)) {
      const char *option = agn_tostring(L, -2);
      if (tools_streq("nonewline", option)) {  /* 2.16.12/2.27.0 tweak */
        nargs--;
        nonewline = lua_toboolean(L, -1);  /* if -1 is not a boolean, returns 1, i.e. do not print nl */
      } else if (tools_streq("simple", option)) {
        nargs--;
        simple = lua_toboolean(L, -1);
      } else if (tools_streq("delim", option)) {  /* 2.16.12/2.27.0 tweak */
        nargs--;
        xfree(delim);  /* 5.3.1 fix */
        delim = tools_strdup(agn_checkstring(L, -1));  /* 2.27.0 tweak */
        if (!delim) luaL_error(L, "Error: memory allocation failed.");  /* 7.7.7 fix */
      } else if (tools_streq("enclose", option)) {  /* new 2.27.0 */
        nargs--;
        xfree(encloseby);  /* 5.3.1 fix */
        encloseby = tools_strdup(agn_checkstring(L, -1));
        if (encloseby) luaL_error(L, "Error: memory allocation failed.");  /* 7.7.7 fix */
      }
    }
    agn_poptoptwo(L);  /* 0.30.4 */
  }
  if (nargs == 0) {
    xfreeall(delim, encloseby);
    return 0;
  }
  lua_getglobal(L, simple ? "tostring" : "tostringx");
  if (!lua_isfunction(L, -1)) {  /* 5.3.3 fix */
    xfreeall(delim, encloseby);
    agn_poptop(L);
    luaL_error(L, "Error in " LUA_QS ": could not find `tostringx`.", "mprint");
  }
  if (tools_shellgeom(&width, &height) == -1) {  /* 5.3.3 change */
    width = 80; height = 43;
  }
  l = 0;
  lua_pushliteral(L, "");
  for (i=1; i <= nargs; i++) {
    luaL_checkstack(L, 3 + (encloseby != NULL) + lua_iscomplex(L, i), "too many arguments");  /* 2.11.4/3.5.4 fix */
    if (encloseby) lua_pushstring(L, encloseby);
    lua_pushvalue(L, -2 - (encloseby != NULL));  /* function to be called */
    lua_pushvalue(L, i);  /* value to print */
    if ( (iscmplx = (simple && lua_iscomplex(L, i))) ) {
      lua_pushtrue(L);  /* print complex number in re+I*im notation */
    }
    if (lua_pcall(L, 1 + iscmplx, 1, 0) != 0 ||  /* 7.3.4 extension */
        lua_tolstring(L, -1, &l0) == NULL) {
      xfreeall(delim, encloseby);
      return luaL_error(L, LUA_QL("tostring") " must return a string to "
                           LUA_QL("mprint") ".");
    }
    if (encloseby) lua_pushstring(L, encloseby);
    lua_pushstring(L, i == nargs ? "" : delim);
    lua_tolstring(L, -1, &l1);
    l += l0 + l1 + 2*tools_strlen(encloseby);
    lua_concat(L, 3 + 2*(encloseby != NULL));
  }
  l = (width - l)/2;
  if (l < 1) {
    lua_pop(L, 2);  /* pop string and `tostring` */
    xfreeall(delim, encloseby);
    return 0;
  }
  s = agn_stralloc(L, l, "mprint", NULL);  /* 7.3.4 */
  for (i=0; i < l - 1; i++) s[i] = ' ';
  fputs(s, stdout);
  fputs(agn_tostring(L, -1), stdout);
  lua_pop(L, 2);  /* pop string and `tostring` */
  xfreeall(delim, encloseby, s);
  if (!nonewline) fputs("\n", stdout);
  fflush(stdout);
  return 0;
}


static int luaB_tonumber (lua_State *L) {
  int base, nargs, split;
  nargs = lua_gettop(L);
  split = 0;
  base = 10;
  if (nargs > 1) {  /* new 6.6.2 */
    if (agn_isinteger(L, 2)) base = agn_tointeger(L, 2);
    if (lua_istrue(L, nargs)) split = 1;
  }
  if (base == 10) {  /* standard conversion */
    lua_Number n;
    int exception;
    luaL_checkany(L, 1);
    switch (lua_type(L, 1)) {
      case LUA_TSTRING: case LUA_TNUMBER: {    /* changed 0.12.2/3.13.3 */
        n = agn_tonumberx(L, 1, &exception);
        if (exception) {  /* conversion failed ? Check whether string contains a complex value */
          int cexception;
#ifndef PROPCMPLX
          agn_Complex c;
          c = agn_tocomplexx(L, 1, &cexception);
#else
          lua_Number c[2];
          agn_tocomplexx(L, 1, &cexception, c);
#endif
          if (cexception == 0)
#ifndef PROPCMPLX
            agn_createcomplex(L, c);
#else
            agn_createcomplex(L, c[0], c[1]);
#endif
          else
            lua_pushvalue(L, 1);
        } else
          lua_pushnumber(L, n);
        break;
      }
      case LUA_TCOMPLEX: {
        if (!split) {
          lua_pushvalue(L, 1);
        } else {
          lua_Number a, b;
          agn_getcmplxparts(L, 1, &a, &b);
          luaL_checkstack(L, 2, "not enough stack space");
          lua_pushnumber(L, a);
          lua_pushnumber(L, b);
          return 2;
        }
        break;
      }
      default:
        lua_pushfail(L);  /* 3.7.7 fix: work as described in the documentation */
    }
    return 1;
  } else {
    char *s2;
    unsigned long n;
    const char *s1 = luaL_checkstring(L, 1);
    luaL_argcheck(L, 2 <= base && base <= 36, 2, "base out of range");
    n = strtoul(s1, &s2, base);
    if (s1 != s2) {  /* at least one valid digit? */
      while (isspace((unsigned char)(*s2))) s2++;  /* skip trailing spaces */
      if (*s2 == '\0') {  /* no invalid trailing characters? */
        lua_pushnumber(L, (lua_Number)n);
        return 1;
      }
    }
  }
  lua_pushfail(L);  /* else not a number */
  return 1;
}


static int luaB_error (lua_State *L) {
  int level = agnL_optinteger(L, 2, 1);
  lua_settop(L, 1);
  if (agn_isstring(L, 1) && level > 0) {  /* add extra information? */
    luaL_where(L, level);
    lua_pushvalue(L, 1);
    lua_concat(L, 2);
  }
  return lua_error(L);
}


static int luaB_getmetatable (lua_State *L) {
  luaL_checkany(L, 1);
  int option = agnL_optboolean(L, 2, 0);
  if (lua_isstring(L, 1) && option) {
    /* get a specific field from the Agena registry, 6.6.11, taken from registry.c/registry_get */
    lua_pushvalue(L, LUA_REGISTRYINDEX);
    lua_pushvalue(L, 1);
    lua_rawget(L, -2);
    lua_remove(L, -2);  /* delete registry */
    /* is return a table and does it include a __metamethod read-only entry ? */
    if (lua_istable(L, -1)) {
      lua_pushnil(L);
      while (lua_next(L, -2)) {
        if ((agn_isstring(L, -2) && tools_streq(agn_tostring(L, -2), "__metatable")) ||
          agn_freeze(L, -1, -1)) {  /* 7.0.2 fix */
          lua_pop(L, 3);   /* delete value, key, and table */
          lua_pushnil(L);  /* push null instead */
          break;
        }
        agn_poptop(L);  /* delete value, leave key on stack */
      }
    }
  } else {
    if (!lua_getmetatable(L, 1)) {
      lua_pushnil(L);
      return 1;  /* no metatable */
    }
    luaL_getmetafield(L, 1, "__metatable");
  }
  return 1;  /* returns either __metatable field (if present) or metatable */
}


static int luaB_setmetatable (lua_State *L) {
  int s, t, nargs;
  nargs = lua_gettop(L);
  s = lua_type(L, 1);
  t = lua_type(L, 2);
  luaL_typecheck(L, s >= LUA_TTABLE || s == LUA_TFUNCTION, 1,  /* do NOT include procedures, as __gc would be overwritten ! */
                    "table, set, sequence, register, pair or function expected", s);  /* 2.16.6 fix */
  luaL_typecheck(L, t == LUA_TNIL || t == LUA_TTABLE, 2, "null or table expected", t);
  if (agn_freeze(L, 1, -1))
    luaL_error(L, "Error in " LUA_QS ": %s is read-only.", "setmetatable", luaL_typename(L, 1));
  if (luaL_getmetafield(L, 1, "__metatable"))
    luaL_error(L, "Error in " LUA_QS ": cannot change a protected metatable.", "setmetatable");
  if (lua_iscfunction(L, 1) && lua_getmetatable(L, 1)) {
    agn_poptop(L);  /* drop metatable */
    luaL_error(L, "Error in " LUA_QS ": cannot set a metatable to a C library function.", "setmetatable");
  }
  if (nargs == 3) {  /* 4.7.3 extension */
    t = lua_type(L, 3);
    luaL_typecheck(L, t == LUA_TSTRING || t == LUA_TNIL, 3, "string or null expected", t);
    agn_setutype(L, 1, 3);
  }
  lua_settop(L, 2);
  lua_setmetatable(L, 1);
  return 1;
}


static int luaB_addtometatable (lua_State *L) {  /* 3.15.5 */
  int s, t, nargs;
  nargs = lua_gettop(L);
  s = lua_type(L, 1);
  t = lua_type(L, 2);
  luaL_typecheck(L, s >= LUA_TTABLE || s == LUA_TFUNCTION || s == LUA_TUSERDATA, 1,
                    "table, set, sequence, register, pair, function or userdata expected", s);  /* 2.16.6 fix */
  luaL_typecheck(L, t == LUA_TTABLE, 2, "table expected", t);
  if (agn_freeze(L, 1, -1))
    luaL_error(L, "Error in " LUA_QS ": %s is read-only.", "addtometatable", luaL_typename(L, 1));
  if (luaL_getmetafield(L, 1, "__metatable")) {
    agn_poptop(L);  /* 4.9.3 level stack */
    luaL_error(L, "Error in " LUA_QS ": cannot change a protected metatable.", "addtometatable");
  }
  if (lua_getmetatable(L, 1)) {
    lua_pushnil(L);
    while (lua_next(L, 2)) {
      if (!lua_isstring(L, -2) || !lua_isfunction(L, -1)) {
        agn_poptoptwo(L);  /* 4.9.3 level stack */
        luaL_error(L, "Error in " LUA_QS ": invalid table entry, expected a string key and a function.", "addtometatable");
      }
      if (tools_streq(agn_tostring(L, -2), "__gc")) {  /* 4.9.3 */
        agn_poptoptwo(L);
        luaL_error(L, "Error in " LUA_QS ": cannot set " LUA_QS " metamethod.", "addtometatable", "__gc");
      }
      lua_rawset2(L, -3);
    }
  } else
    luaL_error(L, "Error in " LUA_QS ": first argument does not have a metatable.", "addtometatable");
  if (nargs == 3) {  /* 4.7.3 extension */
    t = lua_type(L, 3);
    luaL_typecheck(L, t == LUA_TSTRING || t == LUA_TNIL, 3, "string or null expected", t);
    agn_setutype(L, 1, 3);
  }
  return 1;
}


static int luaB_rawequal (lua_State *L) {
  luaL_checkany(L, 1);
  luaL_checkany(L, 2);
  lua_pushboolean(L, lua_rawequal(L, 1, 2));
  return 1;
}


static int luaB_rawget (lua_State *L) {
  agnL_rawget(L, "rawget");
  return 1;
}


static int luaB_rawgeti (lua_State *L) {
  int s = lua_type(L, 1);
  int idx = agn_checkinteger(L, 2);
  switch (s) {
    case LUA_TTABLE:
      lua_rawgeti(L, 1, idx);
      break;
    case LUA_TSEQ:
      lua_seqrawgeti(L, 1, idx);
      break;
    case LUA_TREG:
      agn_regrawgeti(L, 1, idx);
      break;
    case LUA_TPAIR:
      if (idx != 1 && idx != 2)
        luaL_error(L, "Error in " LUA_QS ": pair index must be either 1 or 2, got %d.", "rawgeti", idx);
      agn_pairgeti(L, 1, idx);
      break;
    default:
      luaL_error(L, "Error in " LUA_QS ": table, pair, sequence or register expected, got %s.", "rawgeti",
        lua_typename(L, s));
  }
  return 1;
}


/* the switch/case version is a little bit slower */
static int luaB_rawset (lua_State *L) {
  int s = lua_type(L, 1);
  size_t nargs = lua_gettop(L);
  luaL_checkany(L, 2);  /* check if argument #2 has been given at all */
  switch (s) {
    case LUA_TTABLE: {
      if (nargs == 2) {  /* 0.21.0, index missing ? */
        lua_pushinteger(L, agn_size(L, 1) + 1);  /* determine next free index */
        lua_insert(L, 2);  /* move it into arg position #2 */
      } else
        luaL_checkany(L, 3);
      lua_settop(L, 3);
      lua_rawset(L, 1);
      break;
    }
    case LUA_TSET: {
      int delete = (nargs == 3 && lua_isnil(L, 3));  /* changed 2.9.1 */
      lua_settop(L, 2);  /* the value to be inserted or deleted is now at the top of the stack */
      if (delete)
        lua_sdelete(L, 1);
      else
        lua_srawset(L, 1);
      break;
    }
    case LUA_TSEQ: {
      if (nargs == 2) {  /* 0.21.0, index missing ? */
        lua_pushinteger(L, agn_seqsize(L, 1) + 1);  /* determine next free index */
        lua_insert(L, 2);  /* move it into arg position #2 */
      } else
        luaL_checkany(L, 3);
      lua_settop(L, 3);
      lua_seqrawset(L, 1);
      break;
    }
    case LUA_TPAIR: {
      luaL_checkany(L, 3);
      lua_settop(L, 3);
      agn_pairrawset(L, 1);
      break;
    }
    case LUA_TREG: {
      if (nargs == 2)
        luaL_error(L, "Error in " LUA_QS ": three values expected with registers.", "rawset");
      else
        luaL_checkany(L, 3);
      lua_settop(L, 3);
      agn_regset(L, 1);
      break;
    }
    default:
      luaL_error(L, "Error in " LUA_QS ": structure expected, got %s.", "rawset",
        lua_typename(L, s));
  }
  return 1;
}


static int luaB_rawseti (lua_State *L) {
  int s = lua_type(L, 1);
  int idx = agn_checkinteger(L, 2);
  luaL_checkany(L, 3);  /* check if argument #3 has been given at all */
  switch (s) {
    case LUA_TTABLE:
      lua_rawseti(L, 1, idx);
      break;
    case LUA_TSEQ: {
      switch (lua_seqrawseti(L, 1, idx)) {
        case -2:
          luaL_error(L, "Error in " LUA_QS ": cannot add " LUA_QS " to sequence.", "rawseti", "null");
          break;
        case -1:
          luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "rawseti");
          break;
        case 0:
          luaL_error(L, "Error in " LUA_QS ": index %d is out of range.", "rawseti", idx);
          break;
      }
      break;
    }
    case LUA_TREG:
      if (agn_regrawseti(L, 1, idx) == 0) {
        luaL_error(L, "Error in " LUA_QS ": index %d is out of range.", "rawseti", idx);
      }
      break;
    case LUA_TPAIR:
      if (!agn_pairseti(L, 1, idx)) {
        luaL_error(L, "Error in " LUA_QS ": pair index must be either 1 or 2, got %d.", "rawseti", idx);
      }
      break;
    default:
      luaL_error(L, "Error in " LUA_QS ": table, pair, sequence or register expected, got %s.", "rawseti",
        lua_typename(L, s));
  }
  lua_settop(L, 1);
  return 1;
}


static int luaB_pairs (lua_State *L) {  /* reintroduced and extended in 2.19.1, no changes */
  luaL_checktype(L, 1, LUA_TTABLE);
  luaL_checkstack(L, 3, "not enough stack space");  /* 4.7.1 fix */
  lua_pushvalue(L, lua_upvalueindex(1));  /* return luaB_nextone generator, */
  lua_pushvalue(L, 1);  /* state, */
  lua_pushnil(L);  /* and initial value */
  return 3;
}


static int ipairsaux (lua_State *L) {  /* reintroduced and extended in 2.19.1 */
  int s, i;
  s = lua_type(L, 1);
  i = luaL_checkint(L, 2);
  i++;  /* next value */
  lua_pushinteger(L, i);
  switch (s) {
    case LUA_TTABLE:
      lua_rawgeti(L, 1, i);
      break;
    case LUA_TSEQ:
      if (i > agn_seqsize(L, 1))
        lua_pushnil(L);
      else
        lua_seqrawgeti(L, 1, i);
      break;
    case LUA_TREG:
      if (i > agn_regsize(L, 1))
        lua_pushnil(L);
      else
        agn_regrawgeti(L, 1, i);
      break;
    case LUA_TSTRING: {
      size_t l;
      const char *str = lua_tolstring(L, 1, &l);
      if (i > l)
        lua_pushnil(L);
      else
        lua_pushchar(L, str[i - 1]);
      break;
    }
    default: {
      luaL_checkstack(L, 3, "not enough stack space");  /* 2.31.7 fix */
      if (agnL_getmetafield(L, 1, "__index") == 0)
        luaL_error(L, "Error: userdata has no metatable or '__index' field.");
      lua_pushvalue(L, 1);    /* push userdata */
      lua_pushinteger(L, i);  /* push index */
      if (lua_pcall(L, 2, 1, 0) != 0) {  /* to prevent `index-out of range` messages, etc. */
        agn_poptop(L);  /* remove error string */
        lua_pushnil(L);
      }
    }
  }
  return (lua_isnil(L, -1)) ? 1 : 2;  /* change: instead of returning nothing, return `null` instead */
}


static int luaB_ipairs (lua_State *L) {  /* reintroduced and extended in 2.19.1 */
  int s = lua_type(L, 1);
  luaL_typecheck(L,
     s == LUA_TTABLE || s == LUA_TSEQ || s == LUA_TREG || s == LUA_TSTRING || s == LUA_TUSERDATA, 1,
     "table, sequence, register, string or userdata expected", s);
  luaL_checkstack(L, 3, "not enough stack space");  /* 4.7.1 fix */
  lua_pushvalue(L, lua_upvalueindex(1));  /* return ipairsaux generator, */
  lua_pushvalue(L, 1);    /* state (or better: object), */
  lua_pushinteger(L, 0);  /* and initial value */
  return 3;
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

static int luaB_nextone (lua_State *L) {  /* 0.10.0: extended to handle sets, as well */
  int hassentinel, offset;
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
        testsentinel(L, hassentinel);  /* 2.12.1 */
        return 2;
      } else {
        lua_pushnil(L);
        return 1;
      }
    }
    case LUA_TSET: {
      lua_settop(L, offset + 1);  /* create a 2nd argument if there isn't one */
      if (lua_usnext(L, offset)) {
        testsentinel(L, hassentinel);  /* 2.12.1 */
        return 2;
      } else {
        lua_pushnil(L);
        return 1;
      }
    }
    case LUA_TSEQ: {
      lua_settop(L, offset + 1);  /* create a 2nd argument if there isn't one */
      if (lua_seqnext(L, offset)) {
        testsentinel(L, hassentinel);  /* 2.12.1 */
        return 2;
      } else {
        lua_pushnil(L);
        return 1;
      }
    }
    case LUA_TSTRING: {
      lua_settop(L, offset + 1);  /* create a 2nd argument if there isn't one */
      if (lua_strnext(L, offset)) {
        testsentinel(L, hassentinel);  /* 2.12.1 */
        return 2;
      } else {
        lua_pushnil(L);
        return 1;
      }
    }
    case LUA_TREG: {  /* 2.3.0 RC 3 */
      lua_settop(L, offset + 1);  /* create a 2nd argument if there isn't one */
      if (lua_regnext(L, offset)) {
        testsentinel(L, hassentinel);  /* 2.12.1 */
        return 2;
      } else {
        lua_pushnil(L);
        return 1;
      }
    }
    default: {
      luaL_error(L, "Error in " LUA_QS ": table, set, sequence, register or string expected, got %s.", "nextone",
        luaL_typename(L, offset));  /* 3.9.5 */
    }
  }
  return 1;
}


int load_aux (lua_State *L, int status) {  /* Agena 1.6.1: global declaration */
  if (status == 0)  /* OK? */
    return 1;
  else {
    lua_pushnil(L);
    lua_insert(L, -2);  /* put before error message */
    return 2;  /* return nil plus error message */
  }
}


static int luaB_loadstring (lua_State *L) {
  size_t l;
  const char *s = luaL_checklstring(L, 1, &l);
  const char *chunkname = luaL_optstring(L, 2, s);
  return load_aux(L, luaL_loadbuffer(L, s, l, chunkname));
}


static int luaB_loadfile (lua_State *L) {
  const char *fname = luaL_optstring(L, 1, NULL);
  return load_aux(L, luaL_loadfile(L, fname));
}


/*
** Reader for generic `load' function: `lua_load' uses the
** stack for internal stuff, so the reader cannot change the
** stack top. Instead, it keeps its resulting string in a
** reserved slot inside the stack.
*/
static const char *generic_reader (lua_State *L, void *ud, size_t *size) {
  (void)ud;  /* to avoid warnings */
  luaL_checkstack(L, 2, "too many nested functions");
  lua_pushvalue(L, 1);  /* get function */
  lua_call(L, 0, 1);  /* call it */
  if (lua_isnil(L, -1)) {
    *size = 0;
    return NULL;
  }
  else if (agn_isstring(L, -1)) {
    lua_replace(L, 3);  /* save string in a reserved stack slot */
    return lua_tolstring(L, 3, size);
  }
  else luaL_error(L, "reader function must return a string.");
  return NULL;  /* to avoid warnings */
}


static int luaB_load (lua_State *L) {
  int status;
  const char *cname = luaL_optstring(L, 2, "=(load)");
  luaL_checktype(L, 1, LUA_TFUNCTION);
  lua_settop(L, 3);  /* function, eventual name, plus one reserved slot */
  status = lua_load(L, generic_reader, NULL, cname);
  return load_aux(L, status);
}


static int luaB_run (lua_State *L) {  /* rewritten 1.6.0 */
  int n, r;
  const char *fname = luaL_optstring(L, 1, NULL);
  char *fn;
  n = lua_gettop(L);
  fn = (tools_exists(fname)) ? str_concat(fname, NULL) : str_concat(fname, ".agn", NULL);  /* 2.16.11 change */
  if (fn == NULL) luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "run");
  r = luaL_loadfile(L, fn);
  xfree(fn);
  if (r != 0) lua_error(L);
  lua_call(L, 0, LUA_MULTRET);
  return lua_gettop(L) - n;
}


static int luaB_assume (lua_State *L) {  /* taken from Lua 5.4.8, Agena 5.2.1 */
  if (likely(lua_toboolean(L, 1)))  /* condition is true? */
    return lua_gettop(L);  /* return all arguments */
  else {  /* error */
    luaL_checkany(L, 1);  /* there must be a condition */
    lua_remove(L, 1);  /* remove it */
    lua_pushliteral(L, "assumption failed!");  /* default message */
    lua_settop(L, 1);  /* leave only message (default if no other one) */
    return luaB_error(L);  /* call 'error' */
  }
}


static int luaB_unpack (lua_State *L) {
  int i, e, n, o;
  o = lua_type(L, 1);
  luaL_typecheck(L, o >= LUA_TTABLE && o <= LUA_TSET, 1, "table, pair, set, sequence or register expected", o);
  i = agnL_optinteger(L, 2, 1);
  e = luaL_opt(L, luaL_checkint, 3, (o == LUA_TTABLE) ? luaL_getn(L, 1) : agn_nops(L, 1));  /* 2.4.6 fix */
    /* 0.14.0: with tables, do not use agn_nops for it also counts hash values */
  if (i > e) return 0;  /* empty range; Lua 5.1.3 patch 4 */
  n = e - i + 1;  /* number of elements */
  if (n <= 0 || !lua_checkstack(L, n))  /* n <= 0 means arith. overflow */
    return luaL_error(L, "Error in " LUA_QS ": too many results to unpack.", "unpack");
  switch (o) {
    case LUA_TTABLE:
      lua_rawgeti(L, 1, i);  /* push arg[i] (avoiding overflow problems) */
      while (i++ < e)  /* push arg[i + 1...e] */
        lua_rawgeti(L, 1, i);
      break;
    case LUA_TSET:
      /* 3.20.0, the order of the values returned will vary, depending on how elements are internally stored in the set */
      lua_pushnil(L);
      while (lua_usnext(L, 1));
      break;
    case LUA_TSEQ:
      lua_seqrawgeti(L, 1, i);  /* push arg[i] (avoiding overflow problems) */
      while (i++ < e)  /* push arg[i + 1...e] */
        lua_seqrawgeti(L, 1, i);
      break;
    case LUA_TREG:
      agn_regrawgeti(L, 1, i);  /* push arg[i] (avoiding overflow problems) */
      while (i++ < e)  /* push arg[i + 1...e] */
        agn_regrawgeti(L, 1, i);
      break;
    case LUA_TPAIR:  /* 2.10.0 */
      /* check for properly allocated slots will be done by agn_pairgetiall() */
      agn_pairgetiall(L, 1);  /* 7.7.8 */
      break;
    default:
      lua_assert(0); break;  /* should not happen */
  }
  return n;
}


/* op(obj);
   op(i, obj [, options]);
   op(i:j, obj [, options]);
   op(t, obj [, options]i, );

   The function returns one or more elements from a table, set, pair, sequence or register.

   In the first form, `op` returns all the values in table, set, pair, sequence or register obj and thus works exactly as `unpack` does.

   > op([10, 20, 30]):
   10      20      30

   In the second form, by specifying an integer as the index i, `op` returns member obj[i] from table, sequence, register or pair obj.

   > op(2, ['a', 'b', 'c']):
   b

   In this form, if i is negative integer or zero, then by default the type of obj is returned and additionally - if existing - the
   user-defined type.

   > tbl := [-1~'-a', 0~'zero', 'a', 'b', 'c'];

   > settype(tbl, 'triple');

   > op(0, tbl):
   table   triple

   If you do not want this and want to access an element with an existing zero key, then pass the `gettype=false' as the last argument
   in the call:

   > op(0, tbl, gettype=false):
   zero

   If you want to retrieve a member with a negative key, then pass the `gettype=false' along with the `posrelat=false' options as the
   last two arguments.

   > op(-1, tbl, gettype=false, posrelat=false):
   -a

   Or, for short, to switch off all the defaults, pass `true` as the last argument in the call, not only in this form, but also
   in the third and fourth.

   > op(-1, tbl, true):
   -a

   In the third form, by passing a range i:j, with i and j integers, with i <= j, returns members obj[i] to obj[j] from table,
   sequence or register obj.

   > op(1:3, tbl):
   a       b       c

   In the fourth form, by receiving a table t of one or more integral indices i, j, ... returns all the members obj[i], obj[j], ...
   in table, sequence or register obj.

   > op([3, 1, 0], tbl):
   c       a       zero

   With all forms, if a member does not exist, the function returns `null` in the result.

   > op([4, 3, 1, 0], tbl):
   null    c       a       zero

   By default, in the second, third and fourth form, if you pass a negative index i, it represents the |i|-th element
   `from the right side` of obj, that is obj[size(obj) + i + 1], see `utils.posrelat` for details. If you want to access an
   element that has a zero or negative integral index - relates to tables only -, then pass the `posrelat=false` option
   as the last argument in the call, or just pass `true`.

   > op([3, 0, -1, 1], tbl, true):
   c       zero    -a      a

   The function with all its defaults emulates Maple's `op` function as much as possible with the exception that if an index
   is out-of-bounds, Agena will return `null` instead of an error. It has been introduced to facilitate porting code from
   Maple to Agena. 3.20.0 */

static void aux_checkopoptions (lua_State *L, int pos, int *nargs, int *gettype, int *posrelat) {
  if (*nargs >= pos && lua_isboolean(L, *nargs)) {
    *gettype = lua_isfalseorfail(L, *nargs);
    *posrelat = 0;
    (*nargs)--;
  } else {
    int checkoptions = 2;  /* check n options; CHANGE THIS if you add/delete options */
    *gettype = 1;
    *posrelat = 1;
    while (checkoptions-- && *nargs >= pos && lua_ispair(L, *nargs)) {
      agn_pairgetiall(L, *nargs);  /* get left, right value, set to stack index -2, -1, 7.7.8 */
      /* check for properly allocated slots will be done by agn_pairgetiall() */
      if (agn_isstring(L, -2)) {
        const char *option = agn_tostring(L, -2);
        if (tools_streq(option, "gettype")) {
          *gettype = agn_checkboolean(L, -1);
        } else if (tools_streq(option, "posrelat")) {
          *posrelat = agn_checkboolean(L, -1);
        } else {
          agn_poptoptwo(L);
          luaL_error(L, "Error in " LUA_QS ": unknown option " LUA_QS ".", "op", option);
        }
      }
      /* do not call lua_settop as it would corrupt the argument stack since we have already pushed values */
      (*nargs)--;
      agn_poptoptwo(L);
    }
  }
}

static void aux_opchecktype (lua_State *L, int tt, int gettype) {  /* 3.20.1 extension */
  switch (tt) {
    case LUA_TNUMBER: {
      lua_pushstring(L, tools_isint(agn_tonumber(L, 2)) ? llex_token2str(TK_INTEGER) : llex_token2str(TK_TNUMBER));
      break;
    }
    case LUA_TCOMPLEX: {
      lua_Number a, b;
      agn_getcmplxparts(L, 2, &a, &b);
      if (b == 0.0)
        lua_pushstring(L, !gettype && tools_isint(a) ? llex_token2str(TK_INTEGER) : llex_token2str(TK_TNUMBER));
      else
        lua_pushstring(L, llex_token2str(TK_TCOMPLEX));
      break;
    }
    default:
      lua_pushstring(L, luaL_typename(L, 2));
  }
}

static int agnB_op (lua_State *L) {
  int o1, nargs, nrets, gettype, posrelat, pos;
  o1 = lua_type(L, 1);
  nargs = lua_gettop(L);
  /* we need to treat pairs differently, 3.20.0 */
  pos = 2 + (
    (lua_type(L, 2) == LUA_TPAIR && (o1 == LUA_TNUMBER || o1 == LUA_TPAIR || o1 == LUA_TTABLE))
    || (o1 == LUA_TNUMBER && nargs == 2)  /* case: op(0, (any data)), 2.30.1 */
  );
  aux_checkopoptions(L, pos, &nargs, &gettype, &posrelat);
  if (nargs == 1) {  /* taken from luaB_unpack */
    if (o1 < LUA_TTABLE)
      nrets = 1 + (o1 == LUA_TCOMPLEX);
    else
      nrets = (o1 == LUA_TTABLE) ? luaL_getn(L, 1) : agn_nops(L, 1);
    if (1 > nrets) return 0;  /* empty range; Lua 5.1.3 patch 4 */
    if (nrets <= 0 || !lua_checkstack(L, nrets))  /* n <= 0 means arith. overflow */
      return luaL_error(L, "Error in " LUA_QS ": too many results to unpack.", "op");
    switch (o1) {
      case LUA_TTABLE:
        lua_rawgetirange(L, 1, 1, nrets);
        break;
      case LUA_TSET:
        /* the order of the values returned will vary, depending on how elements are internally stored in the set */
        lua_pushnil(L);
        while (lua_usnext(L, 1));
        break;
      case LUA_TSEQ:
        lua_seqrawgetinoerrrange(L, 1, 1, nrets);  /* 3.20.0 */
        break;
      case LUA_TREG:
        agn_reggetinoerrrange(L, 1, 1, nrets);  /* 3.20.0 */
        break;
      case LUA_TPAIR:
        /* check for properly allocated slots will be done by agn_pairgetiall() */
        agn_pairgetiall(L, 1);  /* 7.7.8 */
        break;
      case LUA_TCOMPLEX: {  /* 4.11.3 */
        lua_Number re, im;
        agn_getcmplxparts(L, 1, &re, &im);
        lua_pushnumber(L, re);
        lua_pushnumber(L, im);
        break;
      }
      default:  /* 4.11.3: just return the argument when given a number, boolean, string, function, userdata, null */
        nrets = 1;
    }
  } else if (nargs == 2) {
    int o2, n;
    lua_Integer idx;
    o2 = lua_type(L, 2);
    /* luaL_typecheck(L, o2 >= LUA_TTABLE && o2 <= LUA_TSET, 1, "table, set, pair, sequence or register expected", o2); */
    n = (o2 == LUA_TTABLE) ? luaL_getn(L, 2) : agn_nops(L, 2);  /* agn_nops returns 0 with non-structures */
    switch (o1) {
      case LUA_TNUMBER: {
        nrets = 1;
        idx = agn_tointeger(L, 1);
        if (gettype && idx <= 0) {  /* default behaviour with idx <= 0, as in Maple */
          aux_opchecktype(L, o2, 0);
          nrets += agn_getutype(L, 2);
        } else {
          if (posrelat)  /* see utils.posrelat */
            idx = tools_posrelat(idx, n);  /* with idx = 0, returns 0 */
          switch (o2) {
            case LUA_TTABLE:
              lua_rawgeti(L, 2, idx);
              break;
            case LUA_TSEQ:
              lua_seqrawgetinoerr(L, 2, idx);  /* issues an error of out-of-range */
              break;
            case LUA_TREG:
              agn_reggetinoerr(L, 2, idx);
              break;
            case LUA_TPAIR:
              if (idx < 1 || idx > 2)
                lua_pushnil(L);
              else
                agn_pairgeti(L, 2, idx);
              break;
            default:
              if (o2 <= LUA_TSTRING) {
                if (idx <= 0)
                  aux_opchecktype(L, o2, gettype);
                else
                  luaL_error(L, "Error in " LUA_QS ": improper op or subscript selector.", "op");
              } else if (o2 == LUA_TNIL) {  /* 4.12.1 */
                lua_pushnil(L);
              } else
                luaL_error(L, "Error in " LUA_QS ": table, pair, sequence or register expected for argument #2.", "op");
          }
        }
        break;
      }
      case LUA_TPAIR: {
        int p, q;
        agnL_pairgetiintegers(L, "op", 1, 0, &p, &q);
        if (posrelat) {
          p = tools_posrelat(p, n);
          q = tools_posrelat(q, n);
        }
        if (p > q)  /* 3.20.0 */
          luaL_error(L, "Error in " LUA_QS ": left-hand integer %d > right-hand integer %d.", "op", p, q);
        nrets = q - p + 1;
        luaL_checkstack(L, nrets, "too many results to unpack");
        switch (o2) {
          case LUA_TTABLE:
            lua_rawgetirange(L, 2, p, q);
            break;
          case LUA_TSEQ:
            lua_seqrawgetinoerrrange(L, 2, p, q);  /* 3.20.0 */
            break;
          case LUA_TREG:
            agn_reggetinoerrrange(L, 2, p, q);  /* 3.20.0 */
            break;
          case LUA_TNIL:  /* 4.12.1 */
            lua_pushnil(L);
            break;
          default:
            luaL_error(L, "Error in " LUA_QS ": expected a table, sequence or register for second argument.", "op");
        }
        break;
      }
      case LUA_TTABLE: {
        nrets = luaL_getn(L, 1);
        if (nrets <= 0 || !lua_checkstack(L, nrets))  /* n <= 0 means arith. overflow */
          return luaL_error(L, "Error in " LUA_QS ": too many results to unpack.", "op");
        idx = 1;
        switch (o2) {
          /* 1) get first index from table 1 (avoiding overflow problems) and if requested turn negint indices to positive,
             2) then get value from structure 2 and leave it on stack */
          case LUA_TTABLE:
            lua_rawgetiposrelat(L, 1, idx, n, posrelat);
            lua_rawget(L, 2);
            while (idx++ < nrets) {
              lua_rawgetiposrelat(L, 1, idx, n, posrelat);
              lua_rawget(L, 2);
            }
            break;
          case LUA_TSEQ:
            lua_rawgetiposrelat(L, 1, idx, n, posrelat);
            lua_seqrawget(L, 2, 1);
            while (idx++ < nrets) {
              lua_rawgetiposrelat(L, 1, idx, n, posrelat);
              lua_seqrawget(L, 2, 1);
            }
            break;
          case LUA_TREG:
            lua_rawgetiposrelat(L, 1, idx, n, posrelat);
            agn_regrawget(L, 2, 1);
            while (idx++ < nrets) {
              lua_rawgetiposrelat(L, 1, idx, n, posrelat);
              agn_regrawget(L, 2, 1);
            }
            break;
          case LUA_TNIL:  /* 4.12.1 */
            lua_pushnil(L);
            break;
          default:
            luaL_error(L, "Error in " LUA_QS ": expected a table, sequence or register for second argument.", "op");
        }
        nrets = --idx;
        break;
      }  /* of LUA_TTABLE */
      default: {
        nrets = 0;
        luaL_error(L, "Error in " LUA_QS ": expected a number, pair or table for first argument.", "op");
      }
    }  /* switch (o1) */
  } else {
    nrets = 0;
    luaL_error(L, "Error in " LUA_QS ": wrong number of arguments.", "op");
  }
  return nrets;
}


/* Drops a number of arguments and returns the remaining. d is the number of arguments to drop. r1 to rn are
   the arguments. The function returns rd+1 to rn. This is useful to avoid creation of dummy variables.

   Based on: LuaSocket 3.1.0 package; license Copyright 2004-2022 Diego Nehab, file luasocket.c; 7.6.10 */

static int agnB_drop (lua_State *L) {
  int nargs, amount, ret;
  nargs = lua_gettop(L);
  amount = agn_checkinteger(L, 1);
  if (amount >= 0) {
    ret = nargs - amount - 1;
  } else {  /* Agena extension */
    int tail = (-amount <= nargs) ? -amount : nargs;
    lua_pop(L, tail);
    ret = nargs - tail - 1;
  }
  return ret >= 0 ? ret : 0;
}


static int luaB_protect (lua_State *L) {  /* Agena 1.0.3 */
  int status;
  luaL_checkany(L, 1);
  /* delete value of lasterror */
  lua_pushnil(L);
  lua_setglobal(L, "lasterror");
  status = lua_pcall(L, lua_gettop(L) - 1, LUA_MULTRET, 0);
  if (status == 0) {  /* successful ? */
    return lua_gettop(L);  /* return all results */
  } else {
    lua_pushvalue(L, -1); /* Agena 1.4.3/1.5.0, duplicate error message */
    lua_setglobal(L, "lasterror");  /* set error message to lasterror, and pop it */
    return 1;  /* return error message */
  }
}


static int luaB_xpcall (lua_State *L) {
  int status, nargs;
  luaL_checkany(L, 2);
  nargs = lua_gettop(L) - 2;
  lua_rotate(L, 2, nargs);  /* 2.22.0, rotate error function to the top of the stack */
  lua_insert(L, 1);  /* put error function under function to be called */
  status = lua_pcall(L, nargs, LUA_MULTRET, 1);  /* call function with nargs arguments */
  lua_pushboolean(L, (status == 0));
  lua_replace(L, 1);  /* replace error function by status */
  return lua_gettop(L);  /* return status + all results */
}


static int luaB_tostring (lua_State *L) {
  int catcmplx;
  luaL_checkany(L, 1);
  catcmplx = lua_gettop(L) > 1;
  if (luaL_callmeta(L, 1, "__tostring"))  /* is there a metafield? */
    return 1;  /* use its value */
  switch (lua_type(L, 1)) {  /* 4.10.0 call luaV_tostring as much as possible */
    case LUA_TNUMBER: case LUA_TBOOLEAN:
      lua_pushstring(L, lua_tostring(L, 1));
      break;
    case LUA_TSTRING:
      lua_pushvalue(L, 1);
      break;
    case LUA_TNIL:  /* do not move this to luaV_tostring as a lot of functions would not work any longer */
      lua_pushliteral(L, "null");
      break;
    case LUA_TCOMPLEX: {  /* 0.14.0 */
      lua_Number a, b;
      agn_getcmplxparts(L, 1, &a, &b);
      luaL_checkstack(L, 2 + 2*catcmplx*(a != 0), "not enough stack space");
      if (catcmplx) {  /* 4.1.0 innovation */
        lua_pushstring(L, lua_tostring(L, 1));  /* 4.10.0 optimisation */
      } else {
        lua_pushnumber(L, a);
        lua_pushstring(L, lua_tostring(L, -1));
        lua_remove(L, -2);  /* delete a */
        lua_pushnumber(L, b);
        lua_pushstring(L, lua_tostring(L, -1));
        lua_remove(L, -2);  /* delete b */
      }
      return 2 - catcmplx;
    }
    case LUA_TPAIR: {   /* 0.14.0 */
      luaL_checkstack(L, 3, "not enough stack space");  /* 3.17.5 security fix */
      lua_pushnumber(L, 1);  /* get left-hand side */
      agn_pairrawget(L, 1);  /* and put it onto the stack top, deleting 1 */
      lua_pushstring(L, lua_tostring(L, -1));
      lua_remove(L, -2);     /* remove left-hand side */
      lua_pushnumber(L, 2);  /* get right-hand side */
      agn_pairrawget(L, 1);  /* and put it onto the stack top, deleting 2 */
      lua_pushstring(L, lua_tostring(L, -1));
      lua_remove(L, -2);     /* remove right-hand side */
      return 2;
    }
    default:
      lua_pushfstring(L, "%s(%p)", luaL_typename(L, 1), lua_topointer(L, 1));
      break;
  }
  return 1;
}


static int luaB_newproxy (lua_State *L) {
  lua_settop(L, 1);
  lua_newuserdata(L, 0);  /* create proxy */
  if (lua_toboolean(L, 1) == 0) {
    return 1;  /* no metatable */
  } else if (lua_isboolean(L, 1)) {
    lua_newtable(L);  /* create a new metatable `m' ... */
    lua_pushvalue(L, -1);  /* ... and mark `m' as a valid metatable */
    lua_pushboolean(L, 1);
    lua_rawset(L, lua_upvalueindex(1));  /* weaktable[m] = true */
  } else {
    int validproxy = 0;  /* to check if weaktable[metatable(u)] == true */
    if (lua_getmetatable(L, 1)) {
      lua_rawget(L, lua_upvalueindex(1));
      validproxy = lua_toboolean(L, -1);
      agn_poptop(L);  /* remove value */
    }
    luaL_argcheck(L, validproxy, 1, "boolean or proxy expected");
    lua_getmetatable(L, 1);  /* metatable is valid; get it */
  }
  lua_setmetatable(L, 2);
  return 1;
}

/* ADDITIONS */

static int agnB_time (lua_State *L) {  /* 2.12.4 change for C's clock() only returns CPU time and not the time elapsed since an epoch */
  lua_pushnumber(L, (lua_Number)tools_time());
  return 1;
}


#define has_query(L, pvalue, pop) { \
  if (lua_type(L, -1) > LUA_TTHREAD) { \
    lua_pushvalue(L, pvalue); \
    agnB_has_aux(L); \
    if (lua_istrue(L, -1)) { \
      lua_pop(L, pop); \
      flag = 1; \
      break; \
    } \
  } \
  agn_poptop(L); }

static int agnB_has_aux (lua_State *L) {  /* Agena 1.6.13, 3 to 7 times faster than the former Agena implementation */
  int flag, isstruct;
  flag = isstruct = 0;
  if (lua_checkstack(L, 2) == 0)
    luaL_error(L, "Error in " LUA_QS ": stack cannot be extended.", "has");
  if (lua_equal(L, -1, -2)) {
    agn_poptoptwo(L);
    lua_pushtrue(L);
    return 1;
  }
  switch (lua_type(L, -2)) {
    case LUA_TTABLE: {
      lua_pushnil(L);
      while (lua_next(L, -3)) {
        if (lua_equal(L, -1, -3)) {
          agn_poptoptwo(L);  /* pop key and value */
          flag = 1;
          break;
        } else if (!agn_isnumber(L, -3)) {  /* x :- number and assigned(struct[x]) */
          lua_pushvalue(L, -3);  /* push second argument */
          lua_gettable(L, -5);   /* get entry */
          if (!lua_isnil(L, -1)) {
            lua_pop(L, 3);  /* remove entry, value, and key */
            flag = 1;
            break;
          } else {
            agn_poptop(L);  /* remove entry */
          }
        }
        if (!isstruct && lua_type(L, -1) > LUA_TTHREAD) isstruct = 1;  /* 1.6.14 */
        agn_poptop(L);  /* remove value */
      }
      break;
    }
    case LUA_TSET: {
      lua_pushvalue(L, -1);
      lua_srawget(L, -3);  /* pushes true or false on the stack */
      if (lua_istrue(L, -1)) {  /* 5.0.1 fix, better sure than sorry */
        flag = 1;
      }
      agn_poptop(L);  /* pop true or false */
      if (!flag) isstruct = 1;  /* 1.6.14 */
      break;
    }
    case LUA_TSEQ: {
      int i;
      for (i=0; i < agn_seqsize(L, -2); i++) {
        lua_seqrawgeti(L, -2, i + 1);
        if (lua_equal(L, -1, -2)) {
          agn_poptop(L);
          flag = 1;
          break;
        } else {
          if (!isstruct && lua_type(L, -1) > LUA_TTHREAD) isstruct = 1;  /* 1.6.14 */
          agn_poptop(L);
        }
      }
      break;
    }
    case LUA_TREG: {  /* 2.3.0 RC 3 */
      int i;
      for (i=0; i < agn_reggettop(L, -2); i++) {
        agn_regrawgeti(L, -2, i + 1);
        if (lua_equal(L, -1, -2)) {
          agn_poptop(L);
          flag = 1;
          break;
        } else {
          if (!isstruct && lua_type(L, -1) > LUA_TTHREAD) isstruct = 1;  /* 1.6.14 */
          agn_poptop(L);
        }
      }
      break;
    }
    case LUA_TPAIR: {
      int i;
      for (i=1; i < 3; i++) {
        agn_pairgeti(L, -2, i);
        if (lua_equal(L, -1, -2)) flag = 1;
        agn_poptop(L);
        if (flag) break;
      }
      if (!flag) {
        agn_createseq(L, 2);
        for (i=1; i < 3; i++) {
          agn_pairgeti(L, -3, i);
          lua_seqrawseti(L, -2, i);
        }
        lua_remove(L, -3); lua_insert(L, -2);
        isstruct = 1;  /* 1.6.14 */
      }
      break;
    }
    case LUA_TFAIL: case LUA_TNIL: case LUA_TNONE: {  /* 1.6.14 */
      luaL_error(L, "Error in " LUA_QS ": type %s not accepted.", "has", luaL_typename(L, -2));
    }
    default: {  /* = case LUA_TNUMBER: case LUA_TBOOLEAN: case LUA_TCOMPLEX: case LUA_TSTRING: case LUA_TFUNCTION: case LUA_TTHREAD:
                     case LUA_TUSERDATA: case LUA_TLIGHTUSERDATA */
      agn_poptoptwo(L);
      lua_pushfail(L);
      return 1;
    }
  }
  if (!flag && isstruct) {  /* 1.6.14 */
    switch (lua_type(L, -2)) {
      case LUA_TTABLE: {
        lua_pushnil(L);
        while (lua_next(L, -3)) {
          has_query(L, -3, 2);
        }
        break;
      }
      case LUA_TSET: {
        lua_pushnil(L);
        while (lua_usnext(L, -3)) {
          has_query(L, -3, 2);
        }
        break;
      }
      /* we do not need to check for pairs for a pair traversed above has been converted to a sequence */
      case LUA_TSEQ: {
        int i;
        for (i=0; i < agn_seqsize(L, -2); i++) {
          lua_seqrawgeti(L, -2, i + 1);
          has_query(L, -2, 1);
        }
        break;
      }
      case LUA_TREG: {  /* 2.3.0 RC 3 */
        int i;
        for (i=0; i < agn_reggettop(L, -2); i++) {
          agn_regrawgeti(L, -2, i + 1);
          has_query(L, -2, 1);
        }
        break;
      }
      default: {
        luaL_error(L, "Error in " LUA_QS ": type %s not accepted.", "has", luaL_typename(L, -2));
      }
    }
  }
  agn_poptoptwo(L);  /* remove first and second argument */
  lua_pushboolean(L, flag);
  return 1;
}


static int agnB_has (lua_State *L) {
  if (lua_gettop(L) != 2) {
    luaL_error(L, "Error in " LUA_QS ": requires two arguments.", "has");
  }
  /* Checks whether at least one character in string s is included in string p and returns `true` or `false`.
     See also strings.contains. 2.31.2 */
  if (agn_isstring(L, 1)) {
    const char *s, *p;
    size_t l;
    s = lua_tolstring(L, 1, &l);
    p = agn_checkstring(L, 2);
    lua_pushboolean(L, tools_hasstrchr(s, p, l));  /* 2.38.0, 25 % tuning */
  } else {
    lua_pushvalue(L, 1);
    lua_pushvalue(L, 2);
    agnB_has_aux(L);
  }
  return 1;
}


/* pushes function pvalue, calls agnB_recurse_aux, and then pops function again */

#define recurse_query(L, proc, pop, collect, offset, type, procname, first, last, quit, ignorehash) { \
  if (lua_type(L, -1) > LUA_TTHREAD) { \
    lua_pushvalue(L, proc); \
    agnB_recurse_aux(L, collect, offset, type, procname, first, last, quit, ignorehash); \
    if ((!collect) && lua_isboolean(L, -1) && ((quit && agn_istrue(L, -1)) || (!quit && agn_isfalse(L, -1)) )) { \
      flag = lua_toboolean(L, -1); \
      lua_pop(L, pop); \
      break; \
    } \
  } \
  agn_poptop(L); \
}

static void recurse_checkoption (lua_State *L, int pos, int *nargs, int *ignorehash, const char *procname) {
  int checkoptions;
  *ignorehash = 0;
  /* check for options, here `map in-place` */
  checkoptions = 1;  /* check n options; CHANGE THIS if you add/delete options */
  while (checkoptions-- && *nargs >= pos && lua_ispair(L, *nargs)) {
    agn_pairgetiall(L, *nargs);  /* get left, right value, set to stack index -2, -1, 7.7.8 */
    /* check for properly allocated slots will be done by agn_pairgetiall() */
    if (agn_isstring(L, -2)) {
      const char *option = agn_tostring(L, -2);
      if (tools_streq(option, "skiphash")) {
        if (lua_isboolean(L, -1)) {
          *ignorehash = agn_istrue(L, -1);
        } else  {
          agn_poptoptwo(L);
          luaL_error(L, "Error in " LUA_QS ": right-hand side of option must be a boolean.", procname);
        }
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


static int satisfyresult = 0;  /* this is dirty, but it works */

static void agnB_recurse_aux_structseti (lua_State *L, int idx, int type, const char *procname) {
  size_t pos = agn_nops(L, idx) + 1;
  switch (type) {
    case LUA_TTABLE: lua_rawseti(L, idx, pos); break;
    case LUA_TSET:   lua_srawset(L, idx);      break;
    case LUA_TSEQ:   lua_seqrawseti(L, idx, pos); break;
    case LUA_TREG: {  /* 2.14.0 fix, with register, agn_nops returns total size not number if inserted elements */
      int i, j, isnil;
      j = 1;
      for (i=0; i < pos - 1; i++) {  /* get first free `null` slot */
        agn_regrawgeti(L, idx, i + 1);
        isnil = lua_isnil(L, -1);
        agn_poptop(L);
        if (isnil) break;
        j++;
      }
      if (j > pos - 1)
        luaL_error(L, "Error in " LUA_QS ": result register full, gor only %d slots j=%d.", "descend", pos - 1);
      agn_regrawseti(L, idx, j); break;
    }
    default:
      luaL_error(L, "Error in " LUA_QS ": invalid %s object at stack slot %d.", procname, luaL_typename(L, idx), idx);
  }
}

static int agnB_recurse_aux (lua_State *L,
  int collect, size_t offset, int type, const char *procname, size_t first, size_t last, int descendorrecurse, int ignorehash) {  /* Agena 1.6.15, collection option added with 2.4.4 */
  /* descendorrecurse = 1: called by `descend` or `recurse`, 0 = called by `satisfy`,  2 = called by `map`;
     collect = 1 with function `descend` or `map` (check entire structure). */
  int flag, isstruct, err;
  size_t c, slots;
  c = 0;
  isstruct = 0;
  flag = 0;
  slots = 2 + 2 + (first <= last ? last - first + 1 : 0);
  luaL_checkstack(L, slots, "not enough stack space");  /* 3.5.4 fix */
  switch (lua_type(L, -2)) {
    case LUA_TTABLE: {
      int j;
      if (collect) lua_createtable(L, 0, 0);
      lua_pushnil(L);
      while (lua_next(L, -3 - collect)) {
        luaL_checkstack(L, slots, "not enough stack space");  /* 3.5.4 fix */
        if (ignorehash && !lua_isnumber(L, -2)) {  /* key is non-numeric ? -> ignore hash part */
          agn_poptop(L);  /* pop value */
          continue;
        }
        if (!descendorrecurse && !isstruct && lua_type(L, -1) > LUA_TTHREAD) isstruct = 1;
        lua_pushvalue(L, -3 - collect);  /* push function */
        lua_pushvalue(L, -2);  /* duplicate table value */
        for (j=first; j <= last; j++) lua_pushvalue(L, j);  /* push additional arguments */
        err = lua_pcall(L, (first <= last) ? 1 + last - first + 1 : 1, 1, 0);  /* 2.14.0 fix */
        if (err != 0) {  /* error ? */
          if (!isstruct && lua_type(L, -2) > LUA_TTHREAD) isstruct = 1;
          agn_poptoptwo(L);  /* remove message and value */
          continue;
        }
        if (descendorrecurse == 2) {  /* called by `map` ? 3.9.2 */
          flag = 1;
          luaL_checkstack(L, 1, "not enough stack space");
          lua_pushvalue(L, -3);  /* copy key */
          lua_pushvalue(L, -2);  /* copy function return */
          lua_settable(L, -6);
          agn_poptoptwo(L);  /* pop function result and original table value */
          continue;
        }
        if (!lua_isboolean(L, -1))
          luaL_error(L, "Error in " LUA_QS ": procedure must evaluate to a boolean, returned %s.", procname, luaL_typename(L, -1));
        if (agn_istrue(L, -1)) {
          flag = satisfyresult = 1;
          if (collect) {
            agn_poptop(L);  /* pop result true */
            if (offset == 0)
              agnB_recurse_aux_structseti(L, -6, type, procname);
            else
              lua_rawseti(L, -3, ++c);  /* pushes value into table and pops value, leave key on stack for next iteration */
            continue;
          } else if (descendorrecurse) {  /* 2.14.0 */
            lua_pop(L, 3);  /* pop true, and value, and key */
            break;
          } else {
            lua_pop(L, 2);
            continue;
          }
        } else {  /* false ! */
          if (!descendorrecurse) { flag = satisfyresult = 0; }
          agn_poptop(L);  /* remove false */
          if (!descendorrecurse && !(lua_type(L, -1) > LUA_TTHREAD)) {
            lua_pop(L, 2);  /* remove value and key */
            agn_poptoptwo(L);
            lua_pushfalse(L);
            return -1;  /* bail out */
          }
        }
        if (descendorrecurse == 1 && !agn_isnumber(L, -2)) {  /* check key, key :- number and assigned(struct[key]), called by `descend` or `recurse` */
          lua_pushvalue(L, -3 - collect);  /* push function */
          lua_pushvalue(L, -3);  /* push key */
          if (!isstruct && lua_type(L, -1) > LUA_TTHREAD) isstruct = 1;
          for (j=first; j <= last; j++) lua_pushvalue(L, j);  /* push additional arguments */
          err = lua_pcall(L, (first <= last) ? 1 + last - first + 1 : 1, 1, 0);  /* 2.14.0 fix */
          if (err != 0) {  /* error ? */
            if (!isstruct && lua_type(L, -2) > LUA_TTHREAD) isstruct = 1;
            agn_poptoptwo(L);  /* remove message and value */
            continue;
          }
          if (!lua_isboolean(L, -1))
            luaL_error(L, "Error in " LUA_QS ": procedure must evaluate to a boolean, returned %s.", procname, luaL_typename(L, -1));
          if (agn_istrue(L, -1)) {
            flag = 1;
            if (collect) {  /* called by `descend` */
              agn_poptop(L);  /* pop result true */
              if (offset == 0)
                agnB_recurse_aux_structseti(L, -6, type, procname);
              else
                lua_rawseti(L, -3, ++c);  /* pushes value into table and pops value, leave key on stack for next iteration */
              continue;
            } else {  /* called by `recurse`, bail out */
              lua_pop(L, 3);  /* pop true, value and key */
              break;
            }
          } else {
            agn_poptop(L);  /* pop false */
          }
        }
        if (descendorrecurse && !isstruct && lua_type(L, -1) > LUA_TTHREAD) isstruct = 1;  /* 2.14.0, atomic type found ? */
        agn_poptop(L);  /* remove value */
      }  /* end of loop iteration */
      if (collect) {
        if (descendorrecurse == 1 && (!flag || offset == 0))
          agn_poptop(L);  /* remove table since there are no hits (or they have been directly inserted into prime table) */
        else  /* assign table with hits to primary result table */
          agnB_recurse_aux_structseti(L, -4 - offset, type, procname);
      }
      break;
    }
    case LUA_TSET: {
      int j;
      if (descendorrecurse == 2) {  /* called by `map` ? 3.9.2 */
        luaL_error(L, "Error in " LUA_QS ": sets are not supported.", procname);
      }
      if (collect) agn_createset(L, 0);
      lua_pushnil(L);
      while (lua_usnext(L, -3 - collect)) {
        luaL_checkstack(L, slots, "not enough stack space");  /* 3.5.4 fix */
        if (!descendorrecurse && !isstruct && lua_type(L, -1) > LUA_TTHREAD) isstruct = 1;
        lua_pushvalue(L, -3 - collect);  /* push function */
        lua_pushvalue(L, -2);  /* push value */
        for (j=first; j <= last; j++) lua_pushvalue(L, j);  /* push additional arguments */
        err = lua_pcall(L, (first <= last) ? 1 + last - first + 1 : 1, 1, 0);  /* 2.14.0 fix */
        if (err != 0) {  /* error ? */
          agn_poptoptwo(L);  /* remove message and value */
          continue;
        }
        if (!lua_isboolean(L, -1))
          luaL_error(L, "Error in " LUA_QS ": procedure must evaluate to a boolean, returned %s.", procname, luaL_typename(L, -1));
        if (agn_istrue(L, -1)) {
          flag = satisfyresult = 1;
          if (collect) {
            agn_poptop(L);  /* pop result true */
            if (offset == 0)
              agnB_recurse_aux_structseti(L, -6, type, procname);
            else
              lua_srawset(L, -3);  /* pushes value into set and pops value, leave key on stack for next iteration */
            continue;
          } else if (descendorrecurse) {
            lua_pop(L, 3);  /* pop true, and value, and key */
            break;
          } else {
            lua_pop(L, 2);
            continue;
          }
        } else {  /* false */
          if (!descendorrecurse) flag = satisfyresult = 0;
          agn_poptop(L);  /* remove false */
          if (!descendorrecurse && !(lua_type(L, -1) > LUA_TTHREAD)) {  /* 2.14.0, atomic type found ? */
            lua_pop(L, 2);  /* remove value and key */
            agn_poptoptwo(L);
            lua_pushfalse(L);
            return -1;  /* bail out */
          }
        }
        if (descendorrecurse && !isstruct && lua_type(L, -1) > LUA_TTHREAD) isstruct = 1;
        agn_poptop(L);  /* remove value */
      }
      if (collect) {
        if (!flag || offset == 0)
          agn_poptop(L);  /* remove set since there are no hits (or they have been directly inserted into prime set) */
        else  /* insert set with hits to primary result table */
          agnB_recurse_aux_structseti(L, -4 - offset, type, procname);
      }
      break;
    }
    case LUA_TSEQ: {
      int i, j;
      if (descendorrecurse == 2) {  /* called by `map` ? 3.9.2 */
        luaL_error(L, "Error in " LUA_QS ": sequences are not supported.", procname);
      }
      if (collect) agn_createseq(L, 0);
      for (i=0; i < agn_seqsize(L, -2 - collect); i++) {
        luaL_checkstack(L, slots, "not enough stack space");  /* 3.5.4 fix */
        lua_pushvalue(L, -1 - collect);  /* push function */
        lua_seqrawgeti(L, -3 - collect, i + 1);  /* push value */
        if (!isstruct && lua_type(L, -1) > LUA_TTHREAD) isstruct = 1;
        for (j=first; j <= last; j++) lua_pushvalue(L, j);  /* push additional arguments */
        err = lua_pcall(L, (first <= last) ? 1 + last - first + 1 : 1, 1, 0);  /* 2.14.0 fix */
        if (err != 0) {  /* error ? */
          if (!isstruct && lua_type(L, -2) > LUA_TTHREAD) isstruct = 1;
          agn_poptop(L);  /* remove message */
          continue;
        }
        if (!lua_isboolean(L, -1))
          luaL_error(L, "Error in " LUA_QS ": procedure must evaluate to a boolean, returned %s.", procname, luaL_typename(L, -1));
        if (agn_istrue(L, -1)) {
          flag = satisfyresult = 1;
          agn_poptop(L);  /* pop result true */
          if (collect) {
            lua_seqrawgeti(L, -3, i + 1);  /* push value */
            if (offset != 0)
              lua_seqrawseti(L, -2, ++c);
            else
              agnB_recurse_aux_structseti(L, -5, type, procname);  /* set value directly into primary structure */
            continue;
          }
          if (descendorrecurse)  /* 2.14.0 */
            break;
          else
            continue;
        } else {  /* false */
          if (!descendorrecurse) flag = satisfyresult = 0;
          agn_poptop(L);  /* pop false */
          if (!descendorrecurse) {
            lua_seqrawgeti(L, -2, i + 1);  /* push value again */
            if (!(lua_type(L, -1) > LUA_TTHREAD)) {  /* 2.14.0, atomic type found ? */
              agn_poptop(L);
              lua_pushfalse(L);
              return -1;
            }
            agn_poptop(L);
          }
        }
      }
      if (collect) {
        if (!flag || offset == 0)
          agn_poptop(L);  /* remove sequence since there are no hits */
        else  /* assign sequence with hits to primary result sequence */
          agnB_recurse_aux_structseti(L, -4 - offset, type, procname);
      }
      break;
    }
    case LUA_TREG: {  /* 2.3.0 RC 3 */
      int i, j;
      if (descendorrecurse == 2) {  /* called by `map` ? 3.9.2 */
        luaL_error(L, "Error in " LUA_QS ": sets are not supported.", procname);
      }
      if (collect) agn_createreg(L, agn_getregsize(L));
      for (i=0; i < agn_reggettop(L, -2 - collect); i++) {
        luaL_checkstack(L, slots, "not enough stack space");  /* 3.5.4 fix */
        lua_pushvalue(L, -1 - collect);  /* push function */
        agn_regrawgeti(L, -3 - collect, i + 1);  /* push value */
        if (!isstruct && lua_type(L, -1) > LUA_TTHREAD) isstruct = 1;
        for (j=first; j <= last; j++) lua_pushvalue(L, j);  /* push additional arguments */
        err = lua_pcall(L, (first <= last) ? 1 + last - first + 1 : 1, 1, 0);  /* 2.14.0 fix */
        if (err != 0) {  /* error ? */
          if (!isstruct && lua_type(L, -2) > LUA_TTHREAD) isstruct = 1;
          agn_poptop(L);  /* remove message */
          continue;
        }
        if (!lua_isboolean(L, -1))
          luaL_error(L, "Error in " LUA_QS ": procedure must evaluate to a boolean, returned %s.", procname, luaL_typename(L, -1));
        if (agn_istrue(L, -1)) {
          flag = satisfyresult = 1;
          agn_poptop(L);  /* pop result true */
          if (collect) {
            agn_regrawgeti(L, -3, i + 1);  /* push value */
            if (offset != 0)
              agn_regrawseti(L, -2, ++c);
            else {
              agnB_recurse_aux_structseti(L, -5, type, procname);  /* set value directly into primary structure */
            }
            continue;
          }
          if (descendorrecurse)  /* 2.14.0 */
            break;
          else
            continue;
        } else {  /* false */
          if (!descendorrecurse) flag = satisfyresult = 0;
          agn_poptop(L);  /* pop false */
          if (!descendorrecurse) {
            agn_regrawgeti(L, -2, i + 1);  /* push value again */
            if (!(lua_type(L, -1) > LUA_TTHREAD)) {  /* 2.14.0, atomic type found ? */
              agn_poptop(L);
              lua_pushfalse(L);
              return -1;
            }
            agn_poptop(L);
          }
        }
      }
      if (collect) {
        if (!flag || offset == 0)
          agn_poptop(L);  /* remove register since there are no hits */
        else  /* assign sequence with hits to primary result sequence */
          agnB_recurse_aux_structseti(L, -4 - offset, type, procname);
      }
      break;
    }
    case LUA_TPAIR: {  /* Agena 1.8.4 */
      int i, j;
      if (descendorrecurse == 2) {  /* called by `map` ? 3.9.2 */
        luaL_error(L, "Error in " LUA_QS ": pairs are not supported.", procname);
      }
      for (i=0; i < 2; i++) {
        luaL_checkstack(L, slots, "not enough stack space");  /* 3.5.4 fix */
        if (!descendorrecurse) flag = 0;
        lua_pushvalue(L, -1);  /* push function */
        agn_pairgeti(L, -3, i + 1);  /* push value */
        if (!isstruct && lua_type(L, -1) > LUA_TTHREAD) isstruct = 1;
        for (j=first; j <= last; j++) lua_pushvalue(L, j);  /* push additional arguments */
        err = lua_pcall(L, (first <= last) ? 1 + last - first + 1 : 1, 1, 0);  /* 2.14.0 fix */
        if (err != 0) {  /* error ? */
          if (!isstruct && lua_type(L, -2) > LUA_TTHREAD) isstruct = 1;
          agn_poptop(L);  /* remove message and value */
          continue;
        }
        if (!lua_isboolean(L, -1))
          luaL_error(L, "Error in " LUA_QS ": procedure must evaluate to a boolean, returned %s.", procname, luaL_typename(L, -1));
        if (agn_istrue(L, -1)) {
          agn_poptop(L);  /* pop true */
          flag = satisfyresult = 1;
          if (descendorrecurse)  /* 2.14.0 */
            break;
          else
            continue;
        } else {
          if (!descendorrecurse) flag = satisfyresult = 0;
          agn_poptop(L);  /* pop false */
          if (!descendorrecurse) {
            agn_pairgeti(L, -2, i + 1);  /* push value again */
            if (!(lua_type(L, -1) > LUA_TTHREAD)) {  /* 2.14.0, atomic type found ? */
              agn_poptop(L);
              lua_pushfalse(L);
              return -1;
            }
            agn_poptop(L);
          }
        }
      }
      break;
    }
    case LUA_TNONE: {  /* Agena 1.8.4 */
      luaL_error(L, "Error in " LUA_QS ": type %s not accepted.", procname, lua_typename(L, LUA_TNONE));
      break;
    }
    default: {  /* = case LUA_TNUMBER: case LUA_TNIL case LUA_TBOOLEAN: case LUA_TCOMPLEX: case LUA_TSTRING: case LUA_TFUNCTION: case LUA_TTHREAD:
                     case LUA_TUSERDATA: case LUA_TLIGHTUSERDATA */
      agn_poptoptwo(L);
      lua_pushfail(L);
      return 1;
    }
  }
  if (( (descendorrecurse && !flag) || (!descendorrecurse) || collect) && isstruct) {
    /* During the last traversal, a structure has been encountered ?  Iterate (sub)table once again and only process substructures in it. */
    switch (lua_type(L, -2)) {
      case LUA_TTABLE: {
        lua_pushnil(L);
        while (lua_next(L, -3)) {
          if (ignorehash && !lua_isnumber(L, -2)) {  /* 2.14.0 fix, key is non-numeric ? -> ignore hash part */
            agn_poptop(L);  /* pop value */
            continue;
          }
          recurse_query(L, -3, 2, collect, offset + 3, type, procname, first, last, descendorrecurse, ignorehash);
        }
        break;
      }
      case LUA_TSET: {
        lua_pushnil(L);
        while (lua_usnext(L, -3)) {
          recurse_query(L, -3, 2, collect, offset + 3, type, procname, first, last, descendorrecurse, ignorehash);
        }
        break;
      }
      case LUA_TSEQ: {
        int i;
        for (i=0; i < agn_seqsize(L, -2); i++) {
          lua_seqrawgeti(L, -2, i + 1);
          recurse_query(L, -2, 1, collect, offset + 2, type, procname, first, last, descendorrecurse, ignorehash);
        }
        break;
      }
      case LUA_TREG: {  /* 2.3.0 RC 3 */
        int i;
        for (i=0; i < agn_reggettop(L, -2); i++) {
          agn_regrawgeti(L, -2, i + 1);
          recurse_query(L, -2, 1, collect, offset + 2, type, procname, first, last, descendorrecurse, ignorehash);
        }
        break;
      }
      case LUA_TPAIR: {  /* Agena 1.8.4 */
        int i;
        for (i=0; i < 2; i++) {  /* Agena 1.8.5 fix */
          agn_pairgeti(L, -2, i + 1);
          recurse_query(L, -2, 1, collect, offset + 3, type, procname, first, last, descendorrecurse, ignorehash);
        }
        break;
      }
      default: {
        luaL_error(L, "Error in " LUA_QS ": type %s not accepted.", procname, luaL_typename(L, -2));
      }
    }
  }
  agn_poptoptwo(L);  /* remove first and second argument */
  lua_pushboolean(L, (descendorrecurse) ? flag: satisfyresult);
  return 1;
}

static int agnB_recurse (lua_State *L) {
  int type, ignorehash, idxfn, idxobj, nargs = lua_gettop(L);
  type = LUA_TNIL;
  ignorehash = 0;
  if (nargs < 2)  /* 2.4.4 */
    luaL_error(L, "Error in " LUA_QS ": requires at least two arguments.", "recurse");
  recurse_checkoption(L, 3, &nargs, &ignorehash, "recurse");  /* 5.3.3 */
  if (lua_isfunction(L, 1)) {  /* 2.17.9 */
    idxfn = 1; idxobj = 2;
  } else if (lua_isfunction(L, 2)) {  /* assure backward-compatibility */
    idxfn = 2; idxobj = 1;
  } else {
    idxfn = 0; idxobj = 0;
    luaL_error(L, "Error in " LUA_QS ": wrong type of arguments.", "recurse");
  }
  type = lua_type(L, idxobj);
  lua_pushvalue(L, idxobj);  /* push object */
  lua_pushvalue(L, idxfn);   /* push function */
  agnB_recurse_aux(L, 0, 0, type, "recurse", 3, nargs, 1, ignorehash);  /* 2.4.4 extension */
  return 1;
}


/* With x a number, complex number, string, boolean, null or userdata, calls the function f which should return
  `true` or `false`. The result is the return of this call.

   With x a structure, checks each element in x by calling function f which also should return `true` or `false`. If
   at least one element in x does not satisfy the condition checked by f, the result is `false`, and otherwise `true`.

  The function performs a recursive descent if it detects tables, sets, pairs, registers, or sequences in x so that it
  can find elements in deeply nested structures. With tables, by default, also non-integer keys and their associated
  values are checked. If you pass the option ignorehash=true, however, the function ignores all non-numeric keys and
  their corresponding values.

  See also: recurse, descend. */
static int agnB_satisfy (lua_State *L) {  /* 2.14.0, based on recurse */
  int ttype, i, ignorehash, idxfn, idxobj, nargs = lua_gettop(L);
  ttype = LUA_TNIL;
  ignorehash = 0;
  if (nargs < 2)  /* 2.4.4 */
    luaL_error(L, "Error in " LUA_QS ": requires at least two arguments.", "satisfy");
  recurse_checkoption(L, 3, &nargs, &ignorehash, "satisfy");  /* 5.3.3, 6.7.8 fix */
  luaL_checkstack(L, nargs, "too many arguments");
  if (lua_isfunction(L, 1)) {
    idxfn = 1; idxobj = 2;
  } else if (lua_isfunction(L, 2)) {  /* assure backward-compatibility */
    idxfn = 2; idxobj = 1;
  } else {
    idxfn = 0; idxobj = 0;
    luaL_error(L, "Error in " LUA_QS ": wrong type of arguments.", "satisfy");
  }
  switch (lua_type(L, idxobj)) {
    case LUA_TNUMBER:
    case LUA_TCOMPLEX:
    case LUA_TBOOLEAN:
    case LUA_TNIL:
    case LUA_TSTRING:
    case LUA_TUSERDATA: {
      lua_pushvalue(L, idxfn);   /* push function, swapped 2.17.9 */
      lua_pushvalue(L, idxobj);  /* push object, swapped 2.17.9 */
      for (i=3; i <= nargs; i++) lua_pushvalue(L, i);
      lua_call(L, nargs - 1, 1);
      if (!lua_isboolean(L, -1)) {
        agn_poptop(L);
        luaL_error(L, "Error in " LUA_QS ": procedure must evaluate to a boolean, returned %s.",
          "satisfy", luaL_typename(L, -1));
      }
      break;   /* return boolean */
    }
    default: {
      lua_pushvalue(L, idxobj);  /* push object, swapped 2.17.9 */
      lua_pushvalue(L, idxfn);   /* push function, swapped 2.17.9 */
      agnB_recurse_aux(L, 0, 0, ttype, "satisfy", 3, nargs, 0, ignorehash);
    }
  }
  satisfyresult = 0;
  return 1;
}


static int agnB_descend (lua_State *L) {
  int type, ignorehash, idxfn, idxobj, nargs = lua_gettop(L);
  type = LUA_TNIL;
  ignorehash = 0;
  if (nargs < 2)  /* 2.4.4 */
    luaL_error(L, "Error in " LUA_QS ": requires at least two arguments.", "descend");
  recurse_checkoption(L, 3, &nargs, &ignorehash, "descend");  /* 5.3.3/6.7.8 */
  if (lua_isfunction(L, 1)) {  /* 2.17.9 */
    idxfn = 1; idxobj = 2;
  } else if (lua_isfunction(L, 2)) {  /* assure backward-compatibility */
    idxfn = 2; idxobj = 1;
  } else {
    idxfn = 0; idxobj = 0;
    luaL_error(L, "Error in " LUA_QS ": wrong type of arguments.", "descend");
  }
  type = lua_type(L, idxobj);
  switch (type) {
    case LUA_TTABLE: lua_newtable(L);     break;
    case LUA_TSET:   agn_createset(L, 0); break;
    case LUA_TSEQ:   agn_createseq(L, 0); break;
    case LUA_TREG:   agn_createreg(L, agn_nops(L, idxobj)); break;  /* 2.14.0 fix, ensure enough space in result register */
    default:
      luaL_error(L, "Error in " LUA_QS ": type %s not accepted with collection option.", "descend", luaL_typename(L, 3));
  }
  lua_pushvalue(L, idxobj);  /* push object */
  lua_pushvalue(L, idxfn);   /* push function */
  agnB_recurse_aux(L, 1, 0, type, "descend", 3, nargs, 1, ignorehash);  /* 2.4.4 extension */
  agn_poptop(L);  /* pop Boolean, return structure with results */
  return 1;
}


/* December 10, 2006; revised December 24, 2007; extended June 21, 2008, patched December 23, 2008;
   changed June 21, 2009; fixed 0.25.4, August 01, 2009; extended November 22, 2009; extended December 24, 2009
   Since values popped may be gc'ed, the strings put on the stack are only popped when not needed
   any longer. Do not put an agn_poptop() statement right after the str = agn_tostring(L, n)
   assignment !!! */
void aux_getpackagetable (lua_State *L, const char *libname) {
  lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");  /* table is at stack index -1 */
  if (!lua_istable(L, -1)) {
    agn_poptop(L);
    luaL_error(L, "Warning: internal registry _LOADED table not found.");
  }
  lua_getfield(L, -1, libname);  /* try to fetch package from _LOADED registry table */
  lua_remove(L, -2);  /* drop _LOADED registry table */
}

static int agnB_readlib (lua_State *L) {
  char *fn;
  const char *procname;
#if !(defined(__OS2__) || defined(LUA_ANSI) || defined(__DJGPP__))
  const char *libopener;
#endif
  int nargs, i, clibsuccess, success, globalsuccess, printsearchpaths, debug, printdllerror;
  printsearchpaths = 0;
  nargs = lua_gettop(L);
  procname = "readlib";
  debug = agn_getdebug(L);  /* 3.4.3 */
  luaL_checkstack(L, nargs, "too many arguments");
  if (lua_isboolean(L, nargs)) {
    if (agn_istrue(L, nargs))  /* 0.29.0 extension, tuned 1.6.0 */
      printsearchpaths = 1;
    else
      procname = "import";  /* 2.0.0 */
    nargs--;  /* 7.3.5 fix */
  }
  if (nargs == 0)
    luaL_error(L, "Error in " LUA_QS ": requires at least one argument, a string.", procname);
  globalsuccess = 1;
  /* try library names */
  for (i=1; i <= nargs; i++) {
    const char *apath, *path, *libname;
    char *buffer;
    int result, rc, havereinitpkgtable;
    printdllerror = 1;
    libname = agn_checkstring(L, i);  /* 2.0.0 change */
    agnL_debuginfo3(debug, "Processing library: ", libname, ".\n");
    success = 0; havereinitpkgtable = 0;
    /* if package does not belong to those loaded at startup, delete entry from _LOADED registry table, and thus also from
       package.loaded - otherwise re-insert package table from _LOADED registry entry into global environment; 2.29.7 fix  */
    if (luaL_isstandardlib(L, libname)) {
      agnL_debuginfo3(debug, "   ", libname, " is a standard library.\n");
      lua_getglobal(L, libname);
      if (lua_isnil(L, -1)) {  /* package no longer in global environment ? */
        agn_poptop(L);  /* drop nil */
        agnL_debuginfo(debug, "  Trying to re-establish package table ... ");
        aux_getpackagetable(L, libname);
        if (lua_istable(L, -1)) {  /* we have an entry ? */
          lua_setglobal(L, libname);  /* re-assign to global environment and drop package table */
          havereinitpkgtable = 1;
          agnL_debuginfo(debug, "  done.");
        } else {
          agn_poptop(L);  /* drop whatever and _LOADED registry table */
          agnL_debuginfo(debug, "\n");
          luaL_error(L, "Error in " LUA_QS ": failed to re-establish package table " LUA_QS "s.", "readlib", libname);  /* 4.6.5 fix */
        }
      } else {
        agnL_debuginfo(debug, "   Nothing to be done.");
        agn_poptop(L);  /* drop table from lua_getglobal(libname) call, do nothing */
      }
#if !(defined(__OS2__) || defined(LUA_ANSI) || defined(__DJGPP__))
      continue;
#endif
      /* in DOS and OS/2 we have to try to read an existing agn file for the library later on, for the _LOADED reg entry
         only includes the C functions initialised before */
    } else {  /* non-standard library ? -> delete _LOADED registry entry and completely re-load package later on */
      agnL_debuginfo3(debug, "   ", libname, " may be an external (plus) package.\n\n");
      aux_getpackagetable(L, libname);
      if (lua_istable(L, -1)) {  /* we have an entry ? */
        agn_poptop(L);  /* pop package table */
        lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");
        lua_pushnil(L);
        lua_setfield(L, -2, libname);  /* delete package from _LOADED registry table */
        agn_poptop(L);  /* drop _LOADED registry table */
      } else
        agn_poptop(L);  /* delete whatever value we retrieved from lua_getglobal call */
    }
    /* prepend current working directory to library name */
    luaL_checkstack(L, 5, "not enough stack space");  /* 2.31.7 */
    buffer = agn_stralloc(L, PATH_MAX, procname, NULL);  /* 2.16.5, (char *)malloc(sizeof(char)*(PATH_MAX + 1)); */ /* Agena 1.5.0 */
    if (getcwd(buffer, PATH_MAX) == buffer) {  /* 1.6.11, to avoid compiler warnings */
      lua_pushstring(L, buffer);
      lua_pushstring(L, LUA_PATHSEP);
    } else {
      xfree(buffer);  /* 2.12.0 RC 4 fix to prevent memory leaks */
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", procname);
    }
    xfree(buffer);  /* 7.3.5 fix */
    /* add mainlibname, 0.29.3 */
    lua_getglobal(L, "mainlibname");
    if (!agn_isstring(L, -1)) {
      luaL_error(L, "Error in " LUA_QS ": " LUA_QS
        " is not a string, must be a path to the main\nAgena folder.", procname, "mainlibname");
    }
    lua_pushstring(L, LUA_PATHSEP);
    /* add libname since path gets modified by pushnexttemplate, reload libname */
    lua_getglobal(L, "libname");
    if (!agn_isstring(L, -1)) {  /* 0.20.2 */
      luaL_error(L, "Error in " LUA_QS ": " LUA_QS
        " is not a string, must be a path to the main\nAgena folder.", procname, "libname");
    }
    rc = 0;
    /* is mainlibname included in libname ? -> no need to check twice, 5.0.1 */
    if (strstr(agn_tostring(L, -1), agn_tostring(L, -3)) != NULL) {
      lua_pop(L, 2); rc = 1;
    }
    lua_concat(L, 5 - 2*rc);
    path = agn_tostring(L, -1);
    /* iterate libname, the *.agn file must be in the same directory as the corresponding *.dll file */
    while ((path = luaL_pushnexttemplate(L, path)) != NULL) {  /* pushes a path onto the stack */
      clibsuccess = 0;
      apath = agn_tostring(L, -1);  /* assign string pushed by pushnexttemplate */
      if (printsearchpaths)
        fprintf(stderr, "Searching " LUA_QS ".\n", apath);
#if !(defined(__OS2__) || defined(LUA_ANSI) || defined(__DJGPP__)) /* 0.27.1 patch: prevent that the DOS, OS/2 and strict-ANSI
      versions of Agena try to initialise external C libraries */
      /* find and read CLIB (DLLs, SOs) if present */
      agnL_debuginfo3(debug, "   Checking path ", apath, ".\n");
      fn = str_concat(apath, "/", libname, AGN_CLIB, NULL);  /* Agena 1.5.0, 1.6.0 */
      if (fn == NULL) {
        luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", procname);
      }
      agnL_debuginfo3(debug, "   Checking C library file ", fn, ".\n");
      libopener = fastloader_C(L, (const char*)fn, libname, &clibsuccess, 0);  /* pushes nothing */
      if (printdllerror && clibsuccess < 0) {  /* 0.29.0, 7.3.5 change */
        if (clibsuccess == -1)
          fprintf(stderr, "Warning in " LUA_QS ", package file " LUA_QS " corrupt.\n", procname, fn);
        else
          fprintf(stderr, "Warning in " LUA_QS ", package file " LUA_QS " found, " LUA_QS " failed.\n", procname, fn, libopener);
        if (!debug) {  /* 1.12.9 */
          fprintf(stderr, "Try `environ.kernel(debug=true)` and retry loading the package to get further hints.\n");
        }
        fflush(stderr);
        /* 3.3.2: avoid multiple error messages for the same library: we won't break but just suppress duplicate messages. */
        printdllerror = 0;
      }
      if (clibsuccess >= 0) {
        if (clibsuccess == 0) {  /* 7.3.5 */
          agnL_debuginfo3(debug, "   ", fn, " not present.\n");
        } else {
          agnL_debuginfo3(debug, "   Library opener ", libopener, " successfully run.\n");
          agnL_debuginfo3(debug, "   Package ", fn, " has been initialised.\n");
        }
      }
      xfree(fn);  /* Agena 1.6.0 Valgrind, 1.7.5 */
#endif
      /* find and read agn text file library if present */
      fn = str_concat(apath, "/", libname, ".agn", NULL);  /* Agena 1.5.0 */
      if (fn == NULL) {
        luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", procname);
      }
      result = luaL_loadfile(L, (const char *)fn);  /* pushes a chunk or an error string */
      agnL_debuginfo3(debug, "   Checking agn library file ", fn,
        result != LUA_ERRFILE ? ": found.\n" : ": not present.\n");
      xfree(fn);  /* Agena 1.6.0 Valgrind */
      if (result != LUA_ERRFILE) {  /* file found, run it */
        if (result != 0) lua_error(L);  /* but there is a syntax error in it */
        if (lua_pcall(L, 0, 0, 0) != 0) {  /* the loaded file must be run to get procs assigned; the chunk is
                                              popped, and no results are pushed on the stack */
          luaL_error(L, "Error in " LUA_QS ": %s", procname, lua_tostring(L, -1));  /* 4.9.1 */
        } else {  /* now clean up */
          success = 1;
        }
      }
      else
        agn_poptop(L);  /* drop error code */
      agn_poptop(L);   /* drop string pushed by pushnexttemplate */
      if (clibsuccess == 1) {
        success = 1;
        /* if a package consists of both a C DLL and an Agena text file, then they
         must reside in the very same folder. */
      }
      if (success)
        break;  /* quit while loop */
      else if (debug) {
        agnL_debuginfo(debug, "\n");
      }
    }  /* end of libname while iteration loop */
#if defined(__OS2__) || defined(LUA_ANSI) || defined(__DJGPP__)  /* 2.3.2: in eCS - OS/2 and DOS: try to find a loaded library */
    lua_getglobal(L, libname);
    if (lua_istable(L, -1)) {  /* try to find at least one procedure in it */
      lua_pushnil(L);
      while (lua_next(L, -2)) {  /* pushes key ~ value pair onto the stack */
        if (lua_isfunction(L, -1)) {
          success = 1;
          agn_poptoptwo(L);  /* drop key ~ value pair */
          break;
        }
        agn_poptop(L);  /* else pop the value */
      }
    }
    agn_poptop(L);  /* pop `getfield' */
#endif
    agn_poptop(L);  /* drop libname */
    if (!havereinitpkgtable && !success) {
      fprintf(stderr, "Warning in " LUA_QS ": package " LUA_QS " not found.\n", procname, libname);
      fflush(stderr);
      globalsuccess = 0;
    } else {
      if (printdllerror == 0) {  /* 6.5.9 fix */
        agnL_debuginfo3(debug, "\n   We got at least one problem, registering ", libname, " anyway.\n");
      } else {
        agnL_debuginfo3(debug, "\n   Everything is fine, now registering ", libname, ".\n");
      }
      /* register package in package.loaded [and package.readlibbed], 0.26.0 */
      lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");  /* table is at stack index -1 */
      lua_getglobal(L, libname);  /* try to get value of `libname' from global environment */
      if (lua_isnil(L, -1)) {
        agn_poptop(L);  /* pop null */
        lua_pushtrue(L);  /* and push true instead */
      }
      lua_setfield(L, -2, libname);  /* set value of `libname' to package.loaded.libname and pop this value */
      agn_poptop(L);  /* pop package.loaded.libname */
      /* add entry to _READLIBBED index in the registry, 2.9.1, 2.31.9 fix for OS/2, DOS and ANSI versions in agn_setreadlibbed to not
         accidentally register built-in libraries as readlib'ed */
      agn_setreadlibbed(L, libname);
      if (i < nargs) agnL_debuginfo(debug, "\n");
    }
  }  /* of for in library names */
  if (globalsuccess)
    lua_pushtrue(L);
  else
    lua_pushfail(L);
  return 1;
}


/* Create composite function */
#define agnX_checkstack(L,n,str) \
  if (lua_checkstack(L, 1 + n) == 0) \
    luaL_error(L, "Error in " LUA_QS ": stack overflow.", str);

/* Pushes a new table with objlen/factor pre-allocated slots to the stack top. */
static FORCE_INLINE void aux_createtable (lua_State *L, int idx, size_t objlen, size_t factor) {
  if (objlen == 0)  /* assume table is a dictionary */
    lua_createtable(L, 0, agn_size(L, idx)/factor);
  else
    lua_createtable(L, objlen/factor, 0);  /* lua_objlen not always returns correct results ! */
  luaL_setmetatype(L, idx);  /* sets the metatable (if present) and the user-defined type (if existing)
    of the object at idx to the object at the top of the stack. */
}

static int aux_compose (lua_State *L) {  /* 2.12.0 RC 1 */
  int f, g, i, nrets, nargs, newtop;
  nargs = lua_gettop(L);
  /* call g */
  f = lua_upvalueindex(1);
  g = lua_upvalueindex(2);
  agnX_checkstack(L, nargs, "map");
  lua_pushvalue(L, g);
  for (i=0; i < nargs; i++) {
    lua_pushvalue(L, i + 1);
  }
  lua_call(L, nargs, LUA_MULTRET);  /* run g on arguments */
  nrets = lua_gettop(L) - nargs;    /* number of results */
  /* call f */
  newtop = lua_gettop(L);
  agnX_checkstack(L, nargs, "map");
  lua_pushvalue(L, f);
  for (i=0; i < nrets; i++) {
    lua_pushvalue(L, -(nrets + 1));
  }
  lua_call(L, nrets, LUA_MULTRET);  /* run f on arguments */
  return lua_gettop(L) - newtop;
}

/* Check options for map, select, remove, subs, subsop; externalised 2.31.5 */
void agnB_aux_fcheckoptions (lua_State *L, int *nargs, int *newstruct, int *multipass, int *strict, int *reshuffle, int *multret, int *descend, int *plain, const char *procname) {
  int checkoptions;
  *newstruct = 1;  /* returns a new structure, do not work in-place */
  *multipass = 1;  /* 1 = always substitute at each pass, 0 = substitute a position only once */
  *strict = 1;     /* 1 = strict equality, 0 = approximate equality */
  *reshuffle = 0;  /* 0 = leave holes in return as they are, 1 = remove holes and return an array */
  *multret = 0;    /* 0 = only return first result of function call with each element, 1 = return all results of function call with each element */
  *descend = 0;    /* 0 = `map` only: do not descend into nested structure, 1 = do so */
  *plain = 0;      /* use simpler, faster substitution method */
  /* check for options, here `map in-place` */
  checkoptions = 7;  /* check n options; CHANGE THIS if you add/delete options, 2.29.0 */
  while (checkoptions-- && *nargs > 2 && lua_ispair(L, *nargs)) {
    /* check for properly allocated slots will be done by agn_pairgetiall() */
    agn_pairgetiall(L, *nargs);  /* get left, right value, set to stack index -2, -1, 7.7.8 */
    if (agn_isstring(L, -2)) {
      const char *option = agn_tostring(L, -2);
      if (tools_streq("inplace", option)) {
        *newstruct = !agn_checkboolean(L, -1);
      } else if (tools_streq("multipass", option)) {
        *multipass = agn_checkboolean(L, -1);
      } else if (tools_streq("strict", option)) {
        *strict = agn_checkboolean(L, -1);
      } else if (tools_streq("multret", option)) {
        *multret = agn_checkboolean(L, -1);
      } else if (tools_streqx(option, "newarray", "reshuffle", NULL)) {  /* extended 3.9.0 */
        *reshuffle = agn_checkboolean(L, -1);
      } else if (tools_streq("descend", option)) {  /* added 3.9.2 */
        *descend = agn_checkboolean(L, -1);
      } else if (tools_streq("plain", option)) {  /* added 5.1.3 */
        *plain = agn_checkboolean(L, -1); *newstruct = 0;
      } else {
        agn_poptoptwo(L);
        luaL_error(L, "Error in " LUA_QS ": unknown option " LUA_QS ".", procname, option);
      }
    }
    /* do not call lua_settop as it would corrupt the argument stack since we have already pushed values */
    (*nargs)--;
    agn_poptoptwo(L);
  }
  /* lhf, May 29, 2011 at 12:20 at
     https://stackoverflow.com/questions/6167555/how-can-i-safely-iterate-a-lua-table-while-keys-are-being-removed
     "You can safely remove entries while traversing a table but you cannot create new entries, that is, new keys.
      You can modify the values of existing entries, though. (Removing an entry being a special case of that rule.)" */
  if (*multret && !(*newstruct))
    luaL_error(L, "Error in " LUA_QS ": illegal combination of " LUA_QS " and " LUA_QS " option.", procname, "multret", "inplace");
  if (*descend) {
    if (!(*newstruct))
      luaL_error(L, "Error in " LUA_QS ": illegal combination of " LUA_QS " and " LUA_QS " option.", procname, "descend", "inplace");
    if (*multret)
      luaL_error(L, "Error in " LUA_QS ": illegal combination of " LUA_QS " and " LUA_QS " option.", procname, "descend", "multret");
    if (!(*newstruct))
      luaL_error(L, "Error in " LUA_QS ": illegal combination of " LUA_QS " and " LUA_QS " option.", procname, "descend", "inplace");
  }
  if (*plain) {  /* deprecated 5.1.4, just pass `inplace=true` with or without the reshuffle option */
    if (!(*strict))
      luaL_error(L, "Error in " LUA_QS ": illegal combination of " LUA_QS " and " LUA_QS " option.", procname, "plain", "strict");
    if (!(*multipass))
      luaL_error(L, "Error in " LUA_QS ": illegal combination of " LUA_QS " and " LUA_QS " option.", procname, "plain", "multipass");
    if (*descend)
      luaL_error(L, "Error in " LUA_QS ": illegal combination of " LUA_QS " and " LUA_QS " option.", procname, "plain", "descend");
    if (*newstruct)
      luaL_error(L, "Error in " LUA_QS ": illegal combination of " LUA_QS " and " LUA_QS " option.", procname, "plain", "inplace");
  }
}


/* Written by Chaos, Shanghai, People's Republic of China, as of August 14, 2020,
   taken from: https://stackoverflow.com/questions/4535152/cloning-a-lua-table-in-lua-c-api
   3.9.2/3 */
static FORCE_INLINE void aux_deepdive_doop (lua_State* L, int copyedValue, int nargs, int caller, int strict, int type) {
  /* The following is the actual mapping segment. We will NOT look up the substituted values as due to all
     the intermediate pushing, they migrate in the stack ...
     The function pushes the modified or original value onto the stack. */
  int err, eq, i;
  switch (caller) {
    case 0: {  /* called by `map`*/
      luaL_checkstack(L, nargs, "not enough stack space");  /* 3.9.5 fix */
      lua_pushvalue(L, 1);  /* push function */
      lua_pushvalue(L, copyedValue);  /* push value */
      for (i=3; i <= nargs; i++) lua_pushvalue(L, i);
      err = lua_pcall(L, nargs - 1, 1, 0);
      if (err) {  /* ignore error, push original value instead */
        agn_poptop(L);
        lua_pushvalue(L, copyedValue);
      }
      break;
    }
    case 1: {  /* called by `subs` */
      luaL_checkstack(L, 2, "not enough stack space");  /* 3.9.5 fix */
      lua_pushvalue(L, copyedValue);
      for (i=1; i < nargs; i++) {
        agn_pairgeti(L, i, 1);
        eq = strict ? lua_equal(L, -2, -1) : lua_rawaequal(L, -2, -1);
        agn_poptop(L);
        if (eq) {
          agn_poptop(L);
          agn_pairgeti(L, i, 2);
          break;
        }
      }
      break;
    }
    default:
      luaL_error(L, "Error in " LUA_QS ": invalid caller.", "(?)", luaL_typename(L, type));
  }
}

LUAI_FUNC int agnB_aux_deepdive (lua_State* L, int n, int CacheT, int nargs, int caller, int strict, const char *procname);

static FORCE_INLINE void aux_deepdive_setmetatype (lua_State* L, int n, int CacheT, int copyIndex, int nargs,
  int caller, int strict, const char *procname) {  /* 3.9.2/3 */
  if (lua_getmetatable(L, n) == 1) {  /* try to get metatable of n (push onto stack if return 1) */
    int metaIndex = lua_absindex(L, -1);  /* push 1 */
    /* int copyedMeta = aux_deepdive(L, metaIndex, CacheT); */ /* try to copy metatable push onto stack */
    agnB_aux_deepdive(L, metaIndex, CacheT, nargs, caller, strict, procname);  /* try to copy metatable push onto stack */
    lua_setmetatable(L, copyIndex);  /* set metatable and pop copyedMeta */
    agn_poptop(L);  /* pop lua_getmetatable pushed value */
  }
  if (agn_getutype(L, n)) {
    agn_setutype(L, copyIndex, -1);
    agn_poptop(L);  /* pop type (string) */
  }
}

int agnB_aux_deepdive (lua_State* L, int n, int CacheT, int nargs, int caller, int strict, const char *procname) {  /* 3.9.2/3 */
  int copyIndex, type;
  copyIndex = 0;
  type = lua_type(L, n);
  if (type < LUA_TTABLE || type > LUA_TSET) {  /* 3.9.4 tweak */
    lua_pushvalue(L, n);
    return lua_gettop(L);  /* = copyIndex */
  }
  switch (type) {
    case LUA_TTABLE: {
      int type;
      luaL_checkstack(L, 3, "not enough stack space");
      lua_pushvalue(L, n);  /* push key */
      /* try to get cached obj (should pop key from stack and push get value onto stack) */
      type = lua_gettable(L, CacheT);
      if (type == LUA_TTABLE) {  /* just return */
        copyIndex = lua_gettop(L);  /* push 1 */
      } else  {  /* no cache hit */
        int keyIndex, valueIndex, copyedKey, copyedValue;
        lua_pop(L, 1);  /* pop the pushed get table return value */
        lua_newtable(L);
        copyIndex = lua_gettop(L);
        lua_pushvalue(L, n);  /* push key */
        lua_pushvalue(L, copyIndex);  /* push value */
        lua_settable(L, CacheT);  /* save created table into cacheT */
        /* table is in the stack at index 't' */
        lua_pushnil(L);  /* first key */
        while (lua_next(L, n)) {
          /* uses 'key' (at index -2) and 'value' (at index -1) */
          luaL_checkstack(L, nargs < 4 ? 4 : nargs, "not enough stack space");
          keyIndex = lua_absindex(L, -2);  /* 1 */
          valueIndex = lua_absindex(L, -1);  /* 2 */
          copyedKey = agnB_aux_deepdive(L, keyIndex, CacheT, nargs, caller, strict, procname);  /* 3 */
          copyedValue = agnB_aux_deepdive(L, valueIndex, CacheT, nargs, caller, strict, procname);  /* 4 */
          lua_pushvalue(L, copyedKey);  /* push key */
          /* The following is the actual mapping segment. We will NOT look up the substituted values as due to all
             the intermediate pushing, they migrate in the stack ... */
          aux_deepdive_doop(L, copyedValue, nargs, caller, strict, type);
          /* end of mapping segment */
          lua_settable(L, copyIndex);
          /* removes 'value'; keeps 'key' for next iteration */
          lua_pop(L, 3);
        }
        aux_deepdive_setmetatype(L, n, CacheT, copyIndex, nargs, caller, strict, procname);
      }
      break;
    }
    case LUA_TSET: {
      int type;
      luaL_checkstack(L, 3, "not enough stack space");
      lua_pushvalue(L, n);  /* push key */
      /* try to get cached obj (should pop key from stack and push get value onto stack) */
      type = lua_gettable(L, CacheT);
      if (type == LUA_TSET) {  /* just return */
        copyIndex = lua_gettop(L);  /* push 1 */
      } else  {  /* no cache hit */
        int valueIndex, copyedValue;
        lua_pop(L, 1);  /* pop the pushed get table return value */
        agn_createset(L, agn_ssize(L, n));
        copyIndex = lua_gettop(L);
        lua_pushvalue(L, n);  /* push key */
        lua_pushvalue(L, copyIndex);  /* push value */
        lua_settable(L, CacheT);  /* save created table into cacheT */
        /* table is in the stack at index 't' */
        lua_pushnil(L);  /* first key */
        while (lua_usnext(L, n)) {
          /* uses 'key' (at index -2) and 'value' (at index -1) */
          luaL_checkstack(L, nargs < 4 ? 4 : nargs, "not enough stack space");
          valueIndex = lua_absindex(L, -1);  /* 2 */
          copyedValue = agnB_aux_deepdive(L, valueIndex, CacheT, nargs, caller, strict, procname);  /* 4 */
          /* The following is the actual mapping segment. We will NOT look up the substituted values as due to all
             the intermediate pushing, they migrate in the stack ... */
          aux_deepdive_doop(L, copyedValue, nargs, caller, strict, type);
          /* end of mapping segment */
          lua_srawset(L, copyIndex);
          /* removes 'value'; keeps 'key' for next iteration */
          lua_pop(L, 2);
        }
        aux_deepdive_setmetatype(L, n, CacheT, copyIndex, nargs, caller, strict, procname);
      }
      break;
    }
    case LUA_TSEQ: {
      int type;
      luaL_checkstack(L, 3, "not enough stack space");
      lua_pushvalue(L, n);  /* push key */
      /* try to get cached obj (should pop key from stack and push get value onto stack) */
      type = lua_gettable(L, CacheT);
      if (type == LUA_TSEQ) {  /* just return */
        copyIndex = lua_gettop(L);  /* push 1 */
      } else  {  /* no cache hit */
        int valueIndex, copyedValue;
        size_t i, seqsize;
        lua_pop(L, 1);  /* pop the pushed get table return value */
        seqsize = agn_seqsize(L, n);
        agn_createseq(L, seqsize);
        copyIndex = lua_gettop(L);
        lua_pushvalue(L, n);  /* push key */
        lua_pushvalue(L, copyIndex);  /* push value */
        lua_settable(L, CacheT);  /* save created table into cacheT */
        /* table is in the stack at index 't' */
        for (i=1; i <= seqsize; i++) {
          /* uses 'key' (at index -2) and 'value' (at index -1) */
          luaL_checkstack(L, nargs < 4 ? 4 : nargs, "not enough stack space");
          lua_seqrawgeti(L, n, i);
          valueIndex = lua_absindex(L, -1);  /* 2 */
          copyedValue = agnB_aux_deepdive(L, valueIndex, CacheT, nargs, caller, strict, procname);  /* 4 */
          /* The following is the actual mapping segment. We will NOT look up the substituted values as due to all
             the intermediate pushing, they migrate in the stack ... */
          aux_deepdive_doop(L, copyedValue, nargs, caller, strict, type);
          /* end of mapping segment */
          lua_seqrawseti(L, copyIndex, i);
          /* removes 'value'; keeps 'key' for next iteration */
          lua_pop(L, 2);
        }
        aux_deepdive_setmetatype(L, n, CacheT, copyIndex, nargs, caller, strict, procname);
      }
      break;
    }
    case LUA_TPAIR: {
      int type;
      luaL_checkstack(L, 3, "not enough stack space");
      lua_pushvalue(L, n);  /* push key */
      /* try to get cached obj (should pop key from stack and push get value onto stack) */
      type = lua_gettable(L, CacheT);
      if (type == LUA_TPAIR) {  /* just return */
        copyIndex = lua_gettop(L);  /* push 1 */
      } else  {  /* no cache hit */
        int valueIndex, copyedValue;
        size_t i;
        lua_pop(L, 1);  /* pop the pushed get table return value */
        lua_pushnil(L);
        lua_pushnil(L);
        agn_createpair(L, -2, -1);
        lua_remove(L, -2); lua_remove(L, -2);
        copyIndex = lua_gettop(L);
        lua_pushvalue(L, n);  /* push key */
        lua_pushvalue(L, copyIndex);  /* push value */
        lua_settable(L, CacheT);  /* save created table into cacheT */
        /* table is in the stack at index 't' */
        for (i=1; i < 3; i++) {
          /* uses 'key' (at index -2) and 'value' (at index -1) */
          luaL_checkstack(L, nargs < 4 ? 4 : nargs, "not enough stack space");
          agn_pairgeti(L, n, i);
          valueIndex = lua_absindex(L, -1);  /* 2 */
          copyedValue = agnB_aux_deepdive(L, valueIndex, CacheT, nargs, caller, strict, procname);  /* 4 */
          /* The following is the actual mapping segment. We will NOT look up the substituted values as due to all
             the intermediate pushing, they migrate in the stack ... */
          aux_deepdive_doop(L, copyedValue, nargs, caller, strict, type);
          /* end of mapping segment */
          agn_pairseti(L, copyIndex, i);
          /* removes 'value'; keeps 'key' for next iteration */
          lua_pop(L, 2);
        }
        aux_deepdive_setmetatype(L, n, CacheT, copyIndex, nargs, caller, strict, procname);
      }
      break;
    }
    case LUA_TREG: {
      int type;
      luaL_checkstack(L, 3, "not enough stack space");
      lua_pushvalue(L, n);  /* push key */
      /* try to get cached obj (should pop key from stack and push get value onto stack) */
      type = lua_gettable(L, CacheT);
      if (type == LUA_TREG) {  /* just return */
        copyIndex = lua_gettop(L);  /* push 1 */
      } else  {  /* no cache hit */
        int valueIndex, copyedValue;
        size_t i, regsize;
        lua_pop(L, 1);  /* pop the pushed get table return value */
        regsize = agn_regsize(L, n);
        agn_createreg(L, regsize);
        copyIndex = lua_gettop(L);
        lua_pushvalue(L, n);  /* push key */
        lua_pushvalue(L, copyIndex);  /* push value */
        lua_settable(L, CacheT);  /* save created table into cacheT */
        /* table is in the stack at index 't' */
        for (i=1; i <= regsize; i++) {
          /* uses 'key' (at index -2) and 'value' (at index -1) */
          luaL_checkstack(L, nargs < 4 ? 4 : nargs, "not enough stack space");
          agn_regrawgeti(L, n, i);
          valueIndex = lua_absindex(L, -1);  /* 2 */
          copyedValue = agnB_aux_deepdive(L, valueIndex, CacheT, nargs, caller, strict, procname);  /* 4 */
          /* The following is the actual mapping segment. We will NOT look up the substituted values as due to all
             the intermediate pushing, they migrate in the stack ... */
          aux_deepdive_doop(L, copyedValue, nargs, caller, strict, type);
          /* end of mapping segment */
          agn_regrawseti(L, copyIndex, i);
          /* removes 'value'; keeps 'key' for next iteration */
          lua_pop(L, 2);
        }
        aux_deepdive_setmetatype(L, n, CacheT, copyIndex, nargs, caller, strict, procname);
      }
      break;
    }
    default:  /* sequences, registers, sets, pairs and complex numbers not accepted in the input structure */
      luaL_error(L, "Error in " LUA_QS ": type " LUA_QS " is not supported.", procname, luaL_typename(L, n));
  }
  return copyIndex;
}


/* patched 25.12.2009, 0.29.3, extended 2.29.0 */
#define AGN_RESIZESET 8
static int agnB_map (lua_State *L) {
  int j, k, nargs, idx, newstruct, multipass, strict, reshuffle, multret, descend, plain, c, flag;
  ptrdiff_t nrets;
  luaL_checktype(L, 1, LUA_TFUNCTION);
  nargs = lua_gettop(L);
  agnB_aux_fcheckoptions(L, &nargs, &newstruct, &multipass, &strict, &reshuffle, &multret, &descend, &plain, "map");
  if (!multipass)
    luaL_error(L, "Error in " LUA_QS ": " LUA_QS " option is not supported.", "map", "multipass");
  if (!strict)
    luaL_error(L, "Error in " LUA_QS ": " LUA_QS " option is not supported.", "map", "strict");
  if (descend) {  /* 3.9.2 */
    int cacheT, type;
    type = lua_type(L, 2);
    if (type == LUA_TNIL) {
      lua_pushnil(L);
      return 1;
    }
    luaL_typecheck(L, type >= LUA_TTABLE, 2, "table, set, sequence, register or pair expected", type);
    luaL_checkstack(L, 1, "not enough stack space");  /* 3.15.2 fix */
    lua_newtable(L);
    cacheT = lua_gettop(L);
    agnB_aux_deepdive(L, 2, cacheT, nargs, 0, 0, "map");
    lua_remove(L, cacheT);
    return 1;
  }
  lua_settop(L, nargs);  /* purge all options if given */
  c = 0; idx = 0; flag = 1;
  switch (lua_type(L, 2)) {
    case LUA_TTABLE: {
      size_t n;
      /* very rough guess whether table is an array or dictionary */
      n = lua_objlen(L, 2);
      idx = -3; nrets = 1;
      luaL_checkstack(L, 2, "not enough stack space");  /* 3.15.1 fix */
      if (newstruct) {
        aux_createtable(L, 2, n, 1);
      } else {  /* push structure explicitly, we cannot play with lua_settop(L, 2) unfortunately INPLACE */
        lua_pushvalue(L, 2);
      }
      lua_pushnil(L);
      while (agn_fnext(L, 2, 1, 0)) {
        if (nargs > 2) {
          luaL_checkstack(L, nargs - 2, "not enough stack space");  /* 3.15.1 fix */
          for (j=3; j <= nargs; j++) lua_pushvalue(L, j);
        }
        /* call function with nargs-1 argument(s) and the given results, 3.9.2 change */
        nrets = lua_call(L, nargs - 1, multret ? LUA_MULTRET : 1);
        /* new table is at -3, store result in a table; rawset2 only deletes the value from the stack, the key is kept */
        if (nrets == 1) {  /* only one return */
          lua_rawset2(L, idx);  /* deletes only the value, but not the key */
        } else {  /* 3.9.1/2, multret extension reserve one further slot for transfer of elements */
          luaL_checkstack(L, 1, "not enough stack space");
          for (k=nrets; k > 0; k--) {
            lua_pushvalue(L, -k);
            lua_seti(L, idx - nrets, ++c);
          }
          lua_pop(L, nrets);
        }
      }
      if (nrets == 1 && reshuffle) {
        agn_reorder(L, -1, 0);  /* remove any hole from the table array, 3.9.2 */
      }
      break;
    }  /* end of case LUA_TTABLE */
    case LUA_TSET: {
      luaL_checkstack(L, 2, "not enough stack space");  /* 3.15.1 fix */
      if (newstruct) {
        agn_createset(L, agn_ssize(L, 2));
        luaL_setmetatype(L, 2);
        lua_pushnil(L);
        while (lua_usnext(L, 2)) {
          luaL_checkstack(L, 2 + (nargs > 2)*(nargs - 2), "not enough stack space");  /* 3.15.1 fix */
          lua_pushvalue(L, 1);   /* push function */
          lua_pushvalue(L, -2);  /* push set value */
          for (j=3; j <= nargs; j++)
            lua_pushvalue(L, j);
          lua_call(L, nargs - 1, 1);  /* call function with nargs-1 argument(s) and one result, 3.9.2 change */
          lua_srawset(L, -4);  /* the new set is at -4, store result to new set */
          agn_poptop(L);
        }
      } else {  /* we cannot directly substitute values in the input set, as deleting the old values
        would invalidate the iteration keys. So collect the mapping results in a new set, cleanse the input
        set thereafter and then reinsert the new values from the new set into the input set. While doing this,
        we reduce the size of the input and the new set incrementally to save memory. Reimplemented 5.1.3.
        The new set is now on the stack top: cleanse input set. */
        size_t l, oldl;
        l = oldl = agn_ssize(L, 2);
        lua_pushvalue(L, 2);
        agn_createset(L, 0);  /* zero to save memory */
        lua_pushnil(L);
        while (lua_usnext(L, -3) != 0) {  /* traverse input set */
          luaL_checkstack(L, 2 + (nargs > 2)*(nargs - 2), "not enough stack space");  /* 3.15.1 fix */
          lua_pushvalue(L, 1);   /* push function */
          lua_pushvalue(L, -2);  /* push set value */
          for (j=3; j <= nargs; j++)
            lua_pushvalue(L, j);
          lua_call(L, nargs - 1, 1);  /* call function with nargs-1 argument(s) and one result, 3.9.2 change */
          lua_srawset(L, -4);  /* the new set is at -4, store result to new set */
          lua_sdelete(L, -4); /* delete value from input set and pop it */
          agn_poptop(L); /* pop the key from stack, too, since it cannot
           be used for next iteration as it has been
           purged from the set */
          lua_pushnil(L); /* restart iteration */
          if (--l % AGN_RESIZESET == 0) {
            agn_setresize(L, -3, l, 1);  /* shrink input set */
            lua_gc(L, LUA_GCCOLLECT, 0);
          }
        }
        l = oldl;
        /* new set is now on the stack top, the input set is beneath */
        lua_pushnil(L);
        while (lua_usnext(L, -2)) {
          lua_sinsert(L, -4);
          lua_sdelete(L, -2);
          lua_pushnil(L);
          if (--l % AGN_RESIZESET == 0) {
            agn_setresize(L, -2, l, 1);  /* shrink new set */
            lua_gc(L, LUA_GCCOLLECT, 0);
          }
        }
        agn_poptop(L);  /* pop new set */
        lua_gc(L, LUA_GCCOLLECT, 0);
      }
      break;
    }  /* end of case LUA_TSET */
    case LUA_TSEQ: {  /* 0.11.0 */
      int i, nops;  /* index */
      nops = agn_seqsize(L, 2);
      idx = -2;
      luaL_checkstack(L, 1, "not enough stack space");  /* 3.15.1 fix */
      if (newstruct) {  /* 2.29.0, new sequence ? */
        agn_createseq(L, nops);
        luaL_setmetatype(L, 2);
      } else {  /* push structure explicitly, we cannot play with lua_settop(L, 2) unfortunately INPLACE */
        lua_pushvalue(L, 2);
      }
      /* now traverse sequence */
      for (i=0; i < nops; i++) {
        luaL_checkstack(L, 2 + (nargs > 2)*(nargs - 2), "not enough stack space");  /* 3.15.1 fix */
        lua_pushvalue(L, 1);  /* push function */
        lua_seqrawgeti(L, 2, i + 1);
        for (j=3; j <= nargs; j++)
          lua_pushvalue(L, j);
        /* call function with nargs-1 argument(s) and the given results, 3.9.2 change */
        nrets = lua_call(L, nargs - 1, multret ? LUA_MULTRET : 1);
        /* lua_seqinsert(L, idx); */
        if (nrets == 1) {  /* 3.9.1 multret extension */
          lua_seqrawseti(L, idx, i + 1);  /* sequence is at -2, store result to new sequence */
        } else {
          /* reserve one further slot for transfer of elements */
          luaL_checkstack(L, 1, "not enough stack space");
          for (k=nrets; k > 0; k--) {
            lua_pushvalue(L, -k);
            lua_seqinsert(L, idx - nrets);
          }
          lua_pop(L, nrets);
        }
      }
      break;
    }  /* end of case LUA_TSEQ */
    case LUA_TREG: {  /* 2.3.0 RC 3: map a function to all the elements in a register up to the _current_ top. Values
      beyond the top are not copied. The size of the new register is determined by gettop(obj). */
      int i, nops;
      nops = agn_reggettop(L, 2);
      idx = -2;
      luaL_checkstack(L, 1, "not enough stack space");  /* 3.15.1 fix */
      if (newstruct) {
        agn_createreg(L, nops);
        luaL_setmetatype(L, 2);
      } else {  /* push structure explicitly, we cannot play with lua_settop(L, 2) unfortunately INPLACE */
        lua_pushvalue(L, 2);
      }
      /* now traverse array */
      for (i=0; i < nops; i++) {
        luaL_checkstack(L, 2 + (nargs > 2)*(nargs - 2), "not enough stack space");  /* 3.15.1 fix */
        lua_pushvalue(L, 1);  /* push function */
        agn_regrawgeti(L, 2, i + 1);
        for (j=3; j <= nargs; j++)
          lua_pushvalue(L, j);
        /* call function with nargs-1 argument(s) and the given results, 3.9.2 change */
        nrets = lua_call(L, nargs - 1, multret ? LUA_MULTRET : 1);
        if (nrets == 1) {  /* 3.9.1 multret extension */
          agn_regrawseti(L, idx, i + 1);  /* store result to register */
        } else {
          luaL_checkstack(L, 1, "not enough stack space");  /* 3.9.2, reserve one further slot for transfer of elements */
          if (flag) {
            agn_regresize(L, idx - nrets + 1, nrets*nops, 0);
            flag = 0;
          }
          for (k=nrets; k > 0; k--) {
            lua_pushvalue(L, -k);
            agn_regrawseti(L, idx - nrets, ++c);
          }
          lua_pop(L, nrets);
        }
      }
      break;
    }  /* end of case LUA_TREQ */
    case LUA_TSTRING: {  /* 1.5.0 */
      size_t l;
      const char *str = agn_checklstring(L, 2, &l);
      idx = -2;
      if (!newstruct)
        luaL_error(L, "Error in " LUA_QS ": strings are not eligible to in-place substitution.", "map");
      luaL_checkstack(L, 1, "not enough stack space");  /* 3.15.1 fix */
      agn_createseq(L, l);
      /* now traverse string */
      while (*str) {
        luaL_checkstack(L, 2 + (nargs > 2)*(nargs - 2), "not enough stack space");  /* 3.15.1 fix */
        lua_pushvalue(L, 1);      /* push function */
        lua_pushlstring(L, str, 1);
        for (j=3; j <= nargs; j++)
          lua_pushvalue(L, j);
        /* call function with nargs-1 argument(s) and the given results, 3.9.2 change */
        nrets = lua_call(L, nargs - 1, multret ? LUA_MULTRET : 1);
        if (nrets == 1) {  /* 3.9.1 multret extension */
          lua_seqinsert(L, -2);
        } else {
          /* 3.9.2, reserve one further slot for transfer of elements */
          luaL_checkstack(L, 1, "not enough stack space");
          for (k=nrets; k > 0; k--) {
            lua_pushvalue(L, -k);
            lua_seqrawseti(L, idx - nrets, ++c);
          }
          lua_pop(L, nrets);
        }
        str++;
      }
      break;
    }  /* end of case LUA_TSTRING */
    case LUA_TPAIR: {  /* 1.8.8 */
      int i;  /* index */
      idx = 2;
      /* traverse pair */
      for (i=1; i < 3; i++) {
        luaL_checkstack(L, 2 + (nargs > 2)*(nargs - 2), "not enough stack space");  /* 3.15.1/2 fix */
        lua_pushvalue(L, 1);  /* push function */
        agn_pairgeti(L, 2, i);
        for (j=3; j <= nargs; j++)
          lua_pushvalue(L, j);
        lua_call(L, nargs - 1, 1);  /* call function with nargs-1 argument(s) and one result */
      }
      if (newstruct) {
        luaL_checkstack(L, 1, "not enough stack space");  /* 3.15.1 fix */
        agn_createpair(L, -2, -1);
        lua_remove(L, -2); lua_remove(L, -2);  /* remove left and right value, agn_createpair does not pop these values */
        /* set its type to the one of the original pair */
        luaL_setmetatype(L, 2);
      } else {
        agn_pairseti(L, idx, 2);
        agn_pairseti(L, idx, 1);
        /* left and right value have been popped */
      }
      break;
    }  /* end of case LUA_TPAIR */
    case LUA_TFUNCTION: {  /* 2.12.0 RC 1, function composition (f @ g)(x) = f(g(x)), by suggestion of Slobodan */
      if (!newstruct)
        luaL_error(L, "Error in " LUA_QS ": functions are not eligible to in-place substitution.", "map");
      lua_settop(L, 2);  /* ignore any further arguments */
      lua_pushcclosure(L, &aux_compose, 2);  /* return composite function */
      break;
    }
    case LUA_TNIL: {  /* 4.12.1 */
      lua_pushnil(L);
      break;
    }
    default:
      luaL_error(L, "Error in " LUA_QS ": structure, string or function expected for argument #2,\n   got %s.", "map",
        luaL_typename(L, 2));
  }  /* end of switch */
  return 1;
}


/* DO NOT DELETE ! Used by `@` operator, based on old 3.3.0 `map` version, 3.17.5. This version processes two arguments only,
   and one result only, ignoring all the other results, preventing stack corruption when executed in the VM. `map` above is
   more versatile but does not work in the VM as it can deal with multiple results which Valgrind complains about by aborting
   execution and trying to index `null` values. Changed 7.6.11 */
static int agnB_vmmap (lua_State *L) {
  agn_map(L, 1, 2);
  return 1;
}


static int agnB_subs (lua_State *L) {  /* changed 4.11.2 */
  int i, n, flag, nargs, nops, newstruct, multipass, strict, reshuffle, multret, descend, plain;
  lua_Number x, y, z;
  nargs = lua_gettop(L);
  agnB_aux_fcheckoptions(L, &nargs, &newstruct, &multipass, &strict, &reshuffle, &multret, &descend, &plain, "subs");
  if (multret)
    luaL_error(L, "Error in " LUA_QS ": " LUA_QS " option is not supported.", "subs", "multret");
  luaL_checkstack(L, nargs, "too many arguments");
  for (i=1; i < nargs; i++)
    luaL_checktype(L, i, LUA_TPAIR);
  if (descend) {  /* 3.9.2 */
    int cacheT, type;
    type = lua_type(L, 2);
    if (type == LUA_TNIL) {
      lua_pushnil(L);
      return 1;
    }
    luaL_typecheck(L, type >= LUA_TTABLE, 2, "table, set, sequence, register or pair expected", type);
    lua_newtable(L);
    cacheT = lua_gettop(L);
    agnB_aux_deepdive(L, nargs, cacheT, nargs, 1, strict, "subs");
    lua_remove(L, cacheT);
    return 1;
  }
  switch (lua_type(L, nargs)) {
    case LUA_TTABLE: {
      lua_Number eps = agn_getepsilon(L);
      if (plain || (newstruct == 0 && multipass == 1 && strict == 1)) {  /* 5.1.3/4 */
        /* check for properly allocated slots will be done by agn_pairgetiall() */
        for (i=1; i < nargs; i++) {
          agn_pairgetiall(L, i);    /* get lhs, rhs, 7.7.8 */
          agn_tabsubs(L, nargs, -2, -1);
          lua_pop(L, 2);
        }
        if (reshuffle) agn_reorder(L, nargs, 0);  /* remove any hole from the table array */
        lua_pushvalue(L, nargs);
        return 1;
      }
      n = luaL_getn(L, nargs);
      if (newstruct) {
        aux_createtable(L, nargs, n, 1);  /* copies metatable and type, as well */
      } else
        lua_pushvalue(L, nargs);
      /* start traversal */
      lua_pushnil(L);
      while (lua_next(L, nargs)) {
        flag = 0;
        for (i=1; i < nargs; i++) {
          if (agn_isnumber(L, -1) && agn_pairgetinumbers(L, i, &y, &z)) {  /* new 4.11.2 */
            x = agn_tonumber(L, -1);
            if ((strict) ? x == y : tools_approx(x, y, eps)) {
              agn_poptop(L);          /* replace value ... */
              lua_pushnumber(L, z);   /* ... with rhs */
              flag = 1;
              if (!multipass) break;  /* don't check rest of the pairs */
            }
          } else {
            agn_pairgeti(L, i, 1);    /* get lhs */
            if (strict ? lua_equal(L, -1, -2) : lua_rawaequal(L, -1, -2)) {
              agn_poptoptwo(L);       /* delete value and lhs */
              agn_pairgeti(L, i, 2);  /* push rhs */
              flag = 1;
              if (!multipass) break;
            } else {
              agn_poptop(L);  /* delete lhs */
            }
          }
        }
        /* we have now checked all substitution pairs against the _current_ table value; on the stack
           now are the table index at -2 and either the original or substituted value at -1. */
        if (!flag && !newstruct)  /* with in-place substitution replace only if needed */
          agn_poptop(L);
        else
          lua_rawset2(L, -3);     /* store value to table */
      }
      if (reshuffle) agn_reorder(L, -1, 0);  /* remove any hole from the table array, 3.9.0 */
      break;
    }  /* end of case LUA_TTABLE */
    case LUA_TSET: {
      if (newstruct) {
        agn_createset(L, agn_ssize(L, nargs));
        lua_pushnil(L);
        while (lua_usnext(L, nargs)) {
          for (i=1; i < nargs; i++) {
            agn_pairgeti(L, i, 1);
            if (strict ? lua_equal(L, -1, -2) : lua_rawaequal(L, -1, -2)) {
              agn_poptoptwo(L);  /* delete value and lhs */
              agn_pairgeti(L, i, 2);  /* push rhs */
              if (!multipass) break;
            } else
              agn_poptop(L);  /* delete lhs */
          }
          lua_srawset(L, -3); /* store value to set, leave index on stack */
        }
        luaL_setmetatype(L, nargs);
      } else {  /* in-place, we must not use usnext for we change the set during traversal, 2.31.5 */
        if (!strict)
          luaL_error(L, "Error in " LUA_QS ": strict=false option is not supported with set in-place operation.", "subs");
        if (multipass) {
          lua_pushvalue(L, nargs);     /* push set */
          for (i=1; i < nargs; i++) {  /* traverse subs list */
            agn_pairgeti(L, i, 1);     /* push lhs */
            if (lua_shas(L, -2, 0)) {  /* leave lhs on stack */
              agn_pairgeti(L, i, 2);   /* push rhs */
              lua_srawset(L, -3);      /* set subs value into set and pop it */
              lua_sdelete(L, -2);      /* delete lhs from set and pop lhs */
            } else
              agn_poptop(L);           /* pop lhs */
          }
        } else {
          int eq;
          Charbuf *buf = bitfield_new(L, nargs - 1, 0x00);
          if (buf == NULL)  /* 4.11.5 fix */
            luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "subs");
          lua_pushvalue(L, nargs);     /* push set */
          for (i=1; i < nargs; i++) {  /* traverse subs list */
            agn_pairgeti(L, i, 1);     /* push lhs */
            eq = lua_shas(L, -2, 0);
            bitfield_setbit(L, buf, i, eq);  /* flag substitution */
            if (eq)  /* delete value from original set and pop value from stack */
              lua_sdelete(L, -2);
            else
              agn_poptop(L);
          }
          for (i=1; i < nargs; i++) {  /* insert rhs's in subs list into original set */
            if (bitfield_get(L, buf, i)) {
              agn_pairgeti(L, i, 2);   /* push rhs */
              lua_sinsert(L, -2);
            }
          }
          bitfield_free(buf);
        }  /* return modified set */
      }
      break;
    }  /* end of case LUA_TSET */
    case LUA_TSEQ: {
      int j;
      lua_Number eps = agn_getepsilon(L);
      if (plain || (newstruct == 0 && multipass == 1 && strict == 1)) {  /* 5.1.3/4 */
        for (i=1; i < nargs; i++) {
          /* check for properly allocated slots will be done by agn_pairgetiall() */
          agn_pairgetiall(L, i);    /* get lhs, rhs, 7.7.8 */
          agn_seqsubs(L, nargs, -2, -1);
          lua_pop(L, 2);
        }
        lua_pushvalue(L, nargs);
        return 1;
      }
      nops = agn_seqsize(L, nargs);
      if (newstruct) {  /* 2.29.0 */
        agn_createseq(L, nops);
        luaL_setmetatype(L, nargs);  /* 4.11.2 fix */
      } else {  /* push structure explicitly, we cannot play with lua_settop(L, 2) unfortunately INPLACE */
        lua_pushvalue(L, nargs);
      }
      /* now traverse sequence */
      for (i=0; i < nops; i++) {
        flag = 0;
        lua_seqrawgeti(L, nargs, i + 1);
        for (j=1; j < nargs; j++) {  /* traverse pairs */
          if (agn_isnumber(L, -1) && agn_pairgetinumbers(L, j, &y, &z)) {  /* new 4.11.2 */
            x = agn_tonumber(L, -1);
            if ((strict) ? x == y : tools_approx(x, y, eps)) {
              agn_poptop(L);          /* pop original sequence or previous substitution value */
              lua_pushnumber(L, z);   /* push new substitution value */
              flag = 1;
              if (!multipass) break;  /* don't check rest of the pairs with current sequence value */
            }
          } else {
            agn_pairgeti(L, j, 1);
            if (strict ? lua_equal(L, -1, -2) : lua_rawaequal(L, -1, -2)) {
              agn_poptoptwo(L);       /* delete value and lhs */
              agn_pairgeti(L, j, 2);  /* push rhs */
              flag = 1;
              if (!multipass) break;
            } else {
              agn_poptop(L);  /* delete lhs */
            }
          }
        }
        /* we have now checked all substitution pairs against the _current_ sequence value; on the stack
           now is the original or substituted value at -1. */
        if (!flag && !newstruct)  /* with in-place substitution replace only if needed */
          agn_poptop(L);
        else
          lua_seqrawseti(L, -2, i + 1);  /* store value in sequence */
      }
      break;
    }  /* end of case LUA_TSEQ */
    case LUA_TREG: {  /* 2.3.0 RC 3 */
      int j;
      lua_Number eps = agn_getepsilon(L);
      if (plain || (newstruct == 0 && multipass == 1 && strict == 1)) {  /* 5.1.3/4 */
        for (i=1; i < nargs; i++) {
          /* check for properly allocated slots will be done by agn_pairgetiall() */
          agn_pairgetiall(L, i);    /* get lhs, rhs, 7.7.8 */
          agn_regsubs(L, nargs, -2, -1);
          lua_pop(L, 2);
        }
        lua_pushvalue(L, nargs);
        return 1;
      }
      nops = agn_reggettop(L, nargs);
      if (newstruct) {  /* 2.29.0 */
        agn_createreg(L, nops);
        luaL_setmetatype(L, nargs);  /* 4.11.2 fix */
      } else {  /* push structure explicitly, we cannot play with lua_settop(L, 2) unfortunately INPLACE */
        lua_pushvalue(L, nargs);
      }
      /* now traverse register */
      for (i=0; i < nops; i++) {
        flag = 0;
        agn_regrawgeti(L, nargs, i + 1);
        for (j=1; j < nargs; j++) {  /* traverse pairs */
          if (agn_isnumber(L, -1) && agn_pairgetinumbers(L, j, &y, &z)) {  /* new 4.11.2 */
            x = agn_tonumber(L, -1);
            if ((strict) ? x == y : tools_approx(x, y, eps)) {
              agn_poptop(L);          /* pop original register or previous substitution value */
              lua_pushnumber(L, z);   /* push new substitution value */
              flag = 1;
              if (!multipass) break;  /* don't check rest of the pairs with current register value */
            }
          } else {
            agn_pairgeti(L, j, 1);
            if (strict ? lua_equal(L, -1, -2) : lua_rawaequal(L, -1, -2)) {
              agn_poptoptwo(L);       /* delete value and lhs */
              agn_pairgeti(L, j, 2);  /* push rhs */
              flag = 1;
              if (!multipass) break;
            } else {
              agn_poptop(L);  /* delete lhs */
            }
          }
        }
        /* we have now checked all substitution pairs against the _current_ register value; on the stack
           now is the original or substituted value at -1. */
        if (!flag && !newstruct)  /* with in-place substitution replace only if needed */
          agn_poptop(L);
        else
          agn_regrawseti(L, -2, i + 1);  /* store value in register */
      }
      break;
    }  /* end of case LUA_TREG */
    case LUA_TNIL: {  /* 4.12.1 */
      lua_pushnil(L);
      break;
    }
    default:
      luaL_error(L, "Error in " LUA_QS ": table, set, sequence or register expected as last argument, got %s.", "subs",
        luaL_typename(L, nargs));
  }  /* end of switch */
  return 1;
}


static int agnB_subsop (lua_State *L) {  /* 3.8.0, based partially on agnB_subs; changed 4.11.2 */
  int i, type, nargs, idx, newstruct, multipass, strict, reshuffle, multret, descend, plain, rc;
  nargs = lua_gettop(L);
  idx = 0;  /* to prevent compiler warnings */
  agnB_aux_fcheckoptions(L, &nargs, &newstruct, &multipass, &strict, &reshuffle, &multret, &descend, &plain, "subsop");
  type = lua_type(L, nargs);  /* if nargs = 0 then a regular error will be issued */
  luaL_typecheck(L, type == LUA_TTABLE || type == LUA_TSEQ || type == LUA_TREG || type == LUA_TNIL, nargs,
     "table, sequence or register expected", type);
  if (multret)
    luaL_error(L, "Error in " LUA_QS ": " LUA_QS " option is not supported.", "subsop", "multret");
  if (!multipass)
    luaL_error(L, "Error in " LUA_QS ": " LUA_QS " option is not supported.", "subsop", "multipass");
  if (!strict)
    luaL_error(L, "Error in " LUA_QS ": " LUA_QS " option is not supported.", "subsop", "strict");
  if (descend)
    luaL_error(L, "Error in " LUA_QS ": " LUA_QS " option is not supported.", "subsop", "descend");
  for (i=1; i < nargs; i++) {  /* before altering the input data, first check the substitution data */
    luaL_checktype(L, i, LUA_TPAIR);
  }
  if (type == LUA_TNIL) {  /* 4.12.1 */
    lua_pushnil(L);
    return 1;
  }
  luaL_checkstack(L, 2 + (type == LUA_TTABLE), "too many arguments");  /* 3.15.2 fix */
  if (newstruct)  /* the default, act non-destructively */
    agn_copy(L, nargs, 3);  /* copy type and metatable, too */
  else
    lua_pushvalue(L, nargs);
  switch (type) {
    case LUA_TTABLE: {
      for (i=1; i < nargs; i++) {  /* traverse pairs */
        agn_pairgeti(L, i, 1);     /* get lhs */
        lua_gettable(L, nargs);    /* get table value */
        rc = lua_isnil(L, -1);
        agn_poptop(L);             /* delete value (the value found or nil) */
        if (!rc) {
          /* check for properly allocated slots will be done by agn_pairgetiall() */
          agn_pairgetiall(L, i);    /* get lhs, rhs, 7.7.8 */
          lua_settable(L, -3);     /* set rhs to table at index lhs */
        }
      }
      if (reshuffle) agn_reorder(L, -1, 0);  /* remove any hole from the table array, 3.9.0 */
      break;
    }  /* end of case LUA_TTABLE */
    case LUA_TSEQ: {
      for (i=1; i < nargs; i++) {
        agn_pairgeti(L, i, 1);
        rc = !agn_isposint(L, -1) || (idx = agn_tointeger(L, -1)) > agn_seqsize(L, nargs);
        agn_poptop(L);
        if (!rc) {
          /* replace value at position idx; if new value is null, purges position and closes the space */
          agn_pairgeti(L, i, 2);
          lua_seqrawseti(L, -2, idx);
        }
      }
      break;
    }  /* end of case LUA_TSEQ */
    case LUA_TREG: {
      for (i=1; i < nargs; i++) {
        agn_pairgeti(L, i, 1);
        rc = !agn_isposint(L, -1) || (idx = agn_tointeger(L, -1)) > agn_regsize(L, nargs);
        agn_poptop(L);
        if (!rc) {
          /* replace value at position idx; if new value is null, purges position and closes the space */
          agn_pairgeti(L, i, 2);
          if (!lua_isnil(L, -1)) {
            agn_regrawseti(L, -2, idx);
          } else {
            agn_poptop(L);
            agn_regpurge(L, -1, idx);
          }
        }
      }
      break;
    }  /* end of case LUA_TREG */
    default:
      luaL_error(L, "Error in " LUA_QS ": this should not happen.", "subsop");
  }  /* end of switch */
  return 1;
}


/* 0.9.1; January 11, 2008; fixed June 30, 2008; fixed December 25, 2009 */
static int luaB_select (lua_State *L) {
  size_t j;
  int idx, nargs, newstruct, multipass, strict, reshuffle, multret, descend, plain;
  luaL_checktype(L, 1, LUA_TFUNCTION);
  nargs = lua_gettop(L);
  agnB_aux_fcheckoptions(L, &nargs, &newstruct, &multipass, &strict, &reshuffle, &multret, &descend, &plain, "select");
  if (multret)
    luaL_error(L, "Error in " LUA_QS ": " LUA_QS " option is not supported.", "select", "multret");
  if (!multipass)
    luaL_error(L, "Error in " LUA_QS ": " LUA_QS " option is not supported.", "select", "multipass");
  if (!strict)
    luaL_error(L, "Error in " LUA_QS ": " LUA_QS " option is not supported.", "select", "strict");
  if (descend)
    luaL_error(L, "Error in " LUA_QS ": " LUA_QS " option is not supported.", "select", "descend");
  switch(lua_type(L, 2)) {
    case LUA_TTABLE: {
      size_t n, c;
      if (!newstruct && reshuffle)
        luaL_error(L, "Error in " LUA_QS ": got invalid combination of " LUA_QS " and " LUA_QS " option.", "select", "reshuffle", "inplace");
      /* very rough guess whether table is an array or dictionary */
      n = lua_objlen(L, 2);
      c = 1;
      idx = -3;
      luaL_checkstack(L, 2, "not enough stack space");  /* 3.15.1 fix */
      if (newstruct) {  /* changed 2.29.0 */
        aux_createtable(L, 2, n, 2);
      } else {  /* push structure explicitly, we cannot play with lua_settop(L, 2) unfortunately INPLACE */
        lua_pushvalue(L, 2);
      }
      /* start traversal */
      lua_pushnil(L);
      while (agn_fnext(L, 2, 1, 1)) {
        if (nargs > 2) {  /* 3.15.1 fix */
          luaL_checkstack(L, nargs - 2, "not enough stack space");
          for (j=3; j <= nargs; j++)
            lua_pushvalue(L, j);
        }
        lua_call(L, nargs - 1, 1);  /* call function with nargs-1 argument(s) and one result */
        if (lua_istrue(L, -1)) {  /* 5.0.1 fix */
          agn_poptop(L);
          if (reshuffle)  /* remap to array */
            lua_rawseti(L, idx, c);  /* table is at -3 */
          else
            lua_rawset2(L, idx);  /* store value in a table, leave key on stack */
          c++;
        } else {  /* in-place operation: delete non-satisfying value from table, 2.29.0 */
          agn_poptoptwo(L);  /* delete result and value, leave key on stack */
          if (!newstruct) {  /* in-place operation: delete non-fitting value from structure */
            lua_pushvalue(L, -1);  /* push key once again */
            lua_pushnil(L);
            lua_rawset(L, idx - 1);  /* deletes key and null */
          }
        }
      }
      agn_tabresize(L, -1, c, !reshuffle);
      break;
    }
    case LUA_TSET: {  /* rewritten 2.31.5 to support in-place operation */
      luaL_checkstack(L, 2 + !newstruct, "not enough stack space");  /* 3.15.1 fix */
      if (!newstruct) lua_pushvalue(L, nargs);
      agn_createset(L, agn_ssize(L, 2)/2);
      luaL_setmetatype(L, 2);
      /* start traversal */
      /* with in-place op, we cannot modify the set that lua_usnext is traversing, so we use a collector help set with
         all the values later to be deleted from the original set */
      lua_pushnil(L);
      while (lua_usnext(L, 2)) {
        luaL_checkstack(L, 2 + (nargs > 2)*(nargs - 2), "not enough stack space");  /* 3.15.1 fix */
        lua_pushvalue(L, 1);      /* push function */
        lua_pushvalue(L, -2);     /* push key (value) */
        for (j=3; j <= nargs; j++)
          lua_pushvalue(L, j);
        lua_call(L, nargs - 1, 1);  /* call function with nargs-1 argument(s) and one result */
        if ((newstruct && lua_istrue(L, -1)) || (!newstruct && lua_isfalse(L, -1))) {  /* set key to new or collector set */
          agn_poptop(L);
          lua_srawset(L, -3);
        } else {
          agn_poptoptwo(L);
        }
      }
      if (!newstruct) {  /* in-place: now delete non-fitting values from original set */
        lua_pushnil(L);
        while (lua_usnext(L, -2)) {
          lua_sdelete(L, -4);
        }
        agn_poptop(L);  /* drop collector set, return original set */
      }
      break;
    }
    case LUA_TSEQ: {  /* 0.11.1 */
      int i, j, c, nops;
      nops = agn_seqsize(L, 2);
      idx = -3;
      luaL_checkstack(L, 1, "not enough stack space");  /* 3.15.1 fix */
      if (newstruct) {
        agn_createseq(L, nops/2);
        luaL_setmetatype(L, 2);
      } else {  /* push structure explicitly, we cannot play with lua_settop(L, 2) unfortunately INPLACE */
        lua_pushvalue(L, 2);
      }
      /* now traverse sequence */
      c = 0;
      for (i=0; i < nops; i++) {
        luaL_checkstack(L, 2 + (nargs > 2)*(nargs - 2), "not enough stack space");  /* 3.15.1 fix */
        lua_pushvalue(L, 1);  /* push function */
        lua_seqrawgeti(L, 2, i + 1);
        for (j=3; j <= nargs; j++)
          lua_pushvalue(L, j);
        lua_call(L, nargs - 1, 1);  /* call function with nargs-1 argument(s) and one result */
        if (lua_istrue(L, -1)) {  /* 5.0.1 fix */
          lua_seqrawgeti(L, 2, i + 1);
          lua_seqrawseti(L, idx, ++c);  /* store value to sequence, new sequence is at -3, changed 2.29.0 */
        }
        agn_poptop(L);
      }
      agn_seqresize(L, -1, c);
      break;
    } /* end of case LUA_TSEQ */
    case LUA_TREG: {  /* 2.3.0 RC 3 */
      int i, nops, c;
      nops = agn_reggettop(L, 2);
      idx = -3;
      luaL_checkstack(L, 1, "not enough stack space");  /* 3.15.1 fix */
      if (newstruct) {  /* changed 2.29.0 */
        agn_createreg(L, nops);  /* patched 2.29.3 */
        luaL_setmetatype(L, 2);
      } else {  /* push structure explicitly, we cannot play with lua_settop(L, 2) unfortunately INPLACE */
        lua_pushvalue(L, 2);
      }
      /* now traverse register */
      c = 0;
      for (i=0; i < nops; i++) {
        luaL_checkstack(L, 2 + (nargs > 2)*(nargs - 2), "not enough stack space");  /* 3.15.1 fix */
        lua_pushvalue(L, 1);  /* push function */
        agn_regrawgeti(L, 2, i + 1);
        for (j=3; j <= nargs; j++)
          lua_pushvalue(L, j);
        lua_call(L, nargs - 1, 1);  /* call function with nargs-1 argument(s) and one result */
        if (lua_istrue(L, -1)) {  /* 5.0.1 fix */
          agn_regrawgeti(L, 2, i + 1);
          agn_regrawseti(L, idx, ++c);  /* store value to new register, register is at -3 or 2 */
        }
        agn_poptop(L);
      }
      if (!newstruct) {
        for (j=nops; j > c; j--) {  /* from right to left ! */
          lua_pushnil(L);
          agn_regrawseti(L, -2, j);  /* 2.29.4 fix */
        }
        lua_pushinteger(L, c);
        agn_regsettop(L, -2);  /* 2.29.4 fix */
      }
      agn_regresize(L, -1, c, 0);  /* 2.29.4 fix */
      break;
    }  /* end of case LUA_TREG */
    case LUA_TNIL: {  /* 4.12.1 */
      lua_pushnil(L);
      break;
    }
    case LUA_TSTRING: {  /* 7.3.3/4 extension */
      size_t i, j, l;
      luaL_Buffer b;
      const char *s = lua_tolstring(L, 2, &l);
      luaL_buffinit(L, &b);
      /* now traverse string from left to right */
      for (i=0; i < l; i++) {
        luaL_checkstack(L, 2 + (nargs > 2)*(nargs - 2), "not enough stack space");
        lua_pushvalue(L, 1);  /* push function */
        lua_pushchar(L, uchar(s[i]));  /* push character */
        for (j=3; j <= nargs; j++)
          lua_pushvalue(L, j);
        lua_call(L, nargs - 1, 1);  /* call function with nargs-1 argument(s) and one result */
        if (lua_istrue(L, -1)) {
          luaL_addchar(&b, uchar(s[i]));
        }
        agn_poptop(L);
      }
      luaL_pushresult(&b);
      break;
    }
    default:
      luaL_error(L, "Error in " LUA_QS ": table, set, sequence, register or string expected for arg #2, got %s.", "select",
        luaL_typename(L, 2));
  }  /* end of switch */
  return 1;
}


/* DO NOT DELETE ! Used by `$` operator, based on 3.17.5 `select` version, 3.17.6. This version processes two arguments only,
   and one result only, ignoring all the other results, preventing stack corruption when executed in the VM. */

static int agnB_vmselect (lua_State *L) {  /* changed 7.6.11 */
  agn_select(L, 1, 2);
  return 1;
}


/* 0.10.0; March 30, 2008; extended with sequences on June 30, 2008; extended 0.28.0; fixed December 25, 2009 */
static int luaB_remove (lua_State *L) {
  size_t j;
  int idx, nargs, newstruct, multipass, strict, reshuffle, multret, descend, plain;
  luaL_checktype(L, 1, LUA_TFUNCTION);
  nargs = lua_gettop(L);
  agnB_aux_fcheckoptions(L, &nargs, &newstruct, &multipass, &strict, &reshuffle, &multret, &descend, &plain, "remove");
  if (multret)
    luaL_error(L, "Error in " LUA_QS ": " LUA_QS " option is not supported.", "remove", "multret");
  if (!multipass)
    luaL_error(L, "Error in " LUA_QS ": " LUA_QS " option is not supported.", "remove", "multipass");
  if (!strict)
    luaL_error(L, "Error in " LUA_QS ": " LUA_QS " option is not supported.", "remove", "strict");
  if (descend)
    luaL_error(L, "Error in " LUA_QS ": " LUA_QS " option is not supported.", "remove", "descend");
  switch (lua_type(L, 2)) {
    case LUA_TTABLE: {
      size_t n, c;
      if (!newstruct && reshuffle)
        luaL_error(L, "Error in " LUA_QS ": got invalid combination of " LUA_QS " and " LUA_QS " option.", "remove", "reshuffle", "inplace");
      /* very rough guess whether table is an array or dictionary */
      n = lua_objlen(L, 2);
      c = 1;
      idx = -3;
      luaL_checkstack(L, 2, "not enough stack space");  /* 3.15.1 fix */
      if (newstruct) {  /* changed 2.29.0 */
        aux_createtable(L, 2, n, 2);
      } else {  /* push structure  explicitly, we cannot play with lua_settop(L, 2) unfortunately INPLACE */
        lua_pushvalue(L, 2);
      }
      lua_pushnil(L);
      while (agn_fnext(L, 2, 1, 1)) {
        if (nargs > 2) {
          luaL_checkstack(L, nargs - 2, "not enough stack space");  /* 3.15.1 fix */
          for (j=3; j <= nargs; j++)
            lua_pushvalue(L, j);
        }
        lua_call(L, nargs - 1, 1);  /* call function with nargs-1 argument(s) and one result */
        if (lua_isfalse(L, -1)) {  /* 5.0.1 fix */
          agn_poptop(L);
          if (reshuffle)  /* remap to array */
            lua_rawseti(L, idx, c);  /* table is at -3 */
          else
            lua_rawset2(L, idx);  /* store value in a table, leave key on stack */
          c++;
        } else {  /* delete non-satisfying value from table, 2.29.0 */
          agn_poptoptwo(L);  /* delete result and value, leave key on stack */
          if (!newstruct) {  /* in-place operation: delete non-fitting value from structure */
            lua_pushvalue(L, -1);  /* push key once again */
            lua_pushnil(L);
            lua_rawset(L, idx - 1);    /* deletes key and null */
          }
        }
      }
      agn_tabresize(L, -1, c, !reshuffle);
      break;
    } /* end of case LUA_TTABLE */
    case LUA_TSET: {  /* rewritten 2.31.5 to support in-place operation */
      luaL_checkstack(L, 2 + !newstruct, "not enough stack space");  /* 3.15.1 fix */
      if (!newstruct) lua_pushvalue(L, nargs);
      agn_createset(L, agn_ssize(L, 2)/2);
      luaL_setmetatype(L, 2);
      /* start traversal */
      /* with in-place op, we cannot modify the set that lua_usnext is traversing, so we use a collector help set with
         all the values later to be deleted from the original set */
      lua_pushnil(L);
      while (lua_usnext(L, 2)) {
        luaL_checkstack(L, 2, "not enough stack space");  /* 3.15.1 fix */
        lua_pushvalue(L, 1);      /* push function */
        lua_pushvalue(L, -2);     /* push key (value) */
        if (nargs > 2) {
          luaL_checkstack(L, nargs - 2, "not enough stack space");  /* 3.15.1 fix */
          for (j=3; j <= nargs; j++)
            lua_pushvalue(L, j);
        }
        lua_call(L, nargs - 1, 1);  /* call function with nargs-1 argument(s) and one result */
        if ((newstruct && lua_isfalse(L, -1)) || (!newstruct && lua_istrue(L, -1))) {  /* set key to new or collector set, 5.0.1 fix */
          agn_poptop(L);
          lua_srawset(L, -3);
        } else {
          agn_poptoptwo(L);
        }
      }
      if (!newstruct) {  /* in-place: now delete non-fitting values from original set */
        lua_pushnil(L);
        while (lua_usnext(L, -2)) {
          lua_sdelete(L, -4);
        }
        agn_poptop(L);  /* drop collector set, return original set */
      }
      break;
    } /* end of case LUA_TSET */
    case LUA_TSEQ: {  /* 0.12.0 */
      int i, c, nops;
      nops = agn_seqsize(L, 2);
      idx = -3;
      luaL_checkstack(L, 1, "not enough stack space");  /* 3.15.1 fix */
      if (newstruct) {  /* changed 2.29.0 */
        agn_createseq(L, nops/2);
        luaL_setmetatype(L, 2);
      } else {  /* push structure  explicitly, we cannot play with lua_settop(L, 2) unfortunately INPLACE */
        lua_pushvalue(L, 2);
      }
      /* now traverse sequence */
      c = 0;
      for (i=0; i < nops; i++) {
        luaL_checkstack(L, 2 + (nargs > 2)*(nargs - 2), "not enough stack space");  /* 3.15.1 fix */
        lua_pushvalue(L, 1);  /* push function */
        lua_seqrawgeti(L, 2, i + 1);
        for (j=3; j <= nargs; j++)
          lua_pushvalue(L, j);
        lua_call(L, nargs - 1, 1);  /* call function with nargs-1 argument(s) and one result */
        if (lua_isfalse(L, -1)) {  /* 5.0.1 fix */
          lua_seqrawgeti(L, 2, i + 1);
          lua_seqrawseti(L, idx, ++c);  /* store value to sequence, new sequence is at -3, changed 2.29.0 */
        }
        agn_poptop(L);  /* delete result */
      }
      agn_seqresize(L, -1, c);
      break;
    } /* end of case LUA_TSEQ */
    case LUA_TREG: {  /* 2.3.0 RC 3 */
      int i, c, nops;
      nops = agn_reggettop(L, 2);
      idx = -3;
      luaL_checkstack(L, 1, "not enough stack space");  /* 3.15.1 fix */
      if (newstruct) {  /* changed 2.29.0 */
        agn_createreg(L, nops);  /* 2.29.3 fix */
        luaL_setmetatype(L, 2);
      } else {  /* push structure  explicitly, we cannot play with lua_settop(L, 2) unfortunately INPLACE */
        lua_pushvalue(L, 2);
      }
      /* now traverse register */
      c = 0;
      for (i=0; i < nops; i++) {
        luaL_checkstack(L, 2 + (nargs > 2)*(nargs - 2), "not enough stack space");  /* 3.15.1 fix */
        lua_pushvalue(L, 1);  /* push function */
        agn_regrawgeti(L, 2, i + 1);
        for (j=3; j <= nargs; j++)
          lua_pushvalue(L, j);
        lua_call(L, nargs - 1, 1);  /* call function with nargs-1 argument(s) and one result */
        if (lua_isfalse(L, -1)) {  /* 5.0.1 fix */
          agn_regrawgeti(L, 2, i + 1);
          agn_regrawseti(L, idx, (c++) + 1);  /* store value to new register, register is at -3 or 2 */
        }
        agn_poptop(L);  /* delete result */
      }
      agn_regresize(L, -1, c, 0);
      break;
    }  /* end of case LUA_TREG */
    case LUA_TNIL: {  /* 4.12.1 */
      lua_pushnil(L);
      break;
    }
    case LUA_TSTRING: {  /* 7.3.3/4 extension */
      size_t i, j, l;
      luaL_Buffer b;
      const char *s = lua_tolstring(L, 2, &l);
      luaL_buffinit(L, &b);
      /* now traverse string from left to right */
      for (i=0; i < l; i++) {
        luaL_checkstack(L, 2 + (nargs > 2)*(nargs - 2), "not enough stack space");
        lua_pushvalue(L, 1);  /* push function */
        lua_pushchar(L, uchar(s[i]));  /* push character */
        for (j=3; j <= nargs; j++)
          lua_pushvalue(L, j);
        lua_call(L, nargs - 1, 1);  /* call function with nargs-1 argument(s) and one result */
        if (lua_isfalse(L, -1)) {
          luaL_addchar(&b, uchar(s[i]));
        }
        agn_poptop(L);
      }
      luaL_pushresult(&b);
      break;
    }
    default:
      luaL_error(L, "Error in " LUA_QS ": table, set, sequence, register or string expected for arg #2, got %s.",
        "remove", luaL_typename(L, 2));
  }
  return 1;
}


/* Agena 1.8.9, November 09, 2012 */
static int luaB_selectremove (lua_State *L) {
  int br, j, slots, nargs, newstruct, multipass, strict, reshuffle, multret, descend, plain;
  luaL_checktype(L, 1, LUA_TFUNCTION);
  nargs = lua_gettop(L);
  agnB_aux_fcheckoptions(L, &nargs, &newstruct, &multipass, &strict, &reshuffle, &multret, &descend, &plain, "selectremove");  /* 3.9.0 improvement */
  if (multret)
    luaL_error(L, "Error in " LUA_QS ": " LUA_QS " option is not supported.", "selectremove", "multret");
  if (!multipass)
    luaL_error(L, "Error in " LUA_QS ": " LUA_QS " option is not supported.", "selectremove", "multipass");
  if (!strict)
    luaL_error(L, "Error in " LUA_QS ": " LUA_QS " option is not supported.", "selectremove", "strict");
  if (descend)
    luaL_error(L, "Error in " LUA_QS ": " LUA_QS " option is not supported.", "selectremove", "descend");
  slots = 5 + nargs - 3 + 1;
  luaL_checkstack(L, slots, "not enough stack space");  /* 2.31.2, reserve enough stack space including the return */
  switch(lua_type(L, 2)) {
    case LUA_TTABLE: {
      size_t n, tsize, c, d, idx;
      int asc;
      asc = reshuffle == 1;
      c = d = 0;  /* c: index counter for first resulting table, d: same for second result table */
      /* very rough guess whether table is an array or dictionary */
      n = lua_objlen(L, 2);
      tsize = agn_size(L, 2);
      if (n == 0) {  /* assume table is a dictionary */
        lua_createtable(L, 0, tsize/2); lua_createtable(L, 0, tsize/2);
      } else {
        lua_createtable(L, n/2, 0); lua_createtable(L, n/2, 0);  /* lua_objlen not always returns correct results ! */
      }
      lua_pushnil(L);
      while (agn_fnext(L, 2, 1, 1)) {
        luaL_checkstack(L, slots, "not enough stack space");  /* 3.5.4 fix */
        for (j=3; j <= nargs; j++)
          lua_pushvalue(L, j);
        lua_call(L, nargs - 1, 1);  /* call function with nargs-1 argument(s) and one result */
        br = lua_istrue(L, -1);  /* 5.0.1 fix */
        agn_poptop(L);
        idx = (br) ? ++c : ++d;
        if (asc)
          lua_rawseti(L, -3 - br, idx);
        else
          lua_rawset2(L, -3 - br);  /* store value in a table, leave key on stack */
      }
      /* set type of new table to the one of the original table */
      if (agn_getutype(L, 2)) {
        agn_setutype(L, -2, -1);
        agn_setutype(L, -3, -1);
        agn_poptop(L);  /* pop type (string) */
      }
      /* copy metatable */
      if (lua_getmetatable(L, 2)) {
        lua_pushvalue(L, -1);
        lua_setmetatable(L, -3);
        lua_setmetatable(L, -2);
      }
      agn_tabresize(L, -2, c, !asc);
      agn_tabresize(L, -1, d, !asc);
      break;
    }
    case LUA_TSET: {
      size_t ssize = agn_ssize(L, 2);
      agn_createset(L, ssize/2);
      agn_createset(L, ssize/2);
      /* set its type to the one of the original array */
      if (agn_getutype(L, 2)) {
        agn_setutype(L, -2, -1);
        agn_setutype(L, -3, -1);
        agn_poptop(L);  /* pop type (string) */
      }
      /* copy metatable */
      if (lua_getmetatable(L, 2)) {
        lua_pushvalue(L, -1);
        lua_setmetatable(L, -3);
        lua_setmetatable(L, -2);
      }
      lua_pushnil(L);
      while (lua_usnext(L, 2)) {
        luaL_checkstack(L, slots, "not enough stack space");  /* 3.5.4 fix */
        lua_pushvalue(L, 1);      /* push function; FIXME: can be optimized */
        lua_pushvalue(L, -2);     /* push key (value) */
        for (j=3; j <= nargs; j++)
          lua_pushvalue(L, j);
        lua_call(L, nargs - 1, 1);  /* call function with nargs-1 argument(s) and one result */
        br = lua_istrue(L, -1);
        agn_poptop(L);
        lua_srawset(L, -3 - br);    /* set key to new set */
      }
      break;
    }
    case LUA_TSEQ: {  /* 0.11.1 */
      int i, c[2], nops;
      c[0] = 0;  /* remove structure */
      c[1] = 0;  /* select structure */
      nops = agn_seqsize(L, 2);
      agn_createseq(L, nops/2);
      agn_createseq(L, nops/2);
      /* set its type to the one of the original array */
      if (agn_getutype(L, 2)) {
        agn_setutype(L, -2, -1);
        agn_setutype(L, -3, -1);
        agn_poptop(L);  /* pop type (string) */
      }
      /* copy metatable */
      if (lua_getmetatable(L, 2)) {
        lua_pushvalue(L, -1);
        lua_setmetatable(L, -3);
        lua_setmetatable(L, -2);
      }
      /* now traverse sequence */
      for (i=0; i < nops; i++) {
        luaL_checkstack(L, slots, "not enough stack space");  /* 3.5.4 fix */
        lua_pushvalue(L, 1);  /* push function */
        lua_seqrawgeti(L, 2, i + 1);
        for (j=3; j <= nargs; j++)
          lua_pushvalue(L, j);
        lua_call(L, nargs - 1, 1);  /* call function with nargs-1 argument(s) and one result */
        br = lua_istrue(L, -1);
        lua_seqrawgeti(L, 2, i + 1);
        lua_seqinsert(L, -3 - br);   /* store value to new sequence */
        c[br]++;
        agn_poptop(L);  /* pop boolean */
      }
      agn_seqresize(L, -1, c[0]);  /* new 2.29.0 */
      agn_seqresize(L, -2, c[1]);  /* ditto */
      break;
    } /* end of case LUA_TSEQ */
    case LUA_TREG: {  /* 2.3.0 RC 3 */
      int i, nops, c[2];
      nops = agn_reggettop(L, 2);
      agn_createreg(L, nops);  /* 2.29.3 fix */
      agn_createreg(L, nops);  /* 2.29.3 fix */
      /* copy metatable */
      if (lua_getmetatable(L, 2)) {
        lua_pushvalue(L, -1);
        lua_setmetatable(L, -3);
        lua_setmetatable(L, -2);
      }
      /* set its type to the one of the original array */
      if (agn_getutype(L, 2)) {
        agn_setutype(L, -2, -1);
        agn_setutype(L, -3, -1);
        agn_poptop(L);  /* pop type (string) */
      }
      /* now traverse register */
      c[0] = c[1] = 0;
      for (i=0; i < nops; i++) {
        luaL_checkstack(L, slots, "not enough stack space");  /* 3.5.4 fix */
        lua_pushvalue(L, 1);        /* push function */
        agn_regrawgeti(L, 2, i + 1);   /* push element */
        for (j=3; j <= nargs; j++)
          lua_pushvalue(L, j);
        lua_call(L, nargs - 1, 1);  /* call function with nargs-1 argument(s) and one result */
        br = lua_istrue(L, -1);
        c[br]++;
        agn_regrawgeti(L, 2, i + 1);
        agn_regrawseti(L, -3 - br, c[br]);  /* store value to new register */
        agn_poptop(L);  /* pop boolean */
      }
      /* positive list is at 2, negative list on -1 */
      for (i=0; i < 2; i++)
        agn_regresize(L, -1 - i, c[i], 0);
      break;
    } /* end of case LUA_TREG */
    case LUA_TNIL: {  /* 4.12.1 */
      lua_pushnil(L);
      lua_pushnil(L);  /* 7.3.3 fix */
      break;
    }
    case LUA_TSTRING: {  /* 7.3.3/4 extension */
      size_t i, j, l;
      luaL_Buffer b, c;
      const char *s = lua_tolstring(L, 2, &l);
      luaL_buffinit(L, &b);
      luaL_buffinit(L, &c);
      /* now traverse string from left to right */
      for (i=0; i < l; i++) {
        luaL_checkstack(L, 2 + (nargs > 2)*(nargs - 2), "not enough stack space");
        lua_pushvalue(L, 1);  /* push function */
        lua_pushchar(L, uchar(s[i]));  /* push character */
        for (j=3; j <= nargs; j++)
          lua_pushvalue(L, j);
        lua_call(L, nargs - 1, 1);  /* call function with nargs-1 argument(s) and one result */
        if (lua_istrue(L, -1)) {
          luaL_addchar(&b, uchar(s[i]));
        } else {
          luaL_addchar(&c, uchar(s[i]));
        }
        agn_poptop(L);
      }
      luaL_pushresult(&b);
      luaL_pushresult(&c);
      break;
    }
    default:
      luaL_error(L, "Error in " LUA_QS
        ": table, set, sequence, register or string expected for arg #2, got %s.",
        "selectremove", luaL_typename(L, 2));
  } /* end of switch */
  return 2;
}


/* Used by the `$$` operator, 3.17.6 UNDOC, changed 7.6.11 */
static int agnB_vmcheckfor (lua_State *L) {
  agn_checkfor(L, 1, 2);
  return 1;
}


/* 0.30.3, 18.01.2010 */
static int agnB_countitems (lua_State *L) {
  /* See: https://www.quora.com/How-can-I-save-a-function-to-a-variable-and-call-it-later-via-the-variable */
  typedef int ftype (lua_State *, int, int);
  ftype *fptr;
  int i, j, nargs, nops;
  size_t c = 0;
  luaL_checkany(L, 1);
  nargs = lua_gettop(L);
  if (lua_type(L, 1) == LUA_TFUNCTION) {
    switch (lua_type(L, 2)) {
      case LUA_TTABLE: {
        lua_pushnil(L);
        while (agn_fnext(L, 2, 1, 0)) {  /* push the table key, the function, and the table value */
          if (nargs > 2) {
            luaL_checkstack(L, nargs - 2, "not enough stack space");  /* 3.15.1 fix */
            for (j=3; j <= nargs; j++)
              lua_pushvalue(L, j);
          }
          lua_call(L, nargs - 1, 1);     /* call function with nargs-1 argument(s) and one result */
          if (lua_istrue(L, -1)) c++;    /* 5.0.1 */
          agn_poptop(L);                 /* pop result */
        }
        break;
      }  /* end of case LUA_TTABLE */
      case LUA_TSET: {
        lua_pushnil(L);
        while (lua_usnext(L, 2)) {  /* push item twice */
          luaL_checkstack(L, 2 + (nargs > 2)*(nargs - 2), "not enough stack space");  /* 3.15.1 fix */
          lua_pushvalue(L, 1);      /* push function; FIXME: can be optimized */
          lua_pushvalue(L, -2);     /* push key (value) */
          for (j=3; j <= nargs; j++)
            lua_pushvalue(L, j);
          lua_call(L, nargs - 1, 1);   /* call function with nargs-1 argument(s) and one result */
          if (lua_istrue(L, -1)) c++;  /* check result, 5.0.1 */
          agn_poptoptwo(L);            /* remove result and one item */
        }
        break;
      }  /* end of case LUA_TSET */
      case LUA_TSEQ: {
        nops = agn_seqsize(L, 2);
        /* now traverse array */
        for (i=0; i < nops; i++) {
          luaL_checkstack(L, 2 + (nargs > 2)*(nargs - 2), "not enough stack space");  /* 3.15.1 fix */
          lua_pushvalue(L, 1);          /* push function; FIXME: can be optimized */
          lua_seqrawgeti(L, 2, i + 1);  /* push item */
          for (j=3; j <= nargs; j++)
            lua_pushvalue(L, j);
          lua_call(L, nargs - 1, 1);    /* call function with nargs-1 argument(s) and one result */
          if (lua_istrue(L, -1)) c++;   /* 5.0.1 */
          agn_poptop(L);                /* remove result */
        }
        break;
      }  /* end of case LUA_TSEQ */
      case LUA_TREG: {  /* 2.3.0 RC 3 */
        nops = agn_reggettop(L, 2);
        /* now traverse array */
        for (i=0; i < nops; i++) {
          luaL_checkstack(L, 2 + (nargs > 2)*(nargs - 2), "not enough stack space");  /* 3.15.1 fix */
          lua_pushvalue(L, 1);        /* push function; FIXME: can be optimized */
          agn_regrawgeti(L, 2, i + 1);   /* push item */
          for (j=3; j <= nargs; j++)
            lua_pushvalue(L, j);
          lua_call(L, nargs - 1, 1);  /* call function with nargs-1 argument(s) and one result */
          if (lua_istrue(L, -1)) c++;  /* 5.0.1 */
          agn_poptop(L);              /* remove result */
        }
        break;
      }  /* end of case LUA_TREG */
      case LUA_TNIL:  /* 4.12.2 extension */
        c = 0;
        break;
      default:
        luaL_error(L, "Error in " LUA_QS ": table, set, sequence or register expected as second argument, got %s.",
          "countitems", luaL_typename(L, 2));
    }  /* end of switch */
  } else {
    int nops, rc, number, string, type, approx;
    approx = nargs == 3;
    fptr = (approx) ? lua_rawaequal : lua_equal;  /* 4.2.0 extension */
    number = agn_isnumber(L, 1);
    string = agn_isstring(L, 1);
    type = lua_type(L, 2);
    if (number && type == LUA_TTABLE && !agn_hashashpart(L, 2)) {  /* 4.11.1 boost */
      lua_Number x, y, eps;
      x = agn_tonumber(L, 1);
      nops = luaL_getn(L, 2);
      eps = agn_getepsilon(L);
      for (i=1; i <= nops; i++) {
        y = agn_rawgetinumber(L, 2, i, &rc);
        if (rc) {
          c += (approx) ? tools_approx(x, y, eps) : x == y;
        }
      }
    } else if (number && type == LUA_TSEQ) {
      lua_Number x, y, eps;
      x = agn_tonumber(L, 1);
      nops = agn_seqsize(L, 2);
      eps = agn_getepsilon(L);
      for (i=1; i <= nops; i++) {
        y = agn_seqrawgetinumber(L, 2, i, &rc);
        if (rc) {
          c += (approx) ? tools_approx(x, y, eps) : x == y;
        }
      }
    } else if (number && type == LUA_TREG) {
      lua_Number x, y, eps;
      x = agn_tonumber(L, 1);
      nops = agn_regsize(L, 2);
      eps = agn_getepsilon(L);
      for (i=1; i <= nops; i++) {
        y = agn_regrawgetinumber(L, 2, i, &rc);
        if (rc) {
          c += (approx) ? tools_approx(x, y, eps) : x == y;
        }
      }
    } else if (number && type == LUA_TNIL) {  /* 4.12.2 extension */
      c = 0;
    } else if (string && type == LUA_TTABLE && !agn_hashashpart(L, 2)) {  /* 4.11.1 boost */
      size_t l;
      const char *x, *y;
      x = agn_tostring(L, 1);
      nops = luaL_getn(L, 2);
      for (i=1; i <= nops; i++) {
        y = agn_rawgetilstring(L, 2, i, &l, &rc);
        if (rc) {
          c += tools_streq(x, y);
        }
      }
    } else if (string && type == LUA_TSEQ) {
      size_t l;
      const char *x, *y;
      x = agn_tostring(L, 1);
      nops = agn_seqsize(L, 2);
      for (i=1; i <= nops; i++) {
        y = agn_seqrawgetilstring(L, 2, i, &l, &rc);
        if (rc) {
          c += tools_streq(x, y);
        }
      }
    } else if (string && type == LUA_TREG) {
      size_t l;
      const char *x, *y;
      x = agn_tostring(L, 1);
      nops = agn_regsize(L, 2);
      for (i=1; i <= nops; i++) {
        y = agn_regrawgetilstring(L, 2, i, &l, &rc);
        if (rc) {
          c += tools_streq(x, y);
        }
      }
    } else if (string && type == LUA_TNIL) {  /* 4.12.2 extension */
      c = 0;
    } else {
      switch (lua_type(L, 2)) {
        case LUA_TTABLE: {
          lua_pushnil(L);
          while (lua_next(L, 2)) {  /* push the table key and the table value */
            if (fptr(L, -1, 1)) c++;
            agn_poptop(L);  /* pop value */
          }
          break;
        }
        case LUA_TSET: {
          lua_pushnil(L);
          while (lua_usnext(L, 2)) {  /* push item twice */
            if (fptr(L, -1, 1)) c++;
            agn_poptop(L);  /* remove one item */
          }
          break;
        }
        case LUA_TSEQ: {
          nops = agn_seqsize(L, 2);
          for (i=0; i < nops; i++) {
            lua_seqrawgeti(L, 2, i + 1);   /* push item */
            if (fptr(L, -1, 1)) c++;
            agn_poptop(L);  /* remove result */
          }
          break;
        }
        case LUA_TREG: {  /* 2.3.0 RC 3 */
          nops = agn_reggettop(L, 2);
          for (i=0; i < nops; i++) {
            agn_regrawgeti(L, 2, i + 1);   /* push item */
            if (fptr(L, -1, 1)) c++;
            agn_poptop(L);  /* remove result */
          }
          break;
        }
        case LUA_TNIL: {  /* 4.12.2 extension */
          c = 0;
          break;
        }
        default:
          luaL_error(L, "Error in " LUA_QS ": table, set, sequence or register expected for argument #2, got %s.",
            "countitems", luaL_typename(L, 2));
      }
    }  /* end of switch */
  }
  lua_pushinteger(L, c);
  return 1;
}


/* Used by the `$$$` operator, 7.6.11 */
static int agnB_vmcount (lua_State *L) {
  agn_count(L, 1, 2);
  return 1;
}


/* Empties a table, set or register and returns the emptied structure. With a register, sets all its places to `null` and returns
   the modified register. With tables and sequences, the memory previously occupied can be reused for other purposes. 2.33.2 */
static int luaB_cleanse (lua_State *L) {
  luaL_checkany(L, 1);
  switch (lua_type(L, 1)) {
    case LUA_TTABLE: {
      agn_cleanse(L, 1, 0);
      lua_settop(L, 1);
      break;
    }
    case LUA_TSET: {
      agn_cleanseset(L, 1, 0);
      break;
    }
    case LUA_TSEQ: {
      agn_seqresize(L, 1, 0);
      break;
    }
    case LUA_TREG: {
      int i, nops;
      /* now traverse register */
      nops = agn_reggettop(L, 1);
      for (i=0; i < nops; i++) {
        lua_pushnil(L);
        agn_regrawseti(L, 1, i + 1);
      }
      break;
    }
    default:
      luaL_error(L, "Error in " LUA_QS ": table, set, sequence or register expected for argument #1, got %s.",
        "cleanse", luaL_typename(L, 1));
  }
  lua_settop(L, 1);
  lua_gc(L, LUA_GCCOLLECT, 0);
  return 1;
}


/* Applies a function f on each item of a structure or string obj and returns one accumulating result. f must have exactly two parameters x, a,
   where x will represent the respective item in obj, and a the accumulator to be updated. If init is given, then the accumulator is initialised
   with it, otherwise the accumulator is set to zero at first. After traversal of obj, the accumulator is returned. The function is equivalent to:

   reduce := proc(f, s, start, ?) is
      local accumulator := start or 0;
      if checkoptions('reduce', [varargs[nargs - 3]], _c=boolean, true)._c then  # _c = true option given ?
         varargs[nargs - 3] := 1;   # replace option with initial value for the counter
         for item in s do
            accumulator := f(item, accumulator, unpack(varargs));
            inc varargs[nargs - 3]  # increment counter
         od
      else
         for item in s do
            accumulator := f(item, accumulator, unpack(varargs))
         od
      fi;
      return accumulator
   end;

   For example, reduce(<< x, a -> x + a >>, [1, 2, 3, 4]) computes the sum of the numbers in a table, i.e. 10; and
   reduce(<< x, a -> a & x & '|' >>, '1234', '') appends a pipe to each character, i.e. returns '1|2|3|4|'. */
void aux_savevar (lua_State *L, const char *varname, int init, void *k, int *usecounter) {  /* 2.12.1 */
  agn_rawgetfield(L, LUA_GLOBALSINDEX, varname);  /* check whether global varname already exists */
  if (!lua_isnil(L, -1)) {  /* save its value to registry */
    lua_pushlightuserdata(L, k);  /* 2.12.3 fix, better use static const char than *const char,
    see: https://www.lua.org/pil/27.3.1.html:
    "A bulletproof method is to use as key the address of a static variable in your code: The C link editor
    ensures that this key is unique among all libraries." */
    lua_pushvalue(L, -2);
    lua_rawset(L, LUA_REGISTRYINDEX);
    (*usecounter)++;
  }
  agn_poptop(L);  /* drop var or null */
  lua_pushinteger(L, init);  /* anchor initial counter value into module's environment */
  agn_rawsetfield(L, LUA_ENVIRONINDEX, varname);
  (*usecounter)++;
}

void aux_restorevar (lua_State *L, const char *varname, void *k, int usecounter) {  /* 2.12.1 */
  lua_pushnil(L);  /* delete counter from module's environment */
  agn_rawsetfield(L, LUA_ENVIRONINDEX, varname);
  if (usecounter == 3) {  /* we must restore original var, 2.20.3 fix */
    lua_pushlightuserdata(L, k);
    lua_gettable(L, LUA_REGISTRYINDEX);             /* get registry entry */
    agn_rawsetfield(L, LUA_GLOBALSINDEX, varname);  /* put it into global environment, stack is leveled now */
    lua_pushlightuserdata(L, k);                    /* delete registry entry, 2.12.3 fix */
    lua_pushnil(L);
    lua_rawset(L, LUA_REGISTRYINDEX);
  }
}

void aux_rf_getoptions (lua_State *L, int *nargs, int *usecounter, int *usenewcounter, int *fold, const char *pn) {  /* 2.36.2 */
  int noptions = 2;
  *usecounter = *usenewcounter = *fold = 0;
  if (*nargs != 0 && lua_ispair(L, *nargs))  /* 3.15.2 fix */
    luaL_checkstack(L, 2, "not enough stack space");
  while (noptions-- && lua_ispair(L, *nargs)) {  /* with more options, just exchange `if' with `while', 2.12.1 */
    const char *option;
    agn_pairgeti(L, *nargs, 1);
    if (!agn_isstring(L, -1)) {
      agn_poptop(L);
      luaL_error(L, "Error in " LUA_QS ": left-hand side of option must be a string.", pn);
    }
    option = lua_tostring(L, -1);
    if (tools_streq(option, "counter") || tools_streq(option, "_c")) {  /* 2.16.12 tweak */
      agn_pairgeti(L, *nargs, 2);
      if (lua_isboolean(L, -1)) {
        if (tools_streq(option, "_c"))
          *usecounter = agn_istrue(L, -1);
        else  /* 2.20.3, the last argument of the given function will be assigned a counter value starting from 1 and step 1 */
          *usenewcounter = agn_istrue(L, -1);
        agn_poptop(L);
      } else  {
        agn_poptoptwo(L);
        luaL_error(L, "Error in " LUA_QS ": right-hand side of counter or _c option must be a boolean.", pn);
      }
    } else if (tools_streq(option, "fold")) {  /* 2.36.2 */
      agn_pairgeti(L, *nargs, 2);
      *fold = lua_istrue(L, -1);
      agn_poptop(L);
    } else {
      agn_poptop(L);
      luaL_error(L, "Error in " LUA_QS ": unknown option.", pn);
    }
    agn_poptop(L);  /* pop lhs */
    (*nargs)--;
  }
}

void aux_rf_accuinit (lua_State *L, int *nargs, size_t *c, int fold, int usecounter, const char *pn) {  /* 2.36.2 */
  /* accumulator initialisation, third argument */
  if (!fold) {
    if (*nargs < 3) {
      lua_pushnumber(L, 0); (*nargs)++;
    } else {
      lua_pushvalue(L, 3);
    }
  } else {  /* 2.36.2, folding: take the first value in the structure or the first character in a string and initialise the accumulator with it */
    switch (lua_type(L, 2)) {
      case LUA_TTABLE: {
        lua_pushnil(L);
        if (lua_next(L, 2)) {
          lua_remove(L, -2);
        }
        break;
      }  /* end of case LUA_TTABLE */
      case LUA_TSET: {
        lua_pushnil(L);
        if (lua_usnext(L, 2)) {
          agn_poptop(L);
        }
        break;
      }  /* end of case LUA_TSET */
      case LUA_TPAIR: {
        agn_pairgeti(L, 2, 1);
        break;
      }  /* end of case LUA_TPAIR */
      case LUA_TSEQ: {
        if (agn_seqsize(L, 2) > 0) {
          lua_seqrawgeti(L, 2, 1);
        }
        break;
      }  /* end of case LUA_TSEQ */
      case LUA_TREG: {
        if (agn_reggettop(L, 2) > 0) {
          agn_regrawgeti(L, 2, 1);
        }
        break;
      }  /* end of case LUA_TREQ */
      case LUA_TSTRING: {
        const char *str = agn_checkstring(L, 2);
        lua_pushlstring(L, str[0] != '\0' ? str : "", 1);  /* \000 represents an embedded zero in Agena */
        break;
      }  /* end of case LUA_TSTRING */
      default:
        luaL_error(L, "Error in " LUA_QS ": structure or string expected for argument #2, got %s.", pn,
          luaL_typename(L, 2));
    }  /* end of switch */
    if (usecounter) (*c)++;  /* increment counter once by one */
  }
}

void aux_rf_main (lua_State *L, int nargs, size_t c, int fold, int usecounter, int usenewcounter, const char *pn) {
  int flag, offset, i;
  size_t slots;
  flag = 1;
  offset = nargs - 3 + fold;
  slots = 5 + nargs - 4 + 1;
  luaL_checkstack(L, slots, "not enough stack space");  /* 2.31.2, reserve enough stack space including the return */
  switch (lua_type(L, 2)) {
    case LUA_TTABLE: {
      lua_pushnil(L);
      while (agn_fnext(L, 2, 1, 0)) {
        luaL_checkstack(L, slots, "not enough stack space");
        if (fold && flag) { lua_pop(L, 2); flag = 0; continue; }  /* pop first table value & function, leave key on stack for immediate next iteration */
        lua_pushvalue(L, -4);  /* get current accumulator */
        for (i=4 - fold; i <= nargs; i++) lua_pushvalue(L, i);  /* push extra arguments */
        if (usenewcounter) lua_pushinteger(L, c++);  /* push internal counter, assigned to the last argument of the given function */
        lua_call(L, 2 + offset + usenewcounter, 1);  /* call function */
        lua_replace(L, -3);  /* update accumulator, pops new accumulator value */
        if (usecounter) {  /* 2.12.1 */
          lua_pushinteger(L, c++);
          agn_rawsetfield(L, LUA_ENVIRONINDEX, "_c");
        }
      }
      break;
    }  /* end of case LUA_TTABLE */
    case LUA_TSET: {
      lua_pushnil(L);
      while (lua_usnext(L, 2)) {
        luaL_checkstack(L, slots, "not enough stack space");
        if (fold && flag) { lua_pop(L, 1); flag = 0; continue; }  /* pop first table value & function, leave key on stack for immediate next iteration */
        lua_pushvalue(L, 1);   /* push function; FIXME: can be optimised */
        lua_pushvalue(L, -2);  /* push key (value) */
        lua_pushvalue(L, -5);  /* get current accumulator */
        for (i=4 - fold; i <= nargs; i++) lua_pushvalue(L, i);  /* push extra arguments */
        if (usenewcounter) lua_pushinteger(L, c++);  /* push internal counter, assigned to the last argument of the given function */
        lua_call(L, 2 + offset + usenewcounter, 1);  /* call function */
        lua_replace(L, -3);    /* update accumulator, pops new accumulator value */
        if (usecounter) {  /* 2.12.1 */
          lua_pushinteger(L, c++);
          agn_rawsetfield(L, LUA_ENVIRONINDEX, "_c");
        }
      }
      break;
    }  /* end of case LUA_TSET */
    case LUA_TPAIR: {
      int i, j;  /* index */
      for (i=1 + fold; i < 3; i++) {
        luaL_checkstack(L, slots, "not enough stack space");
        lua_pushvalue(L, 1);   /* push function */
        agn_pairgeti(L, 2, i);
        lua_pushvalue(L, -3);  /* get current accumulator */
        for (j=4 - fold; j <= nargs; j++) lua_pushvalue(L, j);  /* push extra arguments */
        if (usenewcounter) lua_pushinteger(L, c++);  /* push internal counter, assigned to the last argument of the given function */
        lua_call(L, 2 + offset + usenewcounter, 1);  /* call function */
        lua_replace(L, -2);    /* update accumulator, pops new accumulator value */
        if (usecounter) {  /* 2.12.1 */
          lua_pushinteger(L, c++);
          agn_rawsetfield(L, LUA_ENVIRONINDEX, "_c");
        }
      }
      break;
    }  /* end of case LUA_TPAIR */
    case LUA_TSEQ: {
      int i, j, nops;  /* index */
      nops = agn_seqsize(L, 2);
      for (i=0 + fold; i < nops; i++) {
        luaL_checkstack(L, slots, "not enough stack space");
        lua_pushvalue(L, 1);   /* push function */
        lua_seqrawgeti(L, 2, i + 1);
        lua_pushvalue(L, -3);  /* get current accumulator */
        for (j=4 - fold; j <= nargs; j++) lua_pushvalue(L, j);  /* push extra arguments */
        if (usenewcounter) lua_pushinteger(L, c++);  /* push internal counter, assigned to the last argument of the given function */
        lua_call(L, 2 + offset + usenewcounter, 1);  /* call function */
        lua_replace(L, -2);    /* update accumulator, pops new accumulator value */
        if (usecounter) {  /* 2.12.1 */
          lua_pushinteger(L, c++);
          agn_rawsetfield(L, LUA_ENVIRONINDEX, "_c");
        }
      }
      break;
    }  /* end of case LUA_TSEQ */
    case LUA_TREG: {
      int i, j, nops;  /* index */
      nops = agn_reggettop(L, 2);
      for (i=0 + fold; i < nops; i++) {
        luaL_checkstack(L, slots, "not enough stack space");
        lua_pushvalue(L, 1);   /* push function */
        agn_regrawgeti(L, 2, i + 1);
        lua_pushvalue(L, -3);  /* get current accumulator */
        for (j=4 - fold; j <= nargs; j++) lua_pushvalue(L, j);  /* push extra arguments */
        if (usenewcounter) lua_pushinteger(L, c++);  /* push internal counter, assigned to the last argument of the given function */
        lua_call(L, 2 + offset + usenewcounter, 1);  /* call function */
        lua_replace(L, -2);    /* update accumulator, pops new accumulator value */
        if (usecounter) {  /* 2.12.1 */
          lua_pushinteger(L, c++);
          agn_rawsetfield(L, LUA_ENVIRONINDEX, "_c");
        }
      }
      break;
    }  /* end of case LUA_TREQ */
    case LUA_TSTRING: {
      size_t l;
      const char *str = agn_checklstring(L, 2, &l);
      str += fold;
      while (*(str)) {
        luaL_checkstack(L, slots, "not enough stack space");
        lua_pushvalue(L, 1);   /* push function */
        lua_pushlstring(L, str++, 1);
        lua_pushvalue(L, -3);  /* get current accumulator */
        for (i=4-fold; i <= nargs; i++) lua_pushvalue(L, i);  /* push extra arguments */
        if (usenewcounter) lua_pushinteger(L, c++);  /* push internal counter, assigned to the last argument of the given function */
        lua_call(L, 2 + offset + usenewcounter, 1);  /* call function */
        lua_replace(L, -2);    /* update accumulator, pops new accumulator value */
        if (usecounter) {  /* 2.12.1 */
          lua_pushinteger(L, c++);
          agn_rawsetfield(L, LUA_ENVIRONINDEX, "_c");
        }
      }
      break;
    }  /* end of case LUA_TSTRING */
    default:
      luaL_error(L, "Error in " LUA_QS ": structure or string expected for argument #2, got %s.", pn,
        luaL_typename(L, 2));
  }  /* end of switch */
}

static int agnB_reduce (lua_State *L) {  /* Agena 2.12.0 RC 1, based on agnB_map, 35 percent faster than an implementation in Agena */
  int nargs, usecounter, usenewcounter, fold;
  size_t c = 1;
  static const char key = 'k';
  luaL_checktype(L, 1, LUA_TFUNCTION);
  nargs = lua_gettop(L);
  luaL_checkstack(L, nargs, "too many arguments");  /* 2.14.0/2.31.1/2, reserve enough stack space including the return */
  aux_rf_getoptions(L, &nargs, &usecounter, &usenewcounter, &fold, "reduce");
  aux_rf_accuinit(L, &nargs, &c, fold, usecounter, "reduce");
  /* the accumulator is now at the top of the stack */
  /* _c counter initialisation, if _c=true option given */
  if (usecounter) {  /* we create a counter named `_c' that is visible only in the function (first argument), 2.12.1 */
    /* if variable _c should already exist in the environment, then save a copy of it to Lua's registry and restore it at exit; due
       to unknown reasons, it is not possible to create an independent environment for the function (first argument) by copying _G
       (maybe due to its own circumreference. */
    aux_savevar(L, "_c", c++, (void *)&key, &usecounter);
  }
  aux_rf_main(L, nargs, c, fold, usecounter, usenewcounter, "reduce");
  if (usecounter)
    aux_restorevar(L, "_c", (void *)&key, usecounter);
  return 1;  /* return accumulator */
}


static int agnB_fold (lua_State *L) {  /* 2.36.2, like reduce with the fold=true option */
  int nargs, usecounter, usenewcounter, fold;
  size_t c = 1;
  static const char key = 'k';
  luaL_checktype(L, 1, LUA_TFUNCTION);
  nargs = lua_gettop(L);
  luaL_checkstack(L, nargs, "too many arguments");  /* reserve enough stack space including the return */
  aux_rf_getoptions(L, &nargs, &usecounter, &usenewcounter, &fold, "fold");
  fold = 1;
  aux_rf_accuinit(L, &nargs, &c, fold, usecounter, "fold");
  /* the accumulator is now at the top of the stack */
  /* _c counter initialisation, if _c=true option given */
  if (usecounter) {  /* we create a counter named `_c' that is visible only in the function (first argument) */
    /* if variable _c should already exist in the environment, then save a copy of it to Lua's registry and restore it at exit; due
       to unknown reasons, it is not possible to create an independent environment for the function (first argument) by copying _G
       (maybe due to its own circumreference. */
    aux_savevar(L, "_c", c++, (void *)&key, &usecounter);
  }
  aux_rf_main(L, nargs, c, fold, usecounter, usenewcounter, "fold");
  if (usecounter)
    aux_restorevar(L, "_c", (void *)&key, usecounter);
  return 1;  /* return accumulator */
}


/* convert a string to a table, needed by phonetiqs.config;
   extended to general function converting structures to tables 0.12.2, November 10, 2008 */

static int agnB_totable (lua_State *L) {
  size_t i, l;
  switch (lua_type(L, 1)) {
    case LUA_TSTRING: {
      const char *str = lua_tolstring(L, 1, &l);
      luaL_checkstack(L, 1, "not enough stack space");  /* 3.17.5 fix */
      lua_createtable(L, l, 0);
      for (i=1; i < l + 1; i++) {
        lua_rawsetilstring(L, -1, i, str++, 1);
      }
      break;
    }
    case LUA_TSET: {
      luaL_checkstack(L, 2, "not enough stack space");  /* 3.17.5 fix */
      lua_createtable(L, agn_ssize(L, 1), 0);
      i = 0;
      lua_pushnil(L);
      while (lua_usnext(L, 1)) {
        i++;
        lua_rawseti(L, -3, i);
      }
      break;
    }
    case LUA_TSEQ: {
      size_t nops;
      nops = agn_seqsize(L, 1) + 1;
      luaL_checkstack(L, 2, "not enough stack space");  /* 3.17.5 fix */
      lua_createtable(L, nops - 1, 0);
      for (i=1; i < nops; i++) {
        lua_seqrawgeti(L, 1, i);
        lua_rawseti(L, -2, i);
      }
      break;
    }
    case LUA_TREG: {  /* 2.3.0 RC 3 */
      size_t nops;
      nops = agn_reggettop(L, 1) + 1;
      luaL_checkstack(L, 2, "not enough stack space");  /* 3.17.5 fix */
      lua_createtable(L, nops - 1, 0);
      for (i=1; i < nops; i++) {
        agn_regrawgeti(L, 1, i);
        lua_rawseti(L, -2, i);
      }
      break;
    }
    default: luaL_error(L, "Error in " LUA_QS ": set, sequence, register or string expected, got %s.", "totable",
      luaL_typename(L, 1));
  }
  return 1;
}


static int agnB_toseq (lua_State *L) {
  size_t i, l;
  switch (lua_type(L, 1)) {
    case LUA_TSTRING: {
      const char *str = lua_tolstring(L, 1, &l);
      luaL_checkstack(L, 2, "not enough stack space");  /* 3.17.5 fix */
      agn_createseq(L, l);
      for (i=1; i < l + 1; i++) {
        lua_pushlstring(L, str++, 1);
        lua_seqrawseti(L, -2, i);
      }
      break;
    }
    case LUA_TTABLE: {
      i = 0;
      luaL_checkstack(L, 2, "not enough stack space");  /* 3.17.5 fix */
      agn_createseq(L, agn_size(L, 1));
      lua_pushnil(L);
      while (lua_next(L, 1) != 0) {
        lua_seqrawseti(L, -3, ++i);
      }
      break;
    }
    case LUA_TSET: {
      i = 0;
      luaL_checkstack(L, 2, "not enough stack space");  /* 3.17.5 fix */
      agn_createseq(L, agn_ssize(L, 1));
      lua_pushnil(L);
      while (lua_usnext(L, 1)) {
        lua_seqrawseti(L, -3, ++i);
      }
      break;
    }
    case LUA_TREG: {  /* 2.3.0 RC 3 */
      i = 0;
      l = agn_reggettop(L, 1);
      luaL_checkstack(L, 2, "not enough stack space");  /* 3.17.5 fix */
      agn_createseq(L, l);
      for (i=0; i < l; i++) {
        agn_regrawgeti(L, 1, i + 1);
        lua_seqrawseti(L, -2, i + 1);
      }
      break;
    }
    default: luaL_error(L, "Error in " LUA_QS ": table, set, register or string expected, got %s.", "toseq",
      luaL_typename(L, 1));
  }
  return 1;
}


static int agnB_toset (lua_State *L) {  /* 0.27.2 */
  size_t i, l;
  switch (lua_type(L, 1)) {
    case LUA_TSTRING: {
      const char *str = lua_tolstring(L, 1, &l);
      agn_createset(L, l);
      for (i=1; i < l + 1; i++) {
        lua_srawsetlstring(L, -1, str++, 1);
      }
      break;
    }
    case LUA_TTABLE: case LUA_TSEQ: case LUA_TREG: {
      agn_toset(L, 1);  /* 2.30.2 tuning by 20 percent; grep "TEMPLATE to call the VM via the API" */
      break;
    }
    default: luaL_error(L, "Error in " LUA_QS ": table, sequence, register or string expected, got %s.", "toset",
      luaL_typename(L, 1));
  }
  return 1;
}


static int agnB_toreg (lua_State *L) {  /* 2.3.0 RC 3 */
  size_t i, l;
  switch (lua_type(L, 1)) {
    case LUA_TSTRING: {
      const char *str = lua_tolstring(L, 1, &l);
      luaL_checkstack(L, 2, "not enough stack space");  /* 3.17.5 fix */
      agn_createreg(L, l);
      for (i=1; i < l + 1; i++) {
        lua_pushlstring(L, str++, 1);
        agn_regrawseti(L, -2, i);
      }
      break;
    }
    case LUA_TTABLE: {
      i = 0;
      luaL_checkstack(L, 2, "not enough stack space");  /* 3.17.5 fix */
      agn_createreg(L, agn_size(L, 1));
      lua_pushnil(L);
      while (lua_next(L, 1) != 0) {
        agn_regrawseti(L, -3, ++i);
      }
      break;
    }
    case LUA_TSET: {
      i = 0;
      luaL_checkstack(L, 2, "not enough stack space");  /* 3.17.5 fix */
      agn_createreg(L, agn_ssize(L, 1));
      lua_pushnil(L);
      while (lua_usnext(L, 1)) {
        agn_regrawseti(L, -3, ++i);
      }
      break;
    }
    case LUA_TSEQ: {
      size_t nops;
      nops = agn_seqsize(L, 1) + 1;
      luaL_checkstack(L, 2, "not enough stack space");  /* 3.17.5 fix */
      agn_createreg(L, nops - 1);
      for (i=1; i < nops; i++) {
        lua_seqrawgeti(L, 1, i);
        agn_regrawseti(L, -2, i);
      }
      break;
    }
    default: luaL_error(L, "Error in " LUA_QS ": table, set, sequence or string expected, got %s.", "toreg",
      luaL_typename(L, 1));
  }
  return 1;
}


/* Lua's luaB_select */
static int luaB_ops (lua_State *L) {
  int n, c, type, i, j, nops;
  n = lua_gettop(L);
  type = lua_type(L, 1);
  if (type == LUA_TSTRING && *lua_tostring(L, 1) == '#') {
    lua_pushinteger(L, n - 1);
    return 1;
  }
  c = 0;
  switch (type) {
    case LUA_TNUMBER: {
      i = agnL_checkint(L, 1);
      if (i < 0) i = n + i;
      else if (i > n) i = n;
      luaL_argcheck(L, 1 <= i, 1, "index out of range");
      c = n - i;
      break;
    }
    case LUA_TTABLE: {  /* 3.13.3 */
      int rc;
      nops = agn_asize(L, 1);
      for (j=0; j < nops; j++) {
        i = agn_rawgetinumber(L, 1, j + 1, &rc);  /* get index position */
        if (rc == 0) continue;  /* conversion failed ? -> do nothing and ignore */
        if (i < 0) i = n + i;
        else if (i > n) i = n;
        if (i > 0 && i < n) {  /* ignore invalid indices */
          luaL_checkstack(L, 1, "too many operands");
          lua_pushvalue(L, i + 1);  /* push value on the stack */
          c++;
        }
      }
      break;
    }
    case LUA_TSEQ: {  /* Agena 1.2 */
      nops = agn_seqsize(L, 1);
      for (j=0; j < nops; j++) {
        i = lua_seqrawgetinumber(L, 1, j + 1);  /* get index position */
        if (i == HUGE_VAL) continue;  /* conversion failed ? -> do nothing and ignore */
        if (i < 0) i = n + i;
        else if (i > n) i = n;
        if (i > 0 && i < n) {  /* ignore invalid indices */
          luaL_checkstack(L, 1, "too many operands");  /* 3.13.3 change from c++ to just 1 */
          lua_pushvalue(L, i + 1);  /* push value on the stack */
          c++;
        }
      }
      break;
    }
    case LUA_TREG: {  /* 3.13.3 */
      nops = agn_regsize(L, 1);
      for (j=0; j < nops; j++) {
        i = agn_reggetinumber(L, 1, j + 1);  /* get index position */
        if (i == HUGE_VAL) continue;  /* conversion failed ? -> do nothing and ignore */
        if (i < 0) i = n + i;
        else if (i > n) i = n;
        if (i > 0 && i < n) {  /* ignore invalid indices */
          luaL_checkstack(L, 1, "too many operands");
          lua_pushvalue(L, i + 1);  /* push value on the stack */
          c++;
        }
      }
      break;
    }
    default:
      luaL_error(L, "Error in " LUA_QS ": number, string, table, sequence or register expected, got %s.", "ops",
        luaL_typename(L, 1));
  }
  return c;
}


static int luaB_settype (lua_State *L) {  /* added 0.11.0, extended 0.12.0, 0.12.2, 0.14.0, extended 2.3.0 RC 3 */
  int i, s, t, nargs;
  nargs = lua_gettop(L);
  if (nargs < 2)
    luaL_error(L, "Error in " LUA_QS ": at least two arguments expected, got %d.", "settype", nargs);
  t = lua_type(L, nargs);
  luaL_typecheck(L, t == LUA_TSTRING || t == LUA_TNIL, nargs, "string or null expected", t);
  for (i=1; i < nargs; i++) {
    s = lua_type(L, i);
    luaL_typecheck(L, s >= LUA_TTABLE || s == LUA_TFUNCTION || s == LUA_TUSERDATA,
      i, "procedure, structure or userdata expected", s);
    agn_setutype(L, i, nargs);
  }
  if (nargs == 2)  /* 2.75.6 extension */
    lua_pushvalue(L, 1);
  else
    lua_pushnil(L);
  return 1;
}


static int luaB_gettype (lua_State *L) {  /* added 0.11.0, changed 0.12.2; extended 0.14.0, extended 2.3.0 RC 3 */
  int s = lua_type(L, 1);
  if (!(s >= LUA_TTABLE || s == LUA_TFUNCTION || s == LUA_TUSERDATA) || !agn_getutype(L, 1))
    lua_pushnil(L);
  return 1;
}


/*
** {======================================================
** Quicksort
** (based on `Algorithms in MODULA-3', Robert Sedgewick;
**  Addison-Wesley, 1993.)
*/

#define set2tbl(L,i,j) { \
  lua_rawseti(L, 1, (i)); \
  lua_rawseti(L, 1, (j)); \
}

#define set2seq(L,i,j) { \
  lua_seqrawseti(L, 1, (i)); \
  lua_seqrawseti(L, 1, (j)); \
}

#define set2reg(L,i,j) { \
  agn_regrawseti(L, 1, (i)); \
  agn_regrawseti(L, 1, (j)); \
}

#define luaBset3(L,idx,i,j) { \
  lua_rawseti(L, (idx), (i)); \
  lua_rawseti(L, (idx + 1), (j)); \
}

#define luaBsetseq3(L,idx,i,j) { \
  lua_seqrawseti(L, (idx), (i)); \
  lua_seqrawseti(L, (idx + 1), (j)); \
}

#define luaBsetreg3(L,idx,i,j) { \
  agn_regrawseti(L, (idx), (i)); \
  agn_regrawseti(L, (idx + 1), (j)); \
}

/*
** Return true iff value at stack index 'a' is less than the value at
** index 'b' (according to the order of the sort).
*/
static int luaBsort_comp (lua_State *L, int a, int b) {
  if (lua_isnil(L, 2))  /* no function? */
    /* return lua_compare(L, a, b, LUA_OPLT); */ /* a < b */
    return lua_lessthan(L, a, b);
  else {  /* function */
    int res;
    lua_pushvalue(L, 2);      /* push function */
    lua_pushvalue(L, a - 1);  /* -1 to compensate function */
    lua_pushvalue(L, b - 2);  /* -2 to compensate function and 'a' */
    lua_call(L, 2, 1);        /* call function */
    res = lua_toboolean(L, -1);  /* get result */
    agn_poptop(L);            /* pop result */
    return res;
  }
}

/* In-place table sort taken from Lua 5.4.6 */
/*
** {======================================================
** Quicksort
** (based on 'Algorithms in MODULA-3', Robert Sedgewick;
**  Addison-Wesley, 1993.)
** =======================================================
*/

/* type for array indices */
typedef unsigned int IdxT;

/*
** Produce a "random" 'unsigned int' to randomize pivot choice. This
** macro is used only when 'sort' detects a big imbalance in the result
** of a partition. (If you don't want/need this "randomness", ~0 is a
** good choice.)
*/
#if !defined(l_randomizePivot)		/* { */

#include <time.h>

/* size of 'e' measured in number of 'unsigned int's */
#define sof(e)		(sizeof(e)/sizeof(unsigned int))

/*
** Use 'time' and 'clock' as sources of "randomness". Because we don't
** know the types 'clock_t' and 'time_t', we cannot cast them to
** anything without risking overflows. A safe way to use their values
** is to copy them to an array of a known type and use the array values.
*/
static unsigned int l_randomizePivot (void) {
  clock_t c = clock();
  time_t t = time(NULL);
  unsigned int buff[sof(c) + sof(t)];
  unsigned int i, rnd = 0;
  memcpy(buff, &c, sof(c)*sizeof(unsigned int));
  memcpy(buff + sof(c), &t, sof(t)*sizeof(unsigned int));
  for (i = 0; i < sof(buff); i++)
    rnd += buff[i];
  return rnd;
}

#endif					/* } */

/* arrays larger than 'RANLIMIT' may use randomized pivots */
#define RANLIMIT	100u

/*
** Does the partition: Pivot P is at the top of the stack.
** precondition: a[lo] <= P == a[up-1] <= a[up],
** so it only needs to do the partition from lo + 1 to up - 2.
** Pos-condition: a[lo .. i - 1] <= a[i] == P <= a[i + 1 .. up]
** returns 'i'.
*/
static IdxT partitiontable (lua_State *L, IdxT lo, IdxT up) {
  IdxT i = lo;  /* will be incremented before first use */
  IdxT j = up - 1;  /* will be decremented before first use */
  /* loop invariant: a[lo .. i] <= P <= a[j .. up] */
  for (;;) {
    /* next loop: repeat ++i while a[i] < P */
    while ((void)lua_rawgeti(L, 1, ++i), luaBsort_comp(L, -1, -2)) {
      if (l_unlikely(i == up - 1))  /* a[i] < P  but a[up - 1] == P  ?? */
        luaL_error(L, "invalid order function for sorting");
      agn_poptop(L);  /* remove a[i] */
    }
    /* after the loop, a[i] >= P and a[lo .. i - 1] < P */
    /* next loop: repeat --j while P < a[j] */
    while ((void)lua_rawgeti(L, 1, --j), luaBsort_comp(L, -3, -1)) {
      if (l_unlikely(j < i))  /* j < i  but  a[j] > P ?? */
        luaL_error(L, "invalid order function for sorting");
      agn_poptop(L);  /* remove a[j] */
    }
    /* after the loop, a[j] <= P and a[j + 1 .. up] >= P */
    if (j < i) {  /* no elements out of place? */
      /* a[lo .. i - 1] <= P <= a[j + 1 .. i .. up] */
      agn_poptop(L);  /* pop a[j] */
      /* swap pivot (a[up - 1]) with a[i] to satisfy pos-condition */
      set2tbl(L, up - 1, i);
      return i;
    }
    /* otherwise, swap a[i] - a[j] to restore invariant and repeat */
    set2tbl(L, i, j);
  }
}

static IdxT partitionseq (lua_State *L, IdxT lo, IdxT up) {
  IdxT i = lo;  /* will be incremented before first use */
  IdxT j = up - 1;  /* will be decremented before first use */
  /* loop invariant: a[lo .. i] <= P <= a[j .. up] */
  for (;;) {
    /* next loop: repeat ++i while a[i] < P */
    while ((void)lua_seqrawgeti(L, 1, ++i), luaBsort_comp(L, -1, -2)) {
      if (l_unlikely(i == up - 1))  /* a[i] < P  but a[up - 1] == P  ?? */
        luaL_error(L, "invalid order function for sorting");
      agn_poptop(L);  /* remove a[i] */
    }
    /* after the loop, a[i] >= P and a[lo .. i - 1] < P */
    /* next loop: repeat --j while P < a[j] */
    while ((void)lua_seqrawgeti(L, 1, --j), luaBsort_comp(L, -3, -1)) {
      if (l_unlikely(j < i))  /* j < i  but  a[j] > P ?? */
        luaL_error(L, "invalid order function for sorting");
      agn_poptop(L);  /* remove a[j] */
    }
    /* after the loop, a[j] <= P and a[j + 1 .. up] >= P */
    if (j < i) {  /* no elements out of place? */
      /* a[lo .. i - 1] <= P <= a[j + 1 .. i .. up] */
      agn_poptop(L);  /* pop a[j] */
      /* swap pivot (a[up - 1]) with a[i] to satisfy pos-condition */
      set2seq(L, up - 1, i);
      return i;
    }
    /* otherwise, swap a[i] - a[j] to restore invariant and repeat */
    set2seq(L, i, j);
  }
}

static IdxT partitionreg (lua_State *L, IdxT lo, IdxT up) {
  IdxT i = lo;  /* will be incremented before first use */
  IdxT j = up - 1;  /* will be decremented before first use */
  /* loop invariant: a[lo .. i] <= P <= a[j .. up] */
  for (;;) {
    /* next loop: repeat ++i while a[i] < P */
    while ((void)agn_regrawgeti(L, 1, ++i), luaBsort_comp(L, -1, -2)) {
      if (l_unlikely(i == up - 1))  /* a[i] < P  but a[up - 1] == P  ?? */
        luaL_error(L, "invalid order function for sorting");
      agn_poptop(L);  /* remove a[i] */
    }
    /* after the loop, a[i] >= P and a[lo .. i - 1] < P */
    /* next loop: repeat --j while P < a[j] */
    while ((void)agn_regrawgeti(L, 1, --j), luaBsort_comp(L, -3, -1)) {
      if (l_unlikely(j < i))  /* j < i  but  a[j] > P ?? */
        luaL_error(L, "invalid order function for sorting");
      agn_poptop(L);  /* remove a[j] */
    }
    /* after the loop, a[j] <= P and a[j + 1 .. up] >= P */
    if (j < i) {  /* no elements out of place? */
      /* a[lo .. i - 1] <= P <= a[j + 1 .. i .. up] */
      agn_poptop(L);  /* pop a[j] */
      /* swap pivot (a[up - 1]) with a[i] to satisfy pos-condition */
      set2reg(L, up - 1, i);
      return i;
    }
    /* otherwise, swap a[i] - a[j] to restore invariant and repeat */
    set2reg(L, i, j);
  }
}

/*
** Choose an element in the middle (2nd-3th quarters) of [lo,up]
** "randomized" by 'rnd'
*/
static IdxT choosePivot (IdxT lo, IdxT up, unsigned int rnd) {
  IdxT r4 = (up - lo) / 4;  /* range/4 */
  IdxT p = rnd % (r4 * 2) + (lo + r4);
  lua_assert(lo + r4 <= p && p <= up - r4);
  return p;
}

/*
** Quicksort algorithm (recursive function)
*/
static void auxsorttable (lua_State *L, IdxT lo, IdxT up, unsigned int rnd) {
  while (lo < up) {  /* loop for tail recursion */
    IdxT p;  /* Pivot index */
    IdxT n;  /* to be used later */
    /* sort elements 'lo', 'p', and 'up' */
    lua_rawgeti(L, 1, lo);
    lua_rawgeti(L, 1, up);
    if (luaBsort_comp(L, -1, -2)) { /* a[up] < a[lo]? */
      set2tbl(L, lo, up);  /* swap a[lo] - a[up] */
    } else
      lua_pop(L, 2);  /* remove both values */
    if (up - lo == 1)  /* only 2 elements? */
      return;  /* already sorted */
    if (up - lo < RANLIMIT || rnd == 0)  /* small interval or no randomize? */
      p = (lo + up)/2;  /* middle element is a good pivot */
    else  /* for larger intervals, it is worth a random pivot */
      p = choosePivot(lo, up, rnd);
    lua_rawgeti(L, 1, p);
    lua_rawgeti(L, 1, lo);
    if (luaBsort_comp(L, -2, -1)) {  /* a[p] < a[lo]? */
      set2tbl(L, p, lo);  /* swap a[p] - a[lo] */
    } else {
      agn_poptop(L);  /* remove a[lo] */
      lua_rawgeti(L, 1, up);
      if (luaBsort_comp(L, -1, -2)) { /* a[up] < a[p]? */
        set2tbl(L, p, up);  /* swap a[up] - a[p] */
      } else
        lua_pop(L, 2);
    }
    if (up - lo == 2)  /* only 3 elements? */
      return;  /* already sorted */
    lua_rawgeti(L, 1, p);  /* get middle element (Pivot) */
    lua_pushvalue(L, -1);  /* push Pivot */
    lua_rawgeti(L, 1, up - 1);  /* push a[up - 1] */
    set2tbl(L, p, up - 1);  /* swap Pivot (a[p]) with a[up - 1] */
    p = partitiontable(L, lo, up);
    /* a[lo .. p - 1] <= a[p] == P <= a[p + 1 .. up] */
    if (p - lo < up - p) {  /* lower interval is smaller? */
      auxsorttable(L, lo, p - 1, rnd);  /* call recursively for lower interval */
      n = p - lo;  /* size of smaller interval */
      lo = p + 1;  /* tail call for [p + 1 .. up] (upper interval) */
    }
    else {
      auxsorttable(L, p + 1, up, rnd);  /* call recursively for upper interval */
      n = up - p;  /* size of smaller interval */
      up = p - 1;  /* tail call for [lo .. p - 1]  (lower interval) */
    }
    if ((up - lo) / 128 > n) /* partition too imbalanced? */
      rnd = l_randomizePivot();  /* try a new randomization */
  }  /* tail call auxsorttable(L, lo, up, rnd) */
}

static void auxsortseq (lua_State *L, IdxT lo, IdxT up, unsigned int rnd) {
  while (lo < up) {  /* loop for tail recursion */
    IdxT p;  /* Pivot index */
    IdxT n;  /* to be used later */
    /* sort elements 'lo', 'p', and 'up' */
    lua_seqrawgeti(L, 1, lo);
    lua_seqrawgeti(L, 1, up);
    if (luaBsort_comp(L, -1, -2)) { /* a[up] < a[lo]? */
      set2seq(L, lo, up);  /* swap a[lo] - a[up] */
    } else
      lua_pop(L, 2);  /* remove both values */
    if (up - lo == 1)  /* only 2 elements? */
      return;  /* already sorted */
    if (up - lo < RANLIMIT || rnd == 0)  /* small interval or no randomize? */
      p = (lo + up)/2;  /* middle element is a good pivot */
    else  /* for larger intervals, it is worth a random pivot */
      p = choosePivot(lo, up, rnd);
    lua_seqrawgeti(L, 1, p);
    lua_seqrawgeti(L, 1, lo);
    if (luaBsort_comp(L, -2, -1)) {  /* a[p] < a[lo]? */
      set2seq(L, p, lo);  /* swap a[p] - a[lo] */
    } else {
      agn_poptop(L);  /* remove a[lo] */
      lua_seqrawgeti(L, 1, up);
      if (luaBsort_comp(L, -1, -2)) { /* a[up] < a[p]? */
        set2seq(L, p, up);  /* swap a[up] - a[p] */
      } else
        lua_pop(L, 2);
    }
    if (up - lo == 2)  /* only 3 elements? */
      return;  /* already sorted */
    lua_seqrawgeti(L, 1, p);  /* get middle element (Pivot) */
    lua_pushvalue(L, -1);  /* push Pivot */
    lua_seqrawgeti(L, 1, up - 1);  /* push a[up - 1] */
    set2seq(L, p, up - 1);  /* swap Pivot (a[p]) with a[up - 1] */
    p = partitionseq(L, lo, up);
    /* a[lo .. p - 1] <= a[p] == P <= a[p + 1 .. up] */
    if (p - lo < up - p) {  /* lower interval is smaller? */
      auxsortseq(L, lo, p - 1, rnd);  /* call recursively for lower interval */
      n = p - lo;  /* size of smaller interval */
      lo = p + 1;  /* tail call for [p + 1 .. up] (upper interval) */
    }
    else {
      auxsortseq(L, p + 1, up, rnd);  /* call recursively for upper interval */
      n = up - p;  /* size of smaller interval */
      up = p - 1;  /* tail call for [lo .. p - 1]  (lower interval) */
    }
    if ((up - lo) / 128 > n) /* partition too imbalanced? */
      rnd = l_randomizePivot();  /* try a new randomization */
  }  /* tail call auxsorttable(L, lo, up, rnd) */
}

static void auxsortreg (lua_State *L, IdxT lo, IdxT up, unsigned int rnd) {
  while (lo < up) {  /* loop for tail recursion */
    IdxT p;  /* Pivot index */
    IdxT n;  /* to be used later */
    /* sort elements 'lo', 'p', and 'up' */
    agn_regrawgeti(L, 1, lo);
    agn_regrawgeti(L, 1, up);
    if (luaBsort_comp(L, -1, -2)) { /* a[up] < a[lo]? */
      set2reg(L, lo, up);  /* swap a[lo] - a[up] */
    } else
      lua_pop(L, 2);  /* remove both values */
    if (up - lo == 1)  /* only 2 elements? */
      return;  /* already sorted */
    if (up - lo < RANLIMIT || rnd == 0)  /* small interval or no randomize? */
      p = (lo + up)/2;  /* middle element is a good pivot */
    else  /* for larger intervals, it is worth a random pivot */
      p = choosePivot(lo, up, rnd);
    agn_regrawgeti(L, 1, p);
    agn_regrawgeti(L, 1, lo);
    if (luaBsort_comp(L, -2, -1)) {  /* a[p] < a[lo]? */
      set2reg(L, p, lo);  /* swap a[p] - a[lo] */
    } else {
      agn_poptop(L);  /* remove a[lo] */
      agn_regrawgeti(L, 1, up);
      if (luaBsort_comp(L, -1, -2)) { /* a[up] < a[p]? */
        set2reg(L, p, up);  /* swap a[up] - a[p] */
      } else
        lua_pop(L, 2);
    }
    if (up - lo == 2)  /* only 3 elements? */
      return;  /* already sorted */
    agn_regrawgeti(L, 1, p);  /* get middle element (Pivot) */
    lua_pushvalue(L, -1);  /* push Pivot */
    agn_regrawgeti(L, 1, up - 1);  /* push a[up - 1] */
    set2reg(L, p, up - 1);  /* swap Pivot (a[p]) with a[up - 1] */
    p = partitionreg(L, lo, up);
    /* a[lo .. p - 1] <= a[p] == P <= a[p + 1 .. up] */
    if (p - lo < up - p) {  /* lower interval is smaller? */
      auxsortreg(L, lo, p - 1, rnd);  /* call recursively for lower interval */
      n = p - lo;  /* size of smaller interval */
      lo = p + 1;  /* tail call for [p + 1 .. up] (upper interval) */
    }
    else {
      auxsortreg(L, p + 1, up, rnd);  /* call recursively for upper interval */
      n = up - p;  /* size of smaller interval */
      up = p - 1;  /* tail call for [lo .. p - 1]  (lower interval) */
    }
    if ((up - lo) / 128 > n) /* partition too imbalanced? */
      rnd = l_randomizePivot();  /* try a new randomization */
  }  /* tail call auxsorttable(L, lo, up, rnd) */
}

static void auxintrosorttable (lua_State *L, int l, int u, int n) {
  int i;
  dblvec v;
  if (dblvec_init(v, n))  /* 4.10.3 fix */
    luaL_error(L, "Error: memory allocation failed.");
  for (i=0; i < n; i++) {
    (&v)->data[i] = agn_getinumber(L, 1, i + 1);
  }
  tools_dintrosort((&v)->data, l - 1, u - 1, 0, 2*sun_log2(n));
  for (i=0; i < n; i++) {
    agn_setinumber(L, 1, i + 1, (&v)->data[i]);
  }
  dblvec_free(v);
}

static void auxintrosortseq (lua_State *L, int l, int u, int n) {
  int i;
  dblvec v;
  if (dblvec_init(v, n))  /* 4.10.3 fix */
    luaL_error(L, "Error: memory allocation failed.");
  for (i=0; i < n; i++) {
    (&v)->data[i] = agn_seqgetinumber(L, 1, i + 1);
  }
  tools_dintrosort((&v)->data, l - 1, u - 1, 0, 2*sun_log2(n));
  for (i=0; i < n; i++) {
    agn_seqsetinumber(L, 1, i + 1, (&v)->data[i]);
  }
  dblvec_free(v);
}

static void auxintrosortreg (lua_State *L, int l, int u, int n) {
  int i;
  dblvec v;
  if (dblvec_init(v, n))  /* 4.10.3 fix */
    luaL_error(L, "Error: memory allocation failed.");
  for (i=0; i < n; i++) {
    (&v)->data[i] = agn_reggetinumber(L, 1, i + 1);
  }
  tools_dintrosort((&v)->data, l - 1, u - 1, 0, 2*sun_log2(n));
  for (i=0; i < n; i++) {
    agn_regsetinumber(L, 1, i + 1, (&v)->data[i]);
  }
  dblvec_free(v);
}

static int luaB_sort (lua_State *L) {
  int l, u, n, introsort, nargs;
  n = agn_nops(L, 1);
  nargs = lua_gettop(L);
  introsort = agn_isstring(L, nargs) && tools_streq(agn_tostring(L, nargs), "number");
  if (introsort) {
    lua_remove(L, nargs);
  }
  luaL_checkstack(L, 40, "not enough stack space");  /* assume array is smaller than 2^40 */
  l = 1; u = n;  /* 2.30.1 */
  if (lua_gettop(L) > 1 && agn_isinteger(L, 2)) {
    l = agn_tonumber(L, 2); lua_remove(L, 2); if (l < 1) l = 1;
  }
  if (lua_gettop(L) > 1 && agn_isinteger(L, 2)) {
    u = agn_tonumber(L, 2); lua_remove(L, 2); if (u > n) u = n;
  }
  if (!lua_isnoneornil(L, 2)) {  /* is there a 2nd argument? */
    if (lua_isfunction(L, 2)) {
      if (introsort) {
        luaL_error(L, "Error in " LUA_QS ": invalid combination of " LUA_QS " option and function.", "sort", "number");
      }
    } else {
      luaL_error(L, "Error in " LUA_QS ": function expected as second argument, got %s.", "sort",
        luaL_typename(L, 2));
    }
  }
  lua_settop(L, 2 - introsort);  /* make sure there are one or two arguments only */
  switch (lua_type(L, 1)) {
    case LUA_TTABLE:
      if (introsort)  /* 3.10.2 tuning by factor 4 */
        auxintrosorttable(L, l, u, n);
      else
        auxsorttable(L, l, u, 0);
      break;
    case LUA_TSEQ:
      if (introsort)  /* 3.10.2 tuning by factor 4 */
        auxintrosortseq(L, l, u, n);
      else
        auxsortseq(L, l, u, 0);
      break;
    case LUA_TREG:  /* 2.3.0 RC 3 */
      if (introsort)  /* 3.10.2 tuning by factor 4 */
        auxintrosortreg(L, l, u, n);
      else
        auxsortreg(L, l, u, 0);
      break;
    default:
      luaL_error(L, "Error in " LUA_QS ": table, sequence or register expected as first argument, got %s.", "sort",
        luaL_typename(L, 1));
  }
  lua_settop(L, 1);  /* return structure, 2.31.2 */
  return 1;
}


/* Non-destructive sorting */

static void auxsortedtable (lua_State *L, int l, int u) {
  while (l < u) {  /* for tail recursion */
    int i, j;
    /* sort elements a[l], a[(l+u)/2] and a[u] */
    lua_rawgeti(L, -1, l);
    lua_rawgeti(L, -2, u);
    if (luaBsort_comp(L, -1, -2)) { /* a[u] < a[l]? */
      luaBset3(L, -3, l, u);  /* swap a[l] - a[u] */
    } else
      agn_poptoptwo(L);
    if (u - l == 1) break;  /* only 2 elements */
    i = tools_midpoint(l, u);  /* 2.38.2 patch */
    lua_rawgeti(L, -1, i);
    lua_rawgeti(L, -2, l);
    if (luaBsort_comp(L, -2, -1)) {  /* a[i] < a[l]? */
      luaBset3(L, -3, i, l);
    } else {
      agn_poptop(L);  /* remove a[l] */
      lua_rawgeti(L, -2, u);
      if (luaBsort_comp(L, -1, -2)) {  /* a[u] < a[i]? */
        luaBset3(L, -3, i, u);
      } else
        agn_poptoptwo(L);
    }
    if (u - l == 2) break;  /* only 3 elements */
    lua_rawgeti(L, -1, i);  /* Pivot */
    lua_pushvalue(L, -1);
    lua_rawgeti(L, -3, u - 1);
    luaBset3(L, -4, i, u - 1);
    /* a[l] <= P == a[u - 1] <= a[u], only need to sort from l+1 to u-2 */
    i = l; j = u - 1;
    for (;;) {  /* invariant: a[l..i] <= P <= a[j..u] */
      /* repeat ++i until a[i] >= P */
      while (lua_rawgeti(L, -2, ++i), luaBsort_comp(L, -1, -2)) {
        if (i > u) luaL_error(L, "Error in " LUA_QS ": invalid order function for sorting.", "sorted");
        agn_poptop(L);  /* remove a[i] */
      }
      /* repeat --j until a[j] <= P */
      while (lua_rawgeti(L, -3, --j), luaBsort_comp(L, -3, -1)) {
        if (j < l) luaL_error(L, "Error in " LUA_QS ": invalid order function for sorting.", "sorted");
        agn_poptop(L);  /* remove a[j] */
      }
      if (j < i) {
        lua_pop(L, 3);  /* pop pivot, a[i], a[j] */
        break;
      }
      luaBset3(L, -4, i, j);
    }
    lua_rawgeti(L, -1, u - 1);
    lua_rawgeti(L, -2, i);
    luaBset3(L, -3, u - 1, i);  /* swap pivot (a[u - 1]) with a[i] */
    /* a[l..i-1] <= a[i] == P <= a[i + 1..u] */
    /* adjust so that smaller half is in [j..i] and larger one in [l..u] */
    if (i - l < u - i) {
      j = l; i = i - 1; l = i + 2;
    }
    else {
      j = i + 1; i = u; u = j - 2;
    }
    auxsortedtable(L, j, i);  /* call recursively the smaller one */
  }  /* repeat the routine for the larger one */
}


static void auxsortedseq (lua_State *L, int l, int u) {
  while (l < u) {  /* for tail recursion */
    int i, j;
    /* sort elements a[l], a[(l+u)/2] and a[u] */
    lua_seqrawgeti(L, -1, l);
    lua_seqrawgeti(L, -2, u);
    if (luaBsort_comp(L, -1, -2)) {  /* a[u] < a[l]? */
      luaBsetseq3(L, -3, l, u);  /* swap a[l] - a[u] */
    } else
      agn_poptoptwo(L);
    if (u - l == 1) break;  /* only 2 elements */
    i = tools_midpoint(l, u);  /* 2.38.2 patch */
    lua_seqrawgeti(L, -1, i);
    lua_seqrawgeti(L, -2, l);
    if (luaBsort_comp(L, -2, -1)) {  /* a[i] < a[l]? */
      luaBsetseq3(L, -3, i, l);
    } else {
      agn_poptop(L);  /* remove a[l] */
      lua_seqrawgeti(L, -2, u);
      if (luaBsort_comp(L, -1, -2)) {  /* a[u] < a[i]? */
        luaBsetseq3(L, -3, i, u);
      } else
        agn_poptoptwo(L);
    }
    if (u - l == 2) break;  /* only 3 elements */
    lua_seqrawgeti(L, -1, i);  /* Pivot */
    lua_pushvalue(L, -1);
    lua_seqrawgeti(L, -3, u - 1);
    luaBsetseq3(L, -4, i, u - 1);
    /* a[l] <= P == a[u - 1] <= a[u], only need to sort from l+1 to u-2 */
    i = l; j = u - 1;
    for (;;) {  /* invariant: a[l..i] <= P <= a[j..u] */
      /* repeat ++i until a[i] >= P */
      while (lua_seqrawgeti(L, -2, ++i), luaBsort_comp(L, -1, -2)) {
        if (i > u) luaL_error(L, "Error in " LUA_QS ": invalid order function for sorting.", "sorted");
        agn_poptop(L);  /* remove a[i] */
      }
      /* repeat --j until a[j] <= P */
      while (lua_seqrawgeti(L, -3, --j), luaBsort_comp(L, -3, -1)) {
        if (j < l) luaL_error(L, "Error in " LUA_QS ": invalid order function for sorting.", "sorted");
        agn_poptop(L);  /* remove a[j] */
      }
      if (j < i) {
        lua_pop(L, 3);  /* pop pivot, a[i], a[j] */
        break;
      }
      luaBsetseq3(L, -4, i, j);
    }
    lua_seqrawgeti(L, -1, u - 1);
    lua_seqrawgeti(L, -2, i);
    luaBsetseq3(L, -3, u - 1, i);  /* swap pivot (a[u - 1]) with a[i] */
    /* a[l..i-1] <= a[i] == P <= a[i + 1..u] */
    /* adjust so that smaller half is in [j..i] and larger one in [l..u] */
    if (i - l < u - i) {
      j = l; i = i - 1; l = i + 2;
    }
    else {
      j = i + 1; i = u; u = j - 2;
    }
    auxsortedseq(L, j, i);  /* call recursively the smaller one */
  }  /* repeat the routine for the larger one */
}

static void auxsortedreg (lua_State *L, int l, int u) {  /* 2.3.0 RC 3 */
  while (l < u) {  /* for tail recursion */
    int i, j;
    /* sort elements a[l], a[(l+u)/2] and a[u] */
    agn_regrawgeti(L, -1, l);
    agn_regrawgeti(L, -2, u);
    if (luaBsort_comp(L, -1, -2)) {  /* a[u] < a[l]? */
      luaBsetreg3(L, -3, l, u);  /* swap a[l] - a[u] */
    } else
      agn_poptoptwo(L);
    if (u - l == 1) break;  /* only 2 elements */
    i = tools_midpoint(l, u);  /* 2.38.2 patch */
    agn_regrawgeti(L, -1, i);
    agn_regrawgeti(L, -2, l);
    if (luaBsort_comp(L, -2, -1)) { /* a[i] < a[l]? */
      luaBsetreg3(L, -3, i, l);
    } else {
      agn_poptop(L);  /* remove a[l] */
      agn_regrawgeti(L, -2, u);
      if (luaBsort_comp(L, -1, -2)) {  /* a[u] < a[i]? */
        luaBsetreg3(L, -3, i, u);
      } else
        agn_poptoptwo(L);
    }
    if (u - l == 2) break;  /* only 3 elements */
    agn_regrawgeti(L, -1, i);  /* Pivot */
    lua_pushvalue(L, -1);
    agn_regrawgeti(L, -3, u - 1);
    luaBsetreg3(L, -4, i, u - 1);
    /* a[l] <= P == a[u - 1] <= a[u], only need to sort from l+1 to u-2 */
    i = l; j = u - 1;
    for (;;) {  /* invariant: a[l..i] <= P <= a[j..u] */
      /* repeat ++i until a[i] >= P */
      while (agn_regrawgeti(L, -2, ++i), luaBsort_comp(L, -1, -2)) {
        if (i > u) luaL_error(L, "Error in " LUA_QS ": invalid order function for sorting.", "sorted");
        agn_poptop(L);  /* remove a[i] */
      }
      /* repeat --j until a[j] <= P */
      while (agn_regrawgeti(L, -3, --j), luaBsort_comp(L, -3, -1)) {
        if (j < l) luaL_error(L, "Error in " LUA_QS ": invalid order function for sorting.", "sorted");
        agn_poptop(L);  /* remove a[j] */
      }
      if (j < i) {
        lua_pop(L, 3);  /* pop pivot, a[i], a[j] */
        break;
      }
      luaBsetreg3(L, -4, i, j);
    }
    agn_regrawgeti(L, -1, u - 1);
    agn_regrawgeti(L, -2, i);
    luaBsetreg3(L, -3, u - 1, i);  /* swap pivot (a[u - 1]) with a[i] */
    /* a[l..i-1] <= a[i] == P <= a[i + 1..u] */
    /* adjust so that smaller half is in [j..i] and larger one in [l..u] */
    if (i - l < u - i) {
      j = l; i = i - 1; l = i + 2;
    }
    else {
      j = i + 1; i = u; u = j - 2;
    }
    auxsortedreg(L, j, i);  /* call recursively the smaller one */
  }  /* repeat the routine for the larger one */
}


static void auxintrosortedtable (lua_State *L, int l, int u, int n) {
  int i;
  dblvec v;
  if (dblvec_init(v, n))  /* 4.10.3 fix */
    luaL_error(L, "Error: memory allocation failed.");
  for (i=0; i < n; i++) {
    (&v)->data[i] = agn_getinumber(L, 1, i + 1);
  }
  tools_dintrosort((&v)->data, l - 1, u - 1, 0, 2*sun_log2(n));
  lua_createtable(L, n, 0);
  for (i=0; i < n; i++) {
    agn_setinumber(L, -1, i + 1, (&v)->data[i]);
  }
  dblvec_free(v);
}


static void auxintrosortedseq (lua_State *L, int l, int u, int n) {
  int i;
  dblvec v;
  if (dblvec_init(v, n))  /* 4.10.3 fix */
    luaL_error(L, "Error: memory allocation failed.");
  for (i=0; i < n; i++) {
    (&v)->data[i] = agn_seqgetinumber(L, 1, i + 1);
  }
  tools_dintrosort((&v)->data, l - 1, u - 1, 0, 2*sun_log2(n));
  agn_createseq(L, n);
  for (i=0; i < n; i++) {
    agn_seqsetinumber(L, -1, i + 1, (&v)->data[i]);
  }
  dblvec_free(v);
}


static void auxintrosortedreg (lua_State *L, int l, int u, int n) {
  int i;
  dblvec v;
  if (dblvec_init(v, n))  /* 4.10.3 fix */
    luaL_error(L, "Error: memory allocation failed.");
  for (i=0; i < n; i++) {
    (&v)->data[i] = agn_reggetinumber(L, 1, i + 1);
  }
  tools_dintrosort((&v)->data, l - 1, u - 1, 0, 2*sun_log2(n));
  agn_createreg(L, n);
  for (i=0; i < n; i++) {
    agn_regsetinumber(L, -1, i + 1, (&v)->data[i]);
  }
  dblvec_free(v);
}


static int agnB_sorted (lua_State *L) {
  int l, u, n, introsort, nargs;
  n = agn_nops(L, 1);
  nargs = lua_gettop(L);
  introsort = agn_isstring(L, nargs) && tools_streq(agn_tostring(L, nargs), "number");
  if (introsort) {
    lua_remove(L, nargs);
  }
  luaL_checkstack(L, 40, "not enough stack space");  /* assume array is smaller than 2^40 */
  l = 1; u = n;  /* 2.30.1 */
  if (lua_gettop(L) > 1 && agn_isinteger(L, 2)) {
    l = agn_tonumber(L, 2); lua_remove(L, 2); if (l < 1) l = 1;
  }
  if (lua_gettop(L) > 1 && agn_isinteger(L, 2)) {
    u = agn_tonumber(L, 2); lua_remove(L, 2); if (u > n) u = n;
  }
  if (!lua_isnoneornil(L, 2)) {  /* is there a 2nd argument? */
    if (lua_isfunction(L, 2)) {
      if (introsort) {
        luaL_error(L, "Error in " LUA_QS ": invalid combination of " LUA_QS " option and function.", "sorted", "number");
      }
    } else {
      luaL_error(L, "Error in " LUA_QS ": function expected as second argument, got %s.", "sorted",
        luaL_typename(L, 2));
    }
  }
  lua_settop(L, 2 - introsort);  /* make sure there are one or two arguments only */
  if (!introsort) agn_copy(L, 1, 3);
  switch (lua_type(L, 1)) {
    case LUA_TTABLE:
      if (introsort)  /* 3.10.2 tuning by factor 4 */
        auxintrosortedtable(L, l, u, n);
      else
        auxsortedtable(L, l, u);
      break;
    case LUA_TSEQ:
      if (introsort)  /* 3.10.2 tuning by factor 4 */
        auxintrosortedseq(L, l, u, n);
      else
        auxsortedseq(L, l, u);
      break;
    case LUA_TREG:  /* 2.3.0 RC 3 */
      if (introsort)  /* 3.10.2 tuning by factor 4 */
        auxintrosortedreg(L, l, u, n);
      else
        auxsortedreg(L, l, u);
      break;
    default:
      luaL_error(L, "Error in " LUA_QS ": table, sequence or register expected as first argument, got %s.", "sorted",
        luaL_typename(L, 1));
  }
  return 1;
}


/* Radix sort. ***************************************************************************************
   The implementation has been taken from Martin Broadhurst's exemplary website
   http://www.martinbroadhurst.com/radix-sort-of-strings-in-c.html (defunct, unfortunately)
   Adapted for Agena to preserve type (number, string) and avoid re-computing string lengths. 5.3.5
 *****************************************************************************************************/

/* A linked list node containing a string and length */
struct bucket_entry {
  char *str;
  size_t len;
  struct bucket_entry *next;
  char isnum;
};

typedef struct bucket_entry bucket_entry;

/* A linked list */
struct bucket {
  bucket_entry *head;
  bucket_entry *tail;
};

typedef struct bucket bucket;

/* Initialise buckets */
static void init_buckets (bucket *buckets) {
  unsigned int b;
  for (b=0; b < 256; b++) {
    buckets[b].head = NULL;
    buckets[b].tail = NULL;
  }
}

/* Turn entries into a linked list of words with their lengths. Return the length of the longest word. */
static size_t init_entries (char **strings, char *isnum, size_t *lengths, size_t len, bucket_entry *entries) {
  unsigned int s;
  size_t maxlen = 0;
  for (s=0; s < len; s++) {
    entries[s].str   = strings[s];
    entries[s].len   = lengths[s];
    entries[s].isnum = isnum[s];
    if (entries[s].len > maxlen) {
      maxlen = entries[s].len;
    }
    if (s < len - 1) {
      entries[s].next = &entries[s + 1];
    }
    else {
      entries[s].next = NULL;
    }
  }
  return maxlen;
}

/* Put strings into buckets according to the character c from the right */
void bucket_strings (bucket_entry *head, bucket *buckets, unsigned int c) {
  bucket_entry *current = head;
  while (current != NULL) {
    bucket_entry *next = current->next;
    current->next = NULL;
    int pos = current->len - 1 - c;
    unsigned char ch;
    ch = (pos < 0) ? 0 : current->str[pos];
    if (buckets[ch].head == NULL) {
      buckets[ch].head = current;
      buckets[ch].tail = current;
    }
    else {
      buckets[ch].tail->next = current;
      buckets[ch].tail = buckets[ch].tail->next;
    }
    current = next;
  }
}

/* Merge buckets into a single linked list */
bucket_entry *merge_buckets (bucket *buckets) {
  bucket_entry *head = NULL;
  bucket_entry *tail = NULL;  /* 5.4.1, to prevent compiler warnings */
  unsigned int b;
  for (b=0; b < 256; b++) {
    if (buckets[b].head != NULL) {
      if (head == NULL) {
        head = buckets[b].head;
        tail = buckets[b].tail;
      }
      else {
        tail->next = buckets[b].head;
        tail = buckets[b].tail;
      }
    }
  }
  return head;
}

void copy_list (const bucket_entry *head, char **strings, char *isnum, size_t *lengths) {
  const bucket_entry *current;
  unsigned int s;
  for (current=head, s=0; current != NULL; current = current->next, s++) {
    strings[s] = current->str;
    isnum[s]   = current->isnum;
    lengths[s] = current->len;
  }
}

void radix_sort_string (char **strings, char *isnum, size_t *lengths, size_t len) {
  size_t maxlen;
  unsigned int c;
  bucket_entry *head;
  bucket_entry *entries = malloc(len*sizeof(bucket_entry));
  bucket *buckets = malloc(256*sizeof(bucket));
  if (!entries || !buckets) {
    xfreeall(entries, buckets);
  }
  init_buckets(buckets);
  maxlen = init_entries(strings, isnum, lengths, len, entries);
  head = &entries[0];
  for (c=0; c < maxlen; c++) {
    bucket_strings(head, buckets, c);
    head = merge_buckets(buckets);
    init_buckets(buckets);
  }
  /* Copy back into array */
  copy_list(head, strings, isnum, lengths);
  xfreeall(buckets, entries);
}

/* Takes any table of strings and/or numbers and then radix-sorts them, where in the resulting
   new table the items will be in order of length, and then in lexicographic order within each
   length class (shortlex order). If the item in the input table is a number, it will also a
   number in the output table. The sorting is done non-destructively, leaving the input table
   unchanged.

   Example:

   > rsorted(['1', 100, '2', '11']):
   [1, 2, 11, 100]

   Compare to:

   > sorted(['1', '100', '2', '11']):  # can't sort a mix of numbers and strings
   [1, 100, 11, 2]

   See also: sort, sorted. */
static int agnB_rsorted (lua_State *L) {
  int i, j, len;
  char **strings;
  char *isnum;
  size_t *lengths;
  luaL_checktype(L, 1, LUA_TTABLE);
  len = agn_asize(L, 1);
  if (len == 0) {  /* return new (!) empty table */
    lua_newtable(L);
    return 1;
  }
  strings = (char **)malloc(len*sizeof(char *));
  if (strings == NULL) {
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "rsorted");
  }
  isnum = (char *)malloc(len*sizeof(char));
  if (isnum == NULL) {
    xfree(strings);
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "rsorted");
  }
  lengths = (size_t *)malloc(len*sizeof(size_t));
  if (lengths == NULL) {
    xfreeall(strings, isnum);
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "rsorted");
  }
  lua_pushnil(L); i = 0;
  while (lua_next(L, 1)) {
    if (lua_isstring(L, -1)) {
      size_t l;
      const char *str;
      isnum[i] = agn_isnumber(L, -1);
      str = lua_tolstring(L, -1, &l);
      char *tmp = tools_strdup(str);
      if (!tmp) luaL_error(L, "Error: memory allocation failed.");  /* 7.7.7 fix */
      strings[i] = tmp;
      if (strings[i] == NULL) {  /* 7.3.5 */
        xfreeall(strings, isnum, lengths);
        luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "rsorted");
      }
      lengths[i++] = l;
    }
    agn_poptop(L);
  }
  /* better sure than sorry; allocate slots left empty above */
  for (j=i; j < len; j++) { strings[j] = NULL; isnum[j] = 0; lengths[j] = 0; }
  lua_createtable(L, i, 0);
  if (i > 0) {
    radix_sort_string(strings, isnum, lengths, i);
    for (j=0; j < i; j++) {
      lua_pushstring(L, strings[j]);
      if (isnum[j]) {
        int exception;
        lua_Number n = agn_tonumberx(L, -1, &exception);
        if (!exception) {  /* input was a number ?  -> convert it back to number */
          agn_poptop(L);
          lua_pushnumber(L, n);
        }
      }
      xfree(strings[j]);
      lua_rawseti(L, -2, j + 1);
    }
  }
  xfreeall(strings, isnum, lengths);
  return 1;
}


static const int mode[] = {OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_INTDIV, OP_MOD, OP_POW, OP_IPOW};
static const char *const modenames[] = {"+", "-", "*", "/", "\\", "%", "^", "**", NULL};

/* extended 0.29.3, December 25, 2009; extended 0.30.2, January 11, 2010, extended 2.21.1, June 13, 2020 */
static int agnB_zip (lua_State *L) {
  int i, j, sizea, sizeb, nargs, t, op;
  t = lua_type(L, 1);
  luaL_argcheck(L, t == LUA_TFUNCTION || t == LUA_TSTRING, 1, "function or string expected");
  nargs = lua_gettop(L);
  luaL_checkstack(L, 3, "too many arguments");  /* 4.2.1 change */
  op = lua_isfunction(L, 1) ? -1 :
    agnL_getsetting(L, 1, modenames, mode, "zip");
  if (lua_type(L, 2) == LUA_TSEQ && lua_type(L, 3) == LUA_TSEQ) {
    sizea = agn_seqsize(L, 2);
    sizeb = agn_seqsize(L, 3);
    if (sizea != sizeb)
      luaL_error(L, "Error in " LUA_QS ": sequences of different size.", "zip");
    agn_createseq(L, sizea);
    /* set its type to the one of the original sequence */
    if (agn_getutype(L, 2)) {
      agn_setutype(L, -2, -1);
      agn_poptop(L);  /* pop type (string) */
    } else if (agn_getutype(L, 3)) {
      agn_setutype(L, -2, -1);
      agn_poptop(L);  /* pop type (string) */
    }
    /* copy metatable */
    if (lua_getmetatable(L, 2))
      lua_setmetatable(L, -2);
    else if (lua_getmetatable(L, 3))
      lua_setmetatable(L, -2);
    /* now traverse sequences */
    if (op < 0) {  /* new 2.21.1 */
      for (i=0; i < sizea; i++) {
        luaL_checkstack(L, nargs, "too many arguments");  /* 3.5.4 fix */
        lua_pushvalue(L, 1);        /* push function; FIXME: can be optimized */
        lua_seqrawgeti(L, 2, i + 1);
        lua_seqrawgeti(L, 3, i + 1);
        for (j=4; j <= nargs; j++) {
          lua_pushvalue(L, j);
        }
        lua_call(L, nargs - 1, 1);  /* call function with nargs-1 argument(s) and one result */
        lua_seqinsert(L, -2);       /* store result to new sequence */
      }
    } else {
      for (i=0; i < sizea; i++) {
        lua_seqrawgeti(L, 2, i + 1);
        lua_seqrawgeti(L, 3, i + 1);
        lua_arith(L, op);
        lua_seqinsert(L, -2);       /* store result to new sequence */
      }
    }
  } else if (lua_type(L, 2) == LUA_TREG && lua_type(L, 3) == LUA_TREG) {
    sizea = agn_reggettop(L, 2);
    sizeb = agn_reggettop(L, 3);
    if (sizea != sizeb)
      luaL_error(L, "Error in " LUA_QS ": registers of different size.", "zip");
    agn_createreg(L, sizea);
    /* copy metatable */
    if (lua_getmetatable(L, 2))
      lua_setmetatable(L, -2);
    else if (lua_getmetatable(L, 3))
      lua_setmetatable(L, -2);
    /* set its type to the one of the original register */
    if (agn_getutype(L, 2)) {
      agn_setutype(L, -2, -1);
      agn_poptop(L);  /* pop type (string) */
    } else if (agn_getutype(L, 3)) {
      agn_setutype(L, -2, -1);
      agn_poptop(L);  /* pop type (string) */
    }
    /* now traverse registers */
    if (op < 0) {  /* new 2.21.1 */
      for (i=0; i < sizea; i++) {
        luaL_checkstack(L, nargs, "too many arguments");  /* 3.5.4 fix */
        lua_pushvalue(L, 1);      /* push function; FIXME: can be optimized */
        agn_regrawgeti(L, 2, i + 1);
        agn_regrawgeti(L, 3, i + 1);
        for (j=4; j <= nargs; j++) {
          lua_pushvalue(L, j);
        }
        lua_call(L, nargs - 1, 1);  /* call function with nargs-1 argument(s) and one result */
        agn_regrawseti(L, -2, i + 1);  /* store result to new register */
      }
    } else {
      for (i=0; i < sizea; i++) {
        agn_regrawgeti(L, 2, i + 1);
        agn_regrawgeti(L, 3, i + 1);
        lua_arith(L, op);
        agn_regrawseti(L, -2, i + 1);  /* store result to new register */
      }
    }
  }
  else if (lua_type(L, 2) == LUA_TTABLE && lua_type(L, 3) == LUA_TTABLE) {
    sizea = agn_size(L, 2);
    sizeb = agn_size(L, 3);
    if (sizea != sizeb)
      luaL_error(L, "Error in " LUA_QS ": tables of different size.", "zip");
    lua_createtable(L, sizea, 0);
    /* set its type to the one of the original table */
    if (agn_getutype(L, 2)) {
      agn_setutype(L, -2, -1);
      agn_poptop(L);  /* pop type (string) */
    } else if (agn_getutype(L, 3)) {
      agn_setutype(L, -2, -1);
      agn_poptop(L);  /* pop type (string) */
    }
    /* copy metatable */
    if (lua_getmetatable(L, 2))
      lua_setmetatable(L, -2);
    else if (lua_getmetatable(L, 3))
      lua_setmetatable(L, -2);
    /* now traverse tables, changed 0.29.3 */
    if (op < 0) {  /* new 2.21.1 */
      lua_pushnil(L);
      while (agn_fnext(L, 2, 1, 0) != 0) {
        luaL_checkstack(L, nargs, "too many arguments");  /* 3.5.4 fix */
        lua_pushvalue(L, -3);  /* push key of left-hand table */
        lua_gettable(L, 3);    /* get value of right-hand table */
        if (lua_isnil(L, -1)) {
          const char *key = lua_tostring(L, -4);
          if (key != NULL)
            luaL_error(L, "Error in " LUA_QS ": key " LUA_QS " missing in second table.", "zip", key);
          else
            luaL_error(L, "Error in " LUA_QS ": key missing in second table.", "zip");
        }
        for (j=4; j <= nargs; j++) {
          lua_pushvalue(L, j);
        }
        lua_call(L, nargs - 1, 1);  /* call function with nargs-1 argument(s) and one result */
        lua_rawset2(L, -3);         /* set result to new table, keep key on stack */
      }
    } else {
      lua_pushnil(L);
      while (lua_next(L, 2) != 0) {
        lua_pushvalue(L, -2);  /* push key of left-hand table */
        lua_gettable(L, 3);    /* get value of right-hand table */
        if (lua_isnil(L, -1)) {
          const char *key = lua_tostring(L, -4);
          if (key != NULL)
            luaL_error(L, "Error in " LUA_QS ": key " LUA_QS " missing in second table.", "zip", key);
          else
            luaL_error(L, "Error in " LUA_QS ": key missing in second table.", "zip");
        }
        lua_arith(L, op);
        lua_rawset2(L, -3);  /* set result to new table, keep key on stack */
      }
    }
  } else
    luaL_error(L, "Error in " LUA_QS ": expected either two tables, sequences or registers.", "zip");
  return 1;
}


/* luaB_min: return the smallest value in a list. If the option 'sorted' is given,
   the list is not traversed, instead the first entry in the list is returned.
   If the list is empty, NULL is returned. lua_getn must be called at first,
   otherwise lua_rawgeti would return 0 with empty lists. June 20, 2007 */
static int luaB_min (lua_State *L) {
  int n, type;
  size_t minpos, maxpos;
  lua_Number min, max;
  type = lua_type(L, 1);
  luaL_typecheck(L, type == LUA_TNUMBER || (type >= LUA_TTABLE && !(type == LUA_TPAIR)), 1, "number, table, set, sequence or register expected", type);
  min = 0;  /* to prevent compiler warnings */
  if (type != LUA_TNUMBER) {  /* 2.14.4 */
    if (tools_streq(luaL_optstring(L, 2, "default"), "sorted")) {  /* 2.12.0 RC 4 patch, 2.16.12 tweak */
      if (agn_nops(L, 1) == 0)  /* 0.26.1 patch */
        lua_pushnil(L);
      else if (type == LUA_TTABLE)
        lua_pushnumber(L, agn_getinumber(L, 1, 1));
      else if (type == LUA_TSEQ)
        lua_pushnumber(L, lua_seqrawgetinumber(L, 1, 1));
      else if (type == LUA_TREG)
        lua_pushnumber(L, agn_reggetinumber(L, 1, 1));
      else
        luaL_error(L, "Error in " LUA_QS ": " LUA_QS " option cannot be given with sets.", "min", "sorted");
      return 1;
    }
  }
  switch (type) {
    case LUA_TNUMBER: {  /* 2.14.4 */
      lua_Number x, y;
      x = agn_tonumber(L, 1);     /* ! avoid double evaluation in fMin macro ! */
      y = agn_checknumber(L, 2);  /* ditto */
      min = fMin(x, y);
      break;
    }
    case LUA_TTABLE: case LUA_TSEQ: case LUA_TREG: case LUA_TSET: {
      if (!agn_minmax(L, 1, 0, &min, &minpos, &max, &maxpos, &n)) {
        lua_pushnil(L);
        return 1;
      }
      break;
    }
    default: lua_assert(0);  /* should not happen */
  }
  lua_pushnumber(L, min);
  return 1;
}


/* tbl_max: return the largest value in a list. If the option 'sorted' is given,
   the list is not traversed, instead the last entry in the list is returned.
   If the list is empty, `null` is returned. lua_getn must be called at first,
   otherwise lua_rawgeti would return 0 with empty lists.
   June 20, 2007; tuned December 23, 2007 */
static int luaB_max (lua_State *L) {
  int n, type;
  size_t minpos, maxpos;
  lua_Number max, min;
  type = lua_type(L, 1);
  luaL_typecheck(L, type == LUA_TNUMBER || (type >= LUA_TTABLE && !(type == LUA_TPAIR)), 1, "number, table, set, sequence or register expected", type);
  max = 0;  /* to prevent compiler warnings */
  if (type != LUA_TNUMBER) {  /* 2.14.4 */
    if (tools_streq(luaL_optstring(L, 2, "default"), "sorted")) {  /* 2.12.0 RC 4 patch, 2.16.12 tweak */
      size_t n = agn_nops(L, 1);
      if (n == 0)  /* 0.26.1 patch */
        lua_pushnil(L);
      else if (type == LUA_TTABLE)
        lua_pushnumber(L, agn_getinumber(L, 1, n));  /* 4.1.2 fix */
      else if (type == LUA_TSEQ)
        lua_pushnumber(L, lua_seqrawgetinumber(L, 1, n));  /* 4.1.2 fix */
      else if (type == LUA_TREG)
        lua_pushnumber(L, agn_reggetinumber(L, 1, n));  /* 4.1.2 fix */
      else
        luaL_error(L, "Error in " LUA_QS ": " LUA_QS " option cannot be given with sets.", "max", "sorted");
      return 1;
    }
  }
  switch (type) {
    case LUA_TNUMBER: {  /* 2.14.4 */
      lua_Number x, y;
      x = agn_tonumber(L, 1);     /* ! avoid double evaluation in fMin macro ! */
      y = agn_checknumber(L, 2);  /* ditto */
      max = fMax(x, y);
      break;
    }
    case LUA_TTABLE: case LUA_TSEQ: case LUA_TREG: case LUA_TSET: {
      /* 4.9.2 change: 45 percent faster */
      if (!agn_minmax(L, 1, 0, &min, &minpos, &max, &maxpos, &n)) {
        lua_pushnil(L);
        return 1;
      }
      break;
    }
    default: lua_assert(0);  /* should not happen */
  }
  lua_pushnumber(L, max);
  return 1;
}


/* get an entry from a table or sequence without issuing an error if an index does not exist, 13.01.2010, 0.30.3;
   the Agena version is five times slower; patched 0.31.5 to work with sequences as designed */
static int agnB_getentry (lua_State *L) {
  int type, nargs, i;
  type = lua_type(L, 1);
  nargs = lua_gettop(L) + 1;
  luaL_typecheck(L, type == LUA_TTABLE || type == LUA_TSEQ || type == LUA_TREG, 1, "table, sequence or register expected", type);
  lua_pushvalue(L, 1);
  for (i=2; i < nargs && (type == LUA_TTABLE || type == LUA_TSEQ || type == LUA_TREG); i++) {
    lua_pushvalue(L, i);
    if (type == LUA_TTABLE)  /* separating the code for tables and for seqs does not produce faster results */
      lua_rawget(L, -2);
    else if (type == LUA_TSEQ)
      lua_seqrawget(L, -2, 1);  /* return stored value, or if unassigned return null */
    else if (type == LUA_TREG)  /* 2.3.0 RC 3 */
      agn_regrawget(L, -2, 1);
    type = lua_type(L, -1);
    lua_replace(L, -2);  /* remove old structure, replace it by new one */
  }
  if (i < nargs) {   /* return null where Agena would throw an error otherwise because of invalid indexing */
    agn_poptop(L);   /* remove last entry */
    lua_pushnil(L);  /* push null */
  }  /* otherwise return last entry */
  return 1;
}


/* Returns the element at index a[i[1], i[2], ..., i[k-1]], where a is a table, sequence or register. If the index position is invalid, the function returns `null`.
   If a[i[1], i[2], ... i[k-1]] = null, then a[i[1], i[2], ... i[k-1]] := i[k] and return i[k].
   Extended clone of Scala's `getOrElseUpdate` method; based on getentry, 2.14.2 */
static int agnB_getorset (lua_State *L) {
  int type, nargs, i, j, c;
  type = lua_type(L, 1);
  nargs = lua_gettop(L);
  if (nargs < 3)
    luaL_error(L, "Error in " LUA_QS ": too few arguments, expected three or more.", "getorset");
  luaL_checkstack(L, 2 * nargs, "too many arguments");
  luaL_typecheck(L, type == LUA_TTABLE || type == LUA_TSEQ || type == LUA_TREG, 1, "table, sequence or register expected", type);
  lua_pushvalue(L, 1);
  c = 0;
  for (i=2; (i <= nargs - 1) && (type == LUA_TTABLE || type == LUA_TSEQ || type == LUA_TREG); i++) {
    c++;
    lua_pushvalue(L, i);
    lua_pushvalue(L, i);
    if (type == LUA_TTABLE)  /* separating the code for tables and for seqs does not produce faster results */
      lua_rawget(L, -3);
    else if (type == LUA_TSEQ)
      lua_seqrawget(L, -3, 1);  /* return stored value, or if unassigned return null */
    else if (type == LUA_TREG)  /* 2.3.0 RC 3 */
      agn_regrawget(L, -3, 1);
    type = lua_type(L, -1);
  }
  if (c < nargs - 2) {  /* index position invalid */
    lua_settop(L, 0);
    lua_pushnil(L);
  } else if (!lua_isnil(L, -1)) {  /* element found, remove all values below the top, return element found */
    for (j=1; j <= 2*c; j++) lua_remove(L, -2);  /* level stack before returning */
  } else {  /* set value to structure */
    agn_poptop(L);
    lua_pushvalue(L, nargs);
    for (j=1; j <= 2*c; j++) {
      if (lua_type(L, -3) == LUA_TTABLE)
        lua_rawset(L, -3);
      else if (lua_type(L, -3) == LUA_TSEQ)
        lua_seqrawset(L, -3);
      else if (lua_type(L, -3) == LUA_TREG)
        agn_regset(L, -3);
    }
    agn_poptop(L);  /* pop first structure pushed */
    lua_pushvalue(L, nargs);
  }
  return 1;
}


/* ANSI part completely rewritten 0.33.2 */
static int mathB_arctan2 (lua_State *L) {
  if (lua_type(L, 1) == LUA_TNUMBER && lua_type(L, 2) == LUA_TNUMBER)
    lua_pushnumber(L, sun_atan2(agn_checknumber(L, 1), agn_checknumber(L, 2)));  /* 2.11.0 tuning */
  else if (lua_type(L, 1) == LUA_TCOMPLEX && lua_type(L, 2) == LUA_TCOMPLEX) {
#ifndef PROPCMPLX
    agn_Complex x, y;
    y = agn_tocomplex(L, 1);
    x = agn_tocomplex(L, 2);
    agn_createcomplex(L, tools_catan2(y, x));  /* 2.11.2 tuning; 2.40.2 change */
#else
    lua_Number a, b, c, d, re, im;
    agn_getcmplxparts(L, 1, &a, &b);  /* 2.17.6 */
    agn_getcmplxparts(L, 2, &c, &d);  /* 2.17.6 */
    tools_catan2(a, b, c, d, &re, &im);
    agn_createcomplex(L, re, im);
#endif
  }
  else if (lua_type(L, 1) == LUA_TCOMPLEX && lua_type(L, 2) == LUA_TNUMBER) {
#ifndef PROPCMPLX
    agn_Complex x, y, r, d, s;
    y = agn_tocomplex(L, 1);
    x = agn_tonumber(L, 2) + 0*I;  /* Agena 1.4.3/1.5.0 */
    d = tools_csqrt(x*x + y*y);
    if (d == 0.0 + 0.0*I) {  /* 4.5.5 fix */
      agn_createcomplex(L, d);
    } else {
      r = tools_cdiv((x + I*y), d);
      s = (y == 0.0 + 0.0*I && creal(x) == 0.0 && cimag(x) < 0.0) ? 1 : -1;  /* 4.5.5 fix */
      agn_createcomplex(L, s*I*tools_clog(r));
    }
#else
    lua_Number a, b, c, d, re, im;
    agn_getcmplxparts(L, 1, &a, &b);  /* 2.17.6 */
    c = agn_tonumber(L, 2); d = 0;  /* Agena 1.4.3/1.5.0 */
    tools_catan2(a, b, c, d, &re, &im);
    agn_createcomplex(L, re, im);
#endif
  }
  else if (lua_type(L, 1) == LUA_TNUMBER && lua_type(L, 2) == LUA_TCOMPLEX) {
#ifndef PROPCMPLX
    agn_Complex x, y, r, d, s;
    y = agn_tonumber(L, 1) + 0*I;
    x = agn_tocomplex(L, 2);
    d = tools_csqrt(x*x + y*y);
    if (d == 0.0 + 0.0*I) {  /* 4.5.5 fix */
      agn_createcomplex(L, d);
    } else {
      r = tools_cdiv((x + I*y), d);
#if defined(OPENSUSE) || defined(__APPLE__)
      s = -1;  /* 6.8.2 fix */
#else
      s = (y == 0.0 + 0.0*I && creal(x) == 0.0 && cimag(x) < 0.0) ? 1 : -1;  /* 4.5.5 fix */
#endif
      agn_createcomplex(L, s*I*tools_clog(r));
    }
#else
    lua_Number a, b, c, d, re, im;
    a = agn_tonumber(L, 1); b = 0;  /* Agena 1.4.3/1.5.0 */
    agn_getcmplxparts(L, 2, &c, &d);  /* 2.17.6 */
    tools_catan2(a, b, c, d, &re, &im);
    agn_createcomplex(L, re, im);
#endif
  } else if ((agn_isdual(L, 1) && agn_isdual(L, 2)) ||
             (lua_isnumber(L, 1) && agn_isdual(L, 2)) ||
             (agn_isdual(L, 1) && lua_isnumber(L, 2))) {
    trydualfix(L, "arctan2", 2);  /* FIXME: correct underlying dual function, that is fix the 3rd derivative */
    return 1;
  } else {
    int nargs = lua_gettop(L);
    if (nargs != 2)
      luaL_error(L, "Error in " LUA_QS ": two arguments expected, got %d.", "arctan2", nargs);  /* 3.7.8 improvement */
    else
      luaL_error(L, "Error in " LUA_QS ": (complex) numbers expected, got %s, %s.", "arctan2",  /* 3.7.8 improvement */
        luaL_typename(L, 1), luaL_typename(L, 2));
  }
  return 1;
}


/* Returns the inverse hyperbolic sine of the (complex) number x and returns a (complex) number. */
static int mathB_arcsinh (lua_State *L) {  /* 2.14.10, extended 2.32.3 */
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      /* ln(x + sqrt(1 + x^2)) */
      lua_pushnumber(L, sun_asinh(agn_tonumber(L, 1)));
      break;
    }
    case LUA_TCOMPLEX: {
      /* -I*arcsin(I*z) */
      lua_Number a, b;
      agn_getcmplxparts(L, 1, &a, &b);
      if (b == 0) {  /* prevent round-off errors and get quicker result */
        agn_pushcomplex(L, sun_asinh(a), 0);
      } else {  /* do not change to long double as this will lead to out-of-domain errors */
        lua_Number t3, t4, t6, t8, t10, t12, t14;
        t3 = a*a;
        t4 = b*b;
        /* 6.6.11: We have round-off errors in Linux and Mac OS X, resulting in negative redicals, also when compiled
           for SSE2 on Windows (GCC switches -msse2 -mfpmath=sse). So do not optimise the following two statements. */
        t6 = sqrt(t3 + t4 + 2.0*b + 1.0);
        t8 = sqrt(t3 + t4 - 2.0*b + 1.0);
        t10 = sun_pow(0.5*(t6 + t8), 2.0, 1);  /* call sun_pow, do NOT optimise by changing to `x*x` ! */
        t12 = sqrt(t10 - 1.0);
        t14 = 0.5*(t6 - t8);
        tools_adjust(t14, 1.0, AGN_EPSILON, -1);  /* 2.32.4 improvement, only adapt when out-of-range */
        agn_pushcomplex(L, tools_csgn(a, b)*sun_log((t6 + t8)*0.5 + t12), sun_asin(t14));
      }
      break;
    }
    default: {
      trydualfix(L, "arcsinh", 1);  /* FIXME: correct underlying dual function, that is fix the 3rd derivative */
      luaL_nonumorcmplxordual(L, 1, "arcsinh");
    }
  }
  return 1;
}


/* Returns the inverse hyperbolic cosine of the number x and returns a number. sun_acosh is a lot slower. */
static int mathB_arccosh (lua_State *L) {  /* 2.3.3, extended 2.32.3 */
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      /* log(x + tools_sign(x) * sqrt(x*x - 1))) */
      lua_pushnumber(L, acosh(agn_tonumber(L, 1)));
      break;
    }
    case LUA_TCOMPLEX: {
      /* ln(z + sqrt(-1 + z)*sqrt(1 + z)) */
      lua_Number a, b;
      agn_getcmplxparts(L, 1, &a, &b);
      if (b == 0 && a >= 1) {
        agn_pushcomplex(L, acosh(a), 0);
      } else {  /* do not change to long double as this will lead to out-of-domain errors */
        lua_Number t2, t6, t7, t9, t11, t13, t15, t9h, t11h, t16;
        t2 = tools_csgn(b, 1 - a);
        t6 = a*a;
        t7 = b*b;
        /* 6.6.11: We have round-off errors in Linux and Mac OS X, resulting in negative redicals, also when compiled
           for SSE2 on Windows (GCC switches -msse2 -mfpmath=sse). So do not optimise the following two statements. */
        t9 = sqrt(t6 + 2.0*a + 1.0 + t7);
        t11 = sqrt(t6 - 2.0*a + 1.0 + t7);
        t13 = sun_pow(0.5*(t9 + t11), 2.0, 1);  /* call sun_pow, do NOT optimise by changing to `x*x` ! */
        t15 = sqrt(t13 - 1.0);
        t9h = 0.5*t9;
        t11h = 0.5*t11;
        t16 = t9h - t11h;
        tools_adjust(t16, 1.0, AGN_EPSILON, -1);  /* 2.32.4 improvement, only adapt when out-of-range */
        agn_pushcomplex(L, -t2*tools_csgn(-b, a)*sun_log(t9h + t11h + t15), t2*sun_acos(t16));
      }
      break;
    }
    default: {
      trydualfix(L, "arccosh", 1);  /* FIXME: correct underlying dual function, that is fix the 3rd derivative */
      luaL_nonumorcmplxordual(L, 1, "arccosh");
    }
  }
  return 1;
}


static int mathB_arccoth (lua_State *L) {
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      /* 1/2*ln((x + 1)/(x - 1)) */
      lua_Number x, arg;
      x = agn_tonumber(L, 1);
      arg = (x + 1.0)/(x - 1.0);
      if (arg <= 1 || tools_isnan(arg) || !isfinite(arg))  /* patch 0.26.1 */
        lua_pushundefined(L);
      else
        lua_pushnumber(L, 0.5*sun_log(arg));
      break;
    }
    case LUA_TCOMPLEX: {
      /* do not change to long double as this will lead to out-of-domain errors */
      lua_Number re, im, t1, t2, t3, t6;
      agn_getcmplxparts(L, 1, &re, &im);  /* 2.17.6 */
      if (im == 0 && (re == 1 || re == -1))  /* patch 0.26.1 */
        lua_pushundefined(L);
      else {
        t1 = re + 1;
        t2 = t1*t1;
        t3 = im*im;
        t6 = tools_square(re - 1);  /* 2.17.7 optimisation */
        if (im != 0 || re <= 1) {  /* 0.33.1 fix */
          agn_pushcomplex(L, 0.25*sun_log((t2 + t3)/(t6 + t3)), PIO2 + 0.5*sun_atan2(im, t1) - 0.5*sun_atan2(-im, 1 - re));  /* 2.17.6 */
        } else {
          agn_pushcomplex(L, 0.25*sun_log((t2 + t3)/(t6 + t3)), 0);  /* 2.17.6 */
        }
      }
      break;
    }
    default: {
      trydualfix(L, "arccoth", 1);  /* FIXME: correct underlying dual function, that is fix the 3rd derivative */
      luaL_nonumorcmplxordual(L, 1, "arccoth");
    }
  }
  return 1;
}


/* = cos(x) + sin(x) = sqrt(2)*sin(x + Pi/4) */
static int agnB_cas (lua_State *L) {  /* 2.11.1, 20 % faster than the Agena implementation */
  if (agn_isnumber(L, 1)) {
    lua_Number x = agn_tonumber(L, 1);
    lua_pushnumber(L, SQRT2*sun_sin(x + PIO4));
  } else if (lua_iscomplex(L, 1)) {
    /* do not change to long double as this will lead to out-of-domain errors */
    lua_Number a, b, t1, t2, t4, t6;
    agn_getcmplxparts(L, 1, &a, &b);  /* 2.17.6 */
    sun_sincos(a, &t4, &t1);
    sun_sinhcosh(b, &t6, &t2);  /* 4.5.7 overflow tweak */
    agn_pushcomplex(L, t1*t2 + t4*t2, -t4*t6 + t1*t6);  /* 2.17.6 */
  } else {
    trydualfix(L, "cas", 1);
    luaL_nonumorcmplxordual(L, 1, "cas");
  }
  return 1;
}


static int agnB_csc (lua_State *L) {  /* Cosecant, 2.32.5 */
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      lua_pushnumber(L, luai_numcsc(agn_tonumber(L, 1)));  /* changed 3.13.4 */
      break;
    }
    case LUA_TCOMPLEX: {
      lua_Number a, b;
      agn_getcmplxparts(L, 1, &a, &b);
      if (b == 0) {  /* prevent round-off errors and get quicker result */
        agn_pushcomplex(L, luai_numcsc(a), 0);  /* fixed 3.13.5 */
      } else {
        /* do not change to long double as this will lead to out-of-domain errors */
        lua_Number t1, t2, t4, t5, t7, t8, t9, t10, t13;
        sun_sincos(a, &t1, &t7);
        sun_sinhcosh(b, &t9, &t2);  /* 4.5.7 overflow tweak */
        t4 = t1*t1;
        t5 = t2*t2;
        t8 = t7*t7;
        t10 = t9*t9;
        t13 = 1.0/(t4*t5 + t8*t10);
        agn_pushcomplex(L, t1*t2*t13, -t7*t9*t13);
      }
      break;
    }
    default: {
      trydualfix(L, "csc", 1);
      luaL_nonumorcmplxordual(L, 1, "csc");
    }
  }
  return 1;
}


/* Secant, returns the inverse cosine of the (complex) number x and returns a (complex) number. */
static int agnB_sec (lua_State *L) {  /* 2.32.5 */
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      lua_pushnumber(L, luai_numsec(agn_tonumber(L, 1)));  /* changed 3.13.4 */
      break;
    }
    case LUA_TCOMPLEX: {
      lua_Number a, b;
      agn_getcmplxparts(L, 1, &a, &b);
      if (b == 0) {  /* prevent round-off errors and get quicker result */
        agn_pushcomplex(L, luai_numsec(a), 0);  /* changed 3.13.4 */
      } else {
        /* do not change to long double as this will lead to out-of-domain errors */
        lua_Number t1, t2, t4, t5, t7, t8, t9, t10, t13;
        sun_sincos(a, &t7, &t1);
        sun_sinhcosh(b, &t9, &t2);  /* 4.5.7 overflow tweak */
        t4 = t1*t1;
        t5 = t2*t2;
        t8 = t7*t7;
        t10 = t9*t9;
        t13 = 1.0/(t4*t5 + t8*t10);
        agn_pushcomplex(L, t1*t2*t13, t7*t9*t13);
      }
      break;
    }
    default: {
      trydualfix(L, "sec", 1);
      luaL_nonumorcmplxordual(L, 1, "sec");
    }
  }
  return 1;
}


/* Hyperbolic Cotangent, returns the inverse hyperbolic tangent of the (complex) number x and returns a (complex) number. */
static int agnB_coth (lua_State *L) {  /* 2.32.5 */
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      lua_Number x = agn_tonumber(L, 1);
      lua_pushnumber(L, x == 0 ? AGN_NAN : 1.0/luai_numtanh(x));
      break;
    }
    case LUA_TCOMPLEX: {
      lua_Number a, b;
      agn_getcmplxparts(L, 1, &a, &b);
      if (b == 0) {  /* prevent round-off errors and get quicker result */
        if (a == 0) lua_pushundefined(L);
        else agn_pushcomplex(L, 1/luai_numtanh(a), 0);
      } else {
        /* do not change to long double as this will lead to out-of-domain errors */
        lua_Number t1, t2, t4, t5, t6, t7, t9, t11, t12, t14, t15, t20;
        sun_sincos(b, &t14, &t5);
        sun_sinhcosh(a, &t1, &t2);  /* 4.5.7 overflow tweak */
        t4 = t1*t1;
        t6 = t5*t5;
        t7 = t4+t6;
        t9 = t2*t2;
        t11 = t7*t7;
        t12 = 1.0/t11;
        t15 = t14*t14;
        t20 = 1.0/t7/(t4*t9*t12 + t15*t6*t12);
        agn_pushcomplex(L, t1*t2*t20, -t14*t5*t20);
      }
      break;
    }
    default: {
      trydualfix(L, "coth", 1);
      luaL_nonumorcmplxordual(L, 1, "coth");
    }
  }
  return 1;
}


/* Cotangent, 1/sun_tan(x) = cos(x)/sin(x) = -tan(Pi/2 + x); -tan(Pi/2 + x) is too imprecise with large negative and positive arguments;
   cos(x)/sin(x) has ulp < 2; 1/tan(x) has ulp < 1.5 (see https://stackoverflow.com/questions/3738384/stable-cotangent); 3.13.4 */
static int agnB_cot (lua_State *L) {
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      lua_Number x = agn_tonumber(L, 1);
      lua_pushnumber(L, luai_numcot(x));  /* changed 3.13.4 */
      break;
    }
    case LUA_TCOMPLEX: {
      lua_Number a, b;
      agn_getcmplxparts(L, 1, &a, &b);
      if (b == 0) {  /* prevent round-off errors and get quicker result */
        agn_pushcomplex(L, luai_numcot(a), 0);  /* changed 3.13.4 */
      } else {
        /* do not change to long double as this will lead to out-of-domain errors */
        lua_Number t1, t4, t5, t6, t8, co, coh;
        sun_sincos(a, &t1, &co);
        sun_sinhcosh(b, &t5, &coh);  /* 4.5.7 overflow tweak */
        t4 = t1*t1;
        t6 = t5*t5;
        t8 = 1.0/(t4 + t6);
        agn_pushcomplex(L, t1*co*t8, -t5*coh*t8);
      }
      break;
    }
    default: {
      trydualfix(L, "cot", 1);
      luaL_nonumorcmplxordual(L, 1, "cot");
    }
  }
  return 1;
}


/* Maple V Release 4:
Digits := 25; interface(prettyprint=0);
convert(series(tan(x)/x, x=0, 12), polynom);  # get rid of O() term
evalf(convert(", horner, x));
f := unapply(subs(x^2=z, "), z); */
static int mathB_tanc (lua_State *L) {  /* 2.8.3, 12 percent faster than the Agena version */
  if (agn_isnumber(L, 1)) {
    lua_Number x = agn_tonumber(L, 1);
    if (fabs(x) < 1e-4 && x != 0.0) {  /* 4.5.2 catastrophic cancellation fix, max. absolute error is -.214e-21,
       threshold reduced from 0.025 to 1e-4, 6.7.9. */
      lua_Number z = x*x;
      lua_pushnumber(L,
        1.0 + (0.3333333333333333333333333  + (
               0.1333333333333333333333333  + (
               0.05396825396825396825396825 + (
               0.02186948853615520282186949 +
               0.008863235529902196568863236*z)*z)*z)*z)*z);
    } else {
      lua_pushnumber(L, (x == 0.0) ? 1.0 : sun_tan(x)/x);
    }
  } else if (lua_iscomplex(L, 1)) {
#ifndef PROPCMPLX
    agn_Complex z = agn_tocomplex(L, 1);
    if (z == 0 + 0*I)
      agn_createcomplex(L, 1.0 + I*0.0);
    else
      agn_createcomplex(L, tools_ctan(z)/z);
#else
    /* do not change to long double as this will lead to out-of-domain errors */
    lua_Number a, b, t2, t3, t5, t8, t13, t14, t17, t19;
    agn_getcmplxparts(L, 1, &a, &b);  /* 2.17.6 */
    if (a == 0.0 && b == 0.0)
      agn_createcomplex(L, 1.0, 0.0);
    else {
      lua_Number si, coh;
      sun_sincos(a, &si, &t2);
      sun_sinhcosh(b, &t5, &coh);  /* 4.5.7 overflow tweak */
      t3 = si*t2;
      /* t5 = sun_sinh(b); */
      t8 = 1/sun_pytha(t2, t5);
      t13 = 1/sun_pytha(a, b);
      t14 = t8*a*t13;
      t17 = t5*coh;
      t19 = t8*b*t13;
      agn_createcomplex(L, t3*t14 + t17*t19, t17*t14 - t3*t19);
    }
#endif
  } else {
    trydualfix(L, "tanc", 1);
    luaL_nonumorcmplxordual(L, 1, "tanc");
  }
  return 1;
}


static int mathB_arctanh (lua_State *L) {  /* 2.14.10, extended 2.32.3; re-introduced 2.40.0 and tuned */
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {  /* 15 % faster than "0.5 * ln( (1 + x)/(1 - x) )" in Agena. */
      lua_pushnumber(L, sun_atanh(agn_tonumber(L, 1)));
      break;
    }
    case LUA_TCOMPLEX: {  /* as fast as "0.5 * ln( (1 + x)/(1 - x) )" in Agena. */
      /* arctanh := proc(x :: {number, complex}) is
            return 0.5 * ln( (1 + x)/(1 - x) )
         end; */
      /* do not change to long double as this will lead to out-of-domain errors */
      lua_Number a, b, im, t1, t2, t3, t4, t6;
      agn_getcmplxparts(L, 1, &a, &b);
      if (b == 0 && fabs(a) <= 1) {
        agn_pushcomplex(L, sun_atanh(a), 0);
      } else {
        t1 = a + 1.0;
        t2 = t1*t1;
        t3 = b*b;
        t4 = a - 1.0;
        t6 = t4*t4;
        im = 0.5*(sun_atan2(b, t1) - sun_atan2(-b, 1.0 - a));
        agn_pushcomplex(L, 0.25*sun_log((t2 + t3)/(t6 + t3)), (a > 0 && b == 0) ? -im : im);
      }
      break;
    }
    default: {
      trydualfix(L, "arctanh", 1);
      luaL_nonumorcmplxordual(L, 1, "arctanh");
    }
  }
  return 1;
}


static int mathB_arcsech (lua_State *L) {  /* re-introduced 2.40.0 and tuned */
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      lua_Number x = agn_tonumber(L, 1);
      /* 6.6.8 change, Gemini AI on using sun_acsoh:
         Precision: High-quality implementations of acoshl are optimized for precision across the entire range.
         Boundary Handling: acoshl is specifically designed to handle the behavior near its domain limit (). */
      /* invx = 1/x;
         lua_pushnumber(L, x <= 0 || x > 1 ? AGN_NAN : sun_log(invx + sqrt(invx - 1)*sqrt(invx + 1))); */
      lua_pushnumber(L, x <= 0 || x > 1 ? AGN_NAN : sun_acosh(1.0/x));
      break;
    }
    case LUA_TCOMPLEX: {
      /* arcsech := proc(z :: {number, complex}) is
            z := recip z;
            return ln(z + sqrt(z - 1)*sqrt(z + 1))  # do not simplify square roots !
         end; */
      lua_Number a, b;
      agn_getcmplxparts(L, 1, &a, &b);
      if (b == 0 && a > 0 && a <= 1) {  /* prevent round-off errors and get quicker result */
        /* lua_Number invx;
        invx = 1/a;
        agn_pushcomplex(L, sun_log(invx + sqrt(invx - 1)*sqrt(invx + 1)), 0); */
        agn_pushcomplex(L, sun_acosh(1.0/a), 0);  /* 6.6.8 change */
      } else {
        /* do not change to long double as this will lead to out-of-domain errors */
        lua_Number t1, t2, t4, t8, t10, t12, t13, t15, t17, t19, t21, t23, t25, t30, t31;
        t1 = a*a;
        t2 = b*b;
        t4 = tools_csgn(-b, -a + t1 + t2);
        t8 = t1 + t2;
        t10 = a/t8;
        t12 = tools_square(t10 + 1.0);
        t13 = t8*t8;
        t15 = t2/t13;
        t17 = sqrt(t12 + t15);
        t19 = tools_square(t10 - 1.0);
        t21 = sqrt(t19 + t15);
        t23 = tools_square(0.5*(t17 + t21));
        tools_adjust(t23, 1.0, AGN_EPSILON, 0);
        t25 = sqrt(t23 - 1.0);
        t30 = 0.5*(t17 + t21) + t25;
        t31 = 0.5*(t17 - t21);
        tools_adjust(t31, 1, AGN_EPSILON, 0);
        agn_pushcomplex(L, -t4*tools_csgn(b, a)*sun_log(t30), t4*sun_acos(t31));
      }
      break;
    }
    default: {
      trydualfix(L, "arcsech", 1);  /* FIXME: correct underlying dual function, that is fix the 3rd derivative */
      luaL_nonumorcmplxordual(L, 1, "arcsech");
    }
  }
  return 1;
}


static int mathB_arccsc (lua_State *L) {  /* re-introduced 2.40.0 and tuned */
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      lua_Number x = agn_tonumber(L, 1);
      lua_pushnumber(L, (x > -1 && x < 1) ? AGN_NAN : sun_asin(1/x));
      break;
    }
    case LUA_TCOMPLEX: {
      /* arccsc := proc(z :: {number, complex}) is
            return arcsin recip z
         end; */
      lua_Number a, b;
      agn_getcmplxparts(L, 1, &a, &b);
      if (b == 0 && (a <= -1 || a >= 1)) {  /* prevent round-off errors and get quicker result */
        agn_pushcomplex(L, sun_asin(1/a), 0);
      } else {
        /* do not change to long double as this will lead to out-of-domain errors */
        lua_Number t3, t5, t7, t8, t10, t12, t14, t16, t17, t18, t21, t27, t29;
        t3 = sun_pytha(a, b);  /* 4.5.2 precision tweak */
        t5 = 1/t3*a;
        t7 = tools_square(t5 + 1.0);
        t8 = t3*t3;
        t10 = 1/t8*(b*b);
        t12 = sqrt(t7 + t10);
        t14 = tools_square(t5 - 1.0);
        t16 = sqrt(t14 + t10);
        t17 = 0.5*(t12 - t16);
        tools_adjust(t17, 1.0, AGN_EPSILON, 0);
        t18 = sun_asin(t17);
        t21 = tools_csgn(-b, -a);
        t27 = sqrt(0.25*tools_square(t12 + t16) - 1.0);
        t29 = sun_log(0.5*(t12 + t16) + t27);
        agn_pushcomplex(L, t18, t21*t29);
      }
      break;
    }
    default: {
      trydualfix(L, "arccsc", 1);  /* FIXME: correct underlying dual function, that is fix the 3rd derivative */
      luaL_nonumorcmplxordual(L, 1, "arccsc");
    }
  }
  return 1;
}


/* Maple 7:
> restart;
> with(codegen, C):
> assume(a, real, b, real):
> z := a + I*b:
> C(evalc(arccsch(z)), optimized); */

static int mathB_arccsch (lua_State *L) {  /* 2.40.0 */
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      lua_Number x = agn_tonumber(L, 1);
      /* lua_pushnumber(L, (x == 0) ? AGN_NAN : sun_log(1/x + sqrt(1/(x*x) + 1))); */
      lua_pushnumber(L, (x == 0.0) ? AGN_NAN : sun_asinh(1.0L/x));  /* 6.6.9 improvement */
      break;
    }
    case LUA_TCOMPLEX: {
      /* arccsch := proc(x :: {number, complex}) is
            return ln(recip x + sqrt(recip square x + 1))  # = I*arcsin(recip(I*x))
         end; */
      lua_Number a, b;
      agn_getcmplxparts(L, 1, &a, &b);
      if (b == 0 && a != 0) {  /* prevent round-off errors and get quicker result */
        agn_pushcomplex(L, sun_log(1/a + sqrt(1/(a*a) + 1)), 0);
      } else {
        /* do not change to long double as this will lead to out-of-domain errors */
        lua_Number t3, t6, t7, t9, t11, t13, t15, t18, t19, t20, t21, t25, t26, t27, t28, t30;
        t3 = tools_csgn(a, -b);
        t6 = sun_pytha(a, b);  /* 4.5.2 precision tweak */
        t7 = t6*t6;
        t9 = (a*a)/t7;
        t11 = b/t6;
        t13 = tools_square(-t11 + 1.0);
        t15 = sqrt(t9 + t13);
        t18 = tools_square(-t11 - 1.0);
        t19 = t9 + t18;
        /* tools_adjust(t19, 0.0, AGN_EPSILON, -1); */
        t20 = sqrt(t19);
        t21 = 0.25*tools_square(t15 + t20) - 1.0;
        tools_adjust(t21, 0.0, AGN_EPSILON, -1);
        t25 = sqrt(t21);
        t26 = 0.5*t15 + 0.5*t20 + t25;
        tools_adjust(t26, 0.0, AGN_EPSILON, 0);
        t27 = sun_log(t26);
        t28 = 0.5*t15 - 0.5*t20;
        tools_adjust(t28, 1.0, AGN_EPSILON, 0);
        t30 = sun_asin(t28);
        agn_pushcomplex(L, t3*t27, t30);
      }
      break;
    }
    default: {
      trydualfix(L, "arccsch", 1);  /* FIXME: correct underlying dual function, that is fix the 3rd derivative */
      luaL_nonumorcmplxordual(L, 1, "arccsch");
    }
  }
  return 1;
}


static int mathB_argument (lua_State *L) {
  switch (lua_type(L, 1)) {
    case LUA_TCOMPLEX: {
      lua_Number a, b;
      agn_getcmplxparts(L, 1, &a, &b);  /* 7.1.4 change */
      lua_pushnumber(L, sun_atan2(b, a));
      break;
    }
    case LUA_TNUMBER: {
      lua_pushnumber(L, (agn_tonumber(L, 1) < 0) ? PI : 0);
      break;  /* 0.27.0 patch */
    }
    default:
      luaL_nonumorcmplx(L, 1, "argument");
  }
  return 1;
}


static INLINE lua_Number real_root (lua_Number x, int n) {  /* new 7.3.8 */
  if (x == 0 && n < 0) return AGN_NAN;
  if (x >= 0) return n == 3 ? sun_cbrt(x) : sun_pow(x, 1.0/n, 0);  /* 7.3.8 accuracy tweak for n=3 */
  if (luai_numiseven(n)) return AGN_NAN;  /* x < 0 and n is even ? */
  return n == 3 ? -sun_cbrt(-x) : -sun_pow(-x, 1.0/n, 0);  /* 7.3.8 accuracy tweak for n=3 */
}

/* Returns the non-principal n-th root, equivalent to Maple's `surd` */
static int mathB_root (lua_State *L) {
  lua_Number n;
  luaL_checkany(L, 1);  /* 2.34.6 fix */
  n = agnL_optinteger(L, 2, 2);  /* 2.10.4 */
  if (n == 0) {  /* 0.33.2 */
    lua_pushundefined(L);
    return 1;
  }
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      lua_pushnumber(L, real_root(agn_tonumber(L, 1), n));
      break;
    }
    case LUA_TCOMPLEX: {  /* completely rewritten 0.33.2 */
      /* be careful when changing to long double as this might lead to out-of-domain errors,
         depending on the long double functions used */
      lua_Number a, b;
      long double k, t1, t3, t7, t9, t11, t13, t14, t17, t18;
      agn_getcmplxparts(L, 1, &a, &b);  /* 2.17.6 */
      if (b == 0.0 && a >= 0.0) {  /* 7.3.8 tweak */
        agn_pushcomplex(L, real_root(a, n), 0.0);
      } else {  /* 7.3.8 accuracy tweak */
        k = rint(sun_atan2l(b, a)*(n - 1.0L)/M_PI2ld);
        t1 = 1/n;
        t3 = tools_pythal(a, b);  /* 4.5.2 precision tweak */
        t7 = tools_expl(t1*tools_logl(t3)*0.5L);
        t9 = t1*sun_atan2l(b, a);
        sun_sincosl(t9, &t17, &t11);  /* 2.24.2 tweak */
        t11 *= t7;
        t13 = k*M_PIld*t1;  /* DP1 = Pi */
        t17 *= t7;
        sun_sincosl(2.0L*t13, &t18, &t14);  /* 2.24.2 tweak */
        agn_pushcomplex(L, t11*t14 - t17*t18, t17*t14 + t11*t18);  /* 2.17.6 */
      }
      break;
    }
    default:
      luaL_nonumorcmplx(L, 1, "root");
  }
  return 1;
}


/*
> restart;
> assume(n, posint);
> f := (a+I*b)^(1/n);

                                       / 1  \
                                       |----|
                                       \ n~ /
                         f := (a + I b)

> evalc(f);

            2    2
        ln(a  + b )      arctan(b, a)
exp(1/2 -----------) cos(------------)
            n~                n~

                     2    2
                 ln(a  + b )      arctan(b, a)
     + I exp(1/2 -----------) sin(------------)
                     n~                n~

> readlib(C);

                           proc()  ...  end

> C(evalc(f), optimized);
      t1 = 1/n;
      t2 = a*a;
      t3 = b*b;
      t7 = exp(t1*log(t2+t3)/2);
      t9 = t1*atan2(b,a);
      t14 = t7*cos(t9)+sqrt(-1.0)*t7*sin(t9);
*/

static INLINE lua_Number real_proot (lua_Number x, int n) {  /* new 7.3.8 */
  if (x >= 0) return n == 3 ? sun_cbrt(x) : sun_pow(x, 1.0/n, 0);  /* 7.3.8 accuracy tweak */
  if (luai_numiseven(n)) return AGN_NAN;  /* x < 0 and n is even ? */
  return n == 3 ? -sun_cbrt(-x) : -sun_pow(-x, 1.0/n, 0);  /* 7.3.8 accuracy tweak */
}

static int mathB_proot (lua_State *L) {  /* computer the principal root, equivalent to Maple's `root`, 1.12.7 */
  lua_Number n;
  luaL_checkany(L, 1);  /* 2.34.6 fix */
  n = agn_checkposint(L, 2);  /* 2.13.0 */
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      lua_pushnumber(L, real_proot(agn_tonumber(L, 1), n));
      break;
    }
    case LUA_TCOMPLEX: {
      /* be careful when changing to long double as this might lead to out-of-domain errors,
         depending on the long double functions used */
      lua_Number a, b;
      long double t1, t3, t7, t9, si, co;
      agn_getcmplxparts(L, 1, &a, &b);  /* 2.17.6 */
      if (b == 0.0 && a >= 0) {  /* 7.3.8 accuracy tweak */
        agn_pushcomplex(L, real_proot(a, n), 0.0);
      } else {  /* 7.3.8 accuracy tweak */
        t1 = 1.0L/n;
        t3 = tools_pythal(a, b);  /* 4.5.2 precision tweak */
        if (t3 == 0.0L) {
          agn_pushcomplex(L, 0.0, 0.0);
        } else {  /* 2.40.2 change to use sun_log instead of GCC's log */
          t7 = tools_expl(t1*tools_logl(t3)*0.5L);  /* 2.40.2 tweak */
          t9 = t1*sun_atan2l(b, a);
          sun_sincosl(t9, &si, &co);
          agn_pushcomplex(L, t7*co, t7*si);  /* 2.17.6, 2.24.2 tweak */
        }
      }
      break;
    }
    default:
      luaL_nonumorcmplx(L, 1, "proot");
  }
  return 1;
}


/*
> readlib(C);

> f := evalc((a+I*b)^(1/3));

       2    2 1/6
f := (a  + b )    cos(1/3 arctan(b, a))

           2    2 1/6
     + I (a  + b )    sin(1/3 arctan(b, a))

> C(f, optimized);
      t1 = a*a;
      t2 = b*b;
      t4 = pow(t1+t2,1.0/6.0);
      t5 = atan2(b,a);
      t10 = t4*cos(t5/3)+sqrt(-1.0)*t4*sin(t5/3);
*/

static int agnB_cbrt (lua_State *L) {  /* 1.12.7 */
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      lua_Number x, r;
      x = agn_tonumber(L, 1);
      r = sun_cbrt(x);  /* 2.11.1 tuning */
      lua_pushnumber(L, r);
      break;
    }
    case LUA_TCOMPLEX: {  /* completely rewritten 0.33.2 */
      /* be careful when changing to long double as this might lead to out-of-domain errors,
         depending on the long double functions used */
      lua_Number a, b;
      agn_getcmplxparts(L, 1, &a, &b);  /* 2.17.6 */
      if (b == 0.0 && a >= 0.0) {  /* 7.3.8 accuracy tweak */
        agn_pushcomplex(L, sun_cbrt(a), 0.0);
      } else {  /* 4.5.2/7.3.8 accuracy tweak */
        long double t2, t4, t5, si, co;
        t2 = tools_pythal(a, b);
        t4 = tools_powl(t2, 1.0L/6.0L);
        t5 = sun_atan2l(b, a);
        sun_sincosl(t5/3.0L, &si, &co);
        agn_pushcomplex(L, t4*co, t4*si);  /* 2.17.6, 2.24.2 tweak */
      }
      break;
    }
    default: {
      trydualfix(L, "cbrt", 1);  /* FIXME: correct underlying dual function, that is fix the 3rd derivative */
      luaL_nonumorcmplxordual(L, 1, "cbrt");
    }
  }
  return 1;
}


static int agnB_cosc (lua_State *L) {  /* 2.11.0 RC3 */
  if (lua_gettop(L) == 1) {  /* old mode: cosc(x) = cos(x)/x */
    switch (lua_type(L, 1)) {
      case LUA_TNUMBER: {
        lua_Number x;
        x = agn_tonumber(L, 1);
        lua_pushnumber(L, x == 0 ? AGN_NAN : sun_cos(x)/x);  /* 4.5.2 change, see below */
        break;
      }
      case LUA_TCOMPLEX: {
#ifndef PROPCMPLX
        agn_Complex x;
        x = agn_tocomplex(L, 1);
        if (x == 0 + I*0) {
          agn_createcomplex(L, AGN_NAN);
        } else {
          agn_Complex r;
#if (!(defined(__SOLARIS)))
          lua_Number sia, coa, sihb, cohb;
          sun_sincos(__real__ x, &sia, &coa);
          sun_sinhcosh(__imag__ x, &sihb, &cohb);  /* 2.17.7 optimisation, 4.5.7 overflow tweak */
          __real__ r = coa*cohb;
          __imag__ r = -sia*sihb;
          r = tools_cdiv(r, x);
#else
          r = (x == 0.0) ? 0 : tools_ccos(x)/x;  /* 3.4.5 precautionary fix due to faulty GCC ccos implementations; 4.5.2 division by zero fix */
#endif
          if (cimag(r) == -0) r = creal(r) + 0*I;
          agn_createcomplex(L, r);
        }
#else
        lua_Number a, b;
        agn_getcmplxparts(L, 1, &a, &b);  /* 2.17.6 */
        if (a == 0 && b == 0) {  /* 4.5.2 change, see above */
          agn_createcomplex(L, AGN_NAN, 0);  /* 4.5.3 change */
        } else {
          lua_Number i, t3, t7, t8, t12, t13, sia, coa, sihb, cohb;
          sun_sincos(a, &sia, &coa);    /* 2.17.7 optimisation */
          sun_sinhcosh(b, &sihb, &cohb);  /* ditto, 2.21.7 optimisation, 4.5.7 overflow tweak */
          t3 = coa*cohb;
          t7 = 1/sun_pytha(a, b);  /* 4.5.2 precision tweak */
          t8 = a*t7;
          t12 = sia*sihb;
          t13 = b*t7;
          i = -t12*t8 - t3*t13;
          if (i == -0) i = 0.0;
          agn_createcomplex(L, t3*t8 - t12*t13, i);  /* 2.11.0 RC3 fix */
        }
#endif
        break;
      }
      default: {
        trydualfix(L, "cosc", 1);  /* FIXME: correct underlying dual function, that is fix the 3rd derivative */
        luaL_nonumorcmplxordual(L, 1, "cosc");
      }
    }
  } else {  /* 5.4.2 cosc(x) = diff(sinc(x), x) = cos(x)*x - sin(x)/(x^2), with cosc(0) = 0. */
    switch (lua_type(L, 1)) {
      case LUA_TNUMBER: {
        lua_Number si, co, x;
        x = agn_tonumber(L, 1);
        sun_sincos(x, &si, &co);
        lua_pushnumber(L, (x == 0) ? 0 : (co*x - si)/(x*x));
        break;
      }
      case LUA_TCOMPLEX: {
        lua_Number a, b, t1, t2, t3, t5, t6, t7, t10, t11, t12, t13, t15, t16, t18, t23, t25;
        agn_getcmplxparts(L, 1, &a, &b);
        sun_sincos(a, &t5, &t1);
        sun_sinhcosh(b, &t6, &t2);  /* 4.5.7 overflow tweak */
        t3 = t1*t2;
        t7 = t5*t6;
        t10 = t3*a + t7*b - t5*t2;
        t11 = a*a;
        t12 = b*b;
        t13 = t11 - t12;
        t15 = t13*t13;
        t16 = t15 + 4.0*t11*t12;
        if (t16 == 0.0) {
          agn_pushcomplex(L, 0.0, 0.0);
        } else {
          t18 = 1/t16;
          t23 = -t7*a + t3*b - t1*t6;
          t25 = b*t18;
          agn_pushcomplex(L, t10*t13*t18 + 2.0*t23*a*t25, t23*t13*t18 - 2.0*t10*a*t25);
        }
        break;
      }
      default:
        luaL_nonumorcmplx(L, 1, "cosc");
    }
  }
  return 1;
}


static int agnB_drem (lua_State *L) {  /* 2.0.0 R5 */
  lua_pushnumber(L, remainder(agn_checknumber(L, 1), agn_checknumber(L, 2)));
  return 1;
}


/* Computes both the integer quotient and the integer remainder of the number a divided by the number b and returns them.
   If a or b are not integers, the function returns `undefined´ twice. 10.04.2013 */
static int mathB_iqr (lua_State *L) {  /* 1.10.6; rewritten and tuned by 33 % 2.29.5, simplified 2.31.5 */
  lua_Number rem, a, b;
  a = agn_checknumber(L, 1);
  b = agn_checknumber(L, 2);
  lua_pushnumber(L, sun_iqr(a, b, &rem));
  lua_pushnumber(L, rem);
  return 2;
}


static int mathB_iquo (lua_State *L) {  /* 3.17.5, Maple's iquo */
  lua_pushnumber(L, sun_quotient(agn_checkinteger(L, 1), agn_checkinteger(L, 2)));
  return 1;
}


static int mathB_irem (lua_State *L) {  /* 3.17.5, Maple's irem */
  lua_Number rem;
  sun_iqr(agn_checkinteger(L, 1), agn_checkinteger(L, 2), &rem);
  lua_pushnumber(L, rem);
  return 1;
}


static int mathB_modf (lua_State *L) {  /* extended to the complex domain 2.30.3 */
  lua_Number ip, fp;
  int type = lua_type(L, 1);
  luaL_checkstack(L, 2 + (type == LUA_TCOMPLEX), "not enough stack space");  /* 4.7.1 fix */
  switch (type) {
    case LUA_TNUMBER: {
      fp = sun_modf(agn_tonumber(L, 1), &ip);  /* 2.11.0 improvement: 20 % faster */
      lua_pushnumber(L, ip);
      lua_pushnumber(L, fp);
      return 2;
    }
    case LUA_TCOMPLEX: {  /* added 2.30.3 */
      lua_Number re, im;
      agn_getcmplxparts(L, 1, &re, &im);
      fp = sun_modf(re, &ip);
      lua_pushnumber(L, ip);
      lua_pushnumber(L, fp);
      fp = sun_modf(im, &ip);
      lua_pushnumber(L, ip);
      lua_pushnumber(L, fp);
      return 4;
    }
    default:
      luaL_nonumorcmplx(L, 1, "modf");
  }
  return 0;  /* cannot happen */
}


/* Maple's `mod`/modp function/mod op, developed with Gemini AI, 6.4.4 */
static int mathB_modp (lua_State *L) {
  lua_Integer a, b;
  a = agn_checkinteger(L, 1);
  b = agn_checkinteger(L, 2);  /* modulus */
  if (b == 0) {
    lua_pushundefined(L);
  } else {
    lua_Number q, r;
    q = ceil((lua_Number)a/(lua_Number)b); //sun_ceil((lua_Number)a/(lua_Number)b);
    r = a - q*b;
    lua_pushnumber(L, r + (r < 0)*fabs(b));
  }
  return 1;
}


static int mathB_frexp (lua_State *L) {  /* extended to the complex domain 2.30.3 */
  int e, type;
  type = lua_type(L, 1);
  luaL_checkstack(L, 2 + (type == LUA_TCOMPLEX), "not enough stack space");  /* 4.7.1 fix */
  switch (type) {
    case LUA_TNUMBER: {
      lua_pushnumber(L, sun_frexp(agn_tonumber(L, 1), &e));  /* 2.11.1 tuning, 16 % faster */
      lua_pushnumber(L, e);
      return 2;
    }
    case LUA_TCOMPLEX: {  /* added 2.30.3 */
      lua_Number re, im;
      agn_getcmplxparts(L, 1, &re, &im);
      lua_pushnumber(L, sun_frexp(re, &e));
      lua_pushnumber(L, e);
      lua_pushnumber(L, sun_frexp(im, &e));
      lua_pushnumber(L, e);
      return 4;
    }
    default:
      luaL_nonumorcmplx(L, 1, "frexp");
  }
  return 0;  /* cannot happen */
}


static int mathB_ldexp (lua_State *L) {
  lua_pushnumber(L, sun_ldexp(agn_checknumber(L, 1), agnL_checkint(L, 2)));  /* 2.11.1 tuning, 38 % faster than GCC's ldexp */
  return 1;
}


/* Taken from: https://stackoverflow.com/questions/29537685/how-to-get-the-mantissa-and-exponent-of-a-double-in-a-power-of-10:

"The thing about frexp is that if you are using IEEE 754 binary floating-point (or PDP-11 floating-point), it simply reveals the
representation of the floating-point value. As such, the decomposition is exact, and it is bijective: you can reconstitute the
argument from the significand and exponent you have obtained.

No equivalent reason exists for highlighting a power-of-ten decomposition function. Actually, defining such a function would pose a
number of technical hurdles: Not all powers of ten are representable exactly as double: 0.1 and 10^23 aren't. For the
base-ten-significand part, do you want a value that arrives close to the argument when multiplied by 10^e, or by the
double-precision approximation of 10^e? Some floating-point values may have several equally valid decompositions. Some
floating-point values may have no decomposition.

If these accuracy and canonicity aspects do not matter to you, use e = floor(log10(abs(x))) for the base-10-exponent (or ceil for a
convention more like the PDP-11-style frexp), and x / pow(10, e) for the base-10-significand. If it matters to you that the
significand is between 1 and 10, you had better force this property by clamping."

Formula taken from https://stackoverflow.com/questions/29582919/find-exponent-and-mantissa-of-a-double-to-the-base-10 */

static FORCE_INLINE double frexp10 (double x, int *ex) {
  *ex = (x == 0) ? 0 : 1 + sun_ilog10(fabs(x));
  return x*luai_numipow(10, -(*ex));  /* result is in the range [0, 1); if x = 0, preserve the sign */
}

static int mathB_frexp10 (lua_State *L) {  /* added 2.30.3 */
  int e, type;
  type = lua_type(L, 1);
  luaL_checkstack(L, 2 + (type == LUA_TCOMPLEX), "not enough stack space");  /* 4.7.1 fix */
  switch (type) {
    case LUA_TNUMBER: {
      lua_pushnumber(L, frexp10(agn_tonumber(L, 1), &e));
      lua_pushnumber(L, e);
      return 2;
    }
    case LUA_TCOMPLEX: {
      lua_Number re, im;
      agn_getcmplxparts(L, 1, &re, &im);
      lua_pushnumber(L, frexp10(re, &e));
      lua_pushnumber(L, e);
      lua_pushnumber(L, frexp10(im, &e));
      lua_pushnumber(L, e);
      return 4;
    }
    default:
      luaL_nonumorcmplx(L, 1, "frexp10");
  }
  return 0;  /* cannot happen */
}


static int mathB_exp2 (lua_State *L) {  /* needed for regression tests, 2.29.1, tuned by 22 % 2.32.2, extended 2.32.3 */
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      lua_pushnumber(L, tools_exp2(agn_tonumber(L, 1)));
      break;
    }
    case LUA_TCOMPLEX: {
      lua_Number re, im, t3, t4, si, co;
      agn_getcmplxparts(L, 1, &re, &im);
      t3 = sun_exp(re*LN2);
      t4 = im*LN2;
      sun_sincos(t4, &si, &co);
      agn_pushcomplex(L, t3*co, t3*si);
      break;
    }
    default: {
      trydualfix(L, "exp2", 1);
      luaL_nonumorcmplxordual(L, 1, "exp2");
    }
  }
  return 1;
}


static int mathB_exp10 (lua_State *L) {  /* 2.32.2, extended 2.32.3 */
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      lua_pushnumber(L, tools_exp10(agn_tonumber(L, 1)));
      break;
    }
    case LUA_TCOMPLEX: {
      lua_Number re, im, t3, t4, si, co;
      agn_getcmplxparts(L, 1, &re, &im);
      t3 = sun_exp(re*LN10);
      t4 = im*LN10;
      sun_sincos(t4, &si, &co);
      agn_pushcomplex(L, t3*co, t3*si);
      break;
    }
    default: {
      trydualfix(L, "exp10", 1);
      luaL_nonumorcmplxordual(L, 1, "exp10");
    }
  }
  return 1;
}


/* mathB_round: rounds real or complex value x to the d-th digit; patched Dec 22, 2007 for negative values,
   improved 2.9.8, extended 2.11.0;
   15 percent faster than the former implementation:
   if (x < 0) lua_pushnumber(L, ceil(pow(10, d)*x - 0.5) * pow(10, (-d)));
   else lua_pushnumber(L, floor(pow(10, d)*x + 0.5) * pow(10, (-d))); */
static int mathB_round (lua_State *L) {
  lua_Number d;
  d = agnL_optinteger(L, 2, 0);
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      lua_Number x;
      x = agn_tonumber(L, 1);
      if (d == 0)  /* round half up to an integer ?  2.11.0 improvement: 65 percent faster */
        lua_pushnumber(L, sun_round(x));
      else
        lua_pushnumber(L, tools_roundf(x, d, 4));
      break;
    }
    case LUA_TCOMPLEX: {  /* new 2.11.0 */
      lua_Number a, b;
      agn_getcmplxparts(L, 1, &a, &b);  /* 2.17.6 */
      if (d == 0) {  /* round half up to an integer ?  2.11.0 improvement: 65 percent faster */
        a = sun_round(a); b = sun_round(b);
      } else {
        a = tools_roundf(a, d, 4); b = tools_roundf(b, d, 4);
      }
      agn_pushcomplex(L, a, b);  /* 2.17.6 */
      break;
    }
    default:
      luaL_nonumorcmplx(L, 1, "round");
  }
  return 1;
}


/* Rounds upwards towards the nearest integer; faster than the C API implementation
   0.10.0 as of April 20, 2008; patched 0.26.1, August 11, 2009; improved May 01, 2014
   ceil := << x :: {number, complex} -> -entier(-x) >>; */
static int agnB_ceil (lua_State *L) {  /* C implementation 2.11.1 */
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      lua_pushnumber(L, sun_ceil(agn_checknumber(L, 1)));
      break;
    }
    case LUA_TCOMPLEX: {  /* simplified 2.30.4 */
      lua_Number aa, bb, re, im, X, Y, fre, fim;
      agn_getcmplxparts(L, 1, &re, &im);
      re = -re; im = -im;
      fre = sun_floor(re); fim = sun_floor(im);  /* tuned by 15 %, 2.34.8 */
      aa = re - fre;
      bb = im - fim;
      if ((aa + bb) < 1) {
        X = 0.0; Y = 0.0; }
      else if ((aa + bb) >= 1 && (aa >= bb)) {
        X = 1.0; Y = 0.0; }
      else {
        X = 0.0; Y = 1.0; }
      agn_pushcomplex(L, -(fre + X), -(fim + Y));
      break;
    }
    default:
      luaL_nonumorcmplx(L, 1, "ceil");
  }
  return 1;
}


/* Rounds upwards towards the nearest integer; works like entier. 2.34.8. */
static int agnB_floor (lua_State *L) {  /*  */
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      lua_pushnumber(L, sun_floor(agn_checknumber(L, 1)));
      break;
    }
    case LUA_TCOMPLEX: {
      lua_Number aa, bb, re, im, X, Y, fre, fim;
      agn_getcmplxparts(L, 1, &re, &im);
      fre = sun_floor(re); fim = sun_floor(im);
      aa = re - fre;
      bb = im - fim;
      if ((aa + bb) < 1) {
        X = 0.0; Y = 0.0; }
      else if ((aa + bb) >= 1 && (aa >= bb)) {
        X = 1.0; Y = 0.0; }
      else {
        X = 0.0; Y = 1.0; }
      agn_pushcomplex(L, fre + X, fim + Y);
      break;
    }
    default:
      luaL_nonumorcmplx(L, 1, "floor");
  }
  return 1;
}


static int mathB_mdf (lua_State *L) {  /* always rounds up to the d-th digit, 2.2.5, fixed 2.9.8, extended 2.30.3 */
  int d = agnL_optinteger(L, 2, 2);  /* 2.9.8 improvement */
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      lua_pushnumber(L, tools_roundf(agn_tonumber(L, 1), d, 0));
      break;
    }
    case LUA_TCOMPLEX: {  /* added 2.30.3 */
      lua_Number re, im;
      agn_getcmplxparts(L, 1, &re, &im);
      agn_pushcomplex(L, tools_roundf(re, d, 0), tools_roundf(im, d, 0));
      break;
    }
    default:
      luaL_nonumorcmplx(L, 1, "mdf");
  }
  return 1;
}


static int mathB_xdf (lua_State *L) {  /* always rounds down to the d-th digit, same as truncating floats, 2.2.5, fixed 2.9.8;
  extended 2.30.3 */
  int d = agnL_optinteger(L, 2, 2);  /* 2.9.8 improvement */
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      lua_pushnumber(L, tools_roundf(agn_tonumber(L, 1), d, 9));
      break;
    }
    case LUA_TCOMPLEX: {  /* added 2.30.3 */
      lua_Number re, im;
      agn_getcmplxparts(L, 1, &re, &im);
      agn_pushcomplex(L, tools_roundf(re, d, 9), tools_roundf(im, d, 9));
      break;
    }
    default:
      luaL_nonumorcmplx(L, 1, "xdf");
  }
  return 1;
}


static int mathB_hypot (lua_State *L) {  /* = sqrt(a^2 + b^2) */
  if (lua_isnumber(L, 1) && lua_isnumber(L, 2)) {
    lua_pushnumber(L, sun_hypot(agn_tonumber(L, 1), agn_tonumber(L, 2)));  /* changed 2.14.13 to portable sun_hypot */
  } else if (lua_iscomplex(L, 1) && lua_iscomplex(L, 2)) {  /* added 2.30.3 */
    double a, b, c, d;
    long double t1, t2, t5, t7;
    agn_getcmplxparts(L, 1, &a, &b);
    agn_getcmplxparts(L, 2, &c, &d);
    if (b == 0 && d == 0) {  /* 2.31.0 fix to avoid undefined in OS/2 and DOS, and to speed up */
      agn_pushcomplex(L, sun_hypot(a, c), 0);
    } else if (a == 0 && c == 0) {
      /* t1 = b*b;
      t2 = d*d;
      t4 = sqrtl(t1 + t2); */
      agn_pushcomplex(L, 0, sun_hypot(b, d));  /* 3.16.4 fix against overflow and underflow */
    } else {
      /* = x^2 + y^2; simplified to prevent round-off errors 2.30.4 */
      /* t1 = a*a - b*b + c*c - d*d; */
      t1 = sun_pytha(a, c) - sun_pytha(b, d);  /* 3.16.4 fix against overflow and underflow */
      t2 = 2.0L*(a*b + c*d);
      /* sqrt */
      /* t3 = t1*t1;
      t4 = t2*t2; */
      t5 = 2.0L*t1;
      /* t6 = t3 + t4; */
      /* t6 = tools_pythal(t1, t2); */ /* 3.16.4 fix against overflow and underflow */
      /* t7 = 2.0L*sqrtl(t6); */
      t7 = 2.0L*tools_hypotl(t1, t2);
      agn_pushcomplex(L,
        (double)(0.5L*sqrtl(t7 + t5)),
        (double)(0.5L*tools_csgnl(t2, -t1)*sqrtl(t7 - t5)));
    }
  } else if ((lua_iscomplex(L, 1) && lua_isnumber(L, 2)) || (lua_isnumber(L, 1) && lua_iscomplex(L, 2))) {  /* added 2.30.3 */
    double a, b, c;
    long double t1, t2, t3, t4, t5;
    int isnum = lua_isnumber(L, 2);
    c = agn_tonumber(L, 1 + isnum);
    agn_getcmplxparts(L, 1 + !isnum, &a, &b);
    if (b == 0) {
      agn_pushcomplex(L, sun_hypot(a, c), 0);
    } else if (a == 0) {  /* 2.31.0 fix to avoid undefined in OS/2 and DOS, and to speed up */
      t1 = b*b;
      t2 = c*c;
      t3 = -t1 + t2;
      t4 = sqrtl(fabsl(t3));
      t5 = tools_signuml(t3);  /* 3.16.6 fix */
      agn_pushcomplex(L, (double)(0.5L*t4*(1.0L + t5)), (double)(0.5L*t4*(1.0L - t5)));
    } else {
      /* = x^2 + y^2; simplified to prevent round-off errors 2.30.4 */
      /* t1 = a*a - b*b + c*c; */
      t1 = sun_pytha(a, c) - b*b;  /* 3.16.4 fix against overflow and underflow */
      t2 = 2.0L*a*b;
      /* sqrt */
      /* t3 = t1*t1 + t2*t2; */
      t3 = tools_pythal(t1, t2);  /* 3.16.4 fix against overflow and underflow */
      t4 = 2.0L*t1;
      /* t5 = 2.0L*sqrtl(t3); */
      t5 = 2.0L*tools_hypotl(t1, t2);  /* 3.16.4 fix against overflow and underflow */
      agn_pushcomplex(L,
        (double)(0.5L*sqrtl(t5 + t4)),
        (double)(0.5L*tools_csgnl(t2, -t1)*sqrtl(t5 - t4)));
    }
  } else if ((agn_isdual(L, 1) && agn_isdual(L, 2)) ||
             (lua_isnumber(L, 1) && agn_isdual(L, 2)) ||
             (agn_isdual(L, 1) && lua_isnumber(L, 2)) ) {
    trydualfix(L, "hypot", 2);
    return 1;
  } else {
    int nargs = lua_gettop(L);
    if (nargs == 1 && lua_iscomplex(L, 1)) {  /* 3.16.4 extension */
      lua_Number a, b;
      agn_getcmplxparts(L, 1, &a, &b);
      lua_pushnumber(L, sun_hypot(a, b));
    } else {
      luaL_error(L, "Error in " LUA_QS ": (complex) numbers expected, got %s, %s.", "hypot",  /* 3.7.8 improvement */
        luaL_typename(L, 1), luaL_typename(L, 2));
    }
  }
  return 1;
}


static int mathB_hypot2 (lua_State *L) {  /* = sqrt(1 + x^2) */
  if (lua_isnumber(L, 1)) {
    lua_pushnumber(L, sun_hypot2(agn_tonumber(L, 1)));  /* changed 2.14.13 to portable sun_hypot2 */
  } else if (lua_iscomplex(L, 1)) {  /* added 2.30.3 */
    double c, d, re, im;
    long double t0, t1, t2, t3;
    agn_getcmplxparts(L, 1, &c, &d);
    if (d == 0) {  /* 2.31.0 fix to avoid undefined in OS/2 and DOS, and to speed up */
      agn_pushcomplex(L, sun_hypot2(c), 0);
    } else if (c == 0) {
      t0 = d*d;
      t1 = 1.0L - t0;
      t2 = sqrtl(fabsl(t1));
      t3 = tools_signuml(t1);  /* 3.16.6 fix */
      agn_pushcomplex(L, (double)(0.5L*t2*(1.0L + t3)), (double)(0.5L*t2*(1.0L - t3)));
    } else {
      /* simplified 2.30.4 */
      /* re = 1.0L + c*c - d*d; */
      re = 1.0L + tools_mpytha(c, d);  /* 3.16.4 fix against underflow and underflow */
      im = 2.0L*c*d;
      /* t0 = re*re;
      t1 = im*im; */
      t2 = 2.0L*re;
      t3 = 2.0L*sqrtl(tools_pythal(re, im));  /* 3.16.4 fix against underflow and underflow */
      agn_pushcomplex(L, (double)(0.5L*sqrtl(t3 + t2)), (double)(0.5L*tools_csgnl(im, -re)*sqrtl(t3 - t2)));
    }
  } else {  /* 6.7.5 */
    trydualfix(L, "hypot2", 1);
    luaL_nonumorcmplxordual(L, 1, "hypot2");
  }
  return 1;
}


static int mathB_hypot3 (lua_State *L) {  /* = sqrt(1 - x^2), 2.10.4 */
  if (lua_isnumber(L, 1)) {
    lua_pushnumber(L, sun_hypot3(agn_tonumber(L, 1)));  /* added 2.30.3 */
  } else if (lua_iscomplex(L, 1)) {
    double c, d, re, im;
    long double t0, t1, t2, t3;
    agn_getcmplxparts(L, 1, &c, &d);
    if (d == 0) {  /* 2.31.0 fix to avoid undefined in OS/2 and DOS, and to speed up */
      t0 = c*c;
      t1 = 1.0L - t0;
      t2 = sqrtl(fabsl(t1));
      t3 = tools_signuml(t1);
      agn_pushcomplex(L, (double)(0.5L*t2*(1.0L + t3)), (double)(0.5L*t2*(1.0L - t3)));
    } else if (c == 0) {
      t1 = d*d;
      t3 = sqrtl(1.0L + t1);
      agn_pushcomplex(L, (double)t3, 0);
    } else {
      /* simplified 2.30.4 */
      re = 1.0L - c*c + d*d;
      im = -2.0L*c*d;
      /* t0 = re*re;
      t1 = im*im; */
      t2 = 2.0L*re;
      /* t3 = 2.0L*sqrtl(t0 + t1); */
      t3 = 2.0L*tools_hypotl(re, im);  /* 3.16.4 fix against underflow and underflow; tuned 3.5.5 */
      agn_pushcomplex(L, (double)(0.5L*sqrtl(t3 + t2)), (double)(0.5L*tools_csgnl(im, -re)*sqrtl(t3 - t2)));
    }
  } else {  /* 6.7.5 */
    trydualfix(L, "hypot3", 1);
    luaL_nonumorcmplxordual(L, 1, "hypot3");
  }
  return 1;
}


static int mathB_cathet (lua_State *L) {  /* Cathetus: sqrt(a^2 - b^2), 2.11.0 RC2 */
  if (lua_isnumber(L, 1) && lua_isnumber(L, 2)) {
    lua_pushnumber(L, tools_mhypot(agn_tonumber(L, 1), agn_tonumber(L, 2)));
  } else if (lua_iscomplex(L, 1) && lua_iscomplex(L, 2)) {  /* added 2.30.3 */
    double a, b, c, d;
    long double t1, t2, t3, t5, t6, t7;
    agn_getcmplxparts(L, 1, &a, &b);
    agn_getcmplxparts(L, 2, &c, &d);
    if (b == 0 && d == 0) {  /* 2.31.0 fix to avoid undefined in OS/2 and DOS, and to speed up */
      /* t1 = a*a;
      t2 = c*c;
      t3 = t1 - t2; */
      t3 = tools_mpytha(a, c);  /* 3.16.4 fix against underflow and overflow */
      t5 = sqrt(fabsl(t3));
      t6 = tools_signuml(t3);  /* 3.16.6 fix */
      agn_pushcomplex(L, (double)(0.5L*t5*(1.0L + t6)), (double)(0.5L*t5*(1.0L - t6)));
    } else if (a == 0 && c == 0) {
      t1 = b*b;
      t2 = d*d;
      t3 = -t1 + t2;
      t5 = sqrtl(fabsl(t3));   /* 3.16.4 fix */
      t6 = tools_signuml(t3);  /* 3.16.6 fix */
      agn_pushcomplex(L, (double)(0.5*t5*(1.0 + t6)), (double)(0.5*t5*(1.0 - t6)));
    } else {
      /* simplified to prevent round-off errors, 2.30.4 */
      /* t1 = a*a - b*b - c*c + d*d; */
      t1 = sun_pytha(a, d) - sun_pytha(b, c);  /* 3.16.4 fix against overflow and underflow */
      t2 = 2.0L*(a*b - c*d);
      /* t3 = t1*t1;
      t4 = t2*t2; */
      t5 = 2.0L*t1;
      /* t6 = t3 + t4; */
      t7 = 2.0L*tools_hypotl(t1, t2);  /* 3.16.4/4.5.5 fix against overflow and underflow */
      agn_pushcomplex(L,
        (double)(0.5L*sqrtl(t7 + t5)),
        (double)(0.5L*tools_csgnl(t2, -t1)*sqrtl(t7 - t5)));
    }
  } else if (lua_iscomplex(L, 1) && lua_isnumber(L, 2)) {  /* added 2.30.3 */
    double a, b, c;
    long double t1, t2, t3, t4, t5;
    agn_getcmplxparts(L, 1, &a, &b);
    c = agn_tonumber(L, 2);
    if (b == 0) {  /* 2.31.0 fix to avoid undefined in OS/2 and DOS, and to speed up */
      /* t1 = a*a;
      t2 = c*c;
      t3 = t1 - t2; */
      t3 = tools_mpythal(a, c);
      t4 = sqrtl(fabsl(t3));
      t5 = tools_signuml(t3);  /* 3.16.6 fix */
      agn_pushcomplex(L, 0.5L*t4*(1.0L + t5), 0.5L*t4*(1.0L - t5));
    } else if (a == 0) {
      /* t1 = b*b;
      t2 = c*c;
      t4 = sqrtl(t1 + t2); */
      agn_pushcomplex(L, 0, tools_hypotl(b, c));
    } else {
      /* simplified to prevent round-off errors, 2.30.4 */
      /* t1 = a*a - b*b - c*c; */
      t1 = tools_mpythal(a, b) - c*c;
      t2 = 2.0L*a*b;
      /* t3 = t1*t1 + t2*t2; */
      t3 = tools_pythal(t1, t2);
      t4 = 2.0L*t1;
      t5 = 2.0L*sqrtl(t3);
      agn_pushcomplex(L,
        (double)(0.5L*sqrtl(t5 + t4)),
        (double)(0.5L*tools_csgnl(t2, -t1)*sqrtl(t5 - t4)));
    }
  } else if (lua_isnumber(L, 1) && lua_iscomplex(L, 2)) {  /* fixed 3.16.4 */
    double a, c, d;
    long double t1, t2, t3, t5, t6, t7;
    a = agn_tonumber(L, 1);
    agn_getcmplxparts(L, 2, &c, &d);
    if (d == 0) {  /* avoid undefined in OS/2 and DOS */
      t3 = tools_mpytha(a, c);
      t5 = sqrtl(fabsl(t3));
      t6 = tools_signuml(t3);
      agn_pushcomplex(L, (double)(0.5L*t5*(1.0 + t6)), (double)(0.5*t5*(1.0 - t6)));
    } else if (c == 0) {
      agn_pushcomplex(L, (double)tools_hypotl(a, d), 0.0);
    } else {
      /* simplified to prevent round-off errors */
      t1 = sun_pytha(a, d) - c*c;
      t2 = 2.0L*(-c*d);
      t5 = 2.0L*t1;
      t6 = tools_pythal(t1, t2);
      t7 = 2.0L*sqrtl(t6);
      agn_pushcomplex(L,
        (double)(0.5L*sqrtl(t7 + t5)),
        (double)(0.5L*tools_csgnl(t2, -t1)*sqrtl(t7 - t5)));
    }
  } else if ((agn_isdual(L, 1) && agn_isdual(L, 2)) ||
             (lua_isnumber(L, 1) && agn_isdual(L, 2)) ||
             (agn_isdual(L, 1) && lua_isnumber(L, 2)) ) {
    trydualfix(L, "cathet", 2);
    return 1;
  } else {
    int nargs = lua_gettop(L);
    if (nargs != 2)
      luaL_error(L, "Error in " LUA_QS ": two arguments expected, got %d.", "cathet", nargs);  /* 3.7.8 improvement */
    else
      luaL_error(L, "Error in " LUA_QS ": (complex) numbers expected, got %s, %s.", "cathet",  /* 3.7.8 improvement */
        luaL_typename(L, 1), luaL_typename(L, 2));
  }
  return 1;
}


/* Computes 1/sqrt(a^2 + b^2) = 1/hypot(a, b), is 35 % faster than the naive 1/hypot approach and is protected
   against underflow and overflow. 2.35.3 */
static int mathB_invhypot (lua_State *L) {
  lua_Number a, b, c, d;
  int nargs = lua_gettop(L);
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER:
      if (nargs == 2 && lua_isnumber(L, 2)) {
        lua_pushnumber(L, tools_invhypot(agn_tonumber(L, 1), agn_tonumber(L, 2)));
      } else if (nargs == 2 && lua_iscomplex(L, 2)) {
        long double t1, t2, t3, t5, t7, t9, t11, t13, t16, t17, t18, t20, t21, t23;
        a = agn_tonumber(L, 1);
        c = agn_complexreal(L, 2); d = agn_compleximag(L, 2);
        t1 = a*a;
        t2 = t1*t1;
        t3 = c*c;
        t5 = d*d;
        t7 = t3*t3;
        t9 = t5*t5;
        t11 = sqrtl(t2 + 2.0L*(t1*t3 - t1*t5 + t3*t5) + t7 + t9);
        t13 = sqrtl(2.0L*(t11 + t1 + t3 - t5));
        t16 = tools_csgnl(2.0L*c*d, -t1 - t3 + t5);
        t17 = t16*t16;
        t18 = 2.0*(t11 - t1 - t3 + t5);
        t20 = 0.5L*(t11 + t1 + t3 - t5) + 0.25L*t17*t18;
        if (t18 < 0.0L || t20 == 0.0L) {
          lua_pushundefined(L);
          return 1;
        }
        t21 = 1/t20;
        t23 = sqrtl(t18);
        agn_pushcomplex(L, (double)(0.5L*t13*t21), (double)(-0.5L*t16*t23*t21));
      } else {
        trydualfix(L, "invhypot", 2);
        luaL_nonumorcmplxordual(L, 1, "invhypot");
      }
      break;
    case LUA_TCOMPLEX: { /* 3.16.4 extension */
      if (nargs == 1) {
        lua_pushnumber(L, tools_invhypot(agn_complexreal(L, 1), agn_compleximag(L, 1)));
      } else if (nargs == 2 && lua_iscomplex(L, 2)) {
        long double t1, t2, t3, t5, t7, t9, t12, t14, t15, t16, t18, t19, t21, t23, t24, t25, t27, t28, t30;
        a = agn_complexreal(L, 1); b = agn_compleximag(L, 1);
        c = agn_complexreal(L, 2); d = agn_compleximag(L, 2);
        t1 = a*a;
        t2 = t1*t1;
        t3 = b*b;
        t5 = c*c;
        t7 = d*d;
        t9 = t3*t3;
        t12 = t5*t5;
        t14 = t7*t7;
        t15 = a*b;
        t16 = c*d;
        t18 = t2 + 2.0L*(t1*t3 + t1*t5 - t7*t1 - t3*t5 + t3*t7 + t5*t7) + t9 + t12 + t14 + 8.0L*t15*t16;
        t19 = sqrtl(t18);
        t21 = sqrtl(2.0L*(t19 + t1 - t3 + t5 - t7));
        t23 = tools_csgnl(2.0L*(t15 + t16), -t1 + t3 - t5 + t7);
        t24 = t23*t23;
        t25 = 2.0L*(t19 - t1 + t3 - t5 + t7);
        t27 = 0.5L*(t19 + t1 - t3 + t5 - t7) + 0.25L*t24*t25;
        if (t25 < 0.0L || t27 == 0.0L) {
          lua_pushundefined(L);
          return 1;
        }
        t28 = 1/t27;
        t30 = sqrtl(t25);
        agn_pushcomplex(L, (double)(0.5L*t21*t28), (double)(-0.5L*t23*t30*t28));
      } else if (nargs == 2 && lua_isnumber(L, 2)) {
        long double t1, t2, t3, t5, t7, t9, t11, t13, t16, t17, t18, t20, t21, t23;
        a = agn_complexreal(L, 1); b = agn_compleximag(L, 1);
        c = agn_tonumber(L, 2);
        t1 = a*a;
        t2 = t1*t1;
        t3 = b*b;
        t5 = c*c;
        t7 = t3*t3;
        t9 = t5*t5;
        t11 = sqrtl(t2 + 2.0L*(t1*t3 + t1*t5 - t3*t5) + t7 + t9);
        t13 = sqrtl(2.0L*(t11 + t1 - t3 + t5));
        t16 = tools_csgnl(2.0*a*b, -t1 + t3 - t5);
        t17 = t16*t16;
        t18 = 2.0L*(t11 - t1 + t3 - t5);
        t20 = 0.5L*(t11 + t1 - t3 + t5) + 0.25L*t17*t18;
        if (t18 < 0.0L || t20 == 0.0L) {
          lua_pushundefined(L);
          return 1;
        }
        t21 = 1/t20;
        t23 = sqrtl(t18);
        agn_pushcomplex(L, (double)(0.5L*t13*t21), (double)(-0.5L*t16*t23*t21));
      } else {
        trydualfix(L, "invhypot", 2);
        luaL_nonumorcmplxordual(L, 1, "invhypot");
      }
      break;
    }
    default: {
      trydualfix(L, "invhypot", 2);
      luaL_nonumorcmplxordual(L, 1, "invhypot");
    }
  }
  return 1;
}


/* Computes 1/(a^2 + b^2) = 1/pytha(a, b). Based on mathB_invhypot, 4.5.6; computes with 80-bit precision in the complex domain but
   is only half as fast as a primitive 1/(a^2 + b^2). */
static int mathB_invpytha (lua_State *L) {
  lua_Number a, b, c, d;
  int nargs = lua_gettop(L);
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      if (nargs == 2 && lua_isnumber(L, 2)) {
        lua_pushnumber(L, tools_invpytha(agn_tonumber(L, 1), agn_tonumber(L, 2)));
      } else if (nargs == 2 && lua_iscomplex(L, 2)) {
        long double t1, t2, t3, t4, t5, t6, t8;
        a = agn_tonumber(L, 1);
        c = agn_complexreal(L, 2); d = agn_compleximag(L, 2);
        t1 = a*a;
        t2 = c*c;
        t3 = d*d;
        t4 = t1 + t2 - t3;
        t5 = t4*t4;
        t6 = t5 + 4.0*t2*t3;
        if (t6 == 0.0L) {
          lua_pushundefined(L);
        } else {
          t8 = 1/t6;
          agn_pushcomplex(L, (double)(t4*t8), (double)(-2.0*c*d*t8));
        }
      } else {
        luaL_error(L, "Error in " LUA_QS ": one or two (complex) numbers expected.", "invpytha");
      }
      break;
    }
    case LUA_TCOMPLEX: {
      if (nargs == 1) {
        lua_pushnumber(L, tools_invpytha(agn_complexreal(L, 1), agn_compleximag(L, 1)));
      } else if (nargs == 2 && lua_iscomplex(L, 2)) {
        long double t1, t2, t3, t4, t5, t6, t9, t10, t11, t12;
        a = agn_complexreal(L, 1); b = agn_compleximag(L, 1);
        c = agn_complexreal(L, 2); d = agn_compleximag(L, 2);
        t1 = a*a;
        t2 = b*b;
        t3 = c*c;
        t4 = d*d;
        t5 = t1 - t2 + t3 - t4;
        t6 = t5*t5;
        t9 = 2.0*a*b + 2.0*c*d;
        t10 = t9*t9;
        t11 = t6 + t10;
        if (t11 == 0.0L) {
          lua_pushundefined(L);
        } else {
          t12 = 1/t11;
          agn_pushcomplex(L, (double)(t5*t12), (double)(-t9*t12));
        }
      } else if (nargs == 2 && lua_isnumber(L, 2)) {
        long double t1, t2, t3, t4, t5, t6, t8;
        a = agn_complexreal(L, 1); b = agn_compleximag(L, 1);
        c = agn_tonumber(L, 2);
        t1 = a*a;
        t2 = b*b;
        t3 = c*c;
        t4 = t1 - t2 + t3;
        t5 = t4*t4;
        t6 = t5 + 4.0*t1*t2;
        if (t6 == 0.0L) {
          lua_pushundefined(L);
        } else {
          t8 = 1/t6;
          agn_pushcomplex(L, (double)(t4*t8), (double)(-2.0*a*b*t8));
        }
      } else {
        luaL_error(L, "Error in " LUA_QS ": one or two (complex) numbers expected.", "invpytha");
      }
      break;
    }
    default:
      luaL_nonumorcmplx(L, 1, "invpytha");
  }
  return 1;
}


/* Computes the Pythagorean equation c^2 = a^2 + b^2, without undue underflow or overflow. 2.21.7 */
static int mathB_pytha (lua_State *L) {
  lua_Number a, b, c, d;
  int nargs = lua_gettop(L);
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      if (nargs == 2 && lua_isnumber(L, 2)) {
        lua_pushnumber(L, sun_pytha(agn_tonumber(L, 1), agn_tonumber(L, 2)));
      } else if (nargs == 2 && lua_iscomplex(L, 2)) {
        a = agn_tonumber(L, 1);
        c = agn_complexreal(L, 2); d = agn_compleximag(L, 2);
        agn_pushcomplex(L, (double)(a*a + tools_mpythal(c, d)), (double)(2.0*c*d));
      } else {
        luaL_error(L, "Error in " LUA_QS ": one or two (complex) numbers expected.", "pytha");
      }
      break;
    }
    case LUA_TCOMPLEX: {  /* 3.16.4 extension */
      if (nargs == 1) {
        lua_pushnumber(L, sun_pytha(agn_complexreal(L, 1), agn_compleximag(L, 1)));
      } else if (nargs == 2 && lua_iscomplex(L, 2)) {
        a = agn_complexreal(L, 1); b = agn_compleximag(L, 1);
        c = agn_complexreal(L, 2); d = agn_compleximag(L, 2);
        agn_pushcomplex(L, (double)(tools_mpythal(a, b) + tools_mpythal(c, d)), (double)(2.0*(a*b + c*d)));
      } else if (nargs == 2 && lua_isnumber(L, 2)) {
        a = agn_complexreal(L, 1); b = agn_compleximag(L, 1);
        c = agn_tonumber(L, 2);
        agn_pushcomplex(L, (double)(tools_mpythal(a, b) + c*c), (double)(2.0*a*b));
      } else {
        luaL_error(L, "Error in " LUA_QS ": one or two complex numbers expected.", "pytha");
      }
      break;
    }
    default: {
      trydualfix(L, "pytha", 2);
      luaL_nonumorcmplxordual(L, 1, "pytha");
      return 1;
    }
  }
  return 1;
}


/* Computes a^2 - b^2, without undue underflow or overflow. 3.1.3 */
static int mathB_pytha4 (lua_State *L) {
  lua_Number a, b, c, d;
  int nargs = lua_gettop(L);
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      if (nargs == 2 && lua_isnumber(L, 2)) {
        lua_pushnumber(L, tools_mpytha(agn_tonumber(L, 1), agn_tonumber(L, 2)));
      } else if (nargs == 2 && lua_iscomplex(L, 2)) {
        a = agn_tonumber(L, 1);
        c = agn_complexreal(L, 2); d = agn_compleximag(L, 2);
        agn_pushcomplex(L, (double)(a*a - tools_mpythal(c, d)), (double)(-2.0*c*d));
      } else {
        luaL_error(L, "Error in " LUA_QS ": one or two complex numbers expected.", "pytha4");
      }
      break;
    }
    case LUA_TCOMPLEX: {  /* 3.16.4 extension */
      if (nargs == 1) {
        lua_pushnumber(L, tools_mpytha(agn_complexreal(L, 1), agn_compleximag(L, 1)));
      } else if (nargs == 2 && lua_iscomplex(L, 2)) {
        a = agn_complexreal(L, 1); b = agn_compleximag(L, 1);
        c = agn_complexreal(L, 2); d = agn_compleximag(L, 2);
        agn_pushcomplex(L, (double)(tools_mpythal(a, b) - tools_mpythal(c, d)), (double)(2.0*(a*b - c*d)));
      } else if (nargs == 2 && lua_isnumber(L, 2)) {
        a = agn_complexreal(L, 1); b = agn_compleximag(L, 1);
        c = agn_tonumber(L, 2);
        agn_pushcomplex(L, (double)(tools_mpythal(a, b) - c*c), (double)(2.0*a*b));
      } else {
        luaL_error(L, "Error in " LUA_QS ": one or two complex numbers expected.", "pytha4");
      }
      break;
    }
    default: {
      trydualfix(L, "pytha4", 2);
      luaL_nonumorcmplxordual(L, 1, "pytha4");
      return 1;
    }
  }
  return 1;
}


/* computes the binomial coefficient;
   added December 26, 2007; 0.9.0; changed April 20, 2008 - 0.10.0, extended 2.10.4 */
static int mathB_binomial (lua_State *L) {
  lua_pushnumber(L, tools_binomial(agn_checknumber(L, 1), agn_checknumber(L, 2)));
  return 1;
}


static int mathB_erf (lua_State *L) {
  int nargs = lua_gettop(L);
  if (nargs == 1) {
    switch (lua_type(L, 1)) {
      case LUA_TNUMBER: {
        lua_pushnumber(L, sun_erf(agn_tonumber(L, 1)));  /* 3.1.3 15 % tweak */
        break;
      }
      case LUA_TCOMPLEX: {  /* 0.28.1 */
#ifndef PROPCMPLX
        agn_createcomplex(L, tools_cerf(agn_tocomplex(L, 1)));
#else
        lua_Number re, im;
        tools_cerf(agn_complexreal(L, 1), agn_compleximag(L, 1), &re, &im);
        agn_createcomplex(L, re, im);
#endif
        break;
      }
      default: {
        trydualfix(L, "erf", 1);
        luaL_nonumorcmplxordual(L, 1, "erf");
      }
    }
  } else if (nargs > 1) {  /* integral of the Gaussian distribution from z to w with erf(z, w) = erf(w) - erf(z), 3.5.3 */
    if (agn_isnumber(L, 1) && agn_isnumber(L, 2)) {
      lua_pushnumber(L, sun_erf(agn_tonumber(L, 2)) - sun_erf(agn_tonumber(L, 1)));
    } else if (lua_iscomplex(L, 1) && lua_iscomplex(L, 2)) {
#ifndef PROPCMPLX
      agn_Complex z, w;
      z = tools_cerf(agn_tocomplex(L, 1));
      w = tools_cerf(agn_tocomplex(L, 2));
      agn_createcomplex(L, (agnc_sub(w, z)));
#else
      lua_Number rez, imz, rew, imw, z[2];
      tools_cerf(agn_complexreal(L, 1), agn_compleximag(L, 1), &rez, &imz);
      tools_cerf(agn_complexreal(L, 2), agn_compleximag(L, 1), &rew, &imw);
      agnc_sub(z, rew, imw, rez, imz);
      agn_pushcomplex(L, z[0], z[1]);
#endif
    } else if (lua_iscomplex(L, 1) && lua_isnumber(L, 2)) {
#ifndef PROPCMPLX
      agn_Complex z, w;
      z = tools_cerf(agn_tocomplex(L, 1));
      w = tools_cerf(agn_tonumber(L, 2) + 0*I);
      agn_createcomplex(L, (agnc_sub(w, z)));
#else
      lua_Number rez, imz, rew, imw, z[2];
      tools_cerf(agn_complexreal(L, 1), agn_compleximag(L, 1), &rez, &imz);
      tools_cerf(agn_complexreal(L, 2), 0, &rew, &imw);
      agnc_sub(z, rew, imw, rez, imz);
      agn_pushcomplex(L, z[0], z[1]);
#endif
    } else if (agn_isnumber(L, 1) && lua_iscomplex(L, 2)) {
#ifndef PROPCMPLX
      agn_Complex z, w;
      z = tools_cerf(agn_tonumber(L, 1) + 0*I);
      w = tools_cerf(agn_tocomplex(L, 2));
      agn_createcomplex(L, (agnc_sub(w, z)));
#else
      lua_Number rez, imz, rew, imw, z[2];
      tools_cerf(agn_complexreal(L, 1), 0, &rez, &imz);
      tools_cerf(agn_complexreal(L, 2), agn_compleximag(L, 2), &rew, &imw);
      agnc_sub(z, rew, imw, rez, imz);
      agn_pushcomplex(L, z[0], z[1]);
#endif
    } else {
      luaL_error(L, "Error in " LUA_QS ": expected numbers or complex numbers.", "erf");
    }
  } else {
    luaL_error(L, "Error in " LUA_QS ": one or two arguments expected, got %d.", "erf", nargs);
  }
  return 1;
}


static int mathB_erfc (lua_State *L) {
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      lua_pushnumber(L, sun_erfc(agn_checknumber(L, 1)));  /* 3.1.3 15 % tweak */
      break;
    }
    case LUA_TCOMPLEX: {  /* 0.32.1 */
#ifndef PROPCMPLX
      agn_createcomplex(L, tools_cerfc(agn_tocomplex(L, 1)));
#else
      lua_Number re, im;
      tools_cerfc(agn_complexreal(L, 1), agn_compleximag(L, 1), &re, &im);
      agn_createcomplex(L, re, im);
#endif
      break;
    }
    default: {
      trydualfix(L, "erfc", 1);
      luaL_nonumorcmplxordual(L, 1, "erfc");
    }
  }
  return 1;
}


/* Implements the Scaled Complementary Error Function erfcx(x) = exp(x^2)*erfc(x),
   with x a number or complex number and - depending on the type of argument -
   a numeric or complex result. 2.21.6 */
static int agnB_erfcx (lua_State *L) {
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      lua_pushnumber(L, tools_erfcxl(agn_checknumber(L, 1)));  /* 6.7.0 tweak of accuracy */
      break;
    }
    case LUA_TCOMPLEX: {
#ifndef PROPCMPLX
      agn_Complex z0, z1;
#else
      lua_Number z0[2], r[2];
#endif
      lua_Number a, b, re, im;
      agn_getcmplxparts(L, 1, &a, &b);
      /* compute exp(z^2) */
      re = tools_cexpx2(a, b, 1, &im);
#ifndef PROPCMPLX
      z0 = re + I*im;
      /* compute erfc(z) */
      z1 = tools_cerfc(a + I*b);
      /* produce and push result z0*z1 */
      agn_createcomplex(L, z0*z1);
#else
      z0[0] = re;
      z0[1] = im;
      /* compute erfc(z) */
      tools_cerfc(a, b, &re, &im);
      /* produce and push result z0*z1 */
      agnc_mul(r, z0[0], z0[1], re, im);
      agn_createcomplex(L, r[0], r[1]);
#endif
      break;
    }
    default: {
      trydualfix(L, "erfcx", 1);
      luaL_nonumorcmplxordual(L, 1, "erfcx");
    }
  }
  return 1;
}


/* Computes the imaginary error function erfi(z) = -I*erf(I*z) for real or complex z. The type of return depends
   on the type of z.

   For formula, see: http://ab-initio.mit.edu/Faddeeva.cc, function FADDEEVA(erfi). 2.21.7 */
static int agnB_erfi (lua_State *L) {
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      lua_Number x = agn_tonumber(L, 1);
      lua_pushnumber(L, (x*x > 720) ? (x > 0 ? HUGE_VAL : -HUGE_VAL)
                        : tools_expx2(x, 1)*tools_w_im(x));
      break;
    }
    case LUA_TCOMPLEX: {
      lua_Number a, b;
#ifndef PROPCMPLX
      agn_Complex z;
#endif
      agn_getcmplxparts(L, 1, &a, &b);
      /* compute erf(I*z) */
#ifndef PROPCMPLX
      z = tools_cerf(-b + a*I);
      /* produce and push result -I*z0 */
      agn_createcomplex(L, cimag(z) - I*creal(z));
#else
      lua_Number re, im;
      /* compute erf(I*z) */
      tools_cerf(-b, a, &re, &im);
      /* produce and push result -I*z0 */
      agn_createcomplex(L, im, -re);
#endif
      break;
    }
    default:
      luaL_nonumorcmplx(L, 1, "erfi");
  }
  return 1;
}


/* Computes the inverse error function erf^-1(x); 2.21.5, see: https://mathworld.wolfram.com/InverseErf.html */
static int mathB_inverf (lua_State *L) {
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      lua_Number r = tools_inverfl(agn_tonumber(L, 1));  /* 6.6.13 change to long double */
      lua_pushnumber(L, r == -0 ? 0 : r);
      break;
    }
    /* There are various approaches for complex arguments on the net: all produce different results. There is a
       statement: `If you do not provide a 500-page manual with your implementation, nobody will understand what
       your complex function will do.` */
    default: {
      trydualfix(L, "inverf", 1);
      luaL_nonumordual(L, 1, "inverf");
    }
  }
  return 1;
}


/* Computes the inverse complementary error function erfc^-1(x); 2.21.5,
   see: https://www.mathworks.com/help/symbolic/erfcinv.html */
static int mathB_inverfc (lua_State *L) {
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      lua_pushnumber(L, tools_inverfcl(agn_tonumber(L, 1)));
      break;
    }
    /* There are various approaches for complex arguments on the net: all produce different results. There is a
       statement: `If you do not provide a 500-page manual with your implementation, nobody will understand what
       your complex function will do.` */
    default: {
      trydualfix(L, "inverfc", 1);
      luaL_nonumordual(L, 1, "inverfc");
    }
  }
  return 1;
}


/* return exp(x*x) if sign is nonnegative, and exp(-x*x) if sign is negative
   based on file `expx2.c` in the Cephes Math Library Release 2.9:  June, 2000
   Copyright 2000 by Stephen L. Moshier
   0.32.1, 15.05.2010 */

static int mathB_expx2 (lua_State *L) {
  lua_Number sign;
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      lua_Number x;
      x = agn_tonumber(L, 1);
      sign = agnL_optnumber(L, 2, 1);  /* 2.10.1 improvement */
      lua_pushnumber(L, tools_expx2(x, sign));  /* long double expx2l is not better */
      break;
    }
    case LUA_TCOMPLEX: {
      lua_Number a, b, re, im;
      agn_getcmplxparts(L, 1, &a, &b);  /* 2.17.6 */
      sign = agnL_checknumber(L, 2);
      re = tools_cexpx2(a, b, sign, &im);
      agn_pushcomplex(L, re, im);  /* 2.17.6 */
      break;
    }
    default: {
      trydualfix(L, "expx2", 2);
      luaL_nonumorcmplxordual(L, 1, "expx2");
    }
  }
  return 1;
}


#ifndef PROPCMPLX
/*     ===========================================================
       Purpose: Compute complex Bessel functions J0(z), J1(z)
                Y0(z), Y1(z), and their derivatives
       Input :  z --- Complex argument
       Output:  cbj0 --- J0(z)
                cbj1 --- J1(z)
                cby0 --- Y0(z)
                cby1 --- Y1(z)
       =========================================================== */

void cjy01 (agn_Complex z, agn_Complex *cbj0, agn_Complex *cbj1,
            agn_Complex *cby0, agn_Complex *cby1) {
  lua_Number a0, k0, rp2, w0, w1;
  agn_Complex cr, ci, cp, cp0, cp1, cq0, cq1, cs, ct1, ct2, cu, z1, z2;
  int k;
  rp2 = 2.0/PI;
  ci = 0.0 + 1.0*I;
  a0 = tools_cabs(z);
  z2 = z*z;
  z1 = z;
  if (a0 == 0.0) {
    *cbj0 = 1.0 + 0.0*I;
    *cbj1 = 0.0 + 0.0*I;
    *cby0 = -HUGE_VAL;
    *cby1 = -HUGE_VAL;
    return;
  }
  if (creal(z) < 0.0) z1 = -z;
  if (a0 <= 12.0) {
    *cbj0 = 1.0 + 0.0*I;
    cr = 1.0 + 0.0*I;
    for (k=1; k < 41; k++) {
      cr = -0.25*cr*z2/(k*k);
      *cbj0 = *cbj0 + cr;
      if (tools_cabs(cr/(*cbj0)) < 1.0e-15) break;
    }
    *cbj1 = 1.0 + 0.0*I;
    cr = 1.0 + 0.0*I;
    for (k=1; k < 41; k++) {
      cr = -0.25*cr*z2/(k*(k + 1.0));
      *cbj1 = *cbj1 + cr;
      if (tools_cabs(cr/(*cbj1)) < 1.0e-15) break;
    }
    *cbj1 = 0.5*z1*(*cbj1);
    w0 = 0.0;
    cr = 1.0 + 0.0*I;
    cs = 0.0 + 0.0*I;
    for (k=1; k < 41; k++) {
      w0 = w0 + 1.0/k;
      cr = -0.25*cr/(k*k)*z2;
      cp = cr*w0;
      cs = cs + cp;
      if (tools_cabs(cp/cs) < 1.0e-15) break;
    }
    *cby0 = rp2*(tools_clog(z1*0.5) + EULERGAMMA)*(*cbj0) - rp2*cs;  /* 2.17.7 optimisation */
    w1 = 0.0;
    cr = 1.0 + 0.0*I;
    cs = 1.0 + 0.0*I;
    for (k=1; k < 41; k++) {
      w1 = w1 + 1.0/k;
      cr = -0.25*cr/(k*(k + 1))*z2;
      cp = cr*(2.0*w1 + 1.0/(k + 1.0));
      cs = cs + cp;
      if (tools_cabs(cp/cs) < 1.0e-15) break;
    }
    *cby1 = rp2*((tools_clog(z1*0.5) + EULERGAMMA)*(*cbj1) - 1.0/z1 - .25*z1*cs);
  } else {
    lua_Number a[12] = {-.703125e-01, .112152099609375e+00,
       -.5725014209747314e+00, .6074042001273483e+01,
       -.1100171402692467e+03, .3038090510922384e+04,
       -.1188384262567832e+06, .6252951493434797e+07,
       -.4259392165047669e+09, .3646840080706556e+11,
       -.3833534661393944e+13, .4854014686852901e+15};
    lua_Number b[12] = {.732421875e-01, -.2271080017089844e+00,
        .1727727502584457e+01, -.2438052969955606e+02,
        .5513358961220206e+03, -.1825775547429318e+05,
        .8328593040162893e+06, -.5006958953198893e+08,
        .3836255180230433e+10, -.3649010818849833e+12,
        .4218971570284096e+14, -.5827244631566907e+16};
    lua_Number a1[12] = {.1171875e+00, -.144195556640625e+00,
        .6765925884246826e+00, -.6883914268109947e+01,
        .1215978918765359e+03, -.3302272294480852e+04,
        .1276412726461746e+06, -.6656367718817688e+07,
        .4502786003050393e+09, -.3833857520742790e+11,
        .4011838599133198e+13, -.5060568503314727e+15};
    lua_Number b1[12] = {-.1025390625e+00, .2775764465332031e+00,
        -.1993531733751297e+01, .2724882731126854e+02,
        -.6038440767050702e+03, .1971837591223663e+05,
        -.8902978767070678e+06,.5310411010968522e+08,
        -.4043620325107754e+10, .3827011346598605e+12,
        -.4406481417852278e+14, .6065091351222699e+16};
    k0 = 12;
    if (a0 >= 35.0) k0 = 10;
    if (a0 >= 50.0) k0 = 8;
    ct1 = z1 - 0.25*PI;
    cp0 = 1.0 + 0.0*I;
    for (k=0; k < k0; k++)
      cp0 = cp0 + a[k]*tools_cpow(z1, (-2*(k + 1)));  /* 2.11.1 tuning */
    cq0 = -0.125/z1;
    for (k=0; k < k0; k++)
      cq0 = cq0 + b[k]*tools_cpow(z1, (-2*(k + 1)-1)); /* 2.11.1 tuning */
    cu = tools_csqrt(rp2/z1);
    *cbj0 = cu * (cp0*tools_ccos(ct1) - cq0*tools_csin(ct1));  /* 3.4.5 precautionary fix with wrong GCC csin/ccos results */
    *cby0 = cu * (cp0*tools_csin(ct1) + cq0*tools_ccos(ct1));
    ct2 = z1 - 0.75*PI;
    cp1 = 1.0 + 0.0*I;
    for (k=0; k < k0; k++)
      cp1 = cp1 + a1[k]*tools_cpow(z1, (-2*(k + 1)));  /* 2.11.1 tuning */
    cq1 = 0.375/z1;
    for (k=0; k < k0; k++)
      cq1 = cq1 + b1[k]*tools_cpow(z1, (-2*(k + 1) - 1));  /* 2.11.1 tuning */
    *cbj1 = cu*(cp1*tools_ccos(ct2) - cq1*tools_csin(ct2));  /* 3.4.5 precautionary fix with wrong GCC csin/ccos results */
    *cby1 = cu*(cp1*tools_csin(ct2) + cq1*tools_ccos(ct2));
  }
  if (creal(z) < 0.0) {
    if (cimag(z) < 0.0) *cby0 = (*cby0) - 2.0*ci*(*cbj0);
    if (cimag(z) > 0.0) *cby0 = (*cby0) + 2.0*ci*(*cbj0);
    if (cimag(z) < 0.0) *cby1 = -((*cby1) - 2.0*ci*(*cbj1));
    if (cimag(z) > 0.0) *cby1 = -((*cby1) + 2.0*ci*(*cbj1));
    *cbj1 = -(*cbj1);
  }
}

/*     =======================================================
       Purpose: Compute Bessel functions Jn(z), Yn(z) and
                their derivatives for a complex argument
       Input :  z --- Complex argument of Jn(z) and Yn(z)
                n --- Order of Jn(z) and Yn(z)
       Output:  cbj(n) --- Jn(z)
                cby(n) --- Yn(z)
                nm --- Highest order computed
       Routines called:
            (1) CJY01 to calculate J0(z), J1(z), Y0(z), Y1(z)
            (2) tools_msta1 and tools_msta2 to calculate the starting
                point for backward recurrence
       =======================================================

  modified 0.32.2 to correct problems with out-of-range indices

*/

void cjyna (int n, agn_Complex z,
            agn_Complex *ccbj, agn_Complex *ccby) {
  agn_Complex cg0, cg1, cbj0, cbj1, cj0, cj1, cjk, cby0, cby1, cf, cf1, cf2, cs,
              cp11, cp12, cp21, cp22, cyk, cylk, cyl1, cyl2, ch0, ch1, ch2;
  agn_Complex cbj[n + 2], cby[n + 2];
  /* one field more than needed to prevent out-of-range index access */
  lua_Number a0, ya0, ya1, yak;
  int k, lb, lb0, m, nm;
  lb0 = 0;
  a0 = tools_cabs(z);
  nm = n;
  cf = ch0 = 0+0*I;  /* to avoid inexplicable compiler warnings under OpenSUSE 10.3 */
  if (a0 < 1.0e-100) {
    for (k=0; k <= n; k++) {
      cbj[k] = 0.0 + 0.0*I;
      cby[k] = -HUGE_VAL;
    }
    cbj[0] = 1.0 + 0.0*I;
    goto l1;
  } else {  /* better sure than sorry, 0.32.2 */
    for (k=0; k < n + 2; k++) {
      cbj[k] = AGN_NAN;
      cby[k] = AGN_NAN;
    }
  }
  cjy01(z, &cbj0, &cbj1, &cby0, &cby1);
  cbj[0] = cbj0;
  cby[0] = cby0;
  if (n != 0) {  /* 0.32.2 fix */
    cbj[1] = cbj1;
    cby[1] = cby1;
  }
  if (n <= 1) goto l1;
  if (n < sun_trunc(0.25*a0)) {
    cj0 = cbj0;
    cj1 = cbj1;
    for (k=2; k <= n; k++) {
      cjk = 2.0*(k - 1.0)/z*cj1 - cj0;
      cbj[k] = cjk;
      cj0 = cj1;
      cj1 = cjk;
    }
  } else {
    m = tools_msta1(a0, 200);
    if (m < n)
      nm = m;
    else
      m = tools_msta2(a0, n, 15);
    cf2 = 0.0 + 0.0*I;
    cf1 = 1.0e-100 + 0.0*I;
    for (k=m; k >= 0; k--) {
      cf = 2.0*(k + 1.0)/z*cf1 - cf2;
      if (k <= nm) cbj[k] = cf;
      cf2 = cf1;
      cf1 = cf;
    }
    if (tools_cabs(cbj0) > tools_cabs(cbj1))
      cs = cbj0/cf;
    else
      cs = cbj1/cf2;
    for (k=0; k <= nm; k++) {
      cbj[k] = cs*cbj[k];
    }
  }
  ya0 = tools_cabs(cby0);
  lb = 0;
  cg0 = cby0;
  cg1 = cby1;
  for (k=2; k <= nm; k++) {
    cyk = 2.0*(k - 1.0)/z*cg1 - cg0;
    if (tools_cabs(cyk) > 1.0e+290) continue;
    yak = tools_cabs(cyk);
    ya1 = tools_cabs(cg0);
    if ((yak < ya0) && (yak < ya1)) lb = k;
    cby[k] = cyk;
    cg0 = cg1;
    cg1 = cyk;
  }
  if ((lb <= 4) || (cimag(z) == 0.0)) goto l1;
  while (lb != lb0) {
    ch2 = 1.0 + 0.0*I;
    ch1 = 0.0 + 0.0*I;
    lb0 = lb;
    for (k=lb; k > 0; k--) {
      ch0 = 2.0*k/z*ch1 - ch2;
      ch2 = ch1;
      ch1 = ch0;
    }
    cp12 = ch0;
    cp22 = ch2;
    ch2 = 0.0 + 0.0*I;
    ch1 = 1.0 + 0.0*I;
    for (k=lb; k > 0; k--) {
      ch0 = 2.0*k/z*ch1 - ch2;
      ch2 = ch1;
      ch1 = ch0;
    }
    cp11 = ch0;
    cp21 = ch2;
    if (lb == nm) cbj[lb + 1] = 2.0*lb/z*cbj[lb] - cbj[lb - 1];
    if (tools_cabs(cbj[0]) > tools_cabs(cbj[1])) {
      cby[lb + 1] = (cbj[lb + 1]*cby0 - 2.0*cp11/(PI*z))/cbj[0];
      cby[lb] = (cbj[lb]*cby0 + 2.0*cp12/(PI*z))/cbj[0];
    } else {
      cby[lb + 1] = (cbj[lb + 1]*cby1 - 2.0*cp21/(PI*z))/cbj[1];
      cby[lb] = (cbj[lb]*cby1 + 2.0*cp22/(PI*z))/cbj[1];
    }
    cyl2 = cby[lb + 1];
    cyl1 = cby[lb];
    for (k=lb-1; k >= 0; k--) {
      cylk = 2.0*(k + 1.0)/z*cyl1 - cyl2;
      cby[k] = cylk;
      cyl2 = cyl1;
      cyl1 = cylk;
    }
    cyl1 = cby[lb];
    cyl2 = cby[lb + 1];
    for (k=lb + 1; k <= nm; k++) {
      cylk = 2.0*k/z*cyl2 - cyl1;
      cby[k + 1] = cylk;
      cyl1 = cyl2;
      cyl2 = cylk;
    }
    for (k=2; k <= nm; k++) {
      if (tools_cabs(cby[k]) < tools_cabs(cby[k - 1])) lb = k;
    }
  }
l1:
  *ccbj = cbj[n];
  *ccby = cby[n];
}
#endif


static int mathB_besselj (lua_State *L) {
  switch (lua_type(L, 2)) {
    case LUA_TNUMBER: {
      lua_pushnumber(L, sun_jn(agn_checknumber(L, 1), agn_checknumber(L, 2)));  /* 2.17.4 optimisation */
      break;
    }
#ifndef PROPCMPLX
    case LUA_TCOMPLEX: {
      agn_Complex cbj, cby;
      int n = agn_checknumber(L, 1);
      if (n < 0) luaL_error(L, "Error in " LUA_QS ": order must be nonnegative.", "besselj");
      cjyna(n, agn_tocomplex(L, 2), &cbj, &cby);
      agn_createcomplex(L, cbj);
      break;
    }
    default:
      luaL_nonumorcmplx(L, 2, "besselj");
#else
    default:
      luaL_error(L, "Error in " LUA_QS ": number expected, got %s.", "besselj",
        luaL_typename(L, 2));
#endif
  }
  return 1;
}


static int mathB_bessely (lua_State *L) {
  switch (lua_type(L, 2)) {
    case LUA_TNUMBER: {
      lua_pushnumber(L, sun_yn(agn_checknumber(L, 1), agn_checknumber(L, 2)));  /* 2.17.4 optimisation */
      break;
    }
#ifndef PROPCMPLX
    case LUA_TCOMPLEX: {
      agn_Complex cbj, cby;
      int n = agn_checknumber(L, 1);
      if (n < 0) luaL_error(L, "Error in " LUA_QS ": order must be nonnegative.", "bessely");
      cjyna(n, agn_tocomplex(L, 2), &cbj, &cby);
      agn_createcomplex(L, cby);
      break;
    }
    default:
      luaL_nonumorcmplx(L, 2, "bessely");
#else
    default:
      luaL_error(L, "Error in " LUA_QS ": number expected, got %s.", "bessely",
        luaL_typename(L, 2));
#endif
  }
  return 1;
}


static int mathB_fma (lua_State *L) {  /* 0.29.0, patched 2.4.0 & 2.8.4, see agncmpt.h */
  int i, realonly = 1, nargs = lua_gettop(L);
  if (nargs < 3) {
    luaL_error(L, "Error in " LUA_QS ": at least three (complex) numbers expected, got %d.", "fma", nargs);
  }
  if ((nargs & 1) == 0)  /* number of arguments is not odd */
    luaL_error(L, "Error in " LUA_QS ": number of arguments must be add, got %d.", "fma", nargs);
  for (i=1; i <= nargs && realonly; i++) {
    realonly = agn_isnumber(L, i);
  }
  i = nargs - 2;
  if (realonly) {  /* all values are numbers, 7.1.5 25 % tuning */
    lua_Number a, b, s;
    s = agn_tonumber(L, i + 2);
    while (i >= 0) {
      a = agn_tonumber(L, i);
      b = agn_tonumber(L, i + 1);
      s = fma(a, b, s);
      i -= 2;
    }
    lua_pushnumber(L, s);
  } else {
    /* From here on we either have a mix of real and complex numbers, or complex numbers only, so result is
       always complex. Tweaked 7.1.5 */
    lua_Number a, b, c, d, re, im;
    if (agn_isnumber(L, i + 2)) {
      re = agn_tonumber(L, i + 2);
      im = 0.0;
    } else if (lua_iscomplex(L, i + 2)) {
      agn_getcmplxparts(L, i + 2, &re, &im);
    } else {
      luaL_error(L, "Error in " LUA_QS ": argument #%d is not a (complex) number, got %s.", "fma", i, luaL_typename(L, i + 2));
    }
    while (i > 0) {
      if (agn_isnumber(L, i)) {
        a = agn_tonumber(L, i);
        b = 0.0;
      } else if (lua_iscomplex(L, i)) {
        agn_getcmplxparts(L, i, &a, &b);
      } else {
        luaL_error(L, "Error in " LUA_QS ": argument #%d is not a (complex) number, got %s.", "fma", i, luaL_typename(L, i));
      }
      if (agn_isnumber(L, i + 1)) {
        c = agn_tonumber(L, i + 1);
        d = 0.0;
      } else if (lua_iscomplex(L, i + 1)) {
        agn_getcmplxparts(L, i + 1, &c, &d);
      } else {
        luaL_error(L, "Error in " LUA_QS ": argument #%d is not a (complex) number, got %s.", "fma", i, luaL_typename(L, i + 1));
      }
      re = fma(a, c, -fma(b, d, -re));
      im = fma(a, d, fma(b, c, im));
      i -= 2;
    }
    agn_pushcomplex(L, re, im);
  }
  return 1;
}


static int mathB_fact (lua_State *L) {  /* 3.16.2, partially based on mathB_gamma */
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      lua_Number x = agn_tonumber(L, 1);
      lua_pushnumber(L, tools_isint(x) ? cephes_factorial(x) : tools_gammal(x + 1));
      break;
    }
    case LUA_TCOMPLEX: {
#ifndef PROPCMPLX
      agn_createcomplex(L, cephes_cgamma(agn_tocomplex(L, 1) + 1+0*I));
#else
      lua_Number t1, re, im, si, co;
      cephes_clgam(agn_complexreal(L, 1) + 1, agn_compleximag(L, 1), &re, &im);
      t1 = sun_exp(re);
      sun_sincos(im, &si, &co);
      agn_createcomplex(L, t1*co, t1*si);
#endif
      break;
    }
    default:
      luaL_nonumorcmplx(L, 1, "fact");
  }
  return 1;
}


static int mathB_gamma (lua_State *L) {  /* 0.31.2, extended 0.33.2 */
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      if (lua_gettop(L) == 1) {
        lua_pushnumber(L, tools_gammal(agn_tonumber(L, 1)));  /* tuned by 35 % 2.17.4 */
      } else {  /* 6.4.5 extension to compute the incomplete Gamma function like Maple does */
        lua_Number a = agn_tonumber(L, 1);
        if (a < 0) {
          luaL_error(L, "Error in " LUA_QS ": first argument must be non-negative.", "gamma");
        } else {
          /* lua_pushnumber(L, tools_upperincompletegamma(a, agn_checknumber(L, 2))); */
          lua_Number z = agn_checknumber(L, 2);
          if (a == 0.0) {  /* 6.4.9 to comply with the two-argument version of Maple's GAMMA() function */
            lua_pushnumber(L, tools_ei_taylor(1, z));
          } else if (z == 0.0) {  /* ditto */
            lua_pushnumber(L, tools_gamma(a));
          } else {  /* ditto */
            lua_pushnumber(L, igamc(a, z)*tools_gamma(a));
          }
        }
      }
      break;
    }
    case LUA_TCOMPLEX: {
#ifndef PROPCMPLX
      agn_createcomplex(L, cephes_cgamma(agn_tocomplex(L, 1)));
#else
      lua_Number t1, re, im, si, co;  /* 2.12.8 */
      cephes_clgam(agn_complexreal(L, 1), agn_compleximag(L, 1), &re, &im);
      t1 = sun_exp(re);
      sun_sincos(im, &si, &co);
      agn_createcomplex(L, t1*co, t1*si);
#endif
      break;
    }
    default: {
      trydualfix(L, "gamma", 1);
      luaL_nonumorcmplxordual(L, 1, "gamma");
    }
  }
  return 1;
}


/* Reciprocal gamma function 1/GAMMA(x) 3.1.4
   There are various approaches for complex arguments on the net: all produce different results. There is a
   statement: `If you do not provide a 500-page manual with your implementation, nobody will understand what
   your complex function will do.` */
static int mathB_invgamma (lua_State *L) {
  lua_pushnumber(L, slm_rgamma(agn_checknumber(L, 1)));
  return 1;
}


static int agnB_psi (lua_State *L) {  /* 0.32.2 */
  int nargs = lua_gettop(L);
  if (nargs == 1) {
    switch (lua_type(L, 1)) {
      case LUA_TNUMBER: {
        lua_Number x = agn_tonumber(L, 1);
        lua_pushnumber(L, tools_psil(x));  /* Cephes' psi() is more accuarate than jbmath's r8_digamma() */
        break;
      }
      case LUA_TCOMPLEX: {  /* 3.1.4 */
        lua_Number a, b, re, im;
        agn_getcmplxparts(L, 1, &a, &b);
        tools_cpsi(a, b, &re, &im);
        agn_pushcomplex(L, re, im);
        break;
      }
      default: {
        trydualfix(L, "psi", 1);  /* 6.7.2 */
        luaL_nonumorcmplx(L, 1, "psi");
      }
    }
  } else if (nargs == 2) {  /* 3.7.1 */
    int n;
    lua_Number x;
    n = agn_checkinteger(L, 1);
    x = agn_checknumber(L, 2);
    switch (n) {
      case 0:
        lua_pushnumber(L, tools_digamma(x));
        break;
      case 1:
        lua_pushnumber(L, tools_trigammal(x));  /* 6.7.3 improvement by 4 digits */
        break;
      case 2:
        lua_pushnumber(L, tools_tetragammal(x));  /* 6.7.3 improvement by 4 digits */
        break;
      case 3:  /* 6.7.1 */
        lua_pushnumber(L, tools_pentagammal(x));
        break;
      default:
        luaL_error(L, "Error in " LUA_QS ": first argument must be 0, 1, 2 or 3.", "psi");
    }
  } else {
    luaL_error(L, "Error in " LUA_QS ": expected one or two arguments.", "psi");
  }
  return 1;
}


/* Inverse Digamma Function.

   Implementation of fixed point algorithm described in "Estimating a Dirichlet distribution" by Thomas P. Minka, 2000.

   Ported from https://github.com/JuliaMath/SpecialFunctions.jl/blob/master/src/gamma.jl, MIT-licenced. 6.4.6

   Maple V Release 4:

   > invPsi := y -> fsolve(Psi(x) = y, x, 0 .. infinity):

   > evalf(invPsi(1.1));
                             3.490459718
*/
static double invdigamma (double y) {
  int i;
  long double x_old, x_new, delta, tg;
  /* Closed form initial estimates */
  if (y >= -2.22L) {
     x_old = tools_expl(y) + 0.5L;
     x_new = x_old;
  } else {
     x_old = -1.0L/(y - tools_psil(1.0L));
     x_new = x_old;
  }
  /* Fixed point algorithm */
  delta = HUGE_VAL;
  for (i=0; i < 25 && delta > 1e-12L; i++) {
    tg = tools_trigammal(x_old);
    if (tools_isirregularl(tg)) return AGN_NAN;  /* better be sure than sorry */
    x_new = x_old - (tools_psil(x_old) - y)/tg;  /* trigamma is positive over the reals */
    delta = fabsl(x_new - x_old);
    x_old = x_new;
  }
  return x_new;
}

static int agnB_invpsi (lua_State *L) {
  lua_pushnumber(L, invdigamma(agn_checknumber(L, 1)));
  return 1;
}


/* base-2 logarithm of a numeric or complex argument, 1.9.3; optimised 2.1.2 */
static int agnB_log2 (lua_State *L) {
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      lua_pushnumber(L, sun_log2(agn_tonumber(L, 1)));  /* 2.14.13/2.40.0 change */
      break;
    }
    case LUA_TCOMPLEX: {
#ifndef PROPCMPLX
      agn_Complex z = agn_tocomplex(L, 1);
      if (z == 0+I*0)
        agn_createcomplex(L, AGN_NAN);
      else
        agn_createcomplex(L, tools_clog(z)*INVLN2);  /* 2.12.1 improvements */
#else
      lua_Number a, b;
      agn_getcmplxparts(L, 1, &a, &b);  /* 2.17.6 */
      if (a == 0 && b == 0)
        agn_createcomplex(L, AGN_NAN, 0);
      else
        /* agn_createcomplex(L, sun_log(sqrt(a*a + b*b))*INVLN2, sun_atan2(b, a)*INVLN2); */ /* 2.12.1 improvements */
        agn_createcomplex(L, sun_log(sun_hypot(a, b))*INVLN2, sun_atan2(b, a)*INVLN2);  /* 4.5.6 improvement */
#endif
      break;
    }
    default: {
      trydualfix(L, "log2", 1);
      luaL_nonumorcmplxordual(L, 1, "log2");
    }
  }
  return 1;
}


/* base-10 logarithm of a numeric or complex argument, 1.9.3; optimised 2.1.2 */
static int agnB_log10 (lua_State *L) {
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      lua_pushnumber(L, sun_log10(agn_tonumber(L, 1)));  /* sun_log10 cares for x <= 0, 2.11.1 tuning: four percent faster */
      break;
    }
    case LUA_TCOMPLEX: {
#ifndef PROPCMPLX
      agn_Complex z = agn_tocomplex(L, 1);
      if (z == 0+I*0)
        agn_createcomplex(L, AGN_NAN);
      else
        agn_createcomplex(L, tools_clog(z)*INVLN10);  /* clog10 is a GNU extension, so do not use it, 2.17.7 optimisation */
#else
      lua_Number a, b;
      agn_getcmplxparts(L, 1, &a, &b);  /* 2.17.6 */
      if (a == 0 && b == 0)
        agn_createcomplex(L, AGN_NAN, 0);
      else
        /* agn_createcomplex(L, sun_log(sqrt(a*a + b*b))*INVLN10, sun_atan2(b, a)*INVLN10); */ /* 2.17.7 optimisation */
        agn_createcomplex(L, sun_log(sun_hypot(a, b))*INVLN10, sun_atan2(b, a)*INVLN10);  /* 4.5.6 optimisation */
#endif
      break;
    }
    default: {
      trydualfix(L, "log10", 1);
      luaL_nonumorcmplxordual(L, 1, "log10");
    }
  }
  return 1;
}


/* Real and imaginary absolute value. Returns the argument after making sure both the real and imaginary
   parts are positive, 2.1.2. Inspired by the FRACTINT abs function. */
static int agnB_cabs (lua_State *L) {
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      lua_Number x = agn_tonumber(L, 1);
      if (lua_gettop(L) == 1)
        lua_pushnumber(L, ABS(x));
      else  /* 2.14.3 */
        agn_pushcomplex(L, ABS(x), 0);
      break;
    }
    case LUA_TCOMPLEX: {
      lua_Number a, b;
      agn_getcmplxparts(L, 1, &a, &b);  /* 2.17.6 */
      agn_pushcomplex(L, ABS(a), ABS(b));  /* 7.1.4 change */
      break;
    }
    default:
      luaL_nonumorcmplx(L, 1, "cabs");
  }
  return 1;
}


/* transforms the complex number z in Cartesian notation or the number z to polar co-ordinates. If z is a number and is zero,
   or if z is complex and its real and imaginary parts equal zero, it returns zero twice. April 16, 2013 */

static int mathB_polar (lua_State *L) {
  lua_Number x, y;
  x = 0; y = 0;  /* to avoid compiler warnings */
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER:
      x = agn_tonumber(L, 1);
      y = 0;
      break;
    case LUA_TCOMPLEX:
      agn_getcmplxparts(L, 1, &x, &y);  /* 2.17.6 */
      break;
    default:
      luaL_nonumorcmplx(L, 1, "polar");
  }
  lua_pushnumber(L, sun_hypot(x, y));  /* modulus (magnitude), 2.9.8, 2.11.0 tuning */
  lua_pushnumber(L, sun_atan2(y, x));  /* argument (phase angle), 2.11.0 tuning */
  return 2;
}


/* Returns a complex number z in Cartesian notation a + I*b for magnitude/modulus x and argument/phase angle y.
   x and y must be numbers. The result is equivalent to z = x * cis(y). See also: cis, polar. */
static int mathB_cartesian (lua_State *L) {  /* 2.11.0 */
  lua_Number x, y, si, co;
  x = agn_checknumber(L, 1);
  y = agn_checknumber(L, 2);
  sun_sincos(y, &si, &co);
  agn_pushcomplex(L, x*co, x*si);
  return 1;
}


/* returns the integral part of the logarithm of x to the base 2, i.e. extracts the exponent of the number or
   complex number x and returns it as the number entier(log2(x)), but is much faster. */
#undef FLT_RADIX
#define FLT_RADIX  2  /* better sure than sorry */
static int mathB_ilog2 (lua_State *L) {  /* 2.3.3 */
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      /* using bitshifts and lookup tables is slower !
         2.15.3 checks for zeros, too; 2.12.1 fix for non-positive arguments */
      lua_pushnumber(L, sun_ilogb(agn_tonumber(L, 1)));
      break;
    }
    case LUA_TCOMPLEX: {
#ifndef PROPCMPLX
      agn_Complex z;
      z = agn_tocomplex(L, 1);
      if (z == 0 + I*0) {
        agn_createcomplex(L, AGN_NAN);
      } else {
        agn_createcomplex(L, tools_centier(tools_clog(z)*INVLN2));  /* 2.12.1 improvements */
      }
#else
      lua_Number a, b, x, y;
      agn_getcmplxparts(L, 1, &a, &b);  /* 2.17.6 */
      if (a == 0 && b == 0) {
        agn_createcomplex(L, AGN_NAN, 0);
      } else {
        tools_centier(sun_log(sun_hypot(a, b))*INVLN2, sun_atan2(b, a)/LN2, &x, &y);  /* 2.12.1 improvements */
        agn_createcomplex(L, x, y);
      }
#endif
      break;
    }
    default:
      luaL_nonumorcmplx(L, 1, "ilog2");
  }
  return 1;
}


static unsigned int const PowersOf10[] =
    {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000};

static int mathB_ilog10 (lua_State *L) {  /* 2.3.3, taken from: http://graphics.stanford.edu/~seander/bithacks.html#IntegerLog10 */
  lua_Number x;
  x = agn_checknumber(L, 1);
  if (tools_isint(x)) {  /* 2.30.3 */
    if (x < 1)
      lua_pushundefined(L);
    else {
      int t;
      t = (lua_log2(L, x) + 1) * 1233 >> 12;
      if (t > 9)  /* 2.8.2, better be sure than sorry */
        luaL_error(L, "Error in " LUA_QS ": internal error (t = %d).", "ilog10", t);
      lua_pushnumber(L, t - ((LUAI_UINT32)x < PowersOf10[t]));
    }
  } else {
    lua_pushnumber(L, sun_ilog10(x));
  }
  return 1;
}


/* The rectangular function rect(x) is defined as:

             { 1 if |x| < 0.5
   rect(x) = { 0.5 if |x| = 0.5
             { 0 if |x| > 0.5  */

static int mathB_rect (lua_State *L) {  /* 2.4.2 */
  lua_Number x = fabs(agn_checknumber(L, 1));
  if (x > 0.5) lua_pushnumber(L, 0);
  else if (x == 0.5) lua_pushnumber(L, 0.5);
  else lua_pushnumber(L, 1);
  return 1;
}


/* simplified version of Donald Knuth's approximation algorithm published
   in `Seminumerical Algorithms` by checking whether the _relative_ error is
   bound to a given tolerance; patched 0.26.1, 10.08.2009; patched 0.26.4, 20.08.2009;
   extended 0.32.1, 17.05.2010
   Theodore C. Belding's fcmp-1.2.2 frexp/ldexp variant is 50 percent slower and not flexible enough. */
static int mathB_approx (lua_State *L) {
  lua_Number a1, a2, b1, b2, eps, dist;
  int ttype1, ttype2;
  eps = (lua_gettop(L) == 2) ? agn_getepsilon(L) : agn_checknumber(L, 3);  /* 2.1.4, faster than agnL_optnumber */
  ttype1 = lua_type(L, 1); ttype2 = lua_type(L, 2);
  if (ttype1 == LUA_TNUMBER && ttype2 == LUA_TNUMBER) {
    a1 = agn_tonumber(L, 1);
    b1 = agn_tonumber(L, 2);
    lua_pushboolean(L, tools_approx(a1, b1, eps));  /* 2.31.11 change, includes check for +/- inf */
    return 1;
  } else if (ttype1 == LUA_TCOMPLEX && ttype2 == LUA_TNUMBER) {
    agn_getcmplxparts(L, 1, &a1, &b1);  /* 2.17.6 */
    a2 = agn_tonumber(L, 2); b2 = 0;
  } else if (ttype1 == LUA_TNUMBER && ttype2 == LUA_TCOMPLEX) {
    agn_getcmplxparts(L, 2, &a2, &b2);  /* 2.17.6 */
    a1 = agn_tonumber(L, 1); b1 = 0;
  } else if (ttype1 == LUA_TCOMPLEX && ttype2 == LUA_TCOMPLEX) {
    agn_getcmplxparts(L, 1, &a1, &b1);  /* 2.17.6 */
    agn_getcmplxparts(L, 2, &a2, &b2);  /* 2.17.6 */
  } else {  /* improved 2.3.4 */
    a2 = b1 = b2 = 0;  /* just to avoid compiler warnings */
    a1 = !(lua_type(L, 1) == LUA_TNUMBER || lua_type(L, 1) == LUA_TCOMPLEX);
    luaL_nonumorcmplx(L, 2 - a1, "approx");  /* 2.31.11 message fix */
  }
  dist = sun_hypot(a2 - a1, b2 - b1);  /* 2.9.8 improvement */
  lua_pushboolean(L,
    dist < eps || (dist <= (eps * fMax(fabs(a1), fabs(b1))) && dist <= (eps * fMax(fabs(a2), fabs(b2)))) || (tools_isnan(a1) && tools_isnan(a2)));  /* 1.9.3 */
  return 1;
}


/* heaviside := proc(x :: number, z) is
   case
      of x > 0 -> return 1
      of x < 0 -> return 0
      else return if unassigned z then undefined else z fi esle
   esac
end; 6.7.8, 24 percent faster than the Agena version. */
static int mathB_heaviside (lua_State *L) {
  int32_t hx, ix;
  uint32_t lx, sx;
  lua_Number x = agn_checknumber(L, 1);
  EXTRACT_WORDS(hx, lx, x);
  /* sx: sign of x, 0 if x >= +0 and non-zero (negative on Intel at least) otherwise */
  sx = hx & 0x80000000;
  ix = hx ^ sx;  /* |x| */
  if (l_unlikely(ix >= 0x7ff00000)) {  /* heaviside(NaN,INF) is itself */
    lua_settop(L, 1);
  } else if ((ix | lx) == 0) {  /* x = +/-0 ? */
    if (lua_gettop(L) == 2) {  /* any second arg given ? -> return it, regardless of its type. */
      lua_settop(L, 2);
    } else {
      lua_pushundefined(L);
    }
  } else {
    lua_pushinteger(L, (!sx) ? 1 : 0);
  }
  return 1;
}


static int agnB_put (lua_State *L) {  /* formerly tinsert */
  int nops = lua_gettop(L);
  switch(lua_type(L, 1)) {
    case LUA_TTABLE: {
      int e = luaL_getn(L, 1) + 1;  /* first empty element */
      int pos;  /* where to insert new element */
      switch (nops) {
        case 2: {  /* called with only 2 arguments */
          pos = e;  /* insert new element at the end */
          break;
        }
        case 3: {
          int i;
          pos = agnL_checkint(L, 2);  /* 2nd argument is the position */
          if (pos > e) e = pos;  /* `grow' array if necessary */
          for (i = e; i > pos; i--) {  /* move up elements */
            lua_rawgeti(L, 1, i - 1);
            lua_rawseti(L, 1, i);  /* t[i] = t[i-1] */
          }
          break;
        }
        default: {
          return luaL_error(L, "wrong number of arguments to " LUA_QL("put") ", two or three required.");
        }
      }
      luaL_setn(L, 1, e);  /* new size */
      lua_rawseti(L, 1, pos);  /* t[pos] = v */
      break;
    }
    case LUA_TSEQ: {  /* 0.31.5 */
      int e = agn_seqsize(L, 1) + 1;  /* first empty element */
      int pos;  /* where to insert new element */
      switch (nops) {
        case 2: {  /* called with only 2 arguments */
          pos = e;  /* insert new element at the end */
          break;
        }
        case 3: {
          int i;
          pos = agnL_checkint(L, 2);  /* 2nd argument is the position */
          if (pos > e) e = pos;  /* `grow' array if necessary */
          for (i = e; i > pos; i--) {  /* move up elements */
            lua_seqrawgeti(L, 1, i - 1);   /* get old value; if pos is non-positive, lua_rawseqgeti issues an error */
            lua_seqrawseti(L, 1, i);
          }
          break;
        }
        default: {
          return luaL_error(L, "Error in " LUA_QS ": wrong number of arguments, two or three required.", "put");
        }
      }
      lua_seqrawseti(L, 1, pos);  /* value at the top is the last argument of the function call, i.e. the element to be inserted */
      break;
    }
    case LUA_TREG: {  /* 2.9.1 */
      int i, pos, curtop;  /* where to insert new element */
      curtop = agn_reggettop(L, 1);  /* get current top */
      switch (nops) {
        case 2: {  /* called with only 2 arguments */
          for (i=curtop; i > 0; i--) {
            agn_regrawgeti(L, 1, i);
            if (!lua_isnil(L, -1)) {
              agn_poptop(L);
              break;
            }
            agn_poptop(L);
          }
          pos = i + 1;  /* insert new element at the end */
          if (pos > curtop)
            luaL_error(L, "Error in " LUA_QS ": register is full.", "put");
          break;
        }
        case 3: {
          int last;
          pos = agnL_checkint(L, 2);  /* 2nd argument is the position */
          /* is there at least one null value at the current top ? */
          for (i=curtop; i > 0; i--) {
            agn_regrawgeti(L, 1, i);
            if (!lua_isnil(L, -1)) {
              agn_poptop(L);
              break;
            }
            agn_poptop(L);
          }
          if (i >= curtop)
            luaL_error(L, "Error in " LUA_QS ": register is full.", "put");
          /* move up elements, i is the position of the last non-null value */
          last = i;
          for (i=last; i >= pos; i--) {
            agn_regrawgeti(L, 1, i);
            agn_regrawseti(L, 1, i + 1);
          }
          break;
        }
        default: {
          return luaL_error(L, "Error in " LUA_QS ": wrong number of arguments, two or three required.", "put");
        }
      }
      agn_regrawseti(L, 1, pos);  /* value at the top is the last argument of the function call, i.e. the element to be inserted */
      break;
    }
    default:
      luaL_error(L, "Error in " LUA_QS ": table, sequence or register expected as first argument, got %s.", "put",
        luaL_typename(L, 1));
  }
  lua_settop(L, 1);  /* return modified structure, 2.31.2 */
  return 1;
}


static int agnB_prepend (lua_State *L) {  /* formerly tinsert; C implementation 3.9.3, based on agnB_put. */
  int i, e;
  if (lua_gettop(L) != 2)
    luaL_error(L, "Error in " LUA_QS ": two arguments expected.", "prepend");
  switch(lua_type(L, 2)) {
    case LUA_TTABLE: {
      size_t a[2];
      agn_borders(L, 2, a);  /* traverse both array and hash part */
      e = a[1] + 1;  /* first empty element */
      for (i = e; i > 1; i--) {  /* move up elements */
        lua_rawgeti(L, 2, i - 1);
        lua_rawseti(L, 2, i);  /* t[i] = t[i-1] */
      }
      luaL_setn(L, 2, e);  /* new size */
      lua_pushvalue(L, 1);
      lua_rawseti(L, 2, 1);  /* t[pos] = v */
      break;
    }
    case LUA_TSEQ: {
      e = agn_seqsize(L, 2) + 1;  /* first empty element */
      for (i = e; i > 1; i--) {  /* move up elements */
        lua_seqrawgeti(L, 2, i - 1);  /* get old value; if pos is non-positive, lua_rawseqgeti issues an error */
        lua_seqrawseti(L, 2, i);
      }
      lua_pushvalue(L, 1);
      lua_seqrawseti(L, 2, 1);  /* value at the top is the last argument of the function call, i.e. the element to be inserted */
      break;
    }
    case LUA_TREG: {
      int curtop = agn_reggettop(L, 2);  /* get current top */
      agn_regresize(L, 2, curtop + 1, 0);   /* extend by one element */
      /* move up elements, i is the position of the last non-null value */
      for (i=curtop; i >= 1; i--) {
        agn_regrawgeti(L, 2, i);
        agn_regrawseti(L, 2, i + 1);
      }
      lua_pushvalue(L, 1);
      agn_regrawseti(L, 2, 1);  /* value at the top is the last argument of the function call, i.e. the element to be inserted */
      break;
    }
    case LUA_TPAIR: {
      /* prevent a circumpair, so copy its components to a new pair */
      /* check for properly allocated slots will be done by agn_pairgetiall() */
      agn_pairgetiall(L, 2);  /* 7.7.8 */
      agn_createpair(L, -2, -1);
      agn_pairseti(L, 2, 2);
      agn_poptoptwo(L);
      lua_pushvalue(L, 1);
      agn_pairseti(L, 2, 1);
      break;
    }
    default:
      luaL_error(L, "Error in " LUA_QS ": table, sequence, register or pair expected as first argument, got %s.", "prepend",
        luaL_typename(L, 1));
  }
  return 1;
}


static int agnB_append (lua_State *L) {
  if (lua_gettop(L) != 2)
    luaL_error(L, "Error in " LUA_QS ": two arguments expected.", "append");
  if (lua_isreg(L, 2)) {  /* extend by one element */
    agn_regresize(L, 2, agn_reggettop(L, 2) + 1, 0);
  }
  agn_structinsert(L, 2, 1);
  return 1;
}


/* include(obj, x, ...)
   Appends values x, ... to table, set, sequence, register obj in-place and returns the modified structure.
   See also: copyadd, insert statement, prepend, put, purge. 3.9.3 */
static int agnB_include (lua_State *L) {
  int nargs, i;
  nargs = lua_gettop(L);
  if (nargs < 2)
    luaL_error(L, "Error in " LUA_QS ": two or more arguments expected.", "include");
  for (i=2; i <= nargs; i++) {
    agn_structinsert(L, 1, i);
  }
  lua_pushvalue(L, 1);
  return 1;
}


static void masspurge (lua_State *L, size_t (*sizef)(lua_State *, int),
                                       void (*readf)(lua_State *, int, size_t),
                                      int (*writef)(lua_State *, int, size_t) ) {
  int i, c, e, startpos, endpos, elemstbp;
  c = 0;
  e = sizef(L, 1);
  startpos = tools_posrelat(agnL_checkinteger(L, 2), e);
  endpos = tools_posrelat(agnL_checkinteger(L, 3), e);
  if (endpos > e) endpos = e;
  if (!(1 <= startpos && startpos <= e) || !(1 <= endpos && endpos <= e)) return;  /* nothing to remove */
  if (startpos > endpos)
    luaL_error(L, "Error in " LUA_QS ": start %d > end %d.", "purge", startpos, endpos);
  elemstbp = endpos - startpos + 1;
  for (i=endpos + 1; i <= e; i++) {  /* move excess elements to close the space */
    readf(L, 1, i);
    writef(L, 1, startpos++);
    c++;
  }
  /* nullify the rest of the array starting from the end to avoid elements leftover in sequences */
  for (i=endpos + c; i >= startpos; i--) {
    lua_pushnil(L);
    writef(L, 1, i);
  }
  if (lua_isreg(L, 1)) {
    lua_pushinteger(L, e - elemstbp);
    agn_regsettop(L, 1);
  }
}

static int agnB_purge (lua_State *L) {  /* based on tables.remove */
  if (lua_gettop(L) < 3) {
    switch (lua_type(L, 1)) {
      case LUA_TTABLE: {
        int len, pos, rc;
        len = luaL_getn(L, 1);
        pos = agnL_optinteger(L, 2, len);
        if (!(1 <= pos && pos <= len))  /* 5.1.3 patch; position is outside bounds? */
          return 0;  /* nothing to remove */
        lua_rawgeti(L, 1, pos);  /* fetch result (the value to be dropped) */
        rc = agn_tabpurge(L, 1, len, pos);  /* 3.9.0 change */
        if (!rc) agn_poptop(L);  /* level the stack before returning nothing */
        return rc;
      }
      case LUA_TSEQ: {  /* 0.31.5 */
        int e = agn_seqsize(L, 1);
        int pos = tools_posrelat(agnL_optinteger(L, 2, e), e);  /* 3.9.0 improvement */
        if (!(1 <= pos && pos <= e))  /* 5.1.3 patch; position is outside bounds? */
          return 0;  /* nothing to remove */
        lua_seqrawgeti(L, 1, pos);  /* result = t[pos]; */
        lua_pushnil(L);
        lua_seqrawseti(L, 1, pos);  /* t[e] = null, moves all elements at the right to the left */
        return 1;
      }
      case LUA_TREG: {  /* 2.3.0 RC 3 */
        int e = agn_reggettop(L, 1);
        int pos = tools_posrelat(agnL_optinteger(L, 2, e), e);  /* 3.9.0 improvement */
        if (!(1 <= pos && pos <= e))  /* 5.1.3 patch; position is outside bounds? */
          return 0;  /* nothing to remove */
        agn_regrawgeti(L, 1, pos);  /* return former element */
        agn_regpurge(L, 1, pos);
        return 1;
      }
      default:
        luaL_error(L, "Error in " LUA_QS ": table, sequence or register expected as first argument, got %s.", "purge",
          luaL_typename(L, 1));
    }
    return 0;
  } else {
    /* 2.27.7 purge a consecutive range of elements from a structure, move excess elements down to close the space; the
       function automatically performs a garbage collection after shifting. */
    switch(lua_type(L, 1)) {
      case LUA_TTABLE: {
        int i, c, e, startpos, endpos, elemstbp;
        c = 0;
        e = luaL_getn(L, 1);
        startpos = tools_posrelat(agnL_checkinteger(L, 2), e);
        endpos = tools_posrelat(agnL_checkinteger(L, 3), e);
        if (endpos > e) endpos = e;
        if (!(1 <= startpos && startpos <= e) || !(1 <= endpos && endpos <= e))
          return 0;  /* nothing to remove */
        if (startpos > endpos)
          luaL_error(L, "Error in " LUA_QS ": start %d > end %d.", "purge", startpos, endpos);
        elemstbp = endpos - startpos + 1;
        (void)elemstbp;  /* luaL_setn, see below, mostly is (void)0 */
        for (i=endpos + 1; i <= e; i++) {  /* move excess elements to close the space */
          lua_rawgeti(L, 1, i);
          lua_rawseti(L, 1, startpos++);
          c++;
        }
        for (i=endpos + c; i >= startpos; i--) {  /* nullify the rest of the array */
          lua_pushnil(L);
          lua_rawseti(L, 1, i);
        }
        luaL_setn(L, 1, e - elemstbp);  /* t.n = n - (#elems to be purged) */
        break;
      }
      case LUA_TSEQ: {
        masspurge(L, agn_seqsize, lua_seqrawgeti, lua_seqrawseti);
        break;
      }
      case LUA_TREG: {
        masspurge(L, agn_reggettop, agn_regrawgeti, agn_regrawseti);
        break;
      }
      default:
        luaL_error(L, "Error in " LUA_QS ": table, sequence or register expected as first argument, got %s.", "purge",
          luaL_typename(L, 1));
      lua_gc(L, LUA_GCCOLLECT, 0);
    }
    return 0;
  }
}


static int agnB_deletefrom (lua_State *L) {  /* based on purge */
  luaL_checkany(L, 2);
  switch (lua_type(L, 2)) {
    case LUA_TTABLE: case LUA_TSET: case LUA_TSEQ: case LUA_TREG:
      lua_pushinteger(L, agn_deletefrom(L, 1, 2));
      break;
    default:
      luaL_error(L, "Error in " LUA_QS ": table, set, sequence or register expected as first argument, got %s.", "deletefrom",
        luaL_typename(L, 1));
  }
  return 1;
}


/* When called with no second argument, reduces the number of allocated slots in table, sequence or register o to the number
   of slots actually assigned a value, also freeing memory if possible.

   With registers, also removes all trailing null's. With tables removes holes in the array part and closes the space by shifting
   down the other elements, if necessary.

   If the optional argument `newsize', a non-negative integer, is given, then the number of pre-allocated slots is reduced to
   the number of actually assigned values, purging all surplus elements that may exist and giving back memory to the interpreter.

   The function works in-place and returns nothing. It automatically conducts a garbage collection before returning. 4.6.3 */
static int agnB_pack (lua_State *L) {
  int nops, slots;
  switch (lua_type(L, 1)) {
    case LUA_TTABLE: {
      nops = agn_asize(L, 1);
      slots = agnL_optnonnegint(L, 2, nops);
      if (nops == 0) {
        agn_cleanse(L, 1, 0);
      } else {
        agn_resize(L, 1, slots);
      }
      break;
    }
    case LUA_TSEQ: {
      nops = agn_seqsize(L, 1);
      slots = agnL_optnonnegint(L, 2, nops);
      agn_seqresize(L, 1, nops == 0 ? 0 : slots);
      /* agn_seqresize never actually shrinks a sequence to zero to avoid crashes, so now purge the last element if requested. */
      if (nops > 0 && slots == 0) {
        lua_pushnil(L);
        lua_seqrawseti(L, 1, 1);
      }
      break;
    }
    case LUA_TREG: {
      nops = agn_reggettop(L, 1);
      slots = agnL_optnonnegint(L, 2, nops);
      agn_regresize(L, 1, slots, 1);  /* with 0 slots, agn_regreduce deals with them appropriately; 4th argument nil's surplus values. */
      break;
    }
    default:
      luaL_error(L, "Error in " LUA_QS ": table, sequence or register expected as first argument, got %s.", "pack",
        luaL_typename(L, 1));
  }
  lua_gc(L, LUA_GCCOLLECT, 0);  /* 4.6.3 */
  return 0;
}


/* Check options for linalg.extend and linalg.countitems */
void aux_copyaddcheckoptions (lua_State *L, int pos, int *nargs, int *deepcopy, int *inplace, const char *procname) {
  int checkoptions;
  *deepcopy = 1;  /* 0 = deepcopy first structure, 1 = do not */
  *inplace = 0;   /* 0 = do not work in-place, 1 = do so */
  checkoptions = 2;  /* check n options; CHANGE THIS if you add/delete options */
  while (checkoptions-- && *nargs >= pos && lua_ispair(L, *nargs)) {
    /* check for properly allocated slots will be done by agn_pairgetiall() */
    agn_pairgetiall(L, *nargs);  /* get left, right value, set to stack index -2, -1, 7.7.8 */
    if (agn_isstring(L, -2)) {
      const char *option = agn_tostring(L, -2);
      if (tools_streq("deepcopy", option)) {
        *deepcopy = agn_checkboolean(L, -1);
        (*nargs)--;
      } else if (tools_streq("inplace", option)) {
        *inplace = agn_checkboolean(L, -1);
        (*nargs)--;
      }
      /* do not issue an error with any other pair for you might want to insert it later on. */
    }
    agn_poptoptwo(L);
  }
}

static int agnB_copyadd (lua_State *L) {  /* 2.34.2 */
  int nargs, i, deepcopy, inplace;
  nargs = agnL_gettop(L, "need at least one argument", "copyadd");
  aux_copyaddcheckoptions(L, nargs, &nargs, &deepcopy, &inplace, "copyadd");
  if (inplace && !deepcopy) {
    luaL_error(L, "Error in " LUA_QS ": cannot mix options.", "copyadd");
  }
  if (inplace) {  /* 4.10.3 */
    int type = lua_type(L, 1);
    luaL_typecheck(L, type == LUA_TTABLE || type == LUA_TSEQ || type == LUA_TREG, 1, "table, sequence or register expected", type);
    lua_pushvalue(L, 1);
  } else if (deepcopy) {
    int n;
    agn_copy(L, 1, 3);
    if (lua_isseq(L, -1)) {
      n = agn_seqsize(L, -1);
      agn_seqresize(L, -1, n + nargs - 1);
    } else if (lua_isreg(L, -1)) {  /* 4.10.2 fix */
      n = agn_regsize(L, -1);
      agn_regresize(L, -1, n + nargs - 1, 1);
    }
  } else if (lua_istable(L, 1)) {  /* 4.10.2 extension */
    size_t a[3];
    agn_tablesize(L, 1, a);
    luaL_checkstack(L, 5, "not enough stack space");
    lua_createtable(L, a[0] + nargs - 1, a[2]);
    lua_pushnil(L);
    while (lua_next(L, 1)) {  /* push key value pairs of original table into a new table */
      lua_pushvalue(L, -2);   /* push key */
      lua_pushvalue(L, -2);   /* push value */
      lua_settable(L, -5);    /* insert them into the new table and pop duplicate key, value */
      agn_poptop(L);          /* pop value */
    }
  } else if (lua_isseq(L, 1)) {
    int n = agn_seqsize(L, 1);
    agn_createseq(L, n + nargs - 1);
    for (i=1; i <= n; i++) {
      lua_seqrawgeti(L, 1, i);
      lua_seqrawseti(L, -2, i);
    }
  } else if (lua_isreg(L, 1)) {
    int n = agn_regsize(L, 1);
    agn_createreg(L, n + nargs - 1);
    for (i=1; i <= n; i++) {
      agn_regrawgeti(L, 1, i);
      agn_regrawseti(L, -2, i);
    }
  } else {
    luaL_error(L, "Error in " LUA_QS ": table, sequence or register expected as first argument, got %s.", "copyadd",
      luaL_typename(L, 1));
  }
  for (i=2; i <= nargs; i++) {
    agn_structinsert(L, -1, i);
  }
  return 1;
}


static int agnB_copy (lua_State *L) {  /* 2.34.4 */
  int mode, nargs;
  nargs = agnL_gettop(L, "need at least one argument", "copy");
  if (nargs == 2) {
    static const char *const options[] = {"both", "array", "hash", "nometa", NULL};
    static const int vals[] = {3, 1, 2, 7};
    mode = vals[luaL_checkoption(L, 2, "both", options)];  /* get array and hash part of tables by default */
  } else
    mode = 3;
  agn_copy(L, 1, mode);
  return 1;
}


static void aux_checkuniqueoptions (lua_State *L, int pos, int *nargs, int *approx, int *inplace) {  /* new 7.6.4 */
  *approx = 0;
  *inplace = 0;
  if (*nargs >= pos && lua_isboolean(L, *nargs)) {
    *approx = agn_istrue(L, *nargs);
    (*nargs)--;
  } else {
    int checkoptions = 2;  /* check n options; CHANGE THIS if you add/delete options */
    while (checkoptions-- && *nargs >= pos && lua_ispair(L, *nargs)) {
      /* check for properly allocated slots will be done by agn_pairgetiall() */
      agn_pairgetiall(L, *nargs);  /* get left, right value, set to stack index -2, -1, 7.7.8 */
      if (agn_isstring(L, -2)) {
        const char *option = agn_tostring(L, -2);
        if (tools_streq(option, "approx")) {
          *approx = agn_checkboolean(L, -1);
        } else if (tools_streq(option, "inplace")) {
          *inplace = agn_checkboolean(L, -1);
        } else {
          agn_poptoptwo(L);
          luaL_error(L, "Error in " LUA_QS ": unknown option " LUA_QS ".", "unique", option);
        }
      }
      /* do not call lua_settop as it would corrupt the argument stack since we have already pushed values */
      (*nargs)--;
      agn_poptoptwo(L);
    }
  }
}

static int agnB_unique (lua_State *L) {  /* 2.34.4 */
  int nargs, approx, inplace;
  luaL_checkany(L, 1);
  nargs = lua_gettop(L);
  aux_checkuniqueoptions(L, 2, &nargs, &approx, &inplace);  /* new 7.6.4 */
  agn_unique(L, 1, approx, inplace);
  return 1;
}


static int agnB_values (lua_State *L) {  /* 2.34.5 */
  int nargs = lua_gettop(L);
  if (nargs < 2)
    luaL_error(L, "Error in " LUA_QS ": need at least two arguments, got %d.", "values", nargs);
  agn_values(L, 1, nargs);
  return 1;
}


static int agnB_times (lua_State *L) {  /* 2.34.5 */
  int nargs = lua_gettop(L);
  if (nargs < 3)
    luaL_error(L, "Error in " LUA_QS ": need at least three arguments, got %d.", "times", nargs);
  agn_times(L, 1, nargs);
  return 1;
}


#define isaux(L, type, procname) { \
  int nargs, i; \
  nargs = agnL_gettop(L, "need at least one argument", procname); \
  for (i=1; i <= nargs; i++) { \
    if (lua_type(L, i) != type) { \
      lua_pushfalse(L); \
      return 1; \
    } \
  } \
  lua_pushtrue(L); \
  return 1; \
}


static int agnB_isboolean (lua_State *L) {  /* extended 0.34.2 */
  isaux(L, LUA_TBOOLEAN, "isboolean");
}


static int agnB_isnumber (lua_State *L) {  /* extended 0.34.2 */
  isaux(L, LUA_TNUMBER, "isnumber");
}


static int agnB_iscomplex (lua_State *L) {  /* extended 0.34.2 */
  isaux(L, LUA_TCOMPLEX, "iscomplex");
}


static int agnB_isnumeric (lua_State *L) {  /* extended 0.34.2 */
  int nargs, i, t;
  nargs = agnL_gettop(L, "need at least one argument", "isnumeric");
  for (i=1; i <= nargs; i++) {
    t = lua_type(L, i);
    if (t != LUA_TNUMBER && t != LUA_TCOMPLEX) {
      lua_pushfalse(L);
      return 1;
    }
  }
  lua_pushtrue(L);
  return 1;
}


static int agnB_isstring (lua_State *L) {  /* extended 0.34.2 */
  isaux(L, LUA_TSTRING, "isstring");
}


static int agnB_istable (lua_State *L) {  /* extended 0.34.2 */
  isaux(L, LUA_TTABLE, "istable");
}


static int agnB_isset (lua_State *L) {  /* extended 0.34.2 */
  isaux(L, LUA_TSET, "isset");
}


static int agnB_isseq (lua_State *L) {  /* extended 0.34.2 */
  isaux(L, LUA_TSEQ, "isseq");
}


static int agnB_ispair (lua_State *L) {  /* extended 0.34.2 */
  isaux(L, LUA_TPAIR, "ispair");
}


static int agnB_isreg (lua_State *L) {  /* 2.3.0 RC 3 */
  isaux(L, LUA_TREG, "isreg");
}


static int agnB_isstructure (lua_State *L) {  /* extended 0.34.2 */
  int nargs, i;
  nargs = agnL_gettop(L, "need at least one argument", "isstructure");
  for (i=1; i <= nargs; i++) {
    if (lua_type(L, i) < LUA_TTABLE) {
      lua_pushfalse(L);
      return 1;
    }
  }
  lua_pushtrue(L);
  return 1;
}


static int agnB_isint (lua_State *L) {  /* 1.6.0 */
  int nargs, i;
  nargs = agnL_gettop(L, "need at least one argument", "isint");
  for (i=1; i <= nargs; i++) {
    if (lua_type(L, i) != LUA_TNUMBER) {
      lua_pushfail(L);
      return 1;
    } else if (tools_isfrac(agn_tonumber(L, i))) {  /* 2.14.13 change */
      lua_pushfalse(L);
      return 1;
    }
  }
  lua_pushtrue(L);
  return 1;
}


static int agnB_isposint (lua_State *L) {  /* extended 0.34.2 */
  int nargs, i;
  nargs = agnL_gettop(L, "need at least one argument", "isposint");
  for (i=1; i <= nargs; i++) {
    if (lua_type(L, i) != LUA_TNUMBER) {
      lua_pushfail(L);
      return 1;
    } else if (!tools_isposint(agn_tonumber(L, i))) {  /* 2.14.13 change */
      lua_pushfalse(L);
      return 1;
    }
  }
  lua_pushtrue(L);
  return 1;
}


static int agnB_ispositive (lua_State *L) {  /* 1.11.3 */
  int nargs, i;
  nargs = agnL_gettop(L, "need at least one argument", "ispositive");
  for (i=1; i <= nargs; i++) {
    if (lua_type(L, i) != LUA_TNUMBER) {
      lua_pushfail(L);
      return 1;
    } else if (agn_tonumber(L, i) <= 0) {
      lua_pushfalse(L);
      return 1;
    }
  }
  lua_pushtrue(L);
  return 1;
}


static int agnB_isnegative (lua_State *L) {  /* 1.11.3 */
  int nargs, i;
  nargs = agnL_gettop(L, "need at least one argument", "isnegative");
  for (i=1; i <= nargs; i++) {
    if (lua_type(L, i) != LUA_TNUMBER) {
      lua_pushfail(L);
      return 1;
    } else if (agn_tonumber(L, i) >= 0) {
      lua_pushfalse(L);
      return 1;
    }
  }
  lua_pushtrue(L);
  return 1;
}


static int agnB_isnonneg (lua_State *L) {  /* 1.11.3 */
  int nargs, i;
  nargs = agnL_gettop(L, "need at least one argument", "isnonneg");
  for (i=1; i <= nargs; i++) {
    if (lua_type(L, i) != LUA_TNUMBER) {
      lua_pushfail(L);
      return 1;
    } else if (agn_tonumber(L, i) < 0) {
      lua_pushfalse(L);
      return 1;
    }
  }
  lua_pushtrue(L);
  return 1;
}


static int agnB_isnonnegint (lua_State *L) {  /* extended 0.34.2 */
  int nargs, i;
  nargs = agnL_gettop(L, "need at least one argument", "isnonnegint");
  for (i=1; i <= nargs; i++) {
    if (lua_type(L, i) != LUA_TNUMBER) {
      lua_pushfail(L);
      return 1;
    } else if (!tools_isnonnegint(agn_tonumber(L, i))) {  /* 2.14.13 change */
      lua_pushfalse(L);
      return 1;
    }
  }
  lua_pushtrue(L);
  return 1;
}


static int agnB_isnonzeroint (lua_State *L) {  /* 4.11.0 */
  int nargs, i;
  nargs = agnL_gettop(L, "need at least one argument", "isnonzeroint");
  for (i=1; i <= nargs; i++) {
    if (lua_type(L, i) != LUA_TNUMBER) {
      lua_pushfail(L);
      return 1;
    } else if (!tools_isnonzeroint(agn_tonumber(L, i))) {
      lua_pushfalse(L);
      return 1;
    }
  }
  lua_pushtrue(L);
  return 1;
}


static int agnB_isnonposint (lua_State *L) {  /* 1.1.0 */
  int nargs, i;
  nargs = agnL_gettop(L, "need at least one argument", "isnonposint");
  for (i=1; i <= nargs; i++) {
    if (lua_type(L, i) != LUA_TNUMBER) {
      lua_pushfail(L);
      return 1;
    } else {
      if (!tools_isnonposint(agn_tonumber(L, i))) {  /* 2.14.13/3.7.8 change */
        lua_pushfalse(L);
        return 1;
      }
    }
  }
  lua_pushtrue(L);
  return 1;
}


static int agnB_isnegint (lua_State *L) {  /* extended 0.34.2 */
  int nargs, i;
  nargs = agnL_gettop(L, "need at least one argument", "isnegint");
  for (i=1; i <= nargs; i++) {
    if (lua_type(L, i) != LUA_TNUMBER) {
      lua_pushfail(L);
      return 1;
    } else if (!tools_isnegint(agn_tonumber(L, i))) {  /* 2.14.13 change */
      lua_pushfalse(L);
      return 1;
    }
  }
  lua_pushtrue(L);
  return 1;
}


static int agnB_isall (lua_State *L) {  /* 4.6.1 */
  switch (lua_type(L, 1)) {
    case LUA_TTABLE:
      agn_tblisall(L, 1, "isall");
      break;
    case LUA_TSEQ:
      agn_seqisall(L, 1, "isall");
      break;
    case LUA_TREG:
      agn_regisall(L, 1, "isall");
      break;
    case LUA_TSET:  /* 4.6.2 */
      agn_usisall(L, 1, "isall");
      break;
    default:
      luaL_error(L, "Error in " LUA_QS ": table, set, sequence or register expected, got %s.", "isall",
        luaL_typename(L, 1));
  }
  return 1;
}


#define castx(x, issigned) ((issigned) ? (LUA_INT32)((x)) : (LUAI_UINT32)((x)))
#define INTBITS (sizeof(LUAI_UINT32)*CHAR_BIT)  /* 2.12.4 */

static int agnB_setbit (lua_State *L) {  /* 1.6.5 */
  int flag;
  lu_byte issigned;
  lua_Number x, pos;
  issigned = agn_getbitwise(L);
  if (lua_gettop(L) != 3)
    luaL_error(L, "Error in " LUA_QS ": need three arguments.", "setbit");
  x = agn_checkinteger(L, 1);  /* 2.13.0 */
  pos = agn_checkinteger(L, 2);
  if (pos < 1 || pos > INTBITS)
    luaL_error(L, "Error in " LUA_QS ": second argument must be positive and less than 32.", "setbit");
  else
    pos = sun_trunc(pos);
  if (lua_isboolean(L, 3)) {
    flag = lua_toboolean(L, 3);  /* true or false passed ? */
    if (flag < 0)  /* fail ? 2.26.2 fix */
      luaL_error(L, "Error in " LUA_QS ": fourth argument must be either Boolean, or 0 or 1.", "setbit");
  } else if (agn_isnumber(L, 3)) {
    flag = lua_tointeger(L, 3);  /* 1 or 0 passed ? */
    if (flag != 0 && flag != 1)
      luaL_error(L, "Error in " LUA_QS ": third argument must be either Boolean, or 0 or 1.", "setbit");
  } else {
    flag = 0;  /* to prevent compiler warnings */
    luaL_error(L, "Error in " LUA_QS ": third argument must be either Boolean, or 0 or 1.", "setbit");
  }
  lua_pushinteger(L,  /* 2.3.3 optimisation, see
    http://stackoverflow.com/questions/17803889/set-or-reset-a-given-bit-without-branching */
    (castx(x, issigned) & ~(1 << (castx(pos, issigned) - 1))) | (flag << (castx(pos, issigned) - 1)));
  return 1;
}


static int agnB_setbits (lua_State *L) {  /* 2.4.0 */
  int flag, top, i;
  lu_byte issigned;
  lua_Number x;
  issigned = agn_getbitwise(L);
  if (lua_gettop(L) != 2)
    luaL_error(L, "Error in " LUA_QS ": need two arguments.", "setbits");  /* 2.31.2 fix */
  x = agn_checkinteger(L, 1);  /* 2.13.0 */
  if (!(lua_type(L, 2) == LUA_TREG))  /* 2.10.0 patch */
    luaL_error(L, "Error in " LUA_QS ": second argument must be a register, got %s.", "setbits", luaL_typename(L, 2));
  top = agn_reggettop(L, 2);
  if (top > INTBITS)
    luaL_error(L, "Error in " LUA_QS ": register too large, expected up to 32 elements, not %d.", "setbits", top);  /* 3.7.8 */
  for (i=top; i > 0; i--) {
    agn_regrawgeti(L, 2, i);
    if (lua_isboolean(L, -1))
      flag = lua_toboolean(L, -1);  /* true or false passed ? */
    else if (agn_isnumber(L, -1)) {
      flag = lua_tointeger(L, -1);  /* 1 or 0 passed ? */
      if (flag != 0 && flag != 1)
        luaL_error(L, "Error in " LUA_QS ": value must be either Boolean, or 0 or 1.", "setbits");
    } else {
      flag = 0;  /* to prevent compiler warnings */
      luaL_error(L, "Error in " LUA_QS ": value must be either Boolean, or 0 or 1.", "setbits");
    }
    x = (castx(x, issigned) & ~(1 << (top - (castx(i, issigned))))) | (flag << (top - (castx(i, issigned))));
    agn_poptop(L);
  }
  lua_pushinteger(L, x);
  return 1;
}


/* Sets `nbits` bits in 32-bit integer y into position `pos` of 32-bit integer `x`, and returns the modified value
   of `x`. `pos` and `nbits` should be in [1, 32]. If `pos` is not given`, it is 1 by default (the right-most bit in `x`).
   If `nbits` is not given, it is math.mostsigbit(`y`) by default.

   By default, the bits in `x` are overwritten by the bits in `y`. If the fifth argument "or" is given, the bits are
   Boolean-OR'ed. */
static int agnB_setnbits (lua_State *L) {  /* 2.12.4 */
  lu_byte issigned;
  lua_Number x, y;
  int pos, bits, i, flag, mode;
  issigned = agn_getbitwise(L);  /* 0 = unsigned, 1 = signed mode */
  x = agn_checknumber(L, 1);
  y = agn_checknumber(L, 2);
  pos = agnL_optposint(L, 3, 1);
  if (pos > INTBITS)
    luaL_error(L, "Error in " LUA_QS ": third argument must be positive and less than %d.", "setnbits", INTBITS);
  bits = agnL_optposint(L, 4, tools_msb32(castx(y, issigned)));
  mode = tools_strneq(agnL_optstring(L, 5, "or"), "or");  /* else Boolean-OR bits in x and y, 2.16.12 tweak */
  if (pos + bits - 1 > INTBITS)
    luaL_error(L, "Error in " LUA_QS ": fourth argument must be positive and less than %d.", "setnbits", pos + bits - 1);
  pos--;
  for (i=pos; i < pos + bits; i++) {
    flag = (castx(y, issigned) & (1 << (castx(i - pos, issigned)))) != 0;  /* get bit in y */
    x = (mode) ?  /* set this bit in x */
        (castx(x, issigned) & ~(1 << (castx(i, issigned)))) | (flag << (castx(i, issigned))) :  /* overwrite bits */
        (castx(x, issigned) |  (1 << (castx(i, issigned)))) | (flag << (castx(i, issigned)));   /* OR bits */
  }
  lua_pushnumber(L, x);
  return 1;
}


static int agnB_getbit (lua_State *L) {  /* 1.6.5 */
  lu_byte issigned;
  lua_Number x, pos;
  issigned = agn_getbitwise(L);
  if (lua_gettop(L) != 2)
    luaL_error(L, "Error in " LUA_QS ": need two arguments.", "getbit");
  x = agn_checkinteger(L, 1);  /* 2.13.0 */
  pos = agn_checkposint(L, 2);  /* 2.13.0 */
  if (pos > INTBITS)  /* 2.12.4 patch */
    luaL_error(L, "Error in " LUA_QS ": second argument must be less than %d.", "getbit", INTBITS);
  else
    pos = sun_trunc(pos);
  /*  x && (1 shift (pos-1)) <> 0 */
  lua_pushboolean(L, (castx(x, issigned) & (1 << (castx(pos, issigned) - 1))) != 0);
  return 1;
}


static int agnB_getbits (lua_State *L) {  /* 2.4.0 */
  lu_byte issigned;
  lua_Number x;
  int i, anyoption;
  issigned = agn_getbitwise(L);
  x = agn_checknumber(L, 1);
  anyoption = (lua_gettop(L) > 1);
  agn_createreg(L, INTBITS);
  /*  x && (1 shift (pos-1)) <> 0 */
  for (i=0; i < INTBITS; i++) {
    if (anyoption)
      lua_pushinteger(L, (castx(x, issigned) & (1 << (castx(i, issigned)))) != 0);
    else
      lua_pushboolean(L, (castx(x, issigned) & (1 << (castx(i, issigned)))) != 0);
    agn_regrawseti(L, -2, INTBITS - i);
  }
  return 1;
}


/* From the 32-bit integer `x`, starting from bit position `pos` from the right, retrieves `nbits` bits and returns a decimal
   value. `pos` should be in [1, 32]. */
static int agnB_getnbits (lua_State *L) {  /* 2.12.4 */
  lu_byte issigned;
  lua_Number x;
  int pos, bits, byte, bit;
  LUA_UINT32 mask, word;
  issigned = agn_getbitwise(L);  /* 0 = unsigned, 1 = signed */
  x = agn_checknumber(L, 1);
  pos = agn_checkposint(L, 2);
  if (pos > INTBITS)
    luaL_error(L, "Error in " LUA_QS ": second argument must be positive and less than %d.", "getnbits", INTBITS);
  bits = agn_checkposint(L, 3);
  if (pos + bits - 1 > INTBITS)
    luaL_error(L, "Error in " LUA_QS ": third argument must be positive and less than %d.", "getnbits", pos + bits - 1);
  pos--;
  /* taken from: https://cboard.cprogramming.com/c-programming/25575-bit-mux-demux.html */
  byte = pos / CHAR_BIT;
  bit  = pos % CHAR_BIT;
  mask = (1 << bits) - 1;
  if (issigned) {  /* signed ? */
    LUA_INT32 p, *q;
    p = (LUA_INT32)x;
    q = &p;
    word = *(q + byte) | (*(q + byte + 1) << CHAR_BIT);
    lua_pushnumber(L, (word >> bit) & mask);
  } else {
    LUA_UINT32 p, *q;
    p = (LUA_UINT32)x;
    q = &p;
    word = *(q + byte) | (*(q + byte + 1) << CHAR_BIT);
    lua_pushnumber(L, (word >> bit) & mask);
  }
  return 1;
}

/* returns the indices for a given value; December 28, 2007; extended July 06, 2008 */
static int agnB_whereis (lua_State *L) {  /* C implementation: Agena 1.8.2, 06.10.2012 */
  size_t i, ii, t;
  /* See: https://www.quora.com/How-can-I-save-a-function-to-a-variable-and-call-it-later-via-the-variable */
  typedef int ftype (lua_State *, int, int);
  ftype *fptr;
  int nargs = lua_gettop(L);
  ii = 0;
  t = lua_type(L, 1);
  luaL_typecheck(L, t == LUA_TTABLE || t == LUA_TSEQ || t == LUA_TREG || t == LUA_TFUNCTION, 1, "table, sequence, register or function expected", t);
  if (t == LUA_TFUNCTION) {  /* 2.30.1, based on luaB_select */
    size_t j;
    switch (lua_type(L, 2)) {
      case LUA_TTABLE: {
        /* very rough guess whether table is an array or dictionary */
        size_t n, c;
        n = lua_objlen(L, 2);
        c = 1;
        luaL_checkstack(L, 2, "not enough stack space");
        aux_createtable(L, 2, n, 2);
        /* start traversal */
        lua_pushnil(L);
        while (agn_fnext(L, 2, 1, 1)) {
          if (nargs > 2) {
            luaL_checkstack(L, nargs - 2, "not enough stack space");
            for (j=3; j <= nargs; j++)
              lua_pushvalue(L, j);
          }
          lua_call(L, nargs - 1, 1);  /* call function with nargs-1 argument(s) and one result */
          if (lua_istrue(L, -1)) {  /* 5.0.1 fix */
            agn_poptoptwo(L);  /* pop true and value */
            lua_pushvalue(L, -1);  /* push key and set it into table */
            lua_rawseti(L, -3, c);  /* table is at -3 */
            c++;
          } else {
            agn_poptoptwo(L);  /* delete result and value, leave key on stack */
          }
        }
        agn_tabresize(L, -1, c, 1);
        break;
      }  /* end of case LUA_TTABLE */
      case LUA_TSEQ: {
        int i, j, c, nops;
        nops = agn_seqsize(L, 2);
        luaL_checkstack(L, 1, "not enough stack space");
        agn_createseq(L, nops/2);
        luaL_setmetatype(L, 2);
        /* now traverse sequence */
        c = 0;
        for (i=0; i < nops; i++) {
          luaL_checkstack(L, 2 + (nargs > 2)*(nargs - 2), "not enough stack space");
          lua_pushvalue(L, 1);  /* push function */
          lua_seqrawgeti(L, 2, i + 1);
          for (j=3; j <= nargs; j++)
            lua_pushvalue(L, j);
          lua_call(L, nargs - 1, 1);  /* call function with nargs-1 argument(s) and one result */
          if (lua_istrue(L, -1)) {  /* 5.0.1 fix */
            agn_seqsetinumber(L, -2, ++c, i + 1);  /* store value to sequence, new sequence is at -3 */
          }
          agn_poptop(L);
        }
        agn_seqresize(L, -1, c);
        break;
      }  /* end of case LUA_TSEQ */
      case LUA_TREG: {
        int i, nops, c;
        nops = agn_reggettop(L, 2);
        luaL_checkstack(L, 1, "not enough stack space");
        agn_createreg(L, nops);
        luaL_setmetatype(L, 2);
        /* now traverse register */
        c = 0;
        for (i=0; i < nops; i++) {
          luaL_checkstack(L, 2 + (nargs > 2)*(nargs - 2), "not enough stack space");
          lua_pushvalue(L, 1);  /* push function */
          agn_regrawgeti(L, 2, i + 1);
          for (j=3; j <= nargs; j++)
            lua_pushvalue(L, j);
          lua_call(L, nargs - 1, 1);  /* call function with nargs-1 argument(s) and one result */
          if (lua_istrue(L, -1)) {  /* 5.0.1 fix */
            agn_regsetinumber(L, -2, ++c, i + 1);  /* store value to new register, register is at -3 or 2 */
          }
          agn_poptop(L);
        }
        agn_regresize(L, -1, c, 0);
        break;
      }  /* end of case LUA_TREG */
      default:
        luaL_error(L, "Error in " LUA_QS ": table, sequence or register expected, got %s.", "whereis", luaL_typename(L, 2));
    }
  } else {
    int nops, rc, r, number, string, type, approx;
    lua_Number eps = agn_getepsilon(L);
    approx = nargs == 3;
    fptr = (approx) ? lua_rawaequal : lua_equal;  /* 4.2.0 extension */
    type = lua_type(L, 1);
    number = agn_isnumber(L, 2);
    string = agn_isstring(L, 2);
    if (number && type == LUA_TTABLE && !agn_hashashpart(L, 1)) {  /* 4.11.1 boost */
      lua_Number x, y;
      x = agn_tonumber(L, 2);
      nops = luaL_getn(L, 1);
      lua_createtable(L, agn_log2(L, nops), 0);
      for (i=1; i <= nops; i++) {
        y = agn_rawgetinumber(L, 1, i, &rc);
        if (rc) {
          r = (approx) ? tools_approx(x, y, eps) : x == y;
          if (r) {
            lua_rawsetinumber(L, -1, ++ii, i);
          }
        }
      }
      agn_tabresize(L, -1, ii, 1);
    } else if (number && type == LUA_TSEQ) {
      lua_Number x, y, eps;
      x = agn_tonumber(L, 2);
      nops = agn_seqsize(L, 1);
      eps = agn_getepsilon(L);
      agn_createseq(L, agn_log2(L, nops));
      for (i=1; i <= nops; i++) {
        y = agn_seqrawgetinumber(L, 1, i, &rc);
        if (rc) {
          r = (approx) ? tools_approx(x, y, eps) : x == y;
          if (r) {
            agn_seqsetinumber(L, -1, ++ii, i);
          }
        }
      }
      agn_seqresize(L, -1, ii);
    } else if (number && type == LUA_TREG) {
      lua_Number x, y, eps;
      x = agn_tonumber(L, 2);
      nops = agn_regsize(L, 1);
      eps = agn_getepsilon(L);
      agn_createreg(L, nops);
      for (i=1; i <= nops; i++) {
        y = agn_regrawgetinumber(L, 1, i, &rc);
        if (rc) {
          r = (approx) ? tools_approx(x, y, eps) : x == y;
          if (r) {
            agn_regsetinumber(L, -1, ++ii, i);
          }
        }
      }
      agn_regresize(L, -1, ii, 0);
    } else if (string && type == LUA_TTABLE && !agn_hashashpart(L, 1)) {  /* 4.11.1 boost */
      size_t l;
      const char *x, *y;
      x = agn_tostring(L, 2);
      nops = luaL_getn(L, 1);
      lua_createtable(L, agn_log2(L, nops), 0);
      for (i=1; i <= nops; i++) {
        y = agn_rawgetilstring(L, 1, i, &l, &rc);
        if (rc) {
          if (tools_streq(x, y)) {
            lua_rawsetinumber(L, -1, ++ii, i);
          }
        }
      }
      agn_tabresize(L, -1, ii, 1);
    } else if (string && type == LUA_TSEQ) {
      size_t l;
      const char *x, *y;
      x = agn_tostring(L, 2);
      nops = agn_seqsize(L, 1);
      agn_createseq(L, agn_log2(L, nops));
      for (i=1; i <= nops; i++) {
        y = agn_seqrawgetilstring(L, 1, i, &l, &rc);
        if (rc) {
          if (tools_streq(x, y)) {
            agn_seqsetinumber(L, -1, ++ii, i);
          }
        }
      }
      agn_seqresize(L, -1, ii);
    } else if (string && type == LUA_TREG) {
      size_t l;
      const char *x, *y;
      x = agn_tostring(L, 2);
      nops = agn_regsize(L, 1);
      agn_createreg(L, nops);
      for (i=1; i <= nops; i++) {
        y = agn_regrawgetilstring(L, 1, i, &l, &rc);
        if (rc) {
          if (tools_streq(x, y)) {
            agn_regsetinumber(L, -1, ++ii, i);
          }
        }
      }
      agn_regresize(L, -1, ii, 0);
    } else {
      switch (t) {
        case LUA_TTABLE: {
          lua_createtable(L, agn_log2(L, agn_size(L, 1)), 0);  /* 2.30.1 change: use less memory */
          lua_pushnil(L);
          while (lua_next(L, 1)) {
            if (fptr(L, -1, 2)) {
              agn_poptop(L);
              lua_pushvalue(L, -1);  /* push key */
              lua_rawseti(L, -3, ++ii);  /* assign it to table and drop it */
            } else
              agn_poptop(L);
          }
          agn_tabresize(L, -1, ii, 1);  /* 2.30.1 */
          break;
        }
        case LUA_TSEQ: {
          size_t nops;
          nops = agn_seqsize(L, 1);
          agn_createseq(L, agn_log2(L, nops));  /* 2.30.1 change: use less memory */
          for (i=1; i <= nops; i++) {
            lua_seqrawgeti(L, 1, i);
            if (fptr(L, -1, 2)) {
              agn_seqsetinumber(L, -2, ++ii, i);
            }
            agn_poptop(L);
          }
          agn_seqresize(L, -1, ii);  /* 2.30.1 */
          break;
        }
        case LUA_TREG: {  /* 2.3.0 RC 3 */
          size_t nops;
          nops = agn_reggettop(L, 1);
          agn_createreg(L, nops);
          for (i=1; i <= nops; i++) {
            agn_regrawgeti(L, 1, i);
            if (fptr(L, -1, 2)) {
              lua_pushnumber(L, i);
              agn_regrawseti(L, -3, ++ii);
            }
            agn_poptop(L);
          }
          agn_regresize(L, -1, ii, 0);
          break;
        }
        default: lua_assert(0);  /* should not happen */
      }
    }
  }
  return 1;
}


static int agnB_member (lua_State *L) {  /* emulates Maple's `member`, based on `whereis, 3.7.1, changed 3.8.0 */
  int nargs;
  size_t i, type;
  /* See: https://www.quora.com/How-can-I-save-a-function-to-a-variable-and-call-it-later-via-the-variable */
  typedef int ftype (lua_State *, int, int);
  ftype *fptr;
  luaL_checkany(L, 1);
  type = lua_type(L, 2);
  nargs = lua_gettop(L);
  luaL_typecheck(L, type == LUA_TTABLE || type == LUA_TSET || type == LUA_TSEQ || type == LUA_TREG || type == LUA_TSTRING, 2, "table, set, sequence, register or string expected", type);
  if (nargs == 3 && agn_isnumber(L, 3)) {  /* 7.0.1 extension */
    lua_Number x, y, eps = agn_tonumber(L, 3);
    if (eps <= 0) eps = agn_getepsilon(L);
    x = agn_checknumber(L, 1);
    switch (type) {
      case LUA_TTABLE: {
        lua_pushnil(L);
        while (lua_next(L, 2) != 0) {
          y = agn_checknumber(L, -1);
          if (tools_approx(x, y, eps)) {
            agn_poptop(L);  /* pop value */
            return 1;       /* return key */
          } else
            agn_poptop(L);
        }
        break;
      }
      case LUA_TSET: {
        lua_pushnil(L);
        while (lua_usnext(L, 2) != 0) {
          y = agn_checknumber(L, -1);
          if (tools_approx(x, y, eps)) {
            agn_poptoptwo(L);  /* pop value, value */
            lua_pushtrue(L);
            return 1;
          } else
            agn_poptop(L);
        }
        break;
      }
      case LUA_TSEQ: {
        size_t nops;
        nops = agn_seqsize(L, 2);
        for (i=1; i <= nops; i++) {
          y = lua_seqrawgetinumber(L, 2, i);
          if (tools_approx(x, y, eps)) {
            lua_pushinteger(L, i);  /* push key */
            return 1;  /* return index */
          }
        }
        break;
      }
      case LUA_TREG: {
        size_t nops;
        nops = agn_reggettop(L, 2);
        for (i=1; i <= nops; i++) {
          y = agn_reggetinumber(L, 2, i);
          if (tools_approx(x, y, eps)) {
            lua_pushinteger(L, i);  /* push key */
            return 1;  /* return index */
          }
        }
        break;
      }
      default:
        luaL_error(L, "Error in " LUA_QS ": table, set, sequence or register expected, got %s.", "member", luaL_typename(L, 2));
    }
  } else {
    fptr = (nargs == 3) ? lua_rawaequal : lua_equal;  /* 4.2.0 extension */
    switch (type) {
      case LUA_TTABLE: {
        lua_pushnil(L);
        while (lua_next(L, 2) != 0) {
          if (fptr(L, -1, 1)) {
            agn_poptop(L);  /* pop value */
            return 1;       /* return key, 3.8.0 change */
          } else
            agn_poptop(L);
        }
        break;
      }
      case LUA_TSET: {  /* 3.13.3 */
        lua_pushnil(L);
        while (lua_usnext(L, 2) != 0) {
          if (fptr(L, -1, 1)) {
            agn_poptoptwo(L);  /* pop value, value */
            lua_pushtrue(L);
            return 1;
          } else
            agn_poptop(L);
        }
        break;
      }
      case LUA_TSEQ: {
        size_t nops;
        nops = agn_seqsize(L, 2);
        for (i=1; i <= nops; i++) {
          lua_seqrawgeti(L, 2, i);
          if (fptr(L, -1, 1)) {
            agn_poptop(L);
            lua_pushinteger(L, i);  /* push key */
            return 1;  /* return index, 3.8.0 change */
          }
          agn_poptop(L);
        }
        break;
      }
      case LUA_TREG: {  /* patched 7.0.1 */
        size_t nops;
        nops = agn_reggettop(L, 2);
        for (i=1; i <= nops; i++) {
          agn_regrawgeti(L, 2, i);
          if (fptr(L, -1, 1)) {
            agn_poptop(L);
            lua_pushinteger(L, i);  /* push key */
            return 1;  /* return index, 3.8.0 change */
          }
          agn_poptop(L);
        }
        break;
      }
      case LUA_TSTRING: {  /* 3.17.1, taken from str_contains */
        const char *s, *p;
        size_t sl, sp;
        s = agn_checklstring(L, 1, &sl);
        p = lua_tolstring(L, 2, &sp);
        if (sp == 0) break;
        if (tools_strinalphabet(s, p, sl, sp)) {
          lua_pushtrue(L);
          return 1;
        }
        break;
      }
      default: lua_assert(0);  /* should not happen */
    }
  }
  lua_pushnil(L);  /* return `null`, 3.8.0 change */
  return 1;
}


static int agnB_checkoptions (lua_State *L) {  /* Agena 1.8.6, 1.8.12 */
  size_t i, nargs;
  int vtype, ctype, throwunknown;
  const char *procname, *etypename, *ctypename, *option;
  throwunknown = 1;  /* 1.8.12 */
  nargs = lua_gettop(L);
  luaL_checkstack(L, nargs, "too many arguments");  /* 2.11.4 */
  procname = agn_checkstring(L, 1);
  if (nargs < 3)
    luaL_error(L, "Error in " LUA_QS ": at least three arguments expected.", procname);
  vtype = lua_type(L, 2);
  luaL_argcheck(L, vtype == LUA_TTABLE, 2, "table of pairs expected");
  lua_createtable(L, 0, 0);
  lua_createtable(L, 0, nargs - 2);  /* temporary table */
  for (i=3; i <= nargs; i++) {
    if (i == nargs && lua_isboolean(L, nargs)) {  /* 1.8.12 */
      if (agn_istrue(L, nargs))  /* 1.8.12 */
        throwunknown = 0;
    }
    else if (!lua_ispair(L, i))
      luaL_error(L, "Error in " LUA_QS ": pair expected, got %s.", procname, luaL_typename(L, i));
    else { /* value is a pair */
      /* check for properly allocated slots will be done by agn_pairgetiall() */
      agn_pairgetiall(L, i);  /* 7.7.8 */
      if (lua_type(L, -1) != LUA_TSTRING)
        luaL_error(L, "Error in " LUA_QS ": string expected for right operand, got %s.", procname, luaL_typename(L, -1));
      else
        lua_settable(L, -3);  /* enter expected type into temporary table */
    }
  }
  lua_pushnil(L);
  while (lua_next(L, 2)) {
    if (lua_ispair(L, -1)) {
      agn_pairgeti(L, -1, 1);  /* push option name */
      if (!agn_isstring(L, -1))
        luaL_error(L, "Error in " LUA_QS ": left-hand side of option must be a string, got %s.", procname, luaL_typename(L, -1));
      agn_pairgeti(L, -2, 2);  /* push value */
      lua_pushvalue(L, -2);    /* push option name again */
      lua_gettable(L, -6);     /* get expected type from temporary table */
      if (lua_isnil(L, -1)) {  /* 1.8.12 */
        if (throwunknown)
          luaL_error(L, "Error in " LUA_QS ": unknown option %s.", procname, agn_checkstring(L, -3));
        else {
          agn_poptop(L);  /* remove null */
          lua_settable(L, -6);  /* pops option name and value, put `unknown` option:value into result table */
          agn_poptop(L);  /* pop pair, leave key on stack */
          continue;
        }
      }
      ctype = lua_type(L, -2);             /* current type to be checked */
      ctypename = lua_typename(L, ctype);  /* the type as a string */
      etypename = agn_checkstring(L, -1);  /* expected type */
      option = agn_checkstring(L, -3);     /* left-hand of the option */
      if (tools_strneq(etypename, ctypename)) {  /* possible type mismatch ?  2.16.12 tweak */
        if (tools_streq(etypename, "anything")) {
          /* do nothing */
        } else if (tools_streq(etypename, "basic")) {  /* 3.3.6 */
          if (!(ctype == LUA_TNUMBER || ctype == LUA_TSTRING || ctype == LUA_TNIL || ctype == LUA_TBOOLEAN || ctype == LUA_TFAIL))
            luaL_error(L, "Error in " LUA_QS ": %s expected for %s option, got %s.", procname, etypename, option, ctypename);
        } else if (tools_streq(etypename, "listing")) {  /* 3.3.6 */
          if (!(ctype == LUA_TTABLE || ctype == LUA_TSEQ || ctype == LUA_TREG))
            luaL_error(L, "Error in " LUA_QS ": %s expected for %s option, got %s.", procname, etypename, option, ctypename);
        } else if (ctype > LUA_TTHREAD || ctype == LUA_TFUNCTION) {  /* check for user-defined type */
          if (agn_getutype(L, -2) == 1) {  /* a user-defined type has been set ? */
            if (tools_strneq(agn_tostring(L, -1), etypename)) {  /* utype mismatch ?  2.16.12 tweak */
              luaL_error(L, "Error in " LUA_QS ": %s expected for %s option, got %s.", procname, etypename, agn_checkstring(L, -4), ctypename);
            }
            agn_poptop(L);  /* drop utype */
          }
          else
            luaL_error(L, "Error in " LUA_QS ": %s expected for %s option, got %s.", procname, etypename, option, ctypename);
        } else if (ctype == LUA_TNUMBER) {  /* check numeric pseudo-types, 3.3.6 extension */
          lua_Number x = agn_tonumber(L, -2);
          if (tools_streq(etypename, "positive")) {
            if (x <= 0)
              luaL_error(L, "Error in " LUA_QS ": expected a positive number for " LUA_QS " option.", procname, option);
          } else if (tools_streq(etypename, "negative")) {
            if (x >= 0)
              luaL_error(L, "Error in " LUA_QS ": expected a negative number for " LUA_QS " option.", procname, option);
          } else if (tools_streq(etypename, "nonnegative")) {
            if (x < 0)
              luaL_error(L, "Error in " LUA_QS ": expected a non-negative number for " LUA_QS " option.", procname, option);
          } else if (tools_streq(etypename, "integer")) {
            if (!tools_isint(x))
              luaL_error(L, "Error in " LUA_QS ": expected an integer for " LUA_QS " option.", procname, option);
          } else if (tools_streq(etypename, "posint")) {
            if (!(tools_isint(x) && x > 0))  /* 3.12.0 fix */
              luaL_error(L, "Error in " LUA_QS ": expected a positive integer for " LUA_QS " option.", procname, option);
          } else if (tools_streq(etypename, "negint")) {
            if (!(tools_isint(x) && x < 0))  /* 3.12.0 fix */
              luaL_error(L, "Error in " LUA_QS ": expected a negative integer for " LUA_QS " option.", procname, option);
          } else if (tools_streq(etypename, "nonnegint")) {
            if (!(tools_isint(x) && x >= 0))  /* 3.12.0 fix */
              luaL_error(L, "Error in " LUA_QS ": expected a non-negative integer for " LUA_QS " option.", procname, option);
          } else if (tools_streq(etypename, "nonzeroint")) {  /* 4.11.0 */
            if (!(tools_isint(x) && x != 0))
              luaL_error(L, "Error in " LUA_QS ": expected a non-zero integer for " LUA_QS " option.", procname, option);
          }
        } else
          luaL_error(L, "Error in " LUA_QS ": %s expected for %s option, got %s.", procname, etypename, option, ctypename);
      }
      agn_poptop(L);  /* remove expected type */
      lua_settable(L, -6);  /* set option and actual value to result table */
    }
    agn_poptop(L);  /* pop pair or other value */
  }
  agn_poptop(L);  /* pop temp table */
  return 1;
}


static int agnB_alternate (lua_State *L) {
  if (lua_gettop(L) != 2)
    luaL_error(L, "Error in " LUA_QS ": expected two arguments.", "alternate");
  lua_pushvalue(L, 2 - lua_isnil(L, 2));
  return 1;
}


static void agnB_opterrmessage (lua_State *L, int arg, const char *procname, const char *what) {
  if (arg == 0)
    luaL_error(L, "Error in " LUA_QS ": expected %s, got %s.",
      (procname == NULL) ? "?" : procname, what, luaL_typename(L, 1));
  else
    luaL_error(L, "Error in " LUA_QS ": expected %s for argument #%d, got %s.",
      (procname == NULL) ? "?" : procname, what, arg, luaL_typename(L, 1));
}


static void agnB_opterrmessagenumber (lua_State *L, int arg, const char *procname, const char *what, lua_Number x) {  /* 2.14.8 */
  if (arg == 0)
    luaL_error(L, "Error in " LUA_QS ": expected %s, got %f.",
      (procname == NULL) ? "?" : procname, what, x);
  else
    luaL_error(L, "Error in " LUA_QS ": expected %s for argument #%d, got %f.",
      (procname == NULL) ? "?" : procname, what, arg, x);
}

/* The function checks whether x is a number and in this case returns x. If x is `null` it returns the number y.
   If the third argument `procname` is given, this name is printed as the function issuing the error. */
static int agnB_optnumber (lua_State *L) {  /* 2.4.1 */
  int arg = luaL_optinteger(L, 3, 0);
  const char *procname = luaL_optstring(L, 4, NULL);
  if (!agn_isnumber(L, 2))
    luaL_error(L, "Error in " LUA_QS ": default value is not a number, got %s.",
      (procname == NULL) ? "?" : procname, luaL_typename(L, 2));  /* 2.14.8 fix */
  if (lua_isnoneornil(L, 1)) {
    lua_pushvalue(L, 2);
    return 1;
  } else if (!agn_isnumber(L, 1))
    agnB_opterrmessage(L, arg, procname, "a number");
  lua_pushvalue(L, 1);
  return 1;
}


/* grep "(GREP_POINT) types;" if you want to add new types */
static int agnB_optpositive (lua_State *L) {  /* 2.4.1, improved 2.14.8 */
  lua_Number x, y;
  int arg = luaL_optinteger(L, 3, 0);
  const char *procname = luaL_optstring(L, 4, NULL);
  /* check default value */
  if (!agn_isnumber(L, 2))
    luaL_error(L, "Error in " LUA_QS ": default value is not a positive number, got %s.",
      (procname == NULL) ? "?" : procname, luaL_typename(L, 2));  /* 2.14.8 fix */
  y = lua_tonumber(L, 2);
  if (y <= 0)
    luaL_error(L, "Error in " LUA_QS ": default value is not a positive number, got %f.",
      (procname == NULL) ? "?" : procname, y);
  /* check first argument */
  if (lua_isnoneornil(L, 1)) {
    lua_pushvalue(L, 2);
    return 1;
  } else if (!agn_isnumber(L, 1))
    agnB_opterrmessage(L, arg, procname, "a positive number");
  x = lua_tonumber(L, 1);
  if (x <= 0)
    agnB_opterrmessagenumber(L, arg, procname, "a positive number", x);
  lua_pushvalue(L, 1);
  return 1;
}


static int agnB_optnonnegative (lua_State *L) {  /* 2.4.1, improved 2.14.8 */
  lua_Number x, y;
  int arg = luaL_optinteger(L, 3, 0);
  const char *procname = luaL_optstring(L, 4, NULL);
  /* check default value */
  if (!agn_isnumber(L, 2))
    luaL_error(L, "Error in " LUA_QS ": default value is not a non-negative number, got %s.",
      (procname == NULL) ? "?" : procname, luaL_typename(L, 2));  /* 2.14.8 fix */
  y = lua_tonumber(L, 2);
  if (y < 0)
    luaL_error(L, "Error in " LUA_QS ": default value is not a non-negative number, got %f.",
      (procname == NULL) ? "?" : procname, y);
  /* check first argument */
  if (lua_isnoneornil(L, 1)) {
    lua_pushvalue(L, 2);
    return 1;
  } else if (!agn_isnumber(L, 1))
    agnB_opterrmessage(L, arg, procname, "a non-negative number");
  x = lua_tonumber(L, 1);
  if (x < 0)
    agnB_opterrmessagenumber(L, arg, procname, "a non-negative number", x);
  lua_pushvalue(L, 1);
  return 1;
}


static int agnB_optint (lua_State *L) {  /* 2.4.1, improved 2.14.8 */
  lua_Number x, y;
  int arg;
  arg = luaL_optinteger(L, 3, 0);
  const char *procname = luaL_optstring(L, 4, NULL);
  /* check default value */
  if (!agn_isnumber(L, 2))
    luaL_error(L, "Error in " LUA_QS ": default value is not an integer, got %s.",
      (procname == NULL) ? "?" : procname, luaL_typename(L, 2));  /* 2.14.8 fix */
  y = lua_tonumber(L, 2);
  if (tools_isfrac(y))
    luaL_error(L, "Error in " LUA_QS ": default value is not an integer, got %f.",
      (procname == NULL) ? "?" : procname, y);  /* 2.14.8 */
  /* check first argument */
  if (lua_isnoneornil(L, 1)) {
    lua_pushvalue(L, 2);
    return 1;
  } else if (!agn_isnumber(L, 1))
    agnB_opterrmessage(L, arg, procname, "an integer");
  x = lua_tonumber(L, 1);
  if (tools_isfrac(x))
    agnB_opterrmessagenumber(L, arg, procname, "an integer", x);
  lua_pushvalue(L, 1);
  return 1;
}


static int agnB_optposint (lua_State *L) {  /* 2.4.1, improved 2.14.8 */
  lua_Number x, y;
  int arg;
  arg = luaL_optinteger(L, 3, 0);
  const char *procname = luaL_optstring(L, 4, NULL);
  /* check default value */
  if (!agn_isnumber(L, 2))
    luaL_error(L, "Error in " LUA_QS ": default value is not a positive integer, got %s.",
      (procname == NULL) ? "?" : procname, luaL_typename(L, 2));  /* 2.14.8 fix */
  y = lua_tonumber(L, 2);
  if (!tools_isposint(y))  /* 2.14.13 change */
    luaL_error(L, "Error in " LUA_QS ": default value is not a positive integer, got %f.",
      (procname == NULL) ? "?" : procname, y);  /* 2.14.8 */
  /* check first argument */
  if (lua_isnoneornil(L, 1)) {
    lua_pushvalue(L, 2);
    return 1;
  } else if (!agn_isnumber(L, 1))
    agnB_opterrmessage(L, arg, procname, "a positive integer");
  x = lua_tonumber(L, 1);
  if (!tools_isposint(x))  /* 2.14.13 change */
    agnB_opterrmessagenumber(L, arg, procname, "a positive integer", x);  /* 2.14.8 */
  lua_pushvalue(L, 1);
  return 1;
}


static int agnB_optnonnegint (lua_State *L) {  /* 2.4.1, improved 2.14.8 */
  lua_Number x, y;
  int arg;
  arg = luaL_optinteger(L, 3, 0);
  const char *procname = luaL_optstring(L, 4, NULL);
  /* check default value */
  if (!agn_isnumber(L, 2))
    luaL_error(L, "Error in " LUA_QS ": default value is not a non-negative integer, got %s.",
      (procname == NULL) ? "?" : procname, luaL_typename(L, 2));  /* 2.14.8 fix */
  y = lua_tonumber(L, 2);
  if (!tools_isnonnegint(y))  /* 2.14.13 change */
    luaL_error(L, "Error in " LUA_QS ": default value is not a non-negative integer, got %f.",
      (procname == NULL) ? "?" : procname, y);  /* 2.14.8 */
  /* check first argument */
  if (lua_isnoneornil(L, 1)) {
    lua_pushvalue(L, 2);
    return 1;
  } else if (!agn_isnumber(L, 1))
    agnB_opterrmessage(L, arg, procname, "a non-negative integer");
  x = lua_tonumber(L, 1);
  if (!tools_isnonnegint(x))  /* 2.14.13 change */
    agnB_opterrmessagenumber(L, arg, procname, "a non-negative integer", x);  /* 2.14.8 */
  lua_pushvalue(L, 1);
  return 1;
}


static int agnB_optnonzeroint (lua_State *L) {  /* 4.11.0 */
  lua_Number x, y;
  int arg;
  arg = luaL_optinteger(L, 3, 0);
  const char *procname = luaL_optstring(L, 4, NULL);
  /* check default value */
  if (!agn_isnumber(L, 2))
    luaL_error(L, "Error in " LUA_QS ": default value is not a non-zero integer, got %s.",
      (procname == NULL) ? "?" : procname, luaL_typename(L, 2));
  y = lua_tonumber(L, 2);
  if (!tools_isnonzeroint(y))
    luaL_error(L, "Error in " LUA_QS ": default value is not a non-zero integer, got %f.",
      (procname == NULL) ? "?" : procname, y);
  /* check first argument */
  if (lua_isnoneornil(L, 1)) {
    lua_pushvalue(L, 2);
    return 1;
  } else if (!agn_isnumber(L, 1))
    agnB_opterrmessage(L, arg, procname, "a non-zero integer");
  x = lua_tonumber(L, 1);
  if (!tools_isnonzeroint(x))
    agnB_opterrmessagenumber(L, arg, procname, "a non-zero integer", x);
  lua_pushvalue(L, 1);
  return 1;
}


static int agnB_optcomplex (lua_State *L) {  /* 2.4.1 */
  int arg = luaL_optinteger(L, 3, 0);
  const char *procname = luaL_optstring(L, 4, NULL);
  if (!(lua_iscomplex(L, 2) || agn_isnumber(L, 2)))
    luaL_error(L, "Error in " LUA_QS ": default value is not a (complex) number, got %s.",
      (procname == NULL) ? "?" : procname, luaL_typename(L, 2));  /* 2.14.8 fix */
  if (lua_isnoneornil(L, 1)) {
    lua_pushvalue(L, 2);
    return 1;
  }
  else if (!(agn_isnumber(L, 1) || lua_iscomplex(L, 1)))
    agnB_opterrmessage(L, arg, procname, "a (complex) number");
  lua_pushvalue(L, 1);
  return 1;
}


static int agnB_optstring (lua_State *L) {  /* 2.4.1 */
  int arg = luaL_optinteger(L, 3, 0);
  const char *procname = luaL_optstring(L, 4, NULL);
  if (!agn_isstring(L, 2))
    luaL_error(L, "Error in " LUA_QS ": default value is not a string, got %s.",
      (procname == NULL) ? "?" : procname, luaL_typename(L, 2));  /* 2.14.8 fix */
  if (lua_isnoneornil(L, 1)) {
    lua_pushvalue(L, 2);
    return 1;
  }
  else if (!agn_isstring(L, 1))
    agnB_opterrmessage(L, arg, procname, "a string");
  lua_pushvalue(L, 1);
  return 1;
}


static int agnB_optboolean (lua_State *L) {  /* 2.4.1 */
  int arg = luaL_optinteger(L, 3, 0);
  const char *procname = luaL_optstring(L, 4, NULL);
  if (!lua_isboolean(L, 2))
    luaL_error(L, "Error in " LUA_QS ": default value is not a Boolean, got %s.",
      (procname == NULL) ? "?" : procname, luaL_typename(L, 2));  /* 2.14.8 fix */
  if (lua_isnoneornil(L, 1)) {
    lua_pushvalue(L, 2);
    return 1;
  }
  else if (!lua_isboolean(L, 1))
    agnB_opterrmessage(L, arg, procname, "a Boolean");
  lua_pushvalue(L, 1);
  return 1;
}


/* Reverses the order of all elements in a sequence or register in-place. The function returns nothing */

static int agnB_reverse (lua_State *L) {  /* 2.7.0 */
  size_t s, i;
  if (lua_istable(L, 1)) {  /* 2.22.2 */
    s = agn_asize(L, 1);
    lua_pushvalue(L, 1);
    /* reverse order of sequence */
    for (i=1; i <= s / 2; i++) {
      lua_geti(L, 1, i);
      lua_geti(L, 1, s - i + 1);
      /* swap */
      lua_seti(L, -3, i);  /* and pop value */
      lua_seti(L, -2, s - i + 1);  /* and pop value */
    }
  } else if (lua_isseq(L, 1)) {
    s = agn_seqsize(L, 1);
    lua_pushvalue(L, 1);
    /* reverse order of sequence */
    for (i=1; i <= s / 2; i++) {
      lua_seqrawgeti(L, 1, i);
      lua_seqrawgeti(L, 1, s - i + 1);
      /* swap */
      lua_seqrawseti(L, -3, i);  /* and pop value */
      lua_seqrawseti(L, -2, s - i + 1);  /* and pop value */
    }
  } else if (lua_isreg(L, 1)) {
    s = agn_regsize(L, 1);
    lua_pushvalue(L, 1);
    /* reverse order of sequence */
    for (i=1; i <= s / 2; i++) {
      agn_regrawgeti(L, 1, i);
      agn_regrawgeti(L, 1, s - i + 1);
      /* swap */
      agn_regrawseti(L, -3, i);  /* and pop value */
      agn_regrawseti(L, -2, s - i + 1);  /* and pop value */
    }
  } else
    luaL_error(L, "Error in " LUA_QS ": table, sequence or register expected, got %s.",
      "reverse", luaL_typename(L, 1));
  return 1;  /* return modified structure which still is at the top of the stack, 2.31.2 */
}


/* Returns every given k-th element in the table, sequence, or register obj
   in a new structure. The type of return is determined by the type of
   the first argument. With tables, only the array part is traversed. */
static int agnB_everyth (lua_State *L) {  /* 2.8.3 */
  int i, k, n, c;
  k = agn_checkposint(L, 2);
  c = 1;
  if (lua_isnumber(L, 1)) {  /* 2.20.0 */
    lua_pushboolean(L, luai_nummod(agn_tonumber(L, 1), k) == 0);
  } else if (lua_istable(L, 1)) {
    n = agn_asize(L, 1);  /* number of elements in array part */
    if (k > n)
      luaL_error(L, "Error in " LUA_QS ": second argument larger than table size %d.", "everyth", n);
    lua_createtable(L, n/k, 0);
    for (i=k; i <= n; i += k) {
      lua_rawgeti(L, 1, i);
      lua_rawseti(L, -2, c++);
    }
  } else if (lua_isseq(L, 1)) {
    n = agn_seqsize(L, 1);
    if (k > n)
      luaL_error(L, "Error in " LUA_QS ": second argument larger than sequence size %d.", "everyth", n);
    agn_createseq(L, n/k);
    for (i=k; i <= n; i += k) {
      lua_seqrawgeti(L, 1, i);
      lua_seqrawseti(L, -2, c++);
    }
  } else if (lua_isreg(L, 1)) {
    n = agn_reggettop(L, 1);
    if (k > n)
      luaL_error(L, "Error in " LUA_QS ": second argument larger than register size %d.", "everyth", n);
    agn_createreg(L, n/k);
    for (i=k; i <= n; i += k) {
      agn_regrawgeti(L, 1, i);
      agn_regrawseti(L, -2, c++);
    }
  } else
    luaL_error(L, "Error in " LUA_QS ": number, table, sequence or register expected, got %s.",
      "everyth", luaL_typename(L, 1));
  return 1;
}

#include "lstring.h"
/* Pops n numbers from the built-in stack, If any optional argument is given, it does not return the last
   value popped from the stack. If n is not given, only one value is removed. */
static int agnB_popd (lua_State *L) {  /* 2.9.4, reintroduced 2.39.5 */
  int n, nooption;
  n = agnL_optinteger(L, 1, 1);
  nooption = lua_gettop(L) < 2;
  if (L->currentstack == 0)  /* 2.14.2 change */
      luaL_error(L, "Error in " LUA_QS ": cannot operate on stack 0.", "popd");
  if (n == 0) {  /* only return something if no option has been given */
    if (nooption) lua_pushnil(L);
  } else {
    if (stacktop(L) == 0) {  /* 2.12.7 change */
      lua_pushnil(L);
      return 1;
    }
    if (n < 0 || (stacktop(L) - n + 1 <= LUA_CELLSTACKBOTTOM))
      luaL_error(L, "Error in " LUA_QS ": invalid number of values to be removed.", "popd");
    stacktop(L) -= n;
    if (nooption) {
      if (isnumstack(L)) {
        lua_pushnumber(L, cells(L, stacktop(L)));
      } else if (iscachestack(L)) {  /* 2.36.3 */
        lua_pushcachevalue(L, stacktop(L));  /* 7.1.6 fix for strings */
      } else {
        lua_pushlstring(L, getcharcell(L, stacktop(L)), 1);
      }
    }
    if (iscachestack(L)) {  /* 2.37.1 change & fix */
      StkId i;
      for (i=L->C->top - n; i < L->C->top; i++) {
        setobj2s(L->C, i, luaO_nilobject);
      }
      L->C->top -= n;
    }
  }
  return nooption;
}


/* If the stack contains elements, the function returns their number, else it returns false. It can be easily
   used in `while` loops to traverse a stack. See also: sized. */
static int agnB_allotted (lua_State *L) {  /* 2.9.4 */
  size_t n;
  int stacknr, oldstack;
  oldstack = L->currentstack;
  stacknr = agnL_optnonnegint(L, 1, oldstack);  /* 2.12.7 patch, changed 2.14.2 */
  if (stacknr >= LUA_NSTACKS)  /* 2.37.3 fix */
    luaL_error(L, "Error in " LUA_QS ": invalid stack number %d.", "allotted", stacknr);
  if (stacknr != oldstack) {
    L->currentstack = stacknr;
    n = stacktop(L);
    L->currentstack = oldstack;
  } else
    n = stacktop(L);
  if (n > LUA_CELLSTACKBOTTOM)
    lua_pushinteger(L, n);
  else
    lua_pushnil(L);  /* 2.12.7 change */
  return 1;
}


/* The function just returns all its arguments. 2.9.8 */
static int agnB_identity (lua_State *L) {
  int nargs = lua_gettop(L);
  luaL_checkstack(L, nargs, "too many arguments");
  return nargs;
}


/* See: https://stackoverflow.com/questions/16132002/c-implementation-xor-implies-iff

P Q|implies|xor
-----------|---
T T|   T   | F
T F|   F   | T
F T|   T   | T
F F|   T   | F

For iff operation, see xnor.
*/
static int agnB_implies (lua_State *L) {  /* 2.15.5 */
  int a, b;
  if (lua_isnumber(L, 1) && lua_isnumber(L, 2)) {
    lua_Number a, b;
    a = agn_tonumber(L, 1);
    b = agn_tonumber(L, 2);
    lua_pushnumber(L, (agn_getbitwise(L)) ? ((~(LUA_INT32)a) | (LUA_INT32)b) : ((~(LUA_UINT32)a) | (LUA_UINT32)b) );
    return 1;
  }
  if (lua_isboolean(L, 1) && lua_isboolean(L, 2)) {
    a = lua_toboolean(L, 1);
    b = lua_toboolean(L, 2);
  } else if (lua_isnil(L, 1) && lua_isboolean(L, 2)) {  /* 5.3.2 */
    a = 0;
    b = lua_toboolean(L, 2);
  } else if (lua_isboolean(L, 1) && lua_isnil(L, 2)) {  /* 5.3.2 */
    a = lua_toboolean(L, 1);
    b = 0;
  } else {  /* 3.13.3 */
    a = b = 0;
    luaL_error(L, "Error in " LUA_QS ": either numbers or booleans expected.", "implies");
  }
  if (a == -1) a = 0;
  if (b == -1) b = 0;
  lua_pushboolean(L, !a || b);
  return 1;
}


/* Performs a binary search for x in the _sorted_ table, sequence or register o. You may optionally specify the left border l
   and the right border r in o where to search for x, by default l is 1 and r is size o. The very first element in o to be checked
   is given by m which by default is (l + r) \ 2. The function returns `true` on success or `false` otherwise. The second return
   is the index position of the last element checked before the function returns.

   You may have to sort o before invoking the function, otherwise the result would be incorrect. */

static int num_comp (lua_State *L, int fnidx, lua_Number x, lua_Number y) {
  int res;
  luaL_checkstack(L, 3, "not enough stack space");
  lua_pushvalue(L, fnidx);  /* push function */
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  lua_call(L, 2, 1);  /* call function */
  if (!lua_isboolean(L, -1))
    luaL_error(L, "Error in " LUA_QS ": function must evaluate to a Boolean.", "binsearch");
  res = lua_toboolean(L, -1);  /* get result */
  agn_poptop(L);  /* pop result */
  return res;
}

static int str_comp (lua_State *L, int fnidx, const char *x, const char *y) {
  int res;
  luaL_checkstack(L, 3, "not enough stack space");
  lua_pushvalue(L, fnidx);  /* push function */
  lua_pushstring(L, x);
  lua_pushstring(L, y);
  lua_call(L, 2, 1);  /* call function */
  if (!lua_isboolean(L, -1))
    luaL_error(L, "Error in " LUA_QS ": function must evaluate to a Boolean.", "binsearch");
  res = lua_toboolean(L, -1);  /* get result */
  agn_poptop(L);  /* pop result */
  return res;
}

static void aux_binsearchoptions (lua_State *L, int pos, int *nargs, int *sortidx, int *finidx, const char *procname) {
  int checkoptions;
  *sortidx = 0;  /* 0 = no sorting order function given */
  *finidx = 0;   /* 0 = no final comparison function given */
  /* check for options, here `map in-place` */
  checkoptions = 2;  /* check n options; CHANGE THIS if you add/delete options */
  while (checkoptions-- && *nargs >= pos && lua_ispair(L, *nargs)) {
    /* check for properly allocated slots will be done by agn_pairgetiall() */
    agn_pairgetiall(L, *nargs);  /* get left, right value, set to stack index -2, -1, 7.7.8 */
    if (agn_isstring(L, -2)) {
      const char *option = agn_tostring(L, -2);
      if (tools_streq(option, "sorting") && lua_isfunction(L, -1)) {
        lua_replace(L, *nargs);  /* replace option with rhs-function .. */
        *sortidx = (*nargs)--;  /* .. and return its stack index */
        agn_poptop(L);  /* remove lhs of option */
        continue;
      } else if (tools_streq(option, "compare") && lua_isfunction(L, -1)) {
        lua_replace(L, *nargs);  /* replace option with rhs-function .. */
        *finidx = (*nargs)--;  /* .. and return its stack index */
        agn_poptop(L);  /* remove lhs of option */
        continue;
      } else {
        agn_poptoptwo(L);
        luaL_error(L, "Error in " LUA_QS ": unknown option " LUA_QS " or missing right-hand side function.", procname, option);
      }
    }
    /* do not call lua_settop as it would corrupt the argument stack since we have already pushed values */
    (*nargs)--;
    agn_poptoptwo(L);
  }
}

static int agnB_binsearch (lua_State *L) {  /* 2.12.5 */
  int type1, type2, l, r, m, n, res, nargs, sortidx, finidx;
  nargs = lua_gettop(L);
  n = (int)agn_nops(L, 1);
  type1 = lua_type(L, 1);
  type2 = lua_type(L, 2);
  luaL_typecheck(L, type1 == LUA_TTABLE || type1 == LUA_TSEQ || type1 == LUA_TREG, 1, "table, sequence or register expected", type1);
  luaL_typecheck(L, type2 == LUA_TSTRING || type2 == LUA_TNUMBER, 2, "number or string expected", type2);
  aux_binsearchoptions(L, 3, &nargs, &sortidx, &finidx, "binsearch");
  l = nargs > 2 && agn_isposint(L, 4) ? agn_tointeger(L, 4) : 1;
  r = nargs > 2 && agn_isposint(L, 5) ? agn_tointeger(L, 5) : n;
  m = nargs > 2 && agn_isposint(L, 3) ? agn_tointeger(L, 3) : tools_midpoint(l, r);
  if (l >= r || n < 2 || r > n || l < 1 || m < l || m > r) {
    lua_pushfail(L);
    return 1;
  }
  res = 0;
  switch (type2) {
    case LUA_TNUMBER: {
      lua_Number x, y;
      x = agn_checknumber(L, 2);
      y = 0.0;
      switch (type1) {
        case LUA_TTABLE: {
          while (l < r) {
            y = agn_getinumber(L, 1, m);
            if ((!finidx && x == y) || (finidx && num_comp(L, finidx, x, y))) {
              r = m; res = 1; goto binsearchexit;
            }
            if ((!sortidx && y < x) || (sortidx && !num_comp(L, sortidx, x, y)))
              l = m + 1;
            else
              r = m;
            m = tools_midpoint(l, r);  /* 2.38.2 patch */
          }
          y = agn_getinumber(L, 1, r);
          break;
        }
        case LUA_TSEQ: {
          while (l < r) {
            y = lua_seqrawgetinumber(L, 1, m);
            if ((!finidx && x == y) || (finidx && num_comp(L, finidx, x, y))) {
              r = m; res = 1; goto binsearchexit;
            }
            if ((!sortidx && y < x) || (sortidx && !num_comp(L, sortidx, x, y)))
              l = m + 1;
            else
              r = m;
            m = tools_midpoint(l, r);  /* 2.38.2 patch */
          }
          y = lua_seqrawgetinumber(L, 1, r);
          break;
        }
        case LUA_TREG: {
          while (l < r) {
            y = agn_reggetinumber(L, 1, m);
            if ((!finidx && x == y) || (finidx && num_comp(L, finidx, x, y))) {
              r = m; res = 1; goto binsearchexit;
            }
            if ((!sortidx && y < x) || (sortidx && !num_comp(L, sortidx, x, y)))
              l = m + 1;
            else
              r = m;
            m = tools_midpoint(l, r);  /* 2.38.2 patch */
          }
          y = agn_reggetinumber(L, 1, r);
          break;
        }
        default: {  /* should not happen */
          lua_assert(0);
        }
      }  /* end switch */
      res = (!finidx && x == y) || (finidx && num_comp(L, finidx, x, y));
      break;
    }
    case LUA_TSTRING: {  /* new 5.4.3 */
      const char *x, *y;
      size_t len;
      int rc, scmp;
      x = agn_checkstring(L, 2);
      y = NULL;
      switch (type1) {
        case LUA_TTABLE: {
          while (l < r) {
            y = agn_rawgetilstring(L, 1, m, &len, &rc);
            if (!rc)
              luaL_error(L, "Error in " LUA_QS ": table value is not a string.", "binsearch");
            scmp = strcoll(x, y);
            if (scmp == 0) { r = m; res = 1; goto binsearchexit; }
            if ((!sortidx && scmp > 0) || (sortidx && !str_comp(L, sortidx, x, y)))
              l = m + 1;
            else
              r = m;
            m = tools_midpoint(l, r);
          }
          y = agn_rawgetilstring(L, 1, r, &len, &rc);
          if (!rc)
            luaL_error(L, "Error in " LUA_QS ": table value is not a string.", "binsearch");
          break;
        }
        case LUA_TSEQ: {
          while (l < r) {
            y = agn_seqrawgetilstring(L, 1, m, &len, &rc);
            if (!rc)
              luaL_error(L, "Error in " LUA_QS ": sequence value is not a string.", "binsearch");
            scmp = strcoll(x, y);
            if (scmp == 0) { r = m; res = 1; goto binsearchexit; }
            if ((!sortidx && scmp > 0) || (sortidx && !str_comp(L, sortidx, x, y)))
              l = m + 1;
            else
              r = m;
            m = tools_midpoint(l, r);
          }
          y = agn_seqrawgetilstring(L, 1, r, &len, &rc);
          if (!rc)
            luaL_error(L, "Error in " LUA_QS ": sequence value is not a string.", "binsearch");
          break;
        }
        case LUA_TREG: {
          while (l < r) {
            y = agn_regrawgetilstring(L, 1, m, &len, &rc);
            if (!rc)
              luaL_error(L, "Error in " LUA_QS ": register value is not a string.", "binsearch");
            scmp = strcoll(x, y);
            if (scmp == 0) { r = m; res = 1; goto binsearchexit; }
            if ((!sortidx && scmp > 0) || (sortidx && !str_comp(L, sortidx, x, y)))
              l = m + 1;
            else
              r = m;
            m = tools_midpoint(l, r);
          }
          y = agn_regrawgetilstring(L, 1, r, &len, &rc);
          if (!rc)
            luaL_error(L, "Error in " LUA_QS ": register value is not a string.", "binsearch");
          break;
        }
        default: {  /* should not happen */
          lua_assert(0);
        }
      }  /* end switch */
      res = (!finidx && strcoll(x, y) == 0) || (finidx && str_comp(L, finidx, x, y));
      break;
    }
    default: lua_assert(0);  /* should not happen */
  }
binsearchexit:
  lua_pushboolean(L, res);
  lua_pushinteger(L, r);  /* index of the element that either fits or where the algorithm quit. */
  return 2;
}


/* Checks whether numeric or complex x is a multiple of numeric y, i.e. whether x/y evaluates to an integral, and returns `true` or `false`.
   Also returns `true` with x = 0 and any non-zero y.

   If y is zero, `undefined` or +/-`infinity`, the function returns `fail`.

   With complex x, returns `true` if both real(x)/y and imag(x)/y evaluate to the same integral, or if real(x)/y evaluates to an integral
   and imag(x) is zero.

   By passing the optional third argument `true`, a tolerant check is done, with subnormal x or y first converted to zero, and a subsequent
   approximate equality check to the nearest integer of x/y. The tolerance value internally used is the value of `DoubleEps` at the time of
   the function call. */
#define isnearint(q,eps) (tools_approx(q, sun_round(q), eps))
static int agnB_multiple (lua_State *L) {  /* 2.21.8 */
  lua_Number y = agn_checknumber(L, 2);
  int flag = agnL_optboolean(L, 3, 0);  /* inexactness permitted ? */

  if (sun_isexceptional(y)) {  /* +/-0, inf, nan, 6.7.8 change */
    lua_pushfail(L);
    return 1;
  }
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER: {
      lua_Number x = agn_tonumber(L, 1);
      if (!flag) {
        lua_pushboolean(L, tools_isint(x/y));
      } else {
        /* the isnearint() macro evaluates its argument twice so make it short ! */
        lua_Number q = tools_chop(x)/tools_chop(y);
        lua_pushboolean(L, isnearint(q, agn_getdblepsilon(L)));
      }
      break;
    }
    case LUA_TCOMPLEX: {
      lua_Number re, im;
      agn_getcmplxparts(L, 1, &re, &im);
      if (!flag)
        lua_pushboolean(L, (re == im || im == 0) && tools_isint(re/y) && tools_isint(im/y));
      else {
        y = tools_chop(y);
        re = tools_chop(re);
        im = tools_chop(im);
        if (re != im && im != 0)
          lua_pushfalse(L);
        else {  /* agn_getdbleps is a just a one-line macro */
          lua_pushboolean(L, isnearint(re/y, agn_getdblepsilon(L)) && isnearint(im/y, agn_getdblepsilon(L)));
        }
      }
      break;
    }
    default:
      luaL_nonumorcmplx(L, 1, "multiple");
  }
  return 1;
}


static int agnB_char (lua_State *L) {  /* 4.3.3, 13 % faster than just executing `lua_pushchar(L, agn_checknonnegint(L, 1))` */
  agn_char(L, 1);
  return 1;
}


static int agnB_freeze (lua_State *L) {  /* 4.9.0 */
  int t = lua_type(L, 1);
  luaL_typecheck(L, t >= LUA_TTABLE || t == LUA_TUSERDATA, 1, "structure or userdata expected", t);
  agn_freeze(L, 1, 1);
  lua_pushboolean(L, agn_freeze(L, 1, -1) == 1);
  return 1;
}


static int agnB_unfreeze (lua_State *L) {  /* 4.9.0 */
  int t = lua_type(L, 1);
  luaL_typecheck(L, t >= LUA_TTABLE || t == LUA_TUSERDATA, 1, "structure or userdata expected", t);
  agn_freeze(L, 1, 0);
  lua_pushboolean(L, agn_freeze(L, 1, -1) == 0);
  return 1;
}


static const luaL_Reg base_funcs[] = {
  {"addtometatable", luaB_addtometatable}, /* added May 09, 2024 */
  {"allotted", agnB_allotted},             /* added November 30, 2015 */
  {"alternate", agnB_alternate},           /* added January 24, 2013 */
  {"append", agnB_append},                 /* added January 12, 2024 */
  {"assume", luaB_assume},
  {"binsearch", agnB_binsearch},           /* added July 27, 2018 */
  {"char", agnB_char},                     /* added October 11, 2024 */
  {"checkoptions", agnB_checkoptions},     /* added October 29, 2012 */
  {"cleanse", luaB_cleanse},               /* added October 19, 2022 */
  {"copy", agnB_copy},                     /* added November 27, 2022 */
  {"copyadd", agnB_copyadd},               /* added November 12, 2022 */
  {"countitems", agnB_countitems},         /* added January 18, 2010 */
  {"deletefrom", agnB_deletefrom},
  {"descend", agnB_descend},               /* added January 01, 2010 */
  {"drop", agnB_drop},                     /* added June 28, 2026 */
  {"error", luaB_error},
  {"everyth", agnB_everyth},               /* added July 29, 2015 */
  {"fact", mathB_fact},                    /* added on May 16, 2024 */
  {"fold", agnB_fold},                     /* added February 04, 2023 */
  {"freeze", agnB_freeze},                 /* added February 08, 2025 */
  {"getbit", agnB_getbit},                 /* added June 10, 2012 */
  {"getbits", agnB_getbits},               /* added January 08, 2015 */
  {"getnbits", agnB_getnbits},             /* added July 23, 2018 */
  {"getentry", agnB_getentry},             /* added January 13, 2010 */
  {"getmetatable", luaB_getmetatable},
  {"getorset", agnB_getorset},             /* added October 30, 2018 */
  {"gettype", luaB_gettype},               /* added May 05, 2008 */
  {"has", agnB_has},                       /* added June 30, 2012 */
  {"identity", agnB_identity},             /* added August 12, 2016 */
  {"implies", agnB_implies},               /* added July 21, 2018 */
  {"include", agnB_include},               /* added January 12, 2024 */
  {"isall", agnB_isall},                   /* added November 26, 2024 */
  {"isboolean", agnB_isboolean},           /* added June 24, 2010 */
  {"iscomplex", agnB_iscomplex},           /* added June 24, 2010 */
  {"isint", agnB_isint},                   /* added April 01, 2010 */
  {"isnegative", agnB_isnegative},         /* added May 12, 2013 */
  {"isnegint", agnB_isnegint},             /* added June 24, 2010 */
  {"isnonneg", agnB_isnonneg},             /* added May 12, 2013 */
  {"isnonnegint", agnB_isnonnegint},       /* added June 24, 2010 */
  {"isnonposint", agnB_isnonposint},       /* added December 13, 2010 */
  {"isnonzeroint", agnB_isnonzeroint},     /* added April 06, 2025 */
  {"isnumber", agnB_isnumber},             /* added June 24, 2010 */
  {"isnumeric", agnB_isnumeric},           /* added June 24, 2010 */
  {"ispair", agnB_ispair},                 /* added June 24, 2010 */
  {"isposint", agnB_isposint},             /* added June 24, 2010 */
  {"ispositive", agnB_ispositive},         /* added May 12, 2013 */
  {"isstring", agnB_isstring},             /* added June 24, 2010 */
  {"isstructure", agnB_isstructure},       /* added June 24, 2010 */
  {"istable", agnB_istable},               /* added June 24, 2010 */
  {"isset", agnB_isset},                   /* added June 24, 2010 */
  {"isseq", agnB_isseq},                   /* added June 24, 2010 */
  {"isseq", agnB_isseq},                   /* added June 24, 2010 */
  {"isreg", agnB_isreg},                   /* added October 15, 2014 */
  {"load", luaB_load},
  {"loadfile", luaB_loadfile},
  {"loadstring", luaB_loadstring},
  {"map", agnB_map},                       /* added April 07, 2007 */
  {"max", luaB_max},
  {"member", agnB_member},                 /* added December 09, 2023 */
  {"min", luaB_min},
  {"mprint", luaB_mprint},
  {"multiple", agnB_multiple},             /* added August 15, 2020 */
  {"nextone", luaB_nextone},
  {"op", agnB_op},                         /* added August 09, 2024 */
  {"ops", luaB_ops},
  {"optboolean", agnB_optboolean},         /* added February 01, 2015 */
  {"optcomplex", agnB_optcomplex},         /* added February 01, 2015 */
  {"optint", agnB_optint},                 /* added February 01, 2015 */
  {"optnonnegative", agnB_optnonnegative}, /* added February 01, 2015 */
  {"optnonnegint", agnB_optnonnegint},     /* added February 01, 2015 */
  {"optnonzeroint", agnB_optnonzeroint},   /* added April 06, 2025 */
  {"optnumber", agnB_optnumber},           /* added February 01, 2015 */
  {"optposint", agnB_optposint},           /* added February 01, 2015 */
  {"optpositive", agnB_optpositive},       /* added February 01, 2015 */
  {"optstring", agnB_optstring},           /* added February 01, 2015 */
  {"pack", agnB_pack},                     /* added December 13, 2024 */
  {"popd", agnB_popd},                     /* added November 29, 2015 */
  {"prepend", agnB_prepend},               /* added January 12, 2024 */
  {"print", luaB_print},
  {"protect", luaB_protect},               /* added November 07, 2010 */
  {"purge", agnB_purge},
  {"put", agnB_put},
  {"rawequal", luaB_rawequal},
  {"rawget", luaB_rawget},
  {"rawgeti", luaB_rawgeti},               /* added April 10, 2026 */
  {"rawset", luaB_rawset},
  {"rawseti", luaB_rawseti},               /* Added April 17, 2026 */
  {"readlib", agnB_readlib},               /* added December 10, 2006 */
  {"recurse", agnB_recurse},               /* added July 08, 2012 */
  {"reduce", agnB_reduce},                 /* added May 17, 2018 */
  {"remove", luaB_remove},                 /* added March 30, 2008 */
  {"reverse", agnB_reverse},               /* added June 06, 2015 */
  {"rsorted", agnB_rsorted},               /* added on August 06, 2025 */
  {"run", luaB_run},
  {"satisfy", agnB_satisfy},               /* added September 22, 2018 */
  {"select", luaB_select},
  {"selectremove", luaB_selectremove},     /* added November 09, 2012 */
  {"setbit", agnB_setbit},                 /* added June 10, 2012 */
  {"setbits", agnB_setbits},               /* added January 08, 2015 */
  {"setmetatable", luaB_setmetatable},
  {"setnbits", agnB_setnbits},             /* added July 24, 2018 */
  {"settype", luaB_settype},               /* added May 05, 2008 */
  {"sort", luaB_sort},
  {"sorted", agnB_sorted},                 /* added November 17, 2012 */
  {"subs", agnB_subs},                     /* added November 01, 2009 */
  {"subsop", agnB_subsop},                 /* added January 01, 2024 */
  {"time", agnB_time},
  {"times", agnB_times},                   /* added November 28, 2022 */
  {"tonumber", luaB_tonumber},
  {"toreg", agnB_toreg},                   /* added October 15, 2014 */
  {"toseq", agnB_toseq},                   /* added June 14, 2008 */
  {"toset", agnB_toset},                   /* added October 13, 2009 */
  {"tostring", luaB_tostring},
  {"totable", agnB_totable},               /* added June 08, 2007 */
  {"unfreeze", agnB_unfreeze},             /* added February 08, 2025 */
  {"unique", agnB_unique},                 /* added November 27, 2022 */
  {"unpack", luaB_unpack},
  {"values", agnB_values},                 /* added November 28, 2022 */
  {"vmcheckfor", agnB_vmcheckfor},         /* added June 26, 2024 */
  {"vmcount", agnB_vmcount},               /* added June 30, 2026 */
  {"vmmap", agnB_vmmap},                   /* added April 07, 2007 */
  {"vmselect", agnB_vmselect},             /* June 27, 2024 */
  {"whereis", agnB_whereis},               /* added October 06, 2012 */
  {"xpcall", luaB_xpcall},
  {"zip", agnB_zip},                       /* added September 04, 2008 */
  /* mathematical functions */
  {"approx", mathB_approx},
  {"arccosh", mathB_arccosh},              /* added on October 04, 2022 */
  {"arccoth", mathB_arccoth},              /* added on June 29, 2008 */
  {"arccsc", mathB_arccsc},                /* added on October 08, 2022 */
  {"arccsch", mathB_arccsch},              /* added on June 18, 2023 */
  {"arcsech", mathB_arcsech},              /* added on October 07, 2022 */
  {"arcsinh", mathB_arcsinh},              /* added on October 04, 2022 */
  {"arctan2", mathB_arctan2},
  {"arctanh", mathB_arctanh},              /* added June 18, 2023 */
  {"argument", mathB_argument},            /* added June 29, 2008 */
  {"besselj", mathB_besselj},              /* added August 22, 2009 */
  {"bessely", mathB_bessely},              /* added August 22, 2009 */
  {"binomial", mathB_binomial},            /* added December 26, 2007 */
  {"cabs", agnB_cabs},                     /* added January 06, 2014 */
  {"cartesian", mathB_cartesian},          /* added October 17, 2017 */
  {"cas", agnB_cas},                       /* added October 27, 2017 */
  {"cathet", mathB_cathet},                /* added September 17, 2017 */
  {"cbrt", agnB_cbrt},                     /* added August 25, 2013 */
  {"ceil", agnB_ceil},                     /* added October 28, 2017 */
  {"cosc", agnB_cosc},                     /* added October 07, 2017 */
  {"cot", agnB_cot},                       /* added October 07, 2017 */
  {"coth", agnB_coth},                     /* added October 07, 2017 */
  {"csc", agnB_csc},                       /* added October 07, 2022 */
  {"drem", agnB_drem},                     /* added December 01, 2013 */
  {"erf", mathB_erf},                      /* added August 22, 2009 */
  {"erfc", mathB_erfc},                    /* added August 22, 2009 */
  {"erfcx", agnB_erfcx},                   /* added July 21, 2020 */
  {"erfi", agnB_erfi},                     /* added July 28, 2020 */
  {"expx2", mathB_expx2},                  /* added May 15, 2010 */
  {"exp2", mathB_exp2},                    /* added October 04, 2022 */
  {"exp10", mathB_exp10},                  /* added October 04, 2022 */
  {"floor", agnB_floor},                   /* added December 7, 2022 */
  {"fma", mathB_fma},                      /* added November 23, 2009 */
  {"frexp", mathB_frexp},
  {"frexp10", mathB_frexp10},              /* added August 05, 2022 */
  {"gamma", mathB_gamma},                  /* added June 21, 2010 */
  {"heaviside", mathB_heaviside},          /* added February 12, 2026 */
  {"hypot", mathB_hypot},                  /* added June 19, 2007 */
  {"hypot2", mathB_hypot2},                /* added July 10, 2016 */
  {"hypot3", mathB_hypot3},                /* added August 04, 2017 */
  {"ilog2", mathB_ilog2},                  /* added October 21, 2014 */
  {"ilog10", mathB_ilog10},                /* added October 21, 2014 */
  {"inverf", mathB_inverf},                /* added July 08, 2020 */
  {"inverfc", mathB_inverfc},              /* added July 08, 2020 */
  {"invgamma", mathB_invgamma},            /* added on July 29, 2023 */
  {"invhypot", mathB_invhypot},            /* added on January 05, 2023 */
  {"invpsi", agnB_invpsi},                 /* added November 25, 2025 */
  {"invpytha", mathB_invpytha},            /* added on November 20, 2024 */
  {"ldexp", mathB_ldexp},                  /* see also scalbn */
  {"log2", agnB_log2},                     /* added February 16, 2013 */
  {"log10", agnB_log10},                   /* added February 18, 2013 */
  {"iqr", mathB_iqr},                      /* added April 10, 2013 */
  {"iquo", mathB_iquo},                    /* added June 24, 2024 */
  {"irem", mathB_irem},                    /* added June 24, 2024 */
  {"mdf", mathB_mdf},                      /* added July 18, 2014 */
  {"modf", mathB_modf},
  {"modp", mathB_modp},                    /* added November 19, 2025 */
  {"polar", mathB_polar},                  /* added April 16, 2013 */
  {"proot", mathB_proot},                  /* added August 28, 2013 */
  {"psi", agnB_psi},                       /* added on May 25, 2010 */
  {"pytha", mathB_pytha},                  /* added August 08, 2020 */
  {"pytha4", mathB_pytha4},                /* added July 25, 2023 */
  {"rect", mathB_rect},                    /* added February 08, 2015 */
  {"root", mathB_root},                    /* added June 29, 2008 */
  {"round", mathB_round},                  /* added December 03, 2006 */
  {"scalbn", mathB_ldexp},                 /* just an alias to ldexp, June 29, 2022 */
  {"sec", agnB_sec},                       /* added October 07, 2022 */
  {"tanc", mathB_tanc},                    /* added July 29, 2015 */
  {"xdf", mathB_xdf},                      /* added July 18, 2014 */
  {NULL, NULL}
};


/*
** {======================================================
** Coroutine library
** =======================================================
*/

static int auxresume (lua_State *L, lua_State *co, int narg) {
  int status;
  if (!lua_checkstack(co, narg))
    luaL_error(L, "too many arguments to resume.");
  if (lua_status(co) == 0 && lua_gettop(co) == 0) {
    lua_pushliteral(L, "cannot resume dead coroutine");
    return -1;  /* error flag */
  }
  lua_xmove(L, co, narg);
  status = lua_resume(co, narg);
  if (status == 0 || status == LUA_YIELD) {
    int nres = lua_gettop(co);
    /* if (!lua_checkstack(L, nres)) 5.1.4 patch 2 */
    if (!lua_checkstack(L, nres + 1))
      luaL_error(L, "too many results to resume.");
    lua_xmove(co, L, nres);  /* move yielded values */
    return nres;
  }
  else {
    lua_xmove(co, L, 1);  /* move error message */
    return -1;  /* error flag */
  }
}


static int luaB_coresume (lua_State *L) {
  lua_State *co = lua_tothread(L, 1);
  int r;
  luaL_argcheck(L, co, 1, "coroutine expected");
  r = auxresume(L, co, lua_gettop(L) - 1);
  if (r < 0) {
    lua_pushfalse(L);
    lua_insert(L, -2);
    return 2;  /* return false + error message */
  }
  else {
    lua_pushtrue(L);
    lua_insert(L, -(r + 1));
    return r + 1;  /* return true + `resume' returns */
  }
}


static int luaB_auxwrap (lua_State *L) {
  lua_State *co = lua_tothread(L, lua_upvalueindex(1));
  int r = auxresume(L, co, lua_gettop(L));
  if (r < 0) {
    if (agn_isstring(L, -1)) {  /* error object is a string? */
      luaL_where(L, 1);  /* add extra info */
      lua_insert(L, -2);
      lua_concat(L, 2);
    }
    lua_error(L);  /* propagate error */
  }
  return r;
}


static int luaB_cocreate (lua_State *L) {
  lua_State *NL = lua_newthread(L);
  luaL_argcheck(L, lua_isfunction(L, 1) && !lua_iscfunction(L, 1), 1,
    "Agena function expected");
  lua_pushvalue(L, 1);  /* move function to top */
  lua_xmove(L, NL, 1);  /* move function from L to NL */
  return 1;
}


static int luaB_cowrap (lua_State *L) {
  luaB_cocreate(L);
  lua_pushcclosure(L, luaB_auxwrap, 1);
  return 1;
}


static int luaB_yield (lua_State *L) {
  return lua_yield(L, lua_gettop(L));
}


static int luaB_costatus (lua_State *L) {
  lua_State *co = lua_tothread(L, 1);
  luaL_argcheck(L, co, 1, "coroutine expected");
  if (L == co) lua_pushliteral(L, "running");
  else {
    switch (lua_status(co)) {
      case LUA_YIELD:
        lua_pushliteral(L, "suspended");
        break;
      case 0: {
        lua_Debug ar;
        if (lua_getstack(co, 0, &ar) > 0)  /* does it have frames? */
          lua_pushliteral(L, "normal");  /* it is running */
        else if (lua_gettop(co) == 0)
            lua_pushliteral(L, "dead");
        else
          lua_pushliteral(L, "suspended");  /* initial state */
        break;
      }
      default:  /* some error occurred */
        lua_pushliteral(L, "dead");
        break;
    }
  }
  return 1;
}


static int luaB_corunning (lua_State *L) {
  if (lua_pushthread(L))
    return 0;  /* main thread is not a coroutine */
  else
    return 1;
}


static const luaL_Reg co_funcs[] = {
  {"setup", luaB_cocreate},
  {"resume", luaB_coresume},
  {"running", luaB_corunning},
  {"status", luaB_costatus},
  {"wrap", luaB_cowrap},
  {"yield", luaB_yield},
  {NULL, NULL}
};

/* }====================================================== */

static void auxopen (lua_State *L, const char *name,  /* reintroduced 2.19.1 */
                     lua_CFunction f, lua_CFunction u) {
  lua_pushcfunction(L, u);    /* push C iterator */
  lua_pushcclosure(L, f, 1);  /* push baselib C function of one argument */
  lua_setfield(L, -2, name);  /* associate iterator with baselib C function */
}

static void base_open (lua_State *L) {
  /* set global _G */
  lua_pushvalue(L, LUA_GLOBALSINDEX);
  lua_setglobal(L, "_G");
  /* define Pi, 2*Pi, PI/2, Pi/4, EXP(1), EXP(1) */
  lua_pushnumber(L, PI);
  lua_setglobal(L, "Pi");
  lua_pushnumber(L, PI2);         /* 6.2831853071795865 */
  lua_setglobal(L, "Pi2");
  lua_pushnumber(L, PIO2);        /* 1.5707963267948966 */
  lua_setglobal(L, "PiO2");
  lua_pushnumber(L, PIO4);        /* 0.7853981633974483 */
  lua_setglobal(L, "PiO4");
  lua_pushnumber(L, EXP1);        /* 2.7182818284590452 */
  lua_setglobal(L, "Exp");
  lua_pushnumber(L, EXP1);        /* 2.7182818284590452 */
  lua_setglobal(L, "E");
  lua_pushnumber(L, PHI);         /* added 0.5.0, moved to baselib 2.20.0 RC 1 */
  lua_setglobal(L, "Phi");        /* added 0.5.0, Golden ratio */
  lua_pushnumber(L, PHIINV);      /* non-negative inverse Golden ratio 0.6180339887498948482, 3.16.5 */
  lua_setglobal(L, "InvPhi");
  lua_pushnumber(L, EULERGAMMA);  /* 0.5772156649015329 */
  lua_setglobal(L, "EulerGamma");
  lua_pushnumber(L, agn_getepsilon(L));
  lua_setglobal(L, "Eps");
  lua_pushnumber(L, agn_getdblepsilon(L));  /* changed 2.21.8 */
  lua_setglobal(L, "DoubleEps");
  lua_pushnumber(L, agn_gethepsilon(L));    /* added 2.31.1 */
  lua_setglobal(L, "hEps");                 /* epsilon value used in quite a lot of approximations */
  lua_pushnumber(L, HUGE_VAL);
  lua_setglobal(L, "infinity");
  lua_pushnumber(L, AGN_NAN);
  lua_setglobal(L, "undefined");
  lua_pushnumber(L, RADIANS_PER_DEGREE);
  lua_setglobal(L, "radians");
  lua_pushnumber(L, 1/RADIANS_PER_DEGREE);
  lua_setglobal(L, "degrees");
  /* open lib into global table */
  luaL_register(L, "_G", base_funcs);
  lua_pushliteral(L, AGENA_RELEASE);  /* added */
  lua_setglobal(L, "_RELEASE");  /* set global _RELEASE */
  /* `ipairs' and `pairs' need auxiliary functions as upvalues */
  auxopen(L, "ipairs", luaB_ipairs, ipairsaux);  /* reintroduced 2.19.1 */
  auxopen(L, "pairs", luaB_pairs, luaB_nextone); /* reintroduced 2.19.1 */
  /* `newproxy' needs a weaktable as upvalue */
  lua_createtable(L, 0, 1);  /* new table `w' */
  lua_pushvalue(L, -1);  /* `w' will be its own metatable */
  lua_setmetatable(L, -2);
  lua_pushliteral(L, "kv");
  lua_setfield(L, -2, "__weak");  /* metatable(w).__mode = "kv" */
  lua_pushcclosure(L, luaB_newproxy, 1);
  lua_setglobal(L, "newproxy");  /* set global `newproxy' */
}


LUALIB_API int luaopen_base (lua_State *L) {
  base_open(L);
  luaL_register(L, LUA_COLIBNAME, co_funcs);
  return 2;
}

