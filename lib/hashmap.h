#ifndef HASHMAP_H
#define HASHMAP_H

#include <stdint.h>

/* Case-sensitive string -> uint32 map with open addressing (FNV-1a).
   Keys are BORROWED: they must stay valid and unchanged while the map lives
   (m_strpool points them at its id2str allocations). Values are owned by the
   caller. No deletion: maps only grow (append-only IDs).
   ponytail: no tombstones/delete; linear probing, load factor <= 1/2.
   Upgrade: tombstones + backward-shift deletion if removal is needed. */
typedef struct hashmap hashmap_t;

hashmap_t *hm_new (void);
void hm_free (hashmap_t *m);
int hm_put (hashmap_t *m, const char *key, uint32_t value); /* 0 ok, -1 OOM */
int hm_get (const hashmap_t *m, const char *key,
	    uint32_t *out); /* 1 found, 0 miss */
int hm_has (const hashmap_t *m, const char *key);
int hm_len (const hashmap_t *m);

#endif
