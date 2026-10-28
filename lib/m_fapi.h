#ifndef M_FAPI_H
#define M_FAPI_H

#include "m_engine.h"

/* Stable client surface (fdb_*). In the monolith these calls go through
   m_engine/query_exec; m_fclient replaces only the backend later. */
int fdb_set_default_engine (m_engine_t *e);
int fdb_open (const char *socket_path); /* uses the default engine */
int fdb_open_engine (m_engine_t *e);	/* monolith explicit binding */
void fdb_close (int conn);

int fdb_select (int conn, const char *db, const char *table, const char *field,
		const char *value);
int fdb_insert (int conn, const char *db, const char *table, const char *field,
		const char *value);
int fdb_delete (int conn, const char *db, const char *table, const char *field,
		const char *value);

int fdb_result_cols (int rs);
const char *fdb_result_colname (int rs, int i);
const char *fdb_result_coltype (int rs, int i);
int fdb_result_rows (int rs);
long long fdb_result_int (int rs, int row, int col);
const char *fdb_result_str (int rs, int row, int col);
void fdb_result_free (int rs);

#endif
