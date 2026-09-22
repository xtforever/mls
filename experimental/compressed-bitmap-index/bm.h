#ifndef BM_H
#define BM_H

#include <stdint.h>

/* Chunked compressed bitmap index. See design.md.
   Requires m_init() (mls_ext) before use. */

typedef uint32_t bm_bit_t; /* bit positions 0 .. 2^32-1 */
#define BM_NONE ((int64_t)-1)

int bm_init (void);
int bm_create (void);
void bm_destroy (int bs);

int bm_set (int bs, bm_bit_t n);
int bm_clr (int bs, bm_bit_t n);
int bm_test (int bs, bm_bit_t n);

/* per-chunk scans: ci in [0, bm_chunks(bs)) */
int64_t bm_first1 (int bs, int ci);
int64_t bm_last1 (int bs, int ci);
int64_t bm_first0 (int bs, int ci);
int64_t bm_last0 (int bs, int ci);

/* whole-bitmap iterators */
int64_t bm_next1 (int bs, bm_bit_t from);
int64_t bm_next0 (int bs, bm_bit_t from);
int64_t bm_prev1 (int bs, bm_bit_t from);
int64_t bm_prev0 (int bs, bm_bit_t from);

/* return the resulting bs2 handle (>=1) or -1; bs2==0 creates one */
int bm_and (int bs2, int bs0, int bs1, bm_bit_t start, bm_bit_t count);
int bm_or (int bs2, int bs0, int bs1, bm_bit_t start, bm_bit_t count);
int bm_xor (int bs2, int bs0, int bs1, bm_bit_t start, bm_bit_t count);

int bm_split (int bs, bm_bit_t n);
int bm_merge (int bs, int ci);
int bm_compact (int bs);

/* introspection (tests) */
int bm_chunks (int bs);
int64_t bm_chunk_bitpos (int bs, int ci);
int64_t bm_chunk_words (int bs, int ci);

#endif
