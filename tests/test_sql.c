/* Self-check for the lean m_sql builder (terse columns -> SELECT/INSERT).
 * Links m_sql.c + m_var.c + m_tool.c; no database. */
#include "m_sql.h"
#include "m_tool.h"
#include "m_var.h"
#include "mls.h"
#include <stdio.h>
#include <string.h>
#undef ASSERT
#include "greatest.h"

SUITE (mls_m_sql_suite);

TEST test_sql_cols (void)
{
	int l1 = sql_cols ("id name qty");
	ASSERT_EQ (3, (int)m_len (l1));
	ASSERT_STR_EQ ("id", STR (l1, 0));
	ASSERT_STR_EQ ("name", STR (l1, 1));
	ASSERT_STR_EQ ("qty", STR (l1, 2));

	/* comma form gives the same list */
	int l2 = sql_cols ("id,name,qty");
	ASSERT_EQ (3, (int)m_len (l2));
	ASSERT_STR_EQ ("qty", STR (l2, 2));

	/* runs and leading/trailing separators are ignored */
	int l3 = sql_cols ("  id ,  name   qty  ");
	ASSERT_EQ (3, (int)m_len (l3));
	ASSERT_STR_EQ ("id", STR (l3, 0));
	ASSERT_STR_EQ ("name", STR (l3, 1));
	ASSERT_STR_EQ ("qty", STR (l3, 2));

	int l4 = sql_cols ("");
	ASSERT_EQ (0, (int)m_len (l4));

	m_free (l1);
	m_free (l2);
	m_free (l3);
	m_free (l4);
	PASS ();
}

TEST test_sql_select (void)
{
	int h = sql_select ("items", "id name qty", NULL);
	ASSERT_STR_EQ ("SELECT id, name, qty FROM items", m_str (h));
	m_free (h);

	h = sql_select ("items", "name,qty", "qty > 3");
	ASSERT_STR_EQ ("SELECT name, qty FROM items WHERE qty > 3", m_str (h));
	m_free (h);

	/* empty where is omitted */
	h = sql_select ("items", "id", "");
	ASSERT_STR_EQ ("SELECT id FROM items", m_str (h));
	m_free (h);
	PASS ();
}

TEST test_sql_insert (void)
{
	int vs = vs_init ();
	static const char *const vals[] = {"apple", "3", "O'Brien"};
	vs_add (vs, "vals", vals, 3);

	int h = sql_insert (vs, "items", "name qty note");
	ASSERT_STR_EQ ("INSERT INTO items (name, qty, note) VALUES ( "
		       "'apple','3','O''Brien' )",
		       m_str (h));
	m_free (h);
	vs_free (vs);
	PASS ();
}

SUITE (mls_m_sql_suite)
{
	RUN_TEST (test_sql_cols);
	RUN_TEST (test_sql_select);
	RUN_TEST (test_sql_insert);
}

GREATEST_MAIN_DEFS ();

int main (int argc, char **argv)
{
	m_init ();
	GREATEST_MAIN_BEGIN ();
	RUN_SUITE (mls_m_sql_suite);
	GREATEST_MAIN_END ();
}
