/* Source taken from: https://www.happybearsoftware.com/implementing-a-dynamic-array, modified */

#include <stdio.h>
#include <stdlib.h>

#define vecoff64_c
#define LUA_LIB

#include "vecoff64.h"

int vector_init (Vector *vector) {
  /* initialize size and capacity */
  vector->size = 0;
  vector->capacity = VECTOR_INITIAL_CAPACITY;
  /* allocate memory for vector->data */
  vector->data = malloc(vector_sizeof*vector->capacity);
  return vector->data == NULL;
}

/* dynamically doubles the underlying data array capacity */
static int vector_enlarge (Vector *vector) {
  /* double vector->capacity and resize the allocated memory accordingly */
  void *temp = realloc(vector->data, 2*vector_sizeof*vector->capacity);
  if (!temp) return 1;   /* 7.7.7 K&R fix */
  vector->capacity *= 2;
  vector->data = temp;
  return 0;
}

/* appends the given value to the vector */
int vector_add (Vector *vector, indtype value) {
  /* make sure there's room to expand into */
  if (vector->size >= vector->capacity) {
    return vector_enlarge(vector);  /* 1 at failure */
  }
  /* append the value and increment vector->size */
  vector->data[vector->size++] = value;
  return 0;
}


/* returns a value out of a vector at the given _eligible_ index. */
mytype vector_getoff64_t (Vector *vector, indtype index) {
  /* if (index >= vector->size || index < 0) {
    fprintf(stderr, "vector_get: index %d out of bounds for vector of size %d\n", index, vector->size);
    exit(1);
  } */
  return vector->data[index];
}


/* sets the value at the given eligible index to the given value */
void vector_set (Vector *vector, indtype index, indtype value) {
  /* zero fill the vector up to the desired index */
  while (index >= vector->size)
    vector_add(vector, 0);
  /* set the value at the desired index */
  vector->data[index] = value;
}


void vector_free (Vector *vector) {
  free(vector->data);
  vector->data = NULL;
}

