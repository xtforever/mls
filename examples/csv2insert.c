/* csv2insert — turn CSV on stdin into SQL INSERT statements.
 *
 *     printf '1, alice, 3, x, y\n' | ./csv2insert
 *     insert into datax ( id,name,cnt,ref,verify ) values
 * ('1','alice','3','x','y' );
 *
 * The column list lives in a m_var variable system: vs_add() stores the names
 * and vs_string_builder() expands them into the INSERT header ($col_names[*]).
 * Each record is read straight into a variable with vs_read_fields(); the
 * field count is checked via vs_klen() and the value list is produced by
 * vs_sql_string_builder(), which quotes the $'fields[*]' list the SQL way
 * (single quotes doubled: O'Brien -> 'O''Brien'), so no s_implode_q /
 * s_read_fields are needed. An empty line yields one empty field and is
 * skipped, and at EOF vs_read_fields() leaves 0 values, ending the loop.
 *
 * Everything (variable system included) is a `tmls` handle, auto-freed at block
 * end; vs_free() is just m_free() and frees the variables recursively.
 *
 * Build via examples/makefile (`make csv2insert.exe`), or manually:
 *     cc -std=c11 -Ilib examples/csv2insert.c lib/m_var.c lib/mls_base.c \
 *        lib/mls_ext.c lib/m_tool.c lib/list.c lib/table.c \
 *        -o csv2insert -lpthread -lm -ldl
 *
 * ponytail: naive split on ',' — no quoted-field / escaped-comma handling.
 */
#include "../lib/m_tool.h"
#include "../lib/m_var.h"
#include "../lib/mls.h"

#include <stdio.h>

/* Column list: single source of truth for the INSERT header and the check. */
static const char *cols[] = {"id", "name", "cnt", "ref", "verify"};
#define NCOLS ((int)ALEN (cols))

void csv2insert (void)
{
	tmls vset = vs_add (0, "col_names", cols, NCOLS);

	/* header: unquoted, comma separated list */
	tmls buf = vs_string_builder (
		0, vset, "insert into datax ( $col_names[*] ) values (");
	int len = s_strlen (buf);

	for (;;) {
		int fields = vs_read_fields (vset, "fields", stdin, ",");
		if (vs_klen (fields) == 0)
			break; /* EOF */

		/* empty record -> 1 field, so this also drops blanks */
		if (vs_klen (fields) != NCOLS ||
		    (NCOLS == 1 && *vs_kget (fields, 1) == 0))
			continue; /* wrong format: skip silently */

		/* SQL-quoted list via expansion ('' doubling), then back to
		 * header-only. The append overwrote the header's NUL, so put
		 * it back. */
		vs_sql_string_builder (buf, vset, "$'fields[*]' );");
		s_puts (buf);
		m_setlen (buf, len);
	}
}

int main (void)
{
	m_init ();
	csv2insert ();
	m_destruct ();
	return 0;
}
