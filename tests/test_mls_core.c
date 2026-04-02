#include "mls.h"
#include "m_tool.h"
/* Conflict resolution: mls.h defines ASSERT, so does greatest.h.
   We undefine the one from mls.h before including greatest.h */
#undef ASSERT
#include "greatest.h"
#include <string.h>

/* Define a suite for core memory and list functions */
SUITE (mls_core_suite);

TEST test_m_init_destruct (void)
{
	/* m_init() is called in main, but we can check if it returns 1 (already init) */
	ASSERT_EQ (1, m_init ());
	PASS ();
}

TEST test_m_alloc_free (void)
{
	int h = m_alloc (10, sizeof (int), MFREE);
	ASSERT (h > 0);
	ASSERT_EQ (0, m_len (h));
	ASSERT_EQ (sizeof (int), m_width (h));
	m_free (h);
	PASS ();
}

TEST test_m_put_get (void)
{
	int h = m_alloc (0, sizeof (int), MFREE);
	int val = 42;
	m_put (h, &val);
	ASSERT_EQ (1, m_len (h));

	int *ptr = (int *)m_peek (h, 0);
	ASSERT_EQ (42, *ptr);

	int retrieved = 0;
	void *p_ret = &retrieved;
	m_read (h, 0, &p_ret, 1);
	ASSERT_EQ (42, retrieved);

	m_free (h);
	PASS ();
}

TEST test_m_puti_m_putc (void)
{
	int hi = m_alloc (0, sizeof (int), MFREE);
	m_puti (hi, 100);
	m_puti (hi, 200);
	ASSERT_EQ (2, m_len (hi));
	ASSERT_EQ (100, *(int *)m_peek (hi, 0));
	ASSERT_EQ (200, *(int *)m_peek (hi, 1));
	m_free (hi);

	int hc = m_alloc (0, 1, MFREE);
	m_putc (hc, 'A');
	m_putc (hc, 'B');
	ASSERT_EQ (2, m_len (hc));
	ASSERT_EQ ('A', *(char *)m_peek (hc, 0));
	ASSERT_EQ ('B', *(char *)m_peek (hc, 1));
	m_free (hc);
	PASS ();
}

TEST test_m_next (void)
{
	int h = m_alloc (0, sizeof (int), MFREE);
	for (int i = 0; i < 5; i++)
		m_puti (h, i * 10);

	int p = -1;
	int *val_ptr;
	int count = 0;
	while (m_next (h, &p, &val_ptr)) {
		ASSERT_EQ (count * 10, *val_ptr);
		count++;
	}
	ASSERT_EQ (5, count);
	m_free (h);
	PASS ();
}

TEST test_m_clear (void)
{
	int h = m_alloc (10, sizeof (int), MFREE);
	m_puti (h, 1);
	m_puti (h, 2);
	ASSERT_EQ (2, m_len (h));
	m_clear (h);
	ASSERT_EQ (0, m_len (h));
	m_free (h);
	PASS ();
}

TEST test_m_del_remove (void)
{
	int h = m_alloc (0, sizeof (int), MFREE);
	for (int i = 0; i < 5; i++)
		m_puti (h, i);

	/* [0, 1, 2, 3, 4] */
	m_del (h, 2); /* Delete element at index 2 (value 2) */
	/* Should be [0, 1, 3, 4] */
	ASSERT_EQ (4, m_len (h));
	ASSERT_EQ (3, *(int *)m_peek (h, 2));

	m_remove (h, 1, 2); /* Remove 2 elements starting at index 1 */
	/* Should be [0, 4] */
	ASSERT_EQ (2, m_len (h));
	ASSERT_EQ (4, *(int *)m_peek (h, 1));

	m_free (h);
	PASS ();
}

SUITE (mls_string_suite);

TEST test_s_printf_app (void)
{
	int s = s_printf (0, 0, "Hello");
	ASSERT_STR_EQ ("Hello", m_str (s));
	ASSERT_EQ (5, s_strlen (s));

	s_app (s, " World", NULL);
	ASSERT_STR_EQ ("Hello World", m_str (s));

	s_app1 (s, "!");
	ASSERT_STR_EQ ("Hello World!", m_str (s));

	m_free (s);
	PASS ();
}

TEST test_s_copy_split (void)
{
	int s = s_printf (0, 0, "one,two,three");
	int parts = m_alloc (0, sizeof (char *), MFREE_STR);
	s_split (parts, m_str (s), ',', 1);
	ASSERT_EQ (3, m_len (parts));
	ASSERT_STR_EQ ("one", STR (parts, 0));
	ASSERT_STR_EQ ("two", STR (parts, 1));
	ASSERT_STR_EQ ("three", STR (parts, 2));

	int s2 = s_copy (s, 0, -1);
	ASSERT_STR_EQ (m_str (s), m_str (s2));

	m_free (s);
	m_free (s2);
	m_free (parts);
	PASS ();
}

SUITE (mls_search_suite);

TEST test_binary_search (void)
{
	int h = m_alloc (0, sizeof (int), MFREE);
	m_binsert_int (h, 30);
	m_binsert_int (h, 10);
	m_binsert_int (h, 20);

	ASSERT_EQ (3, m_len (h));
	ASSERT_EQ (10, INT (h, 0));
	ASSERT_EQ (20, INT (h, 1));
	ASSERT_EQ (30, INT (h, 2));

	ASSERT_EQ (0, m_bsearch_int (h, 10));
	ASSERT_EQ (1, m_bsearch_int (h, 20));
	ASSERT_EQ (2, m_bsearch_int (h, 30));
	ASSERT_EQ (-1, m_bsearch_int (h, 40));

	m_free (h);
	PASS ();
}

SUITE (mls_variable_suite);

TEST test_variables (void)
{
	int vl = v_init ();
	v_set (vl, "key1", "val1", 1);
	v_set (vl, "key2", "val2", 1);

	ASSERT_EQ (1, v_len (vl, "key1"));
	char *s = v_get (vl, "key2", 1);
	ASSERT_STR_EQ ("val2", s);

	v_free (vl);
	PASS ();
}

/* Suite definitions */
GREATEST_SUITE (mls_core_suite)
{
	RUN_TEST (test_m_init_destruct);
	RUN_TEST (test_m_alloc_free);
	RUN_TEST (test_m_put_get);
	RUN_TEST (test_m_puti_m_putc);
	RUN_TEST (test_m_next);
	RUN_TEST (test_m_clear);
	RUN_TEST (test_m_del_remove);
}

GREATEST_SUITE (mls_string_suite)
{
	RUN_TEST (test_s_printf_app);
	RUN_TEST (test_s_copy_split);
}

GREATEST_SUITE (mls_search_suite)
{
	RUN_TEST (test_binary_search);
}

GREATEST_SUITE (mls_variable_suite)
{
	RUN_TEST (test_variables);
}

/* Main function */
GREATEST_MAIN_DEFS ();

int main (int argc, char **argv)
{
	GREATEST_MAIN_BEGIN ();
	m_init ();
	conststr_init ();
	RUN_SUITE (mls_core_suite);
	RUN_SUITE (mls_string_suite);
	RUN_SUITE (mls_search_suite);
	RUN_SUITE (mls_variable_suite);
	conststr_free ();
	GREATEST_MAIN_END ();
}
