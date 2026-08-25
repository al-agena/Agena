/* Dynamic Vector Array for type `double`, initiated January 31, 2024

   Based on: http://eddmann.com/posts/implementing-a-dynamic-vector-array-in-c, modified

   See the description of the functions exposed here in file dblvec.c */

#ifndef dblvec_h
#define dblvec_h

#include "prepdefs.h"  /* for INLINE */

#define DBLVEC_INICAPA 16
#define DBLVEC_DBLSIZE (sizeof(double))

#define dblvec_init(vec,initslots)  _dblvec_init(&vec, initslots)
#define dblvec_free(vec) 			      _dblvec_free(&vec)
#define dblvec_append(vec,item) 	  _dblvec_append(&vec, (item))
#define dblvec_delete(vec,id)       _dblvec_delete(&vex, (id))
#define dblvec_set(vec,id,item) 	  _dblvec_set(&vec, (id), (item))
#define dblvec_get(vec,id)	        _dblvec_get(&vec, (id))
#define dblvec_size(vec) 			      _dblvec_size(&vec)
#define dblvec_initfail(vec)        (!(&vec)->data)

typedef struct dblvec {
  double *data;      /* container */
  int capacity;      /* maximum number of slots */
  int origcapacity;  /* original maximum number of slots, 3.10.2 */
  int size;          /* current number of entries */
  int padding;       /* 4 bytes - Explicit Padding */
} dblvec;

INLINE int    _dblvec_init (dblvec *v, int initslots);
INLINE double _dblvec_get (dblvec *v, int index);
INLINE int    _dblvec_set (dblvec *v, int index, double item);
INLINE int    _dblvec_append (dblvec *v, double item);
INLINE int    _dblvec_insert (dblvec *v, int index, double item);
INLINE int    _dblvec_delete (dblvec *v, int index);
INLINE int    _dblvec_size (dblvec *);
INLINE void   _dblvec_free (dblvec *);
INLINE int    dblvec_newsize (int n);

#endif

