#include "hashmap.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#define N 1000

int main (void)
{
	hashmap_t *m = hm_new ();
	assert (m);
	assert (hm_len (m) == 0);

	uint32_t v = 123;
	assert (!hm_get (m, "missing", &v));
	assert (v == 123); /* untouched on miss */
	assert (!hm_has (m, "missing"));

	assert (hm_put (m, "alpha", 1) == 0);
	assert (hm_put (m, "beta", 2) == 0);
	assert (hm_put (m, "zero", 0) == 0); /* 0 is a valid value */
	assert (hm_len (m) == 3);

	assert (hm_get (m, "alpha", &v) && v == 1);
	assert (hm_get (m, "beta", &v) && v == 2);
	assert (hm_get (m, "zero", &v) && v == 0);
	assert (hm_has (m, "zero"));

	/* Case-sensitive: different key, not a hit. */
	assert (!hm_has (m, "Alpha"));
	assert (hm_put (m, "Alpha", 11) == 0);
	assert (hm_len (m) == 4);
	assert (hm_get (m, "Alpha", &v) && v == 11);
	assert (hm_get (m, "alpha", &v) && v == 1);

	/* Redefinition replaces, does not grow. */
	assert (hm_put (m, "alpha", 99) == 0);
	assert (hm_len (m) == 4);
	assert (hm_get (m, "alpha", &v) && v == 99);

	/* Many keys force collisions and rehashes. Stable key storage
	 * (borrowed). */
	static char keys[N][16];
	for (int i = 0; i < N; i++) {
		snprintf (keys[i], sizeof keys[i], "key-%d", i);
		assert (hm_put (m, keys[i], (uint32_t)(i + 1)) == 0);
	}
	assert (hm_len (m) == N + 4);
	for (int i = 0; i < N; i++) {
		assert (hm_get (m, keys[i], &v));
		assert (v == (uint32_t)(i + 1));
	}
	assert (!hm_has (m, "key-1000"));
	assert (hm_get (m, "zero", &v) && v == 0);

	/* Invalid arguments. */
	assert (hm_put (m, NULL, 1) == -1);
	assert (!hm_get (m, NULL, &v));
	assert (!hm_get (m, "alpha", NULL));

	hm_free (m);
	hm_free (NULL);
	assert (hm_len (NULL) == 0);

	puts ("hashmap selftest: OK");
	return 0;
}
