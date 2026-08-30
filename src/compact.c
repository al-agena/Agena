/*
** $Id: compact.c, initiated August 27, 2026 $
** Simple implementation of Lu 5.5 compact arrays, created with the help of Gemini AI
** See Copyright Notice in agena.h
*/

#define compact_c
#define LUA_LIB

#include <stdlib.h>
#include <string.h>

#include "agena.h"

#include "agenalib.h"
#include "agnconf.h"
#include "agnxlib.h"
#include "lapi.h"
#include "lobject.h"
#include "lpair.h"
#include "lstate.h"

#if !(defined(__DJGPP__) || defined(__OS2__) || defined(LUA_ANSI))
#define AGENA_COMPACTLIBNAME "compact"
LUALIB_API int (luaopen_compact) (lua_State *L);
#endif


typedef union {
  void *p;        /* light userdata pointers */
  lua_Number n;   /* doubles */
  void *gc;       /* GCObjects (strings, tables, functions, etc.) */
  int b;          /* booleans */
} CompactValue;

typedef struct {
  uint32_t length;       /* Number of active elements */
  uint32_t capacity;     /* Maximum allocated elements */
  uint32_t gc_count;     /* Tracks how many slots contain GC objects */
  /* Tail-allocated layout:
   * 1. CompactValue values[capacity]        (8 bytes * capacity)
   * 2. uint8_t packed_tags[(capacity+1)/2]   (0.5 bytes * capacity)
   * 3. TValue *gc_slots;                     (16 bytes * capacity, ALLOCATED LAZILY)
   */
} CompactArray;


/* Inline memory location macros */
#define get_values(arr)  ((CompactValue *)((char *)(arr) + sizeof(CompactArray)))
#define get_tags(arr)    ((unsigned char *)(get_values(arr) + (arr)->capacity))

/* Read a 4-bit packed tag */
static inline uint8_t get_tag_at (CompactArray *arr, size_t idx) {
  unsigned char *tags = get_tags(arr);
  uint8_t byte = tags[idx >> 1];
  return (idx & 1) ? (byte & 0x0F) : (byte >> 4);
}

/* Write a 4-bit packed tag */
static inline void set_tag_at (CompactArray *arr, size_t idx, uint8_t tag) {
  unsigned char *tags = get_tags(arr);
  if (idx & 1) {
    tags[idx >> 1] = (tags[idx >> 1] & 0xF0) | (tag & 0x0F);
  } else {
    tags[idx >> 1] = (tags[idx >> 1] & 0x0F) | (tag << 4);
  }
}

#define checktrie(L, n)   (CompactArray *)luaL_checkudata(L, n, AGENA_COMPACTLIBNAME)
#define istrie(L,n)       (luaL_isudata(L, n, AGENA_COMPACTLIBNAME))

#define isgctype(t) ((t) >= LUA_TSTRING)


/* Memory location helpers */
#define get_values(arr)    ((CompactValue *)((char *)(arr) + sizeof(CompactArray)))
#define get_tags(arr)      ((unsigned char *)(get_values(arr) + (arr)->capacity))
#define get_gc_slots(arr)  ((TValue **)((char *)(get_tags(arr)) + ((arr)->capacity + 1) / 2))

static int compact_new (lua_State *L) {
  lua_Integer size = agn_checkposint(L, 1);
  size_t header_size = sizeof(CompactArray);
  size_t values_size = (size_t)size * sizeof(CompactValue);
  size_t tags_size   = ((size_t)size + 1) / 2;
  /* Allocate only the base compact structures; gc_slots starts as NULL pointer */
  size_t total_size  = header_size + values_size + tags_size + sizeof(TValue *);
  CompactArray *arr = (CompactArray *)lua_newuserdata(L, total_size);
  arr->length   = (uint32_t)size;
  arr->capacity = (uint32_t)size;
  arr->gc_count = 0;
  memset(get_values(arr), 0, values_size);
  memset(get_tags(arr), 0, tags_size);
  /* Set the lazy pointer slot to NULL */
  *get_gc_slots(arr) = NULL;
  /* Allocate and set an environment table for this array */
  lua_createtable(L, size, 0); 
  lua_setfenv(L, -2); /* Pops table, links it directly to our userdata object */
  /* Bypassed: No lua_newtable(L) or lua_setfenv(L) called here! */
  luaL_getmetatable(L, AGENA_COMPACTLIBNAME);
  lua_setmetatable(L, -2);
  return 1;
}

static inline void ensure_gc_partition (lua_State *L, CompactArray *arr) {
  TValue **gc_ptr = get_gc_slots(arr);
  if (*gc_ptr == NULL) {
    size_t size = arr->capacity * sizeof(TValue);
    TValue *block = (TValue *)lua_newuserdata(L, size);
    memset(block, 0, size);
    /* 1. Anchor raw block pointer in standard registry location */
    lua_pushlightuserdata(L, arr);
    lua_pushvalue(L, -2);
    lua_rawset(L, LUA_REGISTRYINDEX);
    lua_pop(L, 1); 
    *gc_ptr = block;
  }
}

static int compact_rawset (lua_State *L) {
  CompactArray *arr = checktrie(L, 1);
  ptrdiff_t idx = luaL_checkinteger(L, 2) - 1;
  int type = lua_type(L, 3);
  if (idx < 0 || (size_t)idx >= arr->length) {
    luaL_error(L, "Error in " LUA_QS ": index %d is out of range.", "compact.rawset", idx + 1);
    return 1;
  }
  if (type == LUA_TCOMPLEX) {
    /* we cannot simply replace a complex number with a number pair on the stack as the garbage collector
       would sweep it anyway. We would have to insert the new pair into the Agena registry and introduce a 
       `__gc' metamethod */
    luaL_error(L, "Error in " LUA_QS ": cannot store complex numbers.", "compact.rawset");
#ifndef PROPCMPLX
    agn_Complex z = agn_tocomplex(L, 3);
    agn_createpairnumbers(L, creal(z), cimag(z));
#else
    lua_Number re, im;
    agn_getcmplxparts(L, 3, &re, &im);
    agn_createpairnumbers(L, re, im);
#endif
    lua_replace(L, 3);
  }
  CompactValue *vals = get_values(arr);
  uint8_t old_tag = get_tag_at(arr, idx);
  set_tag_at(arr, idx, type);
  if (isgctype(old_tag)) arr->gc_count--;
  if (type == LUA_TNUMBER) {
    vals[idx].n = lua_tonumber(L, 3);
  } else if (type == LUA_TBOOLEAN) {
    vals[idx].b = lua_toboolean(L, 3);
  } else if (type == LUA_TNIL) {
    vals[idx].p = NULL;
    /* Clean up the slot index in the environment table if it exists */
    if (*get_gc_slots(arr) != NULL) {
      lua_getfenv(L, 1);           /* Push the array's fenv table */
      lua_pushnil(L);
      lua_rawseti(L, -2, idx + 1); /* fenv[idx+1] = nil */
      lua_pop(L, 1);               /* Pop fenv table */
    }
  } else {
    ensure_gc_partition(L, arr);
    TValue *gc_slots = *get_gc_slots(arr);
    /* 1. Raw C layout bitwise write */
    StkId val_addr = (L->base + (3 - 1));
    setobj(L, &gc_slots[idx], val_addr);
    arr->gc_count++;
    /* 2. GC ANCHOR: Mirror the object directly into our own fenv table */
    lua_getfenv(L, 1);           /* Push our own environment table to stack top */
    lua_pushvalue(L, 3);         /* Duplicate the element (or complex pair) */
    lua_rawseti(L, -2, idx + 1); /* fenv[idx + 1] = object */
    lua_pop(L, 1);               /* Pop the fenv table */
  }
  lua_pushvalue(L, 1);
  return 1;
}


static int compact_rawget (lua_State *L) {
  CompactArray *arr = checktrie(L, 1);
  ptrdiff_t idx = luaL_checkinteger(L, 2) - 1;
  if (idx < 0 || (size_t)idx >= arr->length) {
    lua_pushnil(L);
    return 1;
  }
  switch (get_tag_at(arr, idx)) {
    case LUA_TNIL:
      lua_pushnil(L);
      break;
    case LUA_TNUMBER:
      lua_pushnumber(L, get_values(arr)[idx].n);
      break;
    case LUA_TBOOLEAN:
      lua_pushboolean(L, get_values(arr)[idx].b);
      break;
    case LUA_TCOMPLEX: {
      TValue *gc_slots = *get_gc_slots(arr);
      setobj(L, L->top, &gc_slots[idx]);
      Pair *p = pairvalue(L->top);
      agn_pushcomplex(L, nvalue(pairitem(p, 0)), nvalue(pairitem(p, 1)));
      break;
    }
    default: {
      TValue *gc_slots = *get_gc_slots(arr);
      if (gc_slots == NULL) {
        lua_pushnil(L);
      } else {
        /* Fix: Use core internal macros rather than a raw pointer increment */
        setobj(L, L->top, &gc_slots[idx]);
        api_incr_top(L);
      }
    }
  }
  return 1;
}

static int aux_in (lua_State *L, int idx1, int idx2) {
  CompactArray *arr = checktrie(L, idx1);
  int target_type = lua_type(L, idx2);
  size_t i, len = arr->length;
  CompactValue *vals = get_values(arr);
  switch (target_type) {
    case LUA_TNIL: {
      for (i=0; i < len; i++) {
        if (get_tag_at(arr, i) == LUA_TNIL) return 1;
      }
      break;
    }
    case LUA_TNUMBER: {
      lua_Number target_num = lua_tonumber(L, idx2);
      for (i=0; i < len; i++) {
        if (get_tag_at(arr, i) == LUA_TNUMBER && vals[i].n == target_num) return 1;
      }
      break;
    }
    case LUA_TBOOLEAN: {
      int target_bool = lua_toboolean(L, idx2);
      for (i=0; i < len; i++) {
        if (get_tag_at(arr, i) == LUA_TBOOLEAN && vals[i].b == target_bool) return 1;
      }
      break;
    }
    case LUA_TCOMPLEX: {
      TValue *gc_slots = *get_gc_slots(arr);
      if (gc_slots == NULL || arr->gc_count == 0) break;
#ifndef PROPCMPLX
      agn_Complex z = agn_tocomplex(L, idx2);
      lua_Number target_re = creal(z);
      lua_Number target_im = cimag(z);
#else
      lua_Number target_re, target_im;
      agn_getcmplxparts(L, idx2, &target_re, &target_im);
#endif
      for (i=0; i < len; i++) {
        if (get_tag_at(arr, i) == LUA_TCOMPLEX) {
          Pair *p = pairvalue(&gc_slots[i]);
          if (nvalue(pairitem(p, 0)) == target_re && 
              nvalue(pairitem(p, 1)) == target_im) return 1;
        }
      }
      break;
    }
    default: {  /* collectable types (Strings, Tables, Functions, Userdata) */
      TValue *gc_slots = *get_gc_slots(arr);
      if (gc_slots == NULL || arr->gc_count == 0) break;
      /* resolve raw address of the argument object on the stack */
      for (i=0; i < len; i++) {
        if (get_tag_at(arr, i) == target_type) {
         setobj(L, L->top, &gc_slots[i]);
          api_incr_top(L);
          /* perform a raw comparison against argument index idx2 */
          if (lua_rawequal(L, -1, idx2)) {
            L->top--; /* pop the temporary comparison copy */
            return 1;
          }
          L->top--; /* pop the temporary comparison copy if not a match */
        }
      }
      break;
    }
  }
  return 0;
}


static int compact_has (lua_State *L) {
  lua_pushboolean(L, aux_in(L, 1, 2));
  return 1;
}


static int compact_member (lua_State *L) {
  lua_pushboolean(L, aux_in(L, 2, 1));
  return 1;
}


/*** Metamethods **************************************************************************/

static int mt_in (lua_State *L) {
  lua_pushboolean(L, aux_in(L, 2, 1));
  return 1;
}


static int mt_notin (lua_State *L) {
  lua_pushboolean(L, !aux_in(L, 2, 1));
  return 1;
}


static const struct luaL_Reg array_metamethods[] = {
  {"__index",      compact_rawget},
  {"__writeindex", compact_rawset},
  {"__in",         mt_in},
  {"__notin",      mt_notin},
  {NULL, NULL}
};

static const struct luaL_Reg array_methods[] = {
  {"new",      compact_new},
  {"rawget",   compact_rawget},
  {"rawset",   compact_rawset},
  {"member",   compact_member},
  {"has",      compact_has},
  {NULL, NULL}
};


/*
** Open compact library
*/

int luaopen_compact (lua_State *L) {
  luaL_newmetatable(L, AGENA_COMPACTLIBNAME);
  luaL_register(L, NULL, array_metamethods);
  lua_pop(L, 1);
  luaL_register(L, "compact", array_methods);
  return 1;
}


/*
slots := 1048576

a := numarray.double(slots);

for i to slots by 2 do
   a[i] := i
od;

drop(2, environ.used()):

####

slots := 1048576

a := [];

for i to slots by 2 do
   a[i] := i
od;

drop(2, environ.used()):


import compact
a := compact.new(3)
a[1] := 1
a[2] := Pi!E
a[3] := ['I am a string']
a[1]:
a[2]:
a[3]:
environ.gc()
a[1]:
a[2]:
a[3]:

*/
