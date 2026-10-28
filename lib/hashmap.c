#include "hashmap.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define HM_INIT_CAP 16

typedef struct {
	const char *key;
	uint32_t value;
} hm_slot_t;

struct hashmap {
	hm_slot_t *slots;
	size_t cap; /* power of two */
	size_t count;
};

/* FNV-1a, 64 bit, case-sensitive. */
static uint64_t hm_hash (const char *s)
{
	uint64_t h = 14695981039346656037ULL;
	for (; *s; s++) {
		h ^= (unsigned char)*s;
		h *= 1099511628211ULL;
	}
	return h;
}

static size_t hm_mask (const hashmap_t *m) { return m->cap - 1; }

hashmap_t *hm_new (void)
{
	hashmap_t *m = malloc (sizeof *m);
	if (!m)
		return NULL;
	m->slots = calloc (HM_INIT_CAP, sizeof *m->slots);
	if (!m->slots) {
		free (m);
		return NULL;
	}
	m->cap = HM_INIT_CAP;
	m->count = 0;
	return m;
}

void hm_free (hashmap_t *m)
{
	if (!m)
		return;
	free (m->slots); /* keys are borrowed */
	free (m);
}

static int hm_grow (hashmap_t *m)
{
	if (m->cap > (size_t)-1 / 2 / sizeof *m->slots)
		return -1;
	size_t cap = m->cap * 2;
	hm_slot_t *slots = calloc (cap, sizeof *slots);
	if (!slots)
		return -1;

	for (size_t j = 0; j < m->cap; j++) {
		if (!m->slots[j].key)
			continue;
		size_t i = (size_t)hm_hash (m->slots[j].key) & (cap - 1);
		while (slots[i].key)
			i = (i + 1) & (cap - 1);
		slots[i] = m->slots[j];
	}

	free (m->slots);
	m->slots = slots;
	m->cap = cap;
	return 0;
}

int hm_put (hashmap_t *m, const char *key, uint32_t value)
{
	if (!m || !key)
		return -1;

	/* Keep the load factor <= 1/2, so a probe always meets a free slot. */
	if ((m->count + 1) * 2 > m->cap && hm_grow (m) != 0)
		return -1;

	size_t i = (size_t)hm_hash (key) & hm_mask (m);
	while (m->slots[i].key) {
		if (strcmp (m->slots[i].key, key) == 0) {
			m->slots[i].value = value; /* redefine */
			return 0;
		}
		i = (i + 1) & hm_mask (m);
	}
	m->slots[i].key = key;
	m->slots[i].value = value;
	m->count++;
	return 0;
}

int hm_get (const hashmap_t *m, const char *key, uint32_t *out)
{
	if (!m || !key || !out)
		return 0;
	size_t i = (size_t)hm_hash (key) & hm_mask (m);
	while (m->slots[i].key) {
		if (strcmp (m->slots[i].key, key) == 0) {
			*out = m->slots[i].value;
			return 1;
		}
		i = (i + 1) & hm_mask (m);
	}
	return 0;
}

int hm_has (const hashmap_t *m, const char *key)
{
	uint32_t v;
	return hm_get (m, key, &v);
}

int hm_len (const hashmap_t *m) { return m ? (int)m->count : 0; }
