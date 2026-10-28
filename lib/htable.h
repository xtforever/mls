#ifndef HTABLE_H
#define HTABLE_H

/* Case-insensitive string -> pointer map (Forth dictionary).
   ponytail: fixed 101 buckets, no resize; fine for a word dictionary, but
   lookup degrades to O(n) with many keys. Upgrade: double buckets + rehash. */
typedef struct hte {
	char *key;
	void *value;
	struct hte *next;
} hte_t;

typedef struct {
	hte_t **buckets;
	int nbuckets;
	int count;
} htable_t;

htable_t *ht_new (void);
void ht_free (htable_t *t);
void ht_set (htable_t *t, const char *key, void *value); /* insert or replace */
void *ht_get (htable_t *t, const char *key);		 /* NULL if absent */
int ht_has (htable_t *t,
	    const char *key); /* key exists, even if value == NULL */

#endif
