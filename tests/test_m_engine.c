#include "m_engine.h"

#include "m_forth.h"
#include "m_fproto.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main (void)
{
	m_isam_db_t *db = m_isam_db_new ();
	assert (db);
	m_isam_col_t cols[] = {{"id", 'i'}, {"name", 's'}, {"age", 'i'}};
	const char *pk[] = {"id"};
	assert (m_isam_create_table (db, "people", cols, 3, pk, 1));

	m_engine_t *e = m_engine_new (db);
	assert (e);

	/* Script -> RESULT script. */
	const char *resp = m_engine_exec (
		e, "Z\" people\" DB-TABLE DROP "
		   "Z\" id\" 1 DB-PUT Z\" name\" S\" John\" DB-PUT "
		   "Z\" age\" 30 DB-PUT DB-INSERT DROP "
		   "1 DB-SEEK DROP DB-FETCH DROP");
	assert (m_engine_error (e) == 0);
	assert (strstr (resp, "RESULT COLS 3"));
	assert (strstr (resp, "COL Z\"id\" Z\"i\""));
	assert (strstr (resp, "COL Z\"name\" Z\"s\""));
	assert (strstr (resp, "COUNT 1"));
	assert (strstr (resp, "ROW 1 S\"John\" 30"));

	/* Parse the answer and re-eval its content in a fresh VM: handles are
	   reproduced from content, never from a foreign ID. */
	m_fproto_result_t *r = m_fproto_parse (resp);
	assert (r);
	assert (r->ncols == 3 && r->nrows == 1 && r->has_count &&
		r->count == 1);
	assert (strcmp (r->cols[1].name, "name") == 0);
	assert (r->values[1].type == M_FPROTO_STR);
	assert (strcmp (r->values[1].s, "John") == 0);

	m_forth_t *f2 = m_forth_new ();
	assert (f2);
	uint32_t from_answer = m_forth_intern (f2, r->values[1].s);
	m_forth_output_clear (f2);
	assert (m_forth_eval (f2, "S\" John\"") == 0);
	uint32_t from_script = (uint32_t)m_forth_pop (f2);
	assert (from_answer == from_script);
	assert (strcmp (m_forth_resolve (f2, from_answer), "John") == 0);
	m_forth_free (f2);
	m_fproto_result_free (r);

	/* Delete -> live count drops; no row is emitted. */
	resp = m_engine_exec (
		e, "Z\" people\" DB-TABLE DROP 1 DB-SEEK DROP DB-DELETE DROP");
	assert (m_engine_error (e) == 0);
	assert (strstr (resp, "COUNT 0"));
	assert (strstr (resp, "END"));
	assert (!strstr (resp, "ROW"));

	/* Errors are returned as ERROR scripts. */
	resp = m_engine_exec (e, "Z\" nope\" 1 DB-PUT");
	assert (m_engine_error (e) != 0);
	assert (strncmp (resp, "ERROR ", 6) == 0);
	assert (strstr (resp, "Z\""));

	/* Selecting an unknown table yields an empty result set. */
	resp = m_engine_exec (e, "Z\" nope\" DB-TABLE DROP");
	assert (m_engine_error (e) == 0);
	assert (strcmp (resp, "RESULT COLS 0 END") == 0);

	assert (m_engine_new (NULL) == NULL);
	m_engine_free (e);
	m_engine_free (NULL);
	m_isam_db_free (db);

	puts ("m_engine selftest: OK");
	return 0;
}
