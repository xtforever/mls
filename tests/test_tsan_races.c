#include "../lib/mls.h"
#include "../lib/m_tool.h"

#ifdef MLS_THREAD_SAFE

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define THREADS  4
#define ITERS    500

static int handles[ITERS];

typedef struct { int start; int end; } range_t;

static void *is_freed_checker (void *arg)
{
	(void)arg;
	for (int round = 0; round < 3; round++)
		for (int i = 0; i < ITERS; i++)
			m_is_freed (handles[i]);
	return NULL;
}

static void *freer_worker (void *arg)
{
	range_t *r = arg;
	for (int i = r->start; i < r->end; i++)
		m_free_safe (handles[i]);
	return NULL;
}

static int test_is_freed_concurrent (void)
{
	for (int i = 0; i < ITERS; i++) {
		handles[i] = m_alloc (4, sizeof (int), MFREE);
		m_puti (handles[i], i);
	}

	pthread_t checkers[THREADS], freers[2];
	range_t r0 = { 0, ITERS / 2 };
	range_t r1 = { ITERS / 2, ITERS };

	for (int i = 0; i < THREADS; i++)
		assert (pthread_create (&checkers[i], NULL, is_freed_checker, NULL) == 0);
	assert (pthread_create (&freers[0], NULL, freer_worker, &r0) == 0);
	assert (pthread_create (&freers[1], NULL, freer_worker, &r1) == 0);

	for (int i = 0; i < THREADS; i++)
		pthread_join (checkers[i], NULL);
	pthread_join (freers[0], NULL);
	pthread_join (freers[1], NULL);

	int ok = 1;
	for (int i = 0; i < ITERS; i++) {
		if (!m_is_freed (handles[i]))
			ok = 0;
	}
	printf ("  m_is_freed concurrent: %s\n", ok ? "OK" : "FAIL");
	return ok;
}

static int str_hdls[ITERS];

static void *str_reader (void *arg)
{
	(void)arg;
	for (int round = 0; round < 3; round++)
		for (int i = 0; i < ITERS; i++) {
			const char *s = m_str (str_hdls[i]);
			(void)s;
		}
	return NULL;
}

static void *str_appender (void *arg)
{
	range_t *r = arg;
	for (int round = 0; round < 2; round++)
		for (int i = r->start; i < r->end; i++)
			s_app1 (str_hdls[i], "x");
	return NULL;
}

static int test_m_str_concurrent (void)
{
	for (int i = 0; i < ITERS; i++)
		str_hdls[i] = s_printf (0, 0, "hello-%d", i);

	pthread_t readers[THREADS], appenders[2];
	range_t r0 = { 0, ITERS / 2 };
	range_t r1 = { ITERS / 2, ITERS };

	for (int i = 0; i < THREADS; i++)
		assert (pthread_create (&readers[i], NULL, str_reader, NULL) == 0);
	assert (pthread_create (&appenders[0], NULL, str_appender, &r0) == 0);
	assert (pthread_create (&appenders[1], NULL, str_appender, &r1) == 0);

	for (int i = 0; i < THREADS; i++)
		pthread_join (readers[i], NULL);
	pthread_join (appenders[0], NULL);
	pthread_join (appenders[1], NULL);

	int ok = 1;
	for (int i = 0; i < ITERS; i++) {
		if (m_is_freed (str_hdls[i])) { ok = 0; break; }
		m_free (str_hdls[i]);
	}
	printf ("  m_str concurrent: %s\n", ok ? "OK" : "FAIL");
	return ok;
}

static void *ring_producer (void *arg)
{
	int r = *(int *)arg;
	for (int i = 0; i < ITERS; i++) {
		while (ring_full (r))
			;
		ring_put (r, i);
	}
	return NULL;
}

static void *ring_consumer (void *arg)
{
	int r = *(int *)arg;
	int count = 0;
	while (count < ITERS) {
		if (!ring_empty (r)) {
			int v = ring_get (r);
			if (v >= 0)
				count++;
		}
	}
	return NULL;
}

static int test_ring_concurrent (void)
{
	int r = ring_create (256);
	pthread_t prod, cons;

	assert (pthread_create (&prod, NULL, ring_producer, &r) == 0);
	assert (pthread_create (&cons, NULL, ring_consumer, &r) == 0);

	pthread_join (prod, NULL);
	pthread_join (cons, NULL);

	int empty = ring_empty (r);
	ring_free (r);

	printf ("  ring concurrent: empty=%d %s\n", empty, empty ? "OK" : "FAIL");
	return empty;
}

extern size_t m_count_allocated (void);
extern size_t m_total_bytes (void);

static void *diag_worker (void *arg)
{
	(void)arg;
	for (int i = 0; i < ITERS; i++) {
		m_count_allocated ();
		m_total_bytes ();
	}
	return NULL;
}

static void *churn_worker (void *arg)
{
	(void)arg;
	for (int i = 0; i < ITERS; i++) {
		int h = m_alloc (8, sizeof (int), MFREE);
		m_puti (h, i);
		m_free (h);
	}
	return NULL;
}

static int test_diag_concurrent (void)
{
	pthread_t diag_threads[2], churn_threads[2];

	for (int i = 0; i < 2; i++)
		assert (pthread_create (&churn_threads[i], NULL, churn_worker, NULL) == 0);
	for (int i = 0; i < 2; i++)
		assert (pthread_create (&diag_threads[i], NULL, diag_worker, NULL) == 0);

	for (int i = 0; i < 2; i++)
		pthread_join (churn_threads[i], NULL);
	for (int i = 0; i < 2; i++)
		pthread_join (diag_threads[i], NULL);

	printf ("  diagnostics concurrent: OK\n");
	return 1;
}

int main (void)
{
	int passed = 0, failed = 0;

	m_init ();
	trace_level = 0;

#define RUN(t)  do { \
	printf ("%s ...\n", #t); \
	if (t ()) passed++; else failed++; \
} while (0)

	RUN (test_is_freed_concurrent);
	RUN (test_m_str_concurrent);
	RUN (test_ring_concurrent);
	RUN (test_diag_concurrent);

	m_destruct ();

	printf ("\n%d passed, %d failed\n", passed, failed);
	return failed > 0 ? 1 : 0;
}

#else

int main (void)
{
	printf ("TSAN race tests skipped (MLS_THREAD_SAFE not defined)\n");
	return 0;
}

#endif
