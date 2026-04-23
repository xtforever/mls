#include "../lib/sd.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define PASS 0
#define FAIL 1

int tests_run = 0;
int tests_passed = 0;
int tests_failed = 0;

double get_time_ms (void)
{
	struct timeval tv;
	gettimeofday (&tv, NULL);
	return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

#define ASSERT_EQ(a, b, msg)                                                   \
	do {                                                                   \
		tests_run++;                                                   \
		if ((a) == (b)) {                                              \
			tests_passed++;                                        \
			printf ("  PASS: %s\n", msg);                          \
		} else {                                                       \
			tests_failed++;                                        \
			printf ("  FAIL: %s (expected %d, got %d)\n", msg,     \
				(int)(b), (int)(a));                           \
			return FAIL;                                           \
		}                                                              \
	} while (0)

#define ASSERT_NEQ(a, b, msg)                                                  \
	do {                                                                   \
		tests_run++;                                                   \
		if ((a) != (b)) {                                              \
			tests_passed++;                                        \
			printf ("  PASS: %s\n", msg);                          \
		} else {                                                       \
			tests_failed++;                                        \
			printf ("  FAIL: %s\n", msg);                          \
			return FAIL;                                           \
		}                                                              \
	} while (0)

#define ASSERT_TRUE(a, msg)                                                    \
	do {                                                                   \
		tests_run++;                                                   \
		if (a) {                                                       \
			tests_passed++;                                        \
			printf ("  PASS: %s\n", msg);                          \
		} else {                                                       \
			tests_failed++;                                        \
			printf ("  FAIL: %s\n", msg);                          \
			return FAIL;                                           \
		}                                                              \
	} while (0)

#define TEST(name) int test_##name (void)

TEST (basic_create_free)
{
	sd_init ();
	int h = sd_alloc (100, sizeof (int), SD_FREE);
	ASSERT_NEQ (h, 0, "alloc returns valid handle");
	ASSERT_EQ (sd_len (h), 0, "new list has length 0");
	sd_free (h);
	sd_destruct ();
	return PASS;
}

TEST (basic_put_get)
{
	sd_init ();
	int h = sd_alloc (100, sizeof (int), SD_FREE);
	int val = 42;
	sd_put (h, &val);
	ASSERT_EQ (sd_len (h), 1, "length is 1 after put");
	int *got = sd_get (h, 0);
	ASSERT_NEQ (got, NULL, "get returns non-null");
	ASSERT_EQ (*got, 42, "got correct value");
	sd_free (h);
	sd_destruct ();
	return PASS;
}

TEST (multiple_puts)
{
	sd_init ();
	int h = sd_alloc (100, sizeof (int), SD_FREE);
	for (int i = 0; i < 100; i++) {
		sd_put (h, &i);
	}
	ASSERT_EQ (sd_len (h), 100, "length is 100 after 100 puts");
	for (int i = 0; i < 100; i++) {
		int *got = sd_get (h, i);
		ASSERT_EQ (*got, i, "got correct value at index");
	}
	sd_free (h);
	sd_destruct ();
	return PASS;
}

TEST (variable_width)
{
	sd_init ();
	int h1 = sd_alloc (100, sizeof (int), SD_FREE);
	int h2 = sd_alloc (100, sizeof (long), SD_FREE);
	ASSERT_EQ (sd_width (h1), sizeof (int), "int width correct");
	ASSERT_EQ (sd_width (h2), sizeof (long), "long width correct");
	long lv = 12345678901234L;
	sd_put (h1, &(int){10});
	sd_put (h2, &lv);
	ASSERT_EQ (*(int *)sd_get (h1, 0), 10, "int stored correctly");
	ASSERT_EQ (*(long *)sd_get (h2, 0), lv, "long stored correctly");
	sd_free (h1);
	sd_free (h2);
	sd_destruct ();
	return PASS;
}

TEST (multiple_lists)
{
	sd_init ();
	int h[10];
	for (int i = 0; i < 10; i++) {
		h[i] = sd_alloc (100, sizeof (int), SD_FREE);
		int val = i * 100;
		sd_put (h[i], &val);
	}
	for (int i = 0; i < 10; i++) {
		ASSERT_EQ (*(int *)sd_get (h[i], 0), i * 100,
			   "list contents preserved");
	}
	for (int i = 0; i < 10; i++) {
		sd_free (h[i]);
	}
	sd_destruct ();
	return PASS;
}

TEST (del)
{
	sd_init ();
	int h = sd_alloc (100, sizeof (int), SD_FREE);
	for (int i = 0; i < 5; i++)
		sd_put (h, &i);
	ASSERT_EQ (sd_len (h), 5, "length is 5");
	sd_del (h, 2);
	ASSERT_EQ (sd_len (h), 4, "length is 4 after del");
	ASSERT_EQ (*(int *)sd_get (h, 0), 0, "first element unchanged");
	ASSERT_EQ (*(int *)sd_get (h, 1), 1, "second element unchanged");
	ASSERT_EQ (*(int *)sd_get (h, 2), 3, "third element shifted");
	ASSERT_EQ (*(int *)sd_get (h, 3), 4, "fourth element shifted");
	sd_free (h);
	sd_destruct ();
	return PASS;
}

TEST (ins)
{
	sd_init ();
	int h = sd_alloc (100, sizeof (int), SD_FREE);
	for (int i = 0; i < 5; i++)
		sd_put (h, &i);
	sd_ins (h, 2, 1);
	ASSERT_EQ (sd_len (h), 6, "length is 6 after ins");
	ASSERT_EQ (*(int *)sd_get (h, 0), 0, "first unchanged");
	ASSERT_EQ (*(int *)sd_get (h, 1), 1, "second unchanged");
	ASSERT_EQ (*(int *)sd_get (h, 2), 0, "inserted zero");
	ASSERT_EQ (*(int *)sd_get (h, 3), 2, "shifted element");
	sd_free (h);
	sd_destruct ();
	return PASS;
}

TEST (setlen)
{
	sd_init ();
	int h = sd_alloc (100, sizeof (int), SD_FREE);
	for (int i = 0; i < 10; i++)
		sd_put (h, &i);
	sd_setlen (h, 5);
	ASSERT_EQ (sd_len (h), 5, "length set to 5");
	sd_setlen (h, 8);
	ASSERT_EQ (sd_len (h), 8, "length set to 8");
	sd_free (h);
	sd_destruct ();
	return PASS;
}

TEST (clear)
{
	sd_init ();
	int h = sd_alloc (100, sizeof (int), SD_FREE);
	for (int i = 0; i < 10; i++)
		sd_put (h, &i);
	sd_clear (h);
	ASSERT_EQ (sd_len (h), 0, "length is 0 after clear");
	sd_free (h);
	sd_destruct ();
	return PASS;
}

TEST (pop)
{
	sd_init ();
	int h = sd_alloc (100, sizeof (int), SD_FREE);
	for (int i = 0; i < 5; i++)
		sd_put (h, &i);
	int *p = sd_pop (h);
	ASSERT_EQ (*p, 4, "pop returns last element");
	ASSERT_EQ (sd_len (h), 4, "length after pop");
	sd_free (h);
	sd_destruct ();
	return PASS;
}

TEST (resize)
{
	sd_init ();
	int h = sd_alloc (10, sizeof (int), SD_FREE);
	ASSERT_EQ (sd_bufsize (h), 10, "initial buffer size");
	for (int i = 0; i < 10; i++)
		sd_put (h, &i);
	sd_resize (h, 20);
	ASSERT_EQ (sd_bufsize (h), 20, "buffer size after resize");
	sd_free (h);
	sd_destruct ();
	return PASS;
}

TEST (shared_list)
{
	sd_init ();
	int h = sd_alloc (100, sizeof (int), SD_FREE);
	ASSERT_NEQ (h, 0, "alloc returns valid handle");
	sd_set_shared (h, 1);
	sd_free (h);
	sd_destruct ();
	return PASS;
}

#define RUN_TEST(name)                                                         \
	do {                                                                   \
		printf ("Running %s...\n", #name);                             \
		if (test_##name () == PASS) {                                  \
			printf ("  RESULT: PASS\n\n");                         \
		} else {                                                       \
			printf ("  RESULT: FAIL\n\n");                         \
			return 1;                                              \
		}                                                              \
	} while (0)

typedef struct {
	char *name;
	int (*fn) (void);
} test_entry_t;

test_entry_t core_tests[] = {{"basic_create_free", test_basic_create_free},
			     {"basic_put_get", test_basic_put_get},
			     {"multiple_puts", test_multiple_puts},
			     {"variable_width", test_variable_width},
			     {"multiple_lists", test_multiple_lists},
			     {"del", test_del},
			     {"ins", test_ins},
			     {"setlen", test_setlen},
			     {"clear", test_clear},
			     {"pop", test_pop},
			     {"resize", test_resize},
			     {"shared_list", test_shared_list},
			     {NULL, NULL}};

int run_core_tests (void)
{
	printf ("\n=== Core Tests ===\n\n");
	for (test_entry_t *t = core_tests; t->name; t++) {
		printf ("Running %s...\n", t->name);
		int result = t->fn ();
		if (result == PASS) {
			printf ("  RESULT: PASS\n\n");
		} else {
			printf ("  RESULT: FAIL\n\n");
			return 1;
		}
	}
	return 0;
}

#define N_THREADS 8
#define OPS_PER_THREAD 10000
#define RUN_DURATION_MS 30000

int stress_should_stop = 0;

void *stress_worker (void *arg)
{
	int thread_id = *(int *)arg;
	unsigned int seed = time (NULL) ^ pthread_self ();

	while (!__atomic_load_n (&stress_should_stop, __ATOMIC_RELAXED)) {
		int h = sd_alloc (512, sizeof (int), SD_FREE);
		if (h <= 0)
			continue;

		for (int i = 0;
		     i < OPS_PER_THREAD &&
		     !__atomic_load_n (&stress_should_stop, __ATOMIC_RELAXED);
		     i++) {
			int op = rand_r (&seed) % 5;
			switch (op) {
			case 0: {
				int val = thread_id * OPS_PER_THREAD + i;
				sd_put (h, &val);
				break;
			}
			case 1: {
				int len = sd_len (h);
				if (len > 0) {
					int idx = rand_r (&seed) % len;
					sd_del (h, idx);
				}
				break;
			}
			case 2: {
				sd_len (h);
				break;
			}
			case 3: {
				int len = sd_len (h);
				if (len > 0) {
					int idx = rand_r (&seed) % len;
					sd_get (h, idx);
				}
				break;
			}
			case 4: {
				int pos = rand_r (&seed) % 10;
				sd_ins (h, pos, 1);
				break;
			}
			}
		}
		sd_free (h);
	}
	return NULL;
}

int run_thread_test (int duration_ms)
{
	printf ("\n=== Thread Safety Test (%d ms) ===\n\n", duration_ms);

	sd_init ();

	pthread_t threads[N_THREADS];
	int thread_ids[N_THREADS];

	double start = get_time_ms ();
	__atomic_store_n (&stress_should_stop, 0, __ATOMIC_RELAXED);

	for (int i = 0; i < N_THREADS; i++) {
		thread_ids[i] = i;
		pthread_create (&threads[i], NULL, stress_worker,
				&thread_ids[i]);
	}

	while (get_time_ms () - start < duration_ms) {
		usleep (100000);
	}

	__atomic_store_n (&stress_should_stop, 1, __ATOMIC_RELAXED);

	for (int i = 0; i < N_THREADS; i++) {
		pthread_join (threads[i], NULL);
	}

	double elapsed = get_time_ms () - start;
	printf ("  Duration: %.1f ms\n", elapsed);
	printf ("  Threads: %d\n", N_THREADS);
	printf ("  Ops per thread: %d\n", OPS_PER_THREAD);
	printf ("  RESULT: PASS (no crashes)\n\n");

	sd_destruct ();
	return 0;
}

int main (int argc, char **argv)
{
	int duration = RUN_DURATION_MS;

	for (int i = 1; i < argc; i++) {
		if (strcmp (argv[i], "-d") == 0 && i + 1 < argc) {
			duration = atoi (argv[++i]);
		}
	}

	printf ("SDS Thread-Safe Library Test Suite\n");
	printf ("==================================\n\n");

	if (run_core_tests () != 0) {
		printf ("Core tests FAILED\n");
		return 1;
	}

	printf ("Core tests PASSED\n\n");

	if (run_thread_test (duration) != 0) {
		printf ("Thread tests FAILED\n");
		return 1;
	}

	printf ("All tests PASSED\n");
	return 0;
}
