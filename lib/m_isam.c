#include "m_isam.h"

#include "bptree.h"
#include "m_strpool.h"
#include "pager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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

	pager_t *data; /* <table>.dat (page 0 = header) */
	int64_t nrecs; /* allocated slots (including tombstones) */
	int64_t live;
	cell_t *buf; /* insert/update buffer */
	int buf_set;
};

struct m_isam_db {
	m_isam_table_t **tables;
	int ntables;
	int cap;
	m_strpool_t *pool; /* persistent string pool (strings.str) */
	char *dir;	   /* database directory (meta + <table>.dat) */
	int anonymous;	   /* dir was mkdtemp: remove files + dir on free */
};

/* --- per-db string table ----------------------------------------------- */

static uint32_t db_intern (m_isam_db_t *db, const char *s)
{
	if (!db || !db->pool || !s)
		return 0;
	return m_strpool_intern (db->pool, s);
}

static const char *db_resolve (const m_isam_db_t *db, uint32_t id)
{
	if (!db || !db->pool)
		return "";
	const char *r = m_strpool_resolve (db->pool, id);
	return r ? r : "";
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

static int dat_record_rw (m_isam_table_t *t, int64_t recno, const cell_t *cells,
			  int write, uint8_t flags, cell_t *out,
			  uint8_t *flags_out);
static int dat_write_header (m_isam_table_t *t);

static void table_free (m_isam_table_t *t)
{
	if (!t)
		return;
	bpt_free (t->idx);
	pager_close (t->data);
	free (t->buf);
	free (t->pk);
	for (int c = 0; c < t->ncols; c++)
		free (t->cols[c].name);
	free (t->cols);
	free (t->name);
	free (t);
}

/* Roll a partially copied table back to empty (used by m_isam_copy_table). */
static void table_clear_records (m_isam_table_t *t)
{
	if (!t)
		return;
	for (int64_t r = 1; r <= t->nrecs; r++)
		dat_record_rw (t, r, NULL, 1, 1, NULL, NULL); /* tombstone */
	t->live = 0;
	if (t->buf) {
		memset (t->buf, 0, (size_t)t->ncols * sizeof *t->buf);
		t->buf_set = 0;
	}
	if (t->idx) {
		bpt_free (t->idx);
		t->idx = bpt_new ((size_t)t->npk * sizeof (cell_t), pk_cmp, t);
	}
	dat_write_header (t);
}

/* --- on-disk schema registry (meta) + <table>.dat header ---------------- */

#define META_MAGIC "MLSDB1\0\0" /* exactly 8 bytes on disk */
#define META_VERSION 1u
#define DAT_MAGIC "MLSISAM1" /* exactly 8 bytes on disk */
#define DAT_VERSION 1u
#define META_MAX_NAME 120u
#define META_MAX_TABLES 100000u

static char *db_path (const char *dir, const char *name, const char *ext)
{
	if (!dir || !name || !ext)
		return NULL;
	size_t n = strlen (dir) + 1 + strlen (name) + strlen (ext) + 1;
	char *p = malloc (n);
	if (p)
		snprintf (p, n, "%s/%s%s", dir, name, ext);
	return p;
}

/* Table names become path components: reject anything that can escape. */
static int valid_table_name (const char *name)
{
	if (!name || !*name || strlen (name) > META_MAX_NAME)
		return 0;
	if (strcmp (name, ".") == 0 || strcmp (name, "..") == 0)
		return 0;
	if (strchr (name, '/') || strchr (name, '\\'))
		return 0;
	return 1;
}

static int write_u32 (FILE *fp, uint32_t v)
{
	return fwrite (&v, sizeof v, 1, fp) == 1 ? 0 : -1;
}

static int read_u32 (FILE *fp, uint32_t *v)
{
	return fread (v, sizeof *v, 1, fp) == 1 ? 0 : -1;
}

static uint32_t table_recsize (const m_isam_table_t *t)
{
	return (uint32_t)(8 + (uint64_t)t->ncols * sizeof (cell_t));
}

static int meta_write (m_isam_db_t *db)
{
	if (!db || !db->dir)
		return 0;
	char *tmp = db_path (db->dir, "meta", ".tmp");
	char *fin = db_path (db->dir, "meta", "");
	if (!tmp || !fin) {
		free (tmp);
		free (fin);
		return -1;
	}
	FILE *fp = fopen (tmp, "wb");
	int ok = fp != NULL;
	if (ok)
		ok = fwrite (META_MAGIC, 1, 8, fp) == 8 &&
		     write_u32 (fp, META_VERSION) == 0 &&
		     write_u32 (fp, (uint32_t)db->ntables) == 0;
	for (int i = 0; ok && i < db->ntables; i++) {
		m_isam_table_t *t = db->tables[i];
		uint32_t nl = (uint32_t)strlen (t->name);
		ok = write_u32 (fp, nl) == 0 &&
		     fwrite (t->name, 1, nl, fp) == nl &&
		     write_u32 (fp, (uint32_t)t->ncols) == 0;
		for (int c = 0; ok && c < t->ncols; c++) {
			uint32_t cl = (uint32_t)strlen (t->cols[c].name);
			uint8_t ty = (uint8_t)t->cols[c].type;
			ok = write_u32 (fp, cl) == 0 &&
			     fwrite (t->cols[c].name, 1, cl, fp) == cl &&
			     fwrite (&ty, 1, 1, fp) == 1;
		}
		if (ok)
			ok = write_u32 (fp, (uint32_t)t->npk) == 0;
	}
	if (fp) {
		if (ok)
			ok = fflush (fp) == 0;
		if (fclose (fp) != 0)
			ok = 0;
	}
	if (ok && rename (tmp, fin) != 0)
		ok = 0;
	if (!ok)
		unlink (tmp);
	free (tmp);
	free (fin);
	return ok ? 0 : -1;
}

static int dat_create (m_isam_db_t *db, const m_isam_table_t *t)
{
	if (!db || !db->dir)
		return 0;
	char *path = db_path (db->dir, t->name, ".dat");
	if (!path)
		return -1;
	FILE *fp = fopen (path, "wb");
	free (path);
	if (!fp)
		return -1;
	unsigned char page[4096];
	memset (page, 0, sizeof page);
	memcpy (page, DAT_MAGIC, 8);
	uint32_t ver = DAT_VERSION, nc = (uint32_t)t->ncols,
		 rs = table_recsize (t);
	memcpy (page + 8, &ver, 4);
	memcpy (page + 12, &nc, 4);
	memcpy (page + 16, &rs, 4);
	int ok = fwrite (page, 1, sizeof page, fp) == sizeof page;
	if (fclose (fp) != 0)
		ok = 0;
	return ok ? 0 : -1;
}

/* Read one 4096-byte page; pages past EOF read as zero. */
static int dat_page_get (m_isam_table_t *t, uint32_t page, unsigned char *buf)
{
	if (!t || !t->data)
		return -1;
	if (page >= pager_pages (t->data)) {
		memset (buf, 0, PAGER_PAGE_SIZE);
		return 0;
	}
	return pager_read (t->data, page, buf) == 0 ? 0 : -1;
}

/* Read or write one fixed-size record (a record may span two pages). */
static int dat_record_rw (m_isam_table_t *t, int64_t recno, const cell_t *cells,
			  int write, uint8_t flags, cell_t *out,
			  uint8_t *flags_out)
{
	if (!t || !t->data || recno < 1)
		return -1;
	uint32_t recsize = table_recsize (t);
	uint64_t off =
		(uint64_t)PAGER_PAGE_SIZE + (uint64_t)(recno - 1) * recsize;
	uint32_t p0 = (uint32_t)(off / PAGER_PAGE_SIZE);
	uint32_t o0 = (uint32_t)(off % PAGER_PAGE_SIZE);
	uint32_t p1 = (uint32_t)((off + recsize - 1) / PAGER_PAGE_SIZE);
	unsigned char b0[PAGER_PAGE_SIZE], b1[PAGER_PAGE_SIZE];
	unsigned char rec[8 + M_ISAM_MAX_COLS * sizeof (cell_t)];
	int two = p1 != p0;
	size_t first;

	if (dat_page_get (t, p0, b0) != 0)
		return -1;
	if (two && dat_page_get (t, p1, b1) != 0)
		return -1;

	first = (o0 + recsize <= PAGER_PAGE_SIZE) ? recsize
						  : PAGER_PAGE_SIZE - o0;
	if (write) {
		memset (rec, 0, recsize);
		rec[0] = flags;
		if (cells)
			memcpy (rec + 8, cells,
				(size_t)t->ncols * sizeof (cell_t));
		memcpy (b0 + o0, rec, first);
		if (two)
			memcpy (b1, rec + first, recsize - first);
		if (pager_write (t->data, p0, b0) != 0)
			return -1;
		if (two && pager_write (t->data, p1, b1) != 0)
			return -1;
		return 0;
	}

	memcpy (rec, b0 + o0, first);
	if (two)
		memcpy (rec + first, b1, recsize - first);
	if (flags_out)
		*flags_out = rec[0];
	if (out)
		memcpy (out, rec + 8, (size_t)t->ncols * sizeof (cell_t));
	return 0;
}

static int dat_write_header (m_isam_table_t *t)
{
	if (!t || !t->data)
		return -1;
	unsigned char page[PAGER_PAGE_SIZE];
	if (dat_page_get (t, 0, page) != 0)
		return -1;
	uint64_t nslots = (uint64_t)t->nrecs, live = (uint64_t)t->live;
	memcpy (page + 24, &nslots, 8);
	memcpy (page + 32, &live, 8);
	return pager_write (t->data, 0, page) == 0 ? 0 : -1;
}

/* Open <table>.dat, validate the header, load counts and rebuild the index. */
static int table_open_data (m_isam_db_t *db, m_isam_table_t *t)
{
	if (!db || !t)
		return -1;
	if (!db->dir)
		return 0;
	char *path = db_path (db->dir, t->name, ".dat");
	if (!path)
		return -1;
	t->data = pager_open (path);
	free (path);
	if (!t->data)
		return -1;

	unsigned char page[PAGER_PAGE_SIZE];
	if (pager_read (t->data, 0, page) != 0)
		return -1;
	uint32_t ver, nc, rs;
	uint64_t nslots;
	memcpy (&ver, page + 8, 4);
	memcpy (&nc, page + 12, 4);
	memcpy (&rs, page + 16, 4);
	memcpy (&nslots, page + 24, 8);
	if (memcmp (page, DAT_MAGIC, 8) != 0 || ver != DAT_VERSION ||
	    nc != (uint32_t)t->ncols || rs != table_recsize (t))
		return -1;

	t->nrecs = (int64_t)nslots;
	t->live = 0;
	for (int64_t r = 1; r <= t->nrecs; r++) {
		cell_t cells[M_ISAM_MAX_COLS];
		uint8_t fl;
		if (dat_record_rw (t, r, NULL, 0, 0, cells, &fl) != 0)
			return -1;
		if (fl & 1)
			continue;
		if (t->idx) {
			cell_t key[M_ISAM_MAX_COLS];
			build_key (t, cells, key);
			if (bpt_insert (t->idx, key, r) != 0)
				return -1; /* duplicate PK = corrupt */
		}
		t->live++;
	}
	return 0;
}

static void dat_unlink (m_isam_db_t *db, const char *name)
{
	if (!db || !db->dir)
		return;
	char *p = db_path (db->dir, name, ".dat");
	if (p) {
		unlink (p);
		free (p);
	}
}

static m_isam_table_t *table_alloc (m_isam_db_t *db, const char *name,
				    const m_isam_col_t *cols, int ncols,
				    int npk)
{
	m_isam_table_t *t = calloc (1, sizeof *t);
	if (!t)
		return NULL;
	t->db = db;
	t->name = dup_str (name);
	t->ncols = ncols;
	t->cols = calloc ((size_t)ncols, sizeof *t->cols);
	t->pk = calloc ((size_t)(npk ? npk : 1), sizeof *t->pk);
	t->buf = calloc ((size_t)ncols, sizeof *t->buf);
	t->npk = npk;
	if (!t->name || !t->cols || !t->pk || !t->buf)
		goto fail;
	for (int c = 0; c < ncols; c++) {
		t->cols[c].name = dup_str (cols[c].name);
		t->cols[c].type = cols[c].type;
		if (!t->cols[c].name)
			goto fail;
	}
	for (int i = 0; i < npk; i++)
		t->pk[i] = i; /* PK is a column prefix */
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
		free (t->name);
		free (t);
	}
	return NULL;
}

static int meta_read_table (FILE *fp, m_isam_db_t *db)
{
	uint32_t nl;
	char name[META_MAX_NAME + 1];
	char *colnames[M_ISAM_MAX_COLS] = {0};
	m_isam_col_t cols[M_ISAM_MAX_COLS];
	int nread = 0;
	int rc = -1;

	if (read_u32 (fp, &nl) != 0 || nl == 0 || nl > META_MAX_NAME)
		return -1;
	if (fread (name, 1, nl, fp) != nl)
		return -1;
	name[nl] = '\0';
	if (!valid_table_name (name) || db_find_table (db, name))
		return -1;

	uint32_t nc;
	if (read_u32 (fp, &nc) != 0 || nc < 1 || nc > M_ISAM_MAX_COLS)
		return -1;
	for (uint32_t c = 0; c < nc; c++) {
		uint32_t cl;
		uint8_t ty;
		if (read_u32 (fp, &cl) != 0 || cl == 0 || cl > META_MAX_NAME)
			goto out;
		char *cn = malloc ((size_t)cl + 1);
		if (!cn)
			goto out;
		colnames[c] = cn;
		nread = (int)c + 1;
		if (fread (cn, 1, cl, fp) != cl)
			goto out;
		cn[cl] = '\0';
		if (!*cn || fread (&ty, 1, 1, fp) != 1 ||
		    (ty != 'i' && ty != 'd' && ty != 's' && ty != 'z'))
			goto out;
		cols[c].name = cn;
		cols[c].type = (char)ty;
	}
	uint32_t npk;
	if (read_u32 (fp, &npk) != 0 || npk > nc)
		goto out;
	if (!table_alloc (db, name, cols, (int)nc, (int)npk))
		goto out;
	if (table_open_data (db, db->tables[db->ntables - 1]) != 0)
		goto out;
	rc = 0;
out:
	for (int i = 0; i < nread; i++)
		free (colnames[i]);
	return rc;
}

static int meta_load (m_isam_db_t *db)
{
	if (!db || !db->dir)
		return -1;
	char *path = db_path (db->dir, "meta", "");
	if (!path)
		return -1;
	FILE *fp = fopen (path, "rb");
	free (path);
	if (!fp)
		return -1;
	char magic[8];
	uint32_t ver, nt;
	int rc = -1;
	if (fread (magic, 1, 8, fp) != 8 || memcmp (magic, META_MAGIC, 8) != 0)
		goto out;
	if (read_u32 (fp, &ver) != 0 || ver != META_VERSION)
		goto out;
	if (read_u32 (fp, &nt) != 0 || nt > META_MAX_TABLES)
		goto out;
	for (uint32_t i = 0; i < nt; i++)
		if (meta_read_table (fp, db) != 0)
			goto out;
	rc = 0;
out:
	fclose (fp);
	return rc;
}

static m_isam_db_t *db_alloc (void) { return calloc (1, sizeof (m_isam_db_t)); }

m_isam_db_t *m_isam_db_new (void)
{
	char tmpl[] = "/tmp/mlsisamXXXXXX";
	char *dir = mkdtemp (tmpl);
	if (!dir)
		return NULL;
	m_isam_db_t *db = db_alloc ();
	if (!db) {
		rmdir (dir);
		return NULL;
	}
	db->dir = dup_str (dir);
	db->anonymous = 1;
	char *sp = db->dir ? db_path (db->dir, "strings", ".str") : NULL;
	db->pool = sp ? m_strpool_create (sp) : NULL;
	free (sp);
	if (!db->dir || !db->pool || meta_write (db) != 0) {
		m_isam_db_free (db);
		return NULL;
	}
	return db;
}

m_isam_db_t *m_isam_db_create (const char *dir)
{
	if (!dir || !*dir)
		return NULL;
	if (mkdir (dir, 0755) != 0)
		return NULL; /* exists or no permission: never clobber */
	m_isam_db_t *db = db_alloc ();
	if (!db) {
		rmdir (dir);
		return NULL;
	}
	db->dir = dup_str (dir);
	db->anonymous = 0;
	char *sp = db->dir ? db_path (db->dir, "strings", ".str") : NULL;
	db->pool = sp ? m_strpool_create (sp) : NULL;
	free (sp);
	if (!db->dir || !db->pool || meta_write (db) != 0) {
		m_isam_db_free (db);
		return NULL;
	}
	return db;
}

m_isam_db_t *m_isam_db_open (const char *dir)
{
	if (!dir || !*dir)
		return NULL;
	m_isam_db_t *db = db_alloc ();
	if (!db)
		return NULL;
	db->dir = dup_str (dir);
	db->anonymous = 0;
	char *sp = db->dir ? db_path (db->dir, "strings", ".str") : NULL;
	db->pool = sp ? m_strpool_open (sp) : NULL;
	free (sp);
	if (!db->dir || !db->pool || meta_load (db) != 0) {
		m_isam_db_free (db);
		return NULL;
	}
	return db;
}

int m_isam_db_close (m_isam_db_t *db)
{
	m_isam_db_free (db);
	return 0;
}

void m_isam_db_free (m_isam_db_t *db)
{
	if (!db)
		return;
	for (int i = 0; i < db->ntables; i++)
		if (db->tables[i]->data) {
			pager_close (db->tables[i]->data);
			db->tables[i]->data = NULL;
		}
	if (db->dir && db->anonymous) {
		char *m = db_path (db->dir, "meta", "");
		char *mt = db_path (db->dir, "meta", ".tmp");
		if (m) {
			unlink (m);
			free (m);
		}
		if (mt) {
			unlink (mt);
			free (mt);
		}
		for (int i = 0; i < db->ntables; i++) {
			char *p =
				db_path (db->dir, db->tables[i]->name, ".dat");
			if (p) {
				unlink (p);
				free (p);
			}
		}
	}
	for (int i = 0; i < db->ntables; i++)
		table_free (db->tables[i]);
	free (db->tables);
	if (db->pool)
		m_strpool_close (db->pool);
	if (db->dir) {
		if (db->anonymous) {
			char *sp = db_path (db->dir, "strings", ".str");
			if (sp) {
				unlink (sp);
				free (sp);
			}
			rmdir (db->dir);
		}
		free (db->dir);
	}
	free (db);
}

m_isam_table_t *m_isam_create_table (m_isam_db_t *db, const char *name,
				     const m_isam_col_t *cols, int ncols,
				     const char *const *pk_names, int npk)
{
	if (!db || !name || !cols || ncols <= 0 || ncols > M_ISAM_MAX_COLS ||
	    npk < 0 || npk > ncols || (npk > 0 && !pk_names))
		return NULL;
	if (!valid_table_name (name) || db_find_table (db, name))
		return NULL;

	for (int c = 0; c < ncols; c++) {
		if (!cols[c].name || !*cols[c].name ||
		    (cols[c].type != 'i' && cols[c].type != 'd' &&
		     cols[c].type != 's' && cols[c].type != 'z'))
			return NULL;
		for (int j = 0; j < c; j++)
			if (strcmp (cols[j].name, cols[c].name) == 0)
				return NULL;
	}
	for (int i = 0; i < npk; i++) {
		int c = -1;
		for (int j = 0; j < ncols; j++)
			if (strcmp (cols[j].name, pk_names[i]) == 0) {
				c = j;
				break;
			}
		if (c < 0 || c != i) /* PK is a column prefix */
			return NULL;
	}

	m_isam_table_t *t = table_alloc (db, name, cols, ncols, npk);
	if (!t)
		return NULL;
	if (dat_create (db, t) != 0 || table_open_data (db, t) != 0 ||
	    meta_write (db) != 0) {
		/* rollback the just-appended table and its data file */
		db->ntables--;
		if (t->data) {
			pager_close (t->data);
			t->data = NULL;
		}
		dat_unlink (db, name);
		table_free (t);
		meta_write (db); /* best effort */
		return NULL;
	}
	return t;
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
	if (!t || !t->data)
		return 0;

	if (t->idx) {
		cell_t key[M_ISAM_MAX_COLS];
		int64_t dummy;
		build_key (t, t->buf, key);
		if (bpt_get (t->idx, key, &dummy))
			return 0; /* duplicate PK */
	}

	int64_t recno = t->nrecs + 1;
	if (dat_record_rw (t, recno, t->buf, 1, 0, NULL, NULL) != 0)
		return 0;
	t->nrecs = recno;
	t->live++;
	if (dat_write_header (t) != 0) {
		t->nrecs = recno - 1;
		t->live--;
		return 0;
	}
	if (t->idx) {
		cell_t key[M_ISAM_MAX_COLS];
		build_key (t, t->buf, key);
		if (bpt_insert (t->idx, key, recno) != 0) {
			/* rollback: tombstone the slot, keep recno monotonic */
			dat_record_rw (t, recno, t->buf, 1, 1, NULL, NULL);
			t->live--;
			dat_write_header (t);
			return 0;
		}
	}
	memset (t->buf, 0, (size_t)t->ncols * sizeof *t->buf);
	t->buf_set = 0;
	return recno;
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
	if (!t || !out || !t->data || recno < 1 || recno > t->nrecs)
		return 0;
	cell_t cells[M_ISAM_MAX_COLS];
	uint8_t fl;
	if (dat_record_rw ((m_isam_table_t *)t, recno, NULL, 0, 0, cells,
			   &fl) != 0)
		return 0;
	if (fl & 1)
		return 0;
	memcpy (out, cells, (size_t)t->ncols * sizeof *out);
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
	if (!t || !t->data || recno < 1 || recno > t->nrecs)
		return 0;
	cell_t cells[M_ISAM_MAX_COLS];
	uint8_t fl;
	if (dat_record_rw (t, recno, NULL, 0, 0, cells, &fl) != 0)
		return 0;
	if (fl & 1)
		return 0;
	if (t->idx) {
		cell_t key[M_ISAM_MAX_COLS];
		build_key (t, cells, key);
		bpt_del (t->idx, key);
	}
	if (dat_record_rw (t, recno, cells, 1, 1, NULL, NULL) != 0)
		return 0;
	t->live--;
	dat_write_header (t);
	return 1;
}

int64_t m_isam_count (const m_isam_table_t *t) { return t ? t->live : 0; }

int64_t m_isam_nrecs (const m_isam_table_t *t) { return t ? t->nrecs : 0; }

int m_isam_ncols (const m_isam_table_t *t) { return t ? t->ncols : 0; }

int m_isam_npk (const m_isam_table_t *t) { return t ? t->npk : 0; }

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

int m_isam_ntables (const m_isam_db_t *db) { return db ? db->ntables : 0; }

const char *m_isam_table_at (const m_isam_db_t *db, int i)
{
	return db && i >= 0 && i < db->ntables ? db->tables[i]->name : NULL;
}

int m_isam_drop_table (m_isam_db_t *db, const char *name)
{
	if (!db || !name)
		return 0;
	for (int i = 0; i < db->ntables; i++) {
		if (strcmp (db->tables[i]->name, name) == 0) {
			m_isam_table_t *t = db->tables[i];
			if (t->data) {
				pager_close (t->data);
				t->data = NULL;
			}
			dat_unlink (db, t->name);
			table_free (t);
			for (int j = i; j < db->ntables - 1; j++)
				db->tables[j] = db->tables[j + 1];
			db->ntables--;
			meta_write (db);
			return 1;
		}
	}
	return 0;
}

int m_isam_rename_table (m_isam_db_t *db, const char *name, const char *newname)
{
	if (!db || !name || !newname || !valid_table_name (newname))
		return 0;
	if (db_find_table (db, newname))
		return 0;
	m_isam_table_t *t = db_find_table (db, name);
	if (!t)
		return 0;
	char *copy = dup_str (newname);
	if (!copy)
		return 0;
	if (db->dir) {
		char *oldp = db_path (db->dir, t->name, ".dat");
		char *newp = db_path (db->dir, newname, ".dat");
		int ok = oldp && newp && rename (oldp, newp) == 0;
		free (oldp);
		free (newp);
		if (!ok) {
			free (copy);
			return 0;
		}
	}
	free (t->name);
	t->name = copy;
	meta_write (db);
	return 1;
}

int m_isam_copy_table (m_isam_db_t *db, const char *src, const char *dst)
{
	if (!db || !src || !dst)
		return 0;
	m_isam_table_t *s = db_find_table (db, src);
	m_isam_table_t *d = db_find_table (db, dst);
	if (!s || !d || s == d)
		return 0;
	if (d->live != 0)
		return 0; /* destination must be empty */

	/* Pre-validate type compatibility for columns present on both sides. */
	for (int c = 0; c < d->ncols; c++) {
		int sc = m_isam_field (s, d->cols[c].name);
		if (sc >= 0 && s->cols[sc].type != d->cols[c].type)
			return 0;
	}

	for (int64_t r = 1; r <= s->nrecs; r++) {
		cell_t scells[M_ISAM_MAX_COLS];
		uint8_t fl;
		if (dat_record_rw (s, r, NULL, 0, 0, scells, &fl) != 0)
			goto fail;
		if (fl & 1)
			continue;
		for (int c = 0; c < d->ncols; c++) {
			int sc = m_isam_field (s, d->cols[c].name);
			cell_t v = sc >= 0 ? scells[sc] : 0;
			if (m_isam_put (d, d->cols[c].name, v) != 0)
				goto fail;
		}
		if (m_isam_insert (d) <= 0)
			goto fail;
	}
	return 1;
fail:
	table_clear_records (d);
	return 0;
}
