/*
** $Id: lenviron.c,v 1.00 17.12.2010 alex Exp $
** library to query the Agena environment
** See Copyright Notice in agena.h
*/

#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <unistd.h>  /* OS/2 `optarg`optarg need this */

#ifdef _WIN32  /* for GetProcessMemoryInfo, GetCurrentProcess */
#include <windows.h>
#include <psapi.h>
#endif

#define lenviron_c
#define LUA_LIB

#include "agena.h"

#include "agenalib.h"
#include "agncmpt.h"
#include "agnxlib.h"
#include "lapi.h"
#include "ldebug.h"
#include "lgc.h"
#include "lobject.h"
#include "lstate.h"
#include "prepdefs.h"
#include "sunpro.h"

static void getfunc (lua_State *L, int opt) {  /* Lua 5.1.2 patch */
  if (lua_isfunction(L, 1)) lua_pushvalue(L, 1);
  else {
    lua_Debug ar;
    int level = opt ? agnL_optinteger(L, 1, 1) : agnL_checkint(L, 1);  /* Lua 5.1.2 patch */
    luaL_argcheck(L, level >= 0, 1, "level must be non-negative");
    if (lua_getstack(L, level, &ar) == 0)
      luaL_argerror(L, 1, "invalid level");
    lua_getinfo(L, "f", &ar);
    if (lua_isnil(L, -1))
      luaL_error(L, "Error in " LUA_QS ": no function environment for tail call at level %d.",
                    "environ.getfunc", level);
  }
}


static int environ_getfenv (lua_State *L) {  /* moved from Lua's baselib to the environ package */
  getfunc(L, 1);  /* Lua 5.1.2 patch */
  if (lua_iscfunction(L, -1))  /* is a C function? */
    lua_pushvalue(L, LUA_GLOBALSINDEX);  /* return the thread's global env. */
  else
    lua_getfenv(L, -1);
  return 1;
}


static int environ_setfenv (lua_State *L) {
  luaL_checktype(L, 2, LUA_TTABLE);
  getfunc(L, 1);  /* Lua 5.1.2 patch */
  lua_pushvalue(L, 2);
  if (agn_isnumber(L, 1) && agn_tonumber(L, 1) == 0) {
    /* change environment of current thread */
    lua_pushthread(L);
    lua_insert(L, -2);
    lua_setfenv(L, -2);
    return 0;
  }
  else if (lua_iscfunction(L, -2) || lua_setfenv(L, -2) == 0)
    luaL_error(L,
          LUA_QL("environ.setfenv") " cannot change environment of given object.");
  return 1;
}


static int environ_userinfo (lua_State *L) {  /* 0.22.3, June 14, 2009, changed 1.6.4 */
  lua_Number a, b;
  int i, exception, type;
  luaL_checktype(L, 1, LUA_TFUNCTION);
  a = agn_checknumber(L, 2);
  type = agnL_gettablefield(L, "environ", "infolevel", "environ.userinfo", 1);  /* 1.6.4 */
  if (type != LUA_TTABLE) {
    agn_poptop(L);  /* remove "infolevel" (or "environ" if table environ does not exist) */
    return 0;  /* infolevel not assigned or not a table */
  }
  lua_pushvalue(L, 1);  /* push function */
  lua_rawget(L, -2);  /* function is popped and the infolevel value is put on top of the stack */
  b = agn_tonumberx(L, -1, &exception);
  agn_poptoptwo(L);  /* pop value and "infolevel" */
  if (a <= b && !exception) {
    for (i=3; i <= lua_gettop(L); i++) {
      lua_pushvalue(L, i);
      agnL_printnonstruct(L, -1);
      agn_poptop(L);
    }
    fflush(stdout);
  }
  return 0;
}


static int environ_used (lua_State *L) {  /* 0.27.2 */
  int rc = 0;
  static const char *const opts[] = {"bytes", "kbytes", "mbytes", "gbytes", "tbytes", NULL};
  int power = agnL_checkoption(L, 1, "kbytes", opts, 0);
  lua_pushnumber(L, agn_usedbytes(L)/tools_intpow(1024, power));
#ifdef _WIN32  /* 5.5.11 extension */
  { /* Gemini AI: The difference between Committed Memory and Working Set is important. For diagnosing memory leaks,
       you should primarily monitor Committed Memory, as it tells you if your application is reserving more and more
       memory from the system over time, regardless of whether that memory is currently in RAM. The Working Set,
       on the other hand, is a good indicator of the program's real-time physical memory footprint, which can fluctuate
       as the OS manages available RAM for all processes. */
    PROCESS_MEMORY_COUNTERS pmc;
    rc = GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
    if (rc) {
      /* committed memory; MSDN: "the Commit Charge value in bytes for this process. Commit Charge is the total amount
         of memory that the memory manager has committed for a running process." */
      lua_pushnumber(L, (lua_Number)pmc.PagefileUsage/(lua_Number)tools_intpow(1024, power));
      /* The current working set size,  program's real-time physical memory footprint, in bytes. */
      lua_pushnumber(L, (lua_Number)pmc.WorkingSetSize/(lua_Number)tools_intpow(1024, power));
    }
  }
#endif
  return 1 + 2*(rc != 0);
}


static int environ_collectgarbage (lua_State *L) {
  static const char *const opts[] = {"stop", "restart", "collect",
    "count", "step", "setpause", "setstepmul", "status", NULL};
  static const int optsnum[] = {LUA_GCSTOP, LUA_GCRESTART, LUA_GCCOLLECT,
    LUA_GCCOUNT, LUA_GCSTEP, LUA_GCSETPAUSE, LUA_GCSETSTEPMUL, LUA_GCSTATUS};  /* extended 2.2.5 */
  int o = luaL_checkoption(L, 1, "collect", opts);
  int ex = agnL_optinteger(L, 2, 0);
  int res = lua_gc(L, optsnum[o], ex);
  switch (optsnum[o]) {
    case LUA_GCCOUNT: {
      int b = lua_gc(L, LUA_GCCOUNTB, 0);
      lua_pushnumber(L, res + ((lua_Number)b/1024));
      return 1;
    }
    case LUA_GCSTEP: case LUA_GCSTATUS: {  /* 2.2.5, GCSTATUS: true = gc active, false = gc stopped */
      lua_pushboolean(L, res);
      return 1;
    }
    default: {
      lua_pushnumber(L, res);
      return 1;
    }
  }
}


int environ_restart (lua_State *L) {  /* added 0.4.0 */
  const char *path, *obj;
  int islibname, resetlibname, type;
  size_t i;
  islibname = resetlibname = 0;
  resetlibname = agn_getlibnamereset(L); /* 0.26.0, also reset mainlibname and libname to its original value ? */
  path = NULL;  /* to prevent compiler warnings */
  lua_pop(L, lua_gettop(L));  /* clear everything that might be on stack, 2.20.0 change */
  lua_settop(L, 0);  /* 2.20.0, we clear the stack instead of popping a value; formerly just: agn_poptop(L); */
  /* first, clean global constants table */
  lua_pushvalue(L, LUA_CONSTANTSINDEX);  /* 2.20.0 */
  agn_cleanseset(L, -1, 0);  /* simply returns if LUA_CONSTANTSINDEX is not a set */
  agn_poptop(L);  /* pop constants set, or whatever */
  /* second, first delete all userdata and invoke GC, otherwise it cannot be collected correctly, 2.31.8 */
  lua_pushnil(L);
  while (lua_next(L, LUA_GLOBALSINDEX) != 0) {  /* 4.6.5 tweak */
    if (lua_isuserdata(L, -1)) {
      lua_pushvalue(L, -2);  /* duplicate key */
      lua_pushnil(L);
      lua_rawset(L, LUA_GLOBALSINDEX);  /* we can safely delete an entry while traversing */
    }
    agn_poptop(L);  /* pop value */
  }
  lua_gc(L, LUA_GCCOLLECT, 0);
  /* third, delete readlib'bed packages from package.loaded; 0.26.0, changed 2.9.1 */
  lua_getfield(L, LUA_REGISTRYINDEX, "_READLIBBED");
  if (lua_isset(L, -1)) {
    lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");
    if (lua_istable(L, -1)) {
      lua_pushvalue(L, LUA_REGISTRYINDEX);
      lua_pushnil(L);
      while (lua_usnext(L, -4) != 0) {  /* delete readlibbed packages from package.loaded */
        /* package name is on the top of the stack */
        /* delete possible metatable */
        lua_pushvalue(L, -2);  /* duplicate package name */
        if (agn_isstring(L, -1) && !tools_streqx(lua_tostring(L, -1), "binary", "dlist", "ulist", "skew", NULL)) {  /* 7.1.1 fix */
          lua_pushnil(L);
          lua_settable(L, -5);  /* delete metatable from registry */
        } else {
          agn_poptop(L);
        }
        /* delete package name from _LOADED */
        lua_pushnil(L);
        lua_settable(L, -5);
      }
      agn_poptop(L);  /* remove registry */
    }
    /* delete _READLIBBED set and create a new one */
    lua_pushstring(L, "_READLIBBED");
    agn_createset(L, 0);
    lua_settable(L, LUA_REGISTRYINDEX);
    agn_poptop(L);  /* pop registry field `_LOADED' */
  }
  agn_poptop(L); /* delete registry field `_READLIBBED' */
  /* fourth, save environ.onexit function, 2.7.0 */
  type = agnL_gettablefield(L, "environ", "onexit", NULL, 0);
  if (type != LUA_TFUNCTION)
    agn_poptop(L);  /* pop whatever */
  else  /* assign environ.onexit to temporary name */
    lua_setfield(L, LUA_GLOBALSINDEX, "_environonexit");  /* and pop function */
  /* fifth, delete all global vars except libname */
  lua_getfield(L, LUA_GLOBALSINDEX, "_G");  /* check if _G._G is still there */
  if (!lua_istable(L, -1)) {  /* Agena 1.7.1 fix */
    fprintf(stderr, "Warning in " LUA_QS  ": " LUA_QS " is missing or not a table, no restart possible.\n", "restart", "_G");
    fflush(stderr);
    agn_poptop(L);
    return 0;
  }
  agn_poptop(L);
  /* now actually delete values from _G */
  lua_pushnil(L);
  while (lua_next(L, LUA_GLOBALSINDEX) != 0) {  /* 4.6.5 tweak */
    if (agn_isstring(L, -2)) {
      obj = lua_tostring(L, -2);
      islibname = tools_streqx(obj, "libname", "mainlibname", NULL);
      if ((!tools_streqx(obj, "_G", "_environonexit", NULL) && !islibname) || (resetlibname && islibname)) {
        agn_deletefield(L, LUA_GLOBALSINDEX, obj);
      }
    }
    agn_poptop(L);  /* pop value */
  }
  /* sixth, reset stacks, patched 2.12.0 RC 2 */
  L->currentstack = LUA_DEFAULTSTACK;
  L->formerstack = LUA_DEFAULTSTACK;
  for (i=0; i < LUA_NUMSTACKS; i++) {
    xfree(L->cells[i]);  /* realloc causes segmentation faults, so drop the stacks completely ... */
  }
  for (i=0; i < LUA_CHARSTACKS; i++) {
    xfree(L->charcells[i]);  /* 2.12.7, realloc causes segmentation faults, so drop the stacks completely ... */
  }
  /* initialise stacks 0, 2.14.2 */
  for (i=0; i < LUA_NUMSTACKS; i++) {  /* ... and set them up freshly again, first character stack */
    L->cells[i] = malloc(LUA_CELLSTACKSIZE * sizeof(lua_Number));
    if (L->cells[i] == NULL) {  /* 4.11.5 fix */
      size_t j;
      for (j=0; j < i; j++) { xfree(L->cells[j]); }
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "restart");
    }
  }
  /* internal numeric 0 stack */
  for (i=0; i < LUA_CELLSTACKSIZE; i++)
    L->cells[0][i] = 0;
  L->stacktop[0] = LUA_CELLSTACKSIZE;
  L->stackmaxsize[0] = LUA_CELLSTACKSIZE;
  /* character stack */
  for (i=0; i < LUA_CHARSTACKS; i++) {  /* ... and set them up freshly again, 2.12.7 */
    L->charcells[i] = malloc(LUA_CELLSTACKSIZE * sizeof(unsigned char));  /* 4.11.5 fix */
    if (L->charcells[i] == NULL) {
      size_t j;
      for (j=0; j < LUA_NUMSTACKS; j++) { xfree(L->cells[j]); }
      for (j=0; j < i; j++) { xfree(L->charcells[j]); }
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "restart");
    }
  }
  for (i=1; i < LUA_NUMSTACKS + LUA_CHARSTACKS; i++) {
    L->stacktop[i] = LUA_CELLSTACKBOTTOM;
    L->stackmaxsize[i] = LUA_CELLSTACKSIZE;
  }
  /* cache stack */
  L->C->top = L->C->base;
  for (i=0; i < LUA_CACHESTACKS; i++) {  /* 2.37.0 */
    L->stacktop[i + LUA_NUMSTACKS + LUA_CHARSTACKS] = LUA_CELLSTACKBOTTOM;  /* first free slot */
    L->stackmaxsize[i + LUA_NUMSTACKS + LUA_CHARSTACKS] = LUA_CELLSTACKSIZE;
  }
  /* seventh, get _origG on stack; for whatever reasons, luaL_openlibs() corrupts the stack so we cannot use it, even
     when additionally resetting the _LOADED and _READLIBBED tables. */
  lua_getfield(L, LUA_REGISTRYINDEX, "_origG");
  if (!lua_istable(L, -1)) {
    fprintf(stderr, "Warning in " LUA_QS  ": " LUA_QS " is missing or not a table, no restart possible.\n", "restart", "_origG");
    fflush(stderr);
    agn_poptop(L);
    return 0;
  }
  /* set all values in _origG into environment */
  lua_pushnil(L);
  while (lua_next(L, -2) != 0) {
    lua_pushvalue(L, -2); /* key */
    lua_pushvalue(L, -2); /* value */
    lua_setfield(L, LUA_GLOBALSINDEX, lua_tostring(L, -2));  /* set key in Agena environment */
    agn_poptoptwo(L);  /* delete copied key and value */
  }
  agn_poptop(L);  /* pop _origG from stack, 0.26.0 patch */
  lua_pushvalue(L, LUA_GLOBALSINDEX);
  lua_setglobal(L, "_G");
  if (resetlibname) agnL_setLibname(L, 0, agn_getdebug(L), agn_getdebug(L));
  /* eighth, get libname */
  lua_getglobal(L, "libname");
  if (!lua_isstring(L, -1)) {  /* 1.7.4 */
    agn_poptop(L);
    luaL_error(L, "Error in " LUA_QS ": " LUA_QS " could not be determined.", "restart", "libname");
  } else
    path = lua_tostring(L, -1);
  if (path == NULL) {
    fprintf(stderr, "Warning in " LUA_QS ": " LUA_QS " could not be determined, initialisation\nunsuccessful.\n",
      "restart", "libname");
    fflush(stderr);
  }
  else {
    agnL_initialise(L, agn_getnoini(L), agn_getdebug(L), agn_getnomainlib(L), agn_getskipagenapath(L));  /* 0.24.0, 2.8.6 */
  }
  agn_poptop(L);  /* drop libname */
  /* get mainlibname (assigned through library.agn at start-up), 0.28.2 */
  if (!agn_getnomainlib(L)) {  /* 2.8.6 */
    lua_getglobal(L, "mainlibname");
    if (!lua_isstring(L, -1))  /* 1.7.4 */
      luaL_error(L, "Error in " LUA_QS ": " LUA_QS " could not be determined.", "restart", "mainlibname");  /* 1.9.4 fix */
    else
      path = lua_tostring(L, -1);
    if (path == NULL) {
      fprintf(stderr, "Warning in " LUA_QS ": " LUA_QS " could not be determined.\n", "restart", "mainlibname");
      fflush(stderr);
    }
    agn_poptop(L);  /* drop mainlibname */
  }
  /* ninth, restore environ.onexit, 2.7.0 */
  lua_getglobal(L, "environ");
  if (lua_istable(L, -1)) {
    lua_getglobal(L, "_environonexit");
    lua_setfield(L, -2, "onexit");
  } else {
    agn_poptop(L);  /* pop whatever */
    luaL_error(L, "Error in " LUA_QS ": " LUA_QS " table could not be determined.", "restart", "environ");
  }
  agn_poptop(L);  /* pop `environ' table from stack */
  lua_pushnil(L);  /* delete temporary _environonexit variable */
  lua_setglobal(L, "_environonexit");
  /* reset seeds for math.random, 2.10.1 */
  lua_getglobal(L, "math");
  if (lua_istable(L, -1)) {
    lua_getfield(L, -1, "randomseed");
    if (!lua_isfunction(L, -1))
      luaL_error(L, "Error in " LUA_QS ": " LUA_QS " math.randomseed is not available.", "restart", "environ");
    lua_pushnumber(L, AGN_RANDOM_MW);
    lua_pushnumber(L, AGN_RANDOM_MZ);
    lua_call(L, 2, 0);  /* do not push results on stack */
    agn_poptop(L);  /* pop `math` table */
  } else {
    agn_poptop(L);  /* pop whatever */
    luaL_error(L, "Error in " LUA_QS ": " LUA_QS " table could not be determined.", "restart", "environ");
  }
  /* tenth, finally, conduct a garbage collection once again */
  lua_gc(L, LUA_GCCOLLECT, 0);
  return 0;
}


static void aux_getobjutype (lua_State *L) {
  lua_pushstring(L, "utype");
  if (agn_getutype(L, 1)) {
    lua_rawset(L, -3);
  } else {
    lua_pushfail(L);
    lua_rawset(L, -3);
  }
}

/* http://www.lua.org/pil/17.html: `The weakness of a table is given by the field __mode of its metatable.` */
static void isweak (lua_State *L) {
  luaL_checkstack(L, 3, "not enough stack space");
  if (lua_getmetatable(L, 1) != 0) {
    lua_pushstring(L, "weak");  /* in Lua, the corresponding index is `__mode` */
    lua_getfield(L, -2, "__weak");
    if (lua_isstring(L, -1)) {
      lua_rawset(L, -4);
    } else {
      agn_poptop(L);  /* pop null */
      lua_pushfail(L);  /* push fail insead */
      lua_rawset(L, -3);
    }
    agn_poptop(L);  /* pop metatable */
  } else {
    lua_rawsetstringboolean(L, -1, "weak", -1);
  }
}

static int environ_attrib (lua_State *L) {  /* added 0.10.0 */
  switch (lua_type(L, 1)) {
    case LUA_TTABLE: {
      size_t a[12];
      lua_newtable(L);
      agn_tablestate(L, 1, a, 1);
      lua_rawsetstringnumber(L,  -1, "array_assigned", a[0]);
      lua_rawsetstringnumber(L,  -1, "hash_assigned", a[1]);
      lua_rawsetstringboolean(L, -1, "array_hasholes", a[2]);
      lua_rawsetstringnumber(L,  -1, "array_allocated", a[3]);
      lua_rawsetstringnumber(L,  -1, "hash_allocated", a[4]);  /* contrary to sets, no need to adjust if only one hash element is in the table */
      lua_rawsetstringnumber(L,  -1, "bytes", agn_getstructuresize(L, 1));
      lua_rawsetstringboolean(L, -1, "metatable", a[6]);  /* 2.3.0 RC 3 */
      lua_rawsetstringboolean(L, -1, "dummynode", a[7]);  /* 2.3.0 RC 4 */
      lua_rawsetstringnumber(L,  -1, "length", a[8]);     /* 2.3.0 RC 4 */
      lua_rawsetstringnumber(L,  -1, "lowest", a[9]);     /* 2.14.10 */
      lua_rawsetstringnumber(L,  -1, "highest", a[10]);   /* 2.14.10 */
      lua_rawsetstringboolean(L, -1, "readonly", a[11]);  /* 4.9.0 */
      aux_getobjutype(L);
      isweak(L);
      break;
    }
    case LUA_TSET: {
      size_t a[4];
      lua_newtable(L);
      agn_sstate(L, 1, a);
      lua_rawsetstringnumber(L,  -1, "hash_assigned", a[0]);
      lua_rawsetstringnumber(L,  -1, "hash_allocated", (a[0] == 1 && a[1] == 0) ? 1 : a[1]);  /* lsizenode is set to 0 even if there is exactly one element in a set; 2.33.4 change */
      lua_rawsetstringboolean(L, -1, "metatable", a[2]);  /* 2.3.0 RC 3 */
      lua_rawsetstringnumber(L,  -1, "bytes", agn_getstructuresize(L, 1));
      lua_rawsetstringboolean(L, -1, "readonly", a[3]);  /* 4.9.0 */
      aux_getobjutype(L);
      isweak(L);
      break;
    }
    case LUA_TSEQ: {  /* corrected 2.3.0 RC 3 */
      size_t a[4];
      lua_newtable(L);
      agn_seqstate(L, 1, a);
      lua_rawsetstringnumber(L,  -1, "size", a[0]);
      lua_rawsetstringnumber(L,  -1, "maxsize", a[1]);
      lua_rawsetstringboolean(L, -1, "metatable", a[2]);  /* 2.3.0 RC 3 */
      lua_rawsetstringnumber(L,  -1, "bytes", agn_getstructuresize(L, 1));
      lua_rawsetstringboolean(L, -1, "readonly", a[3]);  /* 4.9.0 */
      aux_getobjutype(L);
      isweak(L);
      break;
    }
    case LUA_TREG: {  /* 2.3.0 RC 3 */
      size_t a[4];
      lua_newtable(L);
      agn_regstate(L, 1, a);
      lua_rawsetstringnumber(L,  -1, "top", a[0]);
      lua_rawsetstringnumber(L,  -1, "size", a[1]);
      lua_rawsetstringboolean(L, -1, "metatable", a[2]);
      lua_rawsetstringnumber(L,  -1, "bytes", agn_getstructuresize(L, 1));
      lua_rawsetstringboolean(L, -1, "readonly", a[3]);  /* 4.9.0 */
      aux_getobjutype(L);  /* 2.7.0 */
      isweak(L);
      break;
    }
    case LUA_TPAIR: {
      size_t a[2];
      lua_newtable(L);
      agn_pairstate(L, 1, a);
      lua_rawsetstringboolean(L, -1, "metatable", a[0]);  /* 2.3.0 RC 3 */
      lua_rawsetstringnumber(L,  -1, "bytes", agn_getstructuresize(L, 1));
      lua_rawsetstringboolean(L, -1, "readonly", a[1]);  /* 4.9.0 */
      aux_getobjutype(L);
      isweak(L);
      break;
    }
    case LUA_TUSERDATA: {  /* 4.9.0 */
      size_t a[3];
      lua_newtable(L);
      agn_udstate(L, 1, a);
      lua_rawsetstringboolean(L, -1, "readonly", a[0]);
      lua_rawsetstringnumber(L,  -1, "bytes", a[1]);
      lua_rawsetstringnumber(L,  -1, "nuvalue", a[2]);
      aux_getobjutype(L);
      break;
    }
    case LUA_TFUNCTION: {
      lua_Debug ar;
      int a, b, isC, nups, hasstore, hasmeta;
      luaL_checkstack(L, 3, "not enough stack space");  /* 7.7.8 change */
      isC = agn_getfunctiontype(L, 1);
      a = agn_getrtablewritemode(L, 1);
      b = agn_getstructuresize(L, 1);
      nups = lua_nupvalues(L, 1);
      hasstore = agn_getstorage(L, 1);
      if (hasstore) agn_poptop(L);
      lua_arity(L, &ar);
      hasmeta = lua_getmetatable(L, 1);  /* 2.36.2: does procedure have a metatable ? */
      if (hasmeta) agn_poptop(L);
      lua_newtable(L);
      lua_pushstring(L, "rtableWritemode");
      /* returns the mode of a remember table:
         1 = true:  function has a remember table and RETURN statement can update the rtable,
         0 = false: has a remember table and RETURN statement canNOT update the rtable,
         2 = fail:  function has no remember table at all
         -1:        object is not a function) */
      agn_pushboolean(L, a == 2 ? -1 : a);
      lua_rawset(L, -3);
      lua_rawsetstringboolean(L, -1, "C", isC);
      /* true if `idx` is a C function, false if `idx` is an Agena function */
      lua_rawsetstringnumber(L, -1, "bytes", b);
      aux_getobjutype(L);  /* get user-defined type name, if present */
      if (!isC) {
        lua_rawsetstringnumber(L, -1, "arity", ar.arity);
        lua_rawsetstringboolean(L, -1, "varargs", ar.hasvararg);
      } else {
        lua_rawsetstringboolean(L, -1, "arity", -1);
        lua_rawsetstringboolean(L, -1, "varargs", -1);
      }
      lua_rawsetstringnumber(L, -1, "nupvals", nups);  /* 2.12.3 */
      lua_rawsetstringboolean(L, -1, "storage", hasstore);  /* 2.33.1 */
      lua_rawsetstringboolean(L, -1, "metatable", hasmeta);  /* 2.36.2 */
      isweak(L);  /* 7.7.8 fix */
      break;
    }
    default:
      luaL_error(L, "Error in " LUA_QS ": structure or procedure expected, got %s.", "environ.attrib",
        luaL_typename(L, 1));
  }
  return 1;
}


static int environ_frozen (lua_State *L) {  /* added 5.0.2 */
  lua_pushboolean(L, agn_freeze(L, 1, -1));
  return 1;
}


#define unknown(L)  luaL_error(L, "Error in " LUA_QS ": unknown setting " LUA_QS ".", "environ.kernel", lua_tostring(L, -1));

static void processoption (lua_State *L, void (*f)(lua_State *, int), int i, const char *option) {  /* 0.32.0 */
  agn_pairgeti(L, i, 2);  /* get right-hand side */
  if (lua_isboolean(L, -1)) {
    (*f)(L, lua_toboolean(L, -1));
    lua_remove(L, -2);  /* remove left-hand side, leave right-hand side (boolean) on stack for later return */
  } else {
    int type = lua_type(L, -1);
    agn_poptoptwo(L);  /* clear stack, 2.8.6 fix */
    luaL_error(L, "Error in " LUA_QS ": boolean for " LUA_QS " option expected, got %s.", "environ.kernel", option,
      lua_typename(L, type));
  }
}


static void processintegeroption (lua_State *L, void (*f)(lua_State *, int), int i, const char *option) {  /* 2.34.9 */
  agn_pairgeti(L, i, 2);  /* get right-hand side */
  if (lua_isnumber(L, -1)) {
    (*f)(L, lua_tointeger(L, -1));
    lua_remove(L, -2);  /* remove left-hand side, leave right-hand side (number) on stack for later return */
  } else {
    int type = lua_type(L, -1);
    agn_poptoptwo(L);
    luaL_error(L, "Error in " LUA_QS ": number for " LUA_QS " option expected, got %s.", "environ.kernel", option,
      lua_typename(L, type));
  }
}


static void processstringoption (lua_State *L, void (*f)(lua_State *, char *), int i, const char *option) {  /* 5.2.2 */
  agn_pairgeti(L, i, 2);  /* get right-hand side */
  if (agn_isstring(L, -1)) {
    (*f)(L, (char *)agn_tostring(L, -1));
    lua_remove(L, -2);  /* remove left-hand side, leave right-hand side (string) on stack for later return */
  } else {
    int type = lua_type(L, -1);
    agn_poptoptwo(L);
    luaL_error(L, "Error in " LUA_QS ": string for " LUA_QS " option expected, got %s.", "environ.kernel", option,
      lua_typename(L, type));
  }
}


/* leaves either a set of the names (i.e. table keys) of all loaded values or `undefined` on stack. */
static void aux_getloaded (lua_State *L) {
  lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");
  if (!lua_istable(L, -1)) {
    agn_poptop(L);
    lua_pushundefined(L);
  } else {
    agn_createset(L, 0);
    lua_pushnil(L);
    while (lua_next(L, -3)) {
      lua_pushvalue(L, -2);
      lua_srawset(L, -4);
      agn_poptop(L);  /* pop table value */
    }
  }
  lua_remove(L, -2);  /* remove _LOADED */
}


static void aux_getreadlibbed (lua_State *L) {
  lua_getfield(L, LUA_REGISTRYINDEX, "_READLIBBED");
  if (!lua_isset(L, -1)) {
    agn_poptop(L);
    lua_pushundefined(L);
  }
}


/* Written by FreakAnon, taken from: https://stackoverflow.com/questions/152016/detecting-cpu-architecture-compile-time; get current architecture, detectx
   nearly every [current] architecture; adapted. 2.27.0 */
static const char *getBuild () {
  #if defined(__x86_64__) || defined(_M_X64)
  return "x86_64";
  #elif defined(i386) || defined(__i386__) || defined(__i386) || defined(_M_IX86)
  return "x86_32";
  #elif defined(__ARM_ARCH_2__)
  return "ARM2";
  #elif defined(__ARM_ARCH_3__)
  return "ARM3";
  #elif defined(__ARM_ARCH_3M__)
  return "ARM3M";
  #elif defined(__ARM_ARCH_4T__) || defined(__TARGET_ARM_4T)
  return "ARM4T";
  #elif defined(__ARM_ARCH_5_)
  return "ARM5"
  #elif defined(__ARM_ARCH_5E_)
  return "ARM5E"
  #elif defined(__ARM_ARCH_6__)
  return "ARM6";
  #elif defined(__ARM_ARCH_6J__)
  return "ARM6J";
  #elif defined(__ARM_ARCH_6K__)
  return "ARM6K";
  #elif defined(__ARM_ARCH_6T2_)
  return "ARM6T2";
  #elif defined(__ARM_ARCH_6Z__)
  return "ARM6Z";
  #elif defined(__ARM_ARCH_6ZK__)
  return "ARM6ZK";
  #elif defined(__ARM_ARCH_7__)
  return "ARM7";
  #elif defined(__ARM_ARCH_7A__)
  return "ARM7A";
  #elif defined(__ARM_ARCH_7R__)
  return "ARM7R";
  #elif defined(__ARM_ARCH_7M__)
  return "ARM7M";
  #elif defined(__ARM_ARCH_7S__)
  return "ARM7S";
  #elif defined(__aarch64__) || defined(_M_ARM64)
  return "ARM64";
  #elif defined(__ARM)
  return "ARM";
  #elif defined(mips) || defined(__mips__) || defined(__mips)
  return "MIPS";
  #elif defined(__sh__)
  return "SUPERH";
  #elif defined(__PPCCPU)
  return "POWERPC";
  #elif defined(__PPC64__) || defined(__ppc64__) || defined(_ARCH_PPC64)
  return "POWERPC64";
  #elif defined(__sparc__) || defined(__sparc)
  return "SPARC";
  #elif defined(__m68k__)
  return "M68K";
  #else
  return "UNKNOWN";
  #endif
}


/* See: https://stackoverflow.com/questions/1898153/how-to-determine-if-memory-is-aligned */
#define is_aligned(POINTER, BYTE_COUNT) \
    (((uintptr_t)(const void *)(POINTER)) % (BYTE_COUNT) == 0)

static int environ_kernel (lua_State *L) {  /* 0.27.0 */
  int i, nargs;
  const char *setting;
  ptrdiff_t *ptr = NULL;
  nargs = lua_gettop(L);
  if (nargs == 0) {  /* get all modes, 2.6.1 */
    /* general settings */
    lua_createtable(L, 0, 43);
    lua_rawsetstringboolean(L, -1, "constants", agn_getconstants(L));  /* 2.20.0 */
    lua_rawsetstringboolean(L, -1, "constanttoobig", agn_getconstanttoobig(L));  /* 2.39.4 */
    lua_rawsetstringboolean(L, -1, "debug", agn_getdebug(L));
    lua_rawsetstringboolean(L, -1, "duplicates", agn_getduplicates(L));  /* 2.20.1 */
    lua_rawsetstringboolean(L, -1, "emptyline", agn_getemptyline(L));
    lua_rawsetstringboolean(L, -1, "enclose", agn_getenclose(L));  /* 3.10.2 */
    lua_rawsetstringboolean(L, -1, "encloseback", agn_getenclosebackquotes(L));  /* 3.10.2 */
    lua_rawsetstringboolean(L, -1, "enclosedouble", agn_getenclosedouble(L));  /* 3.10.2 */
    lua_rawsetstringboolean(L, -1, "foradjust", agn_getforadjust(L));
    lua_rawsetstringboolean(L, -1, "gui", agn_getgui(L));
    lua_rawsetstringboolean(L, -1, "history", agn_gethistory(L));  /* 5.2.2 */
    lua_rawsetstringboolean(L, -1, "iso8601", agn_getiso8601(L));
    lua_rawsetstringboolean(L, -1, "kahanbabuska", agn_getkahanbabuska(L));
    lua_rawsetstringboolean(L, -1, "kahanozawa", agn_getkahanozawa(L));
    lua_rawsetstringboolean(L, -1, "libnamereset", agn_getlibnamereset(L));
    lua_rawsetstringboolean(L, -1, "longtable", agn_getlongtable(L));
    lua_rawsetstringboolean(L, -1, "promptnewline", agn_getpromptnewline(L));
    lua_rawsetstringboolean(L, -1, "seqautoshrink", agn_getseqautoshrink(L));
    lua_rawsetstringboolean(L, -1, "signedbits", agn_getbitwise(L));
    lua_rawsetstringboolean(L, -1, "skipagenapath", agn_getskipagenapath(L));  /* 2.35.4 */
    lua_rawsetstringboolean(L, -1, "skipinis", agn_getnoini(L));
    lua_rawsetstringboolean(L, -1, "skipmainlib", agn_getnomainlib(L));
    lua_rawsetstringboolean(L, -1, "warnings", lua_getwarnf(L));  /* 2.21.1 */
    lua_rawsetstringboolean(L, -1, "zeroedcomplex", agn_getzeroedcomplex(L));
    lua_rawsetstringboolean(L, -1, "nulliszero", agn_getnulliszero(L));  /* 7.7.6 */
    lua_rawsetstringnumber( L, -1, "buffersize", agn_getbuffersize(L));
    lua_rawsetstringnumber( L, -1, "clockspersec", CLOCKS_PER_SEC);  /* 2.12.4 */
    lua_rawsetstringnumber( L, -1, "digits", agn_getdigits(L));
    lua_rawsetstringnumber( L, -1, "doubleeps", agn_getdblepsilon(L));  /* 2.21.8 */
    lua_rawsetstringnumber( L, -1, "eps", agn_getepsilon(L));
    lua_rawsetstringnumber( L, -1, "errmlinebreak", agn_geterrmlinebreak(L));  /* 2.11.4 */
    lua_rawsetstringnumber( L, -1, "hEps", agn_gethepsilon(L));  /* 2.31.1 */
    lua_rawsetstringnumber( L, -1, "minstack", LUA_MINSTACK);  /* 3.6.2 */
    lua_rawsetstringnumber( L, -1, "pathmax", PATH_MAX);  /* 2.39.13 */
    lua_rawsetstringnumber( L, -1, "regsize", agn_getregsize(L));
    lua_rawsetstringnumber( L, -1, "rthits", agn_getrthits(L));
    lua_rawsetstringnumber( L, -1, "rtmisses", agn_getrtmisses(L));
    lua_rawsetstringstring( L, -1, "builtoncpu", getBuild());  /* 2.27.0 */
    lua_rawsetstringstring( L, -1, "histfile", agn_gethistfile(L));  /* 5.2.2 */
    lua_rawsetstringstring( L, -1, "pathsep", LUA_PATHSEP);
#ifndef __DJGPP__
    agn_getround(L);
    lua_setfield(L, -2, "rounding");
#endif
    /* insert readlibbed set */
    lua_pushstring(L, "readlibbed");
    aux_getreadlibbed(L);
    lua_rawset(L, -3);
    /* push tables.indices(_LOADED) */
    lua_pushstring(L, "loaded");
    aux_getloaded(L);
    lua_rawset(L, -3);  /* insert 'loaded' ~ set into result */
    /* end of general settings */

    /* datatypes */
    lua_pushstring(L, "types");
    lua_createtable(L, 0, 9);
    lua_rawsetstringnumber( L, -1, "nbits", LUA_NBITS);  /* 2.25.0 RC 3 */
    lua_rawsetstringnumber( L, -1, "nbits64", LUA_NBITS64);  /* 2.25.0 RC 3 */
    lua_rawsetstringnumber( L, -1, "nbytesulong", sizeof(unsigned long));  /* 2.25.5 */
    lua_rawsetstringnumber( L, -1, "bitsint", LUAI_BITSINT);  /* 2.25.0 RC 3 */
    lua_rawsetstringnumber( L, -1, "longmantdigs", LDBL_MANT_DIG);  /* 3.7.2 */
    lua_rawsetstringnumber( L, -1, "longmaxexp", LDBL_MAX_EXP);  /* 3.7.2 */
    lua_rawset(L, -3);  /* register */
    /* end of datatypes */
    /* alignment */
    lua_pushstring(L, "alignment");
    lua_createtable(L, 0, 3);
#if defined(IS32BIT)  /* changed to runtime query 2.27.4 */
    lua_rawsetstringboolean(L, -1, "alignable", is_aligned(ptr, sizeof(uint32_t)));  /* __GNUC__ && 32bits && !__APPLE, 2.25.5 */
#else
    lua_rawsetstringboolean(L, -1, "alignable", is_aligned(ptr, sizeof(uint64_t)));
#endif
#if defined(IS32BITALIGNED)  /* setting of IS32BITALIGNED, see prepdefs.h, 2.28.6 */
    lua_rawsetstringboolean(L, -1, "is32bitaligned", 1);
#else
    lua_rawsetstringboolean(L, -1, "is32bitaligned", 0);
#endif
    lua_rawsetstringnumber( L, -1, "blocksize", AGN_BLOCKSIZE);  /* 2.25.5 */
    lua_rawset(L, -3);
    /* end of alignment */
    /* numeric limits */
    lua_pushstring(L, "limits");
    lua_createtable(L, 0, 9);
    lua_rawsetstringnumber( L, -1, "closetozero", agn_getclosetozero(L));  /* 2.32.0 */
    lua_rawsetstringnumber( L, -1, "smallestnormal", sun_pow(2, -1022, 0));  /* 2.11.3, 2.14.13 change from pow to sun_pow */
    lua_rawsetstringnumber( L, -1, "lastcontint", AGN_LASTCONTINT);  /* 2.11.3 */
    lua_rawsetstringnumber( L, -1, "minlong", LUAI_MININT32);  /* 2.11.3 */
    lua_rawsetstringnumber( L, -1, "maxlong", LUAI_MAXINT32);  /* 2.11.3 */
    lua_rawsetstringnumber( L, -1, "maxulong", ULONG_MAX);     /* 2.11.3 */
    lua_rawsetstringnumber( L, -1, "maxinteger", LUA_MAXINTEGER);  /* 2.27.6/2.39.13 */
    lua_rawsetstringnumber( L, -1, "maxdouble", DBL_MAX);  /* 6.6.12 */
    lua_rawsetstringnumber( L, -1, "mindouble", DBL_MIN);  /* 6.6.12 */
    lua_rawset(L, -3);
    /* end of limits */
    /* GCC compilation settings */
    lua_pushstring(L, "gcc");
    lua_createtable(L, 0, 21);
#if defined(__linux__) && defined(__GNUC__)
    lua_rawsetstringnumber( L, -1, "glibc", __GLIBC__);  /* 2.25.5 */
    lua_rawsetstringnumber( L, -1, "glibcminor", __GLIBC_MINOR__);  /* 2.25.5 */
#endif
#if (defined(LUA_USE_POPEN) || defined(_WIN32))
    lua_rawsetstringboolean( L, -1, "popen", 1);  /* 4.7.4, UNDOC */
#else
    lua_rawsetstringboolean( L, -1, "popen", 0);
#endif
#ifdef LUA_USE_POSIX
    lua_rawsetstringboolean( L, -1, "posix", 1);  /* 4.7.4, UNDOC */
#else
    lua_rawsetstringboolean( L, -1, "posix", 0);
#endif
#ifdef LUA_USE_ISATTY
    lua_rawsetstringboolean( L, -1, "isatty", 1);  /* 4.7.4, UNDOC */
#else
    lua_rawsetstringboolean( L, -1, "isatty", 0);
#endif
#ifdef LUA_USE_MKSTEMP
    lua_rawsetstringboolean( L, -1, "mkstemp", 1); /* 4.7.4, UNDOC */
#else
    lua_rawsetstringboolean( L, -1, "mkstemp", 0);
#endif
#ifdef LUA_USE_LINUX
    lua_rawsetstringboolean( L, -1, "luauselinux", 1);  /* 4.7.4, UNDOC */
#else
    lua_rawsetstringboolean( L, -1, "luauselinux", 0);
#endif
#ifdef LUA_ANSI
    lua_rawsetstringboolean( L, -1, "ansi", 1);  /* 4.7.4, UNDOC */
#else
    lua_rawsetstringboolean( L, -1, "ansi", 0);
#endif
#ifdef PROPCMPLX
    lua_rawsetstringboolean( L, -1, "propcmplx", 1);  /* 4.7.4, UNDOC */
#else
    lua_rawsetstringboolean( L, -1, "propcmplx", 0);
#endif
#ifdef USE_TM64
    lua_rawsetstringboolean( L, -1, "y2038usetm64", 1);  /* 7.8.4, UNDOC */
#else
    lua_rawsetstringboolean( L, -1, "y2038usetm64", 0);
#endif
#ifdef HAS_TM_TM_GMTOFF
    lua_rawsetstringboolean( L, -1, "y2038hastmtmgmtoff", 1);  /* 7.8.4, UNDOC */
#else
    lua_rawsetstringboolean( L, -1, "y2038hastmtmgmtoff", 0);
#endif
#ifdef HAS_TM_TM_ZONE
    lua_rawsetstringboolean( L, -1, "y2038hastmtmzone", 1);  /* 7.8.4, UNDOC */
#else
    lua_rawsetstringboolean( L, -1, "y2038hastmtmzone", 0);
#endif
    lua_rawsetstringboolean(L, -1, "sse2", SSE2COMP);  /* 6.6.12 */
#ifdef IS32BIT
    lua_rawsetstringboolean(L, -1, "is32bit", 1);  /* 2.25.5 */
    lua_rawsetstringboolean(L, -1, "is64bit", 0);  /* 2.25.5 */
#elif defined(IS64BIT)
    lua_rawsetstringboolean(L, -1, "is32bit", 0);  /* 2.25.5 */
    lua_rawsetstringboolean(L, -1, "is64bit", 1);  /* 2.25.5 */
#else
    lua_rawsetstringboolean(L, -1, "is32bit", 0);  /* 2.25.5 */
    lua_rawsetstringboolean(L, -1, "is64bit", 0);  /* 2.25.5 */
#endif
#ifdef __SOLARIS
    lua_rawsetstringboolean(L, -1, "isSolaris", 1);  /* 2.27.0 */
#else
    lua_rawsetstringboolean(L, -1, "isSolaris", 0);  /* 2.27.0 */
#endif
#ifdef __linux__
    lua_rawsetstringboolean(L, -1, "isLinux", 1);  /* 2.27.0 */
#else
    lua_rawsetstringboolean(L, -1, "isLinux", 0);  /* 2.27.0 */
#endif
#ifdef __OS2__
    lua_rawsetstringboolean(L, -1, "isOS/2", 1);  /* 2.27.0 */
#else
    lua_rawsetstringboolean(L, -1, "isOS/2", 0);  /* 2.27.0 */
#endif
#ifdef __DJGPP__
    lua_rawsetstringboolean(L, -1, "isDOS", 1);  /* 2.27.4 */
#else
    lua_rawsetstringboolean(L, -1, "isDOS", 0);  /* 2.27.4 */
#endif
#ifdef _WIN32
    lua_rawsetstringboolean(L, -1, "isWindows", 1);  /* 2.27.0 */
#else
    lua_rawsetstringboolean(L, -1, "isWindows", 0);  /* 2.27.0 */
#endif
#ifdef __APPLE__
    lua_rawsetstringboolean(L, -1, "isMac", 1);  /* 2.27.4 */
#else
    lua_rawsetstringboolean(L, -1, "isMac", 0);  /* 2.27.4 */
#endif
#ifdef __INTEL
    lua_rawsetstringboolean(L, -1, "isIntel", 1);  /* 2.27.0 */
#else
    lua_rawsetstringboolean(L, -1, "isIntel", 0);  /* 2.27.0 */
#endif
#ifdef __ARMCPU
    lua_rawsetstringboolean(L, -1, "isARM", 1);  /* 2.27.4 */
#else
    lua_rawsetstringboolean(L, -1, "isARM", 0);  /* 2.27.4 */
#endif
    lua_rawset(L, -3);
    /* end of GCC compilation settings */
    nargs = 1;
  } else {  /* set modes */
    for (i=1; i <= nargs; i++) {
      if (lua_ispair(L, i)) {
        agn_pairgeti(L, i, 1);  /* get left-hand side */
        if (lua_type(L, -1) != LUA_TSTRING) {
          int type = lua_type(L, -1);
          agn_poptop(L);  /* clear stack */
          luaL_error(L, "Error in " LUA_QS ": string expected for left-hand side, got %s.", "environ.kernel",
            lua_typename(L, type));
        }
        setting = lua_tostring(L, -1);
        if (tools_streq(setting, "seqautoshrink")) {
          processoption(L, agn_setseqautoshrink, i, "seqautoshrink");
        } else if (tools_streq(setting, "signedbits")) {
          processoption(L, agn_setbitwise, i, "signedbits");
        } else if (tools_streq(setting, "foradjust")) {
          processoption(L, agn_setforadjust, i, "foradjust");
        } else if (tools_streq(setting, "kahanozawa")) {
          processoption(L, agn_setkahanozawa, i, "kahanozawa");
        } else if (tools_streq(setting, "kahanbabuska")) {
          processoption(L, agn_setkahanbabuska, i, "kahanbabuska");
        } else if (tools_streq(setting, "emptyline")) {
          processoption(L, agn_setemptyline, i, "emptyline");
        } else if (tools_streq(setting, "libnamereset")) {  /* 0.32.0 */
          processoption(L, agn_setlibnamereset, i, "libnamereset");
        } else if (tools_streq(setting, "longtable")) {  /* 0.32.0 */
          processoption(L, agn_setlongtable, i, "longtable");
        } else if (tools_streq(setting, "debug")) {  /* 0.32.2a */
          processoption(L, agn_setdebug, i, "debug");
        } else if (tools_streq(setting, "skipinis")) {  /* 2.8.6 */
          processoption(L, agn_setnoini, i, "skipinis");
        } else if (tools_streq(setting, "skipmainlib")) {  /* 2.8.6 */
          processoption(L, agn_setnomainlib, i, "skipmainlib");
        } else if (tools_streq(setting, "skipagenapath")) {  /* 2.35.4 */
          processoption(L, agn_setskipagenapath, i, "skipagenapath");
        } else if (tools_streq(setting, "gui")) {  /* 0.32.2a */
          processoption(L, agn_setgui, i, "gui");
        } else if (tools_streq(setting, "zeroedcomplex")) {  /* 1.7.6 */
          processoption(L, agn_setzeroedcomplex, i, "zeroedcomplex");
        } else if (tools_streq(setting, "promptnewline")) {  /* 1.7.6 */
          processoption(L, agn_setpromptnewline, i, "promptnewline");
        } else if (tools_streq(setting, "constanttoobig")) {  /* 2.39.4 */
          processoption(L, agn_setconstanttoobig, i, "constanttoobig");
        } else if (tools_streq(setting, "enclose")) {  /* 3.10.2 */
          processoption(L, agn_setenclose, i, "enclose");
        } else if (tools_streq(setting, "enclosedouble")) {  /* 3.10.2 */
          processoption(L, agn_setenclosedouble, i, "enclosedouble");
        } else if (tools_streq(setting, "encloseback")) {  /* 3.10.2 */
          processoption(L, agn_setenclosebackquotes, i, "encloseback");
        } else if (tools_streq(setting, "history")) {  /* 5.2.2 */
          processoption(L, agn_sethistory, i, "history");
        } else if (tools_streq(setting, "nulliszero")) {  /* 7.7.6 */
          processoption(L, agn_setnulliszero, i, "nulliszero");
        } else if (tools_streq(setting, "histfile")) {  /* 5.2.2 */
          processstringoption(L, agn_sethistfile, i, "histfile");
        } else if (tools_streq(setting, "digits")) {
          agn_pairgeti(L, i, 2);  /* get right-hand side */
          if (agn_isnumber(L, -1)) {
            lua_Number x = agn_tonumber(L, -1);
            agn_setdigits(L, x);
            lua_remove(L, -2);  /* remove left-hand side, leave right-hand side (number) on stack */
          } else {
            int type = lua_type(L, -1);
            agn_poptoptwo(L);  /* clear stack, 2.8.6 fix */
            luaL_error(L, "Error in " LUA_QS ": number for `digits` option expected,\ngot %s.", "environ.kernel",
              lua_typename(L, type));
          }
        } else if (tools_streq(setting, "errmlinebreak")) {  /* 2.11.4 */
          agn_pairgeti(L, i, 2);  /* get right-hand side */
          if (agn_isnumber(L, -1)) {
            lua_Number x = agn_tonumber(L, -1);
            agn_seterrmlinebreak(L, x);
            lua_remove(L, -2);  /* remove left-hand side, leave right-hand side (number) on stack */
          } else {
            int type = lua_type(L, -1);
            agn_poptoptwo(L);  /* clear stack, 2.8.6 fix */
            luaL_error(L, "Error in " LUA_QS ": integer for `errmlinebreak` option expected,\ngot %s.", "environ.kernel",
              lua_typename(L, type));
          }
        } else if (tools_streq(setting, "regsize")) {  /* 2.3.0 RC 3 */
          agn_pairgeti(L, i, 2);  /* get right-hand side */
          if (agn_isnumber(L, -1)) {
            lua_Number x = agn_tonumber(L, -1);
            agn_setregsize(L, x);
            lua_remove(L, -2);  /* remove left-hand side, leave right-hand side (number) on stack */
          } else {
            int type = lua_type(L, -1);
            agn_poptoptwo(L);  /* clear stack, 2.8.6 fix */
            luaL_error(L, "Error in " LUA_QS ": number for `regsize` option expected,\ngot %s.", "environ.kernel",
              lua_typename(L, type));
          }
        } else if (tools_streq(setting, "eps") || tools_streq(setting, "Eps")) {  /* 2.1.4 */
          agn_pairgeti(L, i, 2);  /* get right-hand side */
          if (agn_isnumber(L, -1)) {
            lua_Number x = agn_tonumber(L, -1);
            if (x < 0)
              luaL_error(L, "Error in " LUA_QS ": non-negative number for `eps` option expected,\ngot %f.", "environ.kernel", x);
            agn_setepsilon(L, x);
            lua_remove(L, -2);  /* remove left-hand side, leave right-hand side (number) on stack */
          } else {
            int type = lua_type(L, -1);
            agn_poptoptwo(L);  /* clear stack, 2.8.6 fix */
            luaL_error(L, "Error in " LUA_QS ": non-negative number for `eps` option expected,\ngot %s.", "environ.kernel",
              lua_typename(L, type));
          }
        } else if (tools_streq(setting, "doubleeps") || tools_streq(setting, "DoubleEps")) {  /* 2.21.8 */
          agn_pairgeti(L, i, 2);  /* get right-hand side */
          if (agn_isnumber(L, -1)) {
            lua_Number x = agn_tonumber(L, -1);
            if (x < 0)
              luaL_error(L, "Error in " LUA_QS ": non-negative number for `doubleeps` option expected,\ngot %f.", "environ.kernel", x);
            agn_setdblepsilon(L, x);
            lua_remove(L, -2);  /* remove left-hand side, leave right-hand side (number) on stack */
          } else {
            int type = lua_type(L, -1);
            agn_poptoptwo(L);  /* clear stack, 2.8.6 fix */
            luaL_error(L, "Error in " LUA_QS ": non-negative number for `doubleeps` option expected,\ngot %s.", "environ.kernel",
              lua_typename(L, type));
          }
        } else if (tools_streq(setting, "heps") || tools_streq(setting, "hEps")) {  /* 2.31.1 */
          agn_pairgeti(L, i, 2);  /* get right-hand side */
          if (agn_isnumber(L, -1)) {
            lua_Number x = agn_tonumber(L, -1);
            if (x < 0)
              luaL_error(L, "Error in " LUA_QS ": non-negative number for `heps` option expected,\ngot %f.", "environ.kernel", x);
            agn_sethepsilon(L, x);
            lua_remove(L, -2);  /* remove left-hand side, leave right-hand side (number) on stack */
          } else {
            int type = lua_type(L, -1);
            agn_poptoptwo(L);  /* clear stack, 2.8.6 fix */
            luaL_error(L, "Error in " LUA_QS ": non-negative number for `heps` option expected,\ngot %s.", "environ.kernel",
              lua_typename(L, type));
          }
        } else if (tools_streq(setting, "closetozero")) {  /* 2.32.0 */
          agn_pairgeti(L, i, 2);  /* get right-hand side */
          if (agn_isnumber(L, -1)) {
            lua_Number x = agn_tonumber(L, -1);
            if (x <= 0)
              luaL_error(L, "Error in " LUA_QS ": positive number for `closetozero` option expected,\ngot %f.", "environ.kernel", x);
            agn_setclosetozero(L, x);
            lua_remove(L, -2);  /* remove left-hand side, leave right-hand side (number) on stack */
          } else {
            int type = lua_type(L, -1);
            agn_poptoptwo(L);  /* clear stack, 2.8.6 fix */
            luaL_error(L, "Error in " LUA_QS ": positive number for `closetozero` option expected,\ngot %s.", "environ.kernel",
              lua_typename(L, type));
          }
        }
        else if (tools_streq(setting, "buffersize"))  /* 2.2.0 */
          processintegeroption(L, agn_setbuffersize, i, "buffersize");  /* 2.34.9 fix */
        else if (tools_streq(setting, "constants"))   /* 2.20.0 */
          processoption(L, agn_setconstants, i, "constants");
        else if (tools_streq(setting, "duplicates"))  /* 2.20.1 */
          processoption(L, agn_setduplicates, i, "duplicates");
        else if (tools_streq(setting, "warnings")) { /* 2.21.1 */
          agn_pairgeti(L, i, 2);  /* get right-hand side */
          if (lua_isboolean(L, -1)) {
            int mode = lua_toboolean(L, -1) > 0;  /* -1: fail, 0: false, 1: true */
            lua_warning(L, (mode) ? "@on" : "@off", 0);
            lua_remove(L, -2);  /* remove left-hand side, leave right-hand side (number) on stack */
          } else {
            agn_poptoptwo(L);
            luaL_error(L, "Error in " LUA_QS ": right-hand side of `warnings` must be a Boolean.", "environ.kernel");
          }
        }
#ifndef __DJGPP__
        else if (tools_streq(setting, "rounding")) {  /* 2.8.6 */
          agn_pairgeti(L, i, 2);  /* get right-hand side */
          if (agn_isstring(L, -1)) {
            const char *what = lua_tostring(L, -1);
            if (!agn_setround(L, what)) {
              agn_poptoptwo(L);
              luaL_error(L, "Error in " LUA_QS ": invalid setting `%s` for `rounding` option,\nor other error.", "environ.kernel", what);
            }
            lua_remove(L, -2);  /* remove left-hand side, leave right-hand side (string) on stack */
          } else {
            int type = lua_type(L, -1);
            agn_poptoptwo(L);  /* clear stack */
            luaL_error(L, "Error in " LUA_QS ": string for `rounding` option expected,\ngot %s.", "environ.kernel",
              lua_typename(L, type));
          }
        }
#endif
        else if (tools_streq(setting, "iso8601")) {  /* 2.9.8 */
          processoption(L, agn_setiso8601, i, "iso8601");
        } else
          unknown(L);
      } else if (agn_isstring(L, i)) {  /* return individual setting only */
        setting = lua_tostring(L, i);
        if (tools_streq(setting, "seqautoshrink"))
          lua_pushboolean(L, agn_getseqautoshrink(L));
        else if (tools_streq(setting, "signedbits"))
          lua_pushboolean(L, agn_getbitwise(L));
        else if (tools_streq(setting, "emptyline"))
          lua_pushboolean(L, agn_getemptyline(L));
        else if (tools_streq(setting, "libnamereset"))  /* 0.32.0 */
          lua_pushboolean(L, agn_getlibnamereset(L));
        else if (tools_streq(setting, "longtable"))  /* 0.32.0 */
          lua_pushboolean(L, agn_getlongtable(L));
        else if (tools_streq(setting, "debug"))  /* 0.32.0 */
          lua_pushboolean(L, agn_getdebug(L));
        else if (tools_streq(setting, "gui"))  /* 0.33.3 */
          lua_pushboolean(L, agn_getgui(L));
        else if (tools_streq(setting, "zeroedcomplex"))  /* 1.7.6 */
          lua_pushboolean(L, agn_getzeroedcomplex(L));
        else if (tools_streq(setting, "promptnewline"))  /* 1.7.6 */
          lua_pushboolean(L, agn_getpromptnewline(L));
        else if (tools_streq(setting, "digits"))  /* modified 2.3.0 RC 3 */
          lua_pushinteger(L, agn_getdigits(L));
        else if (tools_streq(setting, "errmlinebreak"))  /* 2.11.4 */
          lua_pushinteger(L, agn_geterrmlinebreak(L));
        else if (tools_streq(setting, "eps") || tools_streq(setting, "Eps"))  /* 2.1.4 */
          lua_pushnumber(L, agn_getepsilon(L));
        else if (tools_streq(setting, "doubleeps") || tools_streq(setting, "DoubleEps"))  /* 2.21.8 */
          lua_pushnumber(L, agn_getdblepsilon(L));
        else if (tools_streq(setting, "heps") || tools_streq(setting, "hEps"))  /* 2.31.1 */
          lua_pushnumber(L, agn_gethepsilon(L));
        else if (tools_streq(setting, "closetozero"))  /* 2.32.0 */
          lua_pushnumber(L, agn_gethepsilon(L));
        else if (tools_streq(setting, "regsize"))  /* 2.3.0 RC 3 */
          lua_pushnumber(L, agn_getregsize(L));
        else if (tools_streq(setting, "iso8601"))  /* 2.9.8 */
          lua_pushboolean(L, agn_getiso8601(L));
        else if (tools_streq(setting, "foradjust"))  /* 2.31.0 */
          lua_pushboolean(L, agn_getforadjust(L));
        else if (tools_streq(setting, "kahanozawa"))  /* 2.2.0 */
          lua_pushboolean(L, agn_getkahanozawa(L));   /* 2.4.2 */
        else if (tools_streq(setting, "kahanbabuska"))  /* 2.30.5 */
          lua_pushboolean(L, agn_getkahanbabuska(L));
        else if (tools_streq(setting, "rthits"))  /* 7.5.4 */
          lua_pushinteger(L, agn_getrthits(L));
        else if (tools_streq(setting, "rtmisses"))  /* 7.5.4 */
          lua_pushinteger(L, agn_getrtmisses(L));
        else if (tools_streq(setting, "buffersize"))  /* 2.2.0 */
          lua_pushnumber(L, agn_getbuffersize(L));
#ifndef __DJGPP__
        else if (tools_streq(setting, "rounding"))  /* 2.8.6 */
          agn_getround(L);
#endif
        else if (tools_streq(setting, "skipinis"))  /* 2.8.6 */
          lua_pushboolean(L, agn_getnoini(L));
        else if (tools_streq(setting, "skipmainlib"))  /* 2.8.6 */
          lua_pushboolean(L, agn_getnomainlib(L));
        else if (tools_streq(setting, "skipagenapath"))  /* 2.35.4 */
          lua_pushboolean(L, agn_getskipagenapath(L));
        else if (tools_streq(setting, "readlibbed"))  /* 2.9.1 */
          aux_getreadlibbed(L);
        else if (tools_streq(setting, "loaded"))  /* 2.9.1 */
          aux_getloaded(L);
        else if (tools_streq(setting, "errmlinebreak"))  /* 2.11.4 */
          lua_pushinteger(L, agn_geterrmlinebreak(L));
        /* read-only settings */
        else if (tools_streq(setting, "maxinteger"))  /* 2.39.13 */
          lua_pushnumber(L, LUA_MAXINTEGER);
        else if (tools_streq(setting, "minlong"))  /* 2.1.9 */
          lua_pushnumber(L, LUAI_MININT32);
        else if (tools_streq(setting, "maxlong"))  /* 2.1.9 */
          lua_pushnumber(L, LUAI_MAXINT32);
        else if (tools_streq(setting, "maxulong"))  /* 2.3.1, XXX change agnconf.h if you change the definition of unsigned long int ! */
          lua_pushnumber(L, ULONG_MAX);
        else if (tools_streq(setting, "nbits"))  /* 2.25.0 RC 3 */
          lua_pushnumber(L, LUA_NBITS);
        else if (tools_streq(setting, "nbits64"))  /* 2.25.0 RC 3 */
          lua_pushnumber(L, LUA_NBITS64);
        else if (tools_streq(setting, "longmantdigs"))  /* 3.7.2 */
          lua_pushnumber(L, LDBL_MANT_DIG);
        else if (tools_streq(setting, "longmaxexp"))  /* 3.7.2 */
          lua_pushnumber(L, LDBL_MAX_EXP);
        else if (tools_streq(setting, "bitsint"))  /* 2.25.0 RC 3 */
          lua_pushnumber(L, LUAI_BITSINT);
        else if (tools_streq(setting, "blocksize"))  /* 2.25.5 */
          lua_pushnumber(L, AGN_BLOCKSIZE);
        else if (tools_streq(setting, "nbytesulong"))  /* 2.25.5 */
          lua_pushnumber(L, sizeof(unsigned long));
        else if (tools_streq(setting, "alignable"))  /* 2.25.5, GNUC && 32bits && !APPLE */
#ifdef IS32BITALIGNED
          lua_pushboolean(L, is_aligned(ptr, sizeof(uint32_t)));  /* changed to runtime query 2.27.4 */
#else
          lua_pushboolean(L, is_aligned(ptr, sizeof(uint64_t)));
#endif
        else if (tools_streq(setting, "is32bitaligned"))  /* 2.25.6, setting of compiler switch */
#ifdef IS32BITALIGNED
          lua_pushboolean(L, 1);
#else
          lua_pushboolean(L, 0);
#endif
        else if (tools_streq(setting, "is32bit"))  /* 2.25.5 */
#ifdef IS32BIT
          lua_pushtrue(L);
#else
          lua_pushfalse(L);
#endif
        else if (tools_streq(setting, "is64bit"))  /* 2.25.5 */
#ifdef IS64BIT
          lua_pushtrue(L);
#else
          lua_pushfalse(L);
#endif
        else if (tools_streq(setting, "isSolaris"))  /* 2.27.0 */
#ifdef __SOLARIS
          lua_pushtrue(L);
#else
          lua_pushfalse(L);
#endif
        else if (tools_streq(setting, "isLinux"))  /* 2.27.4 */
#ifdef __linux__
          lua_pushtrue(L);
#else
          lua_pushfalse(L);
#endif
        else if (tools_streq(setting, "isMac"))  /* 2.27.4 */
#ifdef __APPLE__
          lua_pushtrue(L);
#else
          lua_pushfalse(L);
#endif
        else if (tools_streqx(setting, "isOS/2", "isOS2", NULL))  /* 2.27.0 */
#ifdef __OS2__
          lua_pushtrue(L);
#else
          lua_pushfalse(L);
#endif
        else if (tools_streq(setting, "isDOS"))  /* 2.27.4 */
#ifdef __DJGPP__
          lua_pushtrue(L);
#else
          lua_pushfalse(L);
#endif
        else if (tools_streq(setting, "isWindows"))  /* 2.27.0 */
#ifdef _WIN32
          lua_pushtrue(L);
#else
          lua_pushfalse(L);
#endif
        else if (tools_streq(setting, "isIntel"))  /* 2.27.0 */
#ifdef __INTEL
          lua_pushtrue(L);
#else
          lua_pushfalse(L);
#endif
        else if (tools_streq(setting, "isARM"))  /* 2.27.4 */
#ifdef __ARMCPU
          lua_pushtrue(L);
#else
          lua_pushfalse(L);
#endif
        else if (tools_streq(setting, "builtoncpu"))  /* 2.27.0 */
          lua_pushstring(L, getBuild());
        else if (tools_streq(setting, "pathsep"))  /* 2.1.9 */
          lua_pushstring(L, LUA_PATHSEP);
        else if (tools_streq(setting, "pathmax"))  /* 2.9.7 */
          lua_pushinteger(L, PATH_MAX);
        else if (tools_streq(setting, "minstack"))  /* 3.6.2 */
          lua_pushinteger(L, LUA_MINSTACK);
        else if (tools_streq(setting, "lastcontint"))  /* 2.14.3 */
          lua_pushnumber(L, AGN_LASTCONTINT);
        else if (tools_streq(setting, "smallestnormal"))  /* 2.14.3 */
          lua_pushnumber(L, sun_pow(2, -1022, 0));
        else if (tools_streq(setting, "clockspersec"))  /* 2.14.3 */
          lua_pushnumber(L, CLOCKS_PER_SEC);
        else if (tools_streq(setting, "constants"))  /* 2.20.0 */
          lua_pushboolean(L, agn_getconstants(L));
        else if (tools_streq(setting, "constanttoobig"))  /* 2.39.4 */
          lua_pushboolean(L, agn_getconstanttoobig(L));
        else if (tools_streq(setting, "enclose"))  /* 3.10.2 */
          lua_pushboolean(L, agn_getenclose(L));
        else if (tools_streq(setting, "enclosedouble"))  /* 3.10.2 */
          lua_pushboolean(L, agn_getenclosedouble(L));
        else if (tools_streq(setting, "encloseback"))  /* 3.10.2 */
          lua_pushboolean(L, agn_getenclosebackquotes(L));
        else if (tools_streq(setting, "history"))  /* 5.2.2 */
          lua_pushboolean(L, agn_gethistory(L));
        else if (tools_streq(setting, "histfile"))  /* 5.2.2 */
          lua_pushstring(L, agn_gethistfile(L));
        else if (tools_streq(setting, "nulliszero"))  /* 7.7.6 */
          lua_pushboolean(L, agn_getnulliszero(L));
        else if (tools_streq(setting, "duplicates"))  /* 2.20.1 */
          lua_pushboolean(L, agn_getduplicates(L));
        else if (tools_streq(setting, "warnings"))  /* 2.21.1 */
          lua_pushinteger(L, lua_getwarnf(L));
        else
          unknown(L);
      } else
        luaL_error(L, "Error in " LUA_QS ": pair or string expected, got %s.", "environ.kernel",
          luaL_typename(L, i));
    }
  }
  return nargs;
}


static int environ_system (lua_State *L) {  /* 2.9.2 moved from debug package */
  luaL_checkstack(L, 3, "not enough stack space");  /* 4.7.1 fix */
  lua_createtable(L, 0, 5);
  /* create a table for C types */
  lua_pushstring(L, "Ctypes");
  lua_createtable(L, 0, 34);
  lua_rawsetstringnumber(L, -1, "char", CHARSIZE);
  lua_rawsetstringnumber(L, -1, "int", sizeof(int));
  lua_rawsetstringnumber(L, -1, "long", sizeof(long));
  lua_rawsetstringnumber(L, -1, "longlong", sizeof(long long int));
  lua_rawsetstringnumber(L, -1, "int32_t", sizeof(int32_t));
  lua_rawsetstringnumber(L, -1, "int64_t", sizeof(int64_t));  /* 2.9.0 */
  lua_rawsetstringnumber(L, -1, "uint16_t", sizeof(uint16_t));  /* 2.18.2 */
  lua_rawsetstringnumber(L, -1, "uint32_t", sizeof(uint32_t));  /* 2.10.4 */
  lua_rawsetstringnumber(L, -1, "uint64_t", sizeof(uint64_t));  /* 2.10.4 */
  lua_rawsetstringnumber(L, -1, "float", sizeof(float));
  lua_rawsetstringnumber(L, -1, "double", sizeof(double));
  lua_rawsetstringnumber(L, -1, "complexdouble", sizeof(agn_Complex));
  lua_rawsetstringnumber(L, -1, "longdouble", SIZEOFLDBL);  /* 2.35.0 */
  lua_rawsetstringnumber(L, -1, "bitschar", CHAR_BIT);  /* 2.12.4 */
  lua_rawsetstringnumber(L, -1, "bitsint", LUAI_BITSINT);  /* 2.12.4: moved from numberranges table */
  lua_rawsetstringnumber(L, -1, "luaint32", sizeof(LUA_INT32));  /* 2.12.4: moved from numberranges table */
  lua_rawsetstringnumber(L, -1, "luauint32", sizeof(LUA_UINT32));  /* 2.12.4: moved from numberranges table */
  lua_rawsetstringnumber(L, -1, "luaint64", sizeof(LUA_INT64));  /* 2.12.4: moved from numberranges table */
  lua_rawsetstringnumber(L, -1, "luauint64", sizeof(LUA_UINT64));  /* 2.12.4: moved from numberranges table */
  /* now set C types to main table */
  lua_rawset(L, -3);
  lua_pushstring(L, "Ltypes");
  lua_createtable(L, 0, 3);
  lua_rawsetstringnumber(L, -1, "Table", sizeof(Table));
  lua_rawsetstringnumber(L, -1, "TKey", sizeof(TKey));
  lua_rawsetstringnumber(L, -1, "Node", sizeof(Node));
  lua_rawsetstringnumber(L, -1, "UltraSet", sizeof(UltraSet));
  lua_rawsetstringnumber(L, -1, "Seq", sizeof(Seq));
  lua_rawsetstringnumber(L, -1, "Reg", sizeof(Reg));
  lua_rawsetstringnumber(L, -1, "Pair", sizeof(Pair));
  lua_rawsetstringnumber(L, -1, "TString", sizeof(TString));
  lua_rawsetstringnumber(L, -1, "Udata", sizeof(Udata));
  lua_rawsetstringnumber(L, -1, "Proto", sizeof(Proto));
  lua_rawsetstringnumber(L, -1, "UpVal", sizeof(UpVal));
  lua_rawsetstringnumber(L, -1, "Closure", sizeof(Closure));
  lua_rawsetstringnumber(L, -1, "CommonHeader", sizeof(GCObject) + 2*sizeof(lu_byte));
  /* now set Lua/Agena types to main table, 7.5.8 */
  lua_rawset(L, -3);
  /* minimum and maximum numeric values, 2.9.0 */
  lua_pushstring(L, "numberranges");
  lua_createtable(L, 0, 10);
#ifdef LLONG_MIN
  lua_rawsetstringnumber(L,  -1, "minlonglong", LLONG_MIN);
#endif
#ifdef LLONG_MAX
  lua_rawsetstringnumber(L,  -1, "maxlonglong", LLONG_MAX);
#endif
#ifdef ULLONG_MAX
  lua_rawsetstringnumber(L,  -1, "maxulonglong", ULLONG_MAX);
#endif
  lua_rawsetstringnumber(L,  -1, "minlong", LUAI_MININT32);
  lua_rawsetstringnumber(L,  -1, "maxlong", LUAI_MAXINT32);
  lua_rawsetstringnumber(L,  -1, "maxulong", ULONG_MAX);
  lua_rawsetstringnumber(L,  -1, "maxushort", USHRT_MAX);  /* 2.18.2 */
  lua_rawsetstringnumber(L,  -1, "maxdouble", DBL_MAX);
  lua_rawsetstringnumber(L,  -1, "mindouble", DBL_MIN);
  lua_rawsetstringnumber(L,  -1, "floatradix", FLT_RADIX);  /* 2.21.8 */
  /* 3.10.6, DBL_RADIX is missing on many platforms, so we determine it ourselves */
  lua_rawsetstringnumber(L,  -1, "doubleradix", tools_doublebase());
  /* now set values to main table */
  lua_rawset(L, -3);
  /* determine endianness */
  lua_pushstring(L, "endianness");
  switch (tools_endian()) {
    case 0: lua_pushstring(L, "little endian"); break;
    case 1: lua_pushstring(L, "big endian"); break;
    default: lua_pushstring(L, "unknown");
  }
  lua_rawset(L, -3);
  lua_pushstring(L, "OS");
  /* determine operating system */
#ifdef __DJGPP__
  lua_pushstring(L, "DOS");
#elif __linux__
  lua_pushstring(L, "Linux");
#elif defined(__SOLARIS)
  lua_pushstring(L, "Sun Solaris");
#elif __unix__
  lua_pushstring(L, "UNIX");
#elif defined(_WIN32)
  lua_pushstring(L, "Windows");
#elif __OS2__
  lua_pushstring(L, "OS/2");
#elif __BEOS__
  lua_pushstring(L, "BEOS");
#elif __APPLE__
  lua_pushstring(L, "Apple");
#else
  lua_pushstring(L, "unknown");
#endif
  lua_rawset(L, -3);
  /* determine hardware */
  lua_pushstring(L, "hardware");
#ifdef __i386
  lua_pushstring(L, "i386");
#elif __amd64
  lua_pushstring(L, "AMD64");
#elif __sparc
  lua_pushstring(L, "Sun Sparc");
#elif __MACTYPES__
  lua_pushstring(L, "Mac");
#else
  lua_pushstring(L, "unknown");
#endif
  lua_rawset(L, -3);
  return 1;
}


/* Returns the number of parameters of a function and additionally a Boolean indicating whether its parameter list includes a `?`
   (varargs) token or not. Also returns the number of upvalues as third result. */
static int environ_arity (lua_State *L) {  /* 2.12.0 RC 1, improved 2.12.1 */
  lua_Debug ar;
  int nups;
  if (!lua_isfunction(L, 1))
    luaL_error(L, "Error in " LUA_QS ": procedure expected, got %s.", "environ.arity", luaL_typename(L, 1));
  nups = lua_nupvalues(L, 1);  /* 2.12.3 */
  luaL_checkstack(L, 3, "not enough stack space");  /* 4.7.1 fix */
  if (lua_arity(L, &ar)) {
    lua_pushundefined(L);
    lua_pushundefined(L);
  } else {
    lua_pushinteger(L, ar.arity);
    lua_pushboolean(L, ar.hasvararg);
  }
  if (nups == -1) {
    lua_pushundefined(L);
  } else {
    lua_pushinteger(L, nups);
  }
  return 3;
}


/* Creates a unique integer reference for any argument `obj` and inserts `obj` into table `tbl` at position ref (i.e. tbl[ref] := obj).
   The function returns `ref`. Do not manually put any data into `tbl` or delete data, always use `environ.ref` and `environ.unref`
   to modify `tbl`.

   By default, `obj` is always inserted into `tbl`, even if it is already stored there.

   If the optional third argument is 'reference' or 'full', then a check is performed to ensure that obj has not already been
   included in `tbl`. If `obj` is already in `tbl`, it is not inserted again and the integer index of `obj` in `tbl` is simply returned.
   If `option` is 'reference', the function uses `environ.isequal` for the check, whereas with the option 'full', the standard
   `=` equality operator is used.

   See also: environ.ref. */
static int environ_ref (lua_State *L) {  /* 2.12.3 */
  int unique;
  const char *option;
  luaL_checkany(L, 2);
  luaL_argcheck(L, lua_istable(L, 1), 1, "table expected");
  option = agnL_optstring(L, 3, "none");
  if (tools_streq(option, "none"))
    unique = 0;
  else if (tools_streq(option, "reference"))
    unique = 1;
  else if (tools_streq(option, "full"))
    unique = 2;
  else {
    unique = -1;
    luaL_error(L, "Error in " LUA_QS ": unknown option %s.", "environ.ref", option);
  }
  lua_settop(L, 2);  /* drop option */
  switch (unique) {
    case 0: break;  /* no check at all, suited if multiple applications share the same table */
    case 1: /* compare with Lua's `==` operator: check by comparing pointer references whether element is already included */
      lua_pushnil(L);
      while (lua_next(L, 1)) {
        if (agn_equalref(L, 2, -1)) {  /* two different structures (except complex numbers) with the same contents are always not equal */
          agn_poptop(L);  /* pop value */
          return 1;       /* return index, do not insert atomic value (numbers, strings, booleans, complex numbers) multiple times */
        }
        agn_poptop(L);
      }
      break;
    case 2:  /* compare with standard `=` operator */
      lua_pushnil(L);
      while (lua_next(L, 1)) {
        if (lua_equal(L, 2, -1)) {  /* two different structures (except complex numbers) with the same contents are always not equal */
          agn_poptop(L);  /* pop value */
          return 1;       /* return index, do not insert atomic value (numbers, strings, booleans, complex numbers) multiple times */
        }
        agn_poptop(L);
      }
      break;
    default:
      lua_assert();  /* should not happen */
  }
  lua_pushinteger(L, luaL_ref(L, 1));  /* store reference to value */
  return 1;
}


/* With `tbl` a table and `ref` an integer, deletes value tbl[ref] and returns it. See also: `environ.ref`. */
static int environ_unref (lua_State *L) {  /* 2.12.3 */
  int ref;
  luaL_checkany(L, 2);
  luaL_argcheck(L, lua_istable(L, 1), 1, "table expected");
  ref = agn_checkinteger(L, 2);
  lua_rawgeti(L, 1, ref);
  luaL_unref(L, 1, ref);
  return 1;  /* return released object */
}


/* Compares two objects o1, o2 for equality and returns `true` or `false`. Note that two structures a and b of the same type, are
   always considered different if they do not reference one another. Thus, for example, with a := [1] and b := [1], the function
   returns `false`, whereas a and b with a := [1] and b := a are equal. */
static int environ_isequal (lua_State *L) {  /* 2.12.3 */
  luaL_checkany(L, 2);
  lua_pushboolean(L, agn_equalref(L, 1, 2));
  return 1;
}


/* Emits a warning with a message composed by the concatenation of all its arguments (which should be strings or numbers).

   By convention, a one-piece message starting with '@' is intended to be a control message, which is a message to the
   warning system itself. In particular, the standard warning function in Lua recognizes the control messages "@off",
   to stop the emission of warnings, and "@on", to (re)start the emission; it ignores unknown control messages.

   If called without arguments, returns a Boolean indicating whether the warning system is on or off, and the current
   warning state as an integer:

     0 - warning system is off;
     1 - ready to start a new message;
     2 - previous message is to be continued.
*/
/*
** Creates a warning with all given arguments.
** Check first for errors; otherwise an error may interrupt
** the composition of a warning, leaving it unfinished.
** Taken from Lua 5.4.0 RC 4, 2.21.1
*/
static int environ_warn (lua_State *L) {
  int n = lua_gettop(L);  /* number of arguments */
  int i;
  if (n == 0) {  /* extended 2.21.1 */
    int warnstate = lua_getwarnf(L);
    lua_pushboolean(L, warnstate != 0);
    lua_pushinteger(L, warnstate);
    return 2;
  }
  luaL_checkstring(L, 1);  /* at least one argument */
  for (i = 2; i <= n; i++)
    luaL_checkstring(L, i);  /* make sure all arguments are strings */
  for (i = 1; i < n; i++)  /* compose warning */
    lua_warning(L, lua_tostring(L, i), 1);
  lua_warning(L, lua_tostring(L, n), 0);  /* close warning */
  return 0;
}


/* Returns the decimal point separator used in the current locale. It is an alternative to the expression `os.getlocale.decimal_point`,
   but is faster. 2.22.0 */
static int environ_decpoint (lua_State *L) {
  lua_pushchar(L, lua_getlocaledecpoint());
  return 1;
}


/* Parse command-line options. 2.28.2. Taken from luaposix, see:
   https://github.com/luaposix/luaposix/blob/master/ext/posix/unistd.c
   https://linux.die.net/man/3/getopt

   luaposix is the work of several authors (see git history for contributors). It is based on two earlier libraries:

   An earlier version of luaposix (up to 5.1.11):
   Copyright Reuben Thomas <rrt@sc3d.org> 2010-2011
   Copyright Natanael Copa <natanael.copa@gmail.com> 2008-2010
   Clean up and bug fixes by Leo Razoumov <slonik.az@gmail.com> 2006-10-11
   Luiz Henrique de Figueiredo <lhf@tecgraf.puc-rio.br> 07 Apr 2006 23:17:49
   Based on original by Claudio Terra for Lua 3.x, with contributions by Roberto Ierusalimschy.

   The function parses command-line options passed from the underlying operating system to an Agena script.
   Each option (switch) may consist of exactly one letter, preceded by a dash or slash, multiletter switches are not supported
   and will be incorrectly processed. Examples:

   Valid:    agena script.agn -h
   Valid:    agena script.agn /h
   Valid:    agena script.agn -apx  (expanded to -a -p -x)
   Valid:    agena script.agn /apx  (expanded to /a /p /x)
   Valid:    agena script.agn -val 3.141592654
   Valid:    agena script.agn -val=3.141592654
   Invalid:  agena script.agn -help (would be split into the switches -h, -e, -l and -p.

   The function takes the `args' system table and a `format` string denoting the switches to detect and - if found -
   returns the switch name without a preceding dash or slash, an optional value if given, and the index of the next
   `args' entry to be processed in a subsequent call. If `args' is 'null', the function simply returns. Depending on their
   position in the call from the operating system, unknown options might be ignored.

   Example:

   > for switch, optarg, nextidx in environ.getopt(args, 'ab:c::d') do
   >    print(switch, optarg, nextidx)
   > end

   In this example, the format string 'ab:c:p' has the following meaning, and you can use combinations in any order:

   'a'   - check for just the /a or -a switch
   'b:'  - check for the /b or -b switch mandatorily succeeded by a value; the switch and the value may be separated by a blank
           or an equals sign ('=').
   'c::' - check for the /c or -c switch optionally succeeded by a value, both separated by a blank or an equals sign ('=').
   'd'   - check for just the /d or -d switch

   For an example script, check file getopt.agn in the share/scripting folder of your Agena installation.

   The function is a port to a modified version of the C library function getopt. */

#ifndef __OS2__
char *optarg;  /* unistd.h will import this value in OS/2 */
int optind = 1, opterr = 1, optopt, __optpos, __optreset = 0;
#else
int optind = 1, opterr = 1, __optpos, __optreset = 0;
#endif
#define optpos __optpos

/* weak_alias(__optreset, optreset); */

static int musl_getopt (int argc, char * const argv[], const char *optstring) {
  /* taken from musl-1.2.3/src/misc/getopts.c, extended to also support slashes */
  int i;
  wchar_t c, d;
  int k, l;
  if (!optind || __optreset) {
    __optreset = 0;
    __optpos = 0;
    optind = 1;
  }
  if (optind >= argc || !argv[optind]) return -1;
  if (argv[optind][0] != '-' && argv[optind][0] != '/') {
    if (optstring[0] == '-') {
      optarg = argv[optind++];
      return 1;
    }
    return -1;
  }
  if (!argv[optind][1]) return -1;
  if (argv[optind][1] == '-' && !argv[optind][2])
  return optind++, -1;
  else if (argv[optind][1] == '/' && !argv[optind][2])
  return optind++, -1;
  if (!optpos) optpos++;
  if ((k = mbtowc(&c, argv[optind] + optpos, MB_LEN_MAX)) < 0) {
    k = 1;
    c = 0xfffd; /* replacement char */
  }
  optpos += k;
  if (!argv[optind][optpos]) {
    optind++;
    optpos = 0;
  }
  if (optstring[0] == '-' || optstring[0] == '+') optstring++;
  i = 0;
  d = 0;
  do {
    l = mbtowc(&d, optstring + i, MB_LEN_MAX);
    if (l > 0) i += l; else i++;
  } while (l && d != c);
  if (d != c || c == ':') {
    optopt = c;
    return '?';
  }
  if (optstring[i] == ':') {
    optarg = 0;
    if (optstring[i + 1] != ':' || optpos) {
      optarg = argv[optind++] + optpos;
      optpos = 0;
    }
    if (optind > argc) {
      optopt = c;
      if (optstring[0] == ':') return ':';
      return '?';
    }
  }
  return c;
}

static int iter_getopt (lua_State *L) {
  int r, argc = lua_tointeger(L, lua_upvalueindex(1));
  char **argv = (char **)lua_touserdata(L, lua_upvalueindex(3));
  const char *oarg;
  char c;
  size_t l;
  opterr = 0;  /* 2.28.2 change to prevent error messages sent to stderr */
  if (argv == NULL) /* If we have already completed, return now. */
    return 0;
  /* Fetch upvalues to pass to getopt. */
  while ( (r = musl_getopt(argc, argv, lua_tostring(L, lua_upvalueindex(2))) ) == '?' );  /* 2.28.2 change to skip additional chars in option */
  /* r = getopt(argc, argv, lua_tostring(L, lua_upvalueindex(2))); */
  if (r == -1) return 0;
  c = (char)r;
  luaL_checkstack(L, 3, "not enough stack space");
  lua_pushlstring(L, &c, 1);   /* push first character of option */
  lua_pushstring(L, optarg);   /* push option value or if not given first option character */
  /* You can neither modify optarg nor strdup it, so in order to remove a leading '=', we run: */
  oarg = lua_tolstring(L, -1, &l);
  if (l > 1 && *oarg == '=') {
    oarg++; l--;
  }
  lua_pushlstring(L, oarg, l);
  lua_remove(L, -2);
  lua_pushinteger(L, optind);  /* index of the next element to be processed in argv */
  return 3;
}

static int environ_getopt (lua_State *L) {
  int argc, i;
  const char *optstring;
  char **argv;
  luaL_checknargs(L, 4);
  if (lua_isnoneornil(L, 1)) {
    lua_pushnil(L);
    return 1;
  }
  luaL_checktypex(L, 1, LUA_TTABLE, "list");
  optstring = luaL_checkstring(L, 2);
  opterr = luaL_optint(L, 3, 0);
  optind = luaL_optint(L, 4, 1);
  argc = (int)lua_objlen(L, 1) + 1;
  luaL_checkstack(L, 3 + argc, "not enough stack space");  /* 3.18.4 fix */
  lua_pushinteger(L, argc);
  lua_pushstring(L, optstring);
  argv = lua_newuserdata(L, (argc + 1)*sizeof(char *));
  argv[argc] = NULL;
  for (i = 0; i < argc; i++) {
    lua_pushinteger(L, i);
    lua_gettable(L, 1);
    argv[i] = (char *)luaL_checkstring(L, -1);
  }
  /* Push remaining upvalues, and make and push closure. */
  lua_pushcclosure(L, iter_getopt, 3 + argc);
  return 1;
}


static int environ_arithstate (lua_State *L) {  /* 2.32.0 */
  lua_pushinteger(L, L->arithstate);
  return 1;
}


/* Returns x if x is a function. If x is a structure and has a '__call' metamethod, returns this metamethod (see
   Chapter 6.19 of the Primer & Reference). Otherwise returns nothing which is equivalent to `null` if tested.
   All this is equivalent to:

   callable := proc(x) is
      if x :- procedure then
         x := (getmetatable(x) or []).__call
      end;
      if x :: procedure then
         return x
      end
   end;

   The function is useful to first check whether a value f is callable like a function before actually running it:

   r := environ.callable(f) and f(...);

   Idea has been taken from Gary V. Vaughan's lyaml package for Lua 5.x, file functional.lua, translated to C. */
static int environ_callable (lua_State *L) {  /* 3.1.0 */
  if (!lua_isfunction(L, 1))
    return agnL_getmetafield(L, 1, "__call");
  lua_settop(L, 1);
  return 1;
}


static int environ_iscfunc (lua_State *L) {
  lua_pushboolean(L, lua_isfunction(L, 1) && lua_iscfunction(L, 1));
  return 1;
}


static int environ_isafunc (lua_State *L) {
  lua_pushboolean(L, lua_isfunction(L, 1) && !lua_iscfunction(L, 1));
  return 1;
}


static int environ_pointer (lua_State *L) {
  const void *v;
  if (lua_gettop(L) == 2) {
    /* Retrieves the object stored at the given memory address denoted by the string returned by the same function
       called with no option. Non-collectable objects, that is numbers, complex numbers, booleans and null cannot
       be retrieved. To make sure that the objects are the same you may stop the garbage collector before and
       restart it later on. Example:

       > environ.gc('stop');

       > a := [10, 20, 30];

       > p := environ.pointer(a):
       023052E0

       > environ.pointer(p, true):
       [10, 20, 30]

       > environ.gc('restart');

       The search for a given pointer is linear.

       7.5.14. The following is based on code written by Burak Güngör (https://bgslabs.org/), MIT-licenced,
       function `memview.full`, see https://github.com/burakgungor11235/lua-memview/tree/master/src/lmemview.c */
    int i, rc = 0;
    GCObject *o;
    global_State *g = G(L);
    luaL_checktype(L, 1, LUA_TSTRING);
    lua_gc(L, LUA_GCCOLLECT, 0);  /* avoid traversal of dead objects */
    lua_gc(L, LUA_GCSTOP, 0);     /* avoid crashes during traversal */
    luaL_checkstack(L, 2, "not enough stack space");
    for (o=g->rootgc; o != NULL; o = o->gch.next) {
      luaA_pushgcobject(L, o);
      lua_pushfstring(L, "%p", lua_topointer(L, -1));
      if ( (rc = lua_equal(L, 1, -1)) ) {
        agn_poptop(L);
        break;
      }
      agn_poptoptwo(L);
    }
    for (i=0; !rc && i < g->strt.size; i++) {
      for (o=g->strt.hash[i]; o != NULL; o = o->gch.next) {
        luaA_pushgcobject(L, o);
        lua_pushfstring(L, "%p", lua_topointer(L, -1));
        if ( (rc = lua_equal(L, 1, -1)) ) {
          agn_poptop(L);
          break;
        }
        agn_poptoptwo(L);
      }
    }
    lua_gc(L, LUA_GCRESTART, 0);  /* switch on GC again */
    if (!rc) {
      lua_pushnil(L);
    }
  } else {
    v = lua_topointer(L, 1);
    if (v == NULL)
      lua_pushfail(L);
    else
      lua_pushfstring(L, "%p", v);
  }
  return 1;
}


static const luaL_Reg environlib[] = {
  {"arithstate", environ_arithstate},         /* added September 23, 2022 */
  {"arity", environ_arity},                   /* added April 09, 2018 */
  {"attrib", environ_attrib},                 /* added April 20, 2008 */
  {"callable", environ_callable},             /* added July 11, 2023 */
  {"decpoint", environ_decpoint},             /* added October 01, 2020 */
  {"gc", environ_collectgarbage},             /* formerly {"collectgarbage", ...},  changed 0.4.0 */
  {"getfenv", environ_getfenv},
  {"getopt", environ_getopt},                 /* added June 03, 2022 */
  {"isafunc", environ_isafunc},               /* added May 15, 2026 */
  {"iscfunc", environ_iscfunc},               /* added May 15, 2026 */
  {"isequal", environ_isequal},               /* added July 16, 2018 */
  {"kernel", environ_kernel},                 /* added 27.08.2009 */
  {"frozen", environ_frozen},                 /* added June 26, 2025 */
  {"pointer", environ_pointer},               /* added May 22, 2009 */
  {"ref", environ_ref},                       /* added July 16, 2018 */
  {"setfenv", environ_setfenv},
  {"system", environ_system},                 /* moved from debug library November 01, 2015 */
  {"unref", environ_unref},                   /* added July 16, 2018 */
  {"used", environ_used},
  {"userinfo", environ_userinfo},             /* added June 14, 2009 */
  {"warn", environ_warn},                     /* added on June 12, 2020 */
  {"__RESTART", environ_restart},             /* added December 16, 2006 */
  {NULL, NULL}
};

/*
** Open environ library
*/
LUALIB_API int luaopen_environ (lua_State *L) {
  luaL_register(L, AGENA_ENVIRONLIBNAME, environlib);
  lua_rawsetstringnumber(L, -1, "maxpathlength", PATH_MAX - 1);   /* 1.6.0 */
  /* set number of lines to be printed at once at table output */
  lua_rawsetstringnumber(L, -1, "more", 40);
  /* set protected names for `with` function */
  lua_pushstring(L, "withprotected");
  agn_createset(L, 2);
  lua_rawset(L, -3);
  return 1;
}

