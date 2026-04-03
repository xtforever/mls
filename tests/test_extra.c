#include "mls.h"
#include "m_tool.h"
#include "m_extra.h"
#undef ASSERT
#include "greatest.h"

SUITE (mls_extra_suite);

TEST test_s_casecmp (void)
{
	int a = s_dup ("Hello");
	int b = s_dup ("hello");
	int c = s_dup ("World");
	ASSERT_EQ (0, s_casecmp (a, b));
	ASSERT (s_casecmp (a, c) < 0);
	
	/* Robustness: Handle 0 */
	ASSERT_EQ (0, s_casecmp (0, 0));
	ASSERT (s_casecmp (0, a) < 0);
	ASSERT (s_casecmp (a, 0) > 0);

	/* Robustness: Non-terminated strings */
	int nt = m_alloc(5, 1, MFREE);
	m_setlen(nt, 5);
	memcpy(m_buf(nt), "HELLO", 5);
	ASSERT_EQ(0, s_casecmp(a, nt));
	
	s_free (a);
	s_free (b);
	s_free (c);
	s_free (nt);
	PASS ();
}

TEST test_s_ncasecmp (void)
{
	int a = s_dup ("Hello World");
	int b = s_dup ("hello gems");
	
	ASSERT_EQ (0, s_ncasecmp (a, b, 5));
	ASSERT (s_ncasecmp (a, b, 10) != 0);
	
	ASSERT_EQ (0, s_ncasecmp (0, 0, 5));
	
	s_free (a);
	s_free (b);
	PASS ();
}

TEST test_s_to_long (void)
{
	int h = s_dup ("12345");
	ASSERT_EQ (12345L, s_to_long (h));
	s_free (h);
	PASS ();
}

TEST test_s_trim_advanced (void)
{
	int h = s_dup ("   hello   ");
	int left = s_trim_left_c (h, NULL);
	int right = s_trim_right_c (h, NULL);
	
	ASSERT_STR_EQ ("hello   ", m_str (left));
	ASSERT_STR_EQ ("   hello", m_str (right));
	
	s_free (h);
	s_free (left);
	s_free (right);
	PASS ();
}

TEST test_s_reverse (void)
{
	int h = s_dup ("abcde");
	s_reverse (h);
	ASSERT_STR_EQ ("edcba", m_str (h));
	s_free (h);
	PASS ();
}

TEST test_s_pad (void)
{
	int h = s_dup ("abc");
	int left = s_pad_left (h, 5, '*');
	int right = s_pad_right (h, 5, '-');
	
	ASSERT_STR_EQ ("**abc", m_str (left));
	ASSERT_STR_EQ ("abc--", m_str (right));
	
	s_free (h);
	s_free (left);
	s_free (right);
	PASS ();
}

TEST test_s_classification (void)
{
	int n = s_dup ("123");
	int a = s_dup ("abc");
	int m = s_dup ("a1");
	
	ASSERT (s_is_numeric (n));
	ASSERT (!s_is_numeric (a));
	ASSERT (s_is_alpha (a));
	ASSERT (!s_is_alpha (n));
	ASSERT (!s_is_alpha (m));
	ASSERT (!s_is_numeric (m));

	/* Robustness: Non-terminated strings */
	int nt_n = m_alloc(3, 1, MFREE);
	m_setlen(nt_n, 3);
	memcpy(m_buf(nt_n), "456", 3);
	ASSERT (s_is_numeric (nt_n));

	int nt_a = m_alloc(3, 1, MFREE);
	m_setlen(nt_a, 3);
	memcpy(m_buf(nt_a), "XYZ", 3);
	ASSERT (s_is_alpha (nt_a));
	
	s_free (n);
	s_free (a);
	s_free (m);
	s_free (nt_n);
	s_free (nt_a);
	PASS ();
}

TEST test_s_secure_cmp (void)
{
	int a = s_dup ("secret");
	int b = s_dup ("secret");
	int c = s_dup ("wrong");
	ASSERT_EQ (0, s_secure_cmp (a, b));
	ASSERT_EQ (1, s_secure_cmp (a, c));
	s_free (a);
	s_free (b);
	s_free (c);
	PASS ();
}

SUITE (mls_extra_suite)
{
	RUN_TEST (test_s_casecmp);
	RUN_TEST (test_s_ncasecmp);
	RUN_TEST (test_s_to_long);
	RUN_TEST (test_s_trim_advanced);
	RUN_TEST (test_s_reverse);
	RUN_TEST (test_s_pad);
	RUN_TEST (test_s_classification);
	RUN_TEST (test_s_secure_cmp);
}

GREATEST_MAIN_DEFS ();

int main (int argc, char **argv)
{
	m_init ();
	GREATEST_MAIN_BEGIN ();
	RUN_SUITE (mls_extra_suite);
	GREATEST_MAIN_END ();
}
