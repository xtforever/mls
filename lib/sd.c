#include "sd.h"
#include "sd_thread.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAP 512
#define GROWTH_FACTOR 2

pthread_mutex_t g_sd_mtx;
sd_lst_t *SD_ML;
int *SD_FR;
int SD_ML_CAP;
int SD_FR_TOP;
int SD_FR_CAP;
sd_free_fn SD_FREE_FN[SD_MAX_FREE];

#ifdef SD_DEBUG
struct sd_deb_info sd_deb;
int sd_trace_level = 0;

void _sd_deb_capture (const char *name, int line, const char *file,
		      const char *func, int handle, int index, int count,
		      void *data)
{
	sd_deb.name = name;
	sd_deb.line = line;
	sd_deb.file = file;
	sd_deb.func = func;
	sd_deb.handle = handle;
	sd_deb.index = index;
	sd_deb.count = count;
	sd_deb.data = data;
}

void _sd_err (const char *msg)
{
	fprintf (stderr, "\n[sd error] %s", msg);
	exit (1);
}

void _sd_warn (const char *msg) { fprintf (stderr, "[sd warn] %s", msg); }

void _sd_trace (int level, const char *msg)
{
	if (level >= sd_trace_level && sd_trace_level > 0) {
		fprintf (stderr, "[sd trace %d] %s", level, msg);
	}
}
#endif

static void default_free (void *d) { free (d); }
static void default_free_strings (void *d) { free (*(void **)d); }
static void default_free_list (void *d) { _sd_free (0, NULL, NULL, *(int *)d); }
static void default_free_each (void *d)
{
	int h = *(int *)d;
	if (h > 0)
		_sd_free (0, NULL, NULL, h);
}

int sd_init (void)
{
	pthread_mutexattr_t attr;
	pthread_mutexattr_init (&attr);
	pthread_mutexattr_settype (&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init (&g_sd_mtx, &attr);
	pthread_mutexattr_destroy (&attr);

	SD_ML_CAP = INITIAL_CAP;
	SD_ML = calloc (SD_ML_CAP, sizeof (sd_lst_t));
	if (!SD_ML)
		return -1;

	SD_FR_CAP = INITIAL_CAP;
	SD_FR = calloc (SD_FR_CAP, sizeof (int));
	if (!SD_FR) {
		free (SD_ML);
		return -1;
	}
	SD_FR_TOP = 0;

	SD_FREE_FN[SD_FREE] = default_free;
	SD_FREE_FN[SD_FREE_STRINGS] = default_free_strings;
	SD_FREE_FN[SD_FREE_LIST] = default_free_list;
	SD_FREE_FN[SD_FREE_EACH] = default_free_each;

	return 0;
}

void sd_destruct (void)
{
#ifdef SD_DEBUG
	sd_deb.file = NULL;
#endif
	if (!SD_ML)
		return;

	for (int i = 0; i < SD_ML_CAP; i++) {
		if (SD_ML[i]) {
			if (SD_ML[i]->free_hdl > 0 &&
			    SD_ML[i]->free_hdl < SD_MAX_FREE) {
				for (int j = 0; j < SD_ML[i]->len; j++) {
					void *elem = (char *)SD_ML[i]->data +
						     j * SD_ML[i]->width;
					SD_FREE_FN[SD_ML[i]->free_hdl](elem);
				}
			}
			free (SD_ML[i]->data);
			free (SD_ML[i]);
		}
	}
	free (SD_ML);
	free (SD_FR);
	pthread_mutex_destroy (&g_sd_mtx);
	SD_ML = NULL;
}

static sd_lst_t get_ptr (int handle)
{
	int idx = (handle & 0xffffff) - 1;
	if (idx < 0 || idx >= SD_ML_CAP || !SD_ML[idx])
		return NULL;
	return SD_ML[idx];
}

static int alloc_slot (void)
{
	if (SD_FR_TOP > 0) {
		return SD_FR[--SD_FR_TOP];
	}
	for (int i = 0; i < SD_ML_CAP; i++) {
		if (!SD_ML[i])
			return i;
	}
	int old_cap = SD_ML_CAP;
	SD_ML_CAP *= 2;
	SD_ML = realloc (SD_ML, SD_ML_CAP * sizeof (sd_lst_t));
	memset (SD_ML + old_cap, 0, old_cap * sizeof (sd_lst_t));
	return old_cap;
}

static void free_slot (int idx)
{
	if (SD_FR_TOP >= SD_FR_CAP) {
		SD_FR_CAP *= 2;
		SD_FR = realloc (SD_FR, SD_FR_CAP * sizeof (int));
	}
	SD_FR[SD_FR_TOP++] = idx;
}

int _sd_alloc (int line, const char *file, const char *func, int max, int width,
	       int free_hdl)
{
	SD_DEB_CALL ("sd_alloc", line, file, func, 0, -1, max, NULL);
	SD_GLOBAL_LOCK ();
	int slot = alloc_slot ();
	sd_lst_t lst = calloc (1, sizeof (struct sd_lst_st));
	if (!lst || !SD_ML) {
		if (lst)
			free (lst);
		SD_GLOBAL_UNLOCK ();
		return -1;
	}
	lst->data = calloc (max > 0 ? max : 1, width);
	if (!lst->data) {
		free (lst);
		SD_ML[slot] = NULL;
		SD_GLOBAL_UNLOCK ();
		return -1;
	}
	lst->max = max;
	lst->width = width;
	lst->free_hdl = free_hdl;
	lst->shared = 0;
	pthread_mutex_init (&lst->mtx, NULL);
	SD_ML[slot] = lst;
	SD_GLOBAL_UNLOCK ();
	return slot + 1;
}

int _sd_free (int line, const char *file, const char *func, int handle)
{
	SD_DEB_CALL ("sd_free", line, file, func, handle, -1, 0, NULL);
	if (handle <= 0)
		return 0;
	SD_GLOBAL_LOCK ();
	sd_lst_t lst = get_ptr (handle);
	if (!lst) {
		SD_GLOBAL_UNLOCK ();
		return -1;
	}
	int slot = (handle & 0xffffff) - 1;
	if (lst->free_hdl > 0 && lst->free_hdl < SD_MAX_FREE) {
		for (int i = 0; i < lst->len; i++) {
			void *elem = (char *)lst->data + i * lst->width;
			SD_FREE_FN[lst->free_hdl](elem);
		}
	}
	free (lst->data);
	pthread_mutex_destroy (&lst->mtx);
	free (lst);
	SD_ML[slot] = NULL;
	free_slot (slot);
	SD_GLOBAL_UNLOCK ();
	return 0;
}

int _sd_create_shared (int line, const char *file, const char *func, int max,
		       int width)
{
	SD_DEB_CALL ("sd_create_shared", line, file, func, 0, -1, max, NULL);
	int h = _sd_alloc (line, file, func, max, width, SD_FREE);
	if (h <= 0)
		return h;
	SD_GLOBAL_LOCK ();
	sd_lst_t lst = get_ptr (h);
	if (lst)
		lst->shared = 1;
	SD_GLOBAL_UNLOCK ();
	return h;
}

int _sd_set_shared (int line, const char *file, const char *func, int handle,
		    int shared)
{
	SD_DEB_CALL ("sd_set_shared", line, file, func, handle, -1, 0, NULL);
	SD_GLOBAL_LOCK ();
	sd_lst_t lst = get_ptr (handle);
	if (!lst) {
		SD_GLOBAL_UNLOCK ();
		return -1;
	}
	lst->shared = shared;
	SD_GLOBAL_UNLOCK ();
	return 0;
}

void _sd_lock (int line, const char *file, const char *func, int handle)
{
	SD_DEB_CALL ("sd_lock", line, file, func, handle, -1, 0, NULL);
	SD_GLOBAL_LOCK ();
	sd_lst_t lst = get_ptr (handle);
	if (lst)
		SD_LOCK (lst);
	SD_GLOBAL_UNLOCK ();
}

void _sd_unlock (int line, const char *file, const char *func, int handle)
{
	SD_DEB_CALL ("sd_unlock", line, file, func, handle, -1, 0, NULL);
	SD_GLOBAL_LOCK ();
	sd_lst_t lst = get_ptr (handle);
	if (lst)
		SD_UNLOCK (lst);
	SD_GLOBAL_UNLOCK ();
}

void *_sd_put (int line, const char *file, const char *func, int handle,
	       void *data)
{
	SD_DEB_CALL ("sd_put", line, file, func, handle, -1, 1, data);
	SD_GLOBAL_LOCK ();
	sd_lst_t lst = get_ptr (handle);
	if (!lst) {
		SD_GLOBAL_UNLOCK ();
		SD_ERR ("invalid handle %d", handle);
	}

	if (lst->len >= lst->max) {
		int new_max = lst->max * GROWTH_FACTOR;
		if (new_max < 4)
			new_max = 4;
		void *new_data = realloc (lst->data, new_max * lst->width);
		if (!new_data) {
			SD_GLOBAL_UNLOCK ();
			SD_ERR ("out of memory");
		}
		lst->data = new_data;
		lst->max = new_max;
	}
	SD_GLOBAL_UNLOCK ();

	SD_LOCK (lst);
	void *dst = (char *)lst->data + lst->len * lst->width;
	memcpy (dst, data, lst->width);
	lst->len++;
	SD_UNLOCK (lst);
	return dst;
}

void *_sd_add (int line, const char *file, const char *func, int handle)
{
	return _sd_put (line, file, func, handle, (void *)"");
}

void *_sd_get (int line, const char *file, const char *func, int handle,
	       int index)
{
	SD_DEB_CALL ("sd_get", line, file, func, handle, index, 0, NULL);
	SD_GLOBAL_LOCK ();
	sd_lst_t lst = get_ptr (handle);
	if (!lst) {
		SD_GLOBAL_UNLOCK ();
		SD_ERR ("invalid handle %d", handle);
	}
	SD_GLOBAL_UNLOCK ();

	SD_LOCK (lst);
	if (index < 0 || index >= lst->len) {
		SD_UNLOCK (lst);
		SD_ERR ("index %d out of bounds [0, %d)", index, lst->len);
	}
	void *res = (char *)lst->data + index * lst->width;
	SD_UNLOCK (lst);
	return res;
}

void *_sd_peek (int line, const char *file, const char *func, int handle,
		int index)
{
	SD_DEB_CALL ("sd_peek", line, file, func, handle, index, 0, NULL);
	SD_GLOBAL_LOCK ();
	sd_lst_t lst = get_ptr (handle);
	if (!lst) {
		SD_GLOBAL_UNLOCK ();
		return NULL;
	}
	SD_GLOBAL_UNLOCK ();

	SD_LOCK (lst);
	void *res = (char *)lst->data + index * lst->width;
	SD_UNLOCK (lst);
	return res;
}

int _sd_len (int line, const char *file, const char *func, int handle)
{
	SD_DEB_CALL ("sd_len", line, file, func, handle, -1, 0, NULL);
	SD_GLOBAL_LOCK ();
	sd_lst_t lst = get_ptr (handle);
	if (!lst) {
		SD_GLOBAL_UNLOCK ();
		return 0;
	}
	SD_GLOBAL_UNLOCK ();

	SD_LOCK (lst);
	int len = lst->len;
	SD_UNLOCK (lst);
	return len;
}

int _sd_width (int line, const char *file, const char *func, int handle)
{
	SD_DEB_CALL ("sd_width", line, file, func, handle, -1, 0, NULL);
	SD_GLOBAL_LOCK ();
	sd_lst_t lst = get_ptr (handle);
	if (!lst) {
		SD_GLOBAL_UNLOCK ();
		return 0;
	}
	SD_GLOBAL_UNLOCK ();

	SD_LOCK (lst);
	int w = lst->width;
	SD_UNLOCK (lst);
	return w;
}

int _sd_setlen (int line, const char *file, const char *func, int handle,
		int len)
{
	SD_DEB_CALL ("sd_setlen", line, file, func, handle, -1, len, NULL);
	if (len < 0)
		SD_ERR ("length cannot be negative (%d)", len);
	SD_GLOBAL_LOCK ();
	sd_lst_t lst = get_ptr (handle);
	if (!lst) {
		SD_GLOBAL_UNLOCK ();
		SD_ERR ("invalid handle %d", handle);
	}

	if (len > lst->max) {
		void *new_data = realloc (lst->data, len * lst->width);
		if (!new_data) {
			SD_GLOBAL_UNLOCK ();
			SD_ERR ("out of memory");
		}
		lst->data = new_data;
		lst->max = len;
	}
	SD_GLOBAL_UNLOCK ();

	SD_LOCK (lst);
	lst->len = len;
	SD_UNLOCK (lst);
	return 0;
}

int _sd_bufsize (int line, const char *file, const char *func, int handle)
{
	SD_DEB_CALL ("sd_bufsize", line, file, func, handle, -1, 0, NULL);
	SD_GLOBAL_LOCK ();
	sd_lst_t lst = get_ptr (handle);
	if (!lst) {
		SD_GLOBAL_UNLOCK ();
		return 0;
	}
	SD_GLOBAL_UNLOCK ();

	SD_LOCK (lst);
	int max = lst->max;
	SD_UNLOCK (lst);
	return max;
}

void *_sd_buf (int line, const char *file, const char *func, int handle)
{
	SD_DEB_CALL ("sd_buf", line, file, func, handle, -1, 0, NULL);
	SD_GLOBAL_LOCK ();
	sd_lst_t lst = get_ptr (handle);
	if (!lst) {
		SD_GLOBAL_UNLOCK ();
		return NULL;
	}
	SD_GLOBAL_UNLOCK ();

	SD_LOCK (lst);
	void *buf = lst->data;
	SD_UNLOCK (lst);
	return buf;
}

void _sd_del (int line, const char *file, const char *func, int handle,
	      int index)
{
	SD_DEB_CALL ("sd_del", line, file, func, handle, index, 0, NULL);
	SD_GLOBAL_LOCK ();
	sd_lst_t lst = get_ptr (handle);
	if (!lst) {
		SD_GLOBAL_UNLOCK ();
		SD_ERR ("invalid handle %d", handle);
	}
	SD_GLOBAL_UNLOCK ();

	SD_LOCK (lst);
	if (index < 0 || index >= lst->len) {
		SD_UNLOCK (lst);
		SD_ERR ("index %d out of bounds [0, %d)", index, lst->len);
	}
	char *data = (char *)lst->data;
	int elem_size = lst->width;
	int move_count = lst->len - index - 1;
	if (move_count > 0) {
		memmove (data + index * elem_size,
			 data + (index + 1) * elem_size,
			 move_count * elem_size);
	}
	lst->len--;
	SD_UNLOCK (lst);
}

void *_sd_ins (int line, const char *file, const char *func, int handle,
	       int pos, int n)
{
	SD_DEB_CALL ("sd_ins", line, file, func, handle, pos, n, NULL);
	if (n <= 0)
		SD_ERR ("cannot insert %d elements", n);
	SD_GLOBAL_LOCK ();
	sd_lst_t lst = get_ptr (handle);
	if (!lst) {
		SD_GLOBAL_UNLOCK ();
		SD_ERR ("invalid handle %d", handle);
	}

	if (lst->len + n > lst->max) {
		int new_max = lst->len + n;
		new_max = new_max * GROWTH_FACTOR > new_max
				  ? new_max * GROWTH_FACTOR
				  : new_max;
		void *new_data = realloc (lst->data, new_max * lst->width);
		if (!new_data) {
			SD_GLOBAL_UNLOCK ();
			SD_ERR ("out of memory");
		}
		lst->data = new_data;
		lst->max = new_max;
	}
	SD_GLOBAL_UNLOCK ();

	SD_LOCK (lst);
	if (pos < 0)
		pos = 0;
	if (pos > lst->len)
		pos = lst->len;
	char *data = (char *)lst->data;
	int elem_size = lst->width;
	int move_count = lst->len - pos;
	if (move_count > 0) {
		memmove (data + (pos + n) * elem_size, data + pos * elem_size,
			 move_count * elem_size);
	}
	memset (data + pos * elem_size, 0, n * elem_size);
	lst->len += n;
	void *res = data + pos * elem_size;
	SD_UNLOCK (lst);
	return res;
}

void _sd_remove (int line, const char *file, const char *func, int handle,
		 int pos, int n)
{
	SD_DEB_CALL ("sd_remove", line, file, func, handle, pos, n, NULL);
	if (n <= 0)
		SD_ERR ("cannot remove %d elements", n);
	SD_GLOBAL_LOCK ();
	sd_lst_t lst = get_ptr (handle);
	if (!lst) {
		SD_GLOBAL_UNLOCK ();
		SD_ERR ("invalid handle %d", handle);
	}
	SD_GLOBAL_UNLOCK ();

	SD_LOCK (lst);
	if (pos < 0 || pos >= lst->len) {
		SD_UNLOCK (lst);
		SD_ERR ("index %d out of bounds [0, %d)", pos, lst->len);
	}
	if (pos + n > lst->len)
		n = lst->len - pos;
	char *data = (char *)lst->data;
	int elem_size = lst->width;
	int move_count = lst->len - pos - n;
	if (move_count > 0) {
		memmove (data + pos * elem_size, data + (pos + n) * elem_size,
			 move_count * elem_size);
	}
	lst->len -= n;
	SD_UNLOCK (lst);
}

void *_sd_pop (int line, const char *file, const char *func, int handle)
{
	SD_DEB_CALL ("sd_pop", line, file, func, handle, -1, 0, NULL);
	SD_GLOBAL_LOCK ();
	sd_lst_t lst = get_ptr (handle);
	if (!lst) {
		SD_GLOBAL_UNLOCK ();
		SD_ERR ("invalid handle %d", handle);
	}
	SD_GLOBAL_UNLOCK ();

	SD_LOCK (lst);
	if (lst->len <= 0) {
		SD_UNLOCK (lst);
		return NULL;
	}
	lst->len--;
	void *res = (char *)lst->data + lst->len * lst->width;
	SD_UNLOCK (lst);
	return res;
}

void _sd_clear (int line, const char *file, const char *func, int handle)
{
	SD_DEB_CALL ("sd_clear", line, file, func, handle, -1, 0, NULL);
	SD_GLOBAL_LOCK ();
	sd_lst_t lst = get_ptr (handle);
	if (!lst) {
		SD_GLOBAL_UNLOCK ();
		SD_ERR ("invalid handle %d", handle);
	}

	if (lst->free_hdl > 0 && lst->free_hdl < SD_MAX_FREE && lst->len > 0) {
		for (int i = 0; i < lst->len; i++) {
			void *elem = (char *)lst->data + i * lst->width;
			SD_FREE_FN[lst->free_hdl](elem);
		}
	}
	SD_GLOBAL_UNLOCK ();

	SD_LOCK (lst);
	lst->len = 0;
	SD_UNLOCK (lst);
}

void _sd_resize (int line, const char *file, const char *func, int handle,
		 int size)
{
	SD_DEB_CALL ("sd_resize", line, file, func, handle, -1, size, NULL);
	if (size <= 0)
		SD_ERR ("size must be positive (%d)", size);
	SD_GLOBAL_LOCK ();
	sd_lst_t lst = get_ptr (handle);
	if (!lst) {
		SD_GLOBAL_UNLOCK ();
		SD_ERR ("invalid handle %d", handle);
	}

	if (size > lst->max) {
		void *new_data = realloc (lst->data, size * lst->width);
		if (new_data) {
			lst->data = new_data;
			lst->max = size;
		} else {
			SD_GLOBAL_UNLOCK ();
			SD_ERR ("out of memory");
		}
	}
	SD_GLOBAL_UNLOCK ();
}

int sd_reg_free (int free_hdl, void (*fn) (void *))
{
	SD_DEB_CALL ("sd_reg_free", 0, NULL, NULL, 0, -1, 0, NULL);
	if (free_hdl < 0 || free_hdl >= SD_MAX_FREE)
		return -1;
	SD_FREE_FN[free_hdl] = fn;
	return 0;
}

int _sd_alloc_typed (int line, const char *file, const char *func, int max,
		     int width, int free_hdl, const char *type_name)
{
	SD_DEB_CALL ("sd_alloc_typed", line, file, func, 0, -1, max, NULL);
	int h = _sd_alloc (line, file, func, max, width, free_hdl);
	if (h <= 0)
		return h;
	SD_GLOBAL_LOCK ();
	sd_lst_t lst = get_ptr (h);
	if (lst)
		lst->type_name = type_name;
	SD_GLOBAL_UNLOCK ();
	return h;
}

const char *_sd_get_type_name (int line, const char *file, const char *func,
			       int handle)
{
	SD_DEB_CALL ("sd_get_type_name", line, file, func, handle, -1, 0, NULL);
	SD_GLOBAL_LOCK ();
	sd_lst_t lst = get_ptr (handle);
	const char *name = lst ? lst->type_name : NULL;
	SD_GLOBAL_UNLOCK ();
	return name;
}

int _sd_get_free_hdl (int line, const char *file, const char *func, int handle)
{
	SD_DEB_CALL ("sd_get_free_hdl", line, file, func, handle, -1, 0, NULL);
	SD_GLOBAL_LOCK ();
	sd_lst_t lst = get_ptr (handle);
	int free_hdl = lst ? lst->free_hdl : 0;
	SD_GLOBAL_UNLOCK ();
	return free_hdl;
}
