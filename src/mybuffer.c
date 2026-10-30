/*
** $Id: mybuffer.c, 29/10/2026 $
** Substitute for Lua's Buffer functionality, 1 to 2 % faster with strings smaller than 1024 characters,
** around 7 to 8 % faster with larger strings.
** See Copyright Notice in agena.h
*/

#include <string.h>

#define mybuffer_c
#define LUA_LIB

#include "agena.h"
#include "agnxlib.h"
#include "mybuffer.h"


/* macro version is not faster */
LUALIB_API void my_buffinit (lua_State *L, MyBuffer *B) {
  B->L = L;
  B->ptr = B->initb;
  B->capacity = sizeof(B->initb);
  B->size = 0;
}

void my_buffgrow (MyBuffer *B, size_t extra) {
  size_t new_cap = B->capacity * 2;
  if (new_cap < B->size + extra) {
    new_cap = B->size + extra;
  }
  if (B->ptr == B->initb) {
    char *new_ptr = (char *)malloc(new_cap);
    if (new_ptr == NULL) {
      luaL_error(B->L, "not enough memory (buffer allocation failed)");
    }
    memcpy(new_ptr, B->initb, B->size);
    B->ptr = new_ptr;
  } else {
    char *new_ptr = (char *)realloc(B->ptr, new_cap);
    if (new_ptr) {
      B->ptr = new_ptr;
    } else {
      free(B->ptr);
      B->ptr = B->initb; /* Reset to safe state */
      luaL_error(B->L, "not enough memory (buffer reallocation failed)");
      return;
    }
  }
  B->capacity = new_cap;
}

LUALIB_API void my_addlstring (MyBuffer *B, const char *s, size_t l) {
  if (B->size + l > B->capacity) {  /* Double capacity or match required size */
    my_buffgrow(B, l);
  }
  if (B->ptr) {
    memcpy(B->ptr + B->size, s, l);
    B->size += l;
  }
}

/* Safely hand off the fully built string to Lua 5.1, freeing heap allocation if necessary,
   macro version is not faster */
LUALIB_API void my_pushresult (MyBuffer *B) {
  if (B->ptr) {
    lua_pushlstring(B->L, B->ptr, B->size);
    if (B->ptr != B->initb) {
      free(B->ptr);
    }
  } else {
    lua_pushnil(B->L);
  }
}

