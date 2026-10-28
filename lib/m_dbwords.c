#include "m_dbwords.h"

#include <stdlib.h>
#include <string.h>

struct m_dbwords {
	m_forth_t *f;
	m_isam_db_t *db;
	m_isam_table_t *table;
	int64_t current_recno;
	int have_current;
	cell_t current[M_ISAM_MAX_COLS];
	/* pending schema definition for the DDL words */
	struct {
		int active;
		char *name;
		int ncols;
		struct {
			char *name;
			char type;
		} cols[M_ISAM_MAX_COLS];
		int npk;
		char *pk[M_ISAM_MAX_COLS];
	} def;
};

#define DBW(f) ((m_dbwords_t *)m_forth_user (f))

static m_isam_table_t *need_table (m_forth_t *f)
{
	m_dbwords_t *dw = DBW (f);
	if (!dw || !dw->table)
		m_forth_raise (f, -1, "no table selected");
	return dw ? dw->table : NULL;
}

static const char *pop_name (m_forth_t *f)
{
	uint32_t id = (uint32_t)m_forth_pop (f);
	const char *s = m_forth_resolve (f, id);
	if (!s)
		m_forth_raise (f, -1, "invalid string id");
	return s;
}

static char *copy_str (const char *s)
{
	size_t n = strlen (s) + 1;
	char *p = malloc (n);
	if (p)
		memcpy (p, s, n);
	return p;
}

static void def_clear (m_dbwords_t *dw)
{
	if (!dw)
		return;
	free (dw->def.name);
	dw->def.name = NULL;
	for (int c = 0; c < dw->def.ncols; c++)
		free (dw->def.cols[c].name);
	for (int i = 0; i < dw->def.npk; i++)
		free (dw->def.pk[i]);
	dw->def.active = 0;
	dw->def.ncols = 0;
	dw->def.npk = 0;
}

static void p_db_open (m_forth_t *f)
{
	(void)pop_name (f); /* monolith: database object is already bound */
	m_dbwords_t *dw = DBW (f);
	m_forth_push (f, dw && dw->db ? -1 : 0);
}

static void p_db_table (m_forth_t *f)
{
	m_dbwords_t *dw = DBW (f);
	if (!dw)
		m_forth_raise (f, -1, "DB words not bound");
	const char *name = pop_name (f);
	dw->table = m_isam_table (dw->db, name);
	dw->current_recno = 0;
	dw->have_current = 0;
	m_forth_push (f, dw->table ? -1 : 0);
}

static void p_db_field (m_forth_t *f)
{
	m_isam_table_t *t = need_table (f);
	const char *name = pop_name (f);
	m_forth_push (f, m_isam_field (t, name));
}

static void p_db_put (m_forth_t *f)
{
	m_isam_table_t *t = need_table (f);
	cell_t value = m_forth_pop (f);
	const char *name = pop_name (f);
	int c = m_isam_field (t, name);
	if (c < 0)
		m_forth_raise (f, -13, "unknown column");
	if (m_isam_coltype (t, c) == 's' || m_isam_coltype (t, c) == 'z') {
		const char *text = m_forth_resolve (f, (uint32_t)value);
		if (!text)
			m_forth_raise (f, -1, "string value expected");
		uint32_t id = m_isam_intern (t, text);
		if (!id)
			m_forth_raise (f, -8, "string intern failed");
		value = id;
	}
	if (m_isam_put (t, name, value) != 0)
		m_forth_raise (f, -1, "DB-PUT failed");
}

static void p_db_insert (m_forth_t *f)
{
	m_dbwords_t *dw = DBW (f);
	m_isam_table_t *t = need_table (f);
	int64_t uid = m_isam_insert (t);
	if (uid <= 0)
		m_forth_raise (f, -1, "duplicate key or insert failed");
	dw->current_recno = uid;
	dw->have_current = 0;
	m_forth_push (f, uid);
}

static void p_db_seek (m_forth_t *f)
{
	m_dbwords_t *dw = DBW (f);
	m_isam_table_t *t = need_table (f);
	int npk = m_isam_npk (t);
	if (npk <= 0)
		m_forth_raise (f, -1, "table has no primary index");
	cell_t key[M_ISAM_MAX_COLS];
	for (int i = npk - 1; i >= 0; i--) {
		cell_t v = m_forth_pop (f);
		if (m_isam_coltype (t, i) == 's' ||
		    m_isam_coltype (t, i) == 'z') {
			const char *text = m_forth_resolve (f, (uint32_t)v);
			if (!text)
				m_forth_raise (f, -1, "string key expected");
			uint32_t id = m_isam_intern (t, text);
			if (!id)
				m_forth_raise (f, -8, "string intern failed");
			v = id;
		}
		key[i] = v;
	}
	int64_t recno = 0;
	if (m_isam_seek (t, key, npk, &recno)) {
		dw->current_recno = recno;
		dw->have_current = 0;
		m_forth_push (f, -1);
	} else {
		dw->current_recno = 0;
		dw->have_current = 0;
		m_forth_push (f, 0);
	}
}

static void p_db_fetch (m_forth_t *f)
{
	m_dbwords_t *dw = DBW (f);
	m_isam_table_t *t = need_table (f);
	if (!dw->current_recno ||
	    !m_isam_fetch (t, dw->current_recno, dw->current)) {
		dw->have_current = 0;
		m_forth_push (f, 0);
		return;
	}
	dw->have_current = 1;
	m_forth_push (f, -1);
}

static void p_db_get (m_forth_t *f)
{
	m_dbwords_t *dw = DBW (f);
	m_isam_table_t *t = need_table (f);
	const char *name = pop_name (f);
	int c = m_isam_field (t, name);
	if (c < 0)
		m_forth_raise (f, -13, "unknown column");
	if (!dw->have_current) {
		if (!dw->current_recno ||
		    !m_isam_fetch (t, dw->current_recno, dw->current))
			m_forth_raise (f, -1, "no current record");
		dw->have_current = 1;
	}
	cell_t v = dw->current[c];
	if (m_isam_coltype (t, c) == 's' || m_isam_coltype (t, c) == 'z') {
		const char *text = m_isam_resolve (t, (uint32_t)v);
		uint32_t id = m_forth_intern (f, text);
		if (!id)
			m_forth_raise (f, -8, "string intern failed");
		v = id;
	}
	m_forth_push (f, v);
}

static void p_db_delete (m_forth_t *f)
{
	m_dbwords_t *dw = DBW (f);
	m_isam_table_t *t = need_table (f);
	int rc = dw->current_recno ? m_isam_delete (t, dw->current_recno) : 0;
	dw->have_current = 0;
	m_forth_push (f, rc ? -1 : 0);
}

static void p_db_count (m_forth_t *f)
{
	m_dbwords_t *dw = DBW (f);
	const char *name = pop_name (f);
	m_isam_table_t *t = dw ? m_isam_table (dw->db, name) : NULL;
	if (!t)
		m_forth_raise (f, -1, "unknown table");
	m_forth_push (f, m_isam_count (t));
}

static void p_db_stats (m_forth_t *f)
{
	/* in-memory core: no pager I/O yet */
	m_forth_push (f, 0);
	m_forth_push (f, 0);
}

/* Convert a textual value by column type ('s' intern, numeric parse). */
static cell_t text_value (m_forth_t *f, m_isam_table_t *t, int c,
			  const char *text)
{
	if (!text)
		m_forth_raise (f, -1, "text value expected");
	char ty = m_isam_coltype (t, c);
	if (ty == 's' || ty == 'z') {
		uint32_t id = m_isam_intern (t, text);
		if (!id)
			m_forth_raise (f, -8, "string intern failed");
		return (cell_t)id;
	}
	char *end;
	long long v = strtoll (text, &end, 10);
	if (end == text || *end)
		m_forth_raise (f, -1, "numeric value expected");
	return (cell_t)v;
}

static void p_db_put_str (m_forth_t *f)
{
	m_isam_table_t *t = need_table (f);
	uint32_t vid = (uint32_t)m_forth_pop (f);
	const char *name = pop_name (f);
	int c = m_isam_field (t, name);
	if (c < 0)
		m_forth_raise (f, -13, "unknown column");
	cell_t v = text_value (f, t, c, m_forth_resolve (f, vid));
	if (m_isam_put (t, name, v) != 0)
		m_forth_raise (f, -1, "DB-PUT-STR failed");
}

static void p_db_find (m_forth_t *f)
{
	m_dbwords_t *dw = DBW (f);
	m_isam_table_t *t = need_table (f);
	uint32_t vid = (uint32_t)m_forth_pop (f);
	const char *name = pop_name (f);
	int c = m_isam_field (t, name);
	if (c < 0)
		m_forth_raise (f, -13, "unknown column");
	cell_t want = text_value (f, t, c, m_forth_resolve (f, vid));

	/* ponytail: linear scan; secondary indexes arrive in M2. */
	int64_t nrecs = m_isam_nrecs (t);
	for (int64_t r = 1; r <= nrecs; r++) {
		cell_t v;
		if (m_isam_get (t, r, name, &v) && v == want) {
			dw->current_recno = r;
			dw->have_current = 0;
			m_forth_push (f, -1);
			return;
		}
	}
	dw->current_recno = 0;
	dw->have_current = 0;
	m_forth_push (f, 0);
}

/* --- schema DDL + ALTER-per-copy --------------------------------------- */

static void p_db_create_table (m_forth_t *f)
{
	m_dbwords_t *dw = DBW (f);
	if (!dw) {
		m_forth_raise (f, -1, "DB words not bound");
		return;
	}
	const char *name = pop_name (f);
	if (!name || !*name) {
		m_forth_raise (f, -1, "table name required");
		return;
	}
	if (m_isam_table (dw->db, name)) {
		m_forth_push (f, 0);
		return;
	}
	def_clear (dw);
	dw->def.name = copy_str (name);
	if (!dw->def.name) {
		m_forth_raise (f, -8, "out of memory");
		return;
	}
	dw->def.active = 1;
	dw->table = NULL;
	dw->current_recno = 0;
	dw->have_current = 0;
	m_forth_push (f, -1);
}

static void p_db_col (m_forth_t *f)
{
	m_dbwords_t *dw = DBW (f);
	if (!dw || !dw->def.active) {
		m_forth_raise (f, -1, "no table definition");
		return;
	}
	const char *type = pop_name (f);
	const char *name = pop_name (f);
	if (!name || !*name) {
		m_forth_raise (f, -1, "column name required");
		return;
	}
	if (!type || type[0] == 0 || type[1] != 0 ||
	    (type[0] != 'i' && type[0] != 'd' && type[0] != 's' &&
	     type[0] != 'z')) {
		m_forth_raise (f, -1, "invalid column type");
		return;
	}
	if (dw->def.ncols >= M_ISAM_MAX_COLS) {
		m_forth_raise (f, -1, "too many columns");
		return;
	}
	for (int c = 0; c < dw->def.ncols; c++)
		if (strcmp (dw->def.cols[c].name, name) == 0) {
			m_forth_raise (f, -1, "duplicate column");
			return;
		}
	char *copy = copy_str (name);
	if (!copy) {
		m_forth_raise (f, -8, "out of memory");
		return;
	}
	dw->def.cols[dw->def.ncols].name = copy;
	dw->def.cols[dw->def.ncols].type = type[0];
	dw->def.ncols++;
}

static void p_db_primary (m_forth_t *f)
{
	m_dbwords_t *dw = DBW (f);
	if (!dw || !dw->def.active) {
		m_forth_raise (f, -1, "no table definition");
		return;
	}
	const char *name = pop_name (f);
	if (!name || !*name) {
		m_forth_raise (f, -1, "column name required");
		return;
	}
	if (dw->def.npk >= M_ISAM_MAX_COLS) {
		m_forth_raise (f, -1, "too many key columns");
		return;
	}
	char *copy = copy_str (name);
	if (!copy) {
		m_forth_raise (f, -8, "out of memory");
		return;
	}
	dw->def.pk[dw->def.npk++] = copy;
}

static void p_db_table_create (m_forth_t *f)
{
	m_dbwords_t *dw = DBW (f);
	if (!dw || !dw->def.active) {
		m_forth_raise (f, -1, "no table definition");
		return;
	}
	if (dw->def.ncols < 1) {
		m_forth_raise (f, -1, "no columns");
		return;
	}
	if (dw->def.npk > dw->def.ncols) {
		m_forth_raise (f, -1, "too many key columns");
		return;
	}
	for (int i = 0; i < dw->def.npk; i++)
		if (strcmp (dw->def.pk[i], dw->def.cols[i].name) != 0) {
			m_forth_raise (f, -1,
				       "primary key must be a column prefix");
			return;
		}
	m_isam_col_t cols[M_ISAM_MAX_COLS];
	const char *pkp[M_ISAM_MAX_COLS];
	for (int c = 0; c < dw->def.ncols; c++) {
		cols[c].name = dw->def.cols[c].name;
		cols[c].type = dw->def.cols[c].type;
	}
	for (int i = 0; i < dw->def.npk; i++)
		pkp[i] = dw->def.pk[i];
	m_isam_table_t *t = m_isam_create_table (
		dw->db, dw->def.name, cols, dw->def.ncols, pkp, dw->def.npk);
	if (!t) {
		def_clear (dw);
		m_forth_raise (f, -1, "create table failed");
		return;
	}
	dw->table = t;
	def_clear (dw);
	dw->current_recno = 0;
	dw->have_current = 0;
	m_forth_push (f, -1);
}

static void p_db_copy_table (m_forth_t *f)
{
	m_dbwords_t *dw = DBW (f);
	if (!dw) {
		m_forth_raise (f, -1, "DB words not bound");
		return;
	}
	const char *dst = pop_name (f);
	const char *src = pop_name (f);
	if (!src || !dst) {
		m_forth_raise (f, -1, "table name required");
		return;
	}
	m_forth_push (f, m_isam_copy_table (dw->db, src, dst) ? -1 : 0);
}

static void p_db_drop_table (m_forth_t *f)
{
	m_dbwords_t *dw = DBW (f);
	if (!dw) {
		m_forth_raise (f, -1, "DB words not bound");
		return;
	}
	const char *name = pop_name (f);
	if (!name) {
		m_forth_raise (f, -1, "table name required");
		return;
	}
	m_isam_table_t *t = m_isam_table (dw->db, name);
	int rc = m_isam_drop_table (dw->db, name);
	if (rc && t == dw->table) {
		dw->table = NULL;
		dw->current_recno = 0;
		dw->have_current = 0;
	}
	m_forth_push (f, rc ? -1 : 0);
}

static void p_db_rename_table (m_forth_t *f)
{
	m_dbwords_t *dw = DBW (f);
	if (!dw) {
		m_forth_raise (f, -1, "DB words not bound");
		return;
	}
	const char *newname = pop_name (f);
	const char *name = pop_name (f);
	if (!name || !newname) {
		m_forth_raise (f, -1, "table name required");
		return;
	}
	m_forth_push (f, m_isam_rename_table (dw->db, name, newname) ? -1 : 0);
}

static void p_db_tables (m_forth_t *f)
{
	m_dbwords_t *dw = DBW (f);
	m_forth_push (f, dw ? m_isam_ntables (dw->db) : 0);
}

static void p_db_table_name (m_forth_t *f)
{
	m_dbwords_t *dw = DBW (f);
	int i = (int)m_forth_pop (f);
	const char *name = dw ? m_isam_table_at (dw->db, i) : NULL;
	if (!name) {
		m_forth_push (f, 0);
		return;
	}
	uint32_t id = m_forth_intern (f, name);
	if (!id) {
		m_forth_raise (f, -8, "string intern failed");
		return;
	}
	m_forth_push (f, (cell_t)id);
}

static void p_db_cols (m_forth_t *f)
{
	m_dbwords_t *dw = DBW (f);
	const char *name = pop_name (f);
	m_isam_table_t *t = (dw && name) ? m_isam_table (dw->db, name) : NULL;
	if (!t) {
		m_forth_raise (f, -1, "unknown table");
		return;
	}
	m_forth_push (f, m_isam_ncols (t));
}

m_dbwords_t *m_dbwords_new (m_forth_t *f, m_isam_db_t *db)
{
	if (!f || !db)
		return NULL;
	m_dbwords_t *dw = calloc (1, sizeof *dw);
	if (!dw)
		return NULL;
	dw->f = f;
	dw->db = db;
	m_forth_set_user (f, dw);

	m_forth_define (f, "DB-OPEN", p_db_open, 0);
	m_forth_define (f, "DB-TABLE", p_db_table, 0);
	m_forth_define (f, "DB-FIELD", p_db_field, 0);
	m_forth_define (f, "DB-PUT", p_db_put, 0);
	m_forth_define (f, "DB-PUT-STR", p_db_put_str, 0);
	m_forth_define (f, "DB-FIND", p_db_find, 0);
	m_forth_define (f, "DB-INSERT", p_db_insert, 0);
	m_forth_define (f, "DB-SEEK", p_db_seek, 0);
	m_forth_define (f, "DB-FETCH", p_db_fetch, 0);
	m_forth_define (f, "DB-GET", p_db_get, 0);
	m_forth_define (f, "DB-DELETE", p_db_delete, 0);
	m_forth_define (f, "DB-COUNT", p_db_count, 0);
	m_forth_define (f, "DB-STATS", p_db_stats, 0);
	m_forth_define (f, "DB-CREATE-TABLE", p_db_create_table, 0);
	m_forth_define (f, "DB-COL", p_db_col, 0);
	m_forth_define (f, "DB-PRIMARY", p_db_primary, 0);
	m_forth_define (f, "DB-TABLE-CREATE", p_db_table_create, 0);
	m_forth_define (f, "DB-COPY-TABLE", p_db_copy_table, 0);
	m_forth_define (f, "DB-DROP-TABLE", p_db_drop_table, 0);
	m_forth_define (f, "DB-RENAME-TABLE", p_db_rename_table, 0);
	m_forth_define (f, "DB-TABLES", p_db_tables, 0);
	m_forth_define (f, "DB-TABLE-NAME", p_db_table_name, 0);
	m_forth_define (f, "DB-COLS", p_db_cols, 0);
	return dw;
}

m_isam_table_t *m_dbwords_table (const m_dbwords_t *dw)
{
	return dw ? dw->table : NULL;
}

int64_t m_dbwords_recno (const m_dbwords_t *dw)
{
	return dw ? dw->current_recno : 0;
}

int m_dbwords_have_current (const m_dbwords_t *dw)
{
	return dw ? dw->have_current : 0;
}

void m_dbwords_free (m_dbwords_t *dw)
{
	if (!dw)
		return;
	def_clear (dw);
	if (m_forth_user (dw->f) == dw)
		m_forth_set_user (dw->f, NULL);
	free (dw);
}
