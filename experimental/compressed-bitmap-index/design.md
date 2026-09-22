# Chunked bitmap index — design

Companion to `storage-layout.txt`. That file gives the data shape and one
algorithm sketch; this file pins down the semantics it leaves open so the
thing is actually implementable and testable. Where this document disagrees
with `storage-layout.txt`, this document wins (the deltas are listed at the
end).

## 1. Data model

```
struct chunk {
    uint32_t bitpos;   /* (word_index << 6) | bit63_of_inline_word */
    int64_t  data;     /* >= 0: MLS handle to uint64_t words; < 0: inline word */
};
typedef uint32_t bm_bit_t;   /* bit positions 0 .. 2^32-1 */
#define BM_NONE ((int64_t)-1)
```

A chunk is either a **handle** to a multi-word `uint64_t` array, or **inline**:
a single word stored directly in the chunk, tagged by the sign bit of `data`.
Handles are always positive, so a negative `data` is a safe tag and no
allocation happens for one-word chunks. Since chunk starts are 64-bit aligned,
the low bit of `bitpos` is free and carries bit 63 of the inline word, so the
full 64 bits survive (`bitpos >> 6` is the word index regardless of the tag).
Inline chunks are never zero: clearing their last bit deletes the chunk.

A bitmap handle `bs` is an MLS list of `struct chunk`, sorted ascending by
`bitpos`. It is created with element width `sizeof(struct chunk)`.

A chunk covers the **word range** `[bitpos/64, bitpos/64 + m_len(data))`,
i.e. the bit range `[bitpos, bitpos + 64*m_len(data))`. Every bit in that
range is materialised (dense words). Every bit outside all chunk ranges is
an **implicit zero**. That is the entire compression: runs of zeros are not
stored.

### Invariants (checked at every public API boundary)

- **I1** `bitpos % 64 == 0`. Chunks are word-aligned.
- **I2** strictly ascending and non-overlapping:
  `end_word(i) <= start_word(i+1)` where `end_word = bitpos/64 + len`.
- **I3** `m_len(data) >= 1`.
- **I4** (soft) `m_len(data) <= BM_MAX_WORDS`, and no chunk is all-zero.
  I4 may be violated transiently by `bm_set`/`bm_clr`; `bm_compact` restores
  it. Queries are written so an all-zero or oversized chunk is still correct.

Word alignment (I1) costs at most 63 materialised zero bits at each chunk
edge. It is what makes insert/extend/merge trivial: no two chunks can ever
share a word, so a new chunk at word `w` can never collide with a neighbour.
Without it the "insert new chunk" path needs backward extension of the next
chunk. This is the deliberate simplification.

## 2. Types, constants, helpers

```
#define BM_WORD          64u
#define BM_GAP_JOIN      1u     /* words of implicit zeros worth materialising */
#define BM_MAX_WORDS     1024u  /* soft cap before a chunk is split */
```

- `BM_GAP_JOIN`: when setting a bit in word `w` and the nearest predecessor
  chunk ends at word `e`, extend the predecessor iff `w - e <= BM_GAP_JOIN`
  (this includes `w == e`, the contiguous case). Otherwise start a new chunk.
  One word of gap fills; two or more fragments. Tune here, nowhere else.
- `BM_MAX_WORDS`: bounds per-op and per-chunk cost. On overflow the chunk is
  split in half (`bm_split` at the middle word).

Internal helpers (static):

- `uint32_t word_of(bm_bit_t n) { return n >> 6; }`
- `int chunk_le(int bs, uint32_t key)` — binary search for the largest `i`
  with `bs[i].bitpos <= key`; `key` must be word-aligned; `-1` if none.
  `m_bsearch` is exact-match only and cannot do a predecessor search, so this
  is hand-written over `mls_safe`/`m_len`.
- `uint64_t word_get(int bs, uint32_t sw)` — returns the word at global word
  index `sw`, or `0` if implicit. Value, never a pointer (set ops may alias
  inputs and outputs).
- `int word_write(int bs, uint32_t sw, uint64_t val)` — the single write
  primitive behind `bm_set`, `bm_clr` and the set ops. Nonzero `val`: ensure a
  chunk covers `sw` (extend predecessor if within `BM_GAP_JOIN`, else insert a
  new one), then store `val`. Zero `val`: if a chunk covers `sw`, clear the
  word and trim trailing zero words; else no-op.
- `int maybe_split(int bs, int ci)` — if chunk `ci` exceeds `BM_MAX_WORDS`,
  `bm_split` at its middle word.

## 3. Ownership / lifecycle

The sketch's `m_create(1, sizeof(struct chunk))` is wrong: `m_create` uses the
default `MFREE` handler, so the nested `data` handles leak, and the predefined
`MFREE_EACH` handler is also wrong because `free_list_wrap` frees the *first
int of each element* — that would free `bitpos` as a handle.

Use a dedicated handler that skips inline chunks (`m_free` treats handle 0 as a
no-op):

```
static void bm_free(int bs) {
    int i; struct chunk *c;
    m_foreach(bs, i, c)
        if (!CH_INLINE(c))            /* inline data is a word, not a handle */
            m_free(CH_HANDLE(c));
}
static int BM_FREE = -1;
int bm_init(void) {
    if (BM_FREE < 0)
        BM_FREE = m_reg_freefn(bm_free);   /* idempotent per call site */
    return BM_FREE < 0 ? -1 : 0;
}
int bm_create(void) {
    if (bm_init() != 0) return -1;
    return m_alloc_safe(1, sizeof(struct chunk), BM_FREE);
}
void bm_destroy(int bs) { m_free(bs); }
```

`bm_init()` must be called once before any other `bm_*` call (it is also safe
to call repeatedly; guard with a `pthread_once` if `bm_init` can race).
`bm_free` does not need to check `m_is_freed` 

## 4. Single-bit operations

`bit_set(bs, n)` / `word_write(bs, word_of(n), 1ULL << (n & 63))`:

1. `key = n & ~63`, `sw = key >> 6`.
2. `i = chunk_le(bs, key)`.
3. If `i >= 0` and `sw < start_word(i) + len(i)`: OR the bit in place.
4. Else if `i >= 0` and `sw - end_word(i) <= BM_GAP_JOIN`: `m_setlen_safe`
   the data handle to `sw - start_word(i) + 1` (new words are zeroed by
   `lst_resize_safe`), OR the bit, then `maybe_split`.
5. Else: allocate a 1-word chunk `{bitpos = key, data = h}`, `m_ins_safe(bs,
   i + 1, 1)`, write the struct into the new slot. Free `h` and return -1 if
   either step fails.

Note the sketch's `offs > m_len` must be `>=`; `word_write` handles it by the
extend branch, so the off-by-one disappears.

`bit_clr(bs, n)`:

1. Locate chunk as above; if none or `sw` outside its range, return 0
   (implicit zero — nothing to do).
2. Clear the bit. If the word is now zero, trim trailing zero words with
   `m_setlen_safe`. If the chunk becomes empty, save `h = data`, then
   `m_del_safe(bs, i)`, then `m_free(h)` — delete first so a failed delete
   cannot leave a dangling `data` handle behind.
   Only trailing trim (O(1)); leading zero words are left for `bm_compact`.
   This keeps `bit_clr` cheap while keeping the common edge tight.

`bit_test(bs, n)`: locate chunk; out of range → 0; else `(word >> (n&63)) & 1`.

## 5. Scanning operations

The sketch's `bit_first0/1(bs, chunk_index)` and `bit_last0/1` are **per
chunk**: `chunk_index` is a plain input in `[0, m_len(bs))`, the result is an
absolute bit position or `BM_NONE`. They only see the chunk's materialised
range; implicit zeros *between* chunks are not visible to them.

```
int64_t bm_first1(int bs, int ci);   /* first set bit in chunk ci */
int64_t bm_last1 (int bs, int ci);   /* last set bit in chunk ci */
int64_t bm_first0(int bs, int ci);   /* first zero bit in chunk ci, BM_NONE if full */
int64_t bm_last0 (int bs, int ci);   /* last zero bit in chunk ci, BM_NONE if full */
```

Word scans use `__builtin_ctzll` / `__builtin_clzll`; `bm_first0` looks for
the first word `!= ~0ULL`, `bm_last0` for the last one.

Because per-chunk primitives cannot enumerate a bitmap on their own, the
design adds four whole-bitmap iterators (thin loops over `chunk_le` + the
per-chunk primitives, skipping implicit gaps):

```
int64_t bm_next1(int bs, bm_bit_t from);  /* smallest set bit >= from, or BM_NONE */
int64_t bm_next0(int bs, bm_bit_t from);  /* smallest clear bit >= from */
int64_t bm_prev1(int bs, bm_bit_t from);  /* largest set bit <= from */
int64_t bm_prev0(int bs, bm_bit_t from);  /* largest clear bit <= from */
```

`bm_next0`/`bm_prev0` treat a `from` that lands in an implicit gap as an
immediate hit (the gap is zero). `bm_next1`/`bm_prev1` skip gaps.

## 6. Bitset operations

```
/* returns the resulting bs2 handle (>= 1), or -1 on error */
int bm_and(int bs2, int bs0, int bs1, bm_bit_t start, bm_bit_t count);
int bm_or (int bs2, int bs0, int bs1, bm_bit_t start, bm_bit_t count);
int bm_xor(int bs2, int bs0, int bs1, bm_bit_t start, bm_bit_t count);
```

Semantics:

- `bs0` and `bs1` must be valid handles (non-zero); a zero is an error.
- `bs2` may be `0`, in which case a new bitmap is created, used as the
  destination, and returned. This mirrors `m_slice()`. If any step fails after
  the creation, the new handle is freed before returning `-1`.
- Only the bit range `[start, start+count)` of `bs2` is modified; bits outside
  are left untouched (for a newly created `bs2` they read as implicit zeros).
- For each bit in the range: `bs2 = bs0 OP bs1`, where absent bits read as 0.
- The result range of `bs2` is (re)chunked by `word_write`, i.e. it follows the
  same `BM_GAP_JOIN`/`BM_MAX_WORDS` policy; no attempt is made to preserve the
  input chunk layout.
- Aliasing (`bs2 == bs0` or `bs2 == bs1`) is allowed. The loop reads `a` and
  `b` for word `sw` *before* writing `sw`, and only ever touches `sw`, so
  already-processed words are never read again.
- `start` need not be word-aligned. First and last words are masked:
  `first_mask = ~0ULL << (start & 63)`,
  `last_mask = (end & 63) == 63 ? ~0ULL : ((1ULL << ((end & 63) + 1)) - 1)`.

Implementation is a word loop from `word_of(start)` to `word_of(start +
count - 1)`; use `uint64_t` for the end computation so `start + count` cannot
wrap.

## 7. Housekeeping

```
int bm_split(int bs, bm_bit_t n);   /* split the chunk containing n */
int bm_merge(int bs, int ci);       /* merge chunk ci with ci+1 */
int bm_compact(int bs);             /* drop empty chunks, join small gaps, split big */
```

- `bm_split(bs, n)`: find the chunk containing `n` (word `key`); let
  `off = word_of(n) - start_word(i)`. If no chunk, or `off == 0`, or
  `off >= len`, no-op. Otherwise move words `[off, len)` into a new chunk
  `{bitpos = bitpos(i) + off*64, data = h2}`, set `len(i) = off`, and
  `m_ins_safe(bs, i + 1, 1)`. Both halves non-empty.
- `bm_merge(bs, ci)`: precondition `0 <= ci < m_len(bs) - 1`. Let `g` be the
  gap in words between the two chunks (`>= 0`). `m_setlen_safe` chunk `ci` to
  `len0 + g + len1` (gap zeroed), `m_write_safe` chunk `ci+1`'s words at
  offset `len0 + g`, `m_free` its data handle, `m_del_safe(bs, ci + 1)`.
  Unconditional — it will materialise a large gap if asked.
- `bm_compact(bs)`: repeat until no change (bounded by `m_len(bs) + 1`
  passes): delete all-zero chunks, merge adjacent pairs whose gap
  `<= BM_GAP_JOIN`, split chunks `> BM_MAX_WORDS`. This is the only operation
  that guarantees I4.

## 8. Errors and threading

- All public functions return `0`/`-1` (`BM_NONE` for queries) and set
  `mls_errno` on recoverable errors (bounds, OOM, overflow). Invalid/freed
  handles are programmer errors and abort via the MLS `_safe` machinery,
  consistent with the rest of the library.
- Exception: `bm_and`/`bm_or`/`bm_xor` return the resulting `bs2` handle
  (>= 1) on success and `-1` on error, because `bs2 == 0` means "create".
- `bs` is **not** safe for concurrent mutation. MLS locks individual handles
  per call, but one `bm_set` is several MLS calls across `bs` and `data`;
  those are not atomic. Callers serialise writers. Concurrent readers with no
  writer are fine. `ponytail:` a global bitmap lock would fix this; add it
  only when a caller actually needs concurrent writes.

## 9. Deltas from `storage-layout.txt`

| Sketch | Design |
|---|---|
| `int bitpos` | `uint32_t bitpos`, word-aligned (I1) |
| `m_create(1, sizeof(struct chunk))` | `m_alloc(1, sizeof, BM_FREE)` + `bm_init()` (nested handles must be freed) |
| `offs > m_len` | handled by extend branch; condition is effectively `>=` |
| `d \|= 1 << (...)` | `*d \|= 1ULL << (...)`; `uint64_t` word |
| "resize-or-insert" unspecified | `BM_GAP_JOIN` / `BM_MAX_WORDS` policy in §2 |
| no reclamation | trailing trim in `bit_clr`, `bm_compact` for the rest |
| `bit_first*/last*(bs, chunk_index)` | per-chunk, absolute `int64_t` or `BM_NONE`; plus global `bm_next*/prev*` iterators |
| set-op semantics unspecified | §6: range-only, implicit zeros, aliasing-safe, result rechunked |
| `chunk_merge`/`chunk_split` unspecified | §7 preconditions and postconditions |

## 10. Minimal test (one runnable check)

`test_bm.c`, assert-based, no framework:

- `bm_set`/`bm_test`/`bm_clr` on bits 0, 63, 64, 65 and a bit 1000 words away;
  verify implicit zeros read as 0.
- gap-join: set bits at word `w` and `w+1` → one chunk; at `w` and `w+3` →
  two chunks.
- boundary: `bm_split` then `bm_merge` round-trips a chunk; `bm_compact`
  removes a chunk whose every bit was cleared.
- `bm_next1`/`bm_prev1` across a gap; `bm_next0` starting inside a gap.
- `bm_and/or/xor` over a range with `bs2 == bs0` (aliasing) matches a scalar
  reference.
- `bm_destroy` frees all nested handles: compare `m_count_allocated()` before
  `bm_create` and after `bm_destroy`.
