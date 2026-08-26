/* Source taken from: https://www.happybearsoftware.com/implementing-a-dynamic-array, modified */

#ifndef vecoff64_h
#define vecoff64_h

#include "agnhlps.h"  /* Linux needs off64_t definition */

#define VECTOR_INITIAL_CAPACITY 64

#define mytype off64_t
#define indtype ssize_t  /* signed [sic !] size_t due to index check < 0 */

/* Define a vector type */
typedef struct {
  indtype size;      /* slots used so far */
  indtype capacity;  /* total available slots */
  mytype *data;      /* array of integers we're storing */
} Vector;

#define vector_sizeof sizeof(mytype)

int  vector_init (Vector *vector);
int  vector_add (Vector *vector, indtype value);
mytype vector_getoff64_t (Vector *vector, indtype index);
void vector_set (Vector *vector, indtype index, indtype value);
void vector_free (Vector *vector);

#endif

