/* Abstract semaphore library, created 6th of March 2019 */

#include <stdlib.h>
#include <sys/types.h>
#include <math.h>    /* OS/2 needs this */

#define semalib_c
#define LUA_LIB

#include "agnhlps.h"
#include "semalib.h"

/* In Agena, you must initialise the userdata with lua_newuserdata and then call this function to set the defaults and
   create the internal dynamic array s->s. */
int semalib_init (Sema *s, int64_t maxopen) {
  int64_t i;
  s->ff = 0;
  s->n = SLOTS;
  s->s = malloc(s->n * SIZESLOT);  /* = 8 uint32_t's */
  if (!s->s) return 1;  /* semaphore initialisation error */
  s->c = 0;
  for (i=0; i < s->n; i++) s->s[i] = 0;
  s->scon = 1;
  s->sc = -1;
  s->noopen = 0;
  s->maxopen = maxopen;
  return 0;
}


Sema *semalib_new (int64_t maxopen) {
  Sema *s = (Sema *)malloc(sizeof(Sema));
  if (s == NULL || semalib_init(s, maxopen)) return NULL;
  return s;
}


void semalib_destroy (Sema *s) {
  xfree(s->s);
  xfree(s);
}


int semalib_expand (Sema *s, int64_t nslots) {  /* expand s->s by the given number of uint32_t's */
  if (s->c == MAXSEMAINT) return 1;  /* too many open semaphores; 2^53 = 9,007,199,254,740,992 */
  nslots = sun_ceil((double)nslots/(double)SLOTS)*SLOTS;
  if (nslots > s->n) {  /* enlarge */
    int64_t i, oldsn;
    s->s = realloc(s->s, nslots * SIZESLOT);
    if (!s->s) return -1;  /* memory allocation error */
    oldsn = s->n;
    s->n = nslots;
    for (i=oldsn; i < s->n; i++) s->s[i] = 0;
    return 1;  /* enlarged */
  }
  return 0;  /* no enlargement necessary */
}

int semalib_enlargearray (Sema *s, int64_t semano, int64_t *chunk, int64_t *pos) {
  *chunk = semano / SIZECHUNK;
  *pos = semano % SIZECHUNK;
  if (*chunk >= s->n) {  /* enlarge */
    int64_t i, oldsn;
    s->s = realloc(s->s, s->n * SIZESLOT + (*chunk - s->n + 1)*SIZECHUNK);
    if (!s->s) return -1;  /* memory allocation error */
    oldsn = s->n;
    s->n += SLOTS*(*chunk - s->n + 1);
    for (i=oldsn; i < s->n; i++) s->s[i] = 0;
  }
  return 0;
}


#define onemask(n)  (tools_intpow(2, n) - 1)
#define onemask32   (tools_intpow(2, SIZECHUNK) - 1);  /* = 4294967295 */

int semalib_open (Sema *s, int64_t semano) {
  int64_t chunk, pos;
  if (s->noopen == s->maxopen) return 2;
  if (s->scon) {  /* simple counter active ? */
    if (s->sc == MAXSEMAINT) return 1;  /* 4.4.2 changes */
    if (semano == s->sc + 1) {
      s->sc++;
      s->noopen++;
      return 0;
    } else if (semano < s->sc + 1)
      return 0;  /* do nothing */
    if (semalib_enlargearray(s, s->sc, &chunk, &pos) == 0) {
      /* switch from memory-effective simple counter mode to dynamic memory management to administer `holes`, e.g.
         have semaphores #0, #1, #3, #4 opened, but not #2. Enlarge array and set bits for all semaphores to be
         left opened */
      int64_t i, mask32;
      mask32 = onemask32;
      for (i=0; i < chunk; i++) {
        s->s[i] = mask32;
      }
      s->s[chunk] = onemask(pos + 1);
      s->scon = 0;
    } else
      return -1;
  }
  if (s->c == MAXSEMAINT) return 1;  /* too many open semaphores; 2^53 = 9,007,199,254,740,992 */
  if (semalib_enlargearray(s, semano, &chunk, &pos) == 0) {
    if (GETBIT(s->s[chunk], pos)) return 2;  /* semaphore already open */
    s->s[chunk] = SETBIT(s->s[chunk], pos, 1);
    s->noopen++;
    return 0;
  }
  return -1;  /* failure */
}


int semalib_close (Sema *s, int64_t semano) {
  int64_t chunk, pos;
  if (s->scon) {
    if (semano == s->sc) {
      if (s->sc >= 0) {
        s->sc--; s->noopen--;
      }
      return 0;
    } else if (semano > s->sc) {
      return 0;  /* do nothing */
    } else {
      if (semalib_enlargearray(s, s->sc, &chunk, &pos) == 0) {  /* enlarge array and set bits for all semaphores to be left opened */
        /* switch from memory-effective simple counter mode to dynamic memory management to administer `holes`, e.g.
           have semaphores #0, #1, #3, #4 opened, but not #2 since that shall be closed now. Enlarge array and set bits for all
           semaphores to be left opened */
        int64_t i, mask32;
        mask32 = onemask32;
        for (i=0; i < chunk; i++) {
          s->s[i] = mask32;
        }
        s->s[chunk] = onemask(pos + 1);
        s->scon = 0;
        if (chunk < s->ff) s->ff = chunk;
      } else
        return -1;
    }
  }
  chunk = semano / SIZECHUNK;
  pos = semano % SIZECHUNK;
  if (chunk > s->n) return 1;  /* invalid semaphore */
  s->s[chunk] = SETBIT(s->s[chunk], pos, 0);
  if (chunk < s->ff) s->ff = chunk;
  s->c = s->ff * SIZECHUNK;  /* just a default */
  s->noopen--;
  return 0;
}


int semalib_isopen (Sema *s, int64_t semano) {  /* checks whether semaphore #semano is currently open */
  int64_t chunk, pos;
  if (s->scon)
    return (semano <= s->sc);
  chunk = semano / SIZECHUNK;
  pos = semano % SIZECHUNK;
  if (chunk > s->n) return -1;
  return GETBIT(s->s[chunk], pos);
}


int semalib_shrink (Sema *s) {  /* shrink s->s to the least number of chunks actually needed */
  int64_t i, chunk, r;
  r = 0;
  i = s->n - 1;
  while (i >= s->ff) {
    if (tools_onebits32(s->s[i]) != 0 || i == 0) break;
    i--;
  }
  chunk = i/SLOTS;  /* number of largest chunk containing ones, counting from 0 */
  chunk++;          /* now counting from 1 */
  if (s->n/SLOTS > chunk) {  /* shrink only when useful */
    s->s = realloc(s->s, chunk * SIZECHUNK);
    if (s->s == NULL) return -1;
    s->n = chunk * SLOTS;
    /* do not change s.c */
    r = 1;
  }
  return r;
}


int semalib_reset (Sema *s) {  /* force s->s to be reduced to the minimum number of slots, all contents being lost */
  int64_t i;
  for (i=0; i < s->n; i++) s->s[i] = 0;
  s->s = realloc(s->s, SIZECHUNK);  /* = 8 uint32_t's */
  if (!s->s) return -1;  /* semaphore re-initialisation error. */
  s->ff = 0;
  s->n = SLOTS;
  s->c = 0;
  s->noopen = 0;
  for (i=0; i < s->n; i++) s->s[i] = 0;
  return 0;
}

