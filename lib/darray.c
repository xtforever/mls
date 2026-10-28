#include "darray.h"

#include <limits.h>
#include <stdlib.h>

#define DA_INIT_CAP 8

darray_t *da_new (void)
{
	darray_t *a = malloc (sizeof *a);
	if (!a)
		return NULL;
	a->items = malloc (sizeof *a->items * DA_INIT_CAP);
	if (!a->items) {
		free (a);
		return NULL;
	}
	a->len = 0;
	a->cap = DA_INIT_CAP;
	return a;
}

void da_free (darray_t *a)
{
	if (!a)
		return;
	free (a->items);
	free (a);
}

void da_push (darray_t *a, void *item)
{
	if (a->len >= a->cap) {
		if (a->cap > INT_MAX / 2 / (int)sizeof (void *))
			return; /* OOM/overflow */
		int cap = a->cap * 2;
		void **items = realloc (a->items, sizeof *items * (size_t)cap);
		if (!items)
			return;
		a->items = items;
		a->cap = cap;
	}
	a->items[a->len++] = item;
}

void *da_pop (darray_t *a)
{
	if (a->len == 0)
		return NULL;
	return a->items[--a->len];
}

void *da_peek (darray_t *a, int n)
{
	if (n < 0)
		n = a->len + n;
	if (n < 0 || n >= a->len)
		return NULL;
	return a->items[n];
}

void da_set (darray_t *a, int i, void *item)
{
	if (i < 0 || i >= a->len)
		return;
	a->items[i] = item;
}

void *da_get (darray_t *a, int i)
{
	if (i < 0 || i >= a->len)
		return NULL;
	return a->items[i];
}

int da_len (darray_t *a) { return a->len; }
