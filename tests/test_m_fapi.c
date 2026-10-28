#include "m_fapi.h"

#include "m_isam.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main (void)
{
	m_isam_db_t *db = m_isam_db_new ();
	assert (db);
	m_isam_col_t cols[] = {{"id", 'i'}, {"name", 's'}, {"age", 'i'}};
	const char *pk[] = {"id"};
	m_isam_table_t *t = m_isam_create_table (db, "people", cols, 3, pk, 1);
	assert (t);

	uint32_t john = m_isam_intern (t, "John");
	assert (m_isam_put (t, "id", 1) == 0);
	assert (m_isam_put (t, "name", john) == 0);
	assert (m_isam_put (t, "age", 30) == 0);
	assert (m_isam_insert (t) == 1);

	m_engine_t *e = m_engine_new (db);
	assert (e);
	assert (fdb_set_default_engine (e) == 0);
	int conn = fdb_open ("./mls-db.sock");
	assert (conn > 0);

	/* fdb_select over one Forth round-trip -> parsed resultset. */
	int rs = fdb_select (conn, "db", "people", "name", "John");
	assert (rs > 0);
	assert (fdb_result_cols (rs) == 3);
	assert (strcmp (fdb_result_colname (rs, 0), "id") == 0);
	assert (strcmp (fdb_result_colname (rs, 1), "name") == 0);
	assert (strcmp (fdb_result_colname (rs, 2), "age") == 0);
	assert (strcmp (fdb_result_coltype (rs, 0), "i") == 0);
	assert (strcmp (fdb_result_coltype (rs, 1), "s") == 0);
	assert (fdb_result_rows (rs) == 1);
	assert (fdb_result_int (rs, 0, 0) == 1);
	assert (strcmp (fdb_result_str (rs, 0, 1), "John") == 0);
	assert (fdb_result_int (rs, 0, 2) == 30);
	assert (fdb_result_int (rs, 0, 1) == 0);    /* not an int cell */
	assert (fdb_result_str (rs, 0, 0) == NULL); /* not a string cell */
	assert (fdb_result_int (rs, 9, 9) == 0);
	assert (fdb_result_str (rs, 9, 9) == NULL);
	fdb_result_free (rs);

	int rs0 = fdb_select (conn, "db", "people", "name", "Nobody");
	assert (rs0 > 0 && fdb_result_rows (rs0) == 0);
	fdb_result_free (rs0);

	/* fdb_insert (single-field helper) and delete. */
	assert (fdb_insert (conn, "db", "people", "id", "2") == 2);
	rs = fdb_select (conn, "db", "people", "id", "2");
	assert (rs > 0 && fdb_result_rows (rs) == 1);
	assert (fdb_result_int (rs, 0, 0) == 2);
	assert (strcmp (fdb_result_str (rs, 0, 1), "") == 0);
	fdb_result_free (rs);
	assert (fdb_delete (conn, "db", "people", "id", "2") == 1);
	rs = fdb_select (conn, "db", "people", "id", "2");
	assert (rs > 0 && fdb_result_rows (rs) == 0);
	fdb_result_free (rs);

	/* Duplicate primary key: insert fails (0). */
	assert (fdb_insert (conn, "db", "people", "id", "1") == 0);

	fdb_close (conn);
	assert (fdb_open_engine (e) > 0);
	assert (fdb_open_engine (NULL) == -1);
	assert (fdb_select (99, "db", "people", "id", "1") == 0);
	assert (fdb_result_cols (0) == 0);
	assert (fdb_result_colname (0, 0) == NULL);
	fdb_result_free (0);

	fdb_set_default_engine (NULL);
	assert (fdb_open ("nowhere") == -1);

	m_engine_free (e);
	m_isam_db_free (db);

	puts ("m_fapi selftest: OK");
	return 0;
}
