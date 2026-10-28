#include "carray.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main (void)
{
	carray_t *a = ca_new ();
	assert (a);
	assert (ca_len (a) == 0);
	assert (ca_pop (a) == INTPTR_MIN); /* underflow sentinel */
	assert (ca_peek (a, 0) == 0);

	/* Growth across the initial capacity (8 -> 16 -> 32). */
	for (cell_t i = 0; i < 20; i++)
		ca_push (a, i);
	assert (ca_len (a) == 20);
	assert (ca_peek (a, 0) == 0);
	assert (ca_peek (a, 19) == 19);
	assert (ca_peek (a, -1) == 19);
	assert (ca_peek (a, -20) == 0);
	assert (ca_peek (a, 20) == 0); /* out of bounds */
	assert (ca_peek (a, -21) == 0);

	assert (ca_pop (a) == 19);
	assert (ca_len (a) == 19);

	/* Extreme cell values roundtrip. */
	ca_push (a, INTPTR_MIN);
	ca_push (a, INTPTR_MAX);
	assert (ca_pop (a) == INTPTR_MAX);
	assert (ca_pop (a) == INTPTR_MIN);

	while (ca_len (a) > 0)
		ca_pop (a);
	assert (ca_len (a) == 0);
	assert (ca_pop (a) == INTPTR_MIN);

	ca_free (a);
	ca_free (NULL);

	puts ("carray selftest: OK");
	return 0;
}
