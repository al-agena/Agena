#ifndef STSETARRAY_H
#define STSETARRAY_H

/**
 * @file dysetarray.h
 * @brief A fixed-capacity array of singly-linked integer key-value lists backed by a node pool.
 * @creator: Gemini AI, 23/11/2026
 * @licence: public domain
 *
 * Overview:
 *   This module implements a two-tier data structure designed to map 0-based indices
 *   [0 .. capacity - 1] to ordered singly-linked lists of (key, value) integer pairs:
 *
 *    [0] ------- [1] ------- [2] -------- [3]
 *     |           |           |            |
 *    10 - 11     10 - 11     NULL         111 - 112
 *     |           |                        |
 *    30 - 31     30 - 32                  ...
 *     |           |
 *    20 - 20     NULL
 *     |
 *   NULL
 *
 * Architecture & Behavior:
 *   - Direct Indexing: Buckets are stored in a fixed-size contiguous array allocated at
 *     creation time, providing O(1) index access without resizing overhead.
 *   - Pre-Allocated Node Pool: Nodes are served from a single contiguous slab allocated
 *     up front (sized for typical bucket occupancy) and recycled via an internal free
 *     list, falling back to standard heap allocation only if the slab is exhausted.
 *   - Ordered Upsert Chains: Within each index bucket, entries are maintained in insertion
 *     order. Inserting a new key appends it to the tail of the list; inserting an existing
 *     key updates its associated value in place (or conditionally when `whengreater` is set).
 */

#include <stdbool.h>
#include <stddef.h>

typedef struct StaticSetArrayNode {
  int key;
  int value;
  struct StaticSetArrayNode *next;
  bool from_pool;   /* Tracks whether node belongs to the pre-allocated slab */
} StaticSetArrayNode;

typedef struct {
  StaticSetArrayNode **buckets;   /* Direct-indexed array of list heads */
  StaticSetArrayNode *pool;       /* Contiguous pre-allocated block of nodes */
  StaticSetArrayNode *free_list;  /* Stack of available pool nodes */
  size_t capacity;  /* Number of buckets allocated */
} StaticSetArray;

/* Lifecycle */
StaticSetArray *ssa_create (size_t initial_capacity);
void ssa_destroy (StaticSetArray *ssa);

/* Core operations */
bool ssa_insert (StaticSetArray *ssa, size_t index, int key, int value, int whengreater);
StaticSetArrayNode *ssa_lookup (const StaticSetArray *ssa, size_t index, int key);
bool ssa_remove (StaticSetArray *ssa, size_t index, int key);

#endif /* STSETARRAY_H */
