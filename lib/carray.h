#ifndef CARRAY_H
#define CARRAY_H

#include <stdint.h>

/* Shared with arena.h; either header may come first. */
#ifndef MLS_CELL_T_DEFINED
#define MLS_CELL_T_DEFINED
typedef intptr_t cell_t;
#endif

/* Growable cell_t stack. */
typedef struct {
	cell_t *items;
	int len;
	int cap;
} carray_t;

carray_t *ca_new (void);
void ca_free (carray_t *a);
void ca_push (carray_t *a, cell_t v);
cell_t ca_pop (carray_t *a);	     /* INTPTR_MIN if empty */
cell_t ca_peek (carray_t *a, int n); /* negative n counts from top */
int ca_len (carray_t *a);

#endif
