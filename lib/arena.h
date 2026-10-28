#ifndef ARENA_H
#define ARENA_H

#include <stdint.h>

/* A Forth cell. Shared by later generic primitives (carray, ...). */
#ifndef MLS_CELL_T_DEFINED
#define MLS_CELL_T_DEFINED
typedef intptr_t cell_t;
#endif

/* Flat, growable byte arena. Addresses are offsets from the arena base and
   stay valid across growth (the backing store may be reallocated).
   ponytail: only offsets are stable, raw pointers into mem are not. Upgrade
   to a chunk list if callers must keep raw pointers (see R-2). */
typedef struct {
	uint8_t *mem;
	int size;
	int used; /* ≡ HERE */
} arena_t;

arena_t *ar_new (int size);
void ar_free (arena_t *a);
int ar_alloc (arena_t *a, int n); /* -1 on failure */
void ar_store_cell (arena_t *a, int offset, cell_t v);
cell_t ar_fetch_cell (arena_t *a, int offset);
void ar_store_byte (arena_t *a, int offset, uint8_t b);
uint8_t ar_fetch_byte (arena_t *a, int offset);

#endif
