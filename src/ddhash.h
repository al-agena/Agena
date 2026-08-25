#ifndef ddhash_h
#define ddhash_h

typedef enum { DDHASH_EMPTY, DDHASH_OCCUPIED, DDHASH_DELETED } DdHashTableEntryStatus;

typedef struct {
  DdHashTableEntryStatus status;
  double key;
  double value;
} DdHashTableEntry;

typedef struct {
  DdHashTableEntry *entries;
  size_t capacity;  /* total number of slots */
  size_t count;     /* tracks DDHASH_OCCUPIED entries for resizing */
} DdHashTable;


DdHashTable      *ddht_create (size_t size);
int               ddht_delete (DdHashTable *ht, double key, double eps);
int               ddht_shrink (DdHashTable *ht, size_t requested_size);
void              ddht_destroy (DdHashTable *ht, int all);
DdHashTableEntry *ddht_get (DdHashTable *ht, double key);
double            ddht_getentry (DdHashTable *ht, double key, int *rc);
DdHashTableEntry *ddht_getbyhash (DdHashTable *ht, unsigned int hashval, size_t *idx);
int               ddht_insert (DdHashTable *ht, double key, double value);

#endif
