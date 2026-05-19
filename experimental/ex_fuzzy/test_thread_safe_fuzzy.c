#include "../lib/mls.h"

#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define THREADS 12
#define ITERS 50000
#define SHARED_HANDLES 4

typedef struct {
	int id;
	int handles[SHARED_HANDLES];
	unsigned int seed;
} fuzz_arg_t;

static volatile int append_count;

static unsigned int fuzz_rand (unsigned int *seed)
{
	*seed = (*seed * 1103515245u) + 12345u;
	return (*seed >> 16) & 0x7fffu;
}

static void exercise_local_alloc_free (unsigned int *seed, int value)
{
	TRACE(1,"");
	int h = m_alloc ((int)(fuzz_rand (seed) % 8), sizeof (int), MFREE);
	int n = (int)(fuzz_rand (seed) % 16);

	for (int i = 0; i < n; i++) {
		int v = value + i;
		assert (m_put (h, &v) >= 0);
	}

	if (n > 0) {
		int copy = 0;
		void *dst = &copy;
		if( m_len(h) > 0 ) {
			assert (m_read (h, 0, &dst, 1) == 0);
		} else {
			WARN("Handle %d: Len: %ld", h & 0xffffff, m_len(h) );
		}
		
	}
	TRACE(1,"start free");
	m_free (h);
	
}

static void *fuzz_worker (void *arg)
{
	fuzz_arg_t *a = arg;

	for (int i = 0; i < ITERS; i++) {
		unsigned int r = fuzz_rand (&a->seed);
		int h = a->handles[r % SHARED_HANDLES];

		switch (r % 6) {
		case 0:
		case 1: {
			int v = (a->id * ITERS) + i;
			assert (m_puti (h, v) >= 0);
			__sync_fetch_and_add (&append_count, 1);
			break;
		}
		case 2: {
			int len = m_len (h);
			assert (len >= 0);
			assert (m_width (h) == (int)sizeof (int));
			break;
		}
		case 3: {
			int len = m_len (h);
			int max = m_bufsize (h);
			assert (len >= 0);
			assert (max > 0);
			break;
		}
		case 4: {
			int copy = 0;
			void *dst = &copy;
			if( m_len(h) ) 	assert (m_read (h, 0, &dst, 1) == 0);
			break;
		}
		default:
			exercise_local_alloc_free (&a->seed, (a->id * ITERS) + i);
			break;
		}
	}

	return NULL;
}

int main (void)
{
	pthread_t threads[THREADS];
	fuzz_arg_t args[THREADS];
	int handles[SHARED_HANDLES];
	int total_len = 0;

	trace_level = 1;
	assert (m_init () >= 0);

	for (int i = 0; i < SHARED_HANDLES; i++) 
		handles[i] = m_alloc (1, sizeof (int), MFREE);

	for (int i = 0; i < THREADS; i++) {
		args[i].id = i;
		args[i].seed = 0xc0ffeeu ^ (unsigned int)(i * 0x9e3779b9u);
		for (int j = 0; j < SHARED_HANDLES; j++)
			args[i].handles[j] = handles[j];
		assert (pthread_create (&threads[i], NULL, fuzz_worker, &args[i]) == 0);
	}

	for (int i = 0; i < THREADS; i++)
		assert (pthread_join (threads[i], NULL) == 0);

	for (int i = 0; i < SHARED_HANDLES; i++) {
		total_len += m_len (handles[i]);
		m_free (handles[i]);
	}

	assert (total_len == append_count);
	m_destruct ();

	printf ("thread-safe fuzzy test passed: %d randomized operations, %d shared appends\n",
		THREADS * ITERS, append_count);
	return 0;
}
