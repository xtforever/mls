#include "mls.h"
#include "m_tool.h"
#include "m_extra.h"
#undef ASSERT
#include "greatest.h"

SUITE (mls_se_string_suite);

TEST test_se_basic (void)
{
	int vs = v_init ();
	v_set (vs, "name", "John", 1);
	v_set (vs, "age", "30", 1);

	char *res = se_string (vs, "Hello $name, you are $age years old.");
	ASSERT_STR_EQ ("Hello John, you are 30 years old.", res);

	v_free (vs);
	PASS ();
}

TEST test_se_indexed (void)
{
	int vs = v_init ();
	/* v_set at pos 1 is index 0 for expansion system (since STR(var,0) is the name) */
	v_set (vs, "items", "apple", 1);
	v_set (vs, "items", "banana", 2);
	v_set (vs, "items", "cherry", 3);

	/* [0] uses the row parameter passed to se_expand, se_string passes 0 */
	ASSERT_STR_EQ ("First: apple", se_string (vs, "First: $items[0]"));
	
	/* [1] in parse_index returns val+2 = 3. 
	   In se_expand: index -= 2 => 1. 
	   STR(var, index+1) => STR(var, 2) => "banana" 
	*/
	ASSERT_STR_EQ ("Second: banana", se_string (vs, "Second: $items[1]"));
	ASSERT_STR_EQ ("Third: cherry", se_string (vs, "Third: $items[2]"));

	v_free (vs);
	PASS ();
}

TEST test_se_star (void)
{
	int vs = v_init ();
	v_set (vs, "list", "a", 1);
	v_set (vs, "list", "b", 2);
	v_set (vs, "list", "c", 3);

	/* [*] in parse_index returns 1. 
	   In se_expand: if (index == 1) it joins all elements with comma.
	*/
	ASSERT_STR_EQ ("All: a,b,c", se_string (vs, "All: $list[*]"));

	v_free (vs);
	PASS ();
}

TEST test_se_quoted (void)
{
	int vs = v_init ();
	v_set (vs, "val", "it's a \"test\"", 1);

	/* $'name' should wrap in single quotes and escape content */
	char *res = se_string (vs, "Quoted: $'val'");
	/* repl_char escapes ' as \' and " as \" */
	ASSERT_STR_EQ ("Quoted: 'it\\'s a \\\"test\\\"'", res);

	v_free (vs);
	PASS ();
}

TEST test_se_escaped_dollar (void)
{
	int vs = v_init ();
	v_set (vs, "var", "val", 1);

	/* \$ should be treated as literal $ */
	ASSERT_STR_EQ ("Price: $100", se_string (vs, "Price: \\$100"));
	ASSERT_STR_EQ ("Value: val", se_string (vs, "Value: $var"));

	v_free (vs);
	PASS ();
}

TEST test_se_multiple_rows (void)
{
	int vs = v_init ();
	v_set (vs, "col", "row0", 1);
	v_set (vs, "col", "row1", 2);

	str_exp_t se;
	se_init (&se);
	se_parse (&se, "Value: $col");
	
	char *res0 = se_expand (&se, vs, 0);
	ASSERT_STR_EQ ("Value: row0", res0);
	
	char *res1 = se_expand (&se, vs, 1);
	ASSERT_STR_EQ ("Value: row1", res1);

	se_free (&se);
	v_free (vs);
	PASS ();
}

SUITE (mls_se_string_suite)
{
	RUN_TEST (test_se_basic);
	RUN_TEST (test_se_indexed);
	RUN_TEST (test_se_star);
	RUN_TEST (test_se_quoted);
	RUN_TEST (test_se_escaped_dollar);
	RUN_TEST (test_se_multiple_rows);
}

GREATEST_MAIN_DEFS ();

int main (int argc, char **argv)
{
	m_init ();
	GREATEST_MAIN_BEGIN ();
	RUN_SUITE (mls_se_string_suite);
	GREATEST_MAIN_END ();
}
