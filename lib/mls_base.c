/* disable macros to override m_alloc, ... */
#define MLS_DEBUG_DISABLE
#include "mls_base.h"
#include "mls_internal.h"

#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Debug globals */
int trace_level = 0;
MLS_THREAD_LOCAL int mls_errno = 0;
MLS_THREAD_LOCAL const char *mls_errfunc = "";
MLS_THREAD_LOCAL const char *mls_errfile = "";
MLS_THREAD_LOCAL int mls_errline = 0;
int error_occurred = 0;
static int UAF_PROTECTION = 0;
struct ls_st ML = {0}; // stack allocated vars
#ifdef MLS_THREAD_SAFE
pthread_mutex_t ml_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cs_map_lock = PTHREAD_MUTEX_INITIALIZER;
static MLS_THREAD_LOCAL int freeing_handle = 0;
#else
static int freeing_handle = 0;
#endif
int FH = 0;
void (*mls_on_free) (int) = 0;

/* prototypes */
static int get_free_hdl (void);

static void init_handle_lock (lst_t lp)
{
#ifdef MLS_THREAD_SAFE
	if (lp->lock)
		return;
	lp->lock = malloc (sizeof (*lp->lock));
	if (!lp->lock)
		ERR ("Out of Memory");
	if (pthread_rwlock_init (lp->lock, NULL) != 0)
		ERR ("Unable to initialize handle lock");
#else
	(void)lp;
#endif
}
static void destroy_handle_lock (lst_t lp)
{
#ifdef MLS_THREAD_SAFE
	if (!lp->lock)
		return;
	pthread_rwlock_destroy (lp->lock);
	free (lp->lock);
	lp->lock = NULL;
#else
	(void)lp;
#endif
}
static inline lst_t get_list_locked (int m)
{
	int idx = REAL_HDL (m);
	int uaf = REAL_UAF (m);
	if (idx < 0 || idx >= ML.l) {
		ERR ("Invalid Handle %d", idx);
	}
	lst_t l = lst (&ML, idx);

	if (l->uaf_protection != uaf) {
		ERR ("uaf protection pattern does not match, expected:%d, "
		     "got:%d",
		     l->uaf_protection, uaf);
	}

	init_handle_lock (l);
	return l;
}
/* One locking core for both paths.
   die=1: invalid handle, UAF mismatch, unallocated/freed list -> ERR()+exit.
   die=0: same conditions set mls_errno and return NULL (used only by
	  m_is_freed, the query that must answer "is this handle dead?"
	  without exiting). */
static lst_t lock_handle_core (int m, int write, int die)
{
	int idx = REAL_HDL (m);
	int uaf = REAL_UAF (m);

	MLS_MASTER_LOCK ();
	if (idx < 0 || idx >= ML.l) {
		MLS_MASTER_UNLOCK ();
		if (die)
			ERR ("Invalid Handle %d", idx);
		mls_errno = MLS_EINVAL;
		return NULL;
	}
	lst_t lp = lst (&ML, idx);

	if (lp->uaf_protection != uaf) {
		MLS_MASTER_UNLOCK ();
		if (die)
			ERR ("uaf protection pattern does not match, "
			     "expected:%d, got:%d",
			     lp->uaf_protection, uaf);
		mls_errno = MLS_EUAF;
		return NULL;
	}

	init_handle_lock (lp);
#ifdef MLS_THREAD_SAFE
	if (write)
		pthread_rwlock_wrlock (lp->lock);
	else
		pthread_rwlock_rdlock (lp->lock);
#else
	(void)write;
#endif
	/* ponytail: data+free_hdl checks moved here from get_list_locked
	   so reads happen under per-handle rwlock, matching the writes
	   in lst_resize/m_free — eliminates TSAN data races */
	if (lp->data == NULL ||
	    (lp->free_hdl == 255 &&
	     (!die || REAL_HDL (freeing_handle) != REAL_HDL (m)))) {
#ifdef MLS_THREAD_SAFE
		pthread_rwlock_unlock (lp->lock);
#endif
		MLS_MASTER_UNLOCK ();
		if (die)
			ERR (lp->data == NULL ? "List %d not allocated"
					      : "List %d is being freed",
			     REAL_HDL (m));
		mls_errno = MLS_EUAF;
		return NULL;
	}
	MLS_MASTER_UNLOCK ();
	return lp;
}
lst_t lock_handle (int m, int write) { return lock_handle_core (m, write, 1); }
void unlock_handle (lst_t lp)
{
#ifdef MLS_THREAD_SAFE
	pthread_rwlock_unlock (lp->lock);
#else
	(void)lp;
#endif
}
/* Non-dying lock used only by m_is_freed(): never exits, sets mls_errno. */
lst_t lock_handle_safe (int m, int write)
{
	return lock_handle_core (m, write, 0);
}
/* Like lst_resize but returns -1 instead of calling ERR. */
static int lst_resize_safe (lst_t lp, size_t new_size)
{
	/* resizing foreign memory is a programmer error, not a handleable one
	 */
	if (lp->free_hdl & MFREE_NOALLOC)
		ERR ("List is marked as NOALLOC, unable to resize");
	size_t newSize = new_size * lp->w;
	size_t oldSize = lp->max * lp->w;
	if (new_size > 0 && newSize / new_size != lp->w) {
		mls_errno = MLS_EOVERFLOW;
		return -1;
	}
	char *newData = realloc (lp->data, newSize);
	if (!newData) {
		mls_errno = MLS_ENOMEM;
		return -1;
	}
	lp->data = newData;
	if (newSize > oldSize)
		memset (lp->data + oldSize, 0, newSize - oldSize);
	lp->max = new_size;
	return 0;
}
/* Like lst_new but returns -1 on error instead of calling ERR. */
int lst_new_safe (lst_t lp, size_t n)
{
	size_t p = lp->l;
	size_t max = lp->max;
	if (p + n > max) {
		size_t newsiz = max + n;
		newsiz = increase_by_percent (newsiz, 50);
		if (lst_resize_safe (lp, newsiz) != 0)
			return -1;
	}
	lp->l += n;
	return (int)p;
}
/* Like lst_put but returns -1 on error instead of calling ERR. */
static int lst_put_safe (lst_t lp, const void *d)
{
	int p = lst_new_safe (lp, 1);
	if (p < 0)
		return -1;
	memcpy (lst (lp, p), d, lp->w);
	return p;
}
/* Like lst_write but returns -1 on error instead of calling ERR. */
int lst_write_safe (lst_t lp, size_t p, const void *data, size_t n)
{
	/* ponytail: NULL data is validated by the public wrappers via ERR. */
	if (p + n > lp->max && lst_resize_safe (lp, p + n) != 0)
		return -1;
	if (p + n > lp->l)
		lp->l = p + n;
	memcpy (lst (lp, p), data, n * lp->w);
	return 0;
}
/* Like lst_read but returns -1 on error instead of calling ERR. */
int lst_read_safe (lst_t l, size_t p, void **data, size_t n)
{
	if (p + n > l->l || data == NULL) {
		mls_errno = MLS_EBOUNDS;
		return -1;
	}
	if (*data == 0) {
		size_t alloc_size = l->w * n;
		if (n > 0 && alloc_size / n != l->w) {
			mls_errno = MLS_EOVERFLOW;
			return -1;
		}
		*data = malloc (alloc_size);
		if (!*data) {
			mls_errno = MLS_ENOMEM;
			return -1;
		}
	}
	memcpy (*data, lst (l, p), n * l->w);
	return 0;
}
/* Like lst_ins but reports OOM/overflow instead of dying.
   p > l is a parameter error and always dies. */
void *lst_ins_safe (lst_t lp, size_t p, size_t n)
{
	if (p > lp->l)
		ERR ("Wrong Arg p=%zu", p);
	size_t cnt = lp->l - p;
	if (lst_new_safe (lp, n) < 0)
		return NULL;
	if (cnt > 0)
		memmove (lst (lp, p + n), lst (lp, p), cnt * lp->w);
	memset (lst (lp, p), 0, n * lp->w);
	return lst (lp, p);
}
/**
 * Prints an error message with file and line information and terminates the
 * program. Sets the error_occurred flag for post-mortem analysis.
 *
 * @param line The line number where the error occurred.
 * @param file The source file where the error occurred.
 * @param function The function where the error occurred.
 * @param format The format string for the error message.
 */
void deb_err (int line, const char *file, const char *function,
	      const char *format, ...)
{
	__atomic_store_n (&error_occurred, 1, __ATOMIC_RELAXED);
	mls_errno = MLS_EINVAL;
	mls_errfunc = function;
	mls_errfile = file;
	mls_errline = line;
	va_list ap;
	char buf[1024];
	int err = errno;
	va_start (ap, format);
	vsnprintf (buf, sizeof (buf), format, ap);
	va_end (ap);
	fprintf (stderr, "\n[mls error] %s:%d %s(): %s\n", file, line, function,
		 buf);
	if (err)
		perror ("  system error");
	exit (1);
}
/**
 * Prints a warning message with file and line information.
 *
 * @param line The line number where the warning occurred.
 * @param file The source file where the warning occurred.
 * @param function The function where the warning occurred.
 * @param format The format string for the warning message.
 */
void deb_warn (int line, const char *file, const char *function,
	       const char *format, ...)
{
	va_list ap;
	char buf[1024];
	va_start (ap, format);
	vsnprintf (buf, sizeof (buf), format, ap);
	va_end (ap);
	fprintf (stderr, "[mls warn] %s:%d %s(): %s\n", file, line, function,
		 buf);
}
/**
 * Prints a trace message if the provided level is greater than or equal to
 * trace_level.
 *
 * @param l The trace level of this message.
 * @param line The line number where the trace occurred.
 * @param file The source file where the trace occurred.
 * @param function The function where the trace occurred.
 * @param format The format string for the trace message.
 */
void deb_trace (int l, int line, const char *file, const char *function,
		const char *format, ...)
{
	va_list ap;
	char buf[1024];
	va_start (ap, format);
	vsnprintf (buf, sizeof (buf), format, ap);
	va_end (ap);
	fprintf (stderr, "[mls trace %d] %s(): %s\n", l, function, buf);
}
const char *mls_errmsg (int code)
{
	switch (code) {
	case MLS_OK:
		return "Success";
	case MLS_EINVAL:
		return "Invalid handle";
	case MLS_EBOUNDS:
		return "Index out of bounds";
	case MLS_ENOMEM:
		return "Out of memory";
	case MLS_EUAF:
		return "Use-after-free detected";
	case MLS_EOVERFLOW:
		return "Integer overflow";
	default:
		return "Unknown error";
	}
}
/**
 * Report the pending mls_errno error with caller location and exit(1).
 * Used by the mls_must() macro; never returns.
 */
void _mls_die (int line, const char *file, const char *function)
{
	fprintf (stderr, "[mls error] %s:%d %s(): %s", file, line, function,
		 mls_errmsg (mls_errno));
	if (mls_errfunc && mls_errfunc[0])
		fprintf (stderr, " (in %s)", mls_errfunc);
	fprintf (stderr, "\n");
	exit (1);
}
/**
 * Returns a pointer to the element at the specified index in a list structure.
 *
 * @param l The list structure.
 * @param i The index of the element.
 * @return A pointer to the element data.
 */
void *lst (lst_t l, size_t i)
{
	if (!l->data)
		ERR ("Not init.");
	return &l->data[l->w * i];
}
/**
 * Resizes a list structure to a new maximum size.
 * Dies on error; lst_resize_safe() reports the error instead.
 *
 * @param lp Pointer to the list structure pointer.
 * @param new_size The new maximum number of elements.
 */
void lst_resize (lst_t lp, size_t new_size)
{
	if (lst_resize_safe (lp, new_size) != 0)
		_mls_die (__LINE__, __FILE__, __FUNCTION__);
}
/**
 * Creates a new list structure.
 *
 * @param l The list structure to initialize.
 * @param max The initial maximum number of elements.
 * @param w The width of each element in bytes.
 */
void lst_create (lst_t l, size_t max, size_t w)
{
	if (max == 0)
		max = 1;
	if (w == 0)
		w = 1;

	size_t alloc_size = max * w;
	if (max > 0 && alloc_size / max != w) {
		ERR ("Integer overflow in allocation");
	}

	l->max = max;
	l->l = 0;
	l->w = w;
	l->data = calloc (max, w);
	if (!l->data)
		ERR ("Out of Memory");
}
/**
 * Reserves space for n new elements in a list.
 * if no space left, increase max space by 50%
 *
 * @param lp Pointer to the list structure pointer.
 * @param n The number of elements to reserve.
 * @return The index of the first newly reserved element.
 */
int lst_new (lst_t lp, size_t n)
{
	int p = lst_new_safe (lp, n);
	if (p < 0)
		_mls_die (__LINE__, __FILE__, __FUNCTION__);
	return p;
}
/**
 * Appends an element to a list.
 *
 * @param lp Pointer to the list structure pointer.
 * @param d Pointer to the element data to append.
 * @return The index of the appended element.
 */
int lst_put (lst_t lp, const void *d)
{
	int p = lst_put_safe (lp, d);
	if (p < 0)
		_mls_die (__LINE__, __FILE__, __FUNCTION__);
	return p;
}
/**
 * Returns a pointer to the element at the specified index without using
 * allocated bounds checking this function allows to access all elementes in the
 * array. normaly the length (l) value is compared to the index, you need to
 * call m_setlen or use m_put which automatically increases the array length
 * value. lst_peek gives you access to all allocated memory postion in this
 * array
 *
 * @param l The list structure.
 * @param i The index of the element.
 * @return A pointer to the element data
 */
void *lst_peek (lst_t l, size_t i)
{
	if (i >= l->max)
		ERR ("index out of bound max=%zu index=%zu", l->max, i);
	return lst (l, i);
}
/* Like lst_del but reports a bounds error instead of dying. */
int lst_del_safe (lst_t l, size_t p)
{
	if (p >= l->l) {
		mls_errno = MLS_EBOUNDS;
		return -1;
	}
	size_t w = l->w;
	size_t n = l->l - p - 1;
	if (n > 0)
		memmove (lst (l, p), lst (l, p + 1), n * w);
	l->l--;
	return 0;
}
/**
 * Deletes an element at the specified index from a list.
 *
 * @param l The list structure.
 * @param p The index of the element to delete.
 */
void lst_del (lst_t l, size_t p)
{
	if (lst_del_safe (l, p) != 0)
		_mls_die (__LINE__, __FILE__, __FUNCTION__);
}
/**
 * Removes n elements starting from the specified index from a list.
 *
 * @param lp Pointer to the list structure pointer.
 * @param p The starting index.
 * @param n The number of elements to remove.
 */
void lst_remove (lst_t lp, size_t p, size_t n)
{
	if (p + n > lp->l)
		ERR ("Wrong Arg p=%zu n=%zu", p, n);
	size_t w = lp->w;
	size_t move_n = lp->l - (p + n);
	if (move_n > 0)
		memmove (lst (lp, p), lst (lp, p + n), move_n * w);
	lp->l -= n;
}
/**
 * Inserts n empty elements at the specified index in a list.
 *
 * @param lp Pointer to the list structure pointer.
 * @param p The insertion index.
 * @param n The number of elements to insert.
 * @return A pointer to the first newly inserted element.
 */
void *lst_ins (lst_t lp, size_t p, size_t n)
{
	void *r = lst_ins_safe (lp, p, n);
	if (!r)
		_mls_die (__LINE__, __FILE__, __FUNCTION__);
	return r;
}
/**
 * Iterates to the next element in a list.
 *
 * @param l The list structure.
 * @param p Pointer to the current index. Should be initialized to -1.
 * @param data Pointer to store the address of the next element.
 * @return 1 if an element was found, 0 otherwise.
 */
int lst_next (lst_t l, int *p, void *data)
{
	if (!l)
		return 0;
	(*p)++;
	if (*p < 0) {
		ERR ("Wrong Arg p=%d", *p);
		return 0;
	}
	if ((size_t)*p >= l->l)
		return 0;
	if (data)
		*(void **)data = lst (l, *p);
	return 1;
}
/**
 * Reads n elements from a list into a buffer.
 *
 * @param l The list structure.
 * @param p The starting index.
 * @param data Pointer to the destination buffer pointer. If *data is 0, a
 * buffer is allocated.
 * @param n The number of elements to read.
 * @return 0 on success.
 */
int lst_read (lst_t l, size_t p, void **data, size_t n)
{
	if (lst_read_safe (l, p, data, n) != 0)
		_mls_die (__LINE__, __FILE__, __FUNCTION__);
	return 0;
}
/**
 * Writes n elements from a buffer into a list at the specified index.
 *
 * @param lp Pointer to the list structure pointer.
 * @param p The starting index.
 * @param data Pointer to the source data buffer.
 * @param n The number of elements to write.
 * @return 0 on success.
 */
int lst_write (lst_t lp, size_t p, const void *data, size_t n)
{
	if (lst_write_safe (lp, p, data, n) != 0)
		_mls_die (__LINE__, __FILE__, __FUNCTION__);
	return 0;
}
/**
 * Internal function to retrieve a pointer to the list structure associated with
 * a handle. Performs validation checks for handle range, existence, and UAF
 * protection.
 * @param m The handle to look up.
 * @return A pointer to the list structure pointer in the master list.
 */

static inline lst_t get_list (int m)
{
	lst_t lp;

	MLS_MASTER_LOCK ();
	lp = get_list_locked (m);
	if (lp->data == NULL) {
		MLS_MASTER_UNLOCK ();
		ERR ("List %d not allocated", REAL_HDL (m));
	}
	if (lp->free_hdl == 255) {
		MLS_MASTER_UNLOCK ();
		ERR ("List %d is being freed", REAL_HDL (m));
	}
	MLS_MASTER_UNLOCK ();
	return lp;
}
/**
 * Public accessor for getting a list pointer from a handle.
 *
 * @param r The handle.
 * @return A pointer to the list structure pointer.
 */
lst_t exported_get_list (int r) { return get_list (r); }
lst_t mls_lock_handle (int m, int write) { return lock_handle (m, write); }
void mls_unlock_handle (lst_t lp) { unlock_handle (lp); }
int new_list (const char *buf, size_t len, size_t max, size_t w, int hdl)
{
	MLS_MASTER_LOCK ();
	int h = get_free_hdl ();
	lst_t lp = (lst_t)lst (&ML, h);
	init_handle_lock (lp);
	lp->w = w;
	lp->data = (char *)buf;
	lp->max = max;
	lp->l = len;
	lp->free_hdl = hdl;
	lp->uaf_protection = UAF_PROTECTION;
	TRACE (1, "Created: %d  %s", h, (hdl & MFREE_NOALLOC) ? "" : "+BUF");
	int ret = (h) | (((int)(lp->uaf_protection) << 24));
	MLS_MASTER_UNLOCK ();
	return ret;
}
int last_created_hdl = -1;
/* find free handle slot or create a new slot and return slot number */
static int get_free_hdl (void)
{
	lst_t lp = lst (&ML, 0);
	if (lp->l > 0) {
		last_created_hdl = *(int *)lst (lp, lp->l - 1);
		lp->l--;
	} else {
		last_created_hdl = lst_new (&ML, 1);
	}

	return last_created_hdl;
}
/**
 * Non-aborting version of m_alloc(): returns -1 and sets mls_errno on
 * OOM or size overflow. A bad free-handler ID is a parameter error and
 * still exits.
 *
 * @param max Initial maximum elements.
 * @param w Width of each element in bytes.
 * @param hfree The ID of the registered free handler to use.
 * @return A new 1-based MLS handle, or -1 on error.
 */
int m_alloc_safe (size_t max, size_t w, uint8_t hfree)
{
	if (max == 0)
		max = 1;
	if (w == 0)
		w = 1;
	int hdl = hfree & MFREE_MASK;
	if (hdl && (size_t)hdl >= m_len (FH))
		ERR ("no such handler: %d", hfree);

	char *data = "";
	if (!(hfree & MFREE_NOALLOC)) {
		size_t alloc_size = max * w;
		if (max > 0 && alloc_size / max != w) {
			mls_errno = MLS_EOVERFLOW;
			return -1;
		}
		data = calloc (max, w);
		if (!data) {
			mls_errno = MLS_ENOMEM;
			return -1;
		}
	}
	return new_list (data, 0, max, w, hfree);
}
int m_alloc (size_t max, size_t w, uint8_t hfree)
{
	int h = m_alloc_safe (max, w, hfree);
	if (h < 0)
		_mls_die (__LINE__, __FILE__, __FUNCTION__);
	return h;
}
/**
 * Creates a new MLS handle with the default free handler (MFREE).
 *
 * @param max Initial maximum elements.
 * @param w Width of each element.
 * @return A new handle.
 */
int m_create (size_t max, size_t w) { return m_alloc (max, w, MFREE); }

int m_create_safe (size_t max, size_t w)
{
	return m_alloc_safe (max, w, MFREE);
}
/**
 * Frees an MLS handle and its associated list data.
 * The handle is marked as freed and returned to the free list for reuse.
 * Handles with MFREE_NODESTRUCT will not be touched - useful if you want to
 * make sure that the memory stays allocated forever and the list never gets
 * destroyed
 *
 * @param m The handle to free.
 * @return 0 on success.
 */
int m_free (int m)
{
	int realh = REAL_HDL (m);
	TRACE (1, "RH: %d, Hdl: %d", realh, m);
	if (m == 0)
		return 0;
retry:
	lst_t lp = lock_handle (m, 1);
	int freehdl = lp->free_hdl;
	int hdl = freehdl & MFREE_MASK;

	if (freehdl == 255 || (freehdl & MFREE_NODESTRUCT)) {
		unlock_handle (lp);
		return 0;
	}
	if (hdl == 0)
		goto simple_free;

	/* special free handler */
	unlock_handle (lp);
	if (hdl >= m_len (FH)) {
		ERR ("no such handler: %d, List:%d", hdl, realh);
	}
	free_fn_t xfree = *(free_fn_t *)mls (FH, hdl);
	lp = lock_handle (m, 1);
	if (lp->free_hdl != freehdl) {
		unlock_handle (lp);
		goto retry;
	}
	lp->free_hdl = 255; /* Mark handle as being freed */
	unlock_handle (lp);
	if (xfree) {
		int prev_freeing_handle = freeing_handle;
		freeing_handle = m;
		xfree (m);
		lp = lock_handle (m, 1);
		freeing_handle = prev_freeing_handle;
	} else {
		lp = lock_handle (m, 1);
	}

simple_free:
	if (!(freehdl & MFREE_NOALLOC)) {
		if (lp->data && lp->max > 0) {
			memset (lp->data, 0, lp->max * lp->w);
		}
		free (lp->data);
	}
	lp->data = 0;
	lp->free_hdl = 255;
	unlock_handle (lp);
	MLS_MASTER_LOCK ();
	lst_put ((lst_t)ML.data, &realh);
	UAF_PROTECTION = (UAF_PROTECTION + 1) & 0x7f;
	MLS_MASTER_UNLOCK ();
	if (mls_on_free)
		mls_on_free (realh);
	TRACE (1, "freed: %d, Hdl: %d", realh, m);
	return 0;
}
/**
 * Returns the number of elements in the list associated with a handle.
 *
 * @param m The handle.
 * @return The number of elements.
 */
size_t m_len (int m)
{
	if (m <= 0)
		return 0;
	lst_t lp = lock_handle (m, 0);
	size_t len = lp->l;
	unlock_handle (lp);
	return len;
}
/**
 * Returns a pointer to the raw data buffer of the list associated with a
 * handle.
 *
 * @param m The handle.
 * @return A pointer to the data buffer, or NULL if handle is invalid.
 */
void *m_buf (int m)
{
	if (m <= 0)
		return NULL;
	return m_peek (m, 0);
}
/**
 * Returns a pointer to the element at the specified index with bounds checking.
 *
 * @param m The handle.
 * @param i The index.
 * @return A pointer to the element.
 */
void *mls (int m, size_t i)
{
	if (m <= 0)
		return NULL;
	void *ret = mls_safe (m, i);
	if (!ret)
		_mls_die (__LINE__, __FILE__, __FUNCTION__);
	return ret;
}
/**
 * Non-aborting version of mls(). Returns NULL on any error and sets mls_errno.
 *
 * @param m The handle.
 * @param i The index.
 * @return A pointer to the element, or NULL on error.
 */
void *mls_safe (int m, size_t i)
{
	if (m <= 0) {
		mls_errno = MLS_EINVAL;
		return NULL;
	}
	lst_t lp = lock_handle (m, 0);
	if (i >= lp->l) {
		mls_errno = MLS_EBOUNDS;
		unlock_handle (lp);
		return NULL;
	}
	void *ret = lst (lp, i);
	unlock_handle (lp);
	return ret;
}
/**
 * Iterates through the elements of a handle's list.
 *
 * @param m The handle.
 * @param p Pointer to the current index.
 * @param d Pointer to store the address of the next element.
 * @return 1 if an element was found, 0 otherwise.
 */
int m_next (int m, int *p, void *d)
{
	if (m <= 0)
		return 0;
	lst_t lp = lock_handle (m, 0);
	int ret = lst_next (lp, p, d);
	unlock_handle (lp);
	return ret;
}
/**
 * Appends an element to a handle's list.
 *
 * @param m The handle.
 * @param data Pointer to the element data to append.
 * @return The index of the appended element.
 */
int m_put (int m, const void *data)
{
	if (m <= 0)
		return -1;
	int p = m_put_safe (m, data);
	if (p < 0)
		_mls_die (__LINE__, __FILE__, __FUNCTION__);
	return p;
}
/**
 * Non-aborting version of m_put(). Returns -1 on error and sets mls_errno.
 *
 * @param m The handle.
 * @param data Pointer to the element data to append.
 * @return The index of the appended element, or -1 on error.
 */
int m_put_safe (int m, const void *data)
{
	if (m <= 0) {
		mls_errno = MLS_EINVAL;
		return -1;
	}
	if (!data)
		ERR ("Wrong arguments");
	lst_t lp = lock_handle (m, 1);
	int p = lst_put_safe (lp, data);
	unlock_handle (lp);
	return p;
}
/**
 * Sets the logical length of a handle's list.
 *
 * @param m The handle.
 * @param len The new length.
 * @return 0 on success.
 */
int m_setlen (int m, size_t len)
{
	if (m <= 0)
		return -1;
	if (m_setlen_safe (m, len) != 0)
		_mls_die (__LINE__, __FILE__, __FUNCTION__);
	return 0;
}
/**
 * Non-aborting version of m_setlen(). Returns -1 on error and sets mls_errno.
 *
 * @param m The handle.
 * @param len The new length.
 * @return 0 on success, -1 on error.
 */
int m_setlen_safe (int m, size_t len)
{
	if (m <= 0) {
		mls_errno = MLS_EINVAL;
		return -1;
	}
	lst_t lp = lock_handle (m, 1);
	if (len > lp->max && lst_resize_safe (lp, len) != 0) {
		unlock_handle (lp);
		return -1;
	}
	lp->l = len;
	unlock_handle (lp);
	return 0;
}
/**
 * Writes data to a handle's list starting at a specific index.
 * Resizes the list if necessary.
 *
 * @param m The handle.
 * @param p The starting index.
 * @param data Pointer to the source data.
 * @param n The number of elements to write.
 * @return 0 on success.
 */
int m_write (int m, size_t p, const void *data, size_t n)
{
	if (m <= 0)
		return -1;
	int ret = m_write_safe (m, p, data, n);
	if (ret != 0)
		_mls_die (__LINE__, __FILE__, __FUNCTION__);
	return ret;
}
/**
 * Non-aborting version of m_write(). Returns -1 on error and sets mls_errno.
 *
 * @param m The handle.
 * @param p The starting index.
 * @param data Pointer to the source data.
 * @param n The number of elements to write.
 * @return 0 on success, -1 on error.
 */
int m_write_safe (int m, size_t p, const void *data, size_t n)
{
	if (m <= 0) {
		mls_errno = MLS_EINVAL;
		return -1;
	}
	if (!data)
		ERR ("Wrong arguments");
	if (p + n < p) {
		mls_errno = MLS_EOVERFLOW;
		return -1;
	}
	lst_t lp = lock_handle (m, 1);
	int ret = lst_write_safe (lp, p, data, n);
	unlock_handle (lp);
	return ret;
}
/**
 * Returns a pointer to the element at the specified index without bounds
 * checking.
 *
 * @param m The handle.
 * @param i The index.
 * @return A pointer to the element, or NULL if index is out of bounds.
 */
void *m_peek (int m, size_t i)
{
	if (m <= 0)
		return NULL;
	lst_t lp = lock_handle (m, 0);
	void *ret = lst_peek (lp, i);
	unlock_handle (lp);
	return ret;
}
/**
 * Returns the width of each element in a handle's list in bytes.
 *
 * @param m The handle.
 * @return The element width.
 */
size_t m_width (int m)
{
	if (m <= 0)
		return 0;
	lst_t lp = lock_handle (m, 0);
	size_t width = lp->w;
	unlock_handle (lp);
	return width;
}
/**
 * Resizes the allocated capacity of a handle's list.
 *
 * @param m The handle.
 * @param new_size The new maximum number of elements.
 */
int m_resize_safe (int m, size_t new_size)
{
	if (m <= 0) {
		mls_errno = MLS_EINVAL;
		return -1;
	}
	lst_t lp = lock_handle (m, 1);
	int ret = lst_resize_safe (lp, new_size);
	unlock_handle (lp);
	return ret;
}
void m_resize (int m, size_t new_size)
{
	if (m <= 0)
		return;
	if (m_resize_safe (m, new_size) != 0)
		_mls_die (__LINE__, __FILE__, __FUNCTION__);
}

/**
 * Initializes the core MLS system: master handle list + free list only.
 * Ext state (const-string map, free-handler table) is set up separately by
 * mls_ext (see m_init()).
 *
 * @return 0 on success.
 */
int m_base_init ()
{
	MLS_MASTER_LOCK ();
	if (ML.data) {
		MLS_MASTER_UNLOCK ();
		return 0;
	}

	srand ((unsigned int)time (NULL));
	UAF_PROTECTION = rand () & 0x7f;

	lst_create (&ML, 100, sizeof (struct ls_st));
	lst_t lp = lst (&ML, lst_new (&ML, 1));
	lst_create (lp, 100, sizeof (int));
	init_handle_lock (lp);
	MLS_MASTER_UNLOCK ();

	FH = m_alloc (10, sizeof (void *), 0);
	return 0;
}

/**
 * Destroys the core MLS system: frees all remaining handle data and the
 * master list. Does not touch ext state.
 */
void m_base_destruct ()
{
	int idx;
	lst_t d;
	MLS_MASTER_LOCK ();
	if (!ML.data)
		ERR ("Not Init.");
	idx = -1;
	while (lst_next (&ML, &idx, &d)) {
		if (d && d->data && !(d->free_hdl & MFREE_NOALLOC)) {
			TRACE (1, "%d Free", idx);
			free (d->data);
			d->data = 0;
		} else
			TRACE (1, "%d", idx);
		if (d)
			destroy_handle_lock (d);
	}

	if (ML.data) {
		free (ML.data);
		ML.data = 0;
	}
	UAF_PROTECTION = 0;
	MLS_MASTER_UNLOCK ();
}
