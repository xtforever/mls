/* disable macros to override m_alloc, ... */
#define MLS_DEBUG_DISABLE
#include "mls_ext.h"
#include "mls_internal.h"

#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int CS_MAP = 0;
static int CS_ZERO = 0;

/* prototypes */
int m_binsert (int buf, const void *data,
	       int (*cmpf) (const void *data, const void *buf_elem),
	       int with_duplicates);
int m_binsert2 (int buf, const void *data,
		int (*cmpf) (const void *data, const void *buf_elem),
		int with_duplicates, int with_copy);
#ifdef MLS_DEBUG
static void _debug_create_list (int m_uaf, const char *dfunc, int ln,
				const char *fn, const char *fun);
#endif

struct lst_owner_st {
	int allocated;
	const char *fn, *fun;
	int ln;
};
typedef struct lst_owner_st lst_owner;

struct debug_info_st {
	char msg[500];
	const char *me, *fn, *fun;
	int ln, args, handle, index;
	const void *data;
};
static int DEB = 0; // debug list
static struct debug_info_st debi;
/**
 * Records caller information for the current MLS operation.
 * Used by debug wrappers to provide context for error messages.
 *
 * @param me The name of the function being wrapped.
 * @param ln The line number of the call.
 * @param fn The filename of the call.
 * @param fun The function name of the caller.
 * @param args Bitmask indicating which arguments are valid (1:handle, 2:index,
 * 4:data).
 * @param handle The MLS handle involved in the operation.
 * @param index The index involved in the operation.
 * @param data Pointer to data involved in the operation.
 */
static void _mlsdb_caller (const char *me, int ln, const char *fn,
			   const char *fun, int args, int handle, int index,
			   const void *data)
{
	debi.me = me;
	debi.ln = ln;
	debi.fn = fn;
	debi.fun = fun;
	debi.args = args;
	debi.handle = handle;
	debi.index = index;
	debi.data = data;
}
/**
 * Prints a formatted error message to stderr, followed by a newline.
 *
 * @param format The format string.
 */
static void perr (const char *format, ...)
{
	va_list argptr;
	va_start (argptr, format);
	vfprintf (stderr, format, argptr);
	fputc ('\n', stderr);
	va_end (argptr);
}
/**
 * Validates the handle stored in the current debug info.
 * Prints detailed information about the handle's state and allocation source.
 *
 * @return 0 if the handle is valid, -1 otherwise.
 */
static int _mlsdb_check_handle ()
{
	lst_t lp;
	lst_owner *o;
	int orig = debi.handle;
	int h = REAL_HDL (orig);
	int uaf = REAL_UAF (orig);

	perr ("  Handle:    %d (UAF protection: %d)", h, uaf);

	if (h < 0 || h >= m_len (DEB)) {
		perr ("  Status:    Handle out of range (max=%d)", m_len (DEB));
		return -1;
	}

	lp = (lst_t)lst (&ML, h);
	if (lp->data == NULL) {
		perr ("  Status:    List base address for handle %d is not "
		      "allocated",
		      h);
	} else {
		if ((*lp).uaf_protection != uaf) {
			perr ("  Status:    uaf protection pattern does not "
			      "match, expected:%d, got:%d",
			      (*lp).uaf_protection, uaf);
			return -1;
		}
	}

	o = (lst_owner *)mls (DEB, h);
	if (!o || o->allocated != 42) {
		perr ("  Status:    Array was not allocated");
		return -1;
	}

	if (o->ln < 0) {
		perr ("  Status:    Previously removed by %s() at %s:%d",
		      o->fun, o->fn, -o->ln);
		return -1;
	}

	perr ("  Created by: %s() at %s:%d", o->fun, o->fn, o->ln);

	if (lp) {
		perr ("  Metadata:  struct=%p, data=%p, width=%d\n"
		      "  Buffer:    used=%d, max=%d",
		      lp, (*lp).data, (*lp).w, (*lp).l, (*lp).max);
	}

	return 0;
}
/**
 * Validates the index stored in the current debug info against its handle.
 * Prints an error message if the index is out of bounds.
 *
 * @return 0 if the index is valid, -1 otherwise.
 */
static int _mlsdb_check_index ()
{
	int i = debi.index, h = debi.handle;
	if (i < -1) {
		perr ("  Index:     %d is invalid (must be >= -1)", i);
		return -1;
	}

	if (i >= m_len (h)) {
		perr ("  Index:     %d is out of bounds (len=%d)", i,
		      m_len (h));
		return -1;
	}

	return 0;
}
/**
 * Performs a post-mortem analysis of the last recorded MLS operation.
 * Registered as an atexit handler. Only runs if an error was detected.
 */
void exit_error ()
{
	if (!__atomic_load_n (&error_occurred, __ATOMIC_RELAXED) || !debi.me)
		return;

	perr ("\n[mls post-mortem analysis]");
	perr ("  Operation: %s()", debi.me);
	perr ("  Context:   Called from %s() at %s:%d", debi.fun, debi.fn,
	      debi.ln);

	if (!ML.data) {
		perr ("  Status:    m_init not called");
		return;
	}

	if (debi.args & 1)
		if (_mlsdb_check_handle ())
			return;

	if (debi.args & 2)
		if (_mlsdb_check_index ())
			return;

	if (debi.args & 4)
		if (debi.data == NULL) {
			perr ("No Ptr to Data given.");
			return;
		}
}
/**
 * same as m_free_strings to be used as a custom free handler.
 *
 * @param h The handle being freed.
 */
static void free_strings_wrap (int h)
{
	int p;
	char **d;
	m_foreach (h, p, d)
	{
		if (*d) {
			free (*d);
			*d = NULL;
		}
	}
}
/**
 * Custom free handler that recursively frees MLS handles stored in a list.
 *
 * @param h The handle being freed.
 */
static void free_list_wrap (int h)
{
	TRACE (1, "HDL:%d", REAL_HDL (h));
	int p, *d;
	m_foreach (h, p, d)
	{
		/* what happens when a list is inserted twice ?
		   can we check if we freed the list? then it is not an error.
		   right now, we just ignore double-free error
		 */
		if (m_is_freed (*d)) {
			TRACE (1, "not freeing list %d allready freed", *d);
		} else {
#ifdef MLS_DEBUG
			_m_free (__LINE__, __FILE__, __FUNCTION__, (*d));
#else
			m_free (*d);
#endif
		}
	}
}
void set_free_protection (int h, int p)
{
	lst_t lp = lock_handle (h, 1);
	lp->free_hdl |= p;
	unlock_handle (lp);
}
void unset_free_protection (int h, int p)
{
	lst_t lp = lock_handle (h, 1);
	lp->free_hdl &= ~(p);
	unlock_handle (lp);
}
static int mscmpc (const void *a, const void *b)
{
	const char *s0 = a;
	const int *d = b;
	const char *s1 = m_str (*d);
	return strncmp (s0, s1, m_len (*d));
}
int m_binsert2 (int buf, const void *data,
		int (*cmpf) (const void *data, const void *buf_elem),
		int with_duplicates, int with_copy);
/**
 * Looks up or creates a constant string from a C-style string.
 *
 * @param s The C-style string.
 * @param copy_string If non-zero, duplicates the string; if zero, uses
 * zero-copy interning.
 * @return The handle of the constant string.
 */
int conststr_lookup_c (const char *s, int copy_string)
{
	if (!s || !*s)
		return CS_ZERO;

	CS_MAP_LOCK ();
	int p = m_binsert2 (CS_MAP, s, mscmpc, 0, 0);
	if (p < 0) {
		CS_MAP_UNLOCK ();
		return INT (CS_MAP, (-p) - 1);
	}

	int hdl;
	int len = strlen (s) + 1;
	if (copy_string) {
		s = strdup (s);
		hdl = MFREE_NODESTRUCT;
	} else
		hdl = MFREE_NODESTRUCT | MFREE_NOALLOC;

	int ret = INT (CS_MAP, p) = new_list (s, len, len, 1, hdl);
	CS_MAP_UNLOCK ();
	return ret;
}
/**
 * Looks up or creates a constant string from an existing string buffer.
 *
 * @param s Handle of the source string buffer.
 * @return The handle of the constant string.
 */
int conststr_lookup (int s) { return conststr_lookup_c (m_str (s), 1); }
/**
 * Formatted creation of a constant string.
 *
 * @param format Format string.
 * @param ... Arguments.
 * @return The handle of the constant string.
 */
int cs_printf (const char *format, ...)
{
	int p;
	char *s;
	va_list ap;
	va_start (ap, format);
	int len = vasprintf (&s, format, ap);
	va_end (ap);

	p = m_binsert (CS_MAP, s, mscmpc, 0);
	if (p < 0) {
		free (s);
		return INT (CS_MAP, (-p) - 1);
	}
	return new_list (s, len + 1, len + 1, 1,
			 MFREE_NODESTRUCT | MFREE_NOALLOC);
}
/* make a constant string handle from a constant c string */
int s_ccstr (const char *s) { return conststr_lookup_c (s, 0); }
/* make a constant string handle from a c string */
int s_cstrdup (const char *s) { return conststr_lookup_c (s, 1); }
/* wrap a string  list into a mls string list */
int m_wrapstrings (char **list, int nelem)
{
	return new_list ((char *)list, nelem, nelem, sizeof (char *),
			 MFREE_NOALLOC);
}
/* wrap a int  list into a mls string list */
int m_wrapints (int *list, int nelem)
{
	return new_list ((char *)list, nelem, nelem, sizeof (int),
			 MFREE_NOALLOC);
}
int m_wrapcstr (char *s)
{
	int len = strlen (s) + 1;
	return new_list (s, len, len, 1, MFREE_NOALLOC);
}
/**
 * Initializes the MLS library system.
 * Allocates the master handle list and registers default free handlers.
 *
 * @return 0 on success, 1 if already initialized.
 */
int m_init ()
{
	m_base_init ();
	if (CS_MAP)
		return 0;

	CS_MAP = m_alloc (100, sizeof (int), 0); /* create list 1 */
	/* system up and running, now some specials */
	CS_ZERO = new_list ("", 1, 1, 1, MFREE_NOALLOC | MFREE_NODESTRUCT);
	m_puti (CS_MAP, CS_ZERO);
	set_free_protection (CS_MAP, MFREE_NODESTRUCT);

	/* FH is created empty by m_base_init(); register default handlers */
	free_fn_t f = NULL;
	m_put (FH, &f);
	f = free_strings_wrap;
	m_put (FH, &f);
	f = free_list_wrap;
	m_put (FH, &f);
	return 0;
}
/**
 * Frees the constant string system.
 * this function does nothing, freeing a constant does not make sense while
 * program is running. instead m_destruct() will free allocated memory for us
 */
void conststr_free (void) { return; }
/**
 * Destroys the MLS library system.
 * Explicitly frees all remaining allocated handles and their contents.
 * Policy: Do not call user registered free_handler functions
 */
void m_destruct ()
{
	MLS_MASTER_LOCK ();
	if (!ML.data)
		ERR ("Not Init.");
	((lst_t)lst (&ML, REAL_HDL (CS_MAP)))->free_hdl = 0;
	MLS_MASTER_UNLOCK ();
	m_base_destruct ();
}
int m_set_data (int m, size_t len, size_t w, const void *data)
{
	if (m <= 0) {
		return new_list (data, len, len, w, MFREE_NOALLOC);
	}
	lst_t lp = lock_handle (m, 1);
	if (!(lp->free_hdl & MFREE_NOALLOC)) {
		ERR ("List %d is not marked MFREE_NOALLOC", m);
	}
	lp->l = len;
	lp->max = len;
	lp->w = w;
	lp->data = (char *)data;
	unlock_handle (lp);
	return m;
}
/**
 * @brief Registers a  a cleanup callback for managed arrays
 * The @p free_fn is triggered automatically when m_free() is called.
 * This allows for custom iteration and deallocation of array elements
 * before the array container itself is removed.
 *
 * @param free_fn The function to be called when freeing handles with this
 * handler ID.
 * @return The handler ID on success, -1 if handler is invalid or too many
 * handlers registered.
 */
int m_reg_freefn (free_fn_t free_fn)
{
	if (m_len (FH) >= MFREE_MASK)
		ERR ("Too many free-fn registered");
	if (!free_fn)
		ERR ("custom free funtion is null");
	return m_put (FH, &free_fn);
}
/**
 * Checks if an MLS handle is valid and hasn't been freed.
 * the purpose of this function is make sure your program
 * does not exit() if you have an invalid handle
 *
 * @param h The handle to check.
 * @return 1 if the handle is valid, 0 if it is not valid.
 */
int m_is_valid (int h) { return !m_is_freed (h); }
/**
 * Checks if an MLS handle is valid and hasn't been freed.
 *
 * @param h The handle to check.
 * @return 1 if the handle is freed or invalid, 0 if it is active.
 */
int m_is_freed (int h)
{
	if (h <= 0)
		return 1;
	/* ponytail: use lock_handle_safe so data+free_hdl are read
	   under the per-handle rwlock, not just the master lock */
	lst_t lp = lock_handle_safe (h, 0);
	if (!lp)
		return 1;
	unlock_handle (lp);
	return 0;
}
/**
 * Returns the free handler ID associated with an MLS handle.
 *
 * @param h The handle.
 * @return The free handler ID.
 */
int m_free_hdl (int h)
{
	if (h <= 0)
		return 0;
	lst_t lp = lock_handle (h, 0);
	int free_hdl = lp->free_hdl;
	unlock_handle (lp);
	return free_hdl;
}
/**
 * Creates a duplicate of an existing m-array.
 * if the original list was write-protected with MFREE_NOALLOC
 * the copy will be read-write. This is not a deep copy operation.
 *
 * @param m The handle of the m-array to duplicate.
 * @return A new handle to the duplicated m-array.
 */
int m_dub (int m)
{
	if (m <= 0)
		return 0;

	lst_t src = lock_handle (m, 0);
	size_t len = src->l;
	size_t width = src->w;
	int free_hdl = src->free_hdl;
	size_t max = len > 0 ? len : 1;

	size_t alloc_size = max * width;
	if (max > 0 && alloc_size / max != width)
		ERR ("Overflow");

	char *data = calloc (max, width);
	if (!data)
		ERR ("Out of Memory");
	if (len > 0)
		memcpy (data, src->data, len * width);
	unlock_handle (src);

	free_hdl &= ~MFREE_NOALLOC;
	int ret = new_list (data, len, max, width, free_hdl);
#ifdef MLS_DEBUG
	if (DEB)
		_debug_create_list (ret, __FUNCTION__, __LINE__, __FILE__,
				    __FUNCTION__);
#endif
	return ret;
}
/**
 * Reserves space for n new elements in a handle's list.
 *
 * @param m The handle.
 * @param n The number of elements to reserve.
 * @return The index of the first newly reserved element.
 */
int m_new_safe (int m, size_t n)
{
	if (m <= 0) {
		mls_errno = MLS_EINVAL;
		return -1;
	}
	lst_t lp = lock_handle (m, 1);
	int p = lst_new_safe (lp, n);
	unlock_handle (lp);
	return p;
}
int m_new (int m, size_t n)
{
	if (m <= 0)
		return -1;
	int p = m_new_safe (m, n);
	if (p < 0)
		_mls_die (__LINE__, __FILE__, __FUNCTION__);
	return p;
}
/**
 * Appends one new element to a handle's list and returns a pointer to it.
 *
 * @param m The handle.
 * @return A pointer to the new element.
 */
void *m_add_safe (int m)
{
	if (m <= 0) {
		mls_errno = MLS_EINVAL;
		return NULL;
	}
	lst_t lp = lock_handle (m, 1);
	int p = lst_new_safe (lp, 1);
	void *ret = p < 0 ? NULL : lst (lp, p);
	unlock_handle (lp);
	return ret;
}
void *m_add (int m)
{
	if (m <= 0)
		return NULL;
	void *ret = m_add_safe (m);
	if (!ret)
		_mls_die (__LINE__, __FILE__, __FUNCTION__);
	return ret;
}
/**
 * Returns the currently allocated capacity (buffer size) of a handle's list.
 *
 * @param m The handle.
 * @return The maximum number of elements before a realloc is needed.
 */
size_t m_bufsize (int m)
{
	if (m <= 0)
		return 0;
	lst_t lp = lock_handle (m, 0);
	size_t max = lp->max;
	unlock_handle (lp);
	return max;
}
/**
 * Reads data from a handle's list into a buffer.
 *
 * @param h The handle.
 * @param p The starting index.
 * @param data Pointer to the destination buffer pointer.
 * @param n The number of elements to read.
 * @return 0 on success.
 */
int m_read (int h, size_t p, void **data, size_t n)
{
	if (h <= 0)
		return -1;
	int ret = m_read_safe (h, p, data, n);
	if (ret != 0)
		_mls_die (__LINE__, __FILE__, __FUNCTION__);
	return ret;
}
/**
 * Non-aborting version of m_read(). Returns -1 on error and sets mls_errno.
 *
 * @param h The handle.
 * @param p The starting index.
 * @param data Pointer to the destination buffer pointer.
 * @param n The number of elements to read.
 * @return 0 on success, -1 on error.
 */
int m_read_safe (int h, size_t p, void **data, size_t n)
{
	if (h <= 0) {
		mls_errno = MLS_EINVAL;
		return -1;
	}
	if (!data)
		ERR ("Wrong arguments");
	if (p + n < p) {
		mls_errno = MLS_EOVERFLOW;
		return -1;
	}
	lst_t lp = lock_handle (h, 0);
	int ret = lst_read_safe (lp, p, data, n);
	unlock_handle (lp);
	return ret;
}
/**
 * Sets the logical length of a handle's list to zero.
 *
 * @param m The handle.
 */
void m_clear (int m)
{
	if (m <= 0)
		return;
	lst_t lp = lock_handle (m, 1);
	(*lp).l = 0;
	unlock_handle (lp);
}
/**
 * Deletes an element at the specified index from a handle's list.
 * Remaining elements are shifted to close the gap.
 *
 * @param m The handle.
 * @param p The index of the element to delete.
 */
void m_del (int m, size_t p)
{
	if (m <= 0)
		return;
	if (m_del_safe (m, p) != 0)
		_mls_die (__LINE__, __FILE__, __FUNCTION__);
}
/**
 * Non-aborting version of m_del(). Returns -1 on error and sets mls_errno.
 *
 * @param m The handle.
 * @param p The index of the element to delete.
 * @return 0 on success, -1 on error.
 */
int m_del_safe (int m, size_t p)
{
	if (m <= 0) {
		mls_errno = MLS_EINVAL;
		return -1;
	}
	lst_t lp = lock_handle (m, 1);
	int ret = lst_del_safe (lp, p);
	unlock_handle (lp);
	return ret;
}
/**
 * Removes and returns a pointer to the last element of a handle's list.
 *
 * @param m The handle.
 * @return A pointer to the element, or NULL if the list is empty.
 */
void *m_pop (int m)
{
	if (m <= 0)
		return NULL;
	lst_t lp = lock_handle (m, 1);
	if (lp->l == 0) {
		unlock_handle (lp);
		return NULL;
	}
	lp->l--;
	void *ret = lst (lp, lp->l);
	unlock_handle (lp);
	return ret;
}
/**
 * Inserts n empty (zero-initialized) elements at a specific index in a handle's
 * list.
 *
 * @param m The handle.
 * @param p The insertion index.
 * @param n The number of elements to insert.
 * @return 1 on success, 0 on failure.
 */
int m_ins_safe (int m, size_t p, size_t n)
{
	if (m <= 0) {
		mls_errno = MLS_EINVAL;
		return 0;
	}
	lst_t lp = lock_handle (m, 1);
	int ret = lst_ins_safe (lp, p, n) != NULL;
	unlock_handle (lp);
	return ret;
}
int m_ins (int m, size_t p, size_t n)
{
	if (m <= 0)
		return 0;
	if (!m_ins_safe (m, p, n))
		_mls_die (__LINE__, __FILE__, __FUNCTION__);
	return 1;
}
/**TODO
 * Extracts a sub-range of elements from one list and appends or writes them
 * into another. Supports negative indices (relative to end of list).
 *
 * @param dest The destination handle. If <= 0, a new handle is created.
 * @param offs The offset in the destination list to start writing.
 * @param m The source handle.
 * @param a The starting index in the source list.
 * @param b The ending index in the source list (inclusive).
 * @return The destination handle.
 */
int m_slice (int dest, int offs, int m, int a, int b)
{
	size_t cnt = 0;
	size_t len = 0;
	size_t width = 1;
	if (m) {

		len = m_len (m);
		if (b < 0)
			b += (int)len;
		if (a < 0)
			a += (int)len;
		if (b >= (int)len)
			b = (int)len - 1;
		if (a >= (int)len)
			a = (int)len - 1;
		if (a < 0)
			a = 0;
		cnt = (b >= a) ? (size_t)(b - a + 1) : 0;
		width = m_width (m);
	}
	if (dest <= 0) {
#ifdef MLS_DEBUG
		dest = _m_create (__LINE__, __FILE__, __FUNCTION__, cnt + offs,
				  width);
#else
		dest = m_create (cnt + offs, width);
#endif
	}
	m_setlen (dest, offs);
	if (m && cnt > 0) {
		size_t nbytes = cnt * width;
		if (cnt > 0 && nbytes / cnt != width)
			ERR ("Integer overflow in slice");
		/* ponytail: copy src to temp and release src lock before
		   locking dest — avoids lock-order-inversion (rwlock → master)
		 */
		lst_t src_lp = lock_handle (m, 0);
		void *tmp = malloc (nbytes);
		if (!tmp)
			ERR ("Out of Memory");
		memcpy (tmp, lst (src_lp, (size_t)a), nbytes);
		unlock_handle (src_lp);

		lst_t dst_lp = lock_handle (dest, 1);
		lst_write (dst_lp, offs, tmp, cnt);
		unlock_handle (dst_lp);
		free (tmp);
	}
	return dest;
}
/**
 * Removes n elements starting from the specified index from a handle's list.
 * Remaining elements are shifted to close the gap.
 *
 * @param m The handle.
 * @param p The starting index.
 * @param n The number of elements to remove.
 */
void m_remove (int m, size_t p, size_t n)
{
	if (m <= 0)
		return;
	lst_t lp = lock_handle (m, 1);
	lst_remove (lp, p, n);
	unlock_handle (lp);
}
/**
 * Zeroes out the entire data buffer of a handle's list.
 *
 * @param m The handle.
 */
void m_bzero (int m)
{
	lst_t lp = lock_handle (m, 1);
	memset ((*lp).data, 0, (*lp).l * (*lp).w);
	unlock_handle (lp);
}
/**
 * Scans a file until a delimiter character is encountered or EOF is reached.
 * Appends the scanned characters to a handle's list.
 *
 * @param m The handle.
 * @param delim The delimiter character.
 * @param fp The file pointer to read from.
 * @return The delimiter character read, or EOF.
 */
int m_fscan2 (int m, char delim, FILE *fp)
{
	int c;
	m_clear (m);
	while ((c = fgetc (fp)) != EOF) {
		if (c == delim)
			break;
		m_putc (m, c);
	}
	return c;
}
/**
 * Similar to m_fscan2, but returns the length of the data scanned.
 *
 * @param m The handle.
 * @param delim The delimiter character.
 * @param fp The file pointer.
 * @return The number of characters scanned, or EOF if no characters
 * were read before EOF.
 */
int m_fscan (int m, char delim, FILE *fp)
{
	int c = m_fscan2 (m, delim, fp);
	if (c == EOF && m_len (m) == 0)
		return EOF;
	return m_len (m);
}
/**
 * Compares two handle's lists element-by-element using memcmp.
 * only makes sense if width is equal on booth lists
 * @param a The first handle.
 * @param b The second handle.
 * @return <0 if a < b, 0 if a == b, >0 if a > b.
 */
int m_cmp (int a, int b)
{
	if (a == b)
		return 0;
	if (m_width (a) != m_width (b))
		return -1;
	int len_a = m_len (a);
	int len_b = m_len (b);
	int min_len = len_a < len_b ? len_a : len_b;
	int res = memcmp (m_buf (a), m_buf (b), min_len * m_width (a));
	if (res != 0)
		return res;
	return len_a - len_b;
}
/**
 * Looks up a key (represented by a handle) in a list of handles.
 * If not found, the key is appended to the list.
 *
 * @param m The handle of the list to search.
 * @param key The handle to look for.
 * @return The handle found or inserted.
 */
int m_lookup (int m, int key)
{
	int p, *d;
	if (m_len (key) == 0)
		ERR ("Key of zero size");
	m_foreach (m, p, d) if (m_cmp (*d, key) == 0) return *d;
	m_put (m, &key);
	return key;
}
/**
 * Looks up an object of fixed size in a list.
 * If not found, the object is appended.
 *
 * @param m The handle of the list to search.
 * @param obj Pointer to the object to look for.
 * @param size The size of the object in bytes.
 * @return The index of the object.
 */
int m_lookup_obj (int m, void *obj, int size)
{
	int p;
	void *d;
	m_foreach (m, p, d) if (memcmp (d, obj, size) == 0) return p;
	p = m_new (m, 1);
	memcpy (mls (m, p), obj, size);
	return p;
}
/**
 * Looks up a string in a list of strings (char *).
 * If not found and NOT_INSERT is 0, the string is duplicated and
 * appended.
 *
 * @param m The handle of the string list.
 * @param key The string to look for.
 * @param NOT_INSERT If non-zero, do not insert the string if not found.
 * @return The index of the string, or -1 if not found and NOT_INSERT is
 * set.
 */
int m_lookup_str (int m, const char *key, int NOT_INSERT)
{
	int p;
	char **d;
	if (!key || strlen (key) == 0)
		ERR ("Key of zero size");
	m_foreach (m, p, d)
	{
		if (*d == NULL)
			continue;
		if (strcmp (*d, key) == 0)
			return p;
	}
	if (NOT_INSERT)
		return -1;
	p = m_new (m, 1);
	*(char **)mls (m, p) = strdup (key);
	return p;
}
/**
 * Appends a single character to a handle's list.
 *
 * @param m The handle.
 * @param c The character to append.
 * @return The character appended.
 */
int m_putc (int m, char c)
{
	lst_t lp = lock_handle (m, 1);
	int p = lst_new (lp, 1);
	*(char *)lst (lp, p) = c;
	unlock_handle (lp);
	return c;
}
/**
 * Appends a single integer to a handle's list.
 *
 * @param m The handle.
 * @param c The integer to append.
 * @return The integer appended.
 */
int m_puti (int m, int c)
{
	lst_t lp = lock_handle (m, 1);
	int p = lst_new (lp, 1);
	*(int *)lst (lp, p) = c;
	unlock_handle (lp);
	return c;
}
#define UTF8GET()                                                              \
	if (EOS ())                                                            \
		return -1;                                                     \
	c = GETCH ();                                                          \
	INC ();                                                                \
	if ((c & 0x80) == 0)                                                   \
		return c;                                                      \
	if ((c & 0x40) == 0)                                                   \
		return 0xFFFD;                                                 \
	if ((c & 0x20) == 0) {                                                 \
		len = 1;                                                       \
		c &= 0x1F;                                                     \
		goto read;                                                     \
	}                                                                      \
	if ((c & 0x10) == 0) {                                                 \
		len = 2;                                                       \
		c &= 0x0F;                                                     \
		goto read;                                                     \
	}                                                                      \
	if ((c & 0x08) == 0) {                                                 \
		len = 3;                                                       \
		c &= 0x07;                                                     \
		goto read;                                                     \
	}                                                                      \
	if ((c & 0x04) == 0) {                                                 \
		len = 4;                                                       \
		c &= 0x03;                                                     \
		goto read;                                                     \
	}                                                                      \
	if ((c & 0x02) == 0) {                                                 \
		len = 5;                                                       \
		c &= 0x01;                                                     \
		goto read;                                                     \
	}                                                                      \
	return 0xFFFD;                                                         \
	read:                                                                  \
	ret = c;                                                               \
	while (len > 0) {                                                      \
		len--;                                                         \
		if (EOS ())                                                    \
			return -1;                                             \
		c = GETCH ();                                                  \
		if ((c & 0xc0) != 0x80)                                        \
			return 0xFFFD;                                         \
		INC ();                                                        \
		ret = (ret << 6) | (c & 0x3f);                                 \
	}                                                                      \
	return ret
/**
 * Decodes a UTF-8 character from a handle's list starting at index p.
 * Increments p by the number of bytes consumed.
 *
 * @param buf The handle of the buffer.
 * @param p Pointer to the current index.
 * @return The decoded Unicode character point, or -1 on error/EOF.
 */
int m_utf8char (int buf, int *p)
{
	unsigned char c;
	uint32_t ret;
	int len;
#define GETCH() (*(unsigned char *)mls (buf, (*p)))
#define EOS() ((*p) >= m_len (buf))
#define INC() ((*p)++)
	UTF8GET ();
#undef GETCH
#undef EOS
#undef INC
}
/**
 * Decodes a UTF-8 character from a string pointer.
 * Increments the pointer by the number of bytes consumed.
 *
 * @param s Pointer to a string pointer.
 * @return The decoded Unicode character point, or -1 on error/EOF.
 */
int utf8char (char **s)
{
	unsigned char c;
	uint32_t ret;
	int len;
#define GETCH() (**s)
#define EOS() ((**s) == 0)
#define INC() ((*s)++)
	UTF8GET ();
#undef GETCH
#undef EOS
#undef INC
}
/**
 * Reads a single UTF-8 character from a file pointer.
 *
 * @param fp The file pointer.
 * @param buf Buffer to store the UTF-8 byte sequence (null-terminated
 * if len < 6).
 * @return The number of bytes in the UTF-8 character, or EOF.
 */
int utf8_getchar (FILE *fp, utf8_char_t buf)
{
	int len, ch, nx, i;
read_single:
	ch = fgetc (fp);
parse_next_char:
	if (ch < 0x80) {
		len = 1;
		goto read_multi_byte;
	}
	if (ch < 0xC0)
		goto read_single;
	if (ch < 0xE0)
		len = 2;
	else if (ch < 0xF0)
		len = 3;
	else if (ch < 0xF8)
		len = 4;
	else if (ch < 0xFC)
		len = 5;
	else
		len = 6;
read_multi_byte:
	buf[0] = ch;
	for (i = 1; i < len; i++) {
		nx = fgetc (fp);
		if (nx == EOF)
			return EOF;
		if ((nx & 0xC0) != 0x80) {
			ch = nx;
			goto parse_next_char;
		}
		buf[i] = nx;
	}
	return len;
}
/**
 * Comparison function for integers, suitable for qsort or binary
 * search.
 *
 * @param a0 Pointer to first integer.
 * @param b0 Pointer to second integer.
 * @return a0 - b0.
 */
int cmp_int (const void *a0, const void *b0)
{
	return (*(const int *)a0) - (*(const int *)b0);
}
/**
 * Inserts an element into a sorted m-array, maintaining order.
 *
 * @param buf The handle of the m-array.
 * @param data Pointer to the element to insert.
 * @param cmpf Comparison function pointer.
 * @param with_duplicates If non-zero, allows duplicate elements.
 * @return The index where the element was inserted, or -index if it
 * exists and duplicates are not allowed.
 * @bugs if data is not allocated or its alloced size if less the
 * m_width(buf) this will crash!
 */
int m_binsert (int buf, const void *data,
	       int (*cmpf) (const void *data, const void *buf_elem),
	       int with_duplicates)
{
	return m_binsert2 (buf, data, cmpf, with_duplicates, 1);
}
/**
 * Inserts an element into a sorted m-array, maintaining order.
 * Like m_binsert, but with_copy=0 skips copying data into the array —
 * only an empty slot is made room for and the caller must fill it.
 * Use when data is not m_width(buf) bytes long (e.g. a C string looked
 * up in an int list).
 *
 * @param buf The handle of the m-array.
 * @param data Pointer to the element to insert.
 * @param cmpf Comparison function pointer.
 * @param with_duplicates If non-zero, allows duplicate elements.
 * @param with_copy If non-zero, data is copied into the array; if zero,
 *        an empty (zeroed) slot is inserted instead.
 * @return The index where the element was inserted, or -index if it
 * exists and duplicates are not allowed.
 */
int m_binsert2 (int buf, const void *data,
		int (*cmpf) (const void *data, const void *buf_elem),
		int with_duplicates, int with_copy)
{
	int left = 0;
	int right = m_len (buf) + 1;
	int cur = 1;
	void *obj;
	int cmp;
	if (m_len (buf) == 0) {
		if (with_copy)
			m_put (buf, data);
		else
			m_ins (buf, 0, 1);
		return 0;
	}
	while (1) {
		cur = (left + right) / 2;
		obj = mls (buf, cur - 1);
		cmp = cmpf (data, obj);
		if (cmp == 0) {
			if (!with_duplicates)
				return -cur;
			break;
		}
		if (cmp < 0) {
			right = cur;
			if (left + 1 == right)
				break;
		} else {
			left = cur;
			if (left + 1 == right) {
				cur++;
				break;
			}
		}
	}
	cur--;
	m_ins (buf, cur, 1);
	if (with_copy)
		m_write (buf, cur, data, 1);
	return cur;
}
/**
 * Looks up an integer in a sorted list using binary search and inserts
 * it if not found.
 *
 * @param buf The handle of the sorted list.
 * @param key The integer to look for.
 * @param new Optional callback function called when a new element is
 * inserted.
 * @param ctx Context pointer for the callback.
 * @return The index of the integer in the list.
 */
int m_blookup_int (int buf, int key, void (*new) (void *, void *), void *ctx)
{
	void *obj = calloc (1, m_width (buf));
	*(int *)obj = key;
	int p = m_binsert (buf, obj, cmp_int, 0);
	free (obj);
	if (p < 0)
		return (-p) - 1;
	if (new)
		new (mls (buf, p), ctx);
	return p;
}
/**
 * Performs a binary search on a sorted m-array.
 *
 * @param key Pointer to the element to search for.
 * @param list The handle of the sorted m-array.
 * @param compar Comparison function pointer.
 * @return The index of the element if found, or -1.
 */
int m_bsearch (const void *key, int list,
	       int (*compar) (const void *, const void *))
{
	if (list < 1 || m_len (list) == 0)
		return -1;
	void *res = bsearch (key, m_buf (list), m_len (list), m_width (list),
			     compar);
	if (res)
		return (res - m_buf (list)) / m_width (list);
	return -1;
}
/**
 * Similar to m_blookup_int, but returns a pointer to the element.
 *
 * @param buf The handle of the sorted list.
 * @param key The integer to look for.
 * @param new Optional callback function.
 * @param ctx Context pointer.
 * @return A pointer to the element.
 */
void *m_blookup_int_p (int buf, int key, void (*new) (void *, void *),
		       void *ctx)
{
	return mls (buf, m_blookup_int (buf, key, new, ctx));
}
/**
 * Inserts an integer into a sorted list using binary search.
 *
 * @param buf The handle of the sorted list.
 * @param key The integer to insert.
 * @return The index where the integer was inserted or found.
 */
int m_binsert_int (int buf, int key)
{
	return m_blookup_int (buf, key, NULL, NULL);
}
/**
 * Searches for an integer in a sorted list using binary search.
 *
 * @param buf The handle of the sorted list.
 * @param key The integer to search for.
 * @return The index of the integer, or -1 if not found.
 */
int m_bsearch_int (int buf, int key)
{
	extern int m_bsearch (const void *key, int list,
			      int (*compar) (const void *, const void *));
	return m_bsearch (&key, buf, cmp_int);
}
/**
 * Returns the number of currently active (allocated, unfreed) handles.
 * Excludes slot 0 (the internal free list).
 *
 * @return The count of active handles.
 */
size_t m_count_allocated (void)
{
	size_t count = 0;
	MLS_MASTER_LOCK ();
	if (!ML.data) {
		MLS_MASTER_UNLOCK ();
		return 0;
	}
	for (int idx = 1; idx < ML.l; idx++) {
		lst_t l = lst (&ML, idx);
#ifdef MLS_THREAD_SAFE
		if (l->lock)
			pthread_rwlock_rdlock (l->lock);
#endif
		if (l->data && l->free_hdl != 255)
			count++;
#ifdef MLS_THREAD_SAFE
		if (l->lock)
			pthread_rwlock_unlock (l->lock);
#endif
	}
	MLS_MASTER_UNLOCK ();
	return count;
}
/**
 * Returns the total number of bytes allocated across all active
 * handles. Sums (capacity x width) for handles that own their memory
 * (no MFREE_NOALLOC).
 *
 * @return The total allocated bytes.
 */
size_t m_total_bytes (void)
{
	size_t total = 0;
	MLS_MASTER_LOCK ();
	if (!ML.data) {
		MLS_MASTER_UNLOCK ();
		return 0;
	}
	for (int idx = 1; idx < ML.l; idx++) {
		lst_t l = lst (&ML, idx);
#ifdef MLS_THREAD_SAFE
		if (l->lock)
			pthread_rwlock_rdlock (l->lock);
#endif
		if (l->data && l->free_hdl != 255 &&
		    !(l->free_hdl & MFREE_NOALLOC))
			total += l->max * l->w;
#ifdef MLS_THREAD_SAFE
		if (l->lock)
			pthread_rwlock_unlock (l->lock);
#endif
	}
	MLS_MASTER_UNLOCK ();
	return total;
}
/**
 * Returns the total number of handle slots in the master list.
 * Since slots are never removed, this equals the peak slot count.
 *
 * @return The slot count.
 */
size_t m_peak_handles (void)
{
	MLS_MASTER_LOCK ();
	size_t slots = (size_t)ML.l;
	MLS_MASTER_UNLOCK ();
	return slots;
}
/**
 * Prints a tabular dump of all active handles to the given stream.
 * Useful for debugging leaks and understanding handle state.
 *
 * @param fp The output FILE stream (stderr if NULL).
 */
void m_debug_print (FILE *fp)
{
	if (!fp)
		fp = stderr;
	MLS_MASTER_LOCK ();
	if (!ML.data) {
		fprintf (fp, "[mls] not initialized\n");
		MLS_MASTER_UNLOCK ();
		return;
	}
	fprintf (fp, "[mls] slots=%zu\n", (size_t)ML.l);
	fprintf (fp, "%-6s  %-10s  %-5s %-8s %-8s %s\n", "Idx", "Handle",
		 "Type", "Len", "Cap", "Data");
	for (int idx = 0; idx < ML.l; idx++) {
		lst_t l = lst (&ML, idx);
#ifdef MLS_THREAD_SAFE
		if (l->lock)
			pthread_rwlock_rdlock (l->lock);
#endif
		if (l->data && l->free_hdl != 255) {
			int h = (int)((unsigned int)idx) |
				((int)(l->uaf_protection) << 24);
			fprintf (fp, "%-6d  0x%08X  0x%02X %-8zu %-8zu %p\n",
				 idx, h, l->free_hdl, l->l, l->max,
				 (void *)l->data);
		}
#ifdef MLS_THREAD_SAFE
		if (l->lock)
			pthread_rwlock_unlock (l->lock);
#endif
	}
	MLS_MASTER_UNLOCK ();
}
/* Debug implementations */
#ifdef MLS_DEBUG
/**
 * Internal debug version of m_init.
 * Initializes the master list and the debug ownership list.
 *
 * @return 0 on success, 1 if already initialized.
 */
int _m_init ()
{
	if (DEB)
		return 1;
	m_init ();
	DEB = m_create (100, sizeof (lst_owner));
	atexit (exit_error);
	return 0;
}

/**
 * Internal debug version of m_destruct.
 * Checks for leaked lists and destroys all resources.
 */
void _m_destruct ()
{
	lst_owner *o;
	int i;
	if (!DEB)
		return;

	for (i = -1; m_next (DEB, &i, &o);) {
		if (o->allocated == 42 && o->ln > 0) {
			WARN ("[%s:%d] [%s]: %d still allocated", o->fn, o->ln,
			      o->fun, i);
		}
	}
	m_free (DEB);
	DEB = 0;
	m_destruct ();
	debi.me = NULL;
}

static void _debug_create_list (int m_uaf, const char *dfunc, int ln,
				const char *fn, const char *fun)
{
	_mlsdb_caller (dfunc, ln, fn, fun, 0, 0, 0, 0);
	int m = REAL_HDL (m_uaf);
	int len = m_len (DEB);
	if (m >= len) {
		m_new (DEB, m - len + 1);
	}

	lst_owner *lo = (lst_owner *)mls (DEB, m);
	lo->ln = ln;
	lo->fn = fn;
	lo->fun = fun;
	lo->allocated = 42;
	TRACE (1, "[%s:%d] [%s] %s(): New: %d (real: %d)", fn, ln, fun, dfunc,
	       m_uaf, m);
}

/**
 * Internal debug version of m_alloc.
 * Records caller information and allocation source.
 *
 * @param ln Caller line number.
 * @param fn Caller filename.
 * @param fun Caller function name.
 * @param n Initial maximum elements.
 * @param w Element width.
 * @return The new handle.
 */
int _m_alloc (int ln, const char *fn, const char *fun, size_t n, size_t w,
	      uint8_t hfree)
{
	int m_uaf = m_alloc (n, w, hfree);
	_debug_create_list (m_uaf, __FUNCTION__, ln, fn, fun);
	return m_uaf;
}

/**
 * Internal debug version of m_free.
 * Records caller information and marks the list as freed in the debug
 * tracker.
 *
 * @param ln Caller line number.
 * @param fn Caller filename.
 * @param fun Caller function name.
 * @param m The handle to free.
 * @return 0.
 */
int _m_free (int ln, const char *fn, const char *fun, int m)
{
	int h = REAL_HDL (m);

	TRACE (1, "[DEBUG] Free List %d", h);

	if (!h)
		return 0;

	_mlsdb_caller (__FUNCTION__, ln, fn, fun, 1, m, 0, 0);
	m_free (m);

	if ((size_t)h >= m_len (DEB)) {
		WARN ("h=%d not in debug list", h);
		return 0;
	}

	lst_owner *o = (lst_owner *)mls (DEB, h);
	o->ln = -ln;
	o->fun = fun;
	o->fn = fn;

	return 0;
}

/**
 * Internal debug version of mls.
 * Records caller information and bounds checks the access.
 *
 * @param ln Caller line number.
 * @param fn Caller filename.
 * @param fun Caller function name.
 * @param h The handle.
 * @param i The index.
 * @return A pointer to the element.
 */
void *_mls (int ln, const char *fn, const char *fun, int h, size_t i)
{
	_mlsdb_caller (__FUNCTION__, ln, fn, fun, 3, h, (int)i, 0);
	return mls (h, i);
}

/**
 * Internal debug version of m_put.
 * Records caller information.
 *
 * @param ln Caller line number.
 * @param fn Caller filename.
 * @param fun Caller function name.
 * @param h The handle.
 * @param d Pointer to the element data.
 * @return The index of the appended element.
 */
int _m_put (int ln, const char *fn, const char *fun, int h, const void *d)
{
	_mlsdb_caller (__FUNCTION__, ln, fn, fun, 5, h, 0, d);
	return m_put (h, d);
}

/**
 * Internal debug version of m_next.
 * Records caller information.
 *
 * @param ln Caller line number.
 * @param fn Caller filename.
 * @param fun Caller function name.
 * @param h The handle.
 * @param i Pointer to the current index.
 * @param d Pointer to store the address of the next element.
 * @return 1 if an element was found, 0 otherwise.
 */
int _m_next (int ln, const char *fn, const char *fun, int h, int *i, void *d)
{
	_mlsdb_caller (__FUNCTION__, ln, fn, fun, 7, h, i ? *i : -1, d);
	return m_next (h, i, d);
}

/**
 * Internal debug version of m_clear.
 * Records caller information.
 *
 * @param ln Caller line number.
 * @param fn Caller filename.
 * @param fun Caller function name.
 * @param h The handle.
 */
void _m_clear (int ln, const char *fn, const char *fun, int h)
{
	_mlsdb_caller (__FUNCTION__, ln, fn, fun, 1, h, 0, 0);
	m_clear (h);
}

/**
 * Internal debug version of m_buf.
 * Records caller information.
 *
 * @param ln Caller line number.
 * @param fn Caller filename.
 * @param fun Caller function name.
 * @param m The handle.
 * @return A pointer to the data buffer.
 */
void *_m_buf (int ln, const char *fn, const char *fun, int m)
{
	if (!m)
		return 0;
	_mlsdb_caller (__FUNCTION__, ln, fn, fun, 3, m, 0, 0);
	return m_buf (m);
}

/**
 * Internal debug version of m_create.
 * Records caller information and allocation source.
 *
 * @param ln Caller line number.
 * @param fn Caller filename.
 * @param fun Caller function name.
 * @param n Initial maximum elements.
 * @param w Element width.
 * @param hfree The free handler ID.
 * @return The new handle.
 */
int _m_create (int ln, const char *fn, const char *fun, size_t n, size_t w)
{
	return _m_alloc (ln, fn, fun, n, w, MFREE);
}

int _s_ccstr (int ln, const char *fn, const char *fun, const char *s)
{
	last_created_hdl = -1;
	int m_uaf = s_ccstr (s);
	if (last_created_hdl != -1) {
		// _debug_create_list(m_uaf, __FUNCTION__, ln, fn, fun
		// );
	}
	return m_uaf;
}

int _s_cstrdup (int ln, const char *fn, const char *fun, const char *s)
{
	last_created_hdl = -1;
	int m_uaf = s_cstrdup (s);
	if (last_created_hdl != -1) {
		_debug_create_list (m_uaf, __FUNCTION__, ln, fn, fun);
	}
	return m_uaf;
}

int _m_wrapstrings (int ln, const char *fn, const char *fun, char **list,
		    int nelem)
{
	int m_uaf = m_wrapstrings (list, nelem);
	// _debug_create_list(m_uaf, __FUNCTION__, ln, fn, fun );
	return m_uaf;
}

int _m_wrapints (int ln, const char *fn, const char *fun, int *list, int nelem)
{
	int m_uaf = m_wrapints (list, nelem);
	// _debug_create_list(m_uaf, __FUNCTION__, ln, fn, fun );
	return m_uaf;
}

int _m_wrapcstr (int ln, const char *fn, const char *fun, char *s)
{
	int m_uaf = m_wrapcstr (s);
	// _debug_create_list(m_uaf, __FUNCTION__, ln, fn, fun );
	return m_uaf;
}
#endif
void conststr_init (void) {}
