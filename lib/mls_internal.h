#ifndef MLS_INTERNAL_H
#define MLS_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

/* Default MLS_THREAD_SAFE to 1 on Unix/POSIX platforms where
   pthreads is universally available. Users can override with
   -DMLS_THREAD_SAFE=0 on platforms without pthreads. */
#ifndef MLS_THREAD_SAFE
#if defined(__unix__) || defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__)
#define MLS_THREAD_SAFE 1
#else
#define MLS_THREAD_SAFE 0
#endif
#endif

#ifdef MLS_THREAD_SAFE
#include <pthread.h>
typedef pthread_rwlock_t mls_rwlock_t;
#else
typedef void mls_rwlock_t;
#endif

#define increase_by_percent(a, p) calc_percent(a, p + 100)
#define calc_percent(a, p) (((p) > 0) ? (a) * (p) / 100 : 0)

struct ls_st {
	size_t w, l, max;
	char uaf_protection;
	uint8_t free_hdl;
	mls_rwlock_t *lock;
	char *data;
};
typedef struct ls_st *lst_t;

void  *lst       (lst_t l, size_t i);
void   lst_create(lst_t l, size_t max, size_t w);
int    lst_new   (lst_t LP, size_t n);
int    lst_put   (lst_t LP, const void *d);
int    lst_next  (lst_t l, int *p, void *data);
int    lst_read  (lst_t l, size_t p, void **data, size_t n);
int    lst_write (lst_t lp, size_t p, const void *data, size_t n);
void  *lst_peek  (lst_t l, size_t i);
void   lst_del   (lst_t l, size_t p);
void   lst_remove(lst_t lp, size_t p, size_t n);
void  *lst_ins   (lst_t lp, size_t p, size_t n);
void   lst_resize(lst_t LP, size_t new_size);

lst_t exported_get_list(int r);

#endif
