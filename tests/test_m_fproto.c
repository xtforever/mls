#include "m_fproto.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main (void)
{
	m_fproto_t *w = m_fproto_new ();
	assert (w);

	m_fproto_result (w, 3);
	m_fproto_col (w, "id", "i");
	m_fproto_col (w, "name", "s");
	m_fproto_col (w, "age", "i");
	m_fproto_count (w, 2);
	m_fproto_row (w);
	m_fproto_int (w, 7);
	m_fproto_str (w, "John");
	m_fproto_int (w, 30);
	m_fproto_row (w);
	m_fproto_int (w, 8);
	m_fproto_str (w, "a\"b\\c\nd\te");
	m_fproto_zstr (w, "zz");
	m_fproto_end (w);

	const char *data = m_fproto_data (w);
	assert (strncmp (data, "RESULT COLS 3", 13) == 0);
	assert (strstr (data, "COL Z\"id\" Z\"i\""));
	assert (strstr (data, "COUNT 2"));
	assert (strstr (data, "ROW 7 S\"John\" 30"));
	assert (strstr (data, "S\"a\\\"b\\\\c\\nd\\te\""));

	m_fproto_result_t *r = m_fproto_parse (data);
	assert (r);
	assert (r->ncols == 3 && r->nrows == 2 && r->has_count &&
		r->count == 2);
	assert (strcmp (r->cols[0].name, "id") == 0);
	assert (strcmp (r->cols[0].type, "i") == 0);
	assert (strcmp (r->cols[1].name, "name") == 0);
	assert (strcmp (r->cols[2].name, "age") == 0);

	assert (r->values[0].type == M_FPROTO_INT && r->values[0].i == 7);
	assert (r->values[1].type == M_FPROTO_STR);
	assert (strcmp (r->values[1].s, "John") == 0);
	assert (r->values[2].type == M_FPROTO_INT && r->values[2].i == 30);
	assert (r->values[3].i == 8);
	assert (r->values[4].type == M_FPROTO_STR);
	assert (strcmp (r->values[4].s, "a\"b\\c\nd\te") == 0);
	assert (r->values[5].type == M_FPROTO_ZSTR);
	assert (strcmp (r->values[5].s, "zz") == 0);
	m_fproto_result_free (r);
	m_fproto_free (w);

	/* No COUNT, one column, zero rows and zero columns. */
	w = m_fproto_new ();
	m_fproto_result (w, 1);
	m_fproto_col (w, "x", "i");
	m_fproto_row (w);
	m_fproto_int (w, -42);
	m_fproto_end (w);
	r = m_fproto_parse (m_fproto_data (w));
	assert (r && r->nrows == 1 && !r->has_count);
	assert (r->values[0].i == -42);
	m_fproto_result_free (r);
	m_fproto_free (w);

	w = m_fproto_new ();
	m_fproto_result (w, 0);
	m_fproto_end (w);
	r = m_fproto_parse (m_fproto_data (w));
	assert (r && r->ncols == 0 && r->nrows == 0);
	m_fproto_result_free (r);
	m_fproto_free (w);

	/* Malformed inputs. */
	assert (m_fproto_parse ("garbage") == NULL);
	assert (m_fproto_parse ("RESULT COLS 1 COL Z\"a\" Z\"i\" ROW 1") ==
		NULL);
	assert (m_fproto_parse (
			"RESULT COLS 1 COL Z\"a\" Z\"i\" ROW S\"x END") ==
		NULL);
	assert (m_fproto_parse ("RESULT COLS 2 COL Z\"a\" Z\"i\" ROW 1 END") ==
		NULL);
	assert (m_fproto_parse ("RESULT COLS 1 COL Z\"a\" Z\"i\" END extra") ==
		NULL);
	assert (m_fproto_parse (NULL) == NULL);

	m_fproto_result_free (NULL);
	m_fproto_free (NULL);

	puts ("m_fproto selftest: OK");
	return 0;
}
