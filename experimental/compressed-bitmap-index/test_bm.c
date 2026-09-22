/* Self-check for the chunked bitmap index. See design.md §10. */
#include "bm.h"
#include "mls.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static void test_basic (void)
{
	int bs = bm_create ();
	assert (bs > 0);
	assert (bm_chunks (bs) == 0);

	assert (bm_test (bs, 0) == 0);
	assert (bm_set (bs, 0) == 0);
	assert (bm_set (bs, 63) == 0);
	assert (bm_set (bs, 64) == 0);
	assert (bm_set (bs, 65) == 0);
	assert (bm_set (bs, 1000 * 64 + 5) == 0);

	assert (bm_test (bs, 0) == 1);
	assert (bm_test (bs, 63) == 1);
	assert (bm_test (bs, 64) == 1);
	assert (bm_test (bs, 65) == 1);
	assert (bm_test (bs, 1000 * 64 + 5) == 1);
	assert (bm_test (bs, 1) == 0);
	assert (bm_test (bs, 62) == 0);
	assert (bm_test (bs, 66) == 0);
	assert (bm_test (bs, 1000 * 64 + 6) == 0);

	/* 0,63,64,65 are contiguous -> one chunk; the far bit -> a second */
	assert (bm_chunks (bs) == 2);

	assert (bm_clr (bs, 64) == 0);
	assert (bm_test (bs, 64) == 0);
	assert (bm_test (bs, 65) == 1);
	assert (bm_clr (bs, 1) == 0); /* clearing an implicit zero is a no-op */
	bm_destroy (bs);
}

static void test_gap_join (void)
{
	/* contiguous / one-word gap: one chunk */
	int a = bm_create ();
	bm_set (a, 10 * 64 + 3);
	bm_set (a, 11 * 64 + 7); /* word 11, end was 11 -> gap 0 */
	assert (bm_chunks (a) == 1);
	bm_set (a, 12 * 64 + 1); /* end 12 -> gap 0 */
	assert (bm_chunks (a) == 1);
	bm_destroy (a);

	/* two-word gap: new chunk */
	int b = bm_create ();
	bm_set (b, 10 * 64 + 3); /* chunk words [10,11) */
	bm_set (b, 13 * 64 + 1); /* sw=13, end=11, gap 2 > GAP_JOIN */
	assert (bm_chunks (b) == 2);
	bm_destroy (b);
}

static void test_split_merge_compact (void)
{
	int bs = bm_create ();
	bm_set (bs, 0);
	bm_set (bs, 2 * 64); /* sw=2, gap 1 -> extend, words 0..2 -> len 3 */
	assert (bm_chunks (bs) == 1);
	assert (bm_chunk_words (bs, 0) == 3);

	assert (bm_split (bs, 2 * 64) == 0);
	assert (bm_chunks (bs) == 2);
	assert (bm_chunk_bitpos (bs, 0) == 0);
	assert (bm_chunk_words (bs, 0) == 2);
	assert (bm_chunk_bitpos (bs, 1) == 2 * 64);
	assert (bm_chunk_words (bs, 1) == 1);
	assert (bm_test (bs, 0) == 1);
	assert (bm_test (bs, 2 * 64) == 1);

	assert (bm_merge (bs, 0) == 0);
	assert (bm_chunks (bs) == 1);
	assert (bm_chunk_words (bs, 0) == 3);
	assert (bm_test (bs, 2 * 64) == 1);

	/* clearing everything makes the chunk vanish (trailing trim) */
	assert (bm_clr (bs, 0) == 0);
	assert (bm_clr (bs, 2 * 64) == 0);
	assert (bm_chunks (bs) == 0);
	assert (bm_compact (bs) == 0);
	assert (bm_chunks (bs) == 0);
	bm_destroy (bs);
}

static void test_scans (void)
{
	int bs = bm_create ();
	bm_set (bs, 5);
	bm_set (bs, 100);
	bm_set (bs, 1000);

	assert (bm_next1 (bs, 0) == 5);
	assert (bm_next1 (bs, 6) == 100);
	assert (bm_next1 (bs, 101) == 1000);
	assert (bm_next1 (bs, 1001) == BM_NONE);
	assert (bm_prev1 (bs, 1000) == 1000);
	assert (bm_prev1 (bs, 999) == 100);
	assert (bm_prev1 (bs, 4) == BM_NONE);

	assert (bm_next0 (bs, 0) == 0);
	assert (bm_next0 (bs, 5) == 6);
	assert (bm_next0 (bs, 99) == 99);
	assert (bm_next0 (bs, 100) == 101);
	assert (bm_next0 (bs, 200) == 200); /* inside the gap */
	assert (bm_prev0 (bs, 4) == 4);
	assert (bm_prev0 (bs, 5) == 4);
	assert (bm_prev0 (bs, 100) == 99);
	assert (bm_prev0 (bs, 101) == 101);
	assert (bm_prev0 (bs, 1000) == 999);

	/* full word: first zero is just past it */
	int f = bm_create ();
	for (int i = 0; i < 64; i++)
		bm_set (f, (bm_bit_t)i);
	assert (bm_next0 (f, 0) == 64);
	assert (bm_next0 (f, 63) == 64);
	assert (bm_first0 (f, 0) == BM_NONE);
	assert (bm_last0 (f, 0) == BM_NONE);
	assert (bm_first1 (f, 0) == 0);
	assert (bm_last1 (f, 0) == 63);
	bm_destroy (f);

	bm_destroy (bs);
}

/* scalar reference over a small range */
static void test_setops (void)
{
	enum { R = 256 };
	uint8_t a[R] = {0}, b[R] = {0};
	int ba = bm_create (), bb = bm_create ();
	srand (4242);
	for (int i = 0; i < R; i++) {
		if (rand () & 1) {
			a[i] = 1;
			bm_set (ba, (bm_bit_t)i);
		}
		if (rand () & 1) {
			b[i] = 1;
			bm_set (bb, (bm_bit_t)i);
		}
	}
	bm_bit_t start = 17, count = 200; /* non-aligned range */

	int r_and = bm_and (0, ba, bb, start, count);
	assert (r_and > 0);
	int r_or = bm_or (0, ba, bb, start, count);
	assert (r_or > 0);
	int r_xor = bm_xor (0, ba, bb, start, count);
	assert (r_xor > 0);

	for (int i = 0; i < R; i++) {
		int in = i >= (int)start && i < (int)(start + count);
		int exp_and = in ? (a[i] & b[i]) : 0;
		int exp_or = in ? (a[i] | b[i]) : 0;
		int exp_xor = in ? (a[i] ^ b[i]) : 0;
		assert (bm_test (r_and, (bm_bit_t)i) == exp_and);
		assert (bm_test (r_or, (bm_bit_t)i) == exp_or);
		assert (bm_test (r_xor, (bm_bit_t)i) == exp_xor);
	}

	/* aliasing: c = a; c OR b in place */
	int c = bm_create ();
	for (int i = 0; i < R; i++)
		if (a[i])
			bm_set (c, (bm_bit_t)i);
	assert (bm_or (c, c, bb, 0, R) == c);
	for (int i = 0; i < R; i++)
		assert (bm_test (c, (bm_bit_t)i) == (a[i] | b[i]));

	bm_destroy (c);
	bm_destroy (r_and);
	bm_destroy (r_or);
	bm_destroy (r_xor);
	bm_destroy (ba);
	bm_destroy (bb);
}

/* fuzzy: random numbers are set, then all of them must be present */
static void test_fuzzy (void)
{
	enum { N = 20000, RANGE = 100000 };
	uint8_t *present = calloc (RANGE, 1);
	int *nums = malloc (N * sizeof *nums);
	assert (present && nums);

	int bs = bm_create ();
	srand (0xB17);
	for (int i = 0; i < N; i++) {
		int n = rand () % RANGE;
		nums[i] = n;
		present[n] = 1;
		assert (bm_set (bs, (bm_bit_t)n) == 0);
	}

	/* every recorded number is stored */
	for (int i = 0; i < N; i++)
		assert (bm_test (bs, (bm_bit_t)nums[i]) == 1);

	/* bitmap agrees with the reference everywhere */
	size_t distinct = 0;
	for (int n = 0; n < RANGE; n++) {
		assert (bm_test (bs, (bm_bit_t)n) == present[n]);
		distinct += present[n];
	}

	/* iteration visits exactly the set bits, in order */
	int64_t bit = bm_next1 (bs, 0);
	for (int n = 0; n < RANGE; n++) {
		if (present[n]) {
			assert (bit == n);
			bit = bm_next1 (bs, (bm_bit_t)(n + 1));
		}
	}
	assert (bit == BM_NONE);

	/* clear every other recorded number, re-verify */
	for (int i = 0; i < N; i++) {
		if (i % 2 == 0) {
			present[nums[i]] = 0;
			assert (bm_clr (bs, (bm_bit_t)nums[i]) == 0);
		}
	}
	for (int n = 0; n < RANGE; n++)
		assert (bm_test (bs, (bm_bit_t)n) == present[n]);

	assert (bm_compact (bs) == 0);
	for (int n = 0; n < RANGE; n++)
		assert (bm_test (bs, (bm_bit_t)n) == present[n]);

	printf ("fuzzy: %d sets, %zu distinct, %d chunks\n", N, distinct,
		bm_chunks (bs));

	bm_destroy (bs);
	free (present);
	free (nums);
}

static void test_ownership (void)
{
	size_t before = m_count_allocated ();
	int bs = bm_create ();
	for (int i = 0; i < 5000; i += 7)
		bm_set (bs, (bm_bit_t)(i * 13));
	assert (m_count_allocated () > before);
	bm_destroy (bs);
	assert (m_count_allocated () == before);
}

int main (void)
{
	m_init ();
	bm_init ();
	test_basic ();
	test_gap_join ();
	test_split_merge_compact ();
	test_scans ();
	test_setops ();
	test_fuzzy ();
	test_ownership ();
	m_destruct ();
	printf ("all bm tests passed\n");
	return 0;
}
