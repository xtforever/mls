#include "m_isam.h"

#include "bptree.h"
#include "darray.h"
#include "hashmap.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
	char *name;
	char type;
} col_t;

struct m_isam_table {
	m_isam_db_t *db;
	char *name;
	int ncols;
	col_t *cols;
	int npk;
	int *pk;       /* column indices, prefix of the record */
	bptree_t *idx; /* NULL for RRDS (npk == 0) */

	cell_t **recs;	/* record cells */
	uint8_t *flags; /* bit0 = tombstone */
	int64_t nrecs;	/* allocated records (including tombstones) */
	int64_t cap;
	int64_t live;
	cell_t *buf; /* insert/update buffer */
	int buf_set;
};

struct m_isam_db {
	m_isam_table_t **tables;
	int ntables;
	int cap;
	darray_t *id2str;
	hashmap_t *str2id;
	uint32_t strcount;
};

/* --- per-db string table ----------------------------------------------- */

static uint32_t db_intern (m_isam_db_t *db, const char *s)
{
	if (!db || !s)
		return 0;
	uint32_t id;
	if (hm_get (db->str2id, s, &id))
		return id;
	if (db->strcount == UINT32_MAX)
		return 0;
	size_t n = strlen (s) + 1;
	char *copy = malloc (n);
	if (!copy)
		return 0;
	memcpy (copy, s, n);
	da_push (db->id2str, copy);
	if (da_len (db->id2str) != (int)db->strcount + 1 ||
	    hm_put (db->str2id, copy, db->strcount + 1) != 0) {
		free (da_pop (db->id2str));
		free (copy);
		return 0;
	}
	return ++db->strcount;
}

static const char *db_resolve (const m_isam_db_t *db, uint32_t id)
{
	if (!db || id == 0 || id > db->strcount)
		return "";
	return da_get (db->id2str, (int)id - 1);
}

/* --- primary-key comparator -------------------------------------------- */

static int col_is_string (const m_isam_table_t *t, int c)
{
	return t->cols[c].type == 's' || t->cols[c].type == 'z';
}

static int pk_cmp (const void *a, const void *b, void *ctx)
{
	const m_isam_table_t *t = ctx;
	const cell_t *ka = a, *kb = b;
	for (int i = 0; i < t->npk; i++) {
		int c = t->pk[i];
		if (col_is_string (t, c)) {
			const char *sa = db_resolve (t->db, (uint32_t)ka[i]);
			const char *sb = db_resolve (t->db, (uint32_t)kb[i]);
			int r = strcmp (sa, sb);
			if (r)
				return r;
		} else {
			if (ka[i] != kb[i])
				return ka[i] < kb[i] ? -1 : 1;
		}
	}
	return 0;
}

/* --- helpers ----------------------------------------------------------- */

static char *dup_str (const char *s)
{
	size_t n = strlen (s) + 1;
	char *p = malloc (n);
	if (p)
		memcpy (p, s, n);
	return p;
}

static m_isam_table_t *db_find_table (m_isam_db_t *db, const char *name)
{
	for (int i = 0; i < db->ntables; i++)
		if (strcmp (db->tables[i]->name, name) == 0)
			return db->tables[i];
	return NULL;
}

static int build_key (const m_isam_table_t *t, const cell_t *cells, cell_t *key)
{
	for (int i = 0; i < t->npk; i++)
		key[i] = cells[t->pk[i]];
	return t->npk;
}

m_isam_db_t *m_isam_db_new (void)
{
	m_isam_db_t *db = calloc (1, sizeof *db);
	if (!db)
		return NULL;
	db->id2str = da_new ();
	db->str2id = hm_new ();
	if (!db->id2str || !db->str2id) {
		da_free (db->id2str);
		hm_free (db->str2id);
		free (db);
		return NULL;
	}
	return db;
}

void m_isam_db_free (m_isam_db_t *db)
{
	if (!db)
		return;
	for (int i = 0; i < db->ntables; i++) {
		m_isam_table_t *t = db->tables[i];
		bpt_free (t->idx);
		for (int64_t r = 0; r < t->nrecs; r++)
			free (t->recs[r]);
		free (t->recs);
		free (t->flags);
		free (t->buf);
		free (t->pk);
		for (int c = 0; c < t->ncols; c++)
			free (t->cols[c].name);
		free (t->cols);
		free (t->name);
		free (t);
	}
	free (db->tables);
	for (int i = 0; i < da_len (db->id2str); i++)
		free (da_get (db->id2str, i));
	da_free (db->id2str);
	hm_free (db->str2id);
	free (db);
}

m_isam_table_t *m_isam_create_table (m_isam_db_t *db, const char *name,
				     const m_isam_col_t *cols, int ncols,
				     const char *const *pk_names, int npk)
{
	if (!db || !name || !cols || ncols <= 0 || ncols > M_ISAM_MAX_COLS ||
	    npk < 0 || npk > ncols || (npk > 0 && !pk_names))
		return NULL;
	if (db_find_table (db, name))
		return NULL;

	m_isam_table_t *t = calloc (1, sizeof *t);
	if (!t)
		return NULL;
	t->db = db;
	t->name = dup_str (name);
	t->ncols = ncols;
	t->cols = calloc ((size_t)ncols, sizeof *t->cols);
	t->pk = calloc ((size_t)(npk ? npk : 1), sizeof *t->pk);
	t->buf = calloc ((size_t)ncols, sizeof *t->buf);
	t->recs = calloc (8, sizeof *t->recs);
	t->flags = calloc (8, sizeof *t->flags);
	t->cap = 8;
	t->npk = npk;
	if (!t->name || !t->cols || !t->pk || !t->buf || !t->recs || !t->flags)
		goto fail;

	for (int c = 0; c < ncols; c++) {
		if (!cols[c].name ||
		    (cols[c].type != 'i' && cols[c].type != 'd' &&
		     cols[c].type != 's' && cols[c].type != 'z'))
			goto fail;
		t->cols[c].name = dup_str (cols[c].name);
		t->cols[c].type = cols[c].type;
		if (!t->cols[c].name)
			goto fail;
	}

	for (int i = 0; i < npk; i++) {
		int c = -1;
		for (int j = 0; j < ncols; j++)
			if (strcmp (cols[j].name, pk_names[i]) == 0) {
				c = j;
				break;
			}
		if (c < 0 || c != i) /* PK is a column prefix */
			goto fail;
		t->pk[i] = c;
	}

	if (npk > 0) {
		t->idx = bpt_new ((size_t)npk * sizeof (cell_t), pk_cmp, t);
		if (!t->idx)
			goto fail;
	}

	if (db->ntables == db->cap) {
		int cap = db->cap ? db->cap * 2 : 8;
		m_isam_table_t **p =
			realloc (db->tables, (size_t)cap * sizeof *p);
		if (!p)
			goto fail;
		db->tables = p;
		db->cap = cap;
	}
	db->tables[db->ntables++] = t;
	return t;

fail:
	if (t) {
		bpt_free (t->idx);
		for (int c = 0; t->cols && c < ncols; c++)
			free (t->cols[c].name);
		free (t->cols);
		free (t->pk);
		free (t->buf);
		free (t->recs);
		free (t->flags);
		free (t->name);
		free (t);
	}
	return NULL;
}

m_isam_table_t *m_isam_table (m_isam_db_t *db, const char *name)
{
	return db && name ? db_find_table (db, name) : NULL;
}

int m_isam_field (const m_isam_table_t *t, const char *name)
{
	if (!t || !name)
		return -1;
	for (int c = 0; c < t->ncols; c++)
		if (strcmp (t->cols[c].name, name) == 0)
			return c;
	return -1;
}

int m_isam_put (m_isam_table_t *t, const char *col, cell_t value)
{
	if (!t)
		return -1;
	int c = m_isam_field (t, col);
	if (c < 0)
		return -1;
	t->buf[c] = value;
	t->buf_set = 1;
	return 0;
}

int64_t m_isam_insert (m_isam_table_t *t)
{
	if (!t)
		return 0;

	cell_t *rec = malloc ((size_t)t->ncols * sizeof *rec);
	if (!rec)
		return 0;

	if (t->nrecs == t->cap) {
		int64_t cap = t->cap * 2;
		cell_t **rp = realloc (t->recs, (size_t)cap * sizeof *rp);
		if (!rp) {
			free (rec);
			return 0;
		}
		t->recs = rp;
		uint8_t *fp = realloc (t->flags, (size_t)cap);
		if (!fp) {
			free (rec);
			return 0;
		}
		t->flags = fp;
		memset (t->flags + t->cap, 0, (size_t)(cap - t->cap));
		t->cap = cap;
	}

	if (t->idx) {
		cell_t key[M_ISAM_MAX_COLS];
		build_key (t, t->buf, key);
		if (bpt_insert (t->idx, key, (int64_t)(t->nrecs + 1)) != 0) {
			free (rec); /* duplicate or error */
			return 0;
		}
	}

	memcpy (rec, t->buf, (size_t)t->ncols * sizeof *rec);
	t->recs[t->nrecs] = rec;
	t->flags[t->nrecs] = 0;
	t->nrecs++;
	t->live++;
	memset (t->buf, 0, (size_t)t->ncols * sizeof *t->buf);
	t->buf_set = 0;
	return t->nrecs; /* recno */
}

int m_isam_seek (const m_isam_table_t *t, const cell_t *key, int nkey,
		 int64_t *recno)
{
	if (!t || !t->idx || !key || !recno || nkey != t->npk)
		return 0;
	int64_t r;
	if (!bpt_get (t->idx, key, &r))
		return 0;
	*recno = r;
	return 1;
}

int m_isam_fetch (const m_isam_table_t *t, int64_t recno, cell_t *out)
{
	if (!t || !out || recno < 1 || recno > t->nrecs)
		return 0;
	int64_t i = recno - 1;
	if (t->flags[i] & 1)
		return 0;
	memcpy (out, t->recs[i], (size_t)t->ncols * sizeof *out);
	return 1;
}

int m_isam_get (const m_isam_table_t *t, int64_t recno, const char *col,
		cell_t *out)
{
	if (!t || !out)
		return 0;
	int c = m_isam_field (t, col);
	if (c < 0)
		return 0;
	cell_t tmp[M_ISAM_MAX_COLS];
	if (!m_isam_fetch (t, recno, tmp))
		return 0;
	*out = tmp[c];
	return 1;
}

int m_isam_delete (m_isam_table_t *t, int64_t recno)
{
	if (!t || recno < 1 || recno > t->nrecs)
		return 0;
	int64_t i = recno - 1;
	if (t->flags[i] & 1)
		return 0;
	if (t->idx) {
		cell_t key[M_ISAM_MAX_COLS];
		build_key (t, t->recs[i], key);
		bpt_del (t->idx, key);
	}
	t->flags[i] |= 1;
	t->live--;
	return 1;
}

int64_t m_isam_count (const m_isam_table_t *t) { return t ? t->live : 0; }

int64_t m_isam_nrecs (const m_isam_table_t *t)
{
	return t ? t->nrecs : 0;
}

int m_isam_ncols (const m_isam_table_t *t)
{
	return t ? t->ncols : 0;
}

int m_isam_npk (const m_isam_table_t *t)
{
	return t ? t->npk : 0;
}

const char *m_isam_colname (const m_isam_table_t *t, int c)
{
	return t && c >= 0 && c < t->ncols ? t->cols[c].name : NULL;
}

char m_isam_coltype (const m_isam_table_t *t, int c)
{
	return t && c >= 0 && c < t->ncols ? t->cols[c].type : 0;
}

uint32_t m_isam_intern (m_isam_table_t *t, const char *s)
{
	return t ? db_intern (t->db, s) : 0;
}

const char *m_isam_resolve (const m_isam_table_t *t, uint32_t id)
{
	return t ? db_resolve (t->db, id) : "";
}
