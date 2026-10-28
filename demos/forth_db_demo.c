#include "m_fapi.h"
#include "m_isam.h"
#include "mls.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static int open_database (const char *path) { return fdb_open (path); }

static int select_user (int conn, const char *table, const char *field,
			const char *value)
{
	return fdb_select (conn, "users", table, field, value);
}

static void print_result (int rs)
{
	int cols = fdb_result_cols (rs);
	int rows = fdb_result_rows (rs);
	for (int c = 0; c < cols; c++)
		printf ("%s%s", c ? "\t" : "", fdb_result_colname (rs, c));
	printf ("\n");
	for (int r = 0; r < rows; r++) {
		for (int c = 0; c < cols; c++) {
			if (c)
				printf ("\t");
			const char *ty = fdb_result_coltype (rs, c);
			if (ty && (ty[0] == 's' || ty[0] == 'z'))
				printf ("%s", fdb_result_str (rs, r, c));
			else
				printf ("%lld", fdb_result_int (rs, r, c));
		}
		printf ("\n");
	}
}

static void close_database (int conn) { fdb_close (conn); }

int main (void)
{
	m_init ();

	m_isam_db_t *db = m_isam_db_new ();
	assert (db);
	m_isam_col_t cols[] = {{"id", 'i'}, {"name", 's'}, {"age", 'i'}};
	const char *pk[] = {"id"};
	m_isam_table_t *t = m_isam_create_table (db, "people", cols, 3, pk, 1);
	assert (t);

	uint32_t john = m_isam_intern (t, "John");
	assert (m_isam_put (t, "id", 7) == 0);
	assert (m_isam_put (t, "name", john) == 0);
	assert (m_isam_put (t, "age", 30) == 0);
	assert (m_isam_insert (t) == 1);

	m_engine_t *e = m_engine_new (db);
	assert (e);
	fdb_set_default_engine (e);

	int dbh = open_database ("./mls-db.sock");
	assert (dbh > 0);
	int rs = select_user (dbh, "people", "name", "John");
	assert (rs > 0);
	assert (fdb_result_cols (rs) == 3);
	assert (fdb_result_rows (rs) == 1);
	assert (strcmp (fdb_result_colname (rs, 1), "name") == 0);
	assert (strcmp (fdb_result_str (rs, 0, 1), "John") == 0);
	print_result (rs);

	fdb_result_free (rs);
	close_database (dbh);
	fdb_set_default_engine (NULL);
	m_engine_free (e);
	m_isam_db_free (db);
	m_destruct ();
	return 0;
}
