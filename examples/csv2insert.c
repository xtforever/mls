/* csv2insert — turn CSV on stdin into SQL INSERT statements.
 *
 *     printf '1, alice, 3, x, y\n' | ./csv2insert
 *     insert into datax ( id, name, cnt, ref, verify ) values ( '1', 'alice',
 * '3', 'x', 'y');
 *
 * Each call to s_read_fields() reads one CSV record and splits it on ',' with
 * surrounding spaces stripped. Records whose field count does not match the
 * column list are skipped silently (an empty record yields one empty field,
 * so blank lines are dropped too).
 *
 * Single-header build (needs mls_core.h next to the project root):
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

	int head = s_printf (0, -1, "insert into datax (");
	for (int i = 0; i < NCOLS; i++)
		s_printf (head, -1, "%s%s", i ? ", " : " ", cols[i]);
	s_printf (head, -1, " ) values (");

	int fields = 0;
	int sql = 0;

	for (;;) {
		int h = s_read_fields (fields, stdin, ",", 1);
		if (h == EOF)
			break;
		fields = h; /* first call allocates, later calls reuse */

		/* empty record -> 1 field, so this also drops blanks */
		if ((int)m_len (fields) != NCOLS)
			continue; /* wrong format: skip silently */

		m_clear (sql);
		sql = s_app (sql, m_str (head), NULL);
		for (int i = 0; i < NCOLS; i++)
			s_printf (sql, -1, "%s'%s'", i ? ", " : " ",
				  m_str (INT (fields, i)));
		s_app (sql, ");", NULL);

		puts (m_str (sql));
	}

	m_free (sql);
	m_free (head);
	m_free (fields);
	m_destruct ();
	return 0;
}
