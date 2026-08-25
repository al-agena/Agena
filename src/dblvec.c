/* Dynamic Vector Array for type `double`, initiated January 31, 2024

   Based on: http://eddmann.com/posts/implementing-a-dynamic-vector-array-in-c, modified

   (You CANNOT pass DOUBLES or other simple 8-byte data types via (void *) arguments, since pointers are usually 4 bytes wide.
   Put them into unions or arrays, alternatively. Casting doubles to pointers like "double *p = (double *)num" and back
   to "double num = *p;" is not portable.)

   See also: https://stackoverflow.com/questions/6575340/converting-double-to-void-in-c
             https://stackoverflow.com/questions/12700384/convert-void-to-double */

#include <stdlib.h>

#define dblvec_c
#define LUA_LIB

#include "agncmpt.h"   /* for AGN_NAN */
#include "dblvec.h"
#include "prepdefs.h"  /* FORCE_INLINE */

/* initialises a vector */
int _dblvec_init (dblvec *v, int initslots) {
  v->capacity = (initslots < 1) ? DBLVEC_INICAPA : initslots;
  v->origcapacity = v->capacity;
  v->size = 0;
  v->data = malloc(DBLVEC_DBLSIZE * v->capacity);
  return v->data ? 0 : 1;  /* 0 = success, 1 = failure */
}


/* returns the number of allocated vector elements */
int _dblvec_size (dblvec *v) {
  return v->size;
}


/* resizes a vector to the given new size */
static FORCE_INLINE int _dblvec_resize (dblvec *v, size_t capacity) {
  double *data = realloc(v->data, DBLVEC_DBLSIZE * capacity);
  if (data) {
    v->data = data;
    v->capacity = capacity;
  }
  return data ? 0 : 1;  /* 0 = success, 1 = failure */
}


/* appends an element to a vector. 3.9.3a fix */
int _dblvec_append (dblvec *v, double item) {
  if (v->capacity == v->size) {
    if (_dblvec_resize(v, agn_newsize(NULL, v->capacity))) return 1;
  }
  v->data[v->size++] = item;
  return 0;  /* 0 = success, 1 = failure */
}


/* sets an item to position index (counting from 0) of a vector, overwriting the existing element */
int _dblvec_set (dblvec *v, int index, double item) {  /* 3.9.3a fix, 3.10.2 extension */
  if (index == v->size) {
    return _dblvec_append(v, item);
  } else if (index >= 0 && index < v->size) {
    v->data[index] = item;  /* 4.10.3 fix */
    return 0;  /* success */
  }
  return 1;  /* failure */
}


/* retrieves the element at position index (counting from 0).
   Call like this: (off64_t)_dblvec_get(&v, index) */
double _dblvec_get (dblvec *v, int index) {
  if (index >= 0 && index < v->size)
    return v->data[index];
  return AGN_NAN;
}


/* delete the element at index position from a vector and move the other elements to close the space;
   2.15.4 */
int _dblvec_delete (dblvec *v, int index) {
  int i;
  if (index < 0 || index >= v->size) return 2;  /* out of range */
  v->data[index] = AGN_NAN;
  for (i=index; i < v->size - 1; i++) {
    v->data[i] = v->data[i + 1];
  }
  v->size--;
  if (v->size > 0) {  /* changed 2.15.4 */
    size_t p = (v->size <= v->origcapacity) ? v->origcapacity : agn_newsize(NULL, v->size);  /* 3.10.2 change */
    return _dblvec_resize(v, p);
  }
  return 0;  /* 0 = success, 1 = failure */
}


/* inserts an item at the index position (counting from 0) and moves the element to be replaced and
   all the other elements to open space; can also insert to the tail. 2.15.4 */
int _dblvec_insert (dblvec *v, int index, double item) {
  int i;
  if (index < 0 || index > v->size) return 2;  /* out of range */
  if (index == v->size) return _dblvec_append(v, item);  /* append */
  if (v->capacity == v->size) {
    if (_dblvec_resize(v, agn_newsize(NULL, v->capacity))) return 1;
  }
  for (i=v->size; i > index; i--) {
    v->data[i] = v->data[i - 1];
  }
  v->data[index] = item;
  v->size++;
  return 0;  /* 0 = success, 1 = failure */
}


/* frees a vector */
void _dblvec_free (dblvec *v) {
  free(v->data);
  v->data = NULL;
}


/* fetch the next capacity size */
int dblvec_newsize (int n) {
  return agn_newsize(NULL, n);
}

