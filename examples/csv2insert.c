/* csv2insert — turn CSV on stdin into SQL INSERT statements.
 *
 *     printf '1, alice, 3, x, y\n' | ./csv2insert
 *     insert into datax ( id, name, cnt, ref, verify ) values ( '1', 'alice',
 * '3', 'x', 'y');
 *
 * The column list is wrapped into mls string handles with m_wrapstrhandles()
 * and imploded into the header with s_implode_q(); each CSV record is read with
 * s_read_fields() and its fields are quoted and imploded the same way. Records
 * whose field count does not match the column list are skipped silently (an
 * empty record yields one empty field, so blank lines are dropped too). At end
 * of input s_read_fields() yields an empty list (0 fields), ending the loop.
 *
 * Single-header build (needs mls_core.h next to the project root; it is the
 * release/single-threaded preset with the config baked in):
 *     cc -std=c11 examples/csv2insert.c -o csv2insert -lpthread -lm -ldl
 *
 * ponytail: naive split on ',' — no quoted-field / escaped-comma handling.
 */
#define MLS_CORE_IMPLEMENTATION
#include "../mls_core.h"

/* Column list: single source of truth for the INSERT header and the check. */
static const char *cols[] = {"id", "name", "cnt", "ref", "verify"};
#define NCOLS ((int)ALEN (cols))

int main (void)
{
	m_init ();

	/* cols[] -> list of handles -> "id, name, cnt, ref, verify" */
	int sep = s_dup (", ");
	int col_list = m_wrapstrhandles (cols, NCOLS);
	int names = s_implode_q (0, col_list, sep, 0);
	int head = s_printf (0, -1, "insert into datax ( %s ) values (",
			     m_str (names));

	int fields = 0;
	int sql = 0;

	for (;;) {
		fields = s_read_fields (fields, stdin, ",", 1);
		if (m_len (fields) == 0)
			break; /* end of input */

		/* empty record -> 1 field, so this also drops blanks */
		if (m_len (fields) != NCOLS ||
		    (NCOLS == 1 && CHAR (INT (fields, 0), 0) == 0))
			continue; /* wrong format: skip silently */

		/* quote and join the fields: 1,2,3 -> '1', '2', '3' */
		int values = s_implode_q (0, fields, sep, '\'');
		m_clear (sql);
		sql = s_printf (sql, -1, "%s %s);", m_str (head),
				m_str (values));
		s_puts (sql);
		m_free (values);
	}

	m_free (sql);
	m_free (head);
	m_free (names);
	m_free (col_list);
	m_free (sep);
	m_destruct ();
	return 0;
}
