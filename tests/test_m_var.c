/* Self-check for the standalone m_var module (vs_ prefix).
 * Mirrors test_se_string.c but links m_var.c only; no table.c involved. */
#include "m_var.h"
#include "mls.h"
#include <stdio.h>
#include <string.h>
#undef ASSERT
#include "greatest.h"

SUITE (mls_m_var_suite);

TEST test_vs_basic (void)
{
	int vs = vs_init ();
	vs_set (vs, "name", "John", 1);
	vs_set (vs, "age", "30", 1);

	ASSERT_STR_EQ ("Hello John, you are 30 years old.",
		       vs_string (vs, "Hello $name, you are $age years old."));

	vs_free (vs);
	PASS ();
}

TEST test_vs_get_len (void)
{
	int vs = vs_init ();
	ASSERT_EQ (-1, vs_lookup (vs, ""));
	vs_set (vs, "items", "a", 1);
	vs_set (vs, "items", "b", 2);

	ASSERT_EQ (2, vs_len (vs, "items"));
	ASSERT_STR_EQ ("a", vs_get (vs, "items", 1));
	ASSERT_STR_EQ ("b", vs_get (vs, "items", 2));
	ASSERT_STR_EQ ("", vs_get (vs, "items", 9));

	vs_free (vs);
	PASS ();
}

TEST test_vs_indexed (void)
{
	int vs = vs_init ();
	vs_set (vs, "items", "apple", 1);
	vs_set (vs, "items", "banana", 2);
	vs_set (vs, "items", "cherry", 3);

	ASSERT_STR_EQ ("First: apple", vs_string (vs, "First: $items[0]"));
	ASSERT_STR_EQ ("Second: banana", vs_string (vs, "Second: $items[1]"));
	ASSERT_STR_EQ ("Third: cherry", vs_string (vs, "Third: $items[2]"));

	vs_free (vs);
	PASS ();
}

TEST test_vs_star (void)
{
	int vs = vs_init ();
	vs_set (vs, "list", "a", 1);
	vs_set (vs, "list", "b", 2);
	vs_set (vs, "list", "c", 3);

	ASSERT_STR_EQ ("All: a,b,c", vs_string (vs, "All: $list[*]"));

	vs_free (vs);
	PASS ();
}

TEST test_vs_quoted (void)
{
	int vs = vs_init ();
	vs_set (vs, "val", "it's a \"test\"", 1);

	ASSERT_STR_EQ ("Quoted: 'it\\'s a \\\"test\\\"'",
		       vs_string (vs, "Quoted: $'val'"));

	vs_free (vs);
	PASS ();
}

TEST test_vs_escaped_dollar (void)
{
	int vs = vs_init ();
	vs_set (vs, "var", "val", 1);

	ASSERT_STR_EQ ("Price: $100", vs_string (vs, "Price: \\$100"));
	ASSERT_STR_EQ ("Value: val", vs_string (vs, "Value: $var"));

	vs_free (vs);
	PASS ();
}

TEST test_vs_multiple_rows (void)
{
	int vs = vs_init ();
	vs_set (vs, "col", "row0", 1);
	vs_set (vs, "col", "row1", 2);

	vs_exp_t se;
	vs_exp_init (&se);
	vs_exp_parse (&se, "Value: $col");

	ASSERT_STR_EQ ("Value: row0", vs_expand (&se, vs, 0));
	ASSERT_STR_EQ ("Value: row1", vs_expand (&se, vs, 1));

	vs_exp_free (&se);
	vs_free (vs);
	PASS ();
}

TEST test_vs_clr_remove (void)
{
	int vs = vs_init ();
	vs_set (vs, "k", "v1", 1);
	vs_set (vs, "k", "v2", 2);

	vs_clr (vs, "k");
	ASSERT_EQ (0, vs_len (vs, "k"));
	/* name survives a clear: expansion resolves to nothing, not garbage */
	ASSERT_STR_EQ ("[]", vs_string (vs, "[$k]"));

	vs_set (vs, "k", "again", 1);
	ASSERT_EQ (1, vs_len (vs, "k"));
	vs_remove (vs, "k");
	ASSERT_EQ (-1, vs_find_key (vs, "k"));

	vs_free (vs);
	PASS ();
}

TEST test_vs_quoted_list (void)
{
	int vs = vs_init ();
	vs_set (vs, "cols", "id", 1);
	vs_set (vs, "cols", "name", 2);

	/* quoted bare variable -> quoted, comma separated list of all values */
	ASSERT_STR_EQ (
		"insert into datax ( 'id','name' ) values (",
		vs_string (vs, "insert into datax ( $'cols' ) values ("));

	vs_free (vs);
	PASS ();
}

TEST test_vs_build_helpers (void)
{
	static const char *cols[] = {"id", "name", "cnt", "ref", "verify"};
	/* vs_add with 0 creates the system, returns it */
	int vs = vs_add (0, "col_names", cols, 5);
	ASSERT_EQ (5, vs_len (vs, "col_names"));

	int h = vs_string_builder (
		0, vs, "insert into datax ( $'col_names' ) values (");
	ASSERT_STR_EQ ("insert into datax ( 'id','name','cnt','ref','verify' ) "
		       "values (",
		       m_str (h));

	/* builder appends on a second call */
	h = vs_string_builder (h, vs, " ...");
	ASSERT_STR_EQ ("insert into datax ( 'id','name','cnt','ref','verify' ) "
		       "values ( ...",
		       m_str (h));

	m_free (h);
	vs_free (vs);
	PASS ();
}

TEST test_vs_sql_string (void)
{
	int vs = vs_init ();
	vs_set (vs, "v", "O'Brien", 1);

	/* default: backslash escaping */
	ASSERT_STR_EQ ("'O\\'Brien'", vs_string (vs, "$'v'"));
	/* SQL mode: quote doubling, valid for MySQL and SQLite */
	ASSERT_STR_EQ ("'O''Brien'", vs_sql_string (vs, "$'v'"));

	vs_set (vs, "l", "a'b", 1);
	vs_set (vs, "l", "c", 2);
	ASSERT_STR_EQ ("'a''b','c'", vs_sql_string (vs, "$'l[*]'"));

	vs_free (vs);
	PASS ();
}

TEST test_vs_sql_builder (void)
{
	int vs = vs_init ();
	vs_set (vs, "v", "x'y", 1);

	int h = vs_sql_string_builder (0, vs, "v=$'v'");
	ASSERT_STR_EQ ("v='x''y'", m_str (h));

	m_free (h);
	vs_free (vs);
	PASS ();
}

TEST test_vs_read_fields (void)
{
	int vs = vs_init ();
	const char *csv = "1, alice, 3\n2,bob\n";
	FILE *fp = fmemopen ((void *)csv, strlen (csv), "r");

	int var = vs_read_fields (vs, "row", fp, ",");
	ASSERT_EQ (3, vs_klen (var));
	ASSERT_STR_EQ ("1", vs_get (vs, "row", 1));
	ASSERT_STR_EQ ("alice", vs_get (vs, "row", 2));
	ASSERT_STR_EQ ("3", vs_get (vs, "row", 3));

	/* reuses the same variable, drops the previous values */
	var = vs_read_fields (vs, "row", fp, ",");
	ASSERT_EQ (2, vs_klen (var));
	ASSERT_STR_EQ ("2", vs_get (vs, "row", 1));
	ASSERT_STR_EQ ("bob", vs_get (vs, "row", 2));

	/* EOF -> 0 values (the loop-end signal) */
	var = vs_read_fields (vs, "row", fp, ",");
	ASSERT_EQ (0, vs_klen (var));

	fclose (fp);
	vs_free (vs);
	PASS ();
}

TEST test_vs_read_fields_empty_line (void)
{
	int vs = vs_init ();
	const char *csv = "\n";
	FILE *fp = fmemopen ((void *)csv, strlen (csv), "r");

	/* an empty line is one empty field, not EOF */
	int var = vs_read_fields (vs, "row", fp, ",");
	ASSERT_EQ (1, vs_klen (var));
	ASSERT_STR_EQ ("", vs_get (vs, "row", 1));

	fclose (fp);
	vs_free (vs);
	PASS ();
}

TEST test_vs_read_fields_no_sep (void)
{
	int vs = vs_init ();
	const char *csv = "  hello world  \n";
	FILE *fp = fmemopen ((void *)csv, strlen (csv), "r");

	/* empty separator -> one trimmed field, no infinite loop */
	int var = vs_read_fields (vs, "row", fp, "");
	ASSERT_EQ (1, vs_klen (var));
	ASSERT_STR_EQ ("hello world", vs_get (vs, "row", 1));

	fclose (fp);
	vs_free (vs);
	PASS ();
}

TEST test_vs_negative_row (void)
{
	int vs = vs_init ();
	vs_set (vs, "c", "v", 1);

	vs_exp_t se;
	vs_exp_init (&se);
	vs_exp_parse (&se, "<$c>");
	/* must not fall through to the name at index 0 */
	ASSERT_STR_EQ ("<>", vs_expand (&se, vs, -1));

	vs_exp_free (&se);
	vs_free (vs);
	PASS ();
}

TEST test_vs_empty_star (void)
{
	int vs = vs_init ();
	/* [*] on an unknown/empty variable must not read past the name */
	ASSERT_STR_EQ ("<>", vs_string (vs, "<$missing[*]>"));

	vs_free (vs);
	PASS ();
}

SUITE (mls_m_var_suite)
{
	RUN_TEST (test_vs_basic);
	RUN_TEST (test_vs_get_len);
	RUN_TEST (test_vs_indexed);
	RUN_TEST (test_vs_star);
	RUN_TEST (test_vs_quoted);
	RUN_TEST (test_vs_escaped_dollar);
	RUN_TEST (test_vs_multiple_rows);
	RUN_TEST (test_vs_clr_remove);
	RUN_TEST (test_vs_quoted_list);
	RUN_TEST (test_vs_build_helpers);
	RUN_TEST (test_vs_sql_string);
	RUN_TEST (test_vs_sql_builder);
	RUN_TEST (test_vs_read_fields);
	RUN_TEST (test_vs_read_fields_empty_line);
	RUN_TEST (test_vs_read_fields_no_sep);
	RUN_TEST (test_vs_negative_row);
	RUN_TEST (test_vs_empty_star);
}

GREATEST_MAIN_DEFS ();

int main (int argc, char **argv)
{
	m_init ();
	GREATEST_MAIN_BEGIN ();
	RUN_SUITE (mls_m_var_suite);
	GREATEST_MAIN_END ();
}
