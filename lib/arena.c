#include "arena.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

_Static_assert (sizeof (cell_t) <= sizeof (uint64_t),
		"cell_t wider than 64 bit");

arena_t *ar_new (int size)
{
	arena_t *a = calloc (1, sizeof *a);
	if (!a)
		return NULL;
	if (size < 0)
		size = 0;
	if (size > 0) {
		a->mem = calloc (1, (size_t)size);
		if (!a->mem) {
			free (a);
			return NULL;
		}
	}
	a->size = size;
	return a;
}

void ar_free (arena_t *a)
{
	if (!a)
		return;
	free (a->mem);
	free (a);
}

static int ar_reserve (arena_t *a, int need)
{
	if (need <= a->size)
		return 0;

	int newsize = a->size > 0 ? a->size : 64;
	while (newsize < need) {
		if (newsize > INT_MAX / 2) {
			newsize = need;
			break;
		}
		newsize *= 2;
	}

	uint8_t *p = realloc (a->mem, (size_t)newsize);
	if (!p)
		return -1;
	memset (p + a->size, 0, (size_t)(newsize - a->size));
	a->mem = p;
	a->size = newsize;
	return 0;
}

int ar_alloc (arena_t *a, int n)
{
	if (!a || n < 0 || n > INT_MAX - a->used)
		return -1;
	int addr = a->used;
	int need = a->used + n;
	if (ar_reserve (a, need) != 0)
		return -1;
	a->used = need;
	return addr;
}

/* Little-endian on purpose: deterministic layout regardless of host endianness
   (NFR-06 documents x86-64, but the arena is also a Forth memory image). */
void ar_store_cell (arena_t *a, int offset, cell_t v)
{
	uint64_t u = (uint64_t)v;
	for (size_t i = 0; i < sizeof (cell_t); i++)
		a->mem[offset + (int)i] = (uint8_t)(u >> (i * 8));
}

cell_t ar_fetch_cell (arena_t *a, int offset)
{
	uint64_t u = 0;
	for (size_t i = 0; i < sizeof (cell_t); i++)
		u |= (uint64_t)a->mem[offset + (int)i] << (i * 8);
	return (cell_t)(int64_t)u;
}

void ar_store_byte (arena_t *a, int offset, uint8_t b) { a->mem[offset] = b; }

uint8_t ar_fetch_byte (arena_t *a, int offset) { return a->mem[offset]; }
