#ifndef strhash_h
#define strhash_h

typedef enum { STRHASH_EMPTY, STRHASH_OCCUPIED, STRHASH_DELETED } StrHashTableEntryStatus;

typedef struct {
  uint32_t meta;  /* 2 bits for StrHashTableEntryStatus, 30 bits for length of the key */
  char *key;      /* the string key and ... */
  double d;       /* ... the associated value */ 
} StrHashTableEntry;

typedef struct {
  StrHashTableEntry *entries;
  size_t capacity;  /* total number of slots */
  size_t count;     /* tracks STRHASH_OCCUPIED entries for resizing */
} StrHashTable;

StrHashTable      *sht_create (size_t size);
int               sht_delete (StrHashTable *ht, const char *key, size_t len);
int               sht_shrink (StrHashTable *ht, size_t requested_size);
void              sht_destroy (StrHashTable *ht, int all);
StrHashTableEntry *sht_get (StrHashTable *ht, const char *key, size_t len);
StrHashTableEntry *sht_getbyhash (StrHashTable *ht, unsigned long h_val);
int               sht_insert (StrHashTable *ht, const char *key, size_t len, double value);

#endif
