#ifndef MLS_THREAD_H
#define MLS_THREAD_H

#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef MLS_THREADSAFE

#define MLS_LOCK(l)                                                            \
	do {                                                                   \
		if ((l)->shared)                                               \
			pthread_mutex_lock (&(l)->mtx);                        \
	} while (0)

#define MLS_UNLOCK(l)                                                          \
	do {                                                                   \
		if ((l)->shared)                                               \
			pthread_mutex_unlock (&(l)->mtx);                      \
	} while (0)

#else

#define MLS_LOCK(l) ((void)0)
#define MLS_UNLOCK(l) ((void)0)

#endif

#ifdef __cplusplus
}
#endif

#endif
