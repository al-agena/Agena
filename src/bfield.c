/* This library implements a character buffer. Initiated 22/03/2020, 2.17.8

   It provides a memory file userdata that stores a string of almost unlimited length, along with functions to work with it.
   It can be used if you have to iteratively concatenate a lot of strings, being 20 times faster than
   the `&` operator.

   The package also provides the following metamethods:

  '__index'       read operation, e.g. n[p] or n[p to q], with p, q indices, both counting from 1
  '__writeindex'  write operation, e.g. n[p] := value, with p the index, counting from 1
  '__size'        number of stored bytes
  '__zero'        zero operator, all bits set to ?
  '__tostring'    output at the console
  '__gc'          garbage collection */

#include <stdio.h>

#define bfield_c
#define LUA_LIB

#include "agena.h"

#include "agenalib.h"
#include "agnconf.h"  /* for uchar */
#include "agnxlib.h"
#include "charbuf.h"

#define checkfield(L, n)      (Charbuf *)luaL_checkudata(L, n, "bfield")

#if !(defined(__DJGPP__) || defined(__OS2__) || defined(LUA_ANSI))
#define AGENA_BFIELDLIBNAME "bfield"
LUALIB_API int (luaopen_bfield) (lua_State *L);
#endif


/* Creates a bitfield of at least n bits and optionally sets zeros or ones into this field. The return is a byte buffer with
   initially all positions set to zero. If you pass optional ones or zeros, or the Booleans `true` or `false`, they are set
   from the right end to the left end of the bitfield, e.g. if we have

   > b := memfile.bitfield(4, 1, 1, 0, 0)

   we store 0b0011 = 3 decimal into the field. The number of bits actually allocated is always a multiple of 8. */
static int bfield_new (lua_State *L) {  /* 2.31.4 */
  size_t nbits;
  Charbuf *buf;
  int value;
  luaL_checkstack(L, lua_gettop(L), "too many arguments");
  nbits = agn_checkposint(L, 1);
  value = agnL_optnonnegint(L, 2, 0);
  buf = (Charbuf *)lua_newuserdata(L, sizeof(Charbuf));
  if (buf)
    bitfield_init(L, buf, nbits, value);
  else
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "bfield.new");  /* 5.3.0 fix */
  lua_setmetatabletoobject(L, -1, "bfield", 1);
  return 1;
}


static int bfield_getbit (lua_State *L) {  /* 2.31.4 */
  lua_pushinteger(L, bitfield_get(L, checkfield(L, 1), agn_checkposint(L, 2)));
  return 1;
}


/* The idea for the following C function that both supports classic indexing and OOP methods has been taken from:
   https://stackoverflow.com/questions/23449458/specifying-both-methods-and-index-operator-in-lua-metatable,
   article `Specifying both "methods" and index operator in Lua metatable`.
   User `luther` describes how to do this on the Lua, not the C side, so here is the C equivalent.
   mt_index will be assigned to `__index'. 4.3.2 */
static int mt_index (lua_State *L) {
  if (agn_isposint(L, 2)) {
    /* classic indexing obj[key] attempt */
    lua_pushinteger(L, bitfield_get(L, checkfield(L, 1), agn_tointeger(L, 2)));
    return 1;
  } else {  /* 4.3.2 call OOP method, 4.4.1 change */
    return agn_initmethodcall(L, AGENA_BFIELDLIBNAME, sizeof(AGENA_BFIELDLIBNAME) - 1);
  }
}


static int bfield_setbitto (lua_State *L) {  /* 2.31.4 */
  bitfield_setbit(L, checkfield(L, 1), agn_checkposint(L, 2), agn_checkinteger(L, 3));
  return 0;
}


static int bfield_setbit (lua_State *L) {
  bitfield_set(L, checkfield(L, 1), agn_checkposint(L, 2));
  return 0;
}


static int bfield_clearbit (lua_State *L) {  /* 2.31.4 */
  bitfield_clear(L, checkfield(L, 1), agn_checkposint(L, 2));
  return 0;
}

static int bfield_flipbit (lua_State *L) {  /* 2.31.4 */
  lua_pushinteger(L, bitfield_flip(L, checkfield(L, 1), agn_checkposint(L, 2)));
  return 1;
}


static int bfield_getbyte (lua_State *L) {  /* 2.31.4 */
  Charbuf *buf = checkfield(L, 1);
  size_t pos = agn_checkposint(L, 2);
  if (pos > buf->size)
    luaL_error(L, "Error in " LUA_QS ": invalid position %d given.", pos, "bfield.getbyte");
  lua_pushinteger(L, uchar(buf->data[pos - 1]));
  return 1;
}


static int bfield_setbyte (lua_State *L) {  /* 2.31.4 */
  Charbuf *buf = checkfield(L, 1);
  size_t pos = agn_checkposint(L, 2);
  if (pos > buf->size)
    luaL_error(L, "Error in " LUA_QS ": invalid position %d given.", pos, "bfield.setbyte");
  buf->data[pos - 1] = agn_checknonnegative(L, 3);
  return 0;
}


static int bfield_resize (lua_State *L) {  /* 2.31.4 */
  bitfield_resize(L, checkfield(L, 1), agn_checkposint(L, 2), agnL_optnonnegint(L, 3, 0));
  return 0;
}


/* --- Metamethods -----------------------------------------------------------------------------------------------*/

static int mt_getsize (lua_State *L) {
  lua_pushnumber(L, bitfield_size(L, checkfield(L, 1)));
  return 1;
}


static int mt_tostring (lua_State *L) {  /* at the console, the array is formatted as follows: */
  if (luaL_isudata(L, 1, "bfield")) {
    unsigned char byte;
    int c, i, j;
    Charbuf *buf = lua_touserdata(L, 1);
    luaL_checkstack(L, CHAR_BIT + 2, "not enough stack space");  /* 2.31.7 fix */
    lua_pushfstring(L, "field(");
    c = 1;
    for (i=0; i < buf->size; i++) {
      byte = uchar(buf->data[i]);
      lua_pushfstring(L, "0b"); c++;
      for (j=CHAR_BIT - 1; j >= 0; j--) {
        lua_pushfstring(L, "%d", ((LUAI_UINT32)(byte) & (1 << ((LUAI_UINT32)j))) != 0);
        c++;
      }
      lua_concat(L, c); c = 1;
      if (i < buf->size - 1) {
        lua_pushfstring(L, ", ");
        lua_concat(L, 2);
      }
    }
    lua_pushfstring(L, ")");
    lua_concat(L, 2);
  } else {
    void *p = lua_touserdata(L, 1);
    lua_pushfstring(L, (p != NULL) ? "userdata(%p)" : "unknown(%p)", lua_topointer(L, 1));
  }
  return 1;
}


static int mt_gc (lua_State *L) {
  Charbuf *buf = checkfield(L, 1);
  lua_setmetatabletoobject(L, 1, NULL, 1);  /* 2.31.7 fix */
  charbuf_free(buf);
  return 0;
}


/* Checks whether all elements in the numarray are zeros and returns `true` or `false`. 2.17.9 */
static int mt_zero (lua_State *L) {
  size_t i;
  Charbuf *buf = checkfield(L, 1);
  for (i=0; i < buf->size && buf->data[i] == 0; i++) { }
  lua_pushboolean(L, i == buf->size);
  return 1;
}


/* ---------------------------------------------------------------------------------------------------------------*/

static const struct luaL_Reg bfield_fieldlib [] = {  /* metamethods for field `n' */
  /* there is no need to include the supported methods here as mt_index will take care of this itself. */
  {"__index",      mt_index},           /* n[p], with p the index, counting from 1 plus OOP method calling */
  {"__writeindex", bfield_setbitto},    /* n[p] := value, with p the index, counting from 1 */
  {"__tostring",   mt_tostring},        /* for output at the console, e.g. print(n) */
  {"__size",       mt_getsize},         /* metamethod for `size` operator */
  {"__zero",       mt_zero},            /* metamethod for `zero` operator */
  {"__gc",         mt_gc},              /* please do not forget garbage collection */
  {NULL, NULL}
};


static const luaL_Reg bfieldlib[] = {
  {"clearbit",     bfield_clearbit},
  {"flipbit",      bfield_flipbit},
  {"getbit",       bfield_getbit},
  {"getbyte",      bfield_getbyte},
  {"iszero",       mt_zero},  /* just for testing purposes as it is the only one requiring no arguments */
  {"new",          bfield_new},
  {"resize",       bfield_resize},
  {"setbit",       bfield_setbit},
  {"setbyte",      bfield_setbyte},
  {"setbitto",     bfield_setbitto},
  {NULL, NULL}
};

/*
** Open bitfield library
*/

static void createmeta (lua_State *L) {
  luaL_newmetatable(L, AGENA_BFIELDLIBNAME);  /* create metatable for file handles */
  lua_pushvalue(L, -1);  /* push metatable */
  lua_setfield(L, -2, "__index");  /* metatable.__index = metatable */
  luaL_register(L, NULL, bfield_fieldlib);  /* methods */
}

LUALIB_API int luaopen_bfield (lua_State *L) {
  createmeta(L);
  /* register library */
  luaL_register(L, AGENA_BFIELDLIBNAME, bfieldlib);
  return 1;
}

