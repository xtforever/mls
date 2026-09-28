/* Chunked compressed bitmap index. See design.md.
 *
 * A chunk is either:
 *   - inline: one 64-bit word stored directly in the chunk (no allocation),
 *     tagged by the sign bit of `data`; or
 *   - a handle to a uint64_t array for multi-word chunks.
 *
 * `bitpos` packs (word_index << 6) | bit63_of_inline_word. The low bit is free
 * because chunk starts are 64-bit aligned, and word_index lives in bits 6..31
 * so `bitpos >> 6` is the word index regardless of the tag. */
#include "bm.h"
#include "mls.h"

#include <stdlib.h>
#include <string.h>

#define BM_WORD 64u
#define BM_GAP_JOIN 1u	   /* implicit zero words worth materialising */
#define BM_MAX_WORDS 1024u /* soft cap before a chunk is split */

struct chunk {
	uint32_t bitpos; /* (word_index << 6) | hi_bit */
	int64_t data;	 /* >= 0: MLS handle; < 0: inline word */
};

#define CH_INLINE(c) ((c)->data < 0)
#define CH_WORDIDX(c) ((c)->bitpos >> 6)
#define CH_BITPOS(c) ((uint32_t)((c)->bitpos & ~1u))
#define CH_HANDLE(c) ((int)(c)->data)
#define INLINE_WORD(c)                                                         \
	(((uint64_t)(c)->data & 0x7fffffffffffffffull) |                       \
	 ((uint64_t)((c)->bitpos & 1u) << 63))

static inline int64_t inline_data (uint64_t w)
{
	return (int64_t)((w & 0x7fffffffffffffffull) | (1ULL << 63));
}
static inline uint32_t inline_bitpos (uint32_t word_idx, uint64_t w)
{
	return (word_idx << 6) | (uint32_t)((w >> 63) & 1u);
}

static int BM_FREE = -1;

/* ---- lifecycle ---------------------------------------------------------- */

static void bm_free (int bs)
{
	int i;
	struct chunk *c;
	m_foreach (bs, i, c)
	{
		if (!CH_INLINE (c))
			m_free (CH_HANDLE (c));
	}
}

int bm_init (void)
{
	if (BM_FREE >= 0)
		return 0;
	BM_FREE = m_reg_freefn (bm_free);
	return BM_FREE < 0 ? -1 : 0;
}

int bm_create (void)
{
	if (bm_init () != 0)
		return -1;
	return m_alloc_safe (1, sizeof (struct chunk), (uint8_t)BM_FREE);
}

void bm_destroy (int bs)
{
	if (bs > 0)
		m_free (bs);
}

/* ---- helpers ------------------------------------------------------------ */

static inline uint32_t word_of (bm_bit_t n) { return n >> 6; }

static size_t chunk_len (const struct chunk *c)
{
	return CH_INLINE (c) ? 1 : m_len (CH_HANDLE (c));
}

static uint64_t chunk_word (const struct chunk *c, size_t i)
{
	if (CH_INLINE (c))
		return i == 0 ? INLINE_WORD (c) : 0;
	return *(uint64_t *)mls (CH_HANDLE (c), i);
}

/* demote a one-word handle chunk to the inline form (frees the handle) */
static void chunk_to_inline (struct chunk *c)
{
	if (CH_INLINE (c))
		return;
	int h = CH_HANDLE (c);
	if (m_len (h) != 1)
		return;
	uint64_t w = *(uint64_t *)mls (h, 0);
	c->bitpos = (c->bitpos & ~1u) | (uint32_t)((w >> 63) & 1u);
	c->data = inline_data (w);
	m_free (h);
}

/* largest i with real bitpos <= key, or -1. key word-aligned. */
static int chunk_le (int bs, uint32_t key)
{
	size_t lo = 0, hi = m_len (bs);
	int res = -1;
	while (lo < hi) {
		size_t mid = lo + (hi - lo) / 2;
		struct chunk *c = mls_safe (bs, mid);
		if (!c)
			return -1;
		if (CH_BITPOS (c) <= key) {
			res = (int)mid;
			lo = mid + 1;
		} else {
			hi = mid;
		}
	}
	return res;
}

static uint64_t word_get (int bs, uint32_t sw)
{
	int i = chunk_le (bs, sw << 6);
	if (i < 0)
		return 0;
	struct chunk *c = mls_safe (bs, (size_t)i);
	if (!c)
		return 0;
	uint32_t start = CH_WORDIDX (c);
	size_t off = sw - start;
	if (off >= chunk_len (c))
		return 0;
	return chunk_word (c, off);
}

static int chunk_delete (int bs, int i, struct chunk *c)
{
	int h = CH_INLINE (c) ? 0 : CH_HANDLE (c);
	if (m_del_safe (bs, (size_t)i) != 0)
		return -1;
	if (h)
		m_free (h);
	return 0;
}

static int maybe_split (int bs, int ci)
{
	for (;;) {
		struct chunk *c = mls_safe (bs, (size_t)ci);
		if (!c)
			return -1;
		if (CH_INLINE (c))
			return 0;
		size_t len = m_len (CH_HANDLE (c));
		if (len <= BM_MAX_WORDS)
			return 0;
		uint32_t start = CH_WORDIDX (c);
		bm_bit_t mid =
			(bm_bit_t)(((uint64_t)start + len / 2) * BM_WORD);
		if (bm_split (bs, mid) != 0)
			return -1;
	}
}

/* single write primitive behind set/clr/set-ops */
static int word_write (int bs, uint32_t sw, uint64_t val)
{
	bm_bit_t key = (bm_bit_t)(sw << 6);
	int i = chunk_le (bs, key);
	if (i >= 0) {
		struct chunk *c = mls_safe (bs, (size_t)i);
		if (!c)
			return -1;
		uint32_t start = CH_WORDIDX (c);
		size_t len = chunk_len (c);
		if (sw < start + len) {
			if (CH_INLINE (c)) {
				if (val == 0)
					return chunk_delete (bs, i, c);
				c->bitpos = (c->bitpos & ~1u) |
					    (uint32_t)((val >> 63) & 1u);
				c->data = inline_data (val);
				return 0;
			}
			int h = CH_HANDLE (c);
			*(uint64_t *)mls (h, sw - start) = val;
			if (val == 0) {
				while (len > 0 &&
				       *(uint64_t *)mls (h, len - 1) == 0)
					len--;
				if (len == 0)
					return chunk_delete (bs, i, c);
				if (m_setlen_safe (h, len) != 0)
					return -1;
				if (len == 1)
					chunk_to_inline (c);
			}
			return 0;
		}
		if (val == 0)
			return 0; /* implicit zero */
		size_t end = start + len;
		if (sw - end <= BM_GAP_JOIN) {
			size_t newlen = sw - start + 1;
			if (CH_INLINE (c)) {
				/* promote inline -> handle */
				int h = m_alloc_safe (newlen, sizeof (uint64_t),
						      MFREE);
				if (h < 0)
					return -1;
				if (m_setlen_safe (h, newlen) != 0) {
					m_free (h);
					return -1;
				}
				*(uint64_t *)mls (h, 0) = INLINE_WORD (c);
				for (size_t g = 1; g < sw - start; g++)
					*(uint64_t *)mls (h, g) = 0;
				*(uint64_t *)mls (h, sw - start) = val;
				c->bitpos = c->bitpos & ~1u;
				c->data = h;
			} else {
				int h = CH_HANDLE (c);
				if (m_setlen_safe (h, newlen) != 0)
					return -1;
				for (size_t g = len; g < newlen - 1; g++) {
					uint64_t z = 0;
					if (m_write_safe (h, g, &z, 1) != 0)
						return -1;
				}
				*(uint64_t *)mls (h, sw - start) = val;
			}
			return maybe_split (bs, i);
		}
	}
	if (val == 0)
		return 0;
	/* new single-word chunk: store it inline, no allocation */
	if (!m_ins_safe (bs, (size_t)i + 1, 1))
		return -1;
	struct chunk *nc = mls (bs, (size_t)i + 1);
	nc->bitpos = inline_bitpos (sw, val);
	nc->data = inline_data (val);
	return 0;
}

/* ---- single bit --------------------------------------------------------- */

int bm_set (int bs, bm_bit_t n)
{
	if (bs <= 0) {
		mls_errno = MLS_EINVAL;
		return -1;
	}
	uint32_t sw = word_of (n);
	uint64_t cur = word_get (bs, sw);
	return word_write (bs, sw, cur | (1ULL << (n & 63)));
}

int bm_clr (int bs, bm_bit_t n)
{
	if (bs <= 0) {
		mls_errno = MLS_EINVAL;
		return -1;
	}
	uint32_t sw = word_of (n);
	uint64_t cur = word_get (bs, sw);
	uint64_t nv = cur & ~(1ULL << (n & 63));
	if (nv == cur)
		return 0;
	return word_write (bs, sw, nv);
}

int bm_test (int bs, bm_bit_t n)
{
	if (bs <= 0)
		return 0;
	return (int)((word_get (bs, word_of (n)) >> (n & 63)) & 1ULL);
}

/* ---- per-chunk scans ---------------------------------------------------- */

int64_t bm_first1 (int bs, int ci)
{
	if (bs <= 0 || ci < 0)
		return BM_NONE;
	struct chunk *c = mls_safe (bs, (size_t)ci);
	if (!c)
		return BM_NONE;
	size_t len = chunk_len (c);
	for (size_t w = 0; w < len; w++) {
		uint64_t v = chunk_word (c, w);
		if (v)
			return (int64_t)CH_BITPOS (c) + (int64_t)w * 64 +
			       __builtin_ctzll (v);
	}
	return BM_NONE;
}

int64_t bm_last1 (int bs, int ci)
{
	if (bs <= 0 || ci < 0)
		return BM_NONE;
	struct chunk *c = mls_safe (bs, (size_t)ci);
	if (!c)
		return BM_NONE;
	size_t len = chunk_len (c);
	for (size_t w = len; w > 0; w--) {
		uint64_t v = chunk_word (c, w - 1);
		if (v)
			return (int64_t)CH_BITPOS (c) + (int64_t)(w - 1) * 64 +
			       63 - __builtin_clzll (v);
	}
	return BM_NONE;
}

int64_t bm_first0 (int bs, int ci)
{
	if (bs <= 0 || ci < 0)
		return BM_NONE;
	struct chunk *c = mls_safe (bs, (size_t)ci);
	if (!c)
		return BM_NONE;
	size_t len = chunk_len (c);
	for (size_t w = 0; w < len; w++) {
		uint64_t v = chunk_word (c, w);
		if (v != ~0ULL)
			return (int64_t)CH_BITPOS (c) + (int64_t)w * 64 +
			       __builtin_ctzll (~v);
	}
	return BM_NONE;
}

int64_t bm_last0 (int bs, int ci)
{
	if (bs <= 0 || ci < 0)
		return BM_NONE;
	struct chunk *c = mls_safe (bs, (size_t)ci);
	if (!c)
		return BM_NONE;
	size_t len = chunk_len (c);
	for (size_t w = len; w > 0; w--) {
		uint64_t v = chunk_word (c, w - 1);
		if (v != ~0ULL)
			return (int64_t)CH_BITPOS (c) + (int64_t)(w - 1) * 64 +
			       63 - __builtin_clzll (~v);
	}
	return BM_NONE;
}

/* ---- whole-bitmap iterators --------------------------------------------- */

int64_t bm_next1 (int bs, bm_bit_t from)
{
	if (bs <= 0)
		return BM_NONE;
	uint32_t sw = word_of (from);
	int i = chunk_le (bs, sw << 6);
	if (i >= 0) {
		struct chunk *c = mls_safe (bs, (size_t)i);
		if (!c)
			return BM_NONE;
		uint32_t start = CH_WORDIDX (c);
		size_t len = chunk_len (c);
		if (sw < start + len) {
			for (size_t w = sw - start; w < len; w++) {
				uint64_t v = chunk_word (c, w);
				if (w == (size_t)(sw - start) && (from & 63))
					v &= ~0ULL << (from & 63);
				if (v)
					return (int64_t)CH_BITPOS (c) +
					       (int64_t)w * 64 +
					       __builtin_ctzll (v);
			}
			i++;
		} else {
			i++;
		}
	} else {
		i = 0;
	}
	for (;; i++) {
		struct chunk *c = mls_safe (bs, (size_t)i);
		if (!c)
			break;
		size_t len = chunk_len (c);
		for (size_t w = 0; w < len; w++) {
			uint64_t v = chunk_word (c, w);
			if (v)
				return (int64_t)CH_BITPOS (c) +
				       (int64_t)w * 64 + __builtin_ctzll (v);
		}
	}
	return BM_NONE;
}

int64_t bm_next0 (int bs, bm_bit_t from)
{
	if (bs <= 0)
		return BM_NONE;
	uint32_t sw = word_of (from);
	int i = chunk_le (bs, sw << 6);
	if (i < 0)
		return from;
	struct chunk *c = mls_safe (bs, (size_t)i);
	if (!c)
		return BM_NONE;
	if (sw >= CH_WORDIDX (c) + chunk_len (c))
		return from; /* gap */
	int mask_first = (from & 63) != 0;
	size_t w0 = sw - CH_WORDIDX (c);
	for (;; i++) {
		c = mls_safe (bs, (size_t)i);
		if (!c)
			break;
		size_t len = chunk_len (c);
		for (size_t w = w0; w < len; w++) {
			uint64_t v = chunk_word (c, w);
			if (mask_first) {
				v |= (1ULL << (from & 63)) - 1;
				mask_first = 0;
			}
			if (v != ~0ULL)
				return (int64_t)CH_BITPOS (c) +
				       (int64_t)w * 64 + __builtin_ctzll (~v);
		}
		w0 = 0;
		int j = i + 1;
		struct chunk *nxt = mls_safe (bs, (size_t)j);
		uint64_t endbit = (uint64_t)CH_BITPOS (c) + (uint64_t)len * 64;
		if (!nxt)
			return endbit <= (uint64_t)UINT32_MAX ? (int64_t)endbit
							      : BM_NONE;
		if ((uint64_t)CH_BITPOS (nxt) > endbit)
			return (int64_t)endbit;
	}
	return BM_NONE;
}

int64_t bm_prev1 (int bs, bm_bit_t from)
{
	if (bs <= 0)
		return BM_NONE;
	uint32_t sw = word_of (from);
	int i = chunk_le (bs, sw << 6);
	if (i < 0)
		return BM_NONE;
	int mask_first = 1;
	int have_w = 0;
	size_t w = 0;
	for (; i >= 0; i--) {
		struct chunk *c = mls_safe (bs, (size_t)i);
		if (!c)
			return BM_NONE;
		uint32_t start = CH_WORDIDX (c);
		size_t len = chunk_len (c);
		if (!have_w) {
			if (sw < start + len) {
				w = sw - start + 1;
				mask_first = (from & 63) != 63;
			} else {
				w = len;
				mask_first = 0;
			}
			have_w = 1;
		} else {
			w = len;
		}
		while (w > 0) {
			w--;
			uint64_t v = chunk_word (c, w);
			if (mask_first) {
				v &= (1ULL << ((from & 63) + 1)) - 1;
				mask_first = 0;
			}
			if (v)
				return (int64_t)CH_BITPOS (c) +
				       (int64_t)w * 64 + 63 -
				       __builtin_clzll (v);
		}
	}
	return BM_NONE;
}

int64_t bm_prev0 (int bs, bm_bit_t from)
{
	if (bs <= 0)
		return BM_NONE;
	uint32_t sw = word_of (from);
	int i = chunk_le (bs, sw << 6);
	if (i < 0)
		return from;
	struct chunk *c = mls_safe (bs, (size_t)i);
	if (!c)
		return BM_NONE;
	if (sw >= CH_WORDIDX (c) + chunk_len (c))
		return from; /* gap */
	int mask_first = (from & 63) != 63;
	size_t w = sw - CH_WORDIDX (c) + 1;
	for (; i >= 0; i--) {
		c = mls_safe (bs, (size_t)i);
		if (!c)
			break;
		while (w > 0) {
			w--;
			uint64_t v = chunk_word (c, w);
			if (mask_first) {
				unsigned b = from & 63;
				if (b != 63)
					v |= ~((1ULL << (b + 1)) - 1);
				mask_first = 0;
			}
			if (v != ~0ULL)
				return (int64_t)CH_BITPOS (c) +
				       (int64_t)w * 64 + 63 -
				       __builtin_clzll (~v);
		}
		int j = i - 1;
		if (j < 0)
			break;
		struct chunk *prv = mls_safe (bs, (size_t)j);
		if (!prv)
			break;
		uint64_t prv_end = (uint64_t)CH_BITPOS (prv) +
				   (uint64_t)chunk_len (prv) * 64;
		if (prv_end < (uint64_t)CH_BITPOS (c))
			return (int64_t)CH_BITPOS (c) - 1;
		w = chunk_len (prv);
	}
	/* exhausted down to chunk 0: the implicit gap before it (if any)
	   is still full of zeros */
	struct chunk *c0 = mls_safe (bs, 0);
	if (c0 && (CH_BITPOS (c0) > 0))
		return (int64_t)CH_BITPOS (c0) - 1;
	return BM_NONE;
}

/* ---- bitset ops --------------------------------------------------------- */

enum { OP_AND, OP_OR, OP_XOR };

static int bm_op (int bs2, int bs0, int bs1, bm_bit_t start, bm_bit_t count,
		  int op)
{
	if (bs0 <= 0 || bs1 <= 0) {
		mls_errno = MLS_EINVAL;
		return -1;
	}
	int created = 0;
	if (bs2 == 0) {
		bs2 = bm_create ();
		if (bs2 < 0)
			return -1;
		created = 1;
	} else if (bs2 < 0) {
		mls_errno = MLS_EINVAL;
		return -1;
	}
	if (count == 0)
		return bs2;
	uint64_t end = (uint64_t)start + count - 1;
	if (end > (uint64_t)UINT32_MAX)
		end = UINT32_MAX; /* out-of-domain tail: clamp, else sw<<6 wraps
				   */
	uint64_t sw0 = start >> 6, sw1 = end >> 6;
	for (uint64_t sw = sw0; sw <= sw1; sw++) {
		uint64_t a = word_get (bs0, (uint32_t)sw);
		uint64_t b = word_get (bs1, (uint32_t)sw);
		uint64_t r = op == OP_AND  ? (a & b)
			     : op == OP_OR ? (a | b)
					   : (a ^ b);
		if (sw == sw0 && (start & 63))
			r &= ~0ULL << (start & 63);
		if (sw == sw1 && (end & 63) != 63)
			r &= (1ULL << ((end & 63) + 1)) - 1;
		if (word_write (bs2, (uint32_t)sw, r) != 0) {
			if (created)
				m_free (bs2);
			return -1;
		}
	}
	return bs2;
}

int bm_and (int bs2, int bs0, int bs1, bm_bit_t start, bm_bit_t count)
{
	return bm_op (bs2, bs0, bs1, start, count, OP_AND);
}
int bm_or (int bs2, int bs0, int bs1, bm_bit_t start, bm_bit_t count)
{
	return bm_op (bs2, bs0, bs1, start, count, OP_OR);
}
int bm_xor (int bs2, int bs0, int bs1, bm_bit_t start, bm_bit_t count)
{
	return bm_op (bs2, bs0, bs1, start, count, OP_XOR);
}

/* ---- housekeeping ------------------------------------------------------- */

int bm_split (int bs, bm_bit_t n)
{
	if (bs <= 0) {
		mls_errno = MLS_EINVAL;
		return -1;
	}
	uint32_t key = word_of (n);
	int i = chunk_le (bs, key << 6);
	if (i < 0)
		return 0;
	struct chunk *c = mls_safe (bs, (size_t)i);
	if (!c)
		return -1;
	if (CH_INLINE (c))
		return 0;
	int h0 = CH_HANDLE (c);
	uint32_t start = CH_WORDIDX (c);
	size_t len = m_len (h0);
	if (key < start || key >= start + len)
		return 0;
	size_t off = key - start;
	if (off == 0 || off >= len)
		return 0;
	int h2 = m_alloc_safe (len - off, sizeof (uint64_t), MFREE);
	if (h2 < 0)
		return -1;
	if (m_setlen_safe (h2, len - off) != 0 ||
	    m_write_safe (h2, 0, mls (h0, off), len - off) != 0) {
		m_free (h2);
		return -1;
	}
	if (m_setlen_safe (h0, off) != 0) {
		m_free (h2);
		return -1;
	}
	uint32_t bpos = CH_BITPOS (c) + (uint32_t)off * BM_WORD;
	if (!m_ins_safe (bs, (size_t)i + 1, 1)) {
		/* restore the truncated first half: OOM must not lose data */
		if (m_setlen_safe (h0, len) == 0)
			m_write_safe (h0, off, mls (h2, 0), len - off);
		m_free (h2);
		return -1;
	}
	/* re-fetch: m_ins may have realloc'd the list backing array */
	c = mls (bs, (size_t)i);
	struct chunk *p = mls (bs, (size_t)i + 1);
	chunk_to_inline (c);
	p->bitpos = bpos;
	p->data = h2;
	chunk_to_inline (p);
	return 0;
}

int bm_merge (int bs, int ci)
{
	if (bs <= 0 || ci < 0) {
		mls_errno = MLS_EINVAL;
		return -1;
	}
	if ((size_t)ci + 1 >= m_len (bs)) {
		mls_errno = MLS_EBOUNDS;
		return -1;
	}
	struct chunk *c0 = mls_safe (bs, (size_t)ci);
	struct chunk *c1 = mls_safe (bs, (size_t)ci + 1);
	if (!c0 || !c1)
		return -1;
	uint64_t s0 = CH_WORDIDX (c0), l0 = chunk_len (c0);
	uint64_t s1 = CH_WORDIDX (c1), l1 = chunk_len (c1);
	uint64_t end0 = s0 + l0;
	if (end0 > s1) {
		mls_errno = MLS_EINVAL;
		return -1;
	}
	uint64_t gap = s1 - end0;
	uint64_t newlen = l0 + gap + l1;
	int h = m_alloc_safe ((size_t)newlen, sizeof (uint64_t), MFREE);
	if (h < 0)
		return -1;
	if (m_setlen_safe (h, (size_t)newlen) != 0) {
		m_free (h);
		return -1;
	}
	for (size_t w = 0; w < l0; w++)
		*(uint64_t *)mls (h, w) = chunk_word (c0, w);
	for (uint64_t g = 0; g < gap; g++)
		*(uint64_t *)mls (h, (size_t)(l0 + g)) = 0;
	for (size_t w = 0; w < l1; w++)
		*(uint64_t *)mls (h, (size_t)(l0 + gap + w)) =
			chunk_word (c1, w);
	int h0 = CH_INLINE (c0) ? 0 : CH_HANDLE (c0);
	int h1 = CH_INLINE (c1) ? 0 : CH_HANDLE (c1);
	/* delete before re-pointing c0: a failed delete must leave the list
	   (and c0's data handle) untouched */
	if (m_del_safe (bs, (size_t)ci + 1) != 0) {
		m_free (h);
		return -1;
	}
	c0 = mls (bs, (size_t)ci); /* re-fetch: m_del may have realloc'd it */
	c0->bitpos = (uint32_t)(s0 << 6);
	c0->data = h;
	if (h0)
		m_free (h0);
	if (h1)
		m_free (h1);
	return 0;
}

int bm_compact (int bs)
{
	if (bs <= 0) {
		mls_errno = MLS_EINVAL;
		return -1;
	}
	int changed = 1;
	size_t guard = m_len (bs) + 1;
	while (changed && guard-- > 0) {
		changed = 0;
		for (int i = 0; i < (int)m_len (bs);) {
			struct chunk *c = mls_safe (bs, (size_t)i);
			if (!c)
				return -1;
			if (CH_INLINE (c)) {
				i++; /* inline words are never zero */
				continue;
			}
			int h = CH_HANDLE (c);
			size_t len = m_len (h);
			int allzero = 1;
			for (size_t w = 0; w < len; w++)
				if (*(uint64_t *)mls (h, w) != 0) {
					allzero = 0;
					break;
				}
			if (allzero) {
				if (chunk_delete (bs, i, c) != 0)
					return -1;
				changed = 1;
			} else {
				i++;
			}
		}
		for (int i = 0; i + 1 < (int)m_len (bs);) {
			struct chunk *c0 = mls_safe (bs, (size_t)i);
			struct chunk *c1 = mls_safe (bs, (size_t)i + 1);
			if (!c0 || !c1)
				return -1;
			uint64_t l0 = chunk_len (c0);
			uint64_t l1 = chunk_len (c1);
			uint64_t end0 = (uint64_t)CH_WORDIDX (c0) + l0;
			int64_t gap = (int64_t)CH_WORDIDX (c1) - (int64_t)end0;
			/* merging inline chunks would allocate a handle and
			   cost more than keeping them; only merge handles */
			if (!CH_INLINE (c0) && !CH_INLINE (c1) && gap >= 0 &&
			    (uint64_t)gap <= BM_GAP_JOIN &&
			    l0 + (uint64_t)gap + l1 <= BM_MAX_WORDS) {
				if (bm_merge (bs, i) != 0)
					return -1;
				changed = 1;
			} else {
				i++;
			}
		}
		for (int i = 0; i < (int)m_len (bs); i++) {
			struct chunk *c = mls_safe (bs, (size_t)i);
			if (!c)
				return -1;
			if (!CH_INLINE (c) &&
			    m_len (CH_HANDLE (c)) > BM_MAX_WORDS) {
				if (maybe_split (bs, i) != 0)
					return -1;
				changed = 1;
			}
		}
	}
	return 0;
}

/* ---- introspection ------------------------------------------------------ */

int bm_chunks (int bs) { return bs > 0 ? (int)m_len (bs) : -1; }

int64_t bm_chunk_bitpos (int bs, int ci)
{
	if (bs <= 0 || ci < 0)
		return BM_NONE;
	struct chunk *c = mls_safe (bs, (size_t)ci);
	return c ? (int64_t)CH_BITPOS (c) : BM_NONE;
}

int64_t bm_chunk_words (int bs, int ci)
{
	if (bs <= 0 || ci < 0)
		return BM_NONE;
	struct chunk *c = mls_safe (bs, (size_t)ci);
	return c ? (int64_t)chunk_len (c) : BM_NONE;
}
