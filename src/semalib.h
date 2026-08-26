#ifndef semalib_h
#define semalib_h

#include <math.h>
#include <stdint.h>  /* for UINT32_MAX */
#include "prepdefs.h"

#define MAXSEMAINT        INT_MAX  /* 4.4.3a change */

typedef struct Sema {
  uint32_t *s;       /* semaphore */
  int64_t n;         /* total number of uint32_t chunks */
  int64_t ff;        /* first chunk with a zero bit */
  int64_t  c;        /* current allocated id, estimate only for sema.close may `corrupt` the value. */
  char     scon;     /* is simple counter currently in use ? 1 = yes, 0 = no */
  int64_t  sc;       /* the simple counter, largest value possible: 9223372036854775807 = 2^64 - 1 = INT64_MAX */
  int64_t  noopen;   /* current number of open semaphore ids */
  int64_t  maxopen;  /* maximum number of open ids per semaphore */
} Sema;

#define SLOTS				      8
#define SIZESLOT          (sizeof(uint32_t))                        /* usually 4 */
#define SIZECHUNK		      (SLOTS*SIZESLOT)                          /* usually 32 = 4 * 8 at package initialisation */
#define SETBIT(x,pos,bit)	((x) & ~(1 << (pos))) | ((bit) << (pos))  /* pos counting from 0 */
#define GETBIT(x,pos)		  ((x & (1 << (pos))) != 0)                 /* pos counting from 0 */

/* See also: https://stackoverflow.com/questions/31393100/how-to-get-position-of-right-most-set-bit-in-c */
#define FIRSTZEROBIT(x)		(tools_uintlog2(~(x) & ((x) + 1)))        /* rightmost unset bit, return counting from 0 */
#define FIRSTONEBIT(x)		(tools_uintlog2((x) & ((~x) + 1)))        /* rightmost set bit, return counting from 0 */
#define LASTONEBIT(x)		  (tools_msb32((x)) - 1)                      /* leftmost set bit, return counting from 0 */

Sema *semalib_new (int64_t maxopen);
int  semalib_init    (Sema *s, int64_t maxopen);
void semalib_destroy (Sema *s);
int  semalib_expand  (Sema *s, int64_t nslots);
int  semalib_open    (Sema *s, int64_t semano);
int  semalib_close   (Sema *s, int64_t semano);
int  semalib_isopen  (Sema *s, int64_t semano);
int  semalib_reset   (Sema *s);
int  semalib_shrink  (Sema *s);

#endif