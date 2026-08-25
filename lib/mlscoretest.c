#include "mls.h"
#include <assert.h>
#include <sys/wait.h>
#include <unistd.h>

int h;

void verify_mls_try_must (void)
{
	h = m_create (2, sizeof (int));

	/* success case: try returns MLS_OK and value was stored */
	int v = 42;
	assert (mls_try (m_put_safe (h, &v)) == MLS_OK);
	assert (INT (h, 0) == 42);

	/* error case: try returns the code and keeps it stored for later */
	size_t bogus = 1 << 20;
	assert (mls_try (m_read_safe (h, bogus, (void **)&v, 1)) != MLS_OK);
	assert (mls_errno != MLS_OK);
	printf ("try: %s in %s\n", mls_errmsg (mls_errno), mls_errfunc);

	/* must() survives a successful call */
	mls_must (m_put_safe (h, &v));
	m_free (h);

	/* must() on a failing call: child reports and exits(1) */
	fflush (stdout);
	pid_t pid = fork ();
	if (pid == 0) {
		h = m_create (2, sizeof (int));
		mls_must (m_read_safe (h, 9999, (void **)&v, 1));
		_exit (0); /* not reached */
	}
	int st = 0;
	waitpid (pid, &st, 0);
	assert (WIFEXITED (st) && WEXITSTATUS (st) == 1);
	printf ("must: exits as expected\n");
}

void verify_pairs (void)
{
	int st = 0;
	h = m_create (2, sizeof (int));
	int v = 42;

	/* try: success stores the value, failure returns a code */
	assert (m_put_try (h, &v) == MLS_OK);
	assert (*(int *)m_at_must (h, 0) == 42);
	v = 43;
	assert (m_write_try (h, 0, &v, 1) == MLS_OK);
	assert (*(int *)m_at_must (h, 0) == 43);
	size_t bogus = 1 << 20;
	void *pp;
	assert (m_read_try (h, bogus, &pp, 1) == MLS_EBOUNDS);
	assert (m_del_try (h, bogus) != MLS_OK);
	assert (m_setlen_try (h, (size_t)-1) != MLS_OK);
	assert (m_put_try (-5, &v) != MLS_OK);

	/* must: works on success */
	m_setlen_must (h, 4);
	m_del_must (h, 3);
	m_free (h);

	/* must: dies with exit(1) on failure */
	fflush (stdout);
	pid_t pid = fork ();
	if (pid == 0)
		m_read_must (h, bogus, &pp, 1); /* not reached */
	waitpid (pid, &st, 0);
	assert (WIFEXITED (st) && WEXITSTATUS (st) == 1);

	/* m_at_must: dies on out-of-bounds too */
	fflush (stdout);
	pid = fork ();
	if (pid == 0) {
		h = m_create (2, sizeof (int));
		m_at_must (h, 7777);
		_exit (0); /* not reached */
	}
	waitpid (pid, &st, 0);
	assert (WIFEXITED (st) && WEXITSTATUS (st) == 1);
	printf ("pairs: ok\n");
}

void verify_trace_info (void)
{
	static char *s = "hello";
	m_wrapcstr (s); /* no alloc */

	m_destruct ();
	m_init ();

	h = s_cstrdup (s); /* copy string -> alloc memory */
	printf ("REAL ALLOC: %d %s\n", h, m_str (h));

	s_ccstr ("world"); /* no alloc: because constant c string */

	h = s_ccstr (s); /* "hello" is already alloced,no alloc */

	int ii[] = {1};
	m_wrapints (ii, 1);

	char *ss[] = {"HELLO"};
	m_wrapstrings (ss, 1);
}

int main (int argc, char **argv)
{
	(void)argc;
	(void)argv;
	m_init ();
	trace_level = 1;
	verify_trace_info ();
	verify_mls_try_must ();
	verify_pairs ();
	printf ("%d:%s\n", h, m_str (h));
	m_destruct ();
}
