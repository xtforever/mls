#ifndef M_ISAM_H
#define M_ISAM_H

#include <stdint.h>

#include "arena.h" /* cell_t */

#define M_ISAM_MAX_COLS 16

/* ISAM table core (Phase 2 + M1.1 schema persistence).
   The schema is persisted in <dir>/meta and <dir>/<table>.dat (4096-byte
   header page). Records and the primary index still live in RAM; paged
   records + the persistent string pool are the next M1.1 steps.
   recno is monotonic, never reused; delete = tombstone.
   String columns ('s'/'z') store a per-database string ID. */
typedef struct m_isam_db m_isam_db_t;
typedef struct m_isam_table m_isam_table_t;

typedef struct {
	const char *name;
	char type; /* 'i','d','s','z' */
} m_isam_col_t;

m_isam_db_t *m_isam_db_new (void);		 /* anonymous temp-dir DB */
m_isam_db_t *m_isam_db_create (const char *dir); /* mkdir, empty schema */
m_isam_db_t *m_isam_db_open (const char *dir);	 /* validate + reload */
int m_isam_db_close (m_isam_db_t *db);		 /* flush + free */
void m_isam_db_free (m_isam_db_t *db);		 /* NULL-safe close */

m_isam_table_t *m_isam_create_table (m_isam_db_t *db, const char *name,
				     const m_isam_col_t *cols, int ncols,
				     const char *const *pk_names, int npk);
m_isam_table_t *m_isam_table (m_isam_db_t *db, const char *name);

int m_isam_field (const m_isam_table_t *t, const char *name); /* -1 miss */
int m_isam_put (m_isam_table_t *t, const char *col, cell_t value);

int64_t m_isam_insert (m_isam_table_t *t); /* recno > 0, 0 on duplicate/error */
int m_isam_seek (const m_isam_table_t *t, const cell_t *key, int nkey,
		 int64_t *recno);
int m_isam_fetch (const m_isam_table_t *t, int64_t recno, cell_t *out);
int m_isam_get (const m_isam_table_t *t, int64_t recno, const char *col,
		cell_t *out);
int m_isam_delete (m_isam_table_t *t, int64_t recno); /* 1 deleted */
int64_t m_isam_count (const m_isam_table_t *t);
int64_t m_isam_nrecs (const m_isam_table_t *t);

int m_isam_ncols (const m_isam_table_t *t);
int m_isam_npk (const m_isam_table_t *t);
const char *m_isam_colname (const m_isam_table_t *t, int c);
char m_isam_coltype (const m_isam_table_t *t, int c);

uint32_t m_isam_intern (m_isam_table_t *t, const char *s);
const char *m_isam_resolve (const m_isam_table_t *t, uint32_t id);

/* Schema / DDL helpers (in-memory core; persistent meta is M1.1, §11a.7). */
int m_isam_ntables (const m_isam_db_t *db);
const char *m_isam_table_at (const m_isam_db_t *db, int i); /* NULL if oob */
int m_isam_drop_table (m_isam_db_t *db, const char *name);  /* 1 removed */
int m_isam_rename_table (m_isam_db_t *db, const char *name,
			 const char *newname); /* 1 renamed */
/* Copy live records from src into the (empty) dst, mapping columns by name;
   missing source columns become 0, type mismatches and duplicate PKs fail. */
int m_isam_copy_table (m_isam_db_t *db, const char *src, const char *dst);

#endif
