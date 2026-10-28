#ifndef BPTREE_H
#define BPTREE_H

#include <stddef.h>
#include <stdint.h>

/* In-memory B+ tree: opaque fixed-size keys (compared by callback) map to an
   int64 value. Leaves are linked for ordered range scans (next/prev).
   Keys are copied into the tree; the caller's buffer is only read. */
typedef struct bptree bptree_t;
typedef struct bpt_iter bpt_iter_t;

/* <0, 0, >0 like strcmp. ctx is the tree's context pointer. */
typedef int (*bpt_cmp_t) (const void *a, const void *b, void *ctx);

bptree_t *bpt_new (size_t key_size, bpt_cmp_t cmp, void *ctx);
void bpt_free (bptree_t *t);

/* insert-only: 0 inserted, 1 duplicate key, -1 error */
int bpt_insert (bptree_t *t, const void *key, int64_t value);
/* insert or replace: 0 ok, -1 error */
int bpt_put (bptree_t *t, const void *key, int64_t value);
/* 1 found, 0 missing */
int bpt_get (const bptree_t *t, const void *key, int64_t *value);
/* 1 deleted, 0 not found */
int bpt_del (bptree_t *t, const void *key);
int64_t bpt_count (const bptree_t *t);

bpt_iter_t *bpt_iter_new (const bptree_t *t);
void bpt_iter_free (bpt_iter_t *it);
int bpt_iter_first (bpt_iter_t *it); /* 1 valid */
int bpt_iter_last (bpt_iter_t *it);
int bpt_iter_seek (bpt_iter_t *it, const void *key); /* first >= key */
int bpt_iter_next (bpt_iter_t *it);		     /* 1 valid */
int bpt_iter_prev (bpt_iter_t *it);
const void *bpt_iter_key (const bpt_iter_t *it); /* NULL if invalid */
int64_t bpt_iter_value (const bpt_iter_t *it);

#endif
