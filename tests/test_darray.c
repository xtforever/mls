#include "darray.h"

#include <assert.h>
#include <stdio.h>

int main (void)
{
	darray_t *a = da_new ();
	assert (a);
	assert (da_len (a) == 0);
	assert (da_pop (a) == NULL);
	assert (da_peek (a, 0) == NULL);

	/* Growth across the initial capacity (8 -> 16 -> 32). */
	int vals[20];
	for (int i = 0; i < 20; i++) {
		vals[i] = i;
		da_push (a, &vals[i]);
	}
	assert (da_len (a) == 20);
	assert (da_peek (a, 0) == &vals[0]);
	assert (da_peek (a, 19) == &vals[19]);
	assert (da_peek (a, -1) == &vals[19]);
	assert (da_peek (a, -20) == &vals[0]);
	assert (da_peek (a, 20) == NULL);
	assert (da_peek (a, -21) == NULL);

	/* Random access; out-of-range writes are ignored. */
	da_set (a, 0, &vals[5]);
	assert (da_get (a, 0) == &vals[5]);
	da_set (a, -1, &vals[1]);
	da_set (a, 20, &vals[1]);
	assert (da_get (a, 0) == &vals[5]);
	assert (da_get (a, -1) == NULL);
	assert (da_get (a, 100) == NULL);

	/* NULL is a valid stored item. */
	da_push (a, NULL);
	assert (da_len (a) == 21);
	assert (da_peek (a, -1) == NULL);
	assert (da_pop (a) == NULL);
	assert (da_len (a) == 20);

	assert (da_pop (a) == &vals[19]);
	while (da_len (a) > 0)
		da_pop (a);
	assert (da_pop (a) == NULL);

	da_free (a);
	da_free (NULL);

	puts ("darray selftest: OK");
	return 0;
}
