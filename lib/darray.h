#ifndef DARRAY_H
#define DARRAY_H

/* Growable pointer stack (generic, no ownership of the pointed-to objects). */
typedef struct {
	void **items;
	int len;
	int cap;
} darray_t;

darray_t *da_new (void);
void da_free (darray_t *a);
void da_push (darray_t *a, void *item);
void *da_pop (darray_t *a);	    /* NULL if empty */
void *da_peek (darray_t *a, int n); /* negative n counts from top */
void da_set (darray_t *a, int i, void *item);
void *da_get (darray_t *a, int i);
int da_len (darray_t *a);

#endif
