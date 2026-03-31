#include "mls.h"
#include "m_tool.h"
#undef ASSERT
#include "greatest.h"

SUITE (m_string_suite);

TEST test_s_new (void)
{
	int h = s_new ();
	ASSERT (h > 0);
	ASSERT_EQ (0, m_len (h));
	ASSERT_EQ (1, m_width (h));
	s_free (h);
	PASS ();
}

TEST test_s_dup (void)
{
	int h = s_dup ("Hello");
	ASSERT (h > 0);
	ASSERT_STR_EQ ("Hello", m_str (h));
	s_free (h);

	h = s_dup ("");
	ASSERT (h > 0);
	ASSERT_STR_EQ ("", m_str (h));
	s_free (h);

	h = s_dup (NULL);
	ASSERT (h > 0);
	ASSERT_STR_EQ ("", m_str (h));
	s_free (h);

	PASS ();
}

TEST test_s_clone (void)
{
	int h1 = s_dup ("Clone me");
	int h2 = s_clone (h1);
	ASSERT (h2 > 0);
	ASSERT (h1 != h2);
	ASSERT_STR_EQ (m_str (h1), m_str (h2));
	s_free (h1);
	s_free (h2);
	PASS ();
}

TEST test_s_resize (void)
{
	int h = s_dup ("Hello");
	s_resize (h, 10);
	ASSERT_EQ (11, m_len (h));
	ASSERT_EQ (0, CHAR (h, 10));
	s_resize (h, 3);
	ASSERT_EQ (4, m_len (h));
	ASSERT_EQ (0, CHAR (h, 3));
	ASSERT_STR_EQ ("Hel", m_str (h));
	s_free (h);
	PASS ();
}

TEST test_s_clear (void)
{
	int h = s_dup ("Clear me");
	s_clear (h);
	ASSERT_EQ (0, m_len (h));
	s_free (h);
	PASS ();
}

TEST test_s_has_prefix (void)
{
	int h = s_dup ("Hello World");
	ASSERT (s_has_prefix (h, "Hello"));
	ASSERT (s_has_prefix (h, "Hell"));
	ASSERT (!s_has_prefix (h, "world"));
	ASSERT (!s_has_prefix (h, "Hello World!"));
	s_free (h);
	PASS ();
}

TEST test_s_has_suffix (void)
{
	int h = s_dup ("Hello World");
	ASSERT (s_has_suffix (h, "World"));
	ASSERT (s_has_suffix (h, "rld"));
	ASSERT (!s_has_suffix (h, "hello"));
	ASSERT (!s_has_suffix (h, "!Hello World"));
	s_free (h);
	PASS ();
}

TEST test_s_from_long (void)
{
	int h = s_from_long (12345);
	ASSERT_STR_EQ ("12345", m_str (h));
	s_free (h);

	h = s_from_long (-1);
	ASSERT_STR_EQ ("-1", m_str (h));
	s_free (h);
	PASS ();
	}

	TEST test_s_hash (void)
	{
	int h1 = s_dup ("test");
	int h2 = s_dup ("test");
	int h3 = s_dup ("other");
	ASSERT_EQ (s_hash (h1), s_hash (h2));
	ASSERT (s_hash (h1) != s_hash (h3));
	s_free (h1);
	s_free (h2);
	s_free (h3);
	PASS ();
	}

	TEST test_s_join (void)
	{
	int h = s_join (", ", "one", "two", "three", NULL);
	ASSERT_STR_EQ ("one, two, three", m_str (h));
	s_free (h);

	h = s_join ("-", "A", "B", NULL);
	ASSERT_STR_EQ ("A-B", m_str (h));
	s_free (h);

	h = s_join (" ", "Hello", NULL);
	ASSERT_STR_EQ ("Hello", m_str (h));
	s_free (h);

	h = s_join ("-", NULL);
	ASSERT_STR_EQ ("", m_str (h));
	s_free (h);
	PASS ();
	}

	TEST test_s_cmp (void)
	{
	int h1 = s_dup ("abc");
	int h2 = s_dup ("abc");
	int h3 = s_dup ("abd");
	int h4 = s_dup ("ab");

	ASSERT_EQ (0, s_cmp (h1, h2));
	ASSERT (s_cmp (h1, h3) < 0);
	ASSERT (s_cmp (h3, h1) > 0);
	ASSERT (s_cmp (h1, h4) > 0);
	ASSERT (s_cmp (h4, h1) < 0);

	s_free (h1);
	s_free (h2);
	s_free (h3);
	s_free (h4);
	PASS ();
	}

	TEST test_s_ncmp (void)
	{
	int h1 = s_dup ("abcde");
	int h2 = s_dup ("abc");
	int h3 = s_dup ("abcXY");

	ASSERT_EQ (0, s_ncmp (h1, h2, 3));
	ASSERT_EQ (0, s_ncmp (h1, h3, 3));
	ASSERT (s_ncmp (h1, h3, 4) > 0);

	s_free (h1);
	s_free (h2);
	s_free (h3);
	PASS ();
	}

	TEST test_s_chr (void)
	{
	int h = s_dup ("abcabc");
	ASSERT_EQ (0, s_chr (h, 'a', 0));
	ASSERT_EQ (1, s_chr (h, 'b', 0));
	ASSERT_EQ (3, s_chr (h, 'a', 1));
	ASSERT_EQ (4, s_chr (h, 'b', 2));
	ASSERT_EQ (-1, s_chr (h, 'z', 0));
	ASSERT_EQ (-1, s_chr (h, 'a', 10));
	s_free (h);
	PASS ();
	}

	TEST test_s_rchr (void)
	{
	int h = s_dup ("abcabc");
	ASSERT_EQ (3, s_rchr (h, 'a'));
	ASSERT_EQ (4, s_rchr (h, 'b'));
	ASSERT_EQ (5, s_rchr (h, 'c'));
	ASSERT_EQ (-1, s_rchr (h, 'z'));
	s_free (h);
	PASS ();
	}

	TEST test_s_find (void)
	{
	int h = s_dup ("Hello World");
	ASSERT_EQ (6, s_find (h, "World"));
	ASSERT_EQ (0, s_find (h, "Hello"));
	ASSERT_EQ (-1, s_find (h, "world"));
	ASSERT_EQ (-1, s_find (h, ""));
	ASSERT_EQ (-1, s_find (h, NULL));
	s_free (h);
	PASS ();
	}

	TEST test_s_spn (void)
	{
	int h = s_dup ("12345abc");
	ASSERT_EQ (5, s_spn (h, "1234567890"));
	ASSERT_EQ (0, s_spn (h, "abc"));
	s_free (h);
	PASS ();
	}

	TEST test_s_cspn (void)
	{
	int h = s_dup ("abcde123");
	ASSERT_EQ (5, s_cspn (h, "1234567890"));
	ASSERT_EQ (0, s_cspn (h, "abc"));
	s_free (h);
	PASS ();
	}

	TEST test_s_cat (void)
	{
	int h = s_dup ("Hello");
	s_cat (h, " World");
	ASSERT_STR_EQ ("Hello World", m_str (h));
	s_free (h);

	h = s_cat (0, "Start");
	ASSERT_STR_EQ ("Start", m_str (h));
	s_free (h);
	PASS ();
	}

	TEST test_s_ncat (void)
	{
	int h = s_dup ("Hello");
	s_ncat (h, " World", 3);
	ASSERT_STR_EQ ("Hello Wo", m_str (h));
	s_free (h);

	h = s_ncat (0, "Starting", 5);
	ASSERT_STR_EQ ("Start", m_str (h));
	s_free (h);
	PASS ();
	}

	TEST test_s_sub (void)
	{
	int h = s_dup ("Hello World");
	int sub = s_sub (h, 6, 5);
	ASSERT_STR_EQ ("World", m_str (sub));
	s_free (sub);

	sub = s_sub (h, 0, 5);
	ASSERT_STR_EQ ("Hello", m_str (sub));
	s_free (sub);

	sub = s_sub (h, 6, 100);
	ASSERT_STR_EQ ("World", m_str (sub));
	s_free (sub);

	sub = s_sub (h, 20, 5);
	ASSERT_STR_EQ ("", m_str (sub));
	s_free (sub);

	s_free (h);
	PASS ();
	}

	TEST test_s_left (void)
	{
	int h = s_dup ("Hello World");
	int left = s_left (h, 5);
	ASSERT_STR_EQ ("Hello", m_str (left));
	s_free (left);
	s_free (h);
	PASS ();
	}

	TEST test_s_right (void)
	{
	int h = s_dup ("Hello World");
	int right = s_right (h, 5);
	ASSERT_STR_EQ ("World", m_str (right));
	s_free (right);
	s_free (h);
	PASS ();
	}

	TEST test_s_replace_c (void)
	{
	int h = s_dup ("Hello World World");
	int rep = s_replace_c (h, "World", "Gemini");
	ASSERT_STR_EQ ("Hello Gemini Gemini", m_str (rep));
	s_free (rep);

	rep = s_replace_c (h, "Foo", "Bar");
	ASSERT_STR_EQ ("Hello World World", m_str (rep));
	s_free (rep);

	s_free (h);
	PASS ();
	}

	TEST test_s_trim_c (void)
	{
	int h = s_dup ("  Hello  ");
	int trim = s_trim_c (h, NULL);
	ASSERT_STR_EQ ("Hello", m_str (trim));
	s_free (trim);

	trim = s_trim_c (h, " ");
	ASSERT_STR_EQ ("Hello", m_str (trim));
	s_free (trim);

	s_free (h);
	h = s_dup ("!!Hello!!");
	trim = s_trim_c (h, "!");
	ASSERT_STR_EQ ("Hello", m_str (trim));
	s_free (trim);

	s_free (h);
	PASS ();
	}

	GREATEST_SUITE (m_string_suite)
	{
	RUN_TEST (test_s_new);
	RUN_TEST (test_s_dup);
	RUN_TEST (test_s_clone);
	RUN_TEST (test_s_resize);
	RUN_TEST (test_s_clear);
	RUN_TEST (test_s_has_prefix);
	RUN_TEST (test_s_has_suffix);
	RUN_TEST (test_s_from_long);
	RUN_TEST (test_s_hash);
	RUN_TEST (test_s_join);
	RUN_TEST (test_s_cmp);
	RUN_TEST (test_s_ncmp);
	RUN_TEST (test_s_chr);
	RUN_TEST (test_s_rchr);
	RUN_TEST (test_s_find);
	RUN_TEST (test_s_spn);
	RUN_TEST (test_s_cspn);
	RUN_TEST (test_s_cat);
	RUN_TEST (test_s_ncat);
	RUN_TEST (test_s_sub);
	RUN_TEST (test_s_left);
	RUN_TEST (test_s_right);
	RUN_TEST (test_s_replace_c);
	RUN_TEST (test_s_trim_c);
	}GREATEST_MAIN_DEFS ();

int main (int argc, char **argv)
{
	GREATEST_MAIN_BEGIN ();
	m_init ();
	RUN_SUITE (m_string_suite);
	GREATEST_MAIN_END ();
}
