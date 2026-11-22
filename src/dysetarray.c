/*
** $Id: dysetarray.c, initiated November 20, 2026 $
** Dynamic Set Array Library
** Licence: Public domain.
** Created mostly by Gemini AI.
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dysetarray.h"

#define MIN_CAPACITY 4

DynamicSetArray *dsa_create (size_t initial_capacity) {
  if (initial_capacity < MIN_CAPACITY) {
    initial_capacity = MIN_CAPACITY;
  }
  DynamicSetArray *dsa = malloc(sizeof(DynamicSetArray));
  if (!dsa) return NULL;
  dsa->buckets = malloc(initial_capacity * sizeof(Bucket));
  if (!dsa->buckets) {
    free(dsa);
    return NULL;
  }
  dsa->count = 0;
  dsa->capacity = initial_capacity;
  return dsa;
}

static bool find_bucket_pos (const DynamicSetArray *dsa, size_t index, size_t *pos) {
  size_t left = 0;
  size_t right = dsa->count;
  while (left < right) {
    size_t mid = left + (right - left) / 2;
    if (dsa->buckets[mid].index == index) {
      *pos = mid;
      return true;
    } else if (dsa->buckets[mid].index < index) {
      left = mid + 1;
    } else {
      right = mid;
    }
  }
  *pos = left;
  return false;
}

static bool dsa_grow(DynamicSetArray *dsa) {
  size_t new_capacity = dsa->capacity * 2;
  Bucket *new_buckets = realloc(dsa->buckets, new_capacity * sizeof(Bucket));
  if (!new_buckets) return false;
  dsa->buckets = new_buckets;
  dsa->capacity = new_capacity;
  return true;
}

static void dsa_shrink (DynamicSetArray *dsa) {
  /* Only shrink when usage drops to 25% or less, and never below MIN_CAPACITY */
  if (dsa->capacity <= MIN_CAPACITY || dsa->count > dsa->capacity / 4) {
    return;
  }
  size_t new_capacity = dsa->capacity / 2;
  if (new_capacity < MIN_CAPACITY) {
    new_capacity = MIN_CAPACITY;
  }
  /* If realloc fails during shrinking, keep the existing valid buffer safely */
  Bucket *new_buckets = realloc(dsa->buckets, new_capacity * sizeof(Bucket));
  if (new_buckets) {
    dsa->buckets = new_buckets;
    dsa->capacity = new_capacity;
  }
}

bool dsa_insert (DynamicSetArray *dsa, size_t index, int key, int value, int whengreater) {
  if (!dsa) return false;
  size_t pos = 0;
  if (!find_bucket_pos(dsa, index, &pos)) {
    if (dsa->count == dsa->capacity) {
      if (!dsa_grow(dsa)) return false;
    }
    if (pos < dsa->count) {
      memmove(&dsa->buckets[pos + 1],
          &dsa->buckets[pos],
          (dsa->count - pos) * sizeof(Bucket));
    }
    dsa->buckets[pos].index = index;
    dsa->buckets[pos].head = NULL;
    dsa->count++;
  }
  Node **curr = &dsa->buckets[pos].head;
  while (*curr != NULL) {
    if ((*curr)->key == key) {
      if (whengreater) {  /* only update value field if new value is greater than current value */
        if (value > (*curr)->value) {
          (*curr)->value = value;
        }  /* else do nothing and just return true */
      } else {
        (*curr)->value = value;
      }
      return true;
    }
    curr = &(*curr)->next;
  }
  Node *new_node = malloc(sizeof(Node));
  if (!new_node) return false;
  new_node->key = key;
  new_node->value = value;
  new_node->next = NULL;
  *curr = new_node;
  return true;
}

Node *dsa_lookup (const DynamicSetArray *dsa, size_t index, int key) {
  if (!dsa) return NULL;
  size_t pos = 0;
  if (!find_bucket_pos(dsa, index, &pos)) {
    return NULL;
  }
  Node *curr = dsa->buckets[pos].head;
  while (curr != NULL) {
    if (curr->key == key) {
      return curr;
    }
    curr = curr->next;
  }
  return NULL;
}

bool dsa_remove (DynamicSetArray *dsa, size_t index, int key) {
  if (!dsa) return false;
  size_t pos = 0;
  if (!find_bucket_pos(dsa, index, &pos)) {
    return false;
  }
  Node **curr = &dsa->buckets[pos].head;
  while (*curr != NULL) {
    if ((*curr)->key == key) {
      Node *to_delete = *curr;
      *curr = to_delete->next;
      free(to_delete);
      /* Compact the bucket array if this index's list is now empty */
      if (dsa->buckets[pos].head == NULL) {
        if (pos + 1 < dsa->count) {
          memmove(&dsa->buckets[pos],
              &dsa->buckets[pos + 1],
              (dsa->count - pos - 1) * sizeof(Bucket));
        }
        dsa->count--;
        dsa_shrink(dsa);
      }
      return true;
    }
    curr = &(*curr)->next;
  }
  return false;
}

void dsa_destroy (DynamicSetArray *dsa) {
  if (!dsa) return;
  for (size_t i = 0; i < dsa->count; i++) {
    Node *curr = dsa->buckets[i].head;
    while (curr != NULL) {
      Node *temp = curr;
      curr = curr->next;
      free(temp);
    }
  }
  free(dsa->buckets);
  free(dsa);
}

/*
void dsa_print (const DynamicSetArray *dsa) {
  if (!dsa) return;
  printf("DynamicSetArray (count=%zu, capacity=%zu):\n", dsa->count, dsa->capacity);
  for (size_t i = 0; i < dsa->count; i++) {
    printf("  [%zu] -> ", dsa->buckets[i].index);
    Node *curr = dsa->buckets[i].head;
    while (curr != NULL) {
      printf("%d-%d -> ", curr->key, curr->value);
      curr = curr->next;
    }
    printf("NULL\n");
  }
}
*/
