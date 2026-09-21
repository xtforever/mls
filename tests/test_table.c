#include "../lib/m_tool.h"
#include "../lib/mls.h"
#include "../lib/table.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void test_tbl_basic ()
{
	printf ("Testing tbl basic string operations...\n");
	int t = tbl_create ();
	assert (t > 0);
	assert (tbl_is_table (t));

	tbl_set (t, "name", "Alice");
	tbl_set (t, "age", "30");
	assert (strcmp (tbl_get (t, "name"), "Alice") == 0);
	assert (strcmp (tbl_get (t, "age"), "30") == 0);
	assert (strcmp (m_str (tbl_get_handle (t, "name")), "Alice") == 0);

	// non-existent
	assert (tbl_get (t, "city") == NULL);
	assert (tbl_get_handle (t, "city") == 0);

	tbl_free (t);
	printf ("tbl basic string operations passed.\n");
}

void test_tbl_sorted ()
{
	printf ("Testing tbl sorted iteration...\n");
	int t = tbl_create ();
	tbl_set (t, "zeta", "26");
	tbl_set (t, "alpha", "1");
	tbl_set (t, "beta", "2");
	assert (tbl_len (t) == 3);
	assert (strcmp (tbl_key (t, 0), "alpha") == 0);
	assert (strcmp (tbl_key (t, 1), "beta") == 0);
	assert (strcmp (tbl_key (t, 2), "zeta") == 0);
	assert (strcmp (tbl_value (t, 1), "2") == 0);
	tbl_free (t);
	printf ("tbl sorted iteration passed.\n");
}

void test_tbl_overwrite ()
{
	printf ("Testing tbl overwrite...\n");
	int t = tbl_create ();
	tbl_set (t, "k", "one");
	assert (strcmp (tbl_get (t, "k"), "one") == 0);
	tbl_set (t, "k", "two");
	assert (strcmp (tbl_get (t, "k"), "two") == 0);
	assert (tbl_len (t) == 1);
	tbl_free (t);
	printf ("tbl overwrite passed.\n");
}

void test_tbl_handle_values ()
{
	printf ("Testing tbl handle values...\n");
	int t = tbl_create ();

	int list_h = m_alloc (0, sizeof (int), MFREE);
	int v = 42;
	m_put (list_h, &v);
	tbl_set_handle (t, "grades", list_h);
	assert (tbl_get_handle (t, "grades") == list_h);
	assert (INT (tbl_get_handle (t, "grades"), 0) == 42);
	assert (tbl_get (t, "grades") == NULL); // not a string value

	tbl_free (t); // frees list_h
	printf ("tbl handle values passed.\n");
}

void test_tbl_str_key_handle ()
{
	printf ("Testing tbl string-handle key...\n");
	int t = tbl_create ();

	int key_h = s_strdup_c ("mykey");
	int val_h = s_strdup_c ("myval");
	tbl_set_handle_str (t, key_h, val_h); // takes ownership of both

	assert (strcmp (tbl_get (t, "mykey"), "myval") == 0);
	assert (tbl_get_handle (t, "mykey") == val_h);

	tbl_free (t);
	printf ("tbl string-handle key passed.\n");
}

void test_tbl_del ()
{
	printf ("Testing tbl del...\n");
	int t = tbl_create ();
	tbl_set (t, "a", "1");
	tbl_set (t, "b", "2");
	assert (tbl_del (t, "a") == 0);
	assert (tbl_del (t, "a") == -1);
	assert (tbl_len (t) == 1);
	assert (tbl_get (t, "a") == NULL);
	assert (strcmp (tbl_get (t, "b"), "2") == 0);
	tbl_free (t);
	printf ("tbl del passed.\n");
}

void test_tbl_is_table ()
{
	printf ("Testing tbl_is_table distinguishes handles...\n");
	int t = tbl_create ();
	int plain = m_alloc (4, sizeof (int), MFREE);
	assert (tbl_is_table (t));
	assert (!tbl_is_table (plain));
	assert (!tbl_is_table (0));
	tbl_free (t);
	m_free (plain);
	printf ("tbl_is_table passed.\n");
}

int main ()
{
	m_init ();
	conststr_init ();

	test_tbl_basic ();
	test_tbl_sorted ();
	test_tbl_overwrite ();
	test_tbl_handle_values ();
	test_tbl_str_key_handle ();
	test_tbl_del ();
	test_tbl_is_table ();

	conststr_free ();
	m_destruct ();
	printf ("All tbl tests completed successfully.\n");
	return 0;
}
