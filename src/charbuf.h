/* Character Buffer;

   Call charbuf_finish if you want to use the buffered string but do not know its size, e.g. when using lua_pushstring, after the last
   append, to include a final and terminating '\0'. */

#ifndef charbuf_h
#define charbuf_h

#include <stdlib.h>

typedef struct Charbuf {
  char *data;               /* container */
  size_t capacity;          /* maximum number of slots */
  size_t originalcapacity;  /* capacity when creating the buffer, in bytes, used in `memfile` package */
  size_t size;              /* current number of entries (characters */
  lu_byte type;             /* 0 for character buffer (MEMFILE_CHARBUF), 1 for byte buffer (MEMFILE_BYTEBUF */
  int init;                 /* the initialiser, 7.5.17 */
  char reserved[3];         /* explicit 3 bytes padding, 7.3.3 */
} Charbuf;

#define MEMFILE_BYTEBUF 1
#define MEMFILE_CHARBUF 0

Charbuf *bitfield_new (lua_State *L, size_t nbits, int init);
void charbuf_init (lua_State *L, Charbuf *buf, int type, size_t initsize, int optsize);
char INLINE *charbuf_growsize (lua_State *L, Charbuf *buf, size_t n);
int         charbuf_extendby (lua_State *L, Charbuf *buf, size_t n);
int         charbuf_resize (lua_State *L, Charbuf *buf, size_t newsize, int fillzeros);
int         charbuf_append (lua_State *L, Charbuf *buf, const char *str, size_t n);
int  INLINE charbuf_appendchar (lua_State *L, Charbuf *buf, char chr);
#define charbuf_prepend(L,buf,str,n) (charbuf_insert(L,buf,0,str,n))
int         charbuf_insert (lua_State *L, Charbuf *buf, size_t pos, const char *str, size_t n);  /* pos >= 0 */
int         charbuf_remove (lua_State *L, Charbuf *buf, size_t pos, size_t n);  /* pos >= 0 */
int         charbuf_removelast (lua_State *L, Charbuf *buf, size_t n);
void        charbuf_reverse (lua_State *L, Charbuf *buf, size_t n);

void bitfield_init (lua_State *L, Charbuf *buf, size_t nbits, int init);
void bitfield_setbit (lua_State *L, Charbuf *buf, ptrdiff_t pos, int value);
void bitfield_set (lua_State *L, Charbuf *buf, ptrdiff_t pos);
void bitfield_clear (lua_State *L, Charbuf *buf, ptrdiff_t pos);
int  bitfield_flip (lua_State *L, Charbuf *buf, ptrdiff_t pos);
int  bitfield_get (lua_State *L, Charbuf *buf, ptrdiff_t pos);
void bitfield_resize (lua_State *L, Charbuf *buf, size_t nbits, int value);
size_t bitfield_size (lua_State *L, Charbuf *buf);
void bitfield_reset (lua_State *L, Charbuf *buf);

#define charbuf_get(buf) ((buf)->data)
#define charbuf_size(buf) ((buf)->size)

/* converted to macro in 2.16.13 */
#define charbuf_free(buf) { \
  free((buf)->data); \
  (buf)->data = NULL; \
}

#define bitfield_free(buf) { \
  if (buf) { \
    charbuf_free(buf); \
    xfree(buf); \
  } \
}

/* 2.16.13; patched 2.39.12 */
#define charbuf_finish(buf) { \
  if ((buf)->size >= (buf)->capacity) { \
    charbuf_appendchar(L, (buf), '\0'); \
    (buf)->size--; \
  } \
}


#define charbuf_addsize(buf,n)   ((buf)->size += (n))
#define charbuf_subsize(buf,n)   ((buf)->size -= (n))

#define IQR(pos,place) { \
  size_t rem = (pos) & (CHAR_BIT - 1); \
  int isrem = (rem != 0); \
  pos = pos/CHAR_BIT + isrem; \
  place = rem + (!isrem)*CHAR_BIT; \
}

#if CHAR_BIT == 8
#define ILOG2_8 3
#elif CHAR_BIT == 16
#define ILOG2_8 4
#elif CHAR_BIT == 32
#define ILOG2_8 5
#endif

#ifndef ILOG2_8
#error wrong bitness of characters
#endif

/* works only if the denominator is a power of two; neither slower nor faster than IQR */
#define IQRP2(pos,place) { \
  pos--; \
  place = (pos) & (CHAR_BIT - 1); \
  pos = (pos) >> ILOG2_8; \
}

#endif

