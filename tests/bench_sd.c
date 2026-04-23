#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "../lib/sd.h"

#define GET_TIME_MS get_time_ms ()

static double get_time_ms (void)
{
	struct timeval tv;
	gettimeofday (&tv, NULL);
	return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

typedef struct {
	int id;
	int ops;
} worker_args_t;

static void *basic_worker (void *arg)
{
	worker_args_t *a = arg;
	int h = sd_alloc (a->ops * 2, sizeof (int), SD_FREE);
	for (int i = 0; i < a->ops; i++) {
		int val = a->id * a->ops + i;
		sd_put (h, &val);
	}
	sd_free (h);
	return NULL;
}

static void *shared_worker (void *arg)
{
	worker_args_t *a = arg;
	int shared = a->id;
	for (int i = 0; i < a->ops; i++) {
		int val = i;
		sd_put (shared, &val);
	}
	return NULL;
}

void benchmark_sd_basic (int ops, const char *name)
{
	double start = GET_TIME_MS;
	sd_init ();

	int h = sd_alloc (ops * 2, sizeof (int), SD_FREE);

	for (int i = 0; i < ops; i++) {
		int val = i;
		sd_put (h, &val);
	}

	for (int i = 0; i < ops; i++) {
		int *v = sd_get (h, i);
		(void)v;
	}

	sd_free (h);
	sd_destruct ();

	double elapsed = GET_TIME_MS - start;
	printf ("%-25s %10.0f ops/sec\n", name, ops * 1000.0 / elapsed);
}

void benchmark_sd_threaded (int threads, int ops_per_thread, const char *name)
{
	sd_init ();

	pthread_t th[16];
	worker_args_t args[16];

	double start = GET_TIME_MS;

	for (int t = 0; t < threads; t++) {
		args[t].id = t;
		args[t].ops = ops_per_thread;
		pthread_create (&th[t], NULL, basic_worker, &args[t]);
	}

	for (int t = 0; t < threads; t++) {
		pthread_join (th[t], NULL);
	}

	double elapsed = GET_TIME_MS - start;
	int total_ops = threads * ops_per_thread;
	printf ("%-25s %10.0f ops/sec\n", name, total_ops * 1000.0 / elapsed);

	sd_destruct ();
}

void benchmark_sd_shared (int threads, int ops_per_thread, const char *name)
{
	sd_init ();

	int shared =
		sd_create_shared (threads * ops_per_thread * 4, sizeof (int));

	pthread_t th[16];
	worker_args_t args[16];

	double start = GET_TIME_MS;

	for (int t = 0; t < threads; t++) {
		args[t].id = shared;
		args[t].ops = ops_per_thread;
		pthread_create (&th[t], NULL, shared_worker, &args[t]);
	}

	for (int t = 0; t < threads; t++) {
		pthread_join (th[t], NULL);
	}

	double elapsed = GET_TIME_MS - start;
	int total_ops = threads * ops_per_thread;
	printf ("%-25s %10.0f ops/sec\n", name, total_ops * 1000.0 / elapsed);

	sd_free (shared);
	sd_destruct ();
}

int main (int argc, char **argv)
{
	int iterations = 100000;

	if (argc > 1) {
		iterations = atoi (argv[1]);
	}

	printf ("\n=== SD Library Benchmark (iterations: %d) ===\n\n",
		iterations);

	printf ("--- Single Operation ---\n");
	benchmark_sd_basic (iterations, "sd: put+get");
	benchmark_sd_basic (iterations * 10, "sd: put+get (large)");

	printf ("\n--- Multi-threaded (isolated lists) ---\n");
	benchmark_sd_threaded (1, iterations, "sd: 1 thread");
	benchmark_sd_threaded (4, iterations, "sd: 4 threads");
	benchmark_sd_threaded (8, iterations, "sd: 8 threads");

	printf ("\n--- Multi-threaded (shared list) ---\n");
	benchmark_sd_shared (1, iterations, "sd: 1 thread shared");
	benchmark_sd_shared (4, iterations, "sd: 4 threads shared");
	benchmark_sd_shared (8, iterations, "sd: 8 threads shared");

	printf ("\n");
	return 0;
}
