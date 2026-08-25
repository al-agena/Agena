/* Character Buffer, initiated January 04, 2020, Alexander Walz. MIT licence, see `src/agena.h`.

   Call `charbuf_finish` if you want to use the buffered string and do not know its size, e.g. when using lua_pushstring, after the last
   append to include a terminating '\0'.

   Note that packages, luaL_Buffer & Co are faster than the functions defined here since luaL_Buffer uses the stack and no mallocs/reallocs. */

#include <stdlib.h>
#include <string.h>

#define charbuf_c
#define LUA_LIB

#include "agena.h"

#include "agnhlps.h"
#include "agnxlib.h"
#include "charbuf.h"
#include "ldebug.h"

#ifdef PLUS
#define myerror luaL_error
#else
#define myerror luaG_runerror
#endif

void INLINE charbuf_init (lua_State *L, Charbuf *buf, int type, size_t initsize, int getoptsize);
int         charbuf_append (lua_State *L, Charbuf *buf, const char *str, size_t n);
int  INLINE charbuf_appendchar (lua_State *L, Charbuf *buf, char chr);
void INLINE bitfield_checkpos (lua_State *L, Charbuf *buf, lua_Number pos);

/* Refactored, 2.16.12, generalised; pass 0 to type for a character buffer, and 1 for a byte buffer. 2.26.3 */
void INLINE charbuf_init (lua_State *L, Charbuf *buf, int type, size_t initsize, int getoptsize) {  /* refactored, 2.16.12 */
  size_t optlen, chunks;
  int reset = (initsize == 0);
  if (reset) initsize = 1;
  optlen = getoptsize ? tools_optstrlen(initsize, &chunks) : initsize;  /* 2.26.2 change */
  buf->data = (char *)malloc(optlen*CHARSIZE);
  if (buf->data == NULL) {
    myerror(L, "Error: allocation error when initialising buffer.");
  }
  /* we cannot zero bytes at the end of a string, as memset cannot do this and would crash the interpreter */
  tools_bzero(buf->data, optlen);
  buf->size = (type == MEMFILE_BYTEBUF) ? (reset ? 0 : optlen) : 0;
  buf->capacity = optlen;
  buf->originalcapacity = reset ? 0 : optlen;
  buf->type = type;
  buf->init = 0;
}


/* Grow buffer by at least n bytes; n must be > 0 ! The function automatically aligns the enlarged buffer along word boundaries. */
char INLINE *charbuf_growsize (lua_State *L, Charbuf *buf, size_t n) {  /* 2.20.2 */
  char *temp;
  if (buf->size + n > buf->capacity) {
    size_t chunks;
    buf->capacity = tools_optstrlen(mul2(buf->capacity) + n, &chunks);  /* Agena 1.0.6 change to speed up computation; 2.17.8 tweak */
    temp = (char *)realloc(buf->data, buf->capacity*CHARSIZE);
    if (temp == NULL) {  /* patched 2.26.3, do not free buf->data */
      myerror(L, "Error: re-allocation error when growing buffer.");
      return NULL;
    } else
      buf->data = temp;
    (void)chunks;
  }
  return buf->data + buf->size;  /* address of next free byte to be filled, can point to '\0' */
}


int charbuf_extendby (lua_State *L, Charbuf *buf, size_t n) {  /* 2.38.0 */
  char *temp;
  if (buf->size + n > buf->capacity) {
    size_t chunks;
    buf->capacity = tools_optstrlen(mul2(buf->capacity) + n, &chunks);  /* Agena 1.0.6 change to speed up computation; 2.17.8 tweak */
    temp = (char *)realloc(buf->data, buf->capacity*CHARSIZE);
    if (temp == NULL) {  /* patched 2.26.3, do not free buf->data */
      myerror(L, "Error: re-allocation error when growing buffer.");
      return 0;
    } else
      buf->data = temp;
    (void)chunks;
  }
  return 1;  /* success or no extension needed so far */
}


/* Resize buffer to exactly n bytes and optionally fill extended space with zeros, preserving the original contents; newsize must be > 0 !
   You may apply tools_optstrlen on n before calling the function to get word-alignment. 2.26.2 */
int charbuf_resize (lua_State *L, Charbuf *buf, size_t newsize, int fillzeros) {
  char *temp;
  if (newsize == buf->capacity) {
    if (fillzeros) {  /* fill new space with zeros completely, 2.26.3 change */
      size_t nzeros = newsize - buf->size;
      tools_bzero(buf->data + buf->size, nzeros);
      charbuf_addsize(buf, nzeros);
    }
    return 0;  /* old = new size, do nothing */
  }
  temp = (char *)realloc(buf->data, newsize*CHARSIZE);
  if (temp == NULL) {
    return -1;
  }
  if (newsize > buf->capacity) {  /* grow ? */
    if (fillzeros) {  /* fill new space with zeros completely, 2.26.3 change */
      size_t nzeros = newsize - buf->size;
      tools_bzero(temp + buf->size, nzeros);  /* 2.26.3 fix */
      charbuf_addsize(buf, nzeros);
    }
    /* do not change buf->size since no bytes have been added */
  } else {  /* shrink */
    buf->size = newsize;
  }
  buf->capacity = newsize;  /* reset total available space */
  buf->data = temp;  /* assign resized array to buffer */
  return 0;
}


int charbuf_append (lua_State *L, Charbuf *buf, const char *str, size_t n) {  /* refactored, 2.16.12, rewritten 2.16.13, changed 2.20.2 */
  if (n == 0) return 1;
  if (!charbuf_extendby(L, buf, n)) return 0;  /* changed 2.38.0 */
  /* special treatment of one-char strings is not necessary as memcpy is faster than a single char assignment. */
  tools_memcpy(buf->data + buf->size, str, n);  /* much faster than strncpy, as fast as `for (i=0; i < n; i++) *buffer++ = *string++` */ /* 2.25.1 tweak */
  charbuf_addsize(buf, n);
  return 1;
}


int INLINE charbuf_appendchar (lua_State *L, Charbuf *buf, char chr) {  /* added 2.16.13, changed 2.20.2 */
  if (!charbuf_extendby(L, buf, 1)) return 0;  /* changed 2.38.0 */
  buf->data[buf->size] = chr;
  charbuf_addsize(buf, 1);
  return 1;
}


/* pos starting from 0 ! */
int charbuf_insert (lua_State *L, Charbuf *buf, size_t pos, const char *str, size_t n) {  /* 2.37.3 */
  size_t i, size;  /* 2.37.4/5 fix */
  size = buf->size;
  if (n == 0) return 1;  /* changed 5.5.1 */
  if (!charbuf_extendby(L, buf, n)) return 0;  /* changed 2.38.0 */
  if (size > 0) {  /* 2.38.0 extension for zero-sized buffers, i.e. skip shifting elements as there are none to be shifted */
    for (i=size; i > pos; i--) buf->data[i + n - 1] = buf->data[i - 1];  /* do not shortcut char *b = buf->data !!! */
  }
  tools_memcpy(buf->data + pos, str, n);
  charbuf_addsize(buf, n);
  return 1;
}


/* pos starting from 0 ! */
int charbuf_remove (lua_State *L, Charbuf *buf, size_t pos, size_t n) {  /* 2.37.3 */
  size_t i;  /* 2.37.4/5 fix */
  if (n == 0 || pos >= buf->size) return 0;
  if (pos + n == buf->size) {  /* delete from pos to the end of the buffer ? 3.3.1 */
    for (i=pos; i < pos + n; i++)
      buf->data[i] = '\0';
  } else {
    for (i=pos; i < buf->size - n; i++)
      buf->data[i] = buf->data[i + n];  /* do not shortcut char *b = buf->data !!! */
  }
  charbuf_subsize(buf, n);
  return 1;
}


/* Remove n characters/bytes from the end of the buffer, 3.3.1 */
int charbuf_removelast (lua_State *L, Charbuf *buf, size_t n) {
  size_t i;
  if (n > buf->size) return 0;  /* if n = 0, just falls through */
  for (i=1; i <= n; i++)
    buf->data[buf->size - i] = '\0';
  charbuf_subsize(buf, n);
  return 1;
}


/* Reverses the first n characters, taken from:
   https://stackoverflow.com/questions/198199/how-do-you-reverse-a-string-in-place-in-c-or-c
   Solution by uvote. The XOR version suggested in the same page assertively is slow on modern CPUs.
   4.7.4 */
void charbuf_reverse (lua_State *L, Charbuf *buf, size_t n) {
  char c, *p, *q;
  if (n == 0) return;
  p = buf->data;
  q = &p[n - 1];
  while (q > p) {
    c = *p;
    *p++ = *q;
    *q-- = c;
  }
}


/* Low-level bit field functions */

void bitfield_init (lua_State *L, Charbuf *buf, size_t nbits, int init) {  /* 2.31.4 */
  size_t nbytes;
  nbytes = ((nbits & (CHAR_BIT - 1)) ? tools_nextmultiple(nbits, CHAR_BIT) : nbits)/CHAR_BIT;
  if (nbytes == 0) {
    myerror(L, "Error: zero-size bit fields are not supported.");
  }
  buf->data = (char *)malloc(nbytes);
  if (buf->data == NULL) {
    myerror(L, "Error: buffer allocation error.");
  }
  memset(buf->data, init, nbytes);  /* fill it completely with init */
  buf->size = nbytes;
  buf->capacity = nbytes;
  buf->originalcapacity = nbytes;
  buf->type = 1;
  buf->init = init;
}


Charbuf *bitfield_new (lua_State *L, size_t nbits, int init) {
  Charbuf *buf = (Charbuf *)malloc(sizeof(Charbuf));
  if (buf == NULL) {  /* 4.11.5 fix */
    myerror(L, "Error: buffer allocation error.");
  }
  bitfield_init(L, buf, nbits, init);
  return buf;
}  /* free this with bitfield_free */


void bitfield_reset (lua_State *L, Charbuf *buf) {  /* 7.5.17 */
  if (buf->data) {  /* memset works with non-NULL data only */
    memset(buf->data, buf->init, buf->capacity);  /* refill it completely with init */
  }
}


/* Parameter pos is 1-based ! */
void bitfield_setbit (lua_State *L, Charbuf *buf, ptrdiff_t pos, int value) {  /* 2.31.4 */
  size_t place;
  unsigned char byte;
  IQRP2(pos, place);  /* 2.31.5 change */
  if (place >= CHAR_BIT*CHARSIZE || pos >= buf->size)
    luaL_error(L, "Error: invalid position %td given.", (pos + 1)*CHAR_BIT);
  byte = uchar(buf->data[pos]);
  byte ^= uchar( ((-value) ^ (LUAI_UINT32)byte) & (1 << (LUAI_UINT32)place) );
  buf->data[pos] = byte;
}


/* Parameter pos is 1-based ! */
void bitfield_set (lua_State *L, Charbuf *buf, ptrdiff_t pos) {  /* 2.31.4 */
  size_t place;
  unsigned char byte;
  IQRP2(pos, place);  /* 2.31.5 change */
  if (place >= CHAR_BIT*CHARSIZE || pos >= buf->size)
    luaL_error(L, "Error: invalid position %td given.", (pos + 1)*CHAR_BIT);
  byte = uchar(buf->data[pos]);
  byte |= 1 << (LUAI_UINT32)place;
  buf->data[pos] = byte;
}


/* Parameter pos is 1-based ! */
void bitfield_clear (lua_State *L, Charbuf *buf, ptrdiff_t pos) {  /* 2.31.4 */
  size_t place;
  unsigned char byte;
  IQRP2(pos, place);  /* 2.31.5 change */
  if (place >= CHAR_BIT*CHARSIZE || pos >= buf->size)
    luaL_error(L, "Error: invalid position %td given.", (pos + 1)*CHAR_BIT);
  byte = uchar(buf->data[pos]);
  byte &= ~(1UL << (LUAI_UINT32)place);
  buf->data[pos] = byte;
}


/* Parameter pos is 1-based ! */
int bitfield_get (lua_State *L, Charbuf *buf, ptrdiff_t pos) {  /* 2.31.4 */
  size_t place;
  unsigned char byte;
  IQRP2(pos, place);  /* 2.31.5 change */
  if (place >= CHAR_BIT*CHARSIZE || pos >= buf->size)
    luaL_error(L, "Error: invalid position %td given.", (pos + 1)*CHAR_BIT);
  byte = uchar(buf->data[pos]);
  return ((LUAI_UINT32)byte >> (LUAI_UINT32)place) & 1UL;
}


/* Parameter pos is 1-based ! */
int bitfield_flip (lua_State *L, Charbuf *buf, ptrdiff_t pos) {  /* 2.31.4 */
  size_t place;
  unsigned char byte;
  IQRP2(pos, place);  /* 2.31.5 change */
  if (place >= CHAR_BIT*CHARSIZE || pos >= buf->size)
    luaL_error(L, "Error: invalid position %td given.", (pos + 1)*CHAR_BIT);
  byte = uchar(buf->data[pos]);
  byte ^= 1UL << (LUAI_UINT32)place;
  buf->data[pos] = byte;
  return ((LUAI_UINT32)(byte) >> (LUAI_UINT32)place) & 1UL;
}


size_t bitfield_size (lua_State *L, Charbuf *buf) {  /* 2.31.4, can't define this as a macro */
  return CHAR_BIT*buf->size;
}


void INLINE bitfield_checkpos (lua_State *L, Charbuf *buf, lua_Number pos) {  /* 2.31.4, can't define this as a macro */
  if (pos < 1 || pos > CHAR_BIT*buf->size)
    luaL_error(L, "Error: position %d is out-of-range.", (int)pos);
}


void bitfield_resize (lua_State *L, Charbuf *buf, size_t nbits, int value) {  /* 2.31.4 */
  char *temp;
  size_t nbytes;
  nbytes = ((nbits & (CHAR_BIT - 1)) ? tools_nextmultiple(nbits, CHAR_BIT) : nbits)/CHAR_BIT;
  if (nbytes == 0) return;  /* do nothing */
  temp = (char *)realloc(buf->data, nbytes*CHARSIZE);
  if (temp == NULL) {
    myerror(L, "Error: when resizing, memory allocation failed.");
  }
  if (nbytes > buf->capacity) {  /* grow ? */
    size_t nzeros = nbytes - buf->size;
    memset(temp + buf->size, value, nzeros);
    charbuf_addsize(buf, nzeros);
  } else {  /* shrink */
    buf->size = nbytes;
  }
  buf->capacity = nbytes;  /* reset total available space */
  buf->data = temp;  /* assign resized array to buffer */
}

