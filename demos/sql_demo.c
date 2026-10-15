/* sql_demo — terse column specs into SELECT/INSERT via m_sql.
 *
 *     ./sql_demo
 *     SELECT id, name, qty FROM items
 *     SELECT name, qty FROM items WHERE qty > 3
 *     INSERT INTO items (name, qty, note) VALUES ( 'apple','3','O''Brien' )
 *
 * The column spec is just "x y z" (or "x,y,z"); sql_select()/sql_insert()
 * expand it with the mls string helpers. INSERT values live in a variable
 * system and are SQL-quoted by vs_sql_string_builder() (O'Brien ->
 * 'O''Brien'), so there is no hand-written escaping. No database is touched:
 * this is pure string building.
 *
 * Build via examples/makefile (`make sql_demo.exe`), or manually:
 *     cc -std=c11 -D_GNU_SOURCE -Ilib sql_demo.c lib/mls_base.c lib/mls_ext.c \
 *        lib/m_tool.c lib/m_var.c lib/m_sql.c -o sql_demo -lpthread -lm -ldl
 */
#include "../lib/m_sql.h"
#include "../lib/m_tool.h"
#include "../lib/m_var.h"

#include <stdio.h>

/* tmls handles auto-free at the end of this block, i.e. before m_destruct(). */
static void demo (void)
{
	/* terse column spec -> SELECT */
	tmls q1 = sql_select ("items", "id name qty", NULL);
	printf ("%s\n", m_str (q1));

	tmls q2 = sql_select ("items", "name,qty", "qty > 3");
	printf ("%s\n", m_str (q2));

	/* the same spec -> INSERT; values come from the variable system */
	tmls vs = vs_init ();
	static const char *const vals[] = {"apple", "3", "O'Brien"};
	vs_add (vs, "vals", vals, 3);

	tmls q3 = sql_insert (vs, "items", "name qty note");
	printf ("%s\n", m_str (q3));
}

int main (void)
{
	m_init ();
	demo ();
	m_destruct ();
	return 0;
}
