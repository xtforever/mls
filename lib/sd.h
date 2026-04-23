#ifndef SD_H
#define SD_H

#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SD_FREE 0
#define SD_FREE_STRINGS 1
#define SD_FREE_LIST 2
#define SD_FREE_EACH 3
#define SD_MAX_FREE 16

struct sd_lst_st {
	void *data;
	int len;
	int max;
	int width;
	int free_hdl;
	int shared;
	pthread_mutex_t mtx;
	const char *type_name;
};

typedef struct sd_lst_st *sd_lst_t;

extern pthread_mutex_t g_sd_mtx;
extern sd_lst_t *SD_ML;
extern int *SD_FR;
extern int SD_ML_CAP;
extern int SD_FR_TOP;
extern int SD_FR_CAP;
typedef void (*sd_free_fn) (void *);
extern sd_free_fn SD_FREE_FN[SD_MAX_FREE];

int sd_init (void);
void sd_destruct (void);

int _sd_alloc (int line, const char *file, const char *func, int max, int width,
	       int free_hdl);
int _sd_free (int line, const char *file, const char *func, int handle);

int _sd_create_shared (int line, const char *file, const char *func, int max,
		       int width);
int _sd_set_shared (int line, const char *file, const char *func, int handle,
		    int shared);
void _sd_lock (int line, const char *file, const char *func, int handle);
void _sd_unlock (int line, const char *file, const char *func, int handle);

void *_sd_put (int line, const char *file, const char *func, int handle,
	       void *data);
void *_sd_add (int line, const char *file, const char *func, int handle);
void *_sd_get (int line, const char *file, const char *func, int handle,
	       int index);
void *_sd_peek (int line, const char *file, const char *func, int handle,
		int index);
int _sd_len (int line, const char *file, const char *func, int handle);
int _sd_width (int line, const char *file, const char *func, int handle);
int _sd_setlen (int line, const char *file, const char *func, int handle,
		int len);
int _sd_bufsize (int line, const char *file, const char *func, int handle);
void *_sd_buf (int line, const char *file, const char *func, int handle);

void _sd_del (int line, const char *file, const char *func, int handle,
	      int index);
void *_sd_ins (int line, const char *file, const char *func, int handle,
	       int pos, int n);
void _sd_remove (int line, const char *file, const char *func, int handle,
		 int pos, int n);
void *_sd_pop (int line, const char *file, const char *func, int handle);
void _sd_clear (int line, const char *file, const char *func, int handle);
void _sd_resize (int line, const char *file, const char *func, int handle,
		 int size);

int sd_reg_free (int free_hdl, void (*fn) (void *));
int _sd_get_free_hdl (int line, const char *file, const char *func, int handle);

int _sd_alloc_typed (int line, const char *file, const char *func, int max,
		     int width, int free_hdl, const char *type_name);
const char *_sd_get_type_name (int line, const char *file, const char *func,
			       int handle);

#ifdef SD_DEBUG

struct sd_deb_info {
	const char *name;
	const char *file;
	const char *func;
	int line;
	int handle;
	int index;
	int count;
	void *data;
};

extern struct sd_deb_info sd_deb;
extern int sd_trace_level;

void _sd_deb_capture (const char *name, int line, const char *file,
		      const char *func, int handle, int index, int count,
		      void *data);
void _sd_err (const char *msg);
void _sd_warn (const char *msg);
void _sd_trace (int level, const char *msg);

#define SD_ERR(msg, ...)                                                       \
	do {                                                                   \
		fprintf (stderr, "\n[sd error] %s:%d %s(): " msg "\n",         \
			 sd_deb.file, sd_deb.line, sd_deb.func,                \
			 ##__VA_ARGS__);                                       \
		exit (1);                                                      \
	} while (0)

#define SD_WARN(msg, ...)                                                      \
	do {                                                                   \
		fprintf (stderr, "[sd warn] %s:%d %s(): " msg "\n",            \
			 sd_deb.file, sd_deb.line, sd_deb.func,                \
			 ##__VA_ARGS__);                                       \
	} while (0)

#define SD_TRACE(level, msg, ...)                                              \
	do {                                                                   \
		if ((level) >= sd_trace_level && sd_trace_level > 0) {         \
			fprintf (stderr, "[sd trace %d] %s(): " msg "\n",      \
				 level, sd_deb.func, ##__VA_ARGS__);           \
		}                                                              \
	} while (0)

#define SD_DEB_CALL(name, line, file, func, h, i, c, d)                        \
	_sd_deb_capture (name, line, file, func, h, i, c, d)

#else

struct sd_deb_info {
	int dummy;
};

#define SD_ERR(msg, ...)                                                       \
	do {                                                                   \
		fprintf (stderr, "\n[sd error] " msg "\n", ##__VA_ARGS__);     \
		exit (1);                                                      \
	} while (0)

#define SD_WARN(msg, ...)                                                      \
	do {                                                                   \
	} while (0)
#define SD_TRACE(level, msg, ...)                                              \
	do {                                                                   \
	} while (0)
#define SD_DEB_CALL(name, line, file, func, h, i, c, d)                        \
	do {                                                                   \
	} while (0)

#endif

#define sd_alloc(max, width, free_hdl)                                         \
	_sd_alloc (__LINE__, __FILE__, __FUNCTION__, max, width, free_hdl)

#define sd_free(handle) _sd_free (__LINE__, __FILE__, __FUNCTION__, handle)

#define sd_create_shared(max, width)                                           \
	_sd_create_shared (__LINE__, __FILE__, __FUNCTION__, max, width)

#define sd_set_shared(handle, shared)                                          \
	_sd_set_shared (__LINE__, __FILE__, __FUNCTION__, handle, shared)

#define sd_lock(handle) _sd_lock (__LINE__, __FILE__, __FUNCTION__, handle)

#define sd_unlock(handle) _sd_unlock (__LINE__, __FILE__, __FUNCTION__, handle)

#define sd_put(handle, data)                                                   \
	_sd_put (__LINE__, __FILE__, __FUNCTION__, handle, data)

#define sd_add(handle) _sd_add (__LINE__, __FILE__, __FUNCTION__, handle)

#define sd_get(handle, index)                                                  \
	_sd_get (__LINE__, __FILE__, __FUNCTION__, handle, index)

#define sd_peek(handle, index)                                                 \
	_sd_peek (__LINE__, __FILE__, __FUNCTION__, handle, index)

#define sd_len(handle) _sd_len (__LINE__, __FILE__, __FUNCTION__, handle)

#define sd_width(handle) _sd_width (__LINE__, __FILE__, __FUNCTION__, handle)

#define sd_setlen(handle, len)                                                 \
	_sd_setlen (__LINE__, __FILE__, __FUNCTION__, handle, len)

#define sd_bufsize(handle)                                                     \
	_sd_bufsize (__LINE__, __FILE__, __FUNCTION__, handle)

#define sd_buf(handle) _sd_buf (__LINE__, __FILE__, __FUNCTION__, handle)

#define sd_del(handle, index)                                                  \
	_sd_del (__LINE__, __FILE__, __FUNCTION__, handle, index)

#define sd_ins(handle, pos, n)                                                 \
	_sd_ins (__LINE__, __FILE__, __FUNCTION__, handle, pos, n)

#define sd_remove(handle, pos, n)                                              \
	_sd_remove (__LINE__, __FILE__, __FUNCTION__, handle, pos, n)

#define sd_pop(handle) _sd_pop (__LINE__, __FILE__, __FUNCTION__, handle)

#define sd_clear(handle) _sd_clear (__LINE__, __FILE__, __FUNCTION__, handle)

#define sd_resize(handle, size)                                                \
	_sd_resize (__LINE__, __FILE__, __FUNCTION__, handle, size)

#define sd_alloc_typed(max, width, free_hdl, type_name)                        \
	_sd_alloc_typed (__LINE__, __FILE__, __FUNCTION__, max, width,         \
			 free_hdl, type_name)

#define sd_get_type_name(handle)                                               \
	_sd_get_type_name (__LINE__, __FILE__, __FUNCTION__, handle)

#define sd_get_free_hdl(handle)                                                \
	_sd_get_free_hdl (__LINE__, __FILE__, __FUNCTION__, handle)

#ifdef __cplusplus
}
#endif

#endif
