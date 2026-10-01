/* Test for the generated single-header amalgam.
 *
 * Built in four configurations (see test_mls_core_amalgam.sh), one per
 * convenience preset header:
 *   mls_core.h (release, ST)   mls_coremt.h (release, MT)
 *   mls_cored.h (debug, ST)    mls_coredmt.h (debug, MT)
 * and run with a selectable trace_level (argv[1]).
 *
 * Exercises a few create / append / dump / free ops and self-checks the
 * results with plain asserts. No test framework, no extra deps.
 *
 *   cc -std=c11 -I.. -DMLS_TEST_HEADER='"../mls_core.h"' \
 *      test_mls_core_amalgam.c -o t -lpthread -lm -ldl
 *   ./t 1        # trace_level = 1
 */
#define MLS_CORE_IMPLEMENTATION
#ifndef MLS_TEST_HEADER
#define MLS_TEST_HEADER "../mls_core.h"
#endif
#include MLS_TEST_HEADER

#define CHECK(cond, msg)                                                       \
	do {                                                                   \
		if (!(cond)) {                                                 \
			fprintf (stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__,  \
				 __LINE__);                                    \
			return 1;                                              \
		}                                                              \
	} while (0)

#if MLS_THREAD_SAFE
#define AMALGAM_THREADS 4
#define AMALGAM_ITERS 500

static void *append_worker (void *arg)
{
	int h = *(int *)arg;
	for (int i = 0; i < AMALGAM_ITERS; i++)
		m_puti (h, i);
	return NULL;
}
#endif

int main (int argc, char **argv)
{
	/* selectable trace level: argv[1], default 0 (off) */
	trace_level = argc > 1 ? (int)strtol (argv[1], NULL, 10) : 0;

	m_init ();
	size_t base = m_count_allocated (); /* debug build has the DEB handle */

	/* --- create --- */
	int h = m_create (0, sizeof (int));
	CHECK (h > 0, "m_create returned a handle");
	CHECK (m_count_allocated () == base + 1, "create allocated one handle");
	CHECK (m_len (h) == 0, "new handle is empty");

	/* --- append --- */
	for (int i = 0; i < 5; i++)
		m_puti (h, i * 10);
	CHECK (m_len (h) == 5, "appended 5 elements");
	CHECK (INT (h, 2) == 20, "element value at index 2");

	/* append via the pointer variant */
	int *p = m_add (h);
	CHECK (p != NULL, "m_add returned a slot");
	*p = 999;
	CHECK (m_len (h) == 6 && INT (h, 5) == 999, "m_add appended value");

	/* --- dump --- */
	m_debug_print (stderr);
	CHECK (m_total_bytes () > 0, "dump sees allocated bytes");

	/* --- free --- */
	m_free (h);
	CHECK (m_count_allocated () == base, "free released the handle");

#if MLS_THREAD_SAFE
	/* concurrent append on a shared handle exercises the locks */
	int th = m_create (0, sizeof (int));
	pthread_t tids[AMALGAM_THREADS];
	for (int i = 0; i < AMALGAM_THREADS; i++)
		CHECK (pthread_create (&tids[i], NULL, append_worker, &th) == 0,
		       "pthread_create");
	for (int i = 0; i < AMALGAM_THREADS; i++)
		CHECK (pthread_join (tids[i], NULL) == 0, "pthread_join");
	CHECK (m_len (th) == AMALGAM_THREADS * AMALGAM_ITERS,
	       "threaded append count");
	m_free (th);
#endif

	m_destruct ();
	return 0;
}
