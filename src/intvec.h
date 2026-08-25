/* Dynamic Vector Array for type `int`, initiated September 27, 2017

   Based on: http://eddmann.com/posts/implementing-a-dynamic-vector-array-in-c, modified

   See the description of the functions exposed here in file intvec.c */

#ifndef intvec_h
#define intvec_h

#include "prepdefs.h"  /* for INLINE */

#define INTVEC_INICAPA 16
#define INTVEC_INTSIZE (sizeof(int))

#define intvec_init(vec,initslots)  intvec vec; _intvec_init(&vec, initslots)
#define intvec_free(vec) 			      _intvec_free(&vec)
#define intvec_append(vec,item) 	  _intvec_append(&vec, (item))
#define intvec_delete(vec,id)       _intvec_delete(&vex, (id))
#define intvec_set(vec,id,item) 	  _intvec_set(&vec, (id), (item))
#define intvec_get(vec,id)          _intvec_get(&vec, (id))
#define intvec_getdouble(vec,id)	  (double)_intvec_get(&vec, (id))
#define intvec_size(vec) 			      _intvec_size(&vec)
#define intvec_initfail(vec)        (!(&vec)->data)

typedef struct intvec {
  int *data;         /* container */
  int origcapacity;  /* original maximum number of slots. 3.10.2 */
  int capacity;      /* maximum number of slots */
  int size;          /* current number of entries */
} intvec;

INLINE int   _intvec_init (intvec *v, int initslots);
INLINE int   _intvec_get (intvec *v, int index);
INLINE int   _intvec_set (intvec *v, int index, int item);
INLINE int   _intvec_append (intvec *v, int item);
INLINE int   _intvec_insert (intvec *v, int index, int item);
INLINE int   _intvec_delete (intvec *v, int index);
INLINE int   _intvec_size (intvec *);
INLINE void  _intvec_free (intvec *);
INLINE int   intvec_newsize (int n);

#endif

