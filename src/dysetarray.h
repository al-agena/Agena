#ifndef DYSETARRAY_H
#define DYSETARRAY_H

/**
 * @file dynamic_list_array.h
 * @brief A sparse, dynamically resizing array of singly-linked integer key-value lists.
 * @creator: Gemini AI, 21/11/2026
 * @licence: public domain
 *
 * Overview:
 *   This module implements a two-tier data structure designed to map non-negative
 *   0-based sparse indices to ordered singly-linked lists of (key, value) integer pairs.
 *
 * Architecture & Behavior:
 *   - Sparse Indexing: Rather than allocating empty slots for unused indices, active
 *     indices are stored in a contiguous dynamic array sorted in ascending order and
 *     located via binary search in O(log n) time.
 *   - Automatic Memory Management: The bucket array doubles in capacity when full and
 *     automatically halves when occupancy drops to 25% or below (down to a minimum floor)
 *     to prevent memory waste without resize thrashing.
 *   - Ordered Upsert Chains: Within each index bucket, entries are maintained in insertion
 *     order. Inserting a new key appends it to the tail of the list; inserting an existing
 *     key updates its associated value in place without altering its position.
 *   - Automatic Compaction: When the last node in a bucket's linked list is removed, the
 *     empty bucket is automatically evicted from the array.
 */

#include <stdbool.h>
#include <stddef.h>

typedef struct Node {
  int key;
  int value;
  struct Node *next;
} Node;

typedef struct {
  size_t index;   // The logical sparse index (e.g., 1, 2, 5000)
  Node *head;     // Head of the singly-linked list for this index
} Bucket;

typedef struct {
  Bucket *buckets; // Sorted dynamic array of active buckets
  size_t count;    // Number of active indices currently stored
  size_t capacity; // Allocated capacity of `buckets`
} DynamicSetArray;

/* Lifecycle */
DynamicSetArray *dsa_create (size_t initial_capacity);
void dsa_destroy (DynamicSetArray *dsa);

/* Core operations */
bool dsa_insert (DynamicSetArray *dsa, size_t index, int key, int value, int whengreater);
Node *dsa_lookup (const DynamicSetArray *dsa, size_t index, int key);
bool dsa_remove (DynamicSetArray *dsa, size_t index, int key);

/* Miscellaneous */
#define dsa_getidx(i) dsa->buckets[i].index

/* Debugging
void dsa_print (const DynamicSetArray *dsa); */

#endif /* DYSETARRAY_H */
