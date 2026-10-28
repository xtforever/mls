#include "carray.h"

#include <limits.h>
#include <stdlib.h>

#define CA_INIT_CAP 8

carray_t *ca_new (void)
{
	carray_t *a = malloc (sizeof *a);
	if (!a)
		return NULL;
	a->items = malloc (sizeof *a->items * CA_INIT_CAP);
	if (!a->items) {
		free (a);
		return NULL;
	}
	a->len = 0;
	a->cap = CA_INIT_CAP;
	return a;
}

void ca_free (carray_t *a)
{
	if (!a)
		return;
	free (a->items);
	free (a);
}

void ca_push (carray_t *a, cell_t v)
{
	if (a->len >= a->cap) {
		if (a->cap > INT_MAX / 2 / (int)sizeof (cell_t))
			return; /* OOM/overflow */
		int cap = a->cap * 2;
		cell_t *items = realloc (a->items, sizeof *items * (size_t)cap);
		if (!items)
			return;
		a->items = items;
		a->cap = cap;
	}
	a->items[a->len++] = v;
}

cell_t ca_pop (carray_t *a)
{
	if (a->len == 0)
		return INTPTR_MIN; /* sentinel; check ca_len for underflow */
	return a->items[--a->len];
}

cell_t ca_peek (carray_t *a, int n)
{
	if (n < 0)
		n = a->len + n;
	if (n < 0 || n >= a->len)
		return 0;
	return a->items[n];
}

int ca_len (carray_t *a) { return a->len; }
