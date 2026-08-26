/*
** $Id: sema.c,v 1.67 13/09/2018 roberto Exp $
** Semaphore Library
** See Copyright Notice in agena.h
*/

#include <signal.h>

#define sema_c
#define LUA_LIB

#include "agena.h"
#include "semalib.h"

#include "agnxlib.h"
#include "agenalib.h"
#include "agnhlps.h"

#define AGENA_LIBVERSION	"sema 1.0.0 for Agena as of March 10, 2019\n"

#define checksema(L, n) (Sema *)luaL_checkudata(L, n, "sema")


static Sema *s;

static int initialised = 0;

/*
import sema
for i from 0 to 300 do print(i, sema.open()) od
sema.close(256);
sema.open():
sema.open():

s := sema.new();
for i from 0 to 300 do print(i, sema.open(s)) od
sema.close(s, 256);
sema.open(s):
sema.open(s):
*/

static int sema_new (lua_State *L) {
  lua_Integer x = agnL_optposint(L, 1, MAXSEMAINT);
  if (x > MAXSEMAINT)
    luaL_error(L, "Error in " LUA_QS ": maximum limit to large.", "sema.new");
  Sema *s = (Sema *)lua_newuserdata(L, sizeof(Sema));
  if (semalib_init(s, x))
    luaL_error(L, "Error in " LUA_QS ": semaphore initialisation error.", "sema.new");
  lua_setmetatabletoobject(L, -1, "sema", 1);
  return 1;
}

static void aux_openerror (lua_State *L, int r) {
  if (r == 1)  /* = 2^53 - 2 = 9,007,199,254,740,992 - 2 */
    luaL_error(L, "Error in " LUA_QS ": too many open semaphores.", "sema.open");
  else if (r == -1)
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "sema.open");
}

static int64_t aux_open (lua_State *L, Sema *s) {
  uint32_t flag, x, i, pos;
  flag = pos = 0;
  if (s->scon) {
    int r;
    if (s->sc == MAXSEMAINT)  /* = 2^53 = 9,007,199,254,740,992; 4.4.2 */
      luaL_error(L, "Error in " LUA_QS ": too many open semaphores.", "sema.open");
    r = semalib_open(s, s->sc + 1);
    if (r == 2) return -1;  /* limit reached */
    if (r != 0) aux_openerror(L, r);
    return s->sc;
  }
  if (s->c == MAXSEMAINT)  /* = 2^53 = 9,007,199,254,740,992 */
    luaL_error(L, "Error in " LUA_QS ": too many open semaphores.", "sema.open");
  for (i=s->ff; i < s->n; i++) {  /* iterate all chunks */
    x = s->s[i];
    if (tools_onebits32(x) == SIZECHUNK)  /* carry / übertrag, i.e. number of bits set = 32, use next slot if existing */
      continue;  /* check next slot */
    else {
      pos = FIRSTZEROBIT(x);  /* get position of rightmost zero bit from chunk, pos = [0, 31],
        if x consists entirely of one bits, the return of FIRSTZEROBIT(x) is 0. */
      s->s[i] = SETBIT(s->s[i], pos, 1);
      s->ff = i;
      flag = 1;
      break;
    }
  }
  if (flag == 0) {  /* enlarge semaphore array */
    s->s = realloc(s->s, s->n * SIZESLOT + SIZECHUNK);
    if (!s->s)
      luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "sema.open");
    s->n += SLOTS;
    s->ff = i;
    for (; i < s->n; i++) s->s[i] = 0;
    s->s[s->ff] = 1;  /* set first bit in new chunk */
    pos = 0;
  }
  s->c = s->ff*SIZECHUNK + pos;
  return s->c;
}

static int64_t aux_openpos (lua_State *L, Sema *s, int offset) {
  int64_t x, r;
  x = agn_checknonnegint(L, 1 + offset);
  r = semalib_open(s, x);
  if (r != 0 && r != 2) aux_openerror(L, r);
  return r == 2 ? -1 : x;
}

/* Creates a new semaphore and returns its number, a nonnegative integer. With the very first call, returns 0, counting up by 1 subsequently
   as along as `sema.close` or `sema.reset` is not executed. */
static int sema_open (lua_State *L) {  /* 2.14.0 */
  lua_Integer x;
  if (lua_gettop(L) == 0)  /* global semaphore s */
    x = aux_open(L, s);
  else if (agn_isnumber(L, 1))
    x = aux_openpos(L, s, 0);
  else if (luaL_isudata(L, 1, "sema")) {
    Sema *s = lua_touserdata(L, 1);
    x = agn_isnumber(L, 2) ? aux_openpos(L, s, 1) : aux_open(L, s);
  } else {
    x = 0;
    luaL_error(L, "Error in " LUA_QS ": invalid argument.", "sema.open");
  }
  lua_pushnumber(L, x == -1 ? AGN_NAN : x);
  return 1;
}


static void aux_close (lua_State *L, Sema *s, int start, int stop) {
  int i;
  for (i=start; i <= stop; i++) {
    if (semalib_close(s, agn_checknonnegint(L, i)))
      luaL_error(L, "Error in " LUA_QS ": invalid semaphore.", "sema.close");
  }
}

/* Closes the given semaphore `s`. If `sema.open` is called thereafter, it will return `s`. The function returns nothing. See also: sema.open, sema.shrink. */
static int sema_close (lua_State *L) {  /* 2.14.0 */
  int nargs;
  nargs = lua_gettop(L);
  if (luaL_isudata(L, 1, "sema")) {
    Sema *s = lua_touserdata(L, 1);
    aux_close(L, s, 2, nargs);
  } else  /* global semaphore s */
    aux_close(L, s, 1, nargs);
  return 0;
}


/* Checks whether the given semaphore `s` is open and returns true or false. */
static int sema_isopen (lua_State *L) {  /* 2.14.0 */
  uint32_t offset;
  int r;
  LUA_INTEGER x;
  offset = luaL_isudata(L, 1, "sema");
  x = agn_checknonnegint(L, 1 + offset);
  if (offset) {
    Sema *s = lua_touserdata(L, 1);
    r = semalib_isopen(s, x);
  } else  /* global semaphore s */
    r = semalib_isopen(s, x);
  if (r == -1)
    luaL_error(L, "Error in " LUA_QS ": invalid semaphore.", "sema.isopen");
  lua_pushboolean(L, r);
  return 1;
}


/* Closes all allocated semaphores and also shrinks the internal memory used to administer the semaphores to the default size, if possible.
   The function returns nothing. See also: sema.shrink. */

static int sema_reset (lua_State *L) {  /* 2.14.0 */
  int r;
  if (luaL_isudata(L, 1, "sema")) {
    Sema *s = lua_touserdata(L, 1);
    r = semalib_reset(s);
  } else  /* global semaphore s */
    r = semalib_reset(s);
  if (r == -1)
    luaL_error(L, "Error in " LUA_QS ": semaphore re-initialisation error.", "sema.reset");
  return 0;
}


/* Shrinks the internal memory used to administer all semaphores so that it consumes the least necessary space. If internal memory size could be reduced, it returns `true` and `false` otherwise. Note that due to
performance reasons, `sema.close` does not try to reduce memory consumption. */

static int sema_shrink (lua_State *L) {  /* 2.14.0 */
  int r;
  if (luaL_isudata(L, 1, "sema")) {
    Sema *s = lua_touserdata(L, 1);
    r = semalib_shrink(s);
  } else  /* global semaphore s */
    r = semalib_shrink(s);
  if (r == -1)
    luaL_error(L, "Error in " LUA_QS ": memory re-allocation error.", "sema.shrink");
  lua_pushboolean(L, r);
  return 1;
}


/* Returns administrative information on the current package state, in a table. The semaphores are internally stored using an 8 * 32 bits array by default.
   Each 32-bit chunk is called a `slot`. The 'firstfreeslot' field contains the number of the first slot that might harbour a new semaphore. 'totalslots'
   depicts the current total number of slots. The 'slot' sequence returns the bits set in all slots, as decimal non-negative integers. 'nextfree'
   denotes the number of the semaphore if `sema.open` would be called next. Field 'current' is an estimate of the last allocated semaphore.
   Key `minopen` denotes the semaphore with the smallest ID, `maxopen` the one with the largest ID. */
static int aux_state (lua_State *L, Sema *s) {  /* 2.14.0 */
  uint32_t i, x, nextfree, flag, min, max, minflag, maxflag, n_onebits, msbstar;
  min = max = 0;
  lua_createtable(L, 0, 2);
  lua_pushstring(L, "firstfreeslot");
  lua_pushnumber(L, s->ff);
  lua_settable(L, -3);
  lua_pushstring(L, "totalslots");
  lua_pushnumber(L, s->n);
  lua_settable(L, -3);
  lua_pushstring(L, "slots");
  agn_createseq(L, s->n);
  nextfree = 0;
  flag = minflag = maxflag = 1;
  for (i=0; i < s->n; i++) {
    x = s->s[i];
    agn_seqsetinumber(L, -1, i + 1, x);
    n_onebits = tools_onebits32(x);
    if (flag && n_onebits < SIZECHUNK) {
      flag = 0;
      nextfree += FIRSTZEROBIT(x);
    } else if (flag)
      nextfree += SIZECHUNK;
    if (n_onebits != 0) {
      if (minflag) {
        min = i * SIZECHUNK + FIRSTONEBIT(x);
        minflag = 0;
      }
      msbstar = i * SIZECHUNK + LASTONEBIT(x);
      if (max < msbstar) max = msbstar;
    }
  }
  lua_settable(L, -3);
  lua_pushstring(L, "nextfree");
  lua_pushinteger(L, nextfree);
  lua_settable(L, -3);
  lua_pushstring(L, "minopen");
  lua_pushinteger(L, min);
  lua_settable(L, -3);
  lua_pushstring(L, "maxopen");
  lua_pushinteger(L, max);
  lua_settable(L, -3);
  lua_pushstring(L, "current");  /* last allocated semaphore */
  lua_pushnumber(L, s->c);
  lua_settable(L, -3);
  lua_pushstring(L, "simplecounter_active");
  lua_pushboolean(L, s->scon);
  lua_settable(L, -3);
  lua_pushstring(L, "simplecounter");
  lua_pushnumber(L, s->sc);
  lua_settable(L, -3);
  lua_pushstring(L, "open");
  lua_pushnumber(L, s->noopen);
  lua_settable(L, -3);
  lua_pushstring(L, "maxopen");
  lua_pushnumber(L, s->maxopen);
  lua_settable(L, -3);
  return 1;
}

static int sema_state (lua_State *L) {  /* 2.14.0 */
  if (luaL_isudata(L, 1, "sema")) {
    Sema *s = lua_touserdata(L, 1);
    aux_state(L, s);
  } else  /* global semaphore s */
    aux_state(L, s);
  return 1;
}


/* Increases the maximum number of concurrently open ids for an instance; 4.4.2 */
static int sema_limit (lua_State *L) {
  int nargs = lua_gettop(L);
  Sema *s = checksema(L, 1);
  if (nargs > 1) {
    lua_Integer x = agn_checkposint(L, 2);
    if (x > MAXSEMAINT) x = MAXSEMAINT;
    if (x < s->noopen)
      luaL_error(L, "Error in " LUA_QS ": limit is less than the number of ids currently open.", "sema.limit");
    s->maxopen = x;
  }
  lua_pushnumber(L, s->maxopen);
  return 1;
}


static int mt_sema2string (lua_State *L) {  /* at the console, the array is formatted as follows: */
  Sema *a = checksema(L, 1);
  if (agn_getutype(L, 1)) {
    lua_pushfstring(L, "(%p)", a);
    lua_concat(L, 2);
  } else
    luaL_error(L, "Error in " LUA_QS ": invalid semaphore.", "sema.__tostring");
  return 1;
}

static int mt_semagc (lua_State *L) {  /* please do not forget to garbage collect deleted userdata */
  (void)L;
  Sema *s = checksema(L, 1);
  xfree(s->s);  /* do not free s itself ! */
  agn_setutypestring(L, 1, NULL);
  return 0;
}

static int mt_getsize (lua_State *L) {  /* 2.14.6, returns the number of allocated slots in the userdata numeric array */
  Sema *s = checksema(L, 1);
  lua_pushinteger(L, s->n);
  return 1;
}


static int sema_get (lua_State *L) {  /* 2.14.6, get a number from the given Lua/Agena index. UNDOC */
  Sema *s = checksema(L, 1);
  if (lua_isnoneornil(L, 2)) {  /* new 4.4.1, query simple counter */
    lua_pushnumber(L, s->sc);  /* might overflow */
    return 1;
  } else if (agn_isposint(L, 2)) {
    uint32_t index = agn_tointeger(L, 2) - 1;  /* realign Lua/Agena index to C index, starting from 0 */
    if (index >= s->n)
      luaL_error(L, "Error in " LUA_QS ": index %d out of range.", "sema.get", index + 1);
    lua_pushnumber(L, s->s[index]);
    return 1;
  } else {  /* 4.4.1 call OOP method */
    return agn_initmethodcall(L, AGENA_SEMALIBNAME, sizeof(AGENA_SEMALIBNAME) - 1);
  }
}


static int sema_set (lua_State *L) {  /* 2.14.6, get a number from the given Lua/Agena index. */
  Sema *s = checksema(L, 1);
  uint32_t index = agn_checkposint(L, 2) - 1;  /* realign Lua/Agena index to C index, starting from 0 */
  uint32_t value = agn_checknonnegint(L, 3);
  if (index >= s->n)
    luaL_error(L, "Error in " LUA_QS ": index %d out of range.", "sema.get", index + 1);
  s->s[index] = value;
  return 0;
}


static int sema_expand (lua_State *L) {  /* 2.14.6 */
  int r;
  Sema *s = checksema(L, 1);
  uint32_t nslots = agn_checknonnegint(L, 2);
  r = semalib_expand(s, nslots);
  if (r == -1)
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "sema.expand");
  lua_pushboolean(L, r);
  return 1;
}


static const struct luaL_Reg sema_lib [] = {  /* metamethods for sema userdata `n` */
  {"close", sema_close},          /* added on September 13, 2018 */
  {"expand", sema_expand},        /* added on March 10, 2019 */
  {"get", sema_get},              /* added on March 10, 2019 */
  {"isopen", sema_isopen},        /* added on September 21, 2018 */
  {"limit", sema_limit},          /* added on October 15, 2024 */
  {"open", sema_open},            /* added on September 13, 2018 */
  {"reset", sema_reset},          /* added on September 20, 2018 */
  {"set", sema_set},              /* added on March 10, 2019 */
  {"shrink", sema_shrink},        /* added on September 21, 2018 */
  {"state", sema_state},          /* added on September 13, 2018 */
  {"__index", sema_get},          /* n[p], with p the index, counting from 1 */
  {"__writeindex", sema_set},     /* n[p] := value, with p the index, counting from 1 */
  {"__size", mt_getsize},
  {"__tostring", mt_sema2string}, /* for output at the console, e.g. print(n) */
  {"__gc", mt_semagc},            /* please do not forget garbage collection */
  {NULL, NULL}
};

static const luaL_Reg semalib[] = {
  {"close", sema_close},          /* added on September 13, 2018 */
  {"expand", sema_expand},        /* added on March 10, 2019 */
  {"get", sema_get},              /* added on March 10, 2019 */
  {"isopen", sema_isopen},        /* added on September 21, 2018 */
  {"limit", sema_limit},          /* added on October 15, 2024 */
  {"new", sema_new},              /* added on March 01, 2019 */
  {"open", sema_open},            /* added on September 13, 2018 */
  {"reset", sema_reset},          /* added on September 20, 2018 */
  {"set", sema_set},              /* added on March 10, 2019 */
  {"shrink", sema_shrink},        /* added on September 21, 2018 */
  {"state", sema_state},          /* added on September 13, 2018 */
  {NULL, NULL}
};


/*
** Open sema library
*/

static void semacleanup (void) {  /* cleanup is not called when pressing CTRL+C */
  semalib_destroy(s);
}

static void semasigcleanup (int sig) {  /* called when pressing CTRL+C */
  semalib_destroy(s);
}

static void createmeta (lua_State *L) {
  luaL_newmetatable(L, AGENA_SEMALIBNAME);  /* create metatable */
  lua_pushvalue(L, -1);                     /* push metatable */
  lua_setfield(L, -2, "__index");           /* metatable.__index = metatable */
  luaL_register(L, NULL, sema_lib);         /* methods */
}

LUALIB_API int luaopen_sema (lua_State *L) {
  /* metamethods */
  createmeta(L);
  /* register library */
  luaL_register(L, AGENA_SEMALIBNAME, semalib);
  lua_rawsetstringstring(L, -1, "initstring", AGENA_LIBVERSION);
  /* initialise global semaphore */
  if (!initialised) {
    s = semalib_new(MAXSEMAINT);
    if (s == NULL)
      luaL_error(L, "Error in " LUA_QS ": semaphore initialisation error.", "sema");
    initialised = 1;
    atexit(semacleanup);
    signal(SIGTERM, semasigcleanup);
  }
  return 1;
}

