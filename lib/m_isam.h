#ifndef M_ISAM_H
#define M_ISAM_H

#include <stdint.h>

#include "arena.h" /* cell_t */

#define M_ISAM_MAX_COLS 16

/* In-memory ISAM table core (Phase 2).
   ponytail: records and the primary index live in RAM; persistence via pager
   is the M1.1 upgrade. recno is monotonic, never reused; delete = tombstone.
   String columns ('s'/'z') store a per-database string ID. */
typedef struct m_isam_db m_isam_db_t;
typedef struct m_isam_table m_isam_table_t;

typedef struct {
	const char *name;
	char type; /* 'i','d','s','z' */
} m_isam_col_t;

m_isam_db_t *m_isam_db_new (void);
void m_isam_db_free (m_isam_db_t *db);

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

#endif
