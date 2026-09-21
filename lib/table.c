#include "table.h"

#include <string.h>

/* A table handle holds one tbl_root_t: a magic + the sorted entry list.
 * The magic makes tbl_is_table() robust without any global registry. */
#define TBL_MAGIC 0x4D4C5354 /* "MLST" */

typedef struct {
	int magic;
	int entries; /* LIST_LIST node of (key value) entry nodes */
} tbl_root_t;

/* Each entry is a LIST_LIST node with children:
 *   [0] = key   (LIST_STRING node)
 *   [1] = value (LIST_STRING or LIST_HANDLE node) */

static tbl_root_t *tbl_root_of (int t)
{
	return t > 0 ? (tbl_root_t *)mls (t, 0) : NULL;
}

static int tbl_entries (int t)
{
	tbl_root_t *r = tbl_root_of (t);
	return r && r->magic == TBL_MAGIC ? r->entries : 0;
}

int tbl_create (void)
{
	tbl_root_t r = {TBL_MAGIC, list_new ()};
	int h = m_create (0, sizeof (tbl_root_t));
	m_put (h, &r);
	return h;
}

int tbl_is_table (int t)
{
	tbl_root_t *r = tbl_root_of (t);
	return r && r->magic == TBL_MAGIC;
}

void tbl_free (int t)
{
	tbl_root_t *r = tbl_root_of (t);
	if (!r || r->magic != TBL_MAGIC)
		return;
	list_free (r->entries);
	m_free (t);
}

int tbl_len (int t) { return list_len (tbl_entries (t)); }

static const char *tbl_key_of (int entry)
{
	int kn = list_get (entry, 0);
	int kh = kn > 0 ? list_data (kn) : 0;
	return kh > 0 ? (const char *)m_buf (kh) : NULL;
}

const char *tbl_key (int t, int i)
{
	int e = list_get (tbl_entries (t), i);
	return e > 0 ? tbl_key_of (e) : NULL;
}

const char *tbl_value (int t, int i)
{
	int e = list_get (tbl_entries (t), i);
	return e > 0 ? list_text (list_get (e, 1)) : NULL;
}

/* binary search for key; returns entry index or -1 */
static int tbl_find (int t, const char *key)
{
	int entries = tbl_entries (t);
	int lo = 0, hi = list_len (entries);
	while (lo < hi) {
		int mid = (lo + hi) / 2;
		const char *k = tbl_key_of (list_get (entries, mid));
		int c = strcmp (k ? k : "", key);
		if (c == 0)
			return mid;
		if (c < 0)
			lo = mid + 1;
		else
			hi = mid;
	}
	return -1;
}

/* insert a child handle into the entry list at position p */
static void insert_at (int t, int p, int entry)
{
	int ch = list_children (tbl_entries (t));
	int dummy = 0;
	m_put (ch, &dummy);
	int *base = m_buf (ch);
	int n = (int)m_len (ch);
	memmove (base + p + 1, base + p, (size_t)(n - 1 - p) * sizeof (int));
	base[p] = entry;
}

/* replace the value node of an entry, freeing the old one */
static void entry_set_value (int e, int new_value_node)
{
	int ch = list_children (e);
	int old = INT (ch, 1);
	INT (ch, 1) = new_value_node;
	list_free (old);
}

/* insert entry keeping the list sorted by key */
static void insert_sorted (int t, const char *key, int entry)
{
	int entries = tbl_entries (t);
	int lo = 0, hi = list_len (entries);
	while (lo < hi) {
		int mid = (lo + hi) / 2;
		if (strcmp (tbl_key_of (list_get (entries, mid)), key) < 0)
			lo = mid + 1;
		else
			hi = mid;
	}
	insert_at (t, lo, entry);
}

static int set_common (int t, const char *key, int key_node, int value_node)
{
	int entries = tbl_entries (t);
	if (!entries || !key)
		return -1;
	int i = tbl_find (t, key);
	if (i >= 0) {
		list_free (key_node); /* keep existing key, drop the new one */
		entry_set_value (list_get (entries, i), value_node);
		return 0;
	}
	int e = list_new ();
	list_add (e, key_node);
	list_add (e, value_node);
	insert_sorted (t, key, e);
	return 0;
}

int tbl_set (int t, const char *key, const char *value)
{
	return set_common (t, key, list_string (key), list_string (value));
}

int tbl_set_handle (int t, const char *key, int value_h)
{
	return set_common (t, key, list_string (key),
			   list_node (LIST_HANDLE, value_h));
}

int tbl_set_handle_str (int t, int key_h, int value_h)
{
	if (key_h <= 0)
		return -1;
	return set_common (t, (const char *)m_buf (key_h),
			   list_node (LIST_STRING, key_h),
			   list_node (LIST_HANDLE, value_h));
}

const char *tbl_get (int t, const char *key)
{
	int i = tbl_find (t, key);
	if (i < 0)
		return NULL;
	int e = list_get (tbl_entries (t), i);
	return list_text (list_get (e, 1));
}

int tbl_get_handle (int t, const char *key)
{
	int i = tbl_find (t, key);
	if (i < 0)
		return 0;
	int e = list_get (tbl_entries (t), i);
	return list_data (list_get (e, 1));
}

int tbl_del (int t, const char *key)
{
	int entries = tbl_entries (t);
	int i = tbl_find (t, key);
	if (i < 0)
		return -1;
	int ch = list_children (entries);
	int *base = m_buf (ch);
	int n = (int)m_len (ch);
	int e = base[i];
	memmove (base + i, base + i + 1, (size_t)(n - i - 1) * sizeof (int));
	m_setlen (ch, (size_t)(n - 1));
	list_free (e);
	return 0;
}
