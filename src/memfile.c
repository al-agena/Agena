/* This library implements a character or byte buffer. Initiated 22/03/2020, 2.17.8

   It provides a memory file userdata that stores a string of almost unlimited length, along with functions to work with it.
   It can be used if you have to iteratively concatenate a lot of strings, being 20 times faster than
   the `&` operator.

   The package also provides the following metamethods:

  '__index',      read operation, e.g. n[p] or n[p to q], with p, q indices, both counting from 1
  '__writeindex', write operation, e.g. n[p] := value, with p the index, counting from 1
  '__size',       number of stored bytes
  '__tostring',   output at the console
  '__in',         `in` operator
  '__gc',         garbage collection
  '__eq',         `=` operator
  '__empty',      `empty` operator
  '__filled',     `filled` operator */

#include <errno.h>
#include <string.h>
#include <unistd.h>   /* write, read, etc. */

#define memfile_c
#define LUA_LIB

#include "agena.h"

#include "agenalib.h"
#include "agnconf.h"  /* for uchar */
#include "agnhlps.h"
#include "agnxlib.h"
#include "charbuf.h"
#include "lstrlib.h"  /* for SPECIALS */
#include "prepdefs.h"  /* FORCE_INLINE */

static FORCE_INLINE void *checkmemfile (lua_State *L, int n) {
  Charbuf *b = (Charbuf *)luaL_testudata(L, n, "bitfield");
  if (!b) {
    b = (Charbuf *)luaL_checkudata(L, n, "memfile");
  }
  return b;
}

#define ismemfile(L,idx) (luaL_isudata(L, idx, "bitfield") || luaL_isudata(L, idx, "memfile"))


#if !(defined(__DJGPP__) || defined(__OS2__) || defined(LUA_ANSI))
#define AGENA_MEMFILELIBNAME "memfile"
LUALIB_API int (luaopen_memfile) (lua_State *L);
#endif


static char *aux_convert (lua_State *L, int idx, size_t *l, int *freeme) {  /* 2.21.1 */
  char *str;
  *freeme = 0;
  switch (lua_type(L, idx)) {
    case LUA_TBOOLEAN:
      if (lua_toboolean(L, idx) == LUA_TFAIL)
        str = "fail";
      else
        str = (lua_toboolean(L, idx) ? "true" : "false");
      *l = tools_strlen(str);
      break;
    case LUA_TNIL:
      str = "null"; *l = 4;
      break;
    case LUA_TCOMPLEX: {  /* 4.1.0 extension; you must FREE the string returned ! */
      lua_Number a, b;
      agn_getcmplxparts(L, idx, &a, &b);
      luaL_checkstack(L, 2 + 2*(a != 0), "not enough stack space");
      if (a != 0.0) {
        lua_pushnumber(L, a);
        lua_pushstring(L, b < 0 ? "" : "+");
      }
      lua_pushnumber(L, b);
      lua_pushstring(L, "*I");
      lua_concat(L, 2 + 2*(a != 0));
      str = tools_strdup(luaL_checklstring(L, -1, l));  /* GC may invalidate the assembled string */
      if (!str) luaL_error(L, "Error: memory allocation failed.");  /* 7.7.7 fix */
      agn_poptop(L);
      *freeme = 1;
      break;
    }
    default:
      str = (char *)luaL_checklstring(L, idx, l);
  }
  return str;
}

static char *aux_getdelim (lua_State *L, int nargs, size_t *ldelim, const char *procname) {  /* FREE IT ! 2.11.1 */
  char *delim = NULL;
  luaL_checkstack(L, 2, "too many arguments");  /* 3.15.2 fix */
  agn_pairgeti(L, nargs, 1);  /* get left value, set to stack index -2 */
  agn_pairgeti(L, nargs, 2);  /* get right value, set to stack index  -1 */
  if (tools_strneq("delim", agn_checkstring(L, -2))) {
    agn_poptoptwo(L);  /* 2.21.1 */
    luaL_error(L, "Error in " LUA_QS ": unknown option.", procname);
  } else {  /* 2.27.0 optimisation */
    const char *str = agn_checklstring(L, -1, ldelim);
    delim = tools_strndup(str, *ldelim);  /* free() see g_write; 2.27.0 optimisation */
  }
  agn_poptoptwo(L);
  return delim;
}

#define add_delim(L,buffer,delim,ldelim) { \
  if (delim) {  \
    if (ldelim == 1) \
      charbuf_appendchar(L, buffer, *delim); \
    else \
      charbuf_append(L, buffer, delim, ldelim); \
  } \
}

static int memfile_iterate (lua_State *L);

/* the memfile must be on the stack top */
static void aux_additerator (lua_State *L) {
  luaL_checkstack(L, 6, "not enough stack space");
  lua_getmetatable(L, -1);
  lua_pushcfunction(L, &memfile_iterate); /* 1 push iterator */
  lua_pushvalue(L, -3);   /* 2 memfile */
  lua_pushinteger(L, 1);  /* 3 starting position */
  lua_pushinteger(L, 1);  /* 4 number of values per call */
  lua_pushtrue(L);        /* 5 retrieve both index and value */
  lua_call(L, 4, 1);      /* create iterator */
  lua_setfield(L, -2, "__iter");  /* assign to metatable */
  agn_poptop(L);  /* pop metatable and memfile on the stack top, to returned */
}

/* Creates a new memfile and optionally puts one or more strings into it. The function returns the memfile created.
   You can also specify an optional delimiter delim = <string> that separates each value to be added to the memory file,
   e.g. memfile.charbuf('a', 'b', 'c', delim = ';') actually adds the string 'a;b;c;'. You may later on drop the final
   delimiter by calling `memfile.dump` with the size of the delimiter. */
static int memfile_charbuf (lua_State *L) {  /* create a new userdata */
  size_t i, nargs, l, ldelim;
  Charbuf *buf;
  char *str, *delim;
  int freeme;
  delim = NULL;
  ldelim = 0;
  nargs = lua_gettop(L);
  luaL_checkstack(L, nargs, "too many arguments");
  if (nargs > 1 && lua_ispair(L, nargs)) {  /* 2.21.1 extension */
    delim = aux_getdelim(L, nargs, &ldelim, "memfile.charbuf");
    nargs--;
  }
  buf = (Charbuf *)lua_newuserdata(L, sizeof(Charbuf));
  if (buf)
    charbuf_init(L, buf, 0, agn_getbuffersize(L), 0);  /* 2.34.9 adaption */
  else {
    xfree(delim);
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "memfile.charbuf");
  }
  lua_setmetatabletoobject(L, -1, "memfile", 1);  /* assign the metatable defined below to the userdata */
  aux_additerator(L);
  for (i=1; i <= nargs; i++) {
    lua_lock(L);
    str = aux_convert(L, i, &l, &freeme);
    if (l == 1)
      charbuf_appendchar(L, buf, *str);
    else
      charbuf_append(L, buf, str, l);
    add_delim(L, buf, delim, ldelim);  /* if ldelim is 0, nothing happens */
    if (freeme) xfree(str);
    lua_unlock(L);
  }
  xfree(delim);
  return 1;
}


/* Creates a memfile of a fixed size and fills it with zeros by default. It can also initialise the buffer with bytes (second to last argument).
   Since the memfile created is no different from the one created by `memfile.charbuf`, you can apply all the other `memfile` functions on it. */
static int memfile_bytebuf (lua_State *L) {  /* create a new userdata, 2.26.2 */
  size_t i, nargs, nbytes;
  Charbuf *buf;
  unsigned char b;
  nargs = lua_gettop(L);
  luaL_checkstack(L, nargs, "too many arguments");
  nbytes = agnL_optnonnegint(L, 1, 0);  /* changed 2.37.3/4 */
  if (nargs > nbytes + 1) {
    luaL_error(L, "Error in " LUA_QS ": too many arguments.", "memfile.bytebuf");
  }
  buf = (Charbuf *)lua_newuserdata(L, sizeof(Charbuf));
  if (buf)
    charbuf_init(L, buf, 1, nbytes, 0);  /* the function takes care itself if something goes wrong with allocation */
  else
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "memfile.bytebuf");
  lua_setmetatabletoobject(L, -1, "memfile", 1);  /* assign the metatable defined below to the userdata */
  aux_additerator(L);
  for (i=2; i <= nargs; i++) {
    lua_lock(L);
    b = uchar(agn_checknonnegint(L, i));
    buf->data[i - 2] = (char)b;
    lua_unlock(L);
  }
  return 1;
}


/* Creates a bitfield of at least n bits and optionally sets zeros or ones into this field. The return is a byte buffer with
   initially all positions set to zero. If you pass optional ones or zeros, or the Booleans `true` or `false`, they are set
   from the right end to the left end of the bitfield, e.g. if we have

   > b := memfile.bitfield(4, 1, 1, 0, 0)

   we store 0b0011 = 3 decimal into the field. The number of bits actually allocated is always a multiple of 8. */
static int memfile_bitfield (lua_State *L) {  /* 2.32.2 */
  size_t i, nargs, nbits, place;
  Charbuf *buf;
  ptrdiff_t pos;
  unsigned char byte;
  int flag;
  nargs = lua_gettop(L);
  luaL_checkstack(L, nargs, "too many arguments");
  nbits = agn_checkposint(L, 1);
  if (nargs > nbits + 1) {
    luaL_error(L, "Error in " LUA_QS ": too many arguments.", "memfile.bitfield");
  }
  if (nbits & (CHAR_BIT - 1))
    nbits = tools_nextmultiple(nbits, CHAR_BIT);
  buf = (Charbuf *)lua_newuserdata(L, sizeof(Charbuf));
  if (buf)
    charbuf_init(L, buf, 1, nbits/CHAR_BIT, 0);  /* the function takes care itself if something goes wrong with allocation */
  else
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "memfile.bitfield");
  lua_setmetatabletoobject(L, -1, "bitfield", 1);  /* assign the metatable defined below to the userdata */
  for (i=2; i <= nargs; i++) {
    lua_lock(L);
    pos = i - 1;
    IQR(pos, place);
    byte = uchar(buf->data[pos - 1]);
    if (lua_isboolean(L, i)) {
      flag = lua_toboolean(L, i);
      if (flag < 0)  /* fail ? */
        luaL_error(L, "Error in " LUA_QS ": argument must be either true or false, or 0 or 1.", "memfile.bitfield");
    } else {
      flag = agn_checknonnegint(L, i);
    }
    flag = !(flag == 0);
    byte = uchar(((LUAI_UINT32)byte & ~(1 << ((LUAI_UINT32)place - 1))) | (flag << ((LUAI_UINT32)place - 1)));
    buf->data[pos - 1] = byte;
    lua_unlock(L);
  }
  return 1;
}


/* Resizes the memfile to exactly n places (bytes). It can grow or shrink a memfile and in the latter case preserves the remaining
   content. If the memfile is to be enlarged, the function optionally fills the new space created with zeros if the third argument
   `true` is given, otherwise you may just pass `false`.

   You may call `bytes.optsize` before to determine the optimal number of places (bytes) in the memfile to get it word-aligned. */
static int memfile_resize (lua_State *L) {  /* 2.26.2/2.26.3 */
  size_t newsize;
  int fillzeros, rc;
  Charbuf *buf = checkmemfile(L, 1);
  agnL_checkudfreezed(L, 1, "buffer", "memfile.resize");  /* 4.9.0 */
  newsize = agn_checkposint(L, 2);
  fillzeros = agnL_optboolean(L, 3, 0);
  if ( (rc = charbuf_resize(L, buf, newsize, fillzeros) ) != 0) {
    agnL_issuememfileerror(L, newsize, "memfile.resize");
  }
  return 0;
}


/* Sets the end of the current memfile to the given position n, inclusive, with n a non-negative integer, or in other words:
   changes the size of the memfile without re-allocating memory. If n is 0, the memfile is cleared. If n is, for example 2, then
   if you call `memfile.append` thereafter with a substring, it will be added starting at position 3, preserving the values at
   position 1 and 2. The function returns nothing. See also: `memfile.append`, `memfile.resize`, `memfile.rewind`. 2.26.3 */
static int memfile_move (lua_State *L) {
  Charbuf *buf = checkmemfile(L, 1);
  agnL_checkudfreezed(L, 1, "buffer", "memfile.move");  /* 4.9.0 */
  size_t pos = agn_checknonnegint(L, 2);
  if (pos > buf->size)
    luaL_error(L, "Error in " LUA_QS ": invalid position %d.", "memfile.move", (int)pos);
  buf->size = pos;
  return 0;
}


/* Sets the current size of a memfile to zero, effectively clearing the buffer without re-allocating memory. The function
   returns nothing. See also `memfile.append`, `memfile.move`. 2.26.3 */
static int memfile_rewind (lua_State *L) {
  Charbuf *buf = checkmemfile(L, 1);
  agnL_checkudfreezed(L, 1, "buffer", "memfile.rewind");  /* 4.9.0 */
  buf->size = 0;
  return 0;
}


/* Appends one or more numbers, strings, Booleans or null's to the end of memfile. The function returns nothing.
   You can also specify an optional delimiter delim = <string> that separates each value to be added to the memory file,
   e.g. memfile.append(m, 'a', 'b', 'c', delim = ';') actually adds the string 'a;b;c;'. You may later on drop the final
   delimiter by calling `memfile.dump` with the size of the delimiter. */

static void aux_checkapoptions (lua_State *L, Charbuf *buf, size_t *nargs, int *bytemode, char **delim, size_t *ldelim, const char *procname) {
  int checkoptions = 2;  /* check n options; CHANGE THIS if you add/delete options, 2.29.0 */
  if (buf->type == MEMFILE_BYTEBUF && *nargs > 2 && lua_isboolean(L, *nargs)) {
    *bytemode = agn_istrue(L, *nargs);
    (*nargs)--;
  }
  if (*nargs > 2 && lua_ispair(L, *nargs))  /* 3.15.2 fix */
    luaL_checkstack(L, 2, "not enough stack space");
  while (checkoptions-- && *nargs > 2 && lua_ispair(L, *nargs)) {
    agn_pairgeti(L, *nargs, 1);  /* get left value, set to stack index -2 */
    agn_pairgeti(L, *nargs, 2);  /* get right value, set to stack index  -1 */
    if (agn_isstring(L, -2)) {
      const char *option = agn_tostring(L, -2);
      if (tools_streq(option, "bytes") && lua_isboolean(L, -1)) {  /* 2.37.4 improvement */
        *bytemode = lua_toboolean(L, -1);
      } else if (tools_streq(option, "delim")) {  /* 2.37.4 improvement */
        const char *str = lua_tolstring(L, -1, ldelim);
        if (!str) {
          xfree(*delim);
          luaL_error(L, "Error in " LUA_QS ": " LUA_QS " option requires a string or number, got %s.", procname, "delim", luaL_typename(L, -1));
        }
        xfree(*delim);  /* 5.5.1 patch */
        *delim = tools_strndup(str, *ldelim);  /* free() see g_write; 2.27.0 optimisation */
      } else {
        xfree(*delim);
        agn_poptoptwo(L);
        luaL_error(L, "Error in " LUA_QS ": unknown option or invalid right-hand side" LUA_QS ".", procname, option);
      }
    }
    /* do not call lua_settop as it would corrupt the argument stack since we have already pushed values */
    (*nargs)--;
    agn_poptoptwo(L);
  }
}

#define luaL_noinsert(L,pn) luaL_error(L, "Error in " LUA_QS ": could not insert data into buffer.", pn)

static int memfile_append (lua_State *L) {
  Charbuf *buf;
  char *str;
  size_t nargs, i, l, ldelim;
  int bytemode, freeme;
  char *delim = NULL;
  ldelim = 0;
  nargs = lua_gettop(L);
  luaL_checkstack(L, nargs, "too many arguments");
  if (nargs < 2)
    luaL_error(L, "Error in " LUA_QS ": need at least a memfile and a value.", "memfile.append");
  buf = (Charbuf *)checkmemfile(L, 1);
  agnL_checkudfreezed(L, 1, "buffer", "memfile.append");  /* 4.9.0 */
  bytemode = (buf->type == MEMFILE_CHARBUF) ? 0 : 1;  /* 2.37.5 change: always insert integers as bytes to byte buffers */
  aux_checkapoptions(L, buf, &nargs, &bytemode, &delim, &ldelim, "memfile.append");
  switch (buf->type) {
    case MEMFILE_CHARBUF: {  /* character buffer ? 2.37.3 */
      for (i=2; i <= nargs; i++) {
        lua_lock(L);
        str = aux_convert(L, i, &l, &freeme);
        if (l == 1) {
          if (!charbuf_appendchar(L, buf, str[0])) luaL_noinsert(L, "memfile.append");
        } else {
          if (!charbuf_append(L, buf, str, l)) luaL_noinsert(L, "memfile.append");
        }
        add_delim(L, buf, delim, ldelim);  /* if ldelim is 0, nothing happens */
        if (freeme) xfree(str);
        lua_unlock(L);
      }
      break;
    }
    case MEMFILE_BYTEBUF: {  /* byte buffer ?  2.37.3 */
      for (i=2; i <= nargs; i++) {
        lua_lock(L);
        if (bytemode && agn_isinteger(L, i)) {
          unsigned char b = uchar(agn_tointeger(L, i));  /* the byte */
          if (!charbuf_growsize(L, buf, 1)) luaL_noinsert(L, "memfile.append"); /* 2.37.4 change */
          buf->data[buf->size++] = (char)b;
        } else if (lua_isstring(L, i)) {
          str = aux_convert(L, i, &l, &freeme);
          if (l == 1) {
            if (!charbuf_appendchar(L, buf, str[0])) luaL_noinsert(L, "memfile.append");
          } else if (!charbuf_append(L, buf, str, l)) luaL_noinsert(L, "memfile.append");
          if (freeme) xfree(str);
        } else if (lua_istable(L, i)) {  /* new 5.5.13 */
          int x, rc;
          size_t j, k, l = agn_asize(L, i);
          char chr[l]; for (k=0; k < l; k++) chr[k] = 0;  /* 5.5.13 tuning */
          for (j=1; j <= l; j++) {
            x = agn_rawgetiinteger(L, i, j, &rc);
            if (!rc || x < 0 || x > 255) {
              luaL_error(L, "Error in " LUA_QS ": value must be in 0 .. 255.", "memfile.append");
            }
            chr[j - 1] = uchar(x);
          }
          charbuf_append(L, buf, chr, l);
        } else if (lua_isseq(L, i)) {
          int x, rc;  /* 2.37.4 fix */
          size_t j, k, l = agn_seqsize(L, i);
          char chr[l]; for (k=0; k < l; k++) chr[k] = 0;  /* 5.5.13 tuning */
          for (j=1; j <= l; j++) {
            x = agn_seqrawgetiinteger(L, i, j, &rc);
            if (!rc || x < 0 || x > 255) {
              luaL_error(L, "Error in " LUA_QS ": value must be in 0 .. 255.", "memfile.append");
            }
            chr[j - 1] = uchar(x);
          }
          charbuf_append(L, buf, chr, l);
        } else if (lua_isreg(L, i)) {  /* new 5.5.13 */
          int x, rc;
          size_t j, k, l = agn_regsize(L, i);
          char chr[l]; for (k=0; k < l; k++) chr[k] = 0;  /* 5.5.13 tuning */
          for (j=1; j <= l; j++) {
            x = agn_regrawgetiinteger(L, i, j, &rc);
            if (!rc || x < 0 || x > 255) {
              luaL_error(L, "Error in " LUA_QS ": value must be in 0 .. 255.", "memfile.append");
            }
            chr[j - 1] = uchar(x);
          }
          charbuf_append(L, buf, chr, l);
        } else {  /* second and further arguments are neither byte, strings, or sequences of bytes */
          xfree(delim);
          luaL_error(L, "Error in " LUA_QS ": expected an integer or a string, for %s.", "memfile.append", luaL_typename(L, i));
        }
        add_delim(L, buf, delim, ldelim);  /* if ldelim is 0, nothing happens */
        lua_unlock(L);
      }
      break;
    }
    default: {
      xfree(delim);
      luaL_error(L, "Error in " LUA_QS ": invalid memfile type.", "memfile.append");
    }
  }
  xfree(delim);
  return 0;
}


static int memfile_prepend (lua_State *L) {  /* based on memfile.append, 2.73.4 */
  Charbuf *buf;
  char *str;
  size_t nargs, i, l, ldelim;
  int bytemode, freeme;
  char *delim = NULL;
  ldelim = 0;
  bytemode = 0;
  nargs = lua_gettop(L);
  luaL_checkstack(L, nargs, "too many arguments");
  if (nargs < 2)
    luaL_error(L, "Error in " LUA_QS ": need at least a memfile and a value.", "memfile.prepend");
  buf = (Charbuf *)checkmemfile(L, 1);
  agnL_checkudfreezed(L, 1, "buffer", "memfile.prepend");  /* 4.9.0 */
  aux_checkapoptions(L, buf, &nargs, &bytemode, &delim, &ldelim, "memfile.prepend");
  switch (buf->type) {
    case MEMFILE_CHARBUF: {  /* character buffer ? */
      for (i=2; i <= nargs; i++) {
        size_t k;
        lua_lock(L);
        str = aux_convert(L, i, &l, &freeme);
        char chr[l + ldelim];
        for (k=0; k < l; k++) chr[k] = str[k];
        for (k=0; k < ldelim; k++) chr[l + k] = delim[k];
        if (!charbuf_insert(L, buf, 0, chr, l + ldelim)) luaL_noinsert(L, "memfile.prepend");
        if (freeme) xfree(str);
        lua_unlock(L);
      }
      break;
    }
    case MEMFILE_BYTEBUF: {  /* byte buffer ?  */
      for (i=2; i <= nargs; i++) {
        lua_lock(L);
        if (bytemode && agn_isinteger(L, i)) {
          char chr[2] = { 0 };
          unsigned char b = uchar(agn_tointeger(L, i));  /* the byte */
          chr[0] = (char)b;
          if (!charbuf_insert(L, buf, 0, chr, 1)) luaL_noinsert(L, "memfile.prepend");
        } else if (lua_isstring(L, i)) {
          str = aux_convert(L, i, &l, &freeme);
          if (!charbuf_insert(L, buf, 0, str, l)) luaL_noinsert(L, "memfile.prepend");
          if (freeme) xfree(str);
        } else if (lua_istable(L, i)) {  /* new 5.5.13 */
          int x, rc;
          size_t j, k, l = agn_asize(L, i);
          char chr[l]; for (k=0; k < l; k++) chr[k] = 0;
          for (j=1; j <= l; j++) {
            x = agn_rawgetiinteger(L, i, j, &rc);
            if (!rc || x < 0 || x > 255) {
              luaL_error(L, "Error in " LUA_QS ": value must be in 0 .. 255.", "memfile.prepend");
            }
            chr[j - 1] = (char)x;
          }
          if (!charbuf_insert(L, buf, 0, chr, l)) luaL_noinsert(L, "memfile.prepend");
        } else if (lua_isseq(L, i)) {
          int x, rc;  /* 2.37.4 fix */
          size_t j, k, l = agn_seqsize(L, i);
          char chr[l]; for (k=0; k < l; k++) chr[k] = 0;
          for (j=1; j <= l; j++) {
            x = agn_seqrawgetiinteger(L, i, j, &rc);  /* 5.5.13 change */
            if (!rc || x < 0 || x > 255) {
              luaL_error(L, "Error in " LUA_QS ": value must be in 0 .. 255.", "memfile.prepend");
            }
            chr[j - 1] = (char)x;
          }
          if (!charbuf_insert(L, buf, 0, chr, l)) luaL_noinsert(L, "memfile.prepend");
        } else if (lua_isreg(L, i)) {  /* new 5.5.13 */
          int x, rc;
          size_t j, k, l = agn_regsize(L, i);
          char chr[l]; for (k=0; k < l; k++) chr[k] = 0;
          for (j=1; j <= l; j++) {
            x = agn_regrawgetiinteger(L, i, j, &rc);
            if (!rc || x < 0 || x > 255) {
              luaL_error(L, "Error in " LUA_QS ": value must be in 0 .. 255.", "memfile.prepend");
            }
            chr[j - 1] = (char)x;
          }
          if (!charbuf_insert(L, buf, 0, chr, l)) luaL_noinsert(L, "memfile.prepend");
        } else {  /* second and further arguments are neither byte, strings, or sequences of bytes */
          xfree(delim);
          luaL_error(L, "Error in " LUA_QS ": expected an integer or a string, for %s.", "memfile.prepend", luaL_typename(L, i));
        }
        /* 5.5.1 fix, if ldelim is 0, nothing happens; IMPROVE ME: avoid double inserts */
        if (!charbuf_insert(L, buf, l, delim, ldelim)) luaL_noinsert(L, "memfile.prepend");
        lua_unlock(L);
      }
      break;
    }
    default: {
      xfree(delim);
      luaL_error(L, "Error in " LUA_QS ": invalid memfile type.", "memfile.prepend");
    }
  }
  xfree(delim);
  return 0;
}


/* Inserts a string s or one or more bytes b into memory file m at position pos, with pos <> 0, shifting other elements into open
   space. If pos is negative then the insert will be at the |pos|-th position from the right. The function returns nothing. 2.37.3 */
static char *aux_fetchdata (lua_State *L, int idx, int nargs, size_t *l, int *freeme, const char *procname) {
  size_t i;
  unsigned char b;
  char *str = NULL;
  *freeme = 0;
  if (agn_isstring(L, idx)) {
    str = (char *)lua_tolstring(L, idx, l);
  } else if (lua_isseq(L, idx)) {
    *l = agn_seqsize(L, idx);
    str = (char *)agn_stralloc(L, *l + 1, procname, NULL);
    *freeme = 1;
    for (i=1; i <= *l; i++) {
      lua_seqrawgeti(L, idx, i);
      if (!agn_isinteger(L, -1)) {
        xfree(str);
        luaL_error(L, "Error in " LUA_QS ": expected an integer in sequence, got %s.", procname, luaL_typename(L, -1));
      }
      b = uchar(agn_tointeger(L, -1));
      agn_poptop(L);
      str[i - 1] = (char)b;
    }
    str[*l] = '\0';
  } else {
    *l = nargs;
    str = (char *)agn_stralloc(L, nargs + 1, procname, NULL);
    *freeme = 1;
    for (i=idx; i <= idx + nargs - 1; i++) {
      if (!agn_isinteger(L, i)) {
        xfree(str);
        luaL_error(L, "Error in " LUA_QS ": expected an integer, got %s.", procname, luaL_typename(L, i));
      }
      b = uchar(agn_tointeger(L, i));
      str[i - idx] = (char)b;
    }
    str[*l] = '\0';
  }
  return str;
}

static int memfile_tostring (lua_State *L) {
  int freeme;
  size_t l;
  luaL_checkany(L, 1);
  if (ismemfile(L, 1)) {  /* 2.38.0 */
    Charbuf *buf = (Charbuf *)checkmemfile(L, 1);
    if (buf->size == 0)
      lua_pushnil(L);
    else
      lua_pushlstring(L, buf->data, buf->size);
  } else {
    char *str = aux_fetchdata(L, 1, lua_gettop(L), &l, &freeme, "memfile.tostring");
    lua_pushlstring(L, str, l);
    if (freeme) xfree(str);
  }
  return 1;
}


/* Puts string str or sequence of bytes v into memory file memfile starting from position pos, shifting the other values into
   open space. The function automatically grows the memory file if needed. If pos is negative then the deletion will start at
   the |pos|-th position from the right. The function returns nothing. 2.37.3 */
static int memfile_put (lua_State *L) {
  Charbuf *buf;
  char *str;
  size_t pos, upto, nargs, l;
  int freeme = 0;
  nargs = lua_gettop(L);
  if (nargs < 3)
    luaL_error(L, "Error in " LUA_QS ": need at least a memfile, a position and a value.", "memfile.put");
  buf = (Charbuf *)checkmemfile(L, 1);
  agnL_checkudfreezed(L, 1, "buffer", "memfile.put");  /* 4.9.0 */
  pos = tools_posrelat(agn_checknumber(L, 2), buf->size);
  str = aux_fetchdata(L, 3, nargs - 3 + 1, &l, &freeme, "memfile.put");
  upto = pos + l - 1;
  if (pos == 0 || upto == 0 || upto < pos || pos > buf->size + 1) {  /* 2.38.0 extension */
    if (freeme) xfree(str);
    luaL_error(L, "Error in " LUA_QS ": invalid position %d given.", "memfile.put", pos);
  }
  if (!charbuf_extendby(L, buf, l)) {
    if (freeme) xfree(str);
    luaL_error(L, "Error in " LUA_QS ": could not grow byte buffer.", "memfile.put");
  }
  if (!charbuf_insert(L, buf, pos - 1, str, l)) {
    if (freeme) xfree(str);
    luaL_error(L, "Error in " LUA_QS ": could not insert data into buffer.", "memfile.put");
  }
  if (freeme) xfree(str);
  return 0;
}


/* Starting from position pos, removes n bytes or characters from memory file m, shifting down other elements to close the
   space. If pos is negative then the deletion will start at the |pos|-th position from the right. The function returns `true`
   on success and `false` otherwise. 2.37.3 */
static int memfile_purge (lua_State *L) {
  Charbuf *buf;
  int pos;
  size_t n;
  buf = (Charbuf *)checkmemfile(L, 1);
  agnL_checkudfreezed(L, 1, "buffer", "memfile.purge");  /* 4.9.0 */
  pos = tools_posrelat(agn_checknumber(L, 2), buf->size);
  n = agnL_optnonnegint(L, 3, 1);  /* 2.38.0 change */
  if (pos == 0 || pos > buf->size)
    luaL_error(L, "Error in " LUA_QS ": invalid position %d given.", "memfile.purge", pos);
  else if (pos + n - 1 > buf->size)
    luaL_error(L, "Error in " LUA_QS ": too many bytes (%d) to remove.", "memfile.purge", n);
  lua_pushboolean(L, charbuf_remove(L, buf, pos - 1, n));  /* if n == 0, returns false */
  return 1;
}


#define checkrc(L,rc,s,s_freeme,p,p_freeme,what) { \
  if (!rc) { \
    if (s_freeme) xfree(s); \
    if (p_freeme) xfree(p); \
    luaL_error(L, "Error in " LUA_QS ": could not %s data.", "memfile.replace", what); \
  } \
}

#define freedata(L,s,s_freeme,p,p_freeme) { \
  if (s_freeme) xfree(s); \
  if (p_freeme) xfree(p); \
}

static int memfile_replace (lua_State *L) {  /* 2.38.0 */
  Charbuf *buf;
  char *s, *p;
  const char *r;
  int rc, pos, s_freeme, p_freeme, hasspecials;
  size_t s_len, p_len, start, end, l;
  ptrdiff_t nsubs, c;
  buf = (Charbuf *)checkmemfile(L, 1);
  agnL_checkudfreezed(L, 1, "buffer", "memfile.replace");  /* 4.9.0 */
  s = aux_fetchdata(L, 2, 0, &s_len, &s_freeme, "memfile.replace");
  p = aux_fetchdata(L, 3, 0, &p_len, &p_freeme, "memfile.replace");
  pos = tools_posrelat(agnL_optposint(L, 4, 1), buf->size);
  nsubs = agnL_optposint(L, 5, -1);
  if (pos == 0 || pos > buf->size || s_len == 0) {
    freedata(L, s, s_freeme, p, p_freeme);
    luaL_error(L, "Error in " LUA_QS ": invalid position %d given or empty haystack or buffer.", "memfile.replace", pos);
  }
  if (!agnL_isvalidpattern(L, s, s_len, &hasspecials, 1)) {
    freedata(L, s, s_freeme, p, p_freeme);
    luaL_error(L, "Error in " LUA_QS ": %s", "memfile.replace", agn_tostring(L, -1));
  }
  c = 0;
  if (!hasspecials) {
    while ( (pos <= buf->size) && (r = tools_lmemfind(buf->data + pos - 1, buf->size - pos + 1, s, s_len) ) ) {
      c++;
      start = r - buf->data + 1;
      end = start + s_len - 1;
      l = end - start + 1;
      if (p_len && (l == p_len)) {
        if (pos + p_len - 1 > buf->size) {
          rc = charbuf_extendby(L, buf, p_len);
          checkrc(L, rc, s, s_freeme, p, p_freeme, "replace");
        }
        tools_memcpy(buf->data + start - 1, p, p_len);
      } else {
        rc = charbuf_remove(L, buf, start - 1, s_len);
        checkrc(L, rc, s, s_freeme, p, p_freeme, "remove");
        if (p_len) {
          rc = charbuf_insert(L, buf, pos - 1, p, p_len);
          checkrc(L, rc, s, s_freeme, p, p_freeme, "insert");
        }
      }
      pos = r - buf->data + p_len + 1;
      if (c == nsubs) break;
    }
  } else {
    ptrdiff_t start, end, growby;
    while ( (pos <= buf->size) && (r = agn_strmatch(L, buf->data, buf->size, s, pos, &start, &end) ) ) {
      c++;
      s_len = end - start + 1;
      if (p_len && (s_len == p_len)) {
        growby = pos + p_len - 1 - buf->size;
        if (growby > 0) {
          rc = charbuf_extendby(L, buf, growby);
          checkrc(L, rc, s, s_freeme, p, p_freeme, "replace");
          buf->size += growby;
        }
        tools_memcpy((void *)r, p, p_len);
      } else {
        rc = charbuf_remove(L, buf, start - 1, s_len);
        checkrc(L, rc, s, s_freeme, p, p_freeme, "remove");
        if (p_len) {
          rc = charbuf_insert(L, buf, start - 1, p, p_len);
          checkrc(L, rc, s, s_freeme, p, p_freeme, "insert");
        }
      }
      pos = (r - buf->data) + p_len + 1;
      if (c == nsubs) break;
    }
  }
  lua_pushinteger(L, c);
  return 1;
}


/* Returns the whole string stored in memfile and resets memfile completely so that it can store a another string.

   If a positive integer n is passed as an optional argument, then the function just dumps n bytes from the end of the memory file
   and returns them as a string, leaving the rest of the memfile untouched. If the memfile should be empty after this operation,
   it is reset, however, which is equal to calling the function without an optional argument.

   See also: `memfile.getsize` (or `size` operator) */
static int memfile_dump (lua_State *L) {
  Charbuf *buf;
  size_t dumplastchars, originalcapacity;
  lu_byte type;
  lua_lock(L);
  buf = checkmemfile(L, 1);
  agnL_checkudfreezed(L, 1, "buffer", "memfile.dump");  /* 4.9.0 */
  dumplastchars = agnL_optposint(L, 2, 0);  /* 2.21.1 extension */
  if (buf->size == 0) {  /* 2.38.0 change */
    lua_pushnil(L);
    return 1;
  }
  else if (buf->size < dumplastchars)
    luaL_error(L, "Error in " LUA_QS ": memfile is too small.", "memfile.dump");
  charbuf_finish(buf);  /* 2.27.3, better be sure than sorry */
  if (dumplastchars != 0) {
    lua_pushlstring(L, buf->data + buf->size - dumplastchars, dumplastchars);  /* dump n chars from end of memfile */
    buf->size -= dumplastchars;    /* shorten memfile */
    if (buf->size != 0) return 1;  /* return last n chars, but do not reset memfile */
    /* memfile is empty now ? -> assume the function has been called without optional argument, i.e. reset it below */
  } else
    lua_pushlstring(L, buf->data, buf->size);
  type = buf->type;
  originalcapacity = buf->originalcapacity;
  charbuf_free(buf);  /* do NOT call charbuf_destroy, this would crash Agena. */
  charbuf_init(L, buf, type, originalcapacity, 0);
  if (originalcapacity == 0) buf->size = 0;
  lua_unlock(L);
  return 1;
}


static int memfile_get (lua_State *L) {  /* 2.31.2 */
  Charbuf *buf;
  size_t dumplastchars;
  lua_lock(L);
  buf = checkmemfile(L, 1);
  dumplastchars = agnL_optposint(L, 2, 0);
  if (buf->size == 0)
    luaL_error(L, "Error in " LUA_QS ": memfile is empty.", "memfile.get");
  else if (buf->size < dumplastchars)
    luaL_error(L, "Error in " LUA_QS ": memfile is too small.", "memfile.get");
  if (dumplastchars != 0)
    lua_pushlstring(L, buf->data + buf->size - dumplastchars, dumplastchars);  /* return n chars from end of memfile */
  else
    lua_pushlstring(L, buf->data, buf->size);
  lua_unlock(L);
  return 1;
}


/* From memfile, returns the substring starting at position p of length n, with p <> 0. If p is negative, the position
   is relative to the end of the string. n is 1 by default. */
static int memfile_getitem (lua_State *L) {
  ptrdiff_t pos;
  size_t n;
  Charbuf *buf = checkmemfile(L, 1);
  pos = tools_posrelat(agn_checknumber(L, 2), buf->size);
  n = agnL_optposint(L, 3, 1);
  if (pos == 0 || pos + n - 1 > buf->size)
    luaL_error(L, "Error in " LUA_QS ": invalid position %d or length given.", "memfile.getitem", pos);
  else
    lua_pushlstring(L, buf->data + pos - 1, n);
  return 1;
}


/* From memfile, returns the character at position p, with p <> 0. If p is negative, the position
   is relative to the end of the string. The return is a string: the character. 2.26.2 */
static int memfile_getchar (lua_State *L) {
  ptrdiff_t pos;
  Charbuf *buf = checkmemfile(L, 1);
  pos = tools_posrelat(agn_checknumber(L, 2), buf->size);
  if (pos == 0 || pos > buf->size)
    luaL_error(L, "Error in " LUA_QS ": invalid position %d given.", "memfile.getchar", pos);
  else
    lua_pushchar(L, buf->data[pos - 1]);
  return 1;
}


/* From memfile, returns the byte at position p, with p <> 0. If p is negative, the position
   is relative to the end of the string. The return is an integer in the range [0, 255]. 2.26.2 */
static int memfile_getbyte (lua_State *L) {
  ptrdiff_t pos;
  Charbuf *buf = checkmemfile(L, 1);
  pos = tools_posrelat(agn_checknumber(L, 2), buf->size);
  if (pos == 0 || pos > buf->size)
    luaL_error(L, "Error in " LUA_QS ": invalid position %d given.", "memfile.getbyte", pos);
  else if (lua_gettop(L) == 2)
    lua_pushinteger(L, uchar(buf->data[pos - 1]));
  else {  /* new 2.31.2, return binary representation of the byte as a string */
    int i, slots;
    unsigned char byte = uchar(buf->data[pos - 1]);
    slots = CHAR_BIT + 1;
    luaL_checkstack(L, slots, "too many values");  /* 2.31.7 fix */
    lua_pushfstring(L, "0b");
    for (i=CHAR_BIT-1; i >= 0; i--) {
      lua_pushfstring(L, "%d", ((LUAI_UINT32)(byte) & (1 << ((LUAI_UINT32)i))) != 0);
    }
    lua_concat(L, slots);
  }
  return 1;
}


static int memfile_getbytes (lua_State *L) {  /* 2.31.2 */
  Charbuf *buf;
  ptrdiff_t pos, i, n, maxn;
  buf = checkmemfile(L, 1);
  pos = tools_posrelat(agnL_optinteger(L, 2, 1), buf->size);
  if (pos == 0 || pos > buf->size)
    luaL_error(L, "Error in " LUA_QS ": invalid position %d given.", "memfile.getbytes", pos);
  else {  /* changed 5.5.13 to read any number of bytes, not just until the end */
    maxn = buf->size - pos + 1;
    n = agnL_optposint(L, 3, maxn);  /* default: read to end of buffer */
    if (n > maxn) n = maxn;
    agn_createseq(L, n);
    for (i=pos - 1; i < pos + n - 1; i++) {
      agn_seqsetinumber(L, -1, i - pos + 2, uchar(buf->data[i]));
    }
  }
  return 1;
}


/* In the first form, returns the bit stored at bit position n, from the right end of the field, in the memfile.
   In the second form, from byte no. b in the memfile m, returns the n-th bit, where b and n > 0.
   The return is either 1 or 0. */
static int memfile_getbit (lua_State *L) {  /* 2.26.2 */
  Charbuf *buf;
  ptrdiff_t pos;
  LUAI_UINT32 place;
  unsigned char byte;
  int twoargsonly = lua_gettop(L) == 2;
  buf = checkmemfile(L, 1);
  pos = tools_posrelat(agn_checknumber(L, 2), twoargsonly ? CHAR_BIT*buf->size : buf->size);
  if (twoargsonly) {
    IQR(pos, place);
  } else {
    place = agn_checkposint(L, 3);
  }
  if (place > CHAR_BIT*CHARSIZE || pos == 0 || pos > buf->size)
    luaL_error(L, "Error in " LUA_QS ": invalid position %d given.", "memfile.getbit", place);
  byte = uchar(buf->data[pos - 1]);
  lua_pushinteger(L, ((LUAI_UINT32)(byte) & (1 << ((LUAI_UINT32)(place) - 1))) != 0);
  return 1;
}


/* Returns the bit stored at absolute bit position n in the memfile. The return is either 1 or 0. */
static int memfile_getfield (lua_State *L) {  /* 2.32.2 */
  Charbuf *buf;
  ptrdiff_t pos;
  LUAI_UINT32 place;
  unsigned char byte;
  int nargs = lua_gettop(L);
  buf = checkmemfile(L, 1);
  if (nargs == 2 && !agn_isinteger(L, 2)) {  /* 4.3.2 call OOP method, 4.4.1 change */
    return agn_initmethodcall(L, "memfile", 7);
  }
  pos = tools_posrelat(agn_checknumber(L, 2), CHAR_BIT*buf->size);
  IQR(pos, place);
  if (nargs > 2)
    luaL_error(L, "Error in " LUA_QS ": invalid " LUA_QS " position given.", "memfile.getfield", "to");
  if (place > CHAR_BIT*CHARSIZE || pos == 0 || pos > buf->size)
    luaL_error(L, "Error in " LUA_QS ": invalid position %d given.", "memfile.getfield", place);
  byte = uchar(buf->data[pos - 1]);
  lua_pushinteger(L, ((LUAI_UINT32)(byte) & (1 << ((LUAI_UINT32)(place) - 1))) != 0);
  return 1;
}


/* From memfile, returns the substring at position p to position q, with p, q <> 0. If p or q are negative, the respective positions
   are relative to the end of the string. q is p by default. */
static int memfile_substring (lua_State *L) {
  ptrdiff_t pos, stop;
  size_t n;
  Charbuf *buf = checkmemfile(L, 1);
  if (buf->size == 0) {  /* 2.38.0 */
    lua_pushnil(L);
    return 1;
  }
  if (lua_gettop(L) == 1) {  /* 2.38.0 */
    pos = 1;
    stop = buf->size;
  } else {
    pos = tools_posrelat(lua_tointeger(L, 2), buf->size);  /* 7.1.1 fix */
    stop = tools_posrelat(agnL_optinteger(L, 3, pos), buf->size);  /* 2.17.9 fix */
  }
  n = stop - pos + 1;
  if (pos == 0 || stop == 0 || stop < pos || pos + n - 1 > buf->size)
    luaL_error(L, "Error in " LUA_QS ": invalid position(s) given.", "memfile.substring");
  else
    lua_pushlstring(L, buf->data + pos - 1, n);
  return 1;
}


/* Searches memfile for a substring s and returns its position, an integer, or `null` if the string has not been found. The optional
   argument pos indicates the position where the search starts, and is 1 by default. See also: `memfile.substring`. 2.26.3 */
static int memfile_find (lua_State *L) {
  ptrdiff_t pos;
  int freeme;
  size_t l;
  const char *r;
  char *str = NULL;
  Charbuf *buf = checkmemfile(L, 1);
  luaL_argcheck(L, agn_isstring(L, 2) || lua_isseq(L, 2), 2, "string or sequence expected");
  str = aux_fetchdata(L, 2, 0, &l, &freeme, "memfile.find");  /* 2.37.4 extension */
  pos = tools_posrelat(luaL_optinteger(L, 3, 1), buf->size);
  if (pos == 0 || pos + l - 1 > buf->size || buf->size == 0) {
    if (freeme) xfree(str);
    luaL_error(L, "Error in " LUA_QS ": empty buffer, invalid position or needle too long.", "memfile.find");
  }
  if (l == 0) {  /* 2.38.0 */
    if (freeme) xfree(str);
    lua_pushnil(L);
    return 1;
  }
  r = tools_lmemfind(buf->data + pos - 1, buf->size - pos + 1, str, l);
  if (freeme) xfree(str);
  if (!r)
    lua_pushnil(L);
  else
    lua_pushinteger(L, r - buf->data + 1);
  return 1;
}


static int memfile_mfind (lua_State *L) {  /* 2.38.0 */
  ptrdiff_t pos;
  int freeme, hasspecials;
  size_t l, c;
  const char *r;
  char *str = NULL;
  Charbuf *buf = checkmemfile(L, 1);
  luaL_argcheck(L, agn_isstring(L, 2) || lua_isseq(L, 2), 2, "string or sequence expected");
  str = aux_fetchdata(L, 2, 0, &l, &freeme, "memfile.mfind");  /* 2.37.4 extension */
  pos = tools_posrelat(luaL_optinteger(L, 3, 1), buf->size);
  if (pos == 0 || pos + l - 1 > buf->size || buf->size == 0) {
    if (freeme) xfree(str);
    luaL_error(L, "Error in " LUA_QS ": empty buffer, invalid position or needle too long.", "memfile.mfind");
  }
  if (l == 0) {  /* 2.38.0 */
    if (freeme) xfree(str);
    lua_pushnil(L);
    return 1;
  }
  c = 0;
  agn_createseq(L, 1);
  if (!agnL_isvalidpattern(L, str, l, &hasspecials, 1)) {  /* 6.1.1 fix */
    if (freeme) xfree(str);
    luaL_error(L, "Error in " LUA_QS ": %s", "memfile.mfind", agn_tostring(L, -1));
  }
  if (!hasspecials) {
    while ( (pos + l - 1 <= buf->size) &&  (r = tools_lmemfind(buf->data + pos - 1, buf->size - pos + 1, str, l) ) ) {
      agn_createpairnumbers(L, r - buf->data + 1, r - buf->data + l);
      lua_seqrawseti(L, -2, ++c);
      pos = r - buf->data + l + 1;
    }
  } else {
    ptrdiff_t start, end;
    while ( (pos <= buf->size) &&  (r = agn_strmatch(L, buf->data, buf->size, str, pos, &start, &end) ) ) {
      agn_createpairnumbers(L, start, end);
      lua_seqrawseti(L, -2, ++c);
      pos = r - buf->data + end - start + 1 + 1;
    }
  }
  if (c == 0) {
    agn_poptop(L);
    lua_pushnil(L);
  }
  if (freeme) xfree(str);
  return 1;
}


static int memfile_match (lua_State *L) {  /* 2.38.0 */
  ptrdiff_t pos, start, end;
  int freeme;
  size_t l;
  const char *r;
  char *str = NULL;
  Charbuf *buf = checkmemfile(L, 1);
  luaL_argcheck(L, agn_isstring(L, 2) || lua_isseq(L, 2), 2, "string or sequence expected");
  str = aux_fetchdata(L, 2, 0, &l, &freeme, "memfile.match");
  pos = tools_posrelat(luaL_optinteger(L, 3, 1), buf->size);
  if (pos == 0 || pos + l - 1 > buf->size || buf->size == 0) {
    if (freeme) xfree(str);
    luaL_error(L, "Error in " LUA_QS ": empty buffer, invalid position or needle too long.", "memfile.match");
  }
  if (l == 0) {  /* 2.38.0 */
    if (freeme) xfree(str);
    lua_pushnil(L);
    return 1;
  }
  if (tools_hasstrchr(str, SPECIALS, l))
    r = agn_strmatch(L, buf->data + pos - 1, buf->size - pos + 1, str, pos, &start, &end);
  else {
    r = tools_lmemfind(buf->data + pos - 1, buf->size - pos + 1, str, l);
    start = r - buf->data + 1;
    end = start + l - 1;
  }
  if (freeme) xfree(str);
  if (!r)
    lua_pushnil(L);
  else {
    lua_pushinteger(L, start);
    lua_pushinteger(L, end);
    lua_pushlstring(L, r, end - start + 1);
  }
  return 1 + 2*(!!r);
}


/* Sets a substring str into memfile at position pos, with pos <> 0. If the substring is too long, the function issues
   an error. If pos is negative, the position is relative to the end of the string. The function returns nothing.*/
#define checkpos(buf,pos,n) { \
  if (pos == 0 || pos + n - 1 > buf->size) \
    luaL_error(L, "Error in " LUA_QS ": invalid position given or string too long.", "memfile.setitem"); \
}
static int memfile_setitem (lua_State *L) {
  const char *str;
  ptrdiff_t pos;
  size_t n;
  Charbuf *buf = checkmemfile(L, 1);
  agnL_checkudfreezed(L, 1, "buffer", "memfile.setitem");  /* 4.9.0 */
  pos = tools_posrelat(agn_checknumber(L, 2), buf->size);
  if (agn_isstring(L, 3)) {
    str = lua_tolstring(L, 3, &n);
    checkpos(buf, pos, n);
    tools_memcpy(buf->data + pos - 1, str, n);  /* 2.21.5 tweak */
  } else if (agn_isinteger(L, 3)) {  /* 2.27.2 */
    char str[2];
    str[0] = (char)lua_tointeger(L, 3);
    str[1] = '\0';
    n = 1;
    checkpos(buf, pos, n);
    tools_memcpy(buf->data + pos - 1, str, n);  /* 2.21.5 tweak */
  } else
    luaL_error(L, "Error in " LUA_QS ": item must be either a string or an integer.", "memfile.setitem");
  return 0;
}


/* Sets the character c of type string into memfile at position pos, with pos <> 0. If pos is
   negative, the position is relative to the end of the string. If c is longer than one character only the
   first character is written to the memfile.

   If a fourth argument count is given, then the function sets count characters - starting from position pos -
   to the given letter. By default, count is 1. If pos has not been set before, the function fills all preceding
   positions with white spaces, if they have not yet been already set.

   The function returns nothing. 2.26.2 */
static int memfile_setchar (lua_State *L) {
  const char *str;
  ptrdiff_t pos, upuntil, nbytes;
  Charbuf *buf = checkmemfile(L, 1);
  agnL_checkudfreezed(L, 1, "buffer", "memfile.setchar");  /* 4.9.0 */
  pos = tools_posrelat(agn_checknumber(L, 2), buf->size);
  str = agn_checkstring(L, 3);
  nbytes = agnL_optposint(L, 4, 1); /* number of bytes, added 2.27.2 */
  upuntil = pos + nbytes - 1;
  if (pos == 0 || upuntil == 0 || upuntil < pos || pos > buf->capacity || upuntil > buf->capacity)
    luaL_error(L, "Error in " LUA_QS ": invalid position given.", "memfile.setchar");
  if (pos + nbytes - 1 > buf->capacity)
    luaL_error(L, "Error in " LUA_QS ": buffer not large enough, please resize it.", "memfile.setchar");
  if (pos > buf->size) {  /* fill vacant space with zeros */
    ptrdiff_t nzeros = pos - buf->size - 1;
    memset(buf->data + buf->size, ' ', nzeros);
    buf->size += nzeros;
  }
  if (nbytes == 1)
    buf->data[pos - 1] = str[0];
  else {
    memset(buf->data + pos - 1, str[0], nbytes);
    buf->size += nbytes;
  }
  return 0;
}


/* Sets the byte b of type (non-negative) integer into memfile at position pos, with pos <> 0.
   If pos is negative, the position is relative to the end of the string. b should be an integer in the range
   [0, 255]. If a fourth argument count is given, then the function sets count bytes - starting from position pos -
   to the given byte. By default, count is 1. If pos has not been set before, the function fills all preceding
   positions with zeros, if they have not yet been already set to any byte. The function returns nothing. 2.26.2 */
static int memfile_setbyte (lua_State *L) {
  unsigned char b;
  ptrdiff_t pos, upto, nbytes;
  Charbuf *buf = checkmemfile(L, 1);
  agnL_checkudfreezed(L, 1, "buffer", "memfile.setbyte");  /* 4.9.0 */
  pos = tools_posrelat(agn_checknumber(L, 2), buf->size);
  b = uchar(agn_checknonnegint(L, 3));  /* the byte */
  nbytes = agnL_optposint(L, 4, 1); /* number of bytes, added 2.27.2 */
  upto = pos + nbytes - 1;
  if (pos == 0 || upto == 0 || upto < pos || buf->size == 0)  /* 2.37.3 fix */
    luaL_error(L, "Error in " LUA_QS ": invalid position %d given.", "memfile.setbyte", pos);
  if (upto > buf->capacity)
    luaL_error(L, "Error in " LUA_QS ": buffer not large enough, you may resize it.", "memfile.setbyte");
  if (pos > buf->size) {  /* fill vacant space with zeros */
    ptrdiff_t nzeros = pos - buf->size - 1;
    memset(buf->data + buf->size, 0, nzeros);
    buf->size += nzeros;
  }
  if (nbytes == 1) {
    buf->data[pos - 1] = (char)b;
  } else {
    memset(buf->data + pos - 1, (char)b, nbytes);
  }
  if (buf->size < upto) buf->size += nbytes;  /* 2.37.3 fix */
  return 0;
}


static int memfile_setbytes (lua_State *L) {  /* 5.5.13, based on memfile_setbyte */
  int t, x, rc;
  ptrdiff_t i, pos, upto, nbytes;
  Charbuf *buf = checkmemfile(L, 1);
  agnL_checkudfreezed(L, 1, "buffer", "memfile.setbytes");
  pos = tools_posrelat(agn_checknumber(L, 2), buf->size);
  t = lua_type(L, 3);
  luaL_typecheck(L, t == LUA_TTABLE || t == LUA_TSEQ || t == LUA_TREG, 3, "table, sequence or register expected", t);
  nbytes = agn_nops(L, 3);
  upto = pos + nbytes - 1;
  if (upto > buf->capacity)
    luaL_error(L, "Error in " LUA_QS ": buffer not large enough, you may resize it.", "memfile.setbytes");
  if (pos == 0 || upto == 0 || upto < pos || buf->size == 0)
    luaL_error(L, "Error in " LUA_QS ": invalid position %d given or structure empty.", "memfile.setbytes", pos);
  if (pos > buf->size) {  /* fill vacant space with zeros */
    ptrdiff_t nzeros = pos - buf->size - 1;
    memset(buf->data + buf->size, 0, nzeros);
    buf->size += nzeros;
  }
  switch (t) {
    case LUA_TTABLE: {
      for (i=pos - 1; i < pos + nbytes - 1; i++) {
        x = agn_rawgetiinteger(L, 3, i - pos + 2, &rc);
        if (!rc || x < 0 || x > 255) {
          luaL_error(L, "Error in " LUA_QS ": value must be in 0 .. 255.", "memfile.setbytes");
        }
        buf->data[i] = uchar(x);
      }
      break;
    }
    case LUA_TSEQ: {
      for (i=pos - 1; i < pos + nbytes - 1; i++) {
        x = agn_seqrawgetiinteger(L, 3, i - pos + 2, &rc);
        if (!rc || x < 0 || x > 255) {
          luaL_error(L, "Error in " LUA_QS ": value must be in 0 .. 255.", "memfile.setbytes");
        }
        buf->data[i] = uchar(x);
      }
      break;
    }
    case LUA_TREG: {
      for (i=pos - 1; i < pos + nbytes - 1; i++) {
        x = agn_regrawgetiinteger(L, 3, i - pos + 2, &rc);
        if (!rc || x < 0 || x > 255) {
          luaL_error(L, "Error in " LUA_QS ": value must be in 0 .. 255.", "memfile.setbytes");
        }
        buf->data[i] = uchar(x);
      }
      break;
    }
    default:
      luaL_error(L, "Error in " LUA_QS ": this should not happen.", "memfile.setbytes");
  }
  if (buf->size < upto) buf->size += nbytes;
  return 0;
}


/* In the first form, sets bit position n, from the right end of the field, in the memfile to 1.
   In the second form, in byte no. b of the memfile m, sets the n-th bit to val, where val is 
   either a Boolean or 0 or 1. If val is omitted, sets the bit to 1.
   The return is the modified byte no. b. 2.26.2 */
static int memfile_setbit (lua_State *L) {
  ptrdiff_t pos;
  size_t place;
  int flag;
  unsigned char byte;
  int twoargsonly = lua_gettop(L) == 2;
  Charbuf *buf = checkmemfile(L, 1);
  agnL_checkudfreezed(L, 1, "buffer", "memfile.setbit");  /* 4.9.0 */
  pos = tools_posrelat(agn_checknumber(L, 2), twoargsonly ? CHAR_BIT*buf->size : buf->size);
  if (twoargsonly) {
    IQR(pos, place);
  } else {
    place = agn_checkposint(L, 3);
  }
  if (place > CHAR_BIT*CHARSIZE || pos == 0 || pos > buf->size)
    luaL_error(L, "Error in " LUA_QS ": invalid position given.", "memfile.getbit");
  if (lua_gettop(L) > 3) {
    switch (lua_type(L, 4)) {
      case LUA_TBOOLEAN:
        flag = lua_toboolean(L, 4);  /* true or false passed ? */
        if (flag < 0)  /* fail ? */
          luaL_error(L, "Error in " LUA_QS ": fourth argument must be either Boolean, or 0 or 1.", "memfile.setbit");
        break;
      case LUA_TNUMBER:
        flag = lua_tointeger(L, 4);  /* 1 or 0 passed ? */
        if (flag != 0 && flag != 1)
          luaL_error(L, "Error in " LUA_QS ": fourth argument must be either Boolean, or 0 or 1.", "memfile.setbit");
        break;
      default: {
        flag = 0;  /* to prevent compiler warnings */
        luaL_error(L, "Error in " LUA_QS ": fourth argument must be either Boolean, or 0 or 1.", "memfile.setbit");
      }
    }
  } else
    flag = 1;
  byte = uchar(buf->data[pos - 1]);
  byte = uchar(((LUAI_UINT32)byte & ~(1 << ((LUAI_UINT32)place - 1))) | (flag << ((LUAI_UINT32)place - 1)));
  buf->data[pos - 1] = byte;
  lua_pushinteger(L, byte);
  return 1;
}


static int memfile_setfield (lua_State *L) {  /* 2.31.2 */
  Charbuf *buf;
  ptrdiff_t pos;
  size_t place;
  int flag;
  unsigned char byte;
  if (lua_gettop(L) != 3)
    luaL_error(L, "Error in " LUA_QS ": need three arguments.", "memfile.setfield");
  buf = checkmemfile(L, 1);
  agnL_checkudfreezed(L, 1, "buffer", "memfile.setfield");  /* 4.9.0 */
  pos = tools_posrelat(agn_checkinteger(L, 2), CHAR_BIT*buf->size);
  IQR(pos, place);
  if (place > CHAR_BIT*CHARSIZE || pos == 0 || pos > buf->size)
    luaL_error(L, "Error in " LUA_QS ": invalid position given.", "memfile.setfield");
  switch (lua_type(L, 3)) {
    case LUA_TBOOLEAN:
      flag = lua_toboolean(L, 3);  /* true or false passed ? */
      if (flag < 0)  /* fail ? */
        luaL_error(L, "Error in " LUA_QS ": third argument must be either Boolean, or 0 or 1.", "memfile.setfield");
      break;
    case LUA_TNUMBER:
      flag = lua_tointeger(L, 3);  /* 1 or 0 passed ? */
      if (flag != 0 && flag != 1)
        luaL_error(L, "Error in " LUA_QS ": third argument must be either Boolean, or 0 or 1.", "memfile.setfield");
      break;
    default: {
      flag = 0;  /* to prevent compiler warnings */
      luaL_error(L, "Error in " LUA_QS ": third argument must be either Boolean, or 0 or 1.", "memfile.setfield");
    }
  }
  byte = uchar(buf->data[pos - 1]);
  byte = uchar(((LUAI_UINT32)byte & ~(1 << ((LUAI_UINT32)place - 1))) | (flag << ((LUAI_UINT32)place - 1)));
  buf->data[pos - 1] = byte;
  lua_pushinteger(L, byte);
  return 1;
}


/* In the first form, unsets the n-th bit,from the right end of the field, in the memfile, i.e. sets it to zero.
   In the second form, in byte no. b of the memfile m, clears the n-th bit.
   The return is the modified byte no. b. 2.26.2 */
static int memfile_clearbit (lua_State *L) {
  ptrdiff_t pos;
  size_t place;
  unsigned char byte;
  int twoargsonly = lua_gettop(L) == 2;
  Charbuf *buf = checkmemfile(L, 1);
  agnL_checkudfreezed(L, 1, "buffer", "memfile.clearbit");  /* 4.9.0 */
  pos = tools_posrelat(agn_checknumber(L, 2), twoargsonly ? CHAR_BIT*buf->size : buf->size);
  if (lua_gettop(L) == 2) {
    IQR(pos, place);
  } else {
    place = agn_checkposint(L, 3);
  }
  if (place > CHAR_BIT*CHARSIZE || pos == 0 || pos > buf->size)
    luaL_error(L, "Error in " LUA_QS ": invalid position given.", "memfile.clearbit");
  byte = uchar(buf->data[pos - 1]);
  byte = uchar((LUAI_UINT32)byte & ~(1 << ((LUAI_UINT32)place - 1)));
  buf->data[pos - 1] = byte;
  lua_pushinteger(L, byte);
  return 1;
}


static int iterate (lua_State *L) {
  int pushidx, nrets = 1;
  size_t n, flag;
  ptrdiff_t pos, bufsize, i;
  Charbuf *buf = (Charbuf *)lua_touserdata(L, lua_upvalueindex(1));
  if (buf == NULL) {
    lua_pushnil(L);
    return 1;
  }
  bufsize = buf->size;
  pos = lua_tonumber(L, lua_upvalueindex(2));
  n = lua_tointeger(L, lua_upvalueindex(3));
  flag = lua_tointeger(L, lua_upvalueindex(4));
  pushidx = lua_tointeger(L, lua_upvalueindex(6));
  if (flag && pos <= bufsize && pos + n - 1 > bufsize) {  /* shorten last chunk */
    n = bufsize - pos + 1;
    lua_pushinteger(L, 0);
    lua_replace(L, lua_upvalueindex(4));
  }
  if (pushidx) {
    luaL_checkstack(L, 1, "not enough stack space");
    lua_pushinteger(L, pushidx++);
    lua_pushinteger(L, pushidx);
    lua_replace(L, lua_upvalueindex(6));
  }
  if (pos + n - 1 > bufsize) {
    lua_pushnil(L);
    if (pushidx) {  /* restart, 7.1.1 */
      lua_pushinteger(L, 1);
      lua_replace(L, lua_upvalueindex(4));
      lua_pushvalue(L, lua_upvalueindex(5));
      lua_replace(L, lua_upvalueindex(2));
      lua_pushvalue(L, lua_upvalueindex(5));
      lua_replace(L, lua_upvalueindex(6));
      pushidx = 0;
    }
  } else {
    lua_pushnumber(L, pos + n);
    lua_replace(L, lua_upvalueindex(2));
    switch (buf->type) {  /* 5.5.14 change */
      case MEMFILE_CHARBUF:  /* character buffer ? */
        lua_pushlstring(L, buf->data + pos - 1, n);
        break;
      case MEMFILE_BYTEBUF:  /* byte buffer ?  */
        nrets = n;
        luaL_checkstack(L, n, "not enough stack space");
        for (i=pos - 1; i < pos + n - 1; i++) {
          lua_pushinteger(L, uchar(*(buf->data + i)));
        }
        break;
      default:
        luaL_error(L, "Error in " LUA_QS ": invalid memfile type.", "memfile.iterator");
    }
  }
  return nrets + (pushidx != 0);
}

/* Returns an iterator function that, when called returns the next n characters stored in memfile, starting at position pos.
   If there are no more characters, the factory returns `null`. */
static int memfile_iterate (lua_State *L) {
  Charbuf *buf = checkmemfile(L, 1);
  ptrdiff_t pos = (ptrdiff_t)tools_posrelat(agnL_optposint(L, 2, 1), buf->size);
  size_t chunks = (size_t)agnL_optposint(L, 3, 1);
  int pushidx = agnL_optboolean(L, 4, 0);
  if (pos < 1)
    luaL_error(L, "Error in " LUA_QS ": start index must be positive.", "memfile.iterate");
  luaL_checkstack(L, 6, "not enough stack space");  /* 3.18.4 fix */
  lua_pushvalue(L, 1);                    /* 1 memfile */
  lua_pushnumber(L, pos);                 /* 2 start position */
  lua_pushnumber(L, chunks);              /* 3 number of chars per call */
  lua_pushinteger(L, 1);                  /* 4 not-end-of-string flag */
  lua_pushnumber(L, pos);                 /* 5 start position backup */
  lua_pushinteger(L, pushidx ? pos : 0);  /* 6 return index and value */
  lua_pushcclosure(L, &iterate, 6);
  return 1;
}


/* memfile.read (fh, memfile [, bufsize])

Reads data from the file denoted by its filehandle fh into the given memfile userdata.

The file should have previously been opened with `binio.open` and should finally be closed with `binio.close`.

By default, the function reads in the entire file if `bufsize` is not given.

If a positive integer has been passed with `bufsize`, the function only reads in the given number of bytes
per each call, returns the number of bytes actually read and increments the file position thereafter so that the
next bytes in the file can be read with a new call to `memfile.read`.

(Passing the bufsize argument may also be necessary if your platform requires that an internal input
buf is aligned to a certain block size.)

If the end of the file has been reached, or there is nothing to read at all, null is returned.
In case of an error, it quits with the respective error.

Example:

> import memfile;
> m := memfile.charbuf();
> print(size m);  # should be zero
> fd := binio.open('memfile.bin');
> pos := 1;
> # the following loop is equivalent to the simple call `memfile.read(fd, m)`:
> do
>    pos := memfile.read(fd, m, 512)  # read 512 bytes per each call
> until pos = null;
> binio.close(fd);   # should be non-zero
> print(size m);
*/

static int memfile_read (lua_State *L) {
  int hnd, n, en, s, nbytes, all;
  Charbuf *buf;
  char *buffer;
  all = lua_gettop(L) == 2;
  hnd = agn_tofileno(L, 1, 1);
  if (hnd == -1) luaL_error(L, "Error in " LUA_QS ": file handle is invalid or closed.", "memfile.read");  /* 2.37.5 */
  /* reading a lot of bytes at one stroke will cause block size errors, so keep the buffer small */
  buf = checkmemfile(L, 2);
  s = luaL_optinteger(L, 3, agn_getbuffersize(L));
  if (s == -1)
    luaL_error(L, "Error in " LUA_QS " with file #%d: file I/O error.", "memfile.read", hnd);
  else if (s <= 0)
    s = agn_getbuffersize(L);  /* 2.34.9 adaption */
  nbytes = s * CHARSIZE;
  buffer = agn_stralloc(L, nbytes, "memfile.read", NULL);  /* 2.37.4 improvement */
  do {
    set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
    n = read(hnd, buffer, nbytes);
    en = errno;
    if (n > 0) {
      if (!charbuf_append(L, buf, (const char *)buffer, n)) {
        xfree(buffer);
        luaL_error(L, "Error in " LUA_QS " could not append data.", "memfile.read");
      }
      if (!all) { lua_pushnumber(L, n); goto readlabel; }
    } else if (n < 0) {
      xfree(buffer);
      luaL_error(L, "Error in " LUA_QS " with file #%d: %s.", "memfile.read", hnd, my_ioerror(en));
    }
  } while (n > 0 && all);
  lua_pushnil(L);
readlabel:
  xfree(buffer);
  return 1;
}


/* memfile.write (fh, memfile [, pos [, nchars]])

based on `numarray.write`; writes the string in a memfile userdata to the file denoted by its numeric file handle `fh`.
The file should have previously been opened with binio.open and closed with binio.close after completion.

The start position `pos` is 1 by default but can be changed to any other valid position in the memfile.

The number of characters (not necessarily bytes) to be written can be changed by passing an optional fourth argument
`nchars`, a positive number, and by default equals the total number of entries in `memfile`. (Passing the `nchars`
argument may be necessary if your platform requires that buffers must be aligned to a particular block size.)

The function returns the index of the next start position (an integer) for a further call to `memfile.write` to write
further characters, where the return should be passed to the third `pos` argument.

If the end of the string in `memfile` has been reached, the function returns `null` and flushes all unwritten content to
the file so that you do not have to call `binio.sync` manually.

No further information is stored to the file created.

Example on how to write a string of 8,000 characters piece-by-piece:

> import memfile
> m := memfile.charbuf();
> to 1000 do
>    memfile.append(m, 'nasa/jpl')
> od;
> fd := binio.open('memfile.bin');
> pos := 1;
> # The following is equivalent to "memfile.write(fd, m)":
> do  # write 1024 values per each call
>    pos := memfile.write(fd, m, pos, 1024)
> until pos = null;
> binio.close(fd);

Use `binio.sync` if you want to make sure that any unwritten content is written to the file when calling `memfile.write`
multiple times. */

static int memfile_write (lua_State *L) {
  int hnd, en;
  int64_t nchars, start;
  ssize_t nbyteswritten, totnbytes, bufsize, bsize;
  char *p;
  Charbuf *buf;
  hnd = agn_tofileno(L, 1, 1);
  if (hnd == -1) luaL_error(L, "Error in " LUA_QS ": file handle is invalid or closed.", "memfile.write");  /* 2.37.5 */
  buf = checkmemfile(L, 2);
  bufsize = agn_getbuffersize(L);
  start = agnL_optinteger(L, 3, 1) - 1;
  if (start < 0 || start >= buf->size)
    luaL_error(L, "Error in " LUA_QS " with file #%d: start position %d out-of-range.", "memfile.write", hnd, start + 1);
  nchars = agnL_optinteger(L, 4, buf->size);  /* number of values in memfile to be written */
  if (start + nchars > buf->size) nchars = buf->size - start;
  if (nchars < 1)
    luaL_error(L, "Error in " LUA_QS " with file #%d: invalid number of bytes to be written.", "memfile.write", hnd);
  totnbytes = nchars*CHARSIZE;
  p = buf->data + start;  /* 2.37.5 fix */
  while (totnbytes > 0) {
    bsize = totnbytes > bufsize ? bufsize : totnbytes;
    set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
    if ( (nbyteswritten = write(hnd, p, bsize)) == -1) {
      en = errno;
      luaL_error(L, "Error in " LUA_QS " with file #%d: %s.", "memfile.write", hnd, my_ioerror(en));
    }
    if (luai_nummod(nbyteswritten, CHARSIZE) != 0) {
      luaL_error(L, "Error in " LUA_QS " with file #%d: write error.", "memfile.write", hnd);
    }
    p += bsize;
    totnbytes -= bsize;
  }
  if (p >= buf->data + buf->size) {
    lua_pushnil(L);
    tools_fsync(hnd);
  } else {  /* return the next start position for a further call to write further parts of the memfile */
    lua_pushnumber(L, p - buf->data + 1);  /* 2.37.5 change */
  }
  return 1;
}


/* memfile.map (f, memfile [, ···])
   Maps a function f on each character in `memfile`, in-place. `f` must always return a number or a string.
   2.17.9 */
static int memfile_map (lua_State *L) {
  Charbuf *buf;
  size_t i, j, nargs;
  luaL_checktype(L, 1, LUA_TFUNCTION);
  buf = checkmemfile(L, 2);
  agnL_checkudfreezed(L, 2, "buffer", "memfile.map");  /* 4.9.0 */
  nargs = lua_gettop(L);
  for (i=0; i < buf->size; i++) {
    luaL_checkstack(L, 2 + (nargs > 2)*(nargs - 2), "too many arguments");  /* 3.5.5/3.15.2 fix */
    lua_pushvalue(L, 1);   /* push function */
    lua_pushchar(L, buf->data[i]);
    for (j=3; j <= nargs; j++)
      lua_pushvalue(L, j);
    lua_call(L, nargs - 1, 1);
    if (agn_isstring(L, -1)) {
      buf->data[i] = *agn_tostring(L, -1);
    } else if (lua_isnumber(L, -1)) {
      buf->data[i] = (char)agn_tonumber(L, -1);
    } else {
      agn_poptop(L);
      luaL_error(L, "Error in " LUA_QS " function must evaluate to either a string or number.", "memfile.map");
    }
    agn_poptop(L);
  }
  return 1;
}


/* Copies all characters in `memfile` to a new memory file and returns it. 2.17.9 */
static int memfile_replicate (lua_State *L) {
  Charbuf *bufin, *bufout;
  bufin = checkmemfile(L, 1);
  agnL_checkudfreezed(L, 1, "buffer", "memfile.replicate");  /* 4.9.0 */
  bufout = (Charbuf *)lua_newuserdata(L, sizeof(Charbuf));
  if (bufout) {
    charbuf_init(L, bufout, bufin->type, bufin->capacity, 0);
    /* charbuf_init has its own error handling */
    bufout->originalcapacity = bufin->originalcapacity;
  }
  else
    luaL_error(L, "Error in " LUA_QS ": memory allocation failed.", "memfile.replicate");
  lua_setmetatabletoobject(L, -1, "memfile", 1);  /* assign the metatable defined below to the userdata */
  charbuf_append(L, bufout, bufin->data, bufin->size);
  return 1;
}


/* Returns the total capacity of a memfile and the current number of characters in it. 2.26.2 */
static int memfile_attrib (lua_State *L) {
  Charbuf *buf;
  buf = checkmemfile(L, 1);
  luaL_checkstack(L, 4, "not enough stack space");  /* 4.7.1 fix */
  lua_pushinteger(L, buf->capacity);
  lua_pushinteger(L, buf->size);
  lua_pushstring(L, buf->type ? "bytebuf" : "charbuf");  /* 2.26.3 */
  lua_pushinteger(L, buf->originalcapacity);  /* 2.26.3 */
  return 4;
}


/* Adapted from https://betterprogramming.pub/3-ways-to-rotate-an-array-2a45b39f7bec, by Alisa Bajramovic. */
void reverse (char *str, size_t start, size_t end) {
  char t;
  while (start < end) {
    t = str[start];
    str[start] = str[end];
    str[end] = t;
    start++;
    end--;
  }
}

void rotateright (char *str, size_t l, size_t k) {
  k = k % l;
  reverse(str, 0, l - 1);
  reverse(str, 0, k - 1);
  reverse(str, k, l - 1);
}

void rotateleft (char *str, size_t l, size_t k) {
  k = k % l;
  reverse(str, k, l - 1);
  reverse(str, 0, k - 1);
  reverse(str, 0, l - 1);
}

/* Rotates the contents of the buffer memfile n places to the right if n is positive, and n places to the left
   if n is negative. The function returns nothing. 2.26.3 */
static int memfile_shift (lua_State *L) {
  Charbuf *buf;
  size_t l;
  int64_t n;
  buf = checkmemfile(L, 1);
  agnL_checkudfreezed(L, 1, "buffer", "memfile.shift");  /* 4.9.0 */
  n = agnL_optinteger(L, 2, 1);
  l = buf->size;
  if (n == 0 || l == 0 || (int64_t)n > (int64_t)l || -(int64_t)n > (int64_t)l) {
    luaL_error(L, "Error in " LUA_QS ": invalid argument #2 or empty buffer.", "memfile.shift");
    return 0;  /* do nothing */
  }
  if (n < 0)  /* shift left */
    rotateleft(buf->data, l, -n);
  else
    rotateright(buf->data, l, n);  /* right shift */
  return 0;
}


static int memfile_reverse (lua_State *L) {  /* 2.27.4 */
  Charbuf *buf;
  buf = checkmemfile(L, 1);
  reverse(buf->data, 0, buf->size - 1);
  return 0;
}


/* --- Metamethods -----------------------------------------------------------------------------------------------*/

static int mt_index (lua_State *L) {  /* 5.5.14 */
  ptrdiff_t pos, stop;
  size_t n;
  int nargs = lua_gettop(L);
  Charbuf *buf = checkmemfile(L, 1);
  if (nargs == 2 && !agn_isinteger(L, 2)) {  /* OOP method */
    return agn_initmethodcall(L, "memfile", 7);
  }
  pos = tools_posrelat(agn_checkinteger(L, 2), buf->size);
  stop = (buf->type == MEMFILE_BYTEBUF) ? pos : tools_posrelat(agnL_optinteger(L, 3, pos), buf->size);
  n = stop - pos + 1;
  if (pos == 0 || stop == 0 || stop < pos || pos + n - 1 > buf->size ||
     (buf->type == MEMFILE_BYTEBUF && lua_gettop(L) > 2)) {
    luaL_error(L, "Error in " LUA_QS ": invalid position(s) given.", "index operation");
  }
  switch (buf->type) {
    case MEMFILE_CHARBUF:  /* character buffer ? */
      lua_pushlstring(L, buf->data + pos - 1, n);
      break;
    case MEMFILE_BYTEBUF:  /* byte buffer ?  */
      lua_pushinteger(L, uchar(buf->data[pos - 1]));
      break;
    default:
      luaL_error(L, "Error in " LUA_QS ": invalid memfile type.", "index operation");
  }
  return 1;
}


static int mt_getsize (lua_State *L) {
  Charbuf *buf = checkmemfile(L, 1);
  lua_pushnumber(L, buf->size);
  return 1;  /* changed 5.5.14, can only return one value */
}


static int mt_bitfieldgetsize (lua_State *L) {
  Charbuf *buf = checkmemfile(L, 1);
  lua_pushnumber(L, CHAR_BIT*buf->size);
  return 1;  /* changed 5.5.14, can only return one value */
}


static int mt_empty (lua_State *L) {
  Charbuf *buf = checkmemfile(L, 1);
  lua_pushboolean(L, buf->size == 0);
  return 1;
}


static int mt_filled (lua_State *L) {
  Charbuf *buf = checkmemfile(L, 1);
  lua_pushboolean(L, buf->size != 0);
  return 1;
}


static int mt_in (lua_State *L) {
  Charbuf *buf;
  const char *r;
  size_t l;
  if (lua_gettop(L) != 2)
    luaL_error(L, "Error in " LUA_QS ": need two arguments.", "memfile.__in");
  buf = checkmemfile(L, 2);
  if (lua_isstring(L, 1)) {
    const char *str = lua_tolstring(L, 1, &l);
    r = (l == 0) ? NULL : tools_lmemfind(buf->data, buf->size, str, l);  /* 2.38.0, return null with an empty string as left op */
  } else if (agn_isinteger(L, 1)) {
    char str[2];
    str[0] = (char)lua_tointeger(L, 1);
    str[1] = '\0';
    r = tools_lmemfind(buf->data, buf->size, str, 1);
  } else {
    r = buf->data;
    luaL_error(L, "Error in " LUA_QS ": invalid argument.", "memfile.__in");
  }
  if (!r)
    lua_pushnil(L);
  else
    lua_pushnumber(L, r - buf->data + 1);
  return 1;
}


static int mt_notin (lua_State *L) {
  Charbuf *buf;
  const char *r;
  size_t l;
  if (lua_gettop(L) != 2)
    luaL_error(L, "Error in " LUA_QS ": need two arguments.", "memfile.__notin");
  buf = checkmemfile(L, 2);
  if (lua_isstring(L, 1)) {
    const char *str = lua_tolstring(L, 1, &l);
    r = (l == 0) ? NULL : tools_lmemfind(buf->data, buf->size, str, l);  /* 2.38.0, return true with an empty string as left op */
  } else if (agn_isinteger(L, 1)) {
    char str[2];
    str[0] = (char)lua_tointeger(L, 1);
    str[1] = '\0';
    r = tools_lmemfind(buf->data, buf->size, str, 1);
  } else {
    r = buf->data;
    luaL_error(L, "Error in " LUA_QS ": invalid argument.", "memfile.__notin");
  }
  lua_pushboolean(L, !r);
  return 1;
}


static int mt_eq (lua_State *L) {
  Charbuf *buf1, *buf2;
  int r, nargs;
  nargs = lua_gettop(L);
  r = 1;
  if (nargs != 2)
    luaL_error(L, "Error in " LUA_QS ": need two arguments.", "memfile.__eq");
  buf1 = checkmemfile(L, 1);
  buf2 = checkmemfile(L, 2);
  r = (buf1->size == buf2->size) ? tools_memcmp(buf1->data, buf2->data, buf2->size) : 1;
  lua_pushboolean(L, r == 0);
  return 1;
}


static int mt_tostring (lua_State *L) {  /* at the console, the array is formatted as follows: */
  if (luaL_isudata(L, 1, "memfile"))
    lua_pushfstring(L, "memfile(%p)", lua_topointer(L, 1));
  else {
    void *p = lua_touserdata(L, 1);
    lua_pushfstring(L, (p != NULL) ? "userdata(%p)" : "unknown(%p)", lua_topointer(L, 1));
  }
  return 1;
}


static int mt_bitfieldtostring (lua_State *L) {  /* at the console, the array is formatted as follows: */
  if (luaL_isudata(L, 1, "bitfield")) {
    unsigned char byte;
    int c, i, j;
    Charbuf *buf = lua_touserdata(L, 1);
    luaL_checkstack(L, CHAR_BIT + 2, "not enough stack space");  /* 2.31.7 fix */
    lua_pushfstring(L, "bitfield(");
    c = 1;
    for (i=buf->size - 1; i >= 0; i--) {  /* 7.7.8 change, print in natural order from the right end of the field */
      byte = uchar(buf->data[i]);
      lua_pushfstring(L, "0b"); c++;
      for (j=CHAR_BIT-1; j >= 0; j--) {
        lua_pushfstring(L, "%d", ((LUAI_UINT32)(byte) & (1 << ((LUAI_UINT32)j))) != 0);
        c++;
      }
      lua_concat(L, c); c = 1;
      if (i != 0) {
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
  Charbuf *buf = checkmemfile(L, 1);
  charbuf_finish(buf);  /* better be sure than sorry, 2.39.12 */
  charbuf_free(buf);
  return 0;
}


/* ---------------------------------------------------------------------------------------------------------------*/

static const struct luaL_Reg memfile_memlib [] = {  /* metamethods for memfile `n' */
  {"__index",      mt_index},           /* n[p], with p the index, counting from 1 */
  {"__writeindex", memfile_setitem},    /* n[p] := value, with p the index, counting from 1 */
  {"__size",       mt_getsize},         /* retrieve the number of bytes in `n' */
  {"__in",         mt_in},              /* `in` operator for memfiles */
  {"__notin",      mt_notin},           /* `in` operator for memfiles */
  {"__gc",         mt_gc},              /* please do not forget garbage collection */
  {"__eq",         mt_eq},              /* metamethod for `=` operator */
  {"__empty",      mt_empty},           /* metamethod for `empty` operator */
  {"__filled",     mt_filled},          /* metamethod for `filled` operator */
  {"__tostring",   mt_tostring},        /* for output at the console, e.g. print(n) */
  {NULL, NULL}
};


static const struct luaL_Reg bitfield_memlib [] = {  /* metamethods for bitfield `n' */
  {"__index",      memfile_getfield},   /* n[p], with p the index, counting from 1 */
  {"__writeindex", memfile_setfield},   /* n[p] := value, with p the index, counting from 1 */
  {"__size",       mt_bitfieldgetsize}, /* retrieve the number of bytes in `n' */
  {"__in",         mt_in},              /* `in` operator for bitfields */
  {"__notin",      mt_notin},           /* `in` operator for bitfields */
  {"__gc",         mt_gc},              /* please do not forget garbage collection */
  {"__eq",         mt_eq},              /* metamethod for `=` operator */
  {"__empty",      mt_empty},           /* metamethod for `empty` operator */
  {"__filled",     mt_filled},          /* metamethod for `filled` operator */
  {"__tostring",   mt_bitfieldtostring},/* for output at the console, e.g. print(n) */
  {NULL, NULL}
};


static const luaL_Reg memlib[] = {
  {"append",       memfile_append},
  {"attrib",       memfile_attrib},
  {"bitfield",     memfile_bitfield},
  {"bytebuf",      memfile_bytebuf},
  {"charbuf",      memfile_charbuf},
  {"clearbit",     memfile_clearbit},
  {"dump",         memfile_dump},
  {"find",         memfile_find},
  {"get",          memfile_get},
  {"getbit",       memfile_getbit},
  {"getbyte",      memfile_getbyte},
  {"getbytes",     memfile_getbytes},
  {"getchar",      memfile_getchar},
  {"getfield",     memfile_getfield},
  {"getitem",      memfile_getitem},
  {"getsize",      mt_getsize},
  {"iterate",      memfile_iterate},
  {"map",          memfile_map},
  {"match",        memfile_match},
  {"mfind",        memfile_mfind},
  {"move",         memfile_move},
  {"prepend",      memfile_prepend},
  {"purge",        memfile_purge},
  {"put",          memfile_put},
  {"read",         memfile_read},
  {"replace",      memfile_replace},
  {"replicate",    memfile_replicate},
  {"resize",       memfile_resize},
  {"reverse",      memfile_reverse},
  {"rewind",       memfile_rewind},
  {"setbit",       memfile_setbit},
  {"setbyte",      memfile_setbyte},
  {"setbytes",     memfile_setbytes},
  {"setchar",      memfile_setchar},
  {"setfield",     memfile_setfield},
  {"setitem",      memfile_setitem},
  {"shift",        memfile_shift},
  {"substring",    memfile_substring},
  {"tostring",     memfile_tostring},
  {"write",        memfile_write},
  {NULL, NULL}
};

/*
** Open memfile library
*/
LUALIB_API int luaopen_memfile (lua_State *L) {
  /* metamethods */
  luaL_newmetatable(L, "memfile");
  luaL_register(L, NULL, memfile_memlib);
  luaL_newmetatable(L, "bitfield");
  luaL_register(L, NULL, bitfield_memlib);
  /* register library */
  luaL_register(L, AGENA_MEMFILELIBNAME, memlib);
  return 1;
}

