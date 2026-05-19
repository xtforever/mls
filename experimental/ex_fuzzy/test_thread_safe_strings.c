#include "mls.h"
#include "m_tool.h"

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define THREADS 4
#define ITERS   5000

static const char *const SHARED_STRINGS[] = {
	"Hello", "World", "MLS", "Thread", "Safe", "Fuzz",
};

static int shared_handles[6];

typedef struct {
	int id;
	unsigned int seed;
} str_arg_t;

static unsigned int str_rand(unsigned int *seed)
{
	*seed = (*seed * 1103515245u) + 12345u;
	return (*seed >> 16) & 0x7fffu;
}

static void *str_worker(void *arg)
{
	str_arg_t *a = arg;

	for (int i = 0; i < ITERS; i++) {
		unsigned int r = str_rand(&a->seed);
		int op = r % 9;

		switch (op) {
		case 0: {
			int sh = shared_handles[r % 6];
			int h = s_printf(sh, -1, " iter=%d", i);
			assert(h > 0);
			break;
		}
		case 1: {
			const char *s = SHARED_STRINGS[r % 6];
			int h = s_dup(s);
			assert(h > 0);
			s_free(h);
			break;
		}
		case 2: {
			int h = s_new();
			assert(h > 0);
			assert(s_app(h, "a", "b", "c", NULL) > 0);
			assert(m_len(h) == 4);
			s_free(h);
			break;
		}
		case 3: {
			int sh = shared_handles[r % 6];
			int len = s_strlen(sh);
			assert(len >= 0);
			break;
		}
		case 4: {
			int sh = shared_handles[r % 6];
			int slen = s_strlen(sh);
			if (slen > 1) {
				int a_idx = (int)(str_rand(&a->seed) % (unsigned int)slen);
				int b_idx = a_idx + (int)(str_rand(&a->seed) % (unsigned int)(slen - a_idx));
				int sliced = s_slice(0, 0, sh, a_idx, b_idx);
				assert(sliced > 0);
				s_free(sliced);
			}
			break;
		}
		case 5: {
			const char *s = SHARED_STRINGS[r % 6];
			int h = s_cstr(s);
			assert(h > 0);
			assert(m_str(h) != NULL);
			break;
		}
		case 6: {
			const char *s = SHARED_STRINGS[r % 6];
			int h = s_ccstr(s);
			assert(h > 0);
			assert(m_str(h) != NULL);
			break;
		}
		case 7: {
			const char *s = SHARED_STRINGS[r % 6];
			int h = s_cstrdup(s);
			assert(h > 0);
			assert(m_str(h) != NULL);
			break;
		}
		case 8: {
			int n = (int)(str_rand(&a->seed) % 5) + 1;
			int h = m_alloc((size_t)n, 1, MFREE);
			assert(h > 0);
			for (int j = 0; j < n; j++) {
				m_putc(h, (char)('A' + (a->id + j) % 26));
			}
			assert(m_len(h) == (size_t)n);
			m_free(h);
			break;
		}
		}
	}

	return NULL;
}

int main(void)
{
	pthread_t threads[THREADS];
	str_arg_t args[THREADS];

	trace_level = 0;
	assert(m_init() >= 0);

	/* Pre-seed constant string map so threaded s_cstr/s_ccstr are
	   read-only lookups (conststr_lookup_c is not safe for concurrent
	   insertion — m_binsert stores a raw C-string pointer that gets
	   overwritten with a handle, and another thread can read the
	   intermediate truncated-pointer-as-handle on 64-bit). */
	for (int i = 0; i < 6; i++) {
		int cs = s_cstr(SHARED_STRINGS[i]);
		assert(cs > 0);
	}

	for (int i = 0; i < 6; i++) {
		shared_handles[i] = s_printf(0, 0, "%s", SHARED_STRINGS[i]);
		assert(shared_handles[i] > 0);
	}

	for (int i = 0; i < THREADS; i++) {
		args[i].id = i;
		args[i].seed = 0xdec0deu ^ (unsigned int)(i * 0x9e3779b9u);
		assert(pthread_create(&threads[i], NULL, str_worker, &args[i]) == 0);
	}

	for (int i = 0; i < THREADS; i++)
		assert(pthread_join(threads[i], NULL) == 0);

	for (int i = 0; i < 6; i++) {
		const char *s = m_str(shared_handles[i]);
		assert(strncmp(s, SHARED_STRINGS[i], strlen(SHARED_STRINGS[i])) == 0);
		s_free(shared_handles[i]);
	}

	m_destruct();

	printf("string thread-safe test passed: %d threads, %d iters each\n",
	       THREADS, ITERS);
	return 0;
}
