#include "mls.h"
#undef ASSERT
#include "greatest.h"
#include <pthread.h>
#include <stdlib.h>
#include <time.h>

#define N_THREADS 8
#define N_OPS 10000
#define MAX_LIST_SIZE (N_THREADS * N_OPS * 2)

#ifndef MLS_THREADSAFE
#warning "Thread safety tests require -DMLS_THREADSAFE for full testing"
#endif

static int verbose = 0;

/* Suite declarations */
SUITE (basic_suite);
SUITE (concurrent_write_suite);
SUITE (concurrent_modify_suite);
SUITE (mixed_operations_suite);
SUITE (stress_suite);

double get_time_ms (void)
{
	struct timespec ts;
	clock_gettime (CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

void *worker_put_int (void *arg)
{
	int *data = (int *)arg;
	for (int i = 0; i < N_OPS; i++) {
		int val = i;
		m_put (*data, &val);
	}
	return NULL;
}

void *worker_put_unique (void *arg)
{
	int *ctx = (int *)arg;
	int handle = ctx[0];
	int thread_id = ctx[1];
	for (int i = 0; i < N_OPS; i++) {
		int val = thread_id * N_OPS + i;
		m_put (handle, &val);
	}
	return NULL;
}

void *worker_ins (void *arg)
{
	int *data = (int *)arg;
	for (int i = 0; i < N_OPS; i++) {
		int idx = rand () % (m_len (*data) + 1);
		m_ins (*data, idx, 1);
	}
	return NULL;
}

void *worker_del (void *arg)
{
	int *data = (int *)arg;
	for (int i = 0; i < N_OPS && m_len (*data) > 0; i++) {
		int idx = rand () % m_len (*data);
		m_del (*data, idx);
	}
	return NULL;
}

void *worker_remove (void *arg)
{
	int *data = (int *)arg;
	for (int i = 0; i < N_OPS / 10 && m_len (*data) > 0; i++) {
		int len = m_len (*data);
		int start = rand () % len;
		int count = rand () % (len - start + 1);
		if (count > 0)
			m_remove (*data, start, count);
	}
	return NULL;
}

void *worker_get (void *arg)
{
	int *data = (int *)arg;
	for (int i = 0; i < N_OPS; i++) {
		int len = m_len (*data);
		if (len > 0) {
			int idx = rand () % len;
			volatile int *val = (int *)mls (*data, idx);
			(void)val;
		}
	}
	return NULL;
}

void *worker_mixed (void *arg)
{
	int *data = (int *)arg;
	for (int i = 0; i < N_OPS; i++) {
		int op = rand () % 7;
		int len;
		switch (op) {
		case 0:
			m_put (*data, &i);
			break;
		case 1:
			len = m_len (*data);
			if (len > 0)
				m_ins (*data, rand () % len, 1);
			break;
		case 2:
			len = m_len (*data);
			if (len > 0)
				m_del (*data, rand () % len);
			break;
		case 3:
			len = m_len (*data);
			if (len > 0) {
				int cnt = rand () % (len / 2 + 1);
				if (cnt > 0)
					m_remove (*data, rand () % len, cnt);
			}
			break;
		case 4:
			m_len (*data);
			break;
		case 5:
			len = m_len (*data);
			if (len > 0)
				(void)mls (*data, rand () % len);
			break;
		case 6:
			m_clear (*data);
			break;
		}
	}
	return NULL;
}

void *worker_create_free (void *arg)
{
	int *ctx = (int *)arg;
	int thread_id = ctx[1];
	for (int i = 0; i < N_OPS / 100; i++) {
		int h = m_create (10, sizeof (int));
		for (int j = 0; j < 100; j++) {
			int val = thread_id * 1000 + i * 100 + j;
			m_put (h, &val);
		}
		m_free (h);
	}
	return NULL;
}

int run_concurrent_test (int list, void *(*worker) (void *), void *arg,
			 int n_threads, const char *name)
{
	pthread_t threads[N_THREADS];
	double start = get_time_ms ();

	for (int i = 0; i < n_threads; i++) {
		if (pthread_create (&threads[i], NULL, worker, arg) != 0) {
			ERR ("pthread_create failed");
		}
	}

	for (int i = 0; i < n_threads; i++) {
		pthread_join (threads[i], NULL);
	}

	double elapsed = get_time_ms () - start;
	if (verbose)
		printf ("  %s: %.1f ms\n", name, elapsed);

	return 0;
}

TEST test_basic_shared_create_free (void)
{
	m_init ();
	int list = m_create_shared (100, sizeof (int));
	if (list == 0) {
		m_destruct ();
		FAIL ();
	}

	for (int i = 0; i < 10; i++) {
		m_put (list, &i);
	}

	m_free (list);
	m_destruct ();

	PASS ();
}

TEST test_concurrent_put_int (void)
{
#ifndef MLS_THREADSAFE
	SKIP ();
#endif
	m_init ();
	int list = m_create_shared (MAX_LIST_SIZE, sizeof (int));
	if (list == 0) {
		m_destruct ();
		SKIP ();
	}
	int shared = list;

	if (verbose)
		printf ("Testing concurrent put with %d threads, %d "
			"ops/thread\n",
			N_THREADS, N_OPS);

	run_concurrent_test (list, worker_put_int, &shared, N_THREADS,
			     "put_int");

	int expected = N_THREADS * N_OPS;
	int actual = m_len (list);

	if (verbose)
		printf ("  Expected: %d, Actual: %d\n", expected, actual);

	m_free (list);
	m_destruct ();

	ASSERT_EQ (expected, actual);
	PASS ();
}

TEST test_concurrent_put_unique (void)
{
#ifndef MLS_THREADSAFE
	SKIP ();
#endif
	m_init ();
	int list = m_create_shared (MAX_LIST_SIZE, sizeof (int));
	if (list == 0) {
		m_destruct ();
		SKIP ();
	}

	int ctx[N_THREADS][2];
	pthread_t threads[N_THREADS];

	double start = get_time_ms ();

	for (int i = 0; i < N_THREADS; i++) {
		ctx[i][0] = list;
		ctx[i][1] = i;
		if (pthread_create (&threads[i], NULL, worker_put_unique,
				    ctx[i]) != 0) {
			ERR ("pthread_create failed");
		}
	}

	for (int i = 0; i < N_THREADS; i++) {
		pthread_join (threads[i], NULL);
	}

	double elapsed = get_time_ms () - start;
	if (verbose)
		printf ("  put_unique: %.1f ms\n", elapsed);

	int expected = N_THREADS * N_OPS;
	int actual = m_len (list);

	if (verbose)
		printf ("  Expected: %d, Actual: %d\n", expected, actual);

	m_free (list);
	m_destruct ();

	ASSERT_EQ (expected, actual);
	PASS ();
}

TEST test_concurrent_ins (void)
{
#ifndef MLS_THREADSAFE
	SKIP ();
#endif
	m_init ();
	int list = m_create_shared (MAX_LIST_SIZE, sizeof (int));
	if (list == 0) {
		m_destruct ();
		SKIP ();
	}
	int shared = list;

	srand (12345);
	for (int i = 0; i < 1000; i++) {
		int val = i;
		m_put (list, &val);
	}

	int expected_ins = N_THREADS * N_OPS;
	int initial_len = m_len (list);

	if (verbose)
		printf ("Testing concurrent ins with %d threads, %d "
			"ops/thread\n",
			N_THREADS, N_OPS);

	run_concurrent_test (list, worker_ins, &shared, N_THREADS, "ins");

	int actual_ins = m_len (list) - initial_len;

	if (verbose)
		printf ("  Inserted: %d, Expected: %d\n", actual_ins,
			expected_ins);

	m_free (list);
	m_destruct ();

	ASSERT_EQ (expected_ins, actual_ins);
	PASS ();
}

TEST test_concurrent_del (void)
{
#ifndef MLS_THREADSAFE
	SKIP ();
#endif
	m_init ();
	int list = m_create_shared (MAX_LIST_SIZE, sizeof (int));
	if (list == 0) {
		m_destruct ();
		SKIP ();
	}

	for (int i = 0; i < MAX_LIST_SIZE / 2; i++) {
		int val = i;
		m_put (list, &val);
	}

	int shared = list;
	int initial_len = m_len (list);

	if (verbose)
		printf ("Testing concurrent del with %d threads, initial len: "
			"%d\n",
			N_THREADS, initial_len);

	run_concurrent_test (list, worker_del, &shared, N_THREADS, "del");

	int actual_len = m_len (list);

	if (verbose)
		printf ("  Remaining: %d (initial: %d)\n", actual_len,
			initial_len);

	m_free (list);
	m_destruct ();

	ASSERT (actual_len >= 0);
	ASSERT (actual_len <= initial_len);
	PASS ();
}

TEST test_concurrent_remove (void)
{
#ifndef MLS_THREADSAFE
	SKIP ();
#endif
	m_init ();
	int list = m_create_shared (MAX_LIST_SIZE, sizeof (int));
	if (list == 0) {
		m_destruct ();
		SKIP ();
	}

	for (int i = 0; i < MAX_LIST_SIZE / 2; i++) {
		int val = i;
		m_put (list, &val);
	}

	int shared = list;
	int initial_len = m_len (list);

	if (verbose)
		printf ("Testing concurrent remove with %d threads, initial "
			"len: %d\n",
			N_THREADS, initial_len);

	run_concurrent_test (list, worker_remove, &shared, N_THREADS, "remove");

	int actual_len = m_len (list);

	if (verbose)
		printf ("  Remaining: %d (initial: %d)\n", actual_len,
			initial_len);

	m_free (list);
	m_destruct ();

	ASSERT (actual_len >= 0);
	ASSERT (actual_len <= initial_len);
	PASS ();
}

TEST test_concurrent_get (void)
{
#ifndef MLS_THREADSAFE
	SKIP ();
#endif
	m_init ();
	int list = m_create_shared (MAX_LIST_SIZE, sizeof (int));
	if (list == 0) {
		m_destruct ();
		SKIP ();
	}

	for (int i = 0; i < 10000; i++) {
		int val = i;
		m_put (list, &val);
	}

	int shared = list;

	if (verbose)
		printf ("Testing concurrent get with %d threads reading, 1 "
			"thread writing\n",
			N_THREADS - 1);

	pthread_t writer;
	pthread_create (&writer, NULL, worker_put_int, &shared);

	run_concurrent_test (list, worker_get, &shared, N_THREADS - 1, "get");

	pthread_join (writer, NULL);

	if (verbose)
		printf ("  Final length: %d\n", m_len (list));

	m_free (list);
	m_destruct ();

	PASS ();
}

TEST test_concurrent_mixed (void)
{
#ifndef MLS_THREADSAFE
	SKIP ();
#endif
	m_init ();
	int list = m_create_shared (MAX_LIST_SIZE, sizeof (int));
	if (list == 0) {
		m_destruct ();
		SKIP ();
	}
	int shared = list;

	srand (54321);

	if (verbose)
		printf ("Testing concurrent mixed ops with %d threads, %d "
			"ops/thread\n",
			N_THREADS, N_OPS);

	run_concurrent_test (list, worker_mixed, &shared, N_THREADS, "mixed");

	if (verbose)
		printf ("  Final length: %d\n", m_len (list));

	m_free (list);
	m_destruct ();

	PASS ();
}

TEST test_long_stress (void)
{
#ifndef MLS_THREADSAFE
	SKIP ();
#endif
	m_init ();
	int list = m_create_shared (MAX_LIST_SIZE, sizeof (int));
	if (list == 0) {
		m_destruct ();
		SKIP ();
	}
	int shared = list;

	double start = get_time_ms ();

	run_concurrent_test (list, worker_mixed, &shared, N_THREADS,
			     "long_stress");

	double elapsed = get_time_ms () - start;
	int final_len = m_len (list);
	int final_max = m_bufsize (list);

	if (verbose) {
		printf ("  Duration: %.1f ms\n", elapsed);
		printf ("  Final length: %d\n", final_len);
		printf ("  Buffer size: %d\n", final_max);
	}

	m_free (list);
	m_destruct ();

	ASSERT (final_len >= 0);
	PASS ();
}

TEST test_create_free_cycle (void)
{
#ifndef MLS_THREADSAFE
	SKIP ();
#endif
	m_init ();

	if (verbose)
		printf ("Testing create/free cycle with %d threads, %d "
			"cycles/thread\n",
			N_THREADS, N_OPS / 100);

	int ctx[N_THREADS][2];
	pthread_t threads[N_THREADS];

	double start = get_time_ms ();

	for (int i = 0; i < N_THREADS; i++) {
		ctx[i][0] = 0;
		ctx[i][1] = i;
		if (pthread_create (&threads[i], NULL, worker_create_free,
				    ctx[i]) != 0) {
			ERR ("pthread_create failed");
		}
	}

	for (int i = 0; i < N_THREADS; i++) {
		pthread_join (threads[i], NULL);
	}

	double elapsed = get_time_ms () - start;
	if (verbose)
		printf ("  create_free_cycle: %.1f ms\n", elapsed);

	m_destruct ();

	PASS ();
}

TEST test_parallel_lists (void)
{
#ifndef MLS_THREADSAFE
	SKIP ();
#endif
	m_init ();

	int lists[N_THREADS];

	if (verbose)
		printf ("Testing %d parallel independent lists\n", N_THREADS);

	for (int i = 0; i < N_THREADS; i++) {
		lists[i] = m_create_shared (MAX_LIST_SIZE, sizeof (int));
		if (lists[i] == 0) {
			for (int j = 0; j < i; j++)
				m_free (lists[j]);
			m_destruct ();
			SKIP ();
		}
	}

	pthread_t threads[N_THREADS];

	for (int i = 0; i < N_THREADS; i++) {
		if (pthread_create (&threads[i], NULL, worker_put_int,
				    &lists[i]) != 0) {
			ERR ("pthread_create failed");
		}
	}

	for (int i = 0; i < N_THREADS; i++) {
		pthread_join (threads[i], NULL);
	}

	for (int i = 0; i < N_THREADS; i++) {
		int len = m_len (lists[i]);
		if (verbose)
			printf ("  List %d: %d elements\n", i, len);
		ASSERT_EQ (N_OPS, len);
		m_free (lists[i]);
	}

	m_destruct ();

	PASS ();
}

SUITE (basic_suite) { RUN_TEST (test_basic_shared_create_free); }
SUITE (concurrent_write_suite)
{
	RUN_TEST (test_concurrent_put_int);
	RUN_TEST (test_concurrent_put_unique);
	RUN_TEST (test_concurrent_ins);
}
SUITE (concurrent_modify_suite)
{
	RUN_TEST (test_concurrent_del);
	RUN_TEST (test_concurrent_remove);
	RUN_TEST (test_concurrent_get);
}
SUITE (mixed_operations_suite) { RUN_TEST (test_concurrent_mixed); }
SUITE (stress_suite)
{
	RUN_TEST (test_long_stress);
	RUN_TEST (test_create_free_cycle);
	RUN_TEST (test_parallel_lists);
}

GREATEST_MAIN_DEFS ();

int main (int argc, char **argv)
{
	if (argc > 1 && strcmp (argv[1], "-v") == 0) {
		verbose = 1;
	}

	GREATEST_INIT ();
	RUN_SUITE (basic_suite);
#ifdef MLS_THREADSAFE
	RUN_SUITE (concurrent_write_suite);
	RUN_SUITE (concurrent_modify_suite);
	RUN_SUITE (mixed_operations_suite);
	RUN_SUITE (stress_suite);
#else
	printf ("NOTE: Thread safety disabled. Compile with -DMLS_THREADSAFE "
		"for thread tests.\n");
#endif

	GREATEST_PRINT_REPORT ();
	return greatest_all_passed () ? 0 : 1;
}
