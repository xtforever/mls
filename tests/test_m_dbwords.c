#include "m_dbwords.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void run (m_forth_t *f, const char *src, const char *out)
{
	m_forth_output_clear (f);
	int rc = m_forth_eval (f, src);
	if (rc != 0)
		fprintf (stderr, "eval [%s] -> %d: %s\n", src, rc,
			 m_forth_error_msg (f));
	assert (rc == 0);
	if (strcmp (m_forth_output (f), out) != 0)
		fprintf (stderr, "output [%s] expected [%s]\n",
			 m_forth_output (f), out);
	assert (strcmp (m_forth_output (f), out) == 0);
}

int main (void)
{
	m_isam_db_t *db = m_isam_db_new ();
	assert (db);

	m_isam_col_t pcols[] = {{"id", 'i'}, {"name", 's'}, {"age", 'i'}};
	const char *ppk[] = {"id"};
	assert (m_isam_create_table (db, "people", pcols, 3, ppk, 1));

	m_isam_col_t ccols[] = {{"a", 'i'}, {"b", 'i'}, {"val", 'i'}};
	const char *cpk[] = {"a", "b"};
	assert (m_isam_create_table (db, "pos", ccols, 3, cpk, 2));

	m_isam_col_t ucols[] = {{"name", 's'}, {"age", 'i'}};
	const char *upk[] = {"name"};
	assert (m_isam_create_table (db, "users", ucols, 2, upk, 1));

	m_forth_t *f = m_forth_new ();
	assert (f);
	m_dbwords_t *dw = m_dbwords_new (f, db);
	assert (dw);

	/* 0.12 done criterion: INSERT -> SEEK -> FETCH -> S" John" = */
	run (f,
	     "Z\" people\" DB-TABLE DROP "
	     "Z\" id\" 1 DB-PUT Z\" name\" S\" John\" DB-PUT "
	     "Z\" age\" 30 DB-PUT DB-INSERT DROP "
	     "Z\" id\" 2 DB-PUT Z\" name\" S\" Jane\" DB-PUT "
	     "Z\" age\" 31 DB-PUT DB-INSERT DROP "
	     "1 DB-SEEK DROP DB-FETCH DROP "
	     "Z\" name\" DB-GET S\" John\" = .",
	     "-1 ");
	run (f, "Z\" age\" DB-GET .", "30 ");
	run (f, "Z\" age\" DB-FIELD .", "2 ");
	run (f, "Z\" people\" DB-COUNT .", "2 ");
	run (f, "Z\" in-memory\" DB-OPEN .", "-1 ");

	/* Delete + missing seek. */
	run (f, "1 DB-SEEK DROP DB-DELETE .", "-1 ");
	run (f, "1 DB-SEEK .", "0 ");
	run (f, "Z\" people\" DB-COUNT .", "1 ");
	run (f, "Z\" nope\" DB-TABLE .", "0 ");

	/* Composite PK: keys are pushed in column order. */
	run (f,
	     "Z\" pos\" DB-TABLE DROP "
	     "Z\" a\" 1 DB-PUT Z\" b\" 2 DB-PUT Z\" val\" 20 DB-PUT "
	     "DB-INSERT DROP "
	     "1 2 DB-SEEK DROP DB-FETCH DROP Z\" val\" DB-GET .",
	     "20 ");

	/* String PK seek accepts the VM string id and converts it. */
	run (f,
	     "Z\" users\" DB-TABLE DROP "
	     "Z\" name\" S\" alice\" DB-PUT Z\" age\" 30 DB-PUT "
	     "DB-INSERT DROP "
	     "S\" alice\" DB-SEEK DROP DB-FETCH DROP Z\" age\" DB-GET .",
	     "30 ");

	/* Unknown column raises a Forth error. */
	m_forth_output_clear (f);
	assert (m_forth_eval (f, "Z\" nope\" 1 DB-PUT") != 0);

	/* DB-STATS placeholder. */
	run (f, "DB-STATS . .", "0 0 ");

	m_dbwords_free (dw);
	m_forth_free (f);
	m_isam_db_free (db);

	puts ("m_dbwords selftest: OK");
	return 0;
}
