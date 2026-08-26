/*
** $Id: stack.c,v 1.38 2005/10/23 17:38:15 roberto Exp $
** Library for Stack Administration
** See Copyright Notice in agena.h
*/


#include <errno.h>      /* for errno */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define stack_c
#define LUA_LIB

#include "agena.h"

#include "agenalib.h"
#include "agncmpt.h"
#include "agnconf.h"
#include "agnhlps.h"
#include "agnxlib.h"
#include "lapi.h"
#include "lstate.h"   /* cells, stackcells, getcharcell, ... iscachestack */
#include "prepdefs.h"  /* FORCE_INLINE */

#define aux_checkstackindex(L, idx, procname) { \
  cell = agn_checkinteger(L, idx); \
  if (cell < 0) \
    cell = stacktop(L) + cell; \
  if (cell < LUA_CELLSTACKBOTTOM || cell >= stacktop(L)) \
    luaL_error(L, "Error in " LUA_QS ": stack position is out-of-range or empty stack.", procname); \
}

#define aux_isnonatomic(L,idx) ({ \
  int __t = lua_type(L, idx); \
  (__t >= LUA_TUSERDATA || __t == LUA_TCOMPLEX); \
})

static int aux_getabsindex (lua_State *L, int *i, int retneg, const char *procname) {  /* 5.3.5 */
  int top = stacktop(L);
  if (*i < 0) *i = top + *i;
  if (*i < LUA_CELLSTACKBOTTOM || *i >= top) return 1;  /* result is out-of-range or stack is empty */
  else {
    if (retneg) *i = top - *i - 1;
    return 0;
  }
}

#define aux_checkstackindices(L, idx1, idx2, procname) { \
  cell1 = agn_checkinteger(L, idx1); \
  if (cell1 < 0) \
    cell1 = stacktop(L) + cell1; \
  if (cell1 < LUA_CELLSTACKBOTTOM || cell1 >= stacktop(L)) \
    luaL_error(L, "Error in " LUA_QS ": stack position is out-of-range or empty stack.", procname); \
  cell2 = agn_checkinteger(L, idx2); \
  if (cell2 < 0) \
    cell2 = stacktop(L) + cell2; \
  if (cell2 < LUA_CELLSTACKBOTTOM || cell2 >= stacktop(L)) \
    luaL_error(L, "Error in " LUA_QS ": stack position is out-of-range or empty stack.", procname); \
}

#define aux_optstackindex(L, idx, procname) { \
  cell = agnL_optinteger(L, idx, -1); \
  if (cell < 0) \
    cell = stacktop(L) + cell; \
  if (cell < LUA_CELLSTACKBOTTOM || cell >= stacktop(L)) \
    luaL_error(L, "Error in " LUA_QS ": stack position is out-of-range or empty stack.", procname); \
}

/* 2.22.2, prevent overflows when growing and do not grow too much; cachestack is stack #7 that can store all kinds of data */
#define aux_checkstack(L, nargs, procname) { \
  if (stacktop(L) + (nargs) - 1 >= stackmax(L)) { \
    size_t newsize; \
    if (stackmax(L) < 1024) newsize = stackmax(L) << 1; \
    else newsize = tools_adjustmultiple((double)stackmax(L)*(5.0/4.0), 4); \
    if (newsize < stackmax(L)) { \
      luaL_error(L, "Error in " LUA_QS ": stack out of bounds.", procname); \
    } \
    if (isnumstack(L)) { \
      lua_Number *temp = realloc(L->cells[L->currentstack], newsize*sizeof(lua_Number)); \
      if (temp == NULL) \
        luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", procname); \
      L->cells[L->currentstack] = temp; \
    } else if (iscachestack(L)) { \
      luaL_checkcache(L->C, nargs, procname); \
      newsize = stacktop(L) + nargs; \
    } else { \
      unsigned char *temp = realloc(L->charcells[L->currentstack - LUA_NUMSTACKS], newsize*sizeof(unsigned char)); \
      if (temp == NULL) \
        luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", procname); \
      L->charcells[L->currentstack - LUA_NUMSTACKS] = temp; \
    } \
    stackmax(L) = newsize; \
  } \
}

/* clears the top n elements in the cache stack; should be called BEFORE L->C->top is reset in a separate statement. 2.37.1 */
static void aux_nullifycache (lua_State *L, int n) {
  StkId i;
  for (i=L->C->top - n; i < L->C->top; i++) {
    setobj2s(L->C, i, luaO_nilobject);  /* does no harm if cache is empty */
  }
}


static int stack_pushvalued (lua_State *L) {  /* 2.9.4, push value residing at the given relative stack position to the top of the stack */
  int cell;
  aux_checkstack(L, 1, "stack.pushvalued");
  aux_optstackindex(L, 1, "stack.pushvalued");
  if (isnumstack(L)) {  /* 2.12.7 */
    cells(L, stacktop(L)) = cells(L, cell);
  } else if (iscachestack(L)) {  /* 2.37.0 */
    setobj2s(L->C, L->C->top++, L->C->base + cell);
  } else {
    charcell(L, stacktop(L)) = charcell(L, cell);
  }
  stacktop(L)++;
  return 0;
}


static int stack_removed (lua_State *L) {  /* 2.9.4, removes the value residing at the given relative stack position,
  returns it, and moves all elements to close the space */
  int cell, i;
  aux_checkstackindex(L, 1, "stack.removed");
  if (isnumstack(L)) {  /* 2.12.7 */
    lua_Number v = cells(L, cell);
    for (i=cell + 1; i < stacktop(L); i++)
      cells(L, i - 1) = cells(L, i);
    lua_pushnumber(L, v);
  } else if (iscachestack(L)) {  /* 2.37.0 */
    StkId q;
    lua_lock(L);
    lua_pushcachevalue(L, -stacktop(L) + cell);
    for (q = L->C->base + cell; q < L->C->top - 1; q++) setobjs2s(L->C, q, q + 1);
    aux_nullifycache(L, 1);  /* 2.37.2 fix */
    L->C->top--;
    lua_unlock(L);
  } else {  /* 2.12.7 */
    char v = charcell(L, cell);
    for (i=cell + 1; i < stacktop(L); i++)
      charcell(L, i - 1) = charcell(L, i);
    lua_pushchar(L, v);
  }
  stacktop(L)--;
  return 1;
}


static int stack_dequeued (lua_State *L) {  /* 2.22.2, remove item at the bottom of a stack, and moves all elements to close the space */
  int i;
  if (stacktop(L) == 0)
    luaL_error(L, "Error in " LUA_QS ": empty stack.", "dequeued");
  if (isnumstack(L)) {  /* 2.12.7 */
    lua_Number v = cells(L, 0);
    for (i=1; i < stacktop(L); i++)
      cells(L, i - 1) = cells(L, i);
    lua_pushnumber(L, v);
  } else if (iscachestack(L)) {  /* 2.37.0 */
    StkId q;
    lua_lock(L);
    lua_pushcachevalue(L, -stacktop(L));
    for (q = L->C->base; q < L->C->top - 1; q++) setobjs2s(L->C, q, q + 1);
    aux_nullifycache(L, 1);  /* 2.37.2 fix */
    L->C->top--;
    lua_unlock(L);
  } else {  /* 2.12.7 */
    char v = charcell(L, 0);
    for (i=1; i < stacktop(L); i++)
      charcell(L, i - 1) = charcell(L, i);
    lua_pushchar(L, v);
  }
  stacktop(L)--;
  return 1;
}


static int stack_insertd (lua_State *L) {  /* 2.9.4, inserts an element to the given stack position shifting up other elements to open space */
  int cell, i;
  aux_checkstack(L, 1, "stack.insertd");
  aux_checkstackindex(L, 1, "stack.insertd");
  if (isnumstack(L)) {
    for (i=stacktop(L); i > cell; i--)
      cells(L, i) = cells(L, i - 1);
    cells(L, cell) = agn_checknumber(L, 2);
  } else if (iscachestack(L)) {  /* 2.37.0 */
    StkId p, q;
    luaL_checkcache(L->C, 1, "stack.insertd");
    lua_lock(L);
    if (aux_isnonatomic(L, 2))  /* 6.5.8 fix: cannot move collectable objects and complex numbers between states */
      luaL_error(L, "Error in " LUA_QS ": cannot insert %s.", "stack.insertd", luaL_typename(L, 2));
    p = index2adr(L, 2);
    for (q = L->C->top; q > L->C->base + cell; q--) setobjs2s(L->C, q, q - 1);
    L->C->top++;
    setobjs2s(L, L->C->base + cell, p);
    lua_unlock(L);
  } else {  /* 2.12.7 */
    for (i=stacktop(L); i > cell; i--)
      charcell(L, i) = charcell(L, i - 1);
    if (agn_isstring(L, 2))
      charcell(L, cell) = agn_tostring(L, 2)[0];
    else if (agn_isnumber(L, 2)) {
      unsigned char n = lua_tointeger(L, 2) + '0';  /* integers 0 to 9 */
      charcell(L, cell) = n;
    } else
      luaL_error(L, "Error in " LUA_QS ": expected a number or string, got %s.", "stack.insertd", luaL_typename(L, 2));   /* 2.18.2 error message fix */
  }
  stacktop(L)++;
  return 0;
}


static int stack_enqueued (lua_State *L) {  /* 2.22.2, inserts element to the bottom of the stack shifting up other elements to open space */
  int i;
  luaL_checkany(L, 1);  /* 2.37.1 */
  aux_checkstack(L, 1, "stack.enqueued");
  if (isnumstack(L)) {
    for (i=stacktop(L); i > 0; i--)
      cells(L, i) = cells(L, i - 1);
    cells(L, 0) = agn_checknumber(L, 1);
  } else if (iscachestack(L)) {
    StkId p, q;
    luaL_checkcache(L->C, 1, "stack.enqueued");
    lua_lock(L);
    if (aux_isnonatomic(L, 1))  /* 6.5.8 fix: cannot move collectable objects and complex numbers between states */
      luaL_error(L, "Error in " LUA_QS ": cannot insert %s.", "stack.enqueued", luaL_typename(L, 1));
    p = index2adr(L, 1);
    for (q = L->C->top; q > L->C->base; q--) setobjs2s(L->C, q, q - 1);
    L->C->top++;
    setobjs2s(L, L->C->base, p);
    lua_unlock(L);
  } else {  /* 2.12.7 */
    for (i=stacktop(L); i > 0; i--)
      charcell(L, i) = charcell(L, i - 1);
    if (agn_isstring(L, 1))
      charcell(L, 0) = agn_tostring(L, 1)[0];
    else if (agn_isnumber(L, 1)) {
      unsigned char n = lua_tointeger(L, 1) + '0';  /* integers 0 to 9 */
      charcell(L, 0) = n;
    } else
      luaL_error(L, "Error in " LUA_QS ": expected a number or string, got %s.", "stack.enqueued", luaL_typename(L, 1));   /* 2.18.2 error message fix */
  }
  stacktop(L)++;
  return 0;
}


static int stack_resetd (lua_State *L) {  /* 2.9.4, Clears the entire stack so that it becomes empty. The function
  does not return anything and should be used if succeedingly working with stacks in a session. The number of
  pre-allocated slots is not changed, however, see shrinkd. */
  size_t nargs;
  nargs = lua_gettop(L);
  if (L->currentstack == 0)
    luaL_error(L, "Error in " LUA_QS ": cannot operator on stack 0.", "stack.resetd");
  if (nargs == 0) {  /* clear current stack */
    if (iscachestack(L)) {
      aux_nullifycache(L, stacktop(L));  /* 2.37.1 change */
      L->C->top = L->C->base;
    }
    stacktop(L) = LUA_CELLSTACKBOTTOM;
  } else {  /* clear given stacks */
    size_t i;
    int cnr, oldcnr;
    oldcnr = L->currentstack;
    for (i=1; i <= nargs; i++) {
      cnr = agn_checkinteger(L, i);
      if (cnr < 0 || cnr >= LUA_NSTACKS)
        luaL_error(L, "Error in " LUA_QS ": invalid stack number %d.", "stack.resetd", cnr);
      L->currentstack = cnr;  /* 2.14.2 */
      if (iscachestack(L)) {
        aux_nullifycache(L, stacktop(L));  /* 2.37.1 change */
        L->C->top = L->C->base;
      }
      stacktop(L) = LUA_CELLSTACKBOTTOM;  /* 2.37.2 fix */
    }
    L->currentstack = oldcnr;
  }
  return 0;
}


static int stack_dumpd (lua_State *L) {  /* 2.9.4, returns all or a given number of values in the built-in stack
  in a new sequence and pops them from the stack. The number of pre-allocated slots is not changed, see shrinkd. */
  int i, n, csf, option, nargs;
  if (L->currentstack == 0)  /* 2.14.2 */
    luaL_error(L, "Error in " LUA_QS ": cannot operate on stack 0.", "stack.dumpd");
  nargs = lua_gettop(L);
  option = 0;
  if ((nargs > 0) && lua_istrue(L, nargs)) {  /* 2.22.2, return elements in reverse order; 2.36.2 optimisation */
    lua_settop(L, nargs - 1); option = 1;
  };
  csf = stacktop(L);
  if (csf == 0)
    lua_pushnil(L);
  else {
    n = luaL_optinteger(L, 1, csf - LUA_CELLSTACKBOTTOM);  /* 2.9.5 */
    if (n < 0 || n > csf - LUA_CELLSTACKBOTTOM)
      luaL_error(L, "Error in " LUA_QS ": invalid number of values %d, maximum is %d.", "stack.dumpd", n, csf - LUA_CELLSTACKBOTTOM);
    if (isnumstack(L)) {
      int c = 0;
      agn_createseq(L, n);
      if (option) {  /* 2.22.2 insert in reverse order */
        for (i=csf - 1; i >= csf - n; i--) {
          agn_seqsetinumber(L, -1, ++c, cells(L, i));
        }
      } else {
        for (i=csf - n; i < csf; i++) {
          agn_seqsetinumber(L, -1, ++c, cells(L, i));
        }
      }
    } else if (iscachestack(L)) {  /* 2.37.0 */
      int c = 0;
      agn_createseq(L, n);
      if (option) {  /* insert in reverse order */
        for (i=csf - 1; i >= csf - n; i--)
          lua_seqseticachevalue(L, -1, ++c, -stacktop(L) + i);  /* 2.37.2 tweak */
      } else {
        for (i=csf - n; i < csf; i++)
          lua_seqseticachevalue(L, -1, ++c, -stacktop(L) + i);  /* 2.37.2 tweak */
      }
      aux_nullifycache(L, n);  /* 2.37.1 change */
      L->C->top -= n;
    } else {  /* 2.12.7 */
      char *s = (char *)getcharcell(L, csf - n);
      if (option) tools_strnrev(s, n);  /* reverse substring, 2.22.2 */
      lua_pushlstring(L, s, n);
    }
    stacktop(L) -= n;
  }
  return 1;
}


static int stack_sized (lua_State *L) {  /* 2.9.4, returns the current number of elements in the built-in current stack
  along with the current maximum number of slots allocated by the system. See also: allotted. */
  lua_pushinteger(L, stacktop(L));
  lua_pushinteger(L, stackmax(L));
  return 2;
}


/* Checks whether the current stack is empty and returns `true` or `false`. See also: stack.attribd. 5.3.5 */
static int stack_isempty (lua_State *L) {
  lua_pushboolean(L, LUA_CELLSTACKBOTTOM == stacktop(L));
  return 1;
}


/* With the current stack, converts a negative index into the current non-negative index and returns it. 5.3.5 */
static int stack_absindex (lua_State *L) {
  int i, nargs, top;
  top = stacktop(L);
  nargs = lua_gettop(L);
  i = agn_checkinteger(L, 1);
  if (i < 0) i = top + i;
  if (i < LUA_CELLSTACKBOTTOM || i >= top) {
    lua_pushnil(L);  /* stack is empty or out-of-range */
  } else {
    lua_pushinteger(L, (nargs > 1) ? top - i - 1 : i);
  }
  return 1;
}


static int stack_shrinkd (lua_State *L) {  /* 2.9.4; if possible, shrinks the number of pre-allocated but not assigned slots in the
  built-in stack. The function returns the new number of pre-allocated slots but does not pop any elements. The function is useful
  to reduce memory consumption if a lot of numbers have been removed from the current stack. */
  size_t p, csf, cn, i, nargs, oldcellnumber;
  if (iscachestack(L))  /* 2.37.0 */
    luaL_error(L, "Error in " LUA_QS ": cannot shrink a cache stack.", "stack.shrinkd");
  nargs = lua_gettop(L);
  cn = L->currentstack;
  if (cn == 0)  /* 2.14.2 */
    luaL_error(L, "Error in " LUA_QS ": cannot operate on stack 0.", "stack.shrinkd");
  if (nargs == 0) {
    csf = stacktop(L);
    /* GNU C Lib: "If the new size is the same as the old size, realloc is guaranteed to change nothing ..." */
    if (csf == 0)  /* 2.15.4 fix */
      p = LUA_CELLSTACKSIZE;
    else
      p = 2 << ( ( (csf < LUA_CELLSTACKSIZE) ? ceillog2(LUA_CELLSTACKSIZE) : ceillog2(csf) ) - 1 );
    if (isnumstack(L)) {
      lua_Number *temp = realloc(L->cells[cn], p * sizeof(lua_Number));  /* 7.7.7 K&R fix */
      if (temp == NULL) luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "stack.shrinkd");
      L->cells[cn] = temp;
    } else {
      unsigned char *temp = realloc(L->charcells[cn - LUA_NUMSTACKS], p * sizeof(unsigned char));
      if (temp == NULL) luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "stack.shrinkd");
      L->charcells[cn - LUA_NUMSTACKS] = temp;
    }
    stackmax(L) = p;
    lua_pushinteger(L, p);
    i = 1;
  } else {  /* 2.9.7: shrink all number and character stacks */
    oldcellnumber = L->currentstack;
    for (i=0; i < LUA_NUMSTACKS + LUA_CHARSTACKS; i++) {
      L->currentstack = cn = i;
      csf = stacktop(L);
      if (csf == 0)  /* 2.15.4 fix */
        p = LUA_CELLSTACKSIZE;
      else
        p = 2 << ( ( (csf < LUA_CELLSTACKSIZE) ? ceillog2(LUA_CELLSTACKSIZE) : ceillog2(csf) ) - 1 );
      if (isnumstack(L)) {
        lua_Number *temp = realloc(L->cells[cn], p * sizeof(lua_Number));  /* 7.7.7 K&R fix */
        if (temp == NULL) luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "stack.shrinkd");
        L->cells[cn] = temp;
      } else {  /* 7.7.7 K&R fix */
        unsigned char *temp = realloc(L->charcells[cn - LUA_NUMSTACKS], p * sizeof(unsigned char));
        if (temp == NULL) luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "stack.shrinkd");
        L->charcells[cn - LUA_NUMSTACKS] = temp;
      }
      stackmax(L) = p;
      lua_pushinteger(L, p);
    }
    i--;
    L->currentstack = oldcellnumber;
  }
  return i;
}


/* Reverses the positions of the elements in the built-in stack. The function returns nothing. */
static int stack_reversed (lua_State *L) {  /* 2.9.4 */
  size_t i, offset, csf;
  int n;  /* 2.9.7 fix */
  if (L->currentstack == 0)  /* 2.14.2 */
    luaL_error(L, "Error in " LUA_QS ": cannot operate on stack 0.", "stack.reversed");
  csf = stacktop(L);
  n = luaL_optinteger(L, 1, csf - LUA_CELLSTACKBOTTOM);  /* 2.9.6 */
  if (n < 0 || n > csf - LUA_CELLSTACKBOTTOM)
    luaL_error(L, "Error in " LUA_QS ": invalid argument.", "stack.reversed");
  offset = csf - n;
  n = csf;
  if (isnumstack(L)) {  /* 2.12.7 */
    lua_Number t;
    for (i=0 + offset; i < (n + offset)/2; i++) {  /* tools_midpoint will crash */
      t = cells(L, i);
      cells(L, i) = cells(L, n - i - 1 + offset);
      cells(L, n - i - 1 + offset) = t;
    }
  } else if (iscachestack(L)) {  /* 2.37.0 */
    ptrdiff_t i, q;
    for (i=0 + offset; i < (n + offset)/2; i++) {
      setobj2s(L->C, L->C->top, L->C->base + i);
      q = n - i - 1 + offset;
      setobj2s(L->C, L->C->base + i, L->C->base + q);
      setobj2s(L->C, L->C->base + q, L->C->top);
    }
  } else {  /* 2.12.7 */
    unsigned char t;
    for (i=0 + offset; i < (n + offset)/2; i++) {
      t = charcell(L, i);
      charcell(L, i) = charcell(L, n - i - 1 + offset);
      charcell(L, n - i - 1 + offset) = t;
    }
  }
  return 0;
}


/* Moves all the elements in the built-in stack n places from the bottom to the top if n is positive,
   and n places from the top to the bottom if n is negative. The function returns nothing. */
static int stack_rotated (lua_State *L) {  /* 2.9.4 */
  size_t s, i, j;
  int n;
  lua_Number t;
  unsigned char t1;
  if (L->currentstack == 0)  /* 2.14.2 */
    luaL_error(L, "Error in " LUA_QS ": cannot operate on stack 0.", "stack.rotated");
  s = stacktop(L);
  n = agnL_optinteger(L, 1, 1);
  if (s < 2) return 0;
  if (n < 0) {
    if (isnumstack(L)) {  /* 2.12.7 */
      for (j=0; j < -n; j++) {
        t = cells(L, 0);
        for (i=0; i < s - 1; i++) cells(L, i) = cells(L, i + 1);
        cells(L, s - 1) = t;
      }
    } else if (iscachestack(L)) {  /* 2.37.0 */
      StkId i;
      for (j=0; j < -n; j++) {
        setobj2s(L->C, L->C->top, L->C->base);
        for (i=L->C->base; i < L->C->top - 1; i++) setobj2s(L->C, i, i + 1);
        setobj2s(L->C, L->C->top - 1, L->C->top);
      }
    } else {  /* 2.12.7 */
      for (j=0; j < -n; j++) {
        t1 = charcell(L, 0);
        for (i=0; i < s - 1; i++) charcell(L, i) = charcell(L, i + 1);
        charcell(L, s - 1) = t1;
      }
    }
  } else if (n > 0) {  /* 2.12.7 */
    if (isnumstack(L)) {
      for (j=0; j < n; j++) {
        t = cells(L, s - 1);
        for (i=s - 1; i > 0; i--) cells(L, i) = cells(L, i - 1);
        cells(L, 0) = t;
      }
    } else if (iscachestack(L)) {  /* 2.37.0 */
      StkId i;
      for (j=0; j < n; j++) {
        setobj2s(L->C, L->C->top, L->C->top - 1);
        for (i=L->C->top - 1; i > L->C->base; i--) setobj2s(L->C, i, i - 1);
        setobj2s(L->C, L->C->base, L->C->top);
      }
    } else {
      for (j=0; j < n; j++) {
        t1 = charcell(L, s - 1);
        for (i=s - 1; i > 0; i--) charcell(L, i) = charcell(L, i - 1);
        charcell(L, 0) = t1;
      }
    }
  }
  return 0;
}


/* Sorts the last given number of elements pushed onto the current stack in ascending order using the fast Introsort
   algorithm. If no argument is passed, all elements in the current stack are processed. The function returns nothing. */
static int stack_sorted (lua_State *L) {  /* 2.9.7 */
  size_t csf;
  int n;
  csf = stacktop(L);
  n = luaL_optinteger(L, 1, csf - LUA_CELLSTACKBOTTOM);
  if (!isnumstack(L))
    luaL_error(L, "Error in " LUA_QS ": cannot sort character or cache stacks.", "stack.sorted");
  if (L->currentstack == 0)  /* 2.14.2 */
    luaL_error(L, "Error in " LUA_QS ": cannot operate on stack 0.", "stack.sortedd");
  if (n < 0 || n > csf - LUA_CELLSTACKBOTTOM)
    luaL_error(L, "Error in " LUA_QS ": invalid argument.", "stack.sorted");
  tools_dintrosort(L->cells[L->currentstack], csf - n, csf - 1, 0, 2*log2(csf));
  return 0;
}


static int stack_selected (lua_State *L) {  /* 2.9.7, returns the current stack number. */
  lua_pushinteger(L, L->currentstack);  /* changed 2.14.2 */
  lua_pushinteger(L, L->formerstack);  /* added 2.37.2 */
  return 2;
}


static int stack_choosed (lua_State *L) {  /* 2.9.7, returns the number of the stack with the least number of stored values. */
  int i, min, minchar, x, smallest, smallestchar, oldstack;
  min = minchar = smallest = smallestchar = MAX_INT;
  oldstack = L->currentstack;
  for (i=LUA_NUMSTACKS + LUA_CHARSTACKS - 1; i >= LUA_NUMSTACKS; i--) {
    L->currentstack = i;
    x = stacktop(L);
    if (x <= minchar) { minchar = x; smallestchar = i; }  /* always return the smallest stack number */
  }
  for (i=LUA_NUMSTACKS - 1; i > 0 ; i--) {  /* changed 2.14.2, do not check stack 0 */
    L->currentstack = i;
    x = stacktop(L);
    if (x <= min) { min = x; smallest = i; }  /* always return the smallest stack number */
  }
  L->currentstack = oldstack;
  lua_pushinteger(L, smallest);      /* changed 2.14.2 */
  lua_pushinteger(L, smallestchar);  /* changed 2.14.2 */
  return 2;
}


/* Determines the current stack number p, and then switches to the number stack q with the least number of elements if
   any number x or no argument is given, or to the character stack q with the least number of elements if any string x is
   given. The function returns p and q, in this order. You can use p later to switch back to the former stack with the
   `switchd` statement. */
static int stack_switchto (lua_State *L) {  /* 2.14.4, switch to smallest stack. */
  int x, i, t, min, smallest;
  if (iscachestack(L))
    luaL_error(L, "Error in " LUA_QS ": cache stacks are not supported.", "stack.switchd");
  t = L->formerstack;  /* 2.37.2 improvement */
  min = smallest = MAX_INT;
  L->formerstack = L->currentstack;
  if (lua_isnoneornil(L, 1) || lua_isnumber(L, 1)) {  /* switch to smallest number stack */
    for (i=LUA_NUMSTACKS - 1; i > 0 ; i--) {
      L->currentstack = i;
      x = stacktop(L);
      if (x <= min) { min = x; smallest = i; }  /* always return the smallest stack number */
    }
  } else if (lua_isstring(L, 1)) {  /* goto char stack */
    for (i=LUA_NUMSTACKS + LUA_CHARSTACKS - 1; i >= LUA_NUMSTACKS; i--) {
      L->currentstack = i;
      x = stacktop(L);
      if (x <= min) { min = x; smallest = i; }  /* always return the smallest stack number */
    }
  } else {
    L->formerstack = t;
    luaL_error(L, "Error in " LUA_QS ": invalid argument.", "stack.switchto");
  }
  L->currentstack = smallest;
  lua_pushinteger(L, L->formerstack);
  lua_pushinteger(L, L->currentstack);
  return 2;
}


/* Pushes all characters in the string str onto a character stack. The function returns the size of `str`. */
static int stack_pushstringd (lua_State *L) {  /* 2.12.7 */
  size_t l;
  const char *str = agn_checklstring(L, 1, &l);
  if (isnumstack(L) || iscachestack(L))
    luaL_error(L, "Error in " LUA_QS ": need a character stack, got stack #%d.", "stack.pushstringd", L->currentstack);
  if (l != 0) {
    aux_checkstack(L, l, "stack.pushstringd");
    while (*str) {
      charcell(L, (stacktop(L)++)) = *str++;
    }
  }
  lua_pushinteger(L, l);
  return 1;
}


static int stack_replaced (lua_State *L) {  /* 2.12.7 */
  ptrdiff_t cell;
  aux_checkstackindex(L, 1, "stack.replaced");
  if (isnumstack(L)) {
    lua_Number x = agn_checknumber(L, 2);
    lua_pushnumber(L, cells(L, cell));
    cells(L, cell) = x;
  } else if (iscachestack(L)) {  /* 2.37.0 */
    StkId p;
    lua_lock(L);
    if (aux_isnonatomic(L, 2))  /* 6.5.8 fix: cannot move collectable objects and complex numbers between states */
      luaL_error(L, "Error in " LUA_QS ": cannot insert %s.", "stack.replaced", luaL_typename(L, 2));
    p = index2adr(L, 2);
    lua_pushcachevalue(L, -stacktop(L) + cell);  /* push former cache value for later return */
    setobjs2s(L, L->C->base + cell, p);
    lua_unlock(L);
  } else {  /* 2.12.7 */
    lua_pushlstring(L, getcharcell(L, cell), 1);
    if (agn_isstring(L, 2))
      charcell(L, cell) = agn_tostring(L, 2)[0];
    else if (agn_isnumber(L, 2)) {
      unsigned char n = lua_tointeger(L, 2) + '0';  /* integers 0 to 9 */
      charcell(L, cell) = n;
    } else
      luaL_error(L, "Error in " LUA_QS ": expected a number or string, got %s.", "stack.replaced", luaL_typename(L, 2));  /* 2.18.2 error message fix */
  }
  return 1;
}


/* Swaps the two elements at the given relative stack positions. The function returns nothing. Equals:
   stack.swapd := proc(i :: integer, j :: integer) is
      stack.replaced(j, stack.replaced(i, cell(j)))
   end; */
static int stack_swapd (lua_State *L) {  /* 2.28.2 */
  ptrdiff_t cell1, cell2;
  aux_checkstackindices(L, 1, 2, "stack.swapd");
  if (isnumstack(L)) {
    lua_Number t = cells(L, cell1);
    cells(L, cell1) = cells(L, cell2);
    cells(L, cell2) = t;
  } else if (iscachestack(L)) {  /* 2.37.0 */
    setobjs2s(L, L->C->top, L->C->base + cell1);
    setobjs2s(L, L->C->base + cell1, L->C->base + cell2);
    setobjs2s(L, L->C->base + cell2, L->C->top);
  } else {
    char t = charcell(L, cell1);
    charcell(L, cell1) = charcell(L, cell2);
    charcell(L, cell2) = t;
  }
  return 0;
}


static int stack_explored (lua_State *L) {  /* 2.12.7 */
  int i, n;
  n = stacktop(L);
  if (n == 0)
    lua_pushnil(L);
  else {
    if (isnumstack(L)) {
      agn_createseq(L, n);
      for (i=0; i < n; i++)
        agn_seqsetinumber(L, -1, i + 1, cells(L, i));
    } else if (iscachestack(L)) {  /* 2.37.0 */
      agn_createseq(L, n);
      for (i=0; i < n; i++)
        lua_seqseticachevalue(L, -1, i + 1, -stacktop(L) + i);  /* 2.37.2 tweak */
    } else
      lua_pushlstring(L, getcharcell(L, 0), n);
  }
  return 1;
}


static int stack_attribd (lua_State *L) {  /* 2.15.4 */
  lua_createtable(L, 0, 0);
  lua_rawsetstringnumber(L, -1, "currentstack", L->currentstack);
  lua_rawsetstringnumber(L, -1, "stacktop", stacktop(L));
  lua_rawsetstringnumber(L, -1, "stackmax", stackmax(L));
  lua_rawsetstringnumber(L, -1, "defaultsize", LUA_CELLSTACKSIZE);
  lua_rawsetstringboolean(L, -1, "isnumstack", isnumstack(L));
  lua_rawsetstringboolean(L, -1, "iscachestack", iscachestack(L));
  return 1;
}


#define assigntonextcharslot(L,n,option,s,procname) { \
  if (option) { \
    aux_checkstack(L, 1, procname); \
    stacktop(L)++; \
  } else { \
    stacktop(L) -= (n - 1); \
  } \
  charcell(L, stacktop(L) - 1) = s; \
}


#define assigntonextnumslot(L,n,option,s,procname) { \
  if (option) { \
    aux_checkstack(L, 1, procname); \
    stacktop(L)++; \
  } else { \
    stacktop(L) -= (n - 1); \
  } \
  cells(L, stacktop(L) - 1) = s; \
}


#define checkstack0(L,procname) { \
  if (L->currentstack == 0) \
    luaL_error(L, "Error in " LUA_QS ": cannot operate on stack 0.", procname); \
}

#define checkemptystack(L,csf,procname) { \
  csf = stacktop(L); \
  if (csf == 0) \
    luaL_error(L, "Error in " LUA_QS ": the stack is empty.", procname); \
}

#define checkoption(L,n,option) { \
  nargs = lua_gettop(L); \
  option = 0; \
  if ((nargs > 0) && lua_istrue(L, nargs)) { \
    lua_settop(L, nargs - 1); option = 1; nargs--; \
  }; \
}

static int stack_sumupd (lua_State *L) {  /* 2.28.2, sums up all or a given number of values in the built-in numeric stack and either pushes the
  result onto the top of the stack if `true` has been passed as the last argument; or pops all the values summed-up and pushes the result on top
  of the stack. Summation is done from top to bottom. The function uses the Kahan-Babuška algorithm to prevent round-off errors during summation
  and also returns the sum. */
  int i, n, csf, option, nargs;
  checkstack0(L, "stack.sumupd");
  checkemptystack(L, csf, "stack.sumupd");  /* csf = stacktop(L) */
  checkoption(L, nargs, option);
  n = luaL_optinteger(L, 1, csf - LUA_CELLSTACKBOTTOM);
  if (n < 0 || n > csf - LUA_CELLSTACKBOTTOM)
    luaL_error(L, "Error in " LUA_QS ": invalid argument.", "stack.sumupd");
  if (isnumstack(L)) {
    /* Kahan-Babuska */
    volatile lua_Number s, cs, ccs, t, c, cc, x;
    s = cs = ccs = 0;
    for (i=csf - n; i < csf; i++) {
      x = cells(L, i);
      t = s + x;
      c = (fabs(s) >= fabs(x)) ? (s - t) + x : (x - t) + s;
      s = t;
      t = cs + c;
      cc = (fabs(cs) >= fabs(c)) ? (cs - t) + c : (c - t) + cs;
      cs = t;
      ccs += cc;
    }
    s = (n == 0) ? AGN_NAN : s + cs + ccs;
    assigntonextnumslot(L, n, option, s, "stack.sumupd");
    lua_pushnumber(L, s);
  } else {
    luaL_error(L, "Error in " LUA_QS ": cannot operate on a character or cache stack.", "stack.sumupd");
  }
  return 1;
}


static int stack_mulupd (lua_State *L) {  /* 2.28.2, multiplies all or a given number of values in the built-in numeric stack and either pushes the
  result onto the top of the stack if `true` has been passed as the last argument; or pops all the values multiplied and pushes the result on top
  of the stack. Multiplication is done from top to bottom. The function in general also returns the product. */
  int i, n, csf, option, nargs;
  checkstack0(L, "stack.mulupd");
  checkemptystack(L, csf, "stack.mulupd");
  checkoption(L, nargs, option);
  n = luaL_optinteger(L, 1, csf - LUA_CELLSTACKBOTTOM);
  if (n < 0 || n > csf - LUA_CELLSTACKBOTTOM)
    luaL_error(L, "Error in " LUA_QS ": invalid argument.", "stack.mulupd");
  if (isnumstack(L)) {
    lua_Number s, x;
    s = 1;
    for (i=csf - n; i < csf; i++) {
      x = cells(L, i);
      s *= x;
    }
    s = (n == 0) ? AGN_NAN : s;
    assigntonextnumslot(L, n, option, s, "stack.mulupd");
    lua_pushnumber(L, s);
  } else {
    luaL_error(L, "Error in " LUA_QS ": cannot operate on a character or cache stack.", "stack.mulupd");
  }
  return 1;
}


static int stack_meand (lua_State *L) {  /* 2.28.2, computes the arithmetic mean of all or a given number of values in the built-in numeric stack and
  either pushes the result onto the top of the stack if `true` has been passed as the last argument; or pops all the values processed and pushes the
  result on top of the stack. The function in general also returns the arithmetic mean.

  By dividing each element before summation, the function avoids arithmetic overflows and also uses the Kahan-Babuška algorithm to prevent round-off
  errors during summation. Computation is done from top to bottom. */
  int i, n, csf, option, nargs;
  checkstack0(L, "stack.meand");
  checkemptystack(L, csf, "stack.meand");
  checkoption(L, nargs, option);
  n = luaL_optinteger(L, 1, csf - LUA_CELLSTACKBOTTOM);
  if (n < 0 || n > csf - LUA_CELLSTACKBOTTOM)
    luaL_error(L, "Error in " LUA_QS ": invalid argument.", "stack.meand");
  if (isnumstack(L)) {
    /* Kahan-Babuska */
    volatile lua_Number s, cs, ccs, t, c, cc, x;
    s = cs = ccs = 0;
    for (i=csf - n; i < csf; i++) {
      x = cells(L, i)/n;
      t = s + x;
      c = (fabs(s) >= fabs(x)) ? (s - t) + x : (x - t) + s;
      s = t;
      t = cs + c;
      cc = (fabs(cs) >= fabs(c)) ? (cs - t) + c : (c - t) + cs;
      cs = t;
      ccs += cc;
    }
    s = (n == 0) ? AGN_NAN : s + cs + ccs;
    assigntonextnumslot(L, n, option, s, "stack.meand");
    lua_pushnumber(L, s);
  } else {
    luaL_error(L, "Error in " LUA_QS ": cannot operate on a character or cache stack.", "stack.meand");
  }
  return 1;
}


static int stack_addtod (lua_State *L) {  /* 2.28.2, adds its argument to the top element of the current numeric stack. The top stack
  element is replaced by the resulting sum, but leaves the rest of the stack untouched. The function also returns the sum computed.
  The function uses the Kahan-Babuška algorithm to prevent round-off errors. */
  int i, csf, option, nargs;
  checkstack0(L, "stack.addtod");
  checkemptystack(L, csf, "stack.addtod");
  checkoption(L, nargs, option);
  if (isnumstack(L)) {
    /* Kahan-Babuska */
    volatile lua_Number s, cs, ccs, t, c, cc, x, y;
    s = cs = ccs = 0;
    y = agn_checknumber(L, 1);
    for (i=0 ; i < 2; i++) {
      x = (i == 0) ? cells(L, csf - 1) : y;
      t = s + x;
      c = (fabs(s) >= fabs(x)) ? (s - t) + x : (x - t) + s;
      s = t;
      t = cs + c;
      cc = (fabs(cs) >= fabs(c)) ? (cs - t) + c : (c - t) + cs;
      cs = t;
      ccs += cc;
    }
    s += cs + ccs;
    assigntonextnumslot(L, 1, option, s, "stack.addtod");
    lua_pushnumber(L, s);
  } else {
    luaL_error(L, "Error in " LUA_QS ": cannot operate on a character or cache stack.", "stack.addtod");
  }
  return 1;
}


static int stack_mulbyd (lua_State *L) {  /* 2.28.2, multiplies its argument by the top element of the current numeric stack. The top stack
  element is replaced by the resulting product, but leaves the rest of the stack untouched. The function also returns the product computed. */
  int csf, option, nargs;
  checkstack0(L, "stack.mulbyd");
  checkemptystack(L, csf, "stack.mulbyd");
  checkoption(L, nargs, option);
  if (isnumstack(L)) {
    lua_Number s, x, y;
    x = cells(L, csf - 1);
    y = agn_checknumber(L, 1);
    s = x * y;
    assigntonextnumslot(L, 1, option, s, "stack.mulbyd");
    lua_pushnumber(L, s);
  } else {
    luaL_error(L, "Error in " LUA_QS ": cannot operate on a character or cache stack.", "stack.mulbyd");
  }
  return 1;
}


static int stack_powd (lua_State *L) {  /* 2.28.2, raises the top element of the current numeric stack to the given power. The top stack
  element is replaced by the result, but leaves the rest of the stack untouched. The function also returns the result. */
  int csf, option, nargs;
  checkstack0(L, "stack.powd");
  checkemptystack(L, csf, "stack.powd");
  checkoption(L, nargs, option);
  if (isnumstack(L)) {
    lua_Number s, x, y;
    x = cells(L, csf - 1);
    y = agn_checknumber(L, 1);
    s = tools_isint(y) ? tools_intpow(x, y) : sun_pow(x, y, 1);
    assigntonextnumslot(L, 1, option, s, "stack.powd");
    lua_pushnumber(L, s);
  } else {
    luaL_error(L, "Error in " LUA_QS ": cannot operate on a character or cache stack.", "stack.powd");
  }
  return 1;
}


static int stack_squared (lua_State *L) {  /* 2.28.2, raises the top element of the current numeric stack to the power of two. The top stack
  element is replaced by the result, but leaves the rest of the stack untouched. The function also returns the result. */
  int csf, option, nargs;
  checkstack0(L, "stack.squared");
  checkemptystack(L, csf, "stack.squared");
  checkoption(L, nargs, option);
  if (isnumstack(L)) {
    lua_Number s;
    s = tools_intpow(cells(L, csf - 1), 2);
    assigntonextnumslot(L, 1, option, s, "stack.squared");
    lua_pushnumber(L, s);
  } else {
    luaL_error(L, "Error in " LUA_QS ": cannot operate on a character or cache stack.", "stack.squared");
  }
  return 1;
}


static int stack_rootd (lua_State *L) {  /* 2.28.2, computes the non-principal n-th root of the top element of the current numeric
  stack. n must be an integer and is 2 by default. The top stack element is replaced by the result, but leaves the rest of the stack
  unchanged. The function also returns the result. */
  int csf, option, nargs;
  checkstack0(L, "stack.rootd");
  checkemptystack(L, csf, "stack.rootd");
  checkoption(L, nargs, option);
  if (isnumstack(L)) {
    lua_Number s, x, n;
    x = cells(L, csf - 1);
    n = agnL_optposint(L, 1, 2);
    if (x == 0 && n < 0)
      s = AGN_NAN;
    else if (x >= 0)
      s = sun_pow(x, 1/n, 0);
    else if (luai_numiseven(n))  /* x < 0 and n is even ? */
      s = AGN_NAN;
    else
      s = -sun_pow(-x, 1/n, 0);
    assigntonextnumslot(L, 1, option, s, "stack.rootd");
    lua_pushnumber(L, s);
  } else {
    luaL_error(L, "Error in " LUA_QS ": cannot operate on a character or cache stack.", "stack.rootd");
  }
  return 1;
}


static int stack_logd (lua_State *L) {  /* 2.28.2, returns the logarithm of the top element of the current numeric stack to the
  given base b. The top stack element is replaced by the result, but leaves the rest of the stack unchanged. The function also
  returns the result. */
  int csf, option, nargs;
  checkstack0(L, "stack.logd");
  checkemptystack(L, csf, "stack.logd");
  checkoption(L, nargs, option);
  if (isnumstack(L)) {
    lua_Number s, x, b;
    x = cells(L, csf - 1);
    b = agn_checknumber(L, 1);
    if (x == 1 && b < 0)
      s = 0;
    else if (x == b && (x < 0 || x > 1))
      s = 1;
    else if (x <= 0 || b <= 0 || b == 1)
      s = AGN_NAN;
    else
      s = sun_log(x)/sun_log(b);
    assigntonextnumslot(L, 1, option, s, "stack.logd");
    lua_pushnumber(L, s);
  } else {
    luaL_error(L, "Error in " LUA_QS ": cannot operate on a character or cache stack.", "stack.logd");
  }
  return 1;
}


static int stack_antilogd (lua_State *L) {  /* 2.28.2, raises the given base to the power of the top element of the current numeric stack.
  The top stack element is replaced by the result, but leaves the rest of the stack untouched. The function also returns the result. */
  int csf, option, nargs;
  checkstack0(L, "stack.antilogd");
  checkemptystack(L, csf, "stack.antilogd");
  checkoption(L, nargs, option);
  if (isnumstack(L)) {
    lua_Number s, b, y;
    b = agn_checknumber(L, 1);  /* the base */
    y = cells(L, csf - 1);  /* the exponent */
    s = sun_pow(b, y, tools_isint(y));  /* 2.29.5 optimisation */
    assigntonextnumslot(L, 1, option, s, "stack.antilogd");
    lua_pushnumber(L, s);
  } else {
    luaL_error(L, "Error in " LUA_QS ": cannot operate on a character or cache stack.", "stack.antilogd");
  }
  return 1;
}


static int stack_modd (lua_State *L) {  /* 2.28.2, computes the modulus of the top element of the current numeric stack and its argument.
  The top stack element is replaced by the result, but leaves the rest of the stack untouched. The function also returns the result. */
  int csf, option, nargs;
  checkstack0(L, "stack.modd");
  checkemptystack(L, csf, "stack.modd");
  checkoption(L, nargs, option);
  if (isnumstack(L)) {
    lua_Number s, x, y;
    x = cells(L, csf - 1);
    y = agn_checknumber(L, 1);
    s = luai_nummod(x, y);
    assigntonextnumslot(L, 1, option, s, "stack.modd");
    lua_pushnumber(L, s);
  } else {
    luaL_error(L, "Error in " LUA_QS ": cannot operate on a character or cache stack.", "stack.modd");
  }
  return 1;
}


static int stack_recipd (lua_State *L) {  /* 2.28.2, computes the reciprocal of the top element of the current numeric stack.
  The top stack element is replaced by the result, but leaves the rest of the stack untouched. The function also returns the result. */
  int csf, option, nargs;
  checkstack0(L, "stack.recipd");
  checkemptystack(L, csf, "stack.recipd");
  checkoption(L, nargs, option);
  if (isnumstack(L)) {
    lua_Number s, x;
    x = cells(L, csf - 1);
    s = luai_numrecip(x);
    assigntonextnumslot(L, 1, option, s, "stack.recipd");
    lua_pushnumber(L, s);
  } else {
    luaL_error(L, "Error in " LUA_QS ": cannot operate on a character or cache stack.", "stack.recipd");
  }
  return 1;
}


static int stack_mapd (lua_State *L) {  /* 2.28.2, maps a function f - the first argument - on the top element of the current numeric stack.
  If f is a multivariate function, its second, third, etc. argument must be passed right after f. The top stack element is replaced by the
  result of the function call, but leaves the rest of the stack untouched. The function also returns the result of the function call. */
  int csf, nargs, i, idx, error, push, checkoptions, offset, allstack;
  checkstack0(L, "stack.mapd");
  checkemptystack(L, csf, "stack.mapd");
  checkoption(L, nargs, push);
  allstack = lua_istrue(L, 1);
  offset = nargs > 0 && (agn_isnumber(L, 1) || allstack);  /* offset to fetch function, etc.; 2.28.6, 2.36.0 */
  luaL_checktype(L, 1 + offset, LUA_TFUNCTION);
  idx = offset ? agn_tonumber(L, 1) : -1;  /* operation to be applied on the |idx|'th element of the stack, counting from the top */
  /* check for options */
  checkoptions = 3;  /* check n options; 2.36.0 fix to get more than one option */
  luaL_checkstack(L, nargs < 2 ? 2 : nargs, "too many arguments");  /* 3.15.2 fix */
  while (checkoptions-- && nargs != 0 && lua_ispair(L, nargs)) {
    agn_pairgeti(L, nargs, 1);  /* get left value, set to stack index -2 */
    agn_pairgeti(L, nargs, 2);  /* get right value, set to stack index  -1 */
    if (agn_isstring(L, -2)) {
      const char *option = agn_tostring(L, -2);
      if (tools_streq("index", option)) {  /* 2.28.6 */
        nargs--;
        idx = agn_checkinteger(L, -1);
      } else if (tools_streq("push", option)) {
        nargs--;
        push = agn_checkboolean(L, -1);
      } else if (tools_streq("allstack", option)) {
        nargs--;
        allstack = agn_checkboolean(L, -1);
      } else {
        agn_poptoptwo(L);
        luaL_error(L, "Error in " LUA_QS ": unknown option " LUA_QS ".", "stack.mapd", option);
      }
    }
    agn_poptoptwo(L);
  }
  if (allstack) {  /* 2.36.0 */
    lua_Debug ar;
    int nparams;
    lua_pushvalue(L, 1 + offset); /* push function first before arity check */
    if (lua_arity(L, &ar))  /* gets arity of the value at the stack top */
      luaL_error(L, "Error in " LUA_QS ": could not determine number of parameters for given function.", "stack.mapd");
    nparams = ar.arity;
    if (nparams > stacktop(L))
      luaL_error(L, "Error in " LUA_QS ": function has %d parameters but there are %d stack values only.", "stack.mapd", nparams, stacktop(L));
    if (ar.hasvararg) nparams = stacktop(L);
    luaL_checkstack(L, nparams + 1, "not enough stack space");
    if (isnumstack(L)) {
      lua_Number s;
      for (idx=nparams; idx > 0; idx--) {
        lua_pushnumber(L, cells(L, csf - idx));
      }
      s = agn_ncall(L, nparams, &error, 1);
      if (!push) {  /* remove nparams values from the top, push result */
        stacktop(L) -= nparams;
        cells(L, stacktop(L)++) = s;
      } else {  /* push result on top */
        aux_checkstack(L, 1, "stack.mapd");
        stacktop(L)++;
        cells(L, stacktop(L) - 1) = s;
      }
      lua_pushnumber(L, s);
    } else if (iscachestack(L)) {  /* 2.37.0 */
      for (idx=nparams; idx > 0; idx--) {
        lua_pushcachevalue(L, stacktop(L) - idx);
      }
      lua_call(L, nparams, 1);
      if (!push) {  /* remove nparams values from the top, push result */
        stacktop(L) -= nparams;
        aux_nullifycache(L, nparams);  /* 2.37.2 */
        L->C->top -= nparams;
      } else {  /* push result on top */
        aux_checkstack(L, 1, "stack.mapd");
      }
      setobj2s(L->C, L->C->top++, L->top - 1);
      stacktop(L)++;
    } else {  /* character stack */
      const char *s;
      for (idx=nparams; idx > 0; idx--) {
        lua_pushchar(L, charcell(L, csf - idx));
      }
      lua_call(L, nparams, 1);
      if (!agn_isstring(L, -1))
        luaL_error(L, "Error in " LUA_QS ": function call did not evaluate to a string, got %s.", "stack.mapd", luaL_typename(L, -1));
      s = agn_tostring(L, -1);
      if (!push) {  /* overwrite top element */
        stacktop(L) -= nparams;
        charcell(L, stacktop(L)++) = s[0];
      } else {  /* push result on top */
        aux_checkstack(L, 1, "stack.mapd");
        stacktop(L)++;
        charcell(L, stacktop(L) - 1) = s[0];
      }
    }
  } else {
    if (aux_getabsindex(L, &idx, 0, "stack.mapd"))
      luaL_error(L, "Error in " LUA_QS ": index %d is out-of-range or empty stack.", "stack.mapd", idx);
    lua_settop(L, nargs);  /* remove options from argument stack */
    /* end of option check */
    lua_pushvalue(L, 1 + offset);  /* push function, 2.37.0 change */
    if (isnumstack(L)) {
      lua_Number s;
      lua_pushnumber(L, cells(L, idx));
      for (i=2 + offset; i <= nargs; i++) lua_pushvalue(L, i);
      s = agn_ncall(L, nargs - offset, &error, 1);
      if (!push) {  /* overwrite indexed element */
        cells(L, idx) = s;  /* 2.28.6 */
      } else {  /* push result on top */
        aux_checkstack(L, 1, "stack.mapd");
        stacktop(L)++;
        cells(L, stacktop(L) - 1) = s;
      }
      lua_pushnumber(L, s);
    } else if (iscachestack(L)) {  /* 2.37.0 */
      int top = stacktop(L);
      lua_pushcachevalue(L, idx);
      for (i=2 + offset; i <= nargs; i++) lua_pushvalue(L, i);
      lua_call(L, nargs - offset, 1);
      if (!push) {  /* overwrite indexed element */
        setobj2s(L->C, L->C->top - (top - idx), L->top - 1);
      } else {  /* push result on top */
        aux_checkstack(L, 1, "stack.mapd");
        setobj2s(L->C, L->C->top++, L->top - 1);
        stacktop(L)++;
      }
    } else {
      const char *s;
      char c[2];
      c[0] = charcell(L, idx);
      c[1] = '\0';
      lua_pushlstring(L, c, 1);
      for (i=2 + offset; i <= nargs; i++) lua_pushvalue(L, i);
      lua_call(L, nargs - offset, 1);
      if (!agn_isstring(L, -1))
        luaL_error(L, "Error in " LUA_QS ": function call did not evaluate to a string, got %s.", "stack.mapd", luaL_typename(L, -1));
      s = agn_tostring(L, -1);
      if (!push) {  /* overwrite top element */
        charcell(L, idx) = s[0];
      } else {  /* 2.28.6; push result on top */
        aux_checkstack(L, 1, "stack.mapd");
        stacktop(L)++;
        charcell(L, stacktop(L) - 1) = s[0];
      }
      lua_pushlstring(L, s, 1);
    }
  }
  return 1;
}


#define computenumfunc(L,fn,procname) { \
  int csf, option, nargs; \
  checkstack0(L, procname); \
  checkemptystack(L, csf, procname); \
  checkoption(L, nargs, option); \
  if (isnumstack(L)) { \
    lua_Number s, x; \
    x = cells(L, csf - 1); \
    s = fn(x); \
    assigntonextnumslot(L, 1, option, s, procname); \
    lua_pushnumber(L, s); \
  } else { \
    luaL_error(L, "Error in " LUA_QS ": cannot operate on a character or cache stack.", procname); \
  } \
}


static int stack_sqrtd (lua_State *L) {  /* 2.28.2, computes the root of the top element of the current numeric stack. The top stack
  element is replaced by the result, but leaves the rest of the stack unchanged. The function also returns the result. */
  computenumfunc(L, sqrt, "stack.sqrtd");
  return 1;
}

static int stack_cbrtd (lua_State *L) {  /* 2.36.0, computes the cubic root of the top element of the current numeric stack. The top stack
  element is replaced by the result, but leaves the rest of the stack unchanged. The function also returns the result. */
  computenumfunc(L, sun_cbrt, "stack.cbrtd");
  return 1;
}

static int stack_lnd (lua_State *L) {  /* 2.28.2, computes the natural logarithm of the top element of the current numeric stack. The top stack
  element is replaced by the result, but leaves the rest of the stack unchanged. The function also returns the result. */
  computenumfunc(L, sun_log, "stack.lnd");
  return 1;
}

static int stack_expd (lua_State *L) {  /* 2.29.1, raises E to the power of the top element of the current numeric stack. The top stack
  element is replaced by the result, but leaves the rest of the stack unchanged. The function also returns the result. */
  computenumfunc(L, sun_exp, "stack.expd");
  return 1;
}

static int stack_exp2d (lua_State *L) {  /* 2.29.1, raises 2 to the power of the top element of the current numeric stack. The top stack
  element is replaced by the result, but leaves the rest of the stack unchanged. The function also returns the result. */
  computenumfunc(L, tools_exp2, "stack.exp2d");
  return 1;
}

static int stack_exp10d (lua_State *L) {  /* 2.29.1, raises 10 to the power of the top element of the current numeric stack. The top stack
  element is replaced by the result, but leaves the rest of the stack unchanged. The function also returns the result. */
  computenumfunc(L, exp10, "stack.exp10d");
  return 1;
}

static int stack_erfd (lua_State *L) {  /* 2.36.0, computes the error function of the top element of the current numeric stack.
  The top stack element is replaced by the result, but leaves the rest of the stack unchanged. The function also returns the result. */
  computenumfunc(L, sun_erf, "stack.erfd");
  return 1;
}

static int stack_sind (lua_State *L) {  /* 2.28.2, computes the sine of the top element of the current numeric stack in radians.
  The top stack element is replaced by the result, but leaves the rest of the stack unchanged. The function also returns the result. */
  computenumfunc(L, sun_sin, "stack.sind");
  return 1;
}

static int stack_cosd (lua_State *L) {  /* 2.28.2, computes the cosine of the top element of the current numeric stack in radians.
  The top stack element is replaced by the result, but leaves the rest of the stack unchanged. The function also returns the result. */
  computenumfunc(L, sun_cos, "stack.cosd");
  return 1;
}

static int stack_tand (lua_State *L) {  /* 2.28.2, computes the tangent of the top element of the current numeric stack in radians.
  The top stack element is replaced by the result, but leaves the rest of the stack unchanged. The function also returns the result. */
  computenumfunc(L, sun_tan, "stack.tand");
  return 1;
}

static int stack_sinhd (lua_State *L) {  /* 2.36.0, computes the hyperbolic sine of the top element of the current numeric stack in radians.
  The top stack element is replaced by the result, but leaves the rest of the stack unchanged. The function also returns the result. */
  computenumfunc(L, sun_sinh, "stack.sinhd");
  return 1;
}

static int stack_coshd (lua_State *L) {  /* 2.36.0, computes the hyperbolic  cosine of the top element of the current numeric stack in radians.
  The top stack element is replaced by the result, but leaves the rest of the stack unchanged. The function also returns the result. */
  computenumfunc(L, sun_cosh, "stack.coshd");
  return 1;
}

static int stack_tanhd (lua_State *L) {  /* 2.36.0, computes the hyperbolic tangent of the top element of the current numeric stack in radians.
  The top stack element is replaced by the result, but leaves the rest of the stack unchanged. The function also returns the result. */
  computenumfunc(L, sun_tanh, "stack.tanhd");
  return 1;
}

static int stack_cotd (lua_State *L) {  /* 3.14.4, computes the cotangent of the top element of the current numeric stack in radians.
  The top stack element is replaced by the result, but leaves the rest of the stack unchanged. The function also returns the result. */
  computenumfunc(L, luai_numcot, "stack.cotd");
  return 1;
}

static int stack_cscd (lua_State *L) {  /* 3.14.4, computes the cosecant of the top element of the current numeric stack in radians.
  The top stack element is replaced by the result, but leaves the rest of the stack unchanged. The function also returns the result. */
  computenumfunc(L, luai_numcsc, "stack.cscd");
  return 1;
}

static int stack_secd (lua_State *L) {  /* 3.14.4, computes the cosecant of the top element of the current numeric stack in radians.
  The top stack element is replaced by the result, but leaves the rest of the stack unchanged. The function also returns the result. */
  computenumfunc(L, luai_numsec, "stack.secd");
  return 1;
}

static int stack_arcsind (lua_State *L) {  /* 2.28.2, computes the arcus sine of the top element of the current numeric stack in radians.
  The top stack element is replaced by the result, but leaves the rest of the stack unchanged. The function also returns the result. */
  computenumfunc(L, sun_asin, "stack.arcsind");
  return 1;
}

static int stack_arccosd (lua_State *L) {  /* 2.28.2, computes the arcus cosine of the top element of the current numeric stack in radians.
  The top stack element is replaced by the result, but leaves the rest of the stack unchanged. The function also returns the result. */
  computenumfunc(L, sun_acos, "stack.arccosd");
  return 1;
}

static int stack_arctand (lua_State *L) {  /* 2.28.2, computes the arcus tangent of the top element of the current numeric stack in radians.
  The top stack element is replaced by the result, but leaves the rest of the stack unchanged. The function also returns the result. */
  computenumfunc(L, sun_atan, "stack.arctand");
  return 1;
}

static int stack_arcsinhd (lua_State *L) {  /* 2.36.0, computes the inverse arcus sine of the top element of the current numeric stack in radians.
  The top stack element is replaced by the result, but leaves the rest of the stack unchanged. The function also returns the result. */
  computenumfunc(L, sun_asinh, "stack.arcsinhd");
  return 1;
}

static int stack_arccoshd (lua_State *L) {  /* 2.36.0, computes the inverse arcus cosine of the top element of the current numeric stack in radians.
  The top stack element is replaced by the result, but leaves the rest of the stack unchanged. The function also returns the result. */
  computenumfunc(L, acosh, "stack.arccoshd");
  return 1;
}

static int stack_arctanhd (lua_State *L) {  /* 2.36.0, computes the inverse arcus tangent of the top element of the current numeric stack in radians.
  The top stack element is replaced by the result, but leaves the rest of the stack unchanged. The function also returns the result. */
  computenumfunc(L, atanh, "stack.arctanhd");
  return 1;
}

static int stack_negated (lua_State *L) {  /* 2.29.1, negates the top element, but leaves the rest of the stack unchanged. The function also
  returns the result. */
  computenumfunc(L, luai_numunm, "stack.negated");
  return 1;
}

static int stack_intd (lua_State *L) {  /* 2.29.1, rounds the number on the stack top to the nearest integer towards 0. The function also
  returns the result.*/
  computenumfunc(L, sun_trunc, "stack.intd");
  return 1;
}

static int stack_fracd (lua_State *L) {  /* 2.29.1, converts the number on the stack top to its fractional part. The function also
  returns the result. */
  computenumfunc(L, luai_numfrac, "stack.fracd");
  return 1;
}

static int stack_absd (lua_State *L) {  /* 2.28.2, computes the absolute value of the top element of the current numeric stack.
  The top stack element is replaced by the result, but leaves the rest of the stack unchanged. The function also returns the result. */
  if (isnumstack(L)) {
    computenumfunc(L, fabs, "stack.absd");
  } else if (iscachestack(L)) {
    luaL_error(L, "Error in " LUA_QS ": cache stack not supported.", "?");
  } else {
    int csf, option, nargs;
    (void)option;
    checkstack0(L, "stack.absd");
    checkemptystack(L, csf, "stack.absd");
    checkoption(L, nargs, option);
    lua_pushinteger(L, (unsigned char)charcell(L, csf - 1));
  }
  return 1;
}

/* Operates on top element and a given argument */
static FORCE_INLINE void computenumfunconearg (lua_State *L, lua_Number (*fn)(lua_Number, lua_Number), const char *procname) {
  int csf, option, nargs;
  checkstack0(L, procname);
  checkemptystack(L, csf, procname);
  checkoption(L, nargs, option);
  if (isnumstack(L)) {
    lua_Number s, x, y;
    x = cells(L, csf - 1);
    y = agn_checknumber(L, 1);
    s = (*fn)(x, y);
    assigntonextnumslot(L, 1, option, s, procname);
    lua_pushnumber(L, s);
  } else {
    luaL_error(L, "Error in " LUA_QS ": cannot operate on a character or cache stack.", procname);
  }
}

static int stack_intdivd (lua_State *L) {  /* 2.28.2, integer-divides the top element of the current numeric stack by its argument.
  The top stack element is replaced by the result, but leaves the rest of the stack untouched. The function also returns the result. */
  computenumfunconearg(L, sun_intdiv, "stack.intdivd");
  return 1;
}


/* Operates on top element only, no argument, 2.28.3 */
static FORCE_INLINE void computecharfuncnoarg (lua_State *L, int (*fn)(int), const char *procname) {
  int csf, option, nargs;
  checkstack0(L, procname);
  checkemptystack(L, csf, procname);
  checkoption(L, nargs, option);
  if (isnumstack(L) || iscachestack(L)) {
    luaL_error(L, "Error in " LUA_QS ": cannot operate on a numeric or cache stack.", procname);
  } else {
    char s, x;
    x = charcell(L, csf - 1);
    s = (*fn)(x);
    assigntonextcharslot(L, 1, option, s, procname);
    lua_pushchar(L, s);
  }
}

static int stack_upperd (lua_State *L) {  /* 2.28.2, integer-divides the top element of the current numeric stack by its argument.
  The top stack element is replaced by the result, but leaves the rest of the stack untouched. The function also returns the result. */
  computecharfuncnoarg(L, toupper, "stack.upperd");
  return 1;
}

static int stack_lowerd (lua_State *L) {  /* 2.28.2, integer-divides the top element of the current numeric stack by its argument.
  The top stack element is replaced by the result, but leaves the rest of the stack untouched. The function also returns the result. */
  computecharfuncnoarg(L, toupper, "stack.lowerd");
  return 1;
}


/* based on binio.readbytes, 2.28.6 */
static int stack_readbytes (lua_State *L) {
  int32_t n;
  size_t ignorelen;
  int hnd, res, en, nargs, eof, checkoptions, oldstacknr, nrets;
  unsigned char character;
  char *ignore = tools_strdup("\n\r");
  if (!ignore) luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "stack.readbytes");  /* 7.7.7 fix */
  ignorelen = tools_strlen(ignore);
  nargs = lua_gettop(L);
  hnd = agn_tofileno(L, 1, 0);
  if (hnd == -1) luaL_error(L, "Error in " LUA_QS ": file handle is invalid or closed.", "stack.readbytes");  /* 2.37.5 */
  nrets = 1;
  eof = 0;
  /* check for options */
  oldstacknr = L->currentstack;
  checkstack0(L, "stack.readbytes");
  checkoptions = 3;  /* check three options */
  while (checkoptions-- && nargs != 0 && lua_ispair(L, nargs)) {
    luaL_checkstack(L, 2, "too many arguments");
    agn_pairgeti(L, nargs, 1);  /* get left value, set to stack index -2 */
    agn_pairgeti(L, nargs, 2);  /* get right value, set to stack index  -1 */
    if (agn_isstring(L, -2)) {
      const char *option = agn_tostring(L, -2);
      if (tools_streq("stack", option)) {
        int tostack;
        nargs--;
        tostack = agn_checkposint(L, -1);
        if (tostack == 7) {  /* 2.37.0 */
          agn_poptoptwo(L);
          luaL_error(L, "Error in " LUA_QS ": cache stacks are not supported.", "stack.readbytes", tostack);
        } else if (tostack > 7) {
          agn_poptoptwo(L);
          luaL_error(L, "Error in " LUA_QS ": stack number %d does not exist.", "stack.readbytes", tostack);
        }
        L->currentstack = tostack;
      } else if (tools_streq("ignore", option)) {
        nargs--;
        xfree(ignore);  /* 5.3.1 fix */
        ignore = tools_strdup(agn_checklstring(L, -1, &ignorelen));
        if (!ignore) luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "stack.readbytes");  /* 7.7.7 fix */
      } else if (tools_streq("eof", option)) {
        nargs--;
        eof = agn_checkboolean(L, -1);
      } else {
        xfree(ignore);
        agn_poptoptwo(L);
        luaL_error(L, "Error in " LUA_QS ": unknown option " LUA_QS ".", "stack.readbytes", option);
      }
    }
    agn_poptoptwo(L);
  }
  lua_settop(L, nargs);  /* remove options from argument stack */
  /* end of option check */
  n = agnL_optnumber(L, 2, L->buffersize);
  if (n < 1) n = L->buffersize;  /* 2.34.9 adaption */
  unsigned char buffer[n];
  set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
  res = read(hnd, buffer, n);
  en = errno;
  if (res > 0) {
    int i, j, oldstacktop;
    oldstacktop = stacktop(L);
    aux_checkstack(L, res, "stack.readbytes");
    for (i=0; i < res; i++) {
      character = buffer[i];  /* 2.6.1 extension */
      if (eof && character == 0) {  /* bail out if en embedded zero has been found ? */
        set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
        if (lseek64(hnd, i - res + 1, SEEK_CUR) == -1) {  /* reset file position to embedded zero */
          L->currentstack = oldstacknr;
          en = errno;
          xfree(ignore);
          luaL_error(L, "Error in " LUA_QS " with file #%d: %s.", "stack.readbytes", hnd, my_ioerror(en));
        }
        break;  /* if an embedded zero has been found, quit */
      }
      if (ignorelen != 0) {
        int flag = 0;
        for (j=0; j < ignorelen; j++) {
          if (ignore[j] == character) {
            flag = 1;
            break;
          }
        }
        if (flag) continue;  /* ignore read character */
      }
      stacktop(L)++;
      if (isnumstack(L)) {
        cells(L, stacktop(L) - 1) = (lua_Number)character;
      } else if (iscachestack(L)) {
        xfree(ignore);
        luaL_error(L, "Error in " LUA_QS ": cache stack not supported.", "?");
      } else {
        charcell(L, stacktop(L) - 1) = uchar(character);
      }
    }
    lua_pushinteger(L, res);
    lua_pushinteger(L, stacktop(L) - oldstacktop);
    nrets++;
  } else if (res == 0) {  /* end of file reached */
    lua_pushnil(L);
  } else {
    L->currentstack = oldstacknr;
    xfree(ignore);
    luaL_error(L, "Error in " LUA_QS " with file #%d: %s.", "stack.readbytes", hnd, my_ioerror(en));
  }
  L->currentstack = oldstacknr;
  xfree(ignore);
  return nrets;
}


/* based on binio.writebytes & binio.writenumber, 2.28.6 */
static int stack_writebytes (lua_State *L) {
  int hnd, en;
  size_t size, i, c, nbytes;
  hnd = agn_tofileno(L, 1, 0);
  if (hnd == -1) luaL_error(L, "Error in " LUA_QS ": file handle is invalid or closed.", "stack.writebytes");  /* 2.37.5 */
  nbytes = agnL_optposint(L, 2, stacktop(L));
  if (nbytes == 0 || nbytes > stacktop(L))
    lua_pushfail(L);  /* stack is empty or more bytes requested than on the stack */
  else {
    if (ischarstack(L)) {
      unsigned char buffer[nbytes];
      size = sizeof(buffer);
      for (i=nbytes, c=0; i > 0; i--, c++) {
        buffer[c] = charcell(L, stacktop(L) - i);
      }
      set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
      if (write(hnd, &buffer, size) != (ssize_t)size) {
        en = errno;
        luaL_error(L, "Error in " LUA_QS " with file #%d: %s.", "stack.writebytes", hnd, my_ioerror(en));
      }
    } else if (iscachestack(L)) {
      luaL_error(L, "Error in " LUA_QS ": cache stack not supported.", "?");
    } else {
      uint64_t ulong;
      double_cast x;
      size = sizeof(ulong);
      for (i=nbytes; i > 0; i--) {
        x.f = cells(L, stacktop(L) - i);
        #if BYTE_ORDER != BIG_ENDIAN
        x.i = tools_swapu64(x.i);  /* swap Little Endian bytes only */
        #endif
        ulong = x.i;
        set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
        if (write(hnd, &ulong, size) != (ssize_t)size) {
          en = errno;
          luaL_error(L, "Error in " LUA_QS " with #%d: %s.", "stack.writebytes", hnd, my_ioerror(en));
        }
      }
    }
    lua_pushtrue(L);
  }
  return 1;
}


#define mathfntoptwo(L,fn,procname) { \
  int csf; \
  checkstack0(L, procname); \
  csf = stacktop(L); \
  if (2 > csf - LUA_CELLSTACKBOTTOM) \
    luaL_error(L, "Error in " LUA_QS ": need two elements on the stack, got %d.", procname, csf - LUA_CELLSTACKBOTTOM); \
  if (isnumstack(L)) { \
    volatile lua_Number s; \
    s = fn(cells(L, csf - 2), cells(L, csf - 1)); \
    stacktop(L)--; \
    cells(L, stacktop(L) - 1) = s; \
    lua_pushnumber(L, s); \
  } else { \
    luaL_error(L, "Error in " LUA_QS ": cannot operate on a character or cache stack.", procname); \
  } \
  return 1; \
}


#define mathfntopthree(L,fn,procname) { \
  int csf; \
  checkstack0(L, procname); \
  csf = stacktop(L); \
  if (3 > csf - LUA_CELLSTACKBOTTOM) \
    luaL_error(L, "Error in " LUA_QS ": need three elements on the stack, got %d.", procname, csf - LUA_CELLSTACKBOTTOM); \
  if (isnumstack(L)) { \
    volatile lua_Number s; \
    s = fn(cells(L, csf - 3), cells(L, csf - 2), cells(L, csf - 1)); \
    stacktop(L)--; \
    stacktop(L)--; \
    cells(L, stacktop(L) - 1) = s; \
    lua_pushnumber(L, s); \
  } else { \
    luaL_error(L, "Error in " LUA_QS ": cannot operate on a character or cache stack.", procname); \
  } \
  return 1; \
}


/* Adds up the two numbers on top of the current stack and replaces them with the result; also returns the sum. 2.29.1 */
static int stack_addtwod (lua_State *L) {
  mathfntoptwo(L, luai_numadd, "addtwod");
}

/* Subtracts the number on top of the current stack from the number below it and replaces them with the result; also returns the difference. 2.29.1 */
static int stack_subtwod (lua_State *L) {
  mathfntoptwo(L, luai_numsub, "subtwod");
}

/* Multiplies the number on top of the current stack by the number below it and replaces them with the result; also returns the product. 2.29.1 */
static int stack_multwod (lua_State *L) {
  mathfntoptwo(L, luai_nummul, "multwod");
}

/* Divides the number below the stack top by the number on the top and replaces them with the result; also returns the quotient. 2.29.1 */
static int stack_divtwod (lua_State *L) {
  mathfntoptwo(L, luai_numdiv, "divtwod");
}

/* Performs an integer division of the number below the stack top by the number on the top and replaces them with the result; also returns
   the integer quotient. 2.29.1 */
static int stack_intdivtwod (lua_State *L) {
  mathfntoptwo(L, luai_numintdiv, "intdivtwod");
}

/* Like `stack.intdivtwod but computes the modulus. 2.29.1 */
static int stack_modtwod (lua_State *L) {
  mathfntoptwo(L, luai_nummod, "modtwod");
}

/* Raises the number below the stack top to the power on the top and replaces both numbers with the result; also returns the power. 2.36.0 */
static int stack_powtwod (lua_State *L) {
  mathfntoptwo(L, luai_numpow, "powtwod");
}

/* Computes the hypotenuse of the number below the stack top and the number on the top and replaces them with the result; also returns
   the computed result. The function avoids over- and underflows and treats subnormal numbers accordingly. 2.36.0 */
static int stack_hypotd (lua_State *L) {
  mathfntoptwo(L, sun_hypot, "hypotd");
}

/* Computes sqrt(x^2 - y^2), where x is the number below the stack top and y the number on the top and replaces them with the result;
   also returns the computed result. The function avoids over- and underflow and treats subnormal numbers accordingly. 2.36.0 */
static int stack_cathetd (lua_State *L) {
  mathfntoptwo(L, tools_mhypot, "cathetd");
}

/* Computes 1/sqrt(x^2 + y^2) = 1/hypot(x, y), where x is the number below the stack top and y the number on the top and replaces them
   with the result; the function is protected against underflow and overflow. 2.36.0 */
static int stack_invhypotd (lua_State *L) {
  mathfntoptwo(L, tools_invhypot, "invhypotd");
}

/* Computes the Pythagorean equation c^2 = a^2 + b^2, without undue underflow or overflow. 2.36.0 */
static int stack_pythad (lua_State *L) {
  mathfntoptwo(L, sun_pytha, "pythad");
}

/* Computes the arc tangent of y/x where y denotes the number below the stack top and x the number on the top and replaces them
   with the result; also returns the computed result. 2.36.0 */
static int stack_arctan2d (lua_State *L) {
  mathfntoptwo(L, sun_atan2, "arctan2d");
}

/* fused multiply-add operation for the top three elements on the top; replaces them with the result; also returns the computed
   result. 2.36.0 */
static int stack_fmad (lua_State *L) {
  mathfntopthree(L, fma, "fmad");
}


/* }====================================================== */

static const luaL_Reg stack_funcs[] = {
  /* stack queries and manipulation */
  {"absindex", stack_absindex},             /* added August 05, 2025 */
  {"attribd", stack_attribd},               /* added July 01, 2019 */
  {"choosed", stack_choosed},               /* added January 13, 2016 */
  {"dequeued", stack_dequeued},             /* added November 01, 2020 */
  {"dumpd", stack_dumpd},                   /* added November 29, 2015 */
  {"enqueued", stack_enqueued},             /* added November 01, 2020 */
  {"explored", stack_explored},             /* added August 11, 2018 */
  {"insertd", stack_insertd},               /* added November 29, 2015 */
  {"isempty", stack_isempty},               /* added August 05, 2025 */
  {"pushvalued", stack_pushvalued},         /* added November 29, 2015 */
  {"pushstringd", stack_pushstringd},       /* added August 11, 2018 */
  {"readbytes", stack_readbytes},           /* added June 16, 2022 */
  {"removed", stack_removed},               /* added November 29, 2015 */
  {"replaced", stack_replaced},             /* added August 11, 2018 */
  {"resetd", stack_resetd},                 /* added November 30, 2015 */
  {"reversed", stack_reversed},             /* added November 30, 2015 */
  {"rotated", stack_rotated},               /* added November 30, 2015 */
  {"selected", stack_selected},             /* added January 11, 2016 */
  {"shrinkd", stack_shrinkd},               /* added November 30, 2015 */
  {"sized", stack_sized},                   /* added November 30, 2015 */
  {"sorted", stack_sorted},                 /* added January 03, 2016 */
  {"swapd", stack_swapd},                   /* added June 06, 2022 */
  {"switchto", stack_switchto},             /* added January 04, 2019 */
  {"writebytes", stack_writebytes},         /* added June 17, 2022 */
  /* math functions */
  {"absd", stack_absd},                     /* added June 06, 2022 */
  {"addtod", stack_addtod},                 /* added June 05, 2022 */
  {"antilogd", stack_antilogd},             /* added June 05, 2022 */
  {"arccosd", stack_arccosd},               /* added June 06, 2022 */
  {"arccoshd", stack_arccoshd},             /* added January 24, 2023 */
  {"arcsind", stack_arcsind},               /* added June 06, 2022 */
  {"arcsinhd", stack_arcsinhd},             /* added January 24, 2023 */
  {"arctand", stack_arctand},               /* added June 06, 2022 */
  {"arctanhd", stack_arctanhd},             /* added January 24, 2023 */
  {"arctan2d", stack_arctan2d},             /* added January 24, 2023 */
  {"cathetd", stack_cathetd},               /* added January 24, 2023 */
  {"cbrtd", stack_cbrtd},                   /* added January 24, 2023 */
  {"cosd", stack_cosd},                     /* added June 05, 2022 */
  {"coshd", stack_coshd},                   /* added January 24, 2023 */
  {"cotd", stack_cotd},                     /* added April 19, 2024 */
  {"cscd", stack_cscd},                     /* added April 19, 2024 */
  {"erfd", stack_erfd},                     /* added April 19, 2024 */
  {"expd", stack_expd},                     /* added June 29, 2022 */
  {"exp2d", stack_exp2d},                   /* added June 29, 2022 */
  {"exp10d", stack_exp10d},                 /* added June 29, 2022 */
  {"fmad", stack_fmad},                     /* added January 24, 2023 */
  {"fracd", stack_fracd},                   /* added June 27, 2022 */
  {"hypotd", stack_hypotd},                 /* added January 24, 2023 */
  {"intd", stack_intd},                     /* added June 27, 2022 */
  {"intdivd", stack_intdivd},               /* added June 06, 2022 */
  {"invhypotd", stack_invhypotd},           /* added January 24, 2023 */
  {"lnd", stack_lnd},                       /* added June 05, 2022 */
  {"logd", stack_logd},                     /* added June 05, 2022 */
  {"mapd", stack_mapd},                     /* added June 05, 2022 */
  {"meand", stack_meand},                   /* added June 05, 2022 */
  {"modd", stack_modd},                     /* added June 06, 2022 */
  {"mulbyd", stack_mulbyd},                 /* added June 05, 2022 */
  {"mulupd", stack_mulupd},                 /* added June 05, 2022 */
  {"negated", stack_negated},               /* added June 27, 2022 */
  {"powd", stack_powd},                     /* added June 05, 2022 */
  {"pythad", stack_pythad},                 /* added January 24, 2023 */
  {"recipd", stack_recipd},                 /* added June 06, 2022 */
  {"rootd", stack_rootd},                   /* added June 05, 2022 */
  {"secd", stack_secd},                     /* added April 19, 2024 */
  {"sind", stack_sind},                     /* added June 05, 2022 */
  {"sinhd", stack_sinhd},                   /* added January 24, 2023 */
  {"squared", stack_squared},               /* added June 05, 2022 */
  {"sqrtd", stack_sqrtd},                   /* added June 05, 2022 */
  {"sumupd", stack_sumupd},                 /* added June 05, 2022 */
  {"tand", stack_tand},                     /* added June 05, 2022 */
  {"tanhd", stack_tanhd},                   /* added January 24, 2023 */
  {"addtwod", stack_addtwod},               /* added June 27, 2022 */
  {"subtwod", stack_subtwod},               /* added June 27, 2022 */
  {"multwod", stack_multwod},               /* added June 27, 2022 */
  {"divtwod", stack_divtwod},               /* added June 27, 2022 */
  {"intdivtwod", stack_intdivtwod},         /* added June 27, 2022 */
  {"modtwod", stack_modtwod},               /* added June 27, 2022 */
  {"powtwod", stack_powtwod},               /* added June 27, 2022 */
  /* char functions */
  {"lowerd", stack_lowerd},                 /* added June 09, 2022 */
  {"upperd", stack_upperd},                 /* added June 09, 2022 */
  {NULL, NULL}
};


LUALIB_API int luaopen_stack (lua_State *L) {
  luaL_register(L, AGENA_STACKLIBNAME, stack_funcs);
  return 1;
}

