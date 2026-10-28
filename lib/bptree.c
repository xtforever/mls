#include "bptree.h"

#include <stdlib.h>
#include <string.h>

/* Max keys per node; internal nodes then have up to BPT_MAX+1 children.
   ponytail: compile-time degree (4) keeps the code small and tests splits/
   merges aggressively. Upgrade: grow degree for fewer I/Os on large indexes. */
#define BPT_MAX 4
#define BPT_MIN 2

typedef struct bpt_node {
	int leaf;
	int n;			/* keys; children = n+1 for internal */
	void *keys;		/* BPT_MAX * key_size */
	int64_t *vals;		/* leaf only */
	struct bpt_node **kids; /* internal only, BPT_MAX+1 */
	struct bpt_node *next;	/* leaf chain */
	struct bpt_node *prev;
} bpt_node;

struct bptree {
	size_t ks;
	bpt_cmp_t cmp;
	void *ctx;
	bpt_node *root;
	int64_t count;
};

struct bpt_iter {
	const bptree_t *t;
	bpt_node *leaf;
	int idx;
};

static char *kptr (const bptree_t *t, bpt_node *n, int i)
{
	return (char *)n->keys + (size_t)i * t->ks;
}

static int cmp_key (const bptree_t *t, const void *a, const void *b)
{
	return t->cmp (a, b, t->ctx);
}

static bpt_node *node_new (const bptree_t *t, int leaf)
{
	bpt_node *n = calloc (1, sizeof *n);
	if (!n)
		return NULL;
	n->leaf = leaf;
	n->keys = malloc (t->ks * BPT_MAX);
	if (!n->keys)
		goto fail;
	if (leaf) {
		n->vals = malloc (sizeof *n->vals * BPT_MAX);
		if (!n->vals)
			goto fail;
	} else {
		n->kids = calloc (BPT_MAX + 1, sizeof *n->kids);
		if (!n->kids)
			goto fail;
	}
	return n;
fail:
	free (n->keys);
	free (n);
	return NULL;
}

static void node_free (bpt_node *n)
{
	if (!n)
		return;
	free (n->keys);
	free (n->vals);
	free (n->kids);
	free (n);
}

static void tree_free (bpt_node *n)
{
	if (!n)
		return;
	if (!n->leaf)
		for (int i = 0; i <= n->n; i++)
			tree_free (n->kids[i]);
	node_free (n);
}

bptree_t *bpt_new (size_t key_size, bpt_cmp_t cmp, void *ctx)
{
	if (key_size == 0 || !cmp)
		return NULL;
	bptree_t *t = calloc (1, sizeof *t);
	if (!t)
		return NULL;
	t->ks = key_size;
	t->cmp = cmp;
	t->ctx = ctx;
	return t;
}

void bpt_free (bptree_t *t)
{
	if (!t)
		return;
	tree_free (t->root);
	free (t);
}

static int leaf_lower_bound (const bptree_t *t, bpt_node *n, const void *key)
{
	int lo = 0, hi = n->n;
	while (lo < hi) {
		int mid = (lo + hi) / 2;
		if (cmp_key (t, kptr (t, n, mid), key) < 0)
			lo = mid + 1;
		else
			hi = mid;
	}
	return lo;
}

static int inner_child (const bptree_t *t, bpt_node *n, const void *key)
{
	int i = 0;
	while (i < n->n && cmp_key (t, key, kptr (t, n, i)) >= 0)
		i++;
	return i;
}

/* Split parent->kids[i] into kids[i] + a new right node, pushing the
   separator up into parent. Parent must have room (n < BPT_MAX). */
static int split_child (const bptree_t *t, bpt_node *p, int i)
{
	bpt_node *c = p->kids[i];
	bpt_node *r = node_new (t, c->leaf);
	if (!r)
		return -1;

	int mid = c->n / 2;
	if (c->leaf) {
		int rn = c->n - mid;
		memcpy (r->keys, kptr (t, c, mid), (size_t)rn * t->ks);
		memcpy (r->vals, c->vals + mid, sizeof *r->vals * (size_t)rn);
		r->n = rn;
		c->n = mid;
		r->next = c->next;
		r->prev = c;
		if (c->next)
			c->next->prev = r;
		c->next = r;
	} else {
		int rn = c->n - mid - 1;
		memcpy (r->keys, kptr (t, c, mid + 1), (size_t)rn * t->ks);
		memcpy (r->kids, c->kids + mid + 1,
			sizeof *r->kids * (size_t)(rn + 1));
		r->n = rn;
		c->n = mid;
	}

	memmove (kptr (t, p, i + 1), kptr (t, p, i),
		 (size_t)(p->n - i) * t->ks);
	memmove (p->kids + i + 2, p->kids + i + 1,
		 sizeof *p->kids * (size_t)(p->n - i));
	p->kids[i + 1] = r;
	if (c->leaf)
		memcpy (kptr (t, p, i), kptr (t, r, 0), t->ks);
	else
		memcpy (kptr (t, p, i), kptr (t, c, mid),
			t->ks); /* promoted, still in slot */
	p->n++;
	return 0;
}

static int do_insert (bptree_t *t, const void *key, int64_t value, int replace,
		      int *dup)
{
	*dup = 0;
	if (!t->root) {
		bpt_node *r = node_new (t, 1);
		if (!r)
			return -1;
		memcpy (r->keys, key, t->ks);
		r->vals[0] = value;
		r->n = 1;
		t->root = r;
		t->count++;
		return 0;
	}

	if (t->root->n == BPT_MAX) {
		bpt_node *nr = node_new (t, 0);
		if (!nr)
			return -1;
		nr->kids[0] = t->root;
		if (split_child (t, nr, 0) != 0) {
			node_free (nr);
			return -1;
		}
		t->root = nr;
	}

	bpt_node *n = t->root;
	for (;;) {
		if (n->leaf) {
			int pos = leaf_lower_bound (t, n, key);
			if (pos < n->n &&
			    cmp_key (t, kptr (t, n, pos), key) == 0) {
				if (replace) {
					n->vals[pos] = value;
					return 0;
				}
				*dup = 1;
				return 0;
			}
			memmove (kptr (t, n, pos + 1), kptr (t, n, pos),
				 (size_t)(n->n - pos) * t->ks);
			memmove (n->vals + pos + 1, n->vals + pos,
				 sizeof *n->vals * (size_t)(n->n - pos));
			memcpy (kptr (t, n, pos), key, t->ks);
			n->vals[pos] = value;
			n->n++;
			t->count++;
			return 0;
		}

		int i = inner_child (t, n, key);
		if (n->kids[i]->n == BPT_MAX) {
			if (split_child (t, n, i) != 0)
				return -1;
			if (cmp_key (t, key, kptr (t, n, i)) >= 0)
				i++;
		}
		n = n->kids[i];
	}
}

int bpt_insert (bptree_t *t, const void *key, int64_t value)
{
	if (!t || !key)
		return -1;
	int dup;
	int rc = do_insert (t, key, value, 0, &dup);
	if (rc != 0)
		return -1;
	return dup ? 1 : 0;
}

int bpt_put (bptree_t *t, const void *key, int64_t value)
{
	if (!t || !key)
		return -1;
	int dup;
	return do_insert (t, key, value, 1, &dup);
}

int bpt_get (const bptree_t *t, const void *key, int64_t *value)
{
	if (!t || !key || !value || !t->root)
		return 0;
	bpt_node *n = t->root;
	while (!n->leaf)
		n = n->kids[inner_child (t, n, key)];
	int pos = leaf_lower_bound (t, n, key);
	if (pos < n->n && cmp_key (t, kptr (t, n, pos), key) == 0) {
		*value = n->vals[pos];
		return 1;
	}
	return 0;
}

/* --- deletion ---------------------------------------------------------- */

static void borrow_left (const bptree_t *t, bpt_node *p, int idx)
{
	bpt_node *c = p->kids[idx], *l = p->kids[idx - 1];
	if (c->leaf) {
		memmove (kptr (t, c, 1), kptr (t, c, 0), (size_t)c->n * t->ks);
		memmove (c->vals + 1, c->vals, sizeof *c->vals * (size_t)c->n);
		memcpy (kptr (t, c, 0), kptr (t, l, l->n - 1), t->ks);
		c->vals[0] = l->vals[l->n - 1];
		c->n++;
		l->n--;
		memcpy (kptr (t, p, idx - 1), kptr (t, c, 0), t->ks);
	} else {
		memmove (kptr (t, c, 1), kptr (t, c, 0), (size_t)c->n * t->ks);
		memmove (c->kids + 1, c->kids,
			 sizeof *c->kids * (size_t)(c->n + 1));
		memcpy (kptr (t, c, 0), kptr (t, p, idx - 1), t->ks);
		c->kids[0] = l->kids[l->n];
		c->n++;
		memcpy (kptr (t, p, idx - 1), kptr (t, l, l->n - 1), t->ks);
		l->n--;
	}
}

static void borrow_right (const bptree_t *t, bpt_node *p, int idx)
{
	bpt_node *c = p->kids[idx], *r = p->kids[idx + 1];
	if (c->leaf) {
		memcpy (kptr (t, c, c->n), kptr (t, r, 0), t->ks);
		c->vals[c->n] = r->vals[0];
		c->n++;
		memmove (kptr (t, r, 0), kptr (t, r, 1),
			 (size_t)(r->n - 1) * t->ks);
		memmove (r->vals, r->vals + 1,
			 sizeof *r->vals * (size_t)(r->n - 1));
		r->n--;
		memcpy (kptr (t, p, idx), kptr (t, r, 0), t->ks);
	} else {
		memcpy (kptr (t, c, c->n), kptr (t, p, idx), t->ks);
		c->kids[c->n + 1] = r->kids[0];
		c->n++;
		memcpy (kptr (t, p, idx), kptr (t, r, 0), t->ks);
		memmove (kptr (t, r, 0), kptr (t, r, 1),
			 (size_t)(r->n - 1) * t->ks);
		memmove (r->kids, r->kids + 1, sizeof *r->kids * (size_t)r->n);
		r->n--;
	}
}

static void merge_right (const bptree_t *t, bpt_node *p, int idx)
{
	bpt_node *c = p->kids[idx], *r = p->kids[idx + 1];
	if (c->leaf) {
		memcpy (kptr (t, c, c->n), r->keys, (size_t)r->n * t->ks);
		memcpy (c->vals + c->n, r->vals,
			sizeof *r->vals * (size_t)r->n);
		c->n += r->n;
		c->next = r->next;
		if (r->next)
			r->next->prev = c;
	} else {
		memcpy (kptr (t, c, c->n), kptr (t, p, idx), t->ks);
		c->n++;
		memcpy (kptr (t, c, c->n), r->keys, (size_t)r->n * t->ks);
		memcpy (c->kids + c->n, r->kids,
			sizeof *r->kids * (size_t)(r->n + 1));
		c->n += r->n;
	}
	memmove (kptr (t, p, idx), kptr (t, p, idx + 1),
		 (size_t)(p->n - idx - 1) * t->ks);
	memmove (p->kids + idx + 1, p->kids + idx + 2,
		 sizeof *p->kids * (size_t)(p->n - idx - 1));
	p->n--;
	node_free (r);
}

static void merge_left (const bptree_t *t, bpt_node *p, int idx)
{
	bpt_node *c = p->kids[idx], *l = p->kids[idx - 1];
	int li = idx - 1;
	if (c->leaf) {
		memcpy (kptr (t, l, l->n), c->keys, (size_t)c->n * t->ks);
		memcpy (l->vals + l->n, c->vals,
			sizeof *l->vals * (size_t)c->n);
		l->n += c->n;
		l->next = c->next;
		if (c->next)
			c->next->prev = l;
	} else {
		memcpy (kptr (t, l, l->n), kptr (t, p, li), t->ks);
		l->n++;
		memcpy (kptr (t, l, l->n), c->keys, (size_t)c->n * t->ks);
		memcpy (l->kids + l->n, c->kids,
			sizeof *l->kids * (size_t)(c->n + 1));
		l->n += c->n;
	}
	memmove (kptr (t, p, li), kptr (t, p, idx),
		 (size_t)(p->n - idx) * t->ks);
	memmove (p->kids + idx, p->kids + idx + 1,
		 sizeof *p->kids * (size_t)(p->n - idx));
	p->n--;
	node_free (c);
}

static void fix_child (const bptree_t *t, bpt_node *p, int idx)
{
	bpt_node *c = p->kids[idx];
	if (c->n >= BPT_MIN)
		return;

	bpt_node *l = idx > 0 ? p->kids[idx - 1] : NULL;
	bpt_node *r = idx < p->n ? p->kids[idx + 1] : NULL;
	if (l && l->n > BPT_MIN) {
		borrow_left (t, p, idx);
		return;
	}
	if (r && r->n > BPT_MIN) {
		borrow_right (t, p, idx);
		return;
	}
	if (r)
		merge_right (t, p, idx);
	else
		merge_left (t, p, idx);
}

static void delete_rec (bptree_t *t, bpt_node *n, const void *key, int *deleted)
{
	if (n->leaf) {
		int pos = leaf_lower_bound (t, n, key);
		if (pos < n->n && cmp_key (t, kptr (t, n, pos), key) == 0) {
			memmove (kptr (t, n, pos), kptr (t, n, pos + 1),
				 (size_t)(n->n - pos - 1) * t->ks);
			memmove (n->vals + pos, n->vals + pos + 1,
				 sizeof *n->vals * (size_t)(n->n - pos - 1));
			n->n--;
			t->count--;
			*deleted = 1;
		}
		return;
	}

	int i = inner_child (t, n, key);
	delete_rec (t, n->kids[i], key, deleted);
	if (*deleted)
		fix_child (t, n, i);
}

int bpt_del (bptree_t *t, const void *key)
{
	if (!t || !key || !t->root)
		return 0;
	int deleted = 0;
	delete_rec (t, t->root, key, &deleted);
	if (!deleted)
		return 0;

	if (t->root->leaf) {
		if (t->root->n == 0) {
			node_free (t->root);
			t->root = NULL;
		}
	} else if (t->root->n == 0) {
		bpt_node *old = t->root;
		t->root = old->kids[0];
		node_free (old);
	}
	return 1;
}

int64_t bpt_count (const bptree_t *t) { return t ? t->count : 0; }

/* --- iterator ---------------------------------------------------------- */

bpt_iter_t *bpt_iter_new (const bptree_t *t)
{
	if (!t)
		return NULL;
	bpt_iter_t *it = malloc (sizeof *it);
	if (!it)
		return NULL;
	it->t = t;
	it->leaf = NULL;
	it->idx = 0;
	return it;
}

void bpt_iter_free (bpt_iter_t *it) { free (it); }

int bpt_iter_first (bpt_iter_t *it)
{
	if (!it || !it->t->root)
		return 0;
	bpt_node *n = it->t->root;
	while (!n->leaf)
		n = n->kids[0];
	it->leaf = n;
	it->idx = 0;
	return n->n > 0;
}

int bpt_iter_last (bpt_iter_t *it)
{
	if (!it || !it->t->root)
		return 0;
	bpt_node *n = it->t->root;
	while (!n->leaf)
		n = n->kids[n->n];
	it->leaf = n;
	it->idx = n->n - 1;
	return n->n > 0;
}

int bpt_iter_seek (bpt_iter_t *it, const void *key)
{
	if (!it || !key || !it->t->root)
		return 0;
	bpt_node *n = it->t->root;
	while (!n->leaf)
		n = n->kids[inner_child (it->t, n, key)];
	int pos = leaf_lower_bound (it->t, n, key);
	it->leaf = n;
	it->idx = pos;
	while (it->leaf && it->idx >= it->leaf->n) {
		it->leaf = it->leaf->next;
		it->idx = 0;
	}
	return it->leaf != NULL;
}

int bpt_iter_next (bpt_iter_t *it)
{
	if (!it || !it->leaf)
		return 0;
	it->idx++;
	while (it->leaf && it->idx >= it->leaf->n) {
		it->leaf = it->leaf->next;
		it->idx = 0;
	}
	return it->leaf != NULL;
}

int bpt_iter_prev (bpt_iter_t *it)
{
	if (!it || !it->leaf)
		return 0;
	it->idx--;
	while (it->leaf && it->idx < 0) {
		it->leaf = it->leaf->prev;
		it->idx = it->leaf ? it->leaf->n - 1 : 0;
	}
	return it->leaf != NULL;
}

const void *bpt_iter_key (const bpt_iter_t *it)
{
	if (!it || !it->leaf || it->idx < 0 || it->idx >= it->leaf->n)
		return NULL;
	return kptr (it->t, it->leaf, it->idx);
}

int64_t bpt_iter_value (const bpt_iter_t *it)
{
	if (!it || !it->leaf || it->idx < 0 || it->idx >= it->leaf->n)
		return 0;
	return it->leaf->vals[it->idx];
}
