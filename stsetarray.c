#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stsetarray.h"

#define EXPECTED_NODES_PER_BUCKET 20

static StaticSetArrayNode *alloc_node (StaticSetArray *ssa) {
  if (ssa->free_list != NULL) {
    StaticSetArrayNode *node = ssa->free_list;
    ssa->free_list = node->next;
    node->from_pool = true;
    return node;
  }
  StaticSetArrayNode *node = malloc(sizeof(StaticSetArrayNode));
  if (node) {
    node->from_pool = false;
  }
  return node;
}


static void free_node (StaticSetArray *ssa, StaticSetArrayNode *node) {
  if (node->from_pool) {
    node->next = ssa->free_list;
    ssa->free_list = node;
  } else {
    free(node);
  }
}


StaticSetArray *ssa_create (size_t capacity) {
  if (capacity == 0) {
    return NULL;
  }
  StaticSetArray *ssa = malloc(sizeof(StaticSetArray));
  if (!ssa) return NULL;
  ssa->buckets = calloc(capacity, sizeof(StaticSetArrayNode *));
  if (!ssa->buckets) {
    free(ssa);
    return NULL;
  }
  size_t pool_size = capacity * EXPECTED_NODES_PER_BUCKET;
  ssa->pool = malloc(pool_size * sizeof(StaticSetArrayNode));
  if (!ssa->pool) {
    free(ssa->buckets);
    free(ssa);
    return NULL;
  }
  /* Thread all pre-allocated nodes into the free list */
  for (size_t i = 0; i + 1 < pool_size; i++) {
    ssa->pool[i].next = &ssa->pool[i + 1];
  }
  ssa->pool[pool_size - 1].next = NULL;
  ssa->free_list = &ssa->pool[0];
  ssa->capacity = capacity;
  return ssa;
}


bool ssa_insert (StaticSetArray *ssa, size_t index, int key, int value, int whengreater) {
  if (!ssa || index >= ssa->capacity) return false;
  StaticSetArrayNode **curr = &ssa->buckets[index];
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
  StaticSetArrayNode *new_node = alloc_node(ssa);
  if (!new_node) return false;
  new_node->key = key;
  new_node->value = value;
  new_node->next = NULL;
  *curr = new_node;
  return true;
}


StaticSetArrayNode *ssa_lookup (const StaticSetArray *ssa, size_t index, int key) {
  if (!ssa || index >= ssa->capacity) return NULL;
  StaticSetArrayNode *curr = ssa->buckets[index];
  while (curr != NULL) {
    if (curr->key == key) {
      return curr;
    }
    curr = curr->next;
  }
  return NULL;
}


bool ssa_remove (StaticSetArray *ssa, size_t index, int key) {
  if (!ssa || index >= ssa->capacity) return false;
  StaticSetArrayNode **curr = &ssa->buckets[index];
  while (*curr != NULL) {
    if ((*curr)->key == key) {
      StaticSetArrayNode *to_delete = *curr;
      *curr = to_delete->next;
      free_node(ssa, to_delete);
      return true;
    }
    curr = &(*curr)->next;
  }
  return false;
}


void ssa_destroy (StaticSetArray *ssa) {
  if (!ssa) return;
  /* Only heap-allocated overflow nodes need individual free() calls */
  for (size_t i = 0; i < ssa->capacity; i++) {
    StaticSetArrayNode *curr = ssa->buckets[i];
    while (curr != NULL) {
      StaticSetArrayNode *temp = curr;
      curr = curr->next;
      if (!temp->from_pool) {
        free(temp);
      }
    }
  }
  free(ssa->pool);
  free(ssa->buckets);
  free(ssa);
}
