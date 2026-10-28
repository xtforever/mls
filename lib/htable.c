#include "htable.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define HT_BUCKETS 101

static unsigned ht_hash (const char *key)
{
	unsigned h = 5381;
	for (; *key; key++)
		h = ((h << 5) + h) ^
		    (unsigned char)toupper ((unsigned char)*key);
	return h;
}

static int ht_key_eq (const char *a, const char *b)
{
	while (*a && *b) {
		if (toupper ((unsigned char)*a) != toupper ((unsigned char)*b))
			return 0;
		a++;
		b++;
	}
	return *a == *b;
}

htable_t *ht_new (void)
{
	htable_t *t = malloc (sizeof *t);
	if (!t)
		return NULL;
	t->buckets = calloc (HT_BUCKETS, sizeof *t->buckets);
	if (!t->buckets) {
		free (t);
		return NULL;
	}
	t->nbuckets = HT_BUCKETS;
	t->count = 0;
	return t;
}

void ht_free (htable_t *t)
{
	if (!t)
		return;
	for (int i = 0; i < t->nbuckets; i++) {
		hte_t *e = t->buckets[i];
		while (e) {
			hte_t *next = e->next;
			free (e->key);
			free (e); /* values are owned by the caller */
			e = next;
		}
	}
	free (t->buckets);
	free (t);
}

static char *ht_strdup (const char *s)
{
	size_t n = strlen (s) + 1;
	char *p = malloc (n);
	if (p)
		memcpy (p, s, n);
	return p;
}

static hte_t *ht_find (htable_t *t, const char *key)
{
	unsigned h = ht_hash (key) % (unsigned)t->nbuckets;
	for (hte_t *e = t->buckets[h]; e; e = e->next)
		if (ht_key_eq (e->key, key))
			return e;
	return NULL;
}

void ht_set (htable_t *t, const char *key, void *value)
{
	hte_t *e = ht_find (t, key);
	if (e) {
		e->value = value;
		return;
	}

	char *k = ht_strdup (key);
	if (!k)
		return;
	e = malloc (sizeof *e);
	if (!e) {
		free (k);
		return;
	}
	unsigned h = ht_hash (key) % (unsigned)t->nbuckets;
	e->key = k;
	e->value = value;
	e->next = t->buckets[h];
	t->buckets[h] = e;
	t->count++;
}

void *ht_get (htable_t *t, const char *key)
{
	hte_t *e = ht_find (t, key);
	return e ? e->value : NULL;
}

int ht_has (htable_t *t, const char *key) { return ht_find (t, key) != NULL; }
