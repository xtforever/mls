#ifndef SD_THREAD_H
#define SD_THREAD_H

#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef SD_THREADSAFE

#define SD_GLOBAL_LOCK() pthread_mutex_lock (&g_sd_mtx)
#define SD_GLOBAL_UNLOCK() pthread_mutex_unlock (&g_sd_mtx)

#define SD_LOCK(l)                                                             \
	do {                                                                   \
		if ((l)->shared)                                               \
			pthread_mutex_lock (&(l)->mtx);                        \
	} while (0)

#define SD_UNLOCK(l)                                                           \
	do {                                                                   \
		if ((l)->shared)                                               \
			pthread_mutex_unlock (&(l)->mtx);                      \
	} while (0)

#else

#define SD_GLOBAL_LOCK() ((void)0)
#define SD_GLOBAL_UNLOCK() ((void)0)
#define SD_LOCK(l) ((void)0)
#define SD_UNLOCK(l) ((void)0)

#endif

#ifdef __cplusplus
}
#endif

#endif
