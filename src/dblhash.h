#ifndef dblhash_h
#define dblhash_h

#include <stddef.h>  /* for size_t */

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#include <stdbool.h>
#elif !defined(__cplusplus) && !defined(bool)
/* legacy C89 support */
typedef enum { false, true } bool;
#endif


typedef enum { DBLHASH_EMPTY, DBLHASH_OCCUPIED, DBLHASH_DELETED } SlotStatus;

typedef struct {
  double *keys;
  SlotStatus *status;
  size_t capacity;  /* can be a power of 2 or other */
  size_t count;
  /* optimised = 1: speed-optimised mode, that is the capacity is always a power of two: this makes
     the package six times faster than when in memory-saving mode */
  bool optimised;
  char padding[7];  /* Explicit Padding */
} DoubleHashSet;

DoubleHashSet *dhs_create (size_t initial_capacity, int optimised);
int           dhs_insert (DoubleHashSet *set, double val);
bool          dhs_contains (DoubleHashSet *set, double val);
static int    dhs_shrink (DoubleHashSet *set);
bool          dhs_remove (DoubleHashSet *set, double val, int shrink);
void          dhs_free (DoubleHashSet *set);
size_t        dhs_getidx (DoubleHashSet *set, double val, int *rc);

#endif
