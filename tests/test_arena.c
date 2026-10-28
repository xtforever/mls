#include "arena.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main (void)
{
	/* Empty arena; first zero-sized allocation is address 0. */
	arena_t *a = ar_new (0);
	assert (a);
	assert (ar_alloc (a, 0) == 0);

	/* Sequential allocation, cell roundtrip incl. negative value. */
	int o1 = ar_alloc (a, 8);
	assert (o1 == 0);
	ar_store_cell (a, o1, -1234567890123LL);
	int o2 = ar_alloc (a, 8);
	assert (o2 == 8);
	ar_store_cell (a, o2, 42);
	assert (ar_fetch_cell (a, o1) == -1234567890123LL);
	assert (ar_fetch_cell (a, o2) == 42);

	/* Cross a chunk boundary twice: offsets and old contents stay stable.
	 */
	int o3 = ar_alloc (a, 100);
	assert (o3 == 16);
	ar_store_cell (a, o3, (cell_t)INT64_MIN);
	int o4 = ar_alloc (a, 5000);
	assert (o4 == 116);
	ar_store_cell (a, o4, (cell_t)INT64_MAX);
	assert (ar_fetch_cell (a, o1) == -1234567890123LL);
	assert (ar_fetch_cell (a, o2) == 42);
	assert (ar_fetch_cell (a, o3) == (cell_t)INT64_MIN);
	assert (ar_fetch_cell (a, o4) == (cell_t)INT64_MAX);

	/* Fresh memory is zeroed; bytes roundtrip. */
	int o5 = ar_alloc (a, 4);
	assert (o5 == 5116);
	assert (ar_fetch_byte (a, o5) == 0);
	ar_store_byte (a, o5, 0xAB);
	ar_store_byte (a, o5 + 3, 0xFF);
	assert (ar_fetch_byte (a, o5) == 0xAB);
	assert (ar_fetch_byte (a, o5 + 1) == 0);
	assert (ar_fetch_byte (a, o5 + 3) == 0xFF);

	/* Negative size is rejected and does not consume space. */
	assert (ar_alloc (a, -1) == -1);
	assert (ar_alloc (a, 0) == o5 + 4);

	ar_free (a);
	ar_free (NULL);

	puts ("arena selftest: OK");
	return 0;
}
