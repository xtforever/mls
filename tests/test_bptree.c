#include "bptree.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int cmp_i64 (const void *a, const void *b, void *ctx)
{
	int64_t x, y;
	(void)ctx;
	memcpy (&x, a, sizeof x);
	memcpy (&y, b, sizeof y);
	return x < y ? -1 : x > y;
}

typedef struct {
	int64_t a, b;
} pair_t;

static int cmp_pair (const void *a, const void *b, void *ctx)
{
	pair_t x, y;
	(void)ctx;
	memcpy (&x, a, sizeof x);
	memcpy (&y, b, sizeof y);
	if (x.a != y.a)
		return x.a < y.a ? -1 : 1;
	return x.b < y.b ? -1 : x.b > y.b;
}

static uint32_t rng_state = 123456789u;
static uint32_t rnd (void)
{
	rng_state = rng_state * 1103515245u + 12345u;
	return rng_state >> 8;
}

int main (void)
{
	bptree_t *t = bpt_new (sizeof (int64_t), cmp_i64, NULL);
	assert (t);
	assert (bpt_count (t) == 0);
	int64_t v = 0;
	assert (!bpt_get (t, &v, &v));

#define N 200
	int present[N];
	int64_t val[N];
	memset (present, 0, sizeof present);
	memset (val, 0, sizeof val);

	/* Random insert/put/delete, checked against a boolean model. */
	for (int step = 0; step < 5000; step++) {
		int64_t k = (int64_t)(rnd () % N);
		int64_t nv = (int64_t)step * 7 + k;
		if (rnd () & 1) {
			if (!present[k]) {
				assert (bpt_insert (t, &k, nv) == 0);
				present[k] = 1;
				val[k] = nv;
			} else {
				assert (bpt_insert (t, &k, nv) ==
					1); /* duplicate */
				assert (bpt_put (t, &k, nv) == 0);
				val[k] = nv;
			}
		} else {
			int got = bpt_del (t, &k);
			assert (got == present[k]);
			present[k] = 0;
		}
	}

	int64_t live = 0;
	for (int k = 0; k < N; k++)
		live += present[k];
	assert (bpt_count (t) == live);

	for (int k = 0; k < N; k++) {
		int64_t out, kk = k;
		if (present[k]) {
			assert (bpt_get (t, &kk, &out));
			assert (out == val[k]);
		} else {
			assert (!bpt_get (t, &kk, &out));
		}
	}

	/* Forward range scan: keys ascending, values matching the model. */
	bpt_iter_t *it = bpt_iter_new (t);
	assert (it);
	int ok = bpt_iter_first (it);
	int expect = 0;
	while (expect < N && !present[expect])
		expect++;
	while (ok) {
		const void *kp = bpt_iter_key (it);
		assert (kp);
		int64_t k;
		memcpy (&k, kp, sizeof k);
		assert (k == expect);
		assert (bpt_iter_value (it) == val[k]);
		do
			expect++;
		while (expect < N && !present[expect]);
		ok = bpt_iter_next (it);
	}
	assert (expect == N);

	/* Backward scan. */
	ok = bpt_iter_last (it);
	expect = N - 1;
	while (expect >= 0 && !present[expect])
		expect--;
	while (ok) {
		int64_t k;
		memcpy (&k, bpt_iter_key (it), sizeof k);
		assert (k == expect);
		assert (bpt_iter_value (it) == val[k]);
		do
			expect--;
		while (expect >= 0 && !present[expect]);
		ok = bpt_iter_prev (it);
	}
	assert (expect < 0);

	/* seek(klo) + iterate is a [klo,khi) range scan. */
	int64_t klo = 40, khi = 120;
	ok = bpt_iter_seek (it, &klo);
	for (int k = 0; k < N; k++) {
		if (k < klo || !present[k])
			continue;
		if (k >= khi) {
			if (ok) {
				int64_t kk;
				memcpy (&kk, bpt_iter_key (it), sizeof kk);
				assert (kk >= khi);
			}
			break;
		}
		assert (ok);
		int64_t kk;
		memcpy (&kk, bpt_iter_key (it), sizeof kk);
		assert (kk == k);
		ok = bpt_iter_next (it);
	}

	/* Delete everything: tree collapses to empty. */
	for (int k = 0; k < N; k++) {
		int64_t kk = k;
		assert (bpt_del (t, &kk) == present[k]);
		present[k] = 0;
	}
	assert (bpt_count (t) == 0);
	assert (!bpt_iter_first (it));
	assert (!bpt_iter_last (it));
	assert (!bpt_iter_seek (it, &klo));
	bpt_iter_free (it);

	/* Reinsert proves the tree is still usable after full drain. */
	for (int k = 0; k < 20; k++) {
		int64_t kk = k;
		assert (bpt_insert (t, &kk, k * 2) == 0);
	}
	assert (bpt_count (t) == 20);
	for (int k = 0; k < 20; k++) {
		int64_t kk = k, out;
		assert (bpt_get (t, &kk, &out) && out == k * 2);
	}
	bpt_free (t);
	bpt_free (NULL);

	/* Opaque composite keys (16 bytes, lexicographic). */
	bptree_t *p = bpt_new (sizeof (pair_t), cmp_pair, NULL);
	assert (p);
	pair_t keys[] = {{1, 2}, {1, 1}, {2, 0}, {1, 3}, {0, 9}};
	for (int i = 0; i < 5; i++)
		assert (bpt_insert (p, &keys[i], i) == 0);
	assert (bpt_count (p) == 5);
	pair_t needle = {1, 2};
	int64_t out;
	assert (bpt_get (p, &needle, &out) && out == 0);
	bpt_iter_t *pi = bpt_iter_new (p);
	pair_t lo = {1, 1};
	assert (bpt_iter_seek (pi, &lo));
	pair_t prev = {0, 0};
	int seen = 0;
	while (bpt_iter_key (pi)) {
		pair_t cur;
		memcpy (&cur, bpt_iter_key (pi), sizeof cur);
		assert (cmp_pair (&prev, &cur, NULL) < 0);
		prev = cur;
		seen++;
		bpt_iter_next (pi);
	}
	assert (seen == 4); /* {1,1} .. {2,0}; {0,9} is below the seek key */
	bpt_iter_free (pi);
	bpt_free (p);

	puts ("bptree selftest: OK");
	return 0;
}
