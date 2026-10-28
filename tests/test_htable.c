#include "htable.h"

#include <assert.h>
#include <stdio.h>

int main (void)
{
	htable_t *t = ht_new ();
	assert (t);
	assert (t->count == 0);

	int a = 1, b = 2, c = 3;
	assert (!ht_has (t, "missing"));
	assert (ht_get (t, "missing") == NULL);

	ht_set (t, "alpha", &a);
	assert (ht_has (t, "alpha"));
	assert (ht_get (t, "alpha") == &a);
	assert (t->count == 1);

	/* Case-insensitive. */
	assert (ht_has (t, "ALPHA"));
	assert (ht_get (t, "AlPhA") == &a);

	/* Redefinition replaces the value, does not add an entry. */
	ht_set (t, "Alpha", &b);
	assert (ht_get (t, "alpha") == &b);
	assert (t->count == 1);

	/* NULL is a storable value and still counts as present. */
	ht_set (t, "nil", NULL);
	assert (ht_has (t, "nil"));
	assert (ht_get (t, "nil") == NULL);
	assert (t->count == 2);

	/* Chaining: more keys than buckets. */
	char key[32];
	for (int i = 0; i < 500; i++) {
		snprintf (key, sizeof key, "word-%d", i);
		ht_set (t, key, &c);
	}
	assert (t->count == 502);
	for (int i = 0; i < 500; i++) {
		snprintf (key, sizeof key, "WORD-%d", i);
		assert (ht_get (t, key) == &c);
	}
	assert (ht_get (t, "alpha") == &b);

	ht_free (t);
	ht_free (NULL);

	puts ("htable selftest: OK");
	return 0;
}
