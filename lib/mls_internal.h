#ifndef MLS_INTERNAL_H
#define MLS_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

/* MLS_THREAD_SAFE is always defined (0 or 1) by mls_base.h, which every
   includer of this internal header pulls in first. Value-based `#if` so
   that -DMLS_THREAD_SAFE=0 actually disables threading. */
#if MLS_THREAD_SAFE
#include <pthread.h>
typedef pthread_rwlock_t mls_rwlock_t;

extern pthread_mutex_t ml_lock;
extern pthread_mutex_t cs_map_lock;
#define MLS_MASTER_LOCK() pthread_mutex_lock (&ml_lock)
#define MLS_MASTER_UNLOCK() pthread_mutex_unlock (&ml_lock)
#define CS_MAP_LOCK() pthread_mutex_lock (&cs_map_lock)
#define CS_MAP_UNLOCK() pthread_mutex_unlock (&cs_map_lock)
#else
typedef void mls_rwlock_t;

#define MLS_MASTER_LOCK() ((void)0)
#define MLS_MASTER_UNLOCK() ((void)0)
#define CS_MAP_LOCK() ((void)0)
#define CS_MAP_UNLOCK() ((void)0)
#endif

#define increase_by_percent(a, p) calc_percent (a, p + 100)
#define calc_percent(a, p) (((p) > 0) ? (a) * (p) / 100 : 0)

struct ls_st {
	size_t w, l, max;
	char uaf_protection;
	uint8_t free_hdl;
	mls_rwlock_t *lock;
	char *data;
};
typedef struct ls_st *lst_t;

static inline int REAL_HDL (int m) { return (m & 0xffffff); }
static inline int REAL_UAF (int m) { return (m >> 24) & 0x7f; }

/* Shared state owned by mls_base.c, used by mls_ext.c */
extern struct ls_st ML;
extern int FH;
extern int last_created_hdl;
extern int error_occurred;

/* Optional debug hook: called by m_free() with the real handle number
 * after the handle has been actually released. The debug layer in
 * mls_ext.c (debug builds only) sets it so that frees happening inside
 * the library (list_free/tbl_free freeing owned handles) do not yield
 * false-positive "still allocated" warnings in _m_destruct(). */
extern void (*mls_on_free) (int realhdl);

void *lst (lst_t l, size_t i);
void lst_create (lst_t l, size_t max, size_t w);
int lst_new (lst_t LP, size_t n);
int lst_put (lst_t LP, const void *d);
int lst_next (lst_t l, int *p, void *data);
int lst_read (lst_t l, size_t p, void **data, size_t n);
int lst_write (lst_t lp, size_t p, const void *data, size_t n);
void *lst_peek (lst_t l, size_t i);
void lst_del (lst_t l, size_t p);
void lst_remove (lst_t lp, size_t p, size_t n);
void *lst_ins (lst_t lp, size_t p, size_t n);
void lst_resize (lst_t LP, size_t new_size);

/* non-aborting lst_* variants used by mls_ext.c's _safe() wrappers */
int lst_new_safe (lst_t lp, size_t n);
int lst_write_safe (lst_t lp, size_t p, const void *data, size_t n);
int lst_read_safe (lst_t l, size_t p, void **data, size_t n);
int lst_del_safe (lst_t l, size_t p);
void *lst_ins_safe (lst_t lp, size_t p, size_t n);

lst_t exported_get_list (int r);
lst_t mls_lock_handle (int m, int write);
void mls_unlock_handle (lst_t lp);

lst_t lock_handle (int m, int write);
lst_t lock_handle_safe (int m, int write);
void unlock_handle (lst_t lp);

int new_list (const char *buf, size_t len, size_t max, size_t w, int hdl);

#endif
