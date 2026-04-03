
/* disable macros to override m_alloc, ... */
#define MLS_DEBUG_DISABLE
#include "mls.h"


#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Debug globals */
int trace_level = 0;
static int error_occurred = 0;
static lst_t freefn = 0;
/* this could be a define but that is not easy to debug */
static inline int REAL_HDL( int m ) { return (m &  0xffffff) - 1; }
static int UAF_PROTECTION = 0;


/**
 * Prints an error message with file and line information and terminates the program.
 * Sets the error_occurred flag for post-mortem analysis.
 *
 * @param line The line number where the error occurred.
 * @param file The source file where the error occurred.
 * @param function The function where the error occurred.
 * @param format The format string for the error message.
 */
void deb_err (int line, const char *file, const char *function,
	      const char *format, ...)
{
	error_occurred = 1;
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
 * Prints a trace message if the provided level is greater than or equal to trace_level.
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

// ********************************************
//
// lst_XXX and
// m_XXX implementation
//
// ********************************************

lst_t ML = 0; // stack allocated vars
lst_t FR = 0; // stack freed vars

/**
 * Returns the current size of the master list (stack of allocated handles).
 *
 * @return The number of handles in the master list.
 */
int print_stacksize ()
{
	if (!ML)
		return 0;
	return ML->l;
}

/**
 * Returns a pointer to the element at the specified index in a list structure.
 *
 * @param l The list structure.
 * @param i The index of the element.
 * @return A pointer to the element data.
 */
void *lst (lst_t l, int i) { return &l->d[l->w * i]; }

struct lst_owner_st {
	int allocated;
	const char *fn, *fun;
	int ln;
};
typedef struct lst_owner_st lst_owner;

static int DEB = 0; // debug list

struct debug_info_st {
	char msg[500];
	const char *me, *fn, *fun;
	int ln, args, handle, index;
	const void *data;
};

static struct debug_info_st debi;

/**
 * Records caller information for the current MLS operation.
 * Used by debug wrappers to provide context for error messages.
 *
 * @param me The name of the function being wrapped.
 * @param ln The line number of the call.
 * @param fn The filename of the call.
 * @param fun The function name of the caller.
 * @param args Bitmask indicating which arguments are valid (1:handle, 2:index, 4:data).
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
	lst_t *lp;
	lst_owner *o;
	int orig = debi.handle;
	int h = (orig & 0xffffff);
	perr ("  Handle:    %d (UAF protection: %d)", h, (orig >> 24) & 0x7f);

	if (h < 1 || h > m_len (DEB)) {
		perr ("  Status:    Handle out of range (max=%d)", m_len (DEB));
		return -1;
	}

	lp = (lst_t *)lst (ML, h - 1);
	if (*lp == NULL) {
		perr ("  Status:    List base address for handle %d is not allocated",
		      h);
	} else {
		if ((**lp).uaf_protection != ((orig >> 24) & 0x7f)) {
			perr ("  Status:    uaf protection pattern does not match, expected:%d, got:%d",
			      (**lp).uaf_protection, (orig >> 24) & 0x7f);
			return -1;
		}
	}

	o = (lst_owner *)mls (DEB, h - 1);
	if (!o || o->allocated != 42) {
		perr ("  Status:    Array was not allocated");
		return -1;
	}

	if (o->ln < 0) {
		perr ("  Status:    Previously removed by %s() at %s:%d", o->fun,
		      o->fn, -o->ln);
		return -1;
	}

	perr ("  Created by: %s() at %s:%d", o->fun, o->fn, o->ln);

	if (*lp) {
		perr ("  Metadata:  struct=%p, data=%p, width=%d\n"
		      "  Buffer:    used=%d, max=%d",
		      *lp, (**lp).d, (**lp).w, (**lp).l, (**lp).max);
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
		perr ("  Index:     %d is out of bounds (len=%d)", i, m_len (h));
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
	if (!error_occurred || !debi.me)
		return;

	perr ("\n[mls post-mortem analysis]");
	perr ("  Operation: %s()", debi.me);
	perr ("  Context:   Called from %s() at %s:%d", debi.fun, debi.fn,
	      debi.ln);

	if (!ML) {
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
 * Resizes a list structure to a new maximum size.
 *
 * @param lp Pointer to the list structure pointer.
 * @param new_size The new maximum number of elements.
 */
void lst_resize (lst_t *lp, int new_size)
{
	int oldSize, diff;
	int w = (**lp).w;
	oldSize = (**lp).max * w + sizeof (struct ls_st);
	*lp = (lst_t)realloc (*lp, new_size * w + sizeof (struct ls_st));
	if (!*lp)
		ERR ("Out of Memory");
	if (new_size > (**lp).max) {
		diff = (new_size - (**lp).max) * w;
		memset (((char *)*lp) + oldSize, 0, diff);
	}
	(**lp).max = new_size;
}

/**
 * Creates a new list structure.
 *
 * @param max The initial maximum number of elements.
 * @param w The width of each element in bytes.
 * @return The newly created list structure.
 */
lst_t lst_create (int max, int w)
{
	lst_t l = (lst_t)calloc (1, (max * w) + sizeof (struct ls_st));
	if (!l)
		ERR ("Out of Memory");
	l->max = max;
	l->l = 0;
	l->w = w;
	return l;
}

/**
 * Reserves space for n new elements in a list.
 *
 * @param lp Pointer to the list structure pointer.
 * @param n The number of elements to reserve.
 * @return The index of the first newly reserved element.
 */
int lst_new (lst_t *lp, int n)
{
	int p = (**lp).l;
	int max = (**lp).max;
	if (p + n > max) {
		int newsiz = max + n;
		newsiz = increase_by_percent (newsiz, 50);
		lst_resize (lp, newsiz);
	}
	(**lp).l += n;
	return p;
}

/**
 * Appends an element to a list.
 *
 * @param lp Pointer to the list structure pointer.
 * @param d Pointer to the element data to append.
 * @return The index of the appended element.
 */
int lst_put (lst_t *lp, const void *d)
{
	int p = lst_new (lp, 1);
	memcpy (lst (*lp, p), d, (**lp).w);
	return p;
}

/**
 * Returns a pointer to the element at the specified index without bounds checking.
 *
 * @param l The list structure.
 * @param i The index of the element.
 * @return A pointer to the element data, or NULL if index is out of bounds.
 */
void *lst_peek (lst_t l, int i)
{
	if (i >= l->l || i < 0)
		return NULL;
	return lst (l, i);
}

/**
 * Deletes an element at the specified index from a list.
 *
 * @param l The list structure.
 * @param p The index of the element to delete.
 */
void lst_del (lst_t l, int p)
{
	if (p < 0 || p >= l->l)
		ERR ("Wrong Arg p=%d", p);
	int w = l->w;
	int n = l->l - p - 1;
	if (n > 0)
		memmove (lst (l, p), lst (l, p + 1), n * w);
	l->l--;
}

/**
 * Removes n elements starting from the specified index from a list.
 *
 * @param lp Pointer to the list structure pointer.
 * @param p The starting index.
 * @param n The number of elements to remove.
 */
void lst_remove (lst_t *lp, int p, int n)
{
	if (p < 0 || n < 0 || p + n > (**lp).l)
		ERR ("Wrong Arg p=%d n=%d", p, n);
	int w = (**lp).w;
	int move_n = (**lp).l - (p + n);
	if (move_n > 0)
		memmove (lst (*lp, p), lst (*lp, p + n), move_n * w);
	(**lp).l -= n;
}

/**
 * Inserts n empty elements at the specified index in a list.
 *
 * @param lp Pointer to the list structure pointer.
 * @param p The insertion index.
 * @param n The number of elements to insert.
 * @return A pointer to the first newly inserted element.
 */
void *lst_ins (lst_t *lp, int p, int n)
{
	int cnt;
	if (p < 0 || p > (**lp).l)
		return 0;
	cnt = (**lp).l - p;
	lst_new (lp, n);
	if (cnt > 0)
		memmove (lst (*lp, p + n), lst (*lp, p), cnt * (**lp).w);
	memset (lst (*lp, p), 0, n * (**lp).w);
	return lst (*lp, p);
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
	if (*p == (int)l->l)
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
 * @param data Pointer to the destination buffer pointer. If *data is 0, a buffer is allocated.
 * @param n The number of elements to read.
 * @return 0 on success.
 */
int lst_read (lst_t l, int p, void **data, int n)
{
	if (p < 0 || n < 0 || data == NULL)
		ERR ("Wrong arguments");
	if (*data == 0)
		*data = malloc (l->w * n);
	if (!*data)
		ERR ("Out of Memory");
	memcpy (*data, lst (l, p), n * l->w);
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
int lst_write (lst_t *lp, int p, const void *data, int n)
{
	if (p < 0 || n < 0 || data == NULL)
		ERR ("Wrong arguments");
	if (p + n > (**lp).max)
		lst_resize (lp, p + n);
	if (p + n > (**lp).l)
		(**lp).l = p + n;
	memcpy (lst (*lp, p), data, n * (**lp).w);
	return 0;
}



/**
 * Internal function to retrieve a pointer to the list structure associated with a handle.
 * Performs validation checks for handle range, existence, and UAF protection.
 * @param m The handle to look up.
 * @return A pointer to the list structure pointer in the master list.
 */

static inline lst_t * get_list(int m) {
	if ( ML == 0 ) {
		ERR("Not initialized");
	}
	int idx =  (m & 0xffffff) - 1;
	int uaf =  ((m >> 24) & 0x7f);
	if (idx < 0 || idx >= ML->l) {
		ERR( "Invalid Handle %d", idx );
	}
	lst_t *l = (lst_t *)lst(ML, idx);
	if (*l == NULL) {
		ERR("List %d not allocated", idx );
	}
	
	if ( (*l)->uaf_protection != uaf ) {
		ERR("uaf protection pattern does not match, expected:%d, got:%d",
		    (*l)->uaf_protection, uaf );
	}

	return l;
}

/**
 * Public accessor for getting a list pointer from a handle.
 *
 * @param r The handle.
 * @return A pointer to the list structure pointer.
 */
lst_t *exported_get_list (int r) { return get_list (r); }

extern void m_free_strings (int list, int CLEAR_ONLY);

/**
 * same as m_free_strings to be used as a custom free handler.
 *
 * @param l The list structure being freed.
 */
static void free_strings_wrap (int h)
{
	int p; char **d;
	m_foreach(h,p,d) {
		if (*d) {
			free (*d);
			*d = NULL;
		}
	}
}

/**
 * Custom free handler that recursively frees MLS handles stored in a list.
 *
 * @param l The list structure being freed.
 */
static void free_list_wrap (int h)
{
	TRACE(1,"HDL:%d", REAL_HDL(h));
	int p, *d;
	m_foreach(h,p,d) {
		/* what happens when a list is inserted twice ? 
		   can we check if we freed the list? then it is not an error.
		   right now, we just ignore double-free error
		 */
		if(  m_is_freed(*d) ) {
			TRACE(1,"not freeing list %d allready freed", *d );
		}
		else {	
		#ifdef MLS_DEBUG
			_m_free (__LINE__, __FILE__, __FUNCTION__, (*d));
		#else
			m_free (*d);
		#endif
		}
	}
}

/**
 * Initializes the MLS library system.
 * Allocates the master handle list and registers default free handlers.
 *
 * @return 0 on success, 1 if already initialized.
 */
int m_init ()
{
	if (ML)
		return 1;
	ML = lst_create (100, sizeof (lst_t));
	FR = lst_create (100, sizeof (int));
	freefn = lst_create (MFREE_MAX + 1, sizeof (void *));
	free_fn_t f = NULL;
	lst_put (&freefn, &f);
	f = free_strings_wrap;
	lst_put (&freefn, &f);
	f = free_list_wrap;
	lst_put (&freefn, &f);
	return 0;
}

/**
 * Destroys the MLS library system.
 * Explicitly frees all remaining allocated handles and their contents.
 */
void m_destruct ()
{
	int idx;
	lst_t *d;
	if (!ML)
		ERR ("Not Init.");

	idx = -1;
	while (lst_next (ML, &idx, &d)) {
		if (*d) {
			int m = (idx + 1) | (((int)(**d).uaf_protection) << 24);
			m_free (m);
		}
	}

	if (FR) {
		free (FR);
		FR = 0;
	}

	if (ML) {
		free (ML);
		ML = 0;
	}

	if (freefn) {
		free (freefn);
		freefn = 0;
	}
}

/**
 * Allocates a new MLS handle with a specific free handler.
 *
 * @param max Initial maximum elements.
 * @param w Width of each element in bytes.
 * @param hfree The ID of the registered free handler to use.
 * @return A new 1-based MLS handle with UAF protection pattern.
 */
int m_alloc (int max, int w, uint8_t hfree)
{
	int h;
	if (FR->l > 0) {
		h = *(int *)lst (FR, FR->l - 1);
		FR->l--;
	} else {
		h = lst_new (&ML, 1);
	}
	lst_t *l = (lst_t *)lst (ML, h);
	*l = lst_create (max, w);
	(*l)->free_hdl = hfree;
	(*l)->uaf_protection = UAF_PROTECTION;
	int res = (h + 1) | (((int)(*l)->uaf_protection) << 24);
	return res;
}

/**
 * Creates a new MLS handle with the default free handler (MFREE).
 *
 * @param max Initial maximum elements.
 * @param w Width of each element.
 * @return A new handle.
 */
int m_create (int max, int w) { return m_alloc (max, w, MFREE); }

/**
 * Frees an MLS handle and its associated list data.
 * The handle is marked as freed and returned to the free list for reuse.
 *
 * @param m The handle to free.
 * @return 0 on success.
 */
int m_free (int m)
{
	TRACE(1, "Hdl:%d real: %d", m, REAL_HDL(m) );
	if( m == 0 ) return 0;
	lst_t *lp = get_list (m);
	lst_t l_ptr = *lp;
	int hdl = l_ptr->free_hdl;
	int realh =  REAL_HDL(m);
	
	if( hdl == 255 ) {
		TRACE(1, "HDL %d free-in-progress", realh );
		return 0;
	};


	if (hdl >= freefn->l)
		ERR ("free handler %d for list %d is invalid max: %d", hdl, realh, freefn->l );
	
	UAF_PROTECTION = (UAF_PROTECTION + 1) & 0x7f;

	if ( hdl >  0 ) { 
		l_ptr->free_hdl = 255; /* Mark handle as being freed */
		free_fn_t *xfree = lst (freefn, hdl);
		if (*xfree) (*xfree) (m);
	}

	free(*lp);
	*lp=0;
	lst_put(&FR, &realh);
	TRACE(1, "freed: %d, real:%d", m, realh );

	
	return 0;
}

/**
 * @brief Registers a  a cleanup callback for managed arrays 
 * The @p free_fn is triggered automatically when m_free() is called.
 * This allows for custom iteration and deallocation of array elements
 * before the array container itself is removed.
 *
 * @param free_fn The function to be called when freeing handles with this handler ID.
 * @return The handler ID on success, -1 if handler is invalid or too many handlers registered.
 */
int m_reg_freefn ( free_fn_t  free_fn )
{
	if( freefn->l >= 255 || ! free_fn ) return -1;
	return lst_put ( &freefn, & free_fn );
}

/**
 * Checks if an MLS handle is valid and hasn't been freed.
 *
 * @param h The handle to check.
 * @return 1 if the handle is freed or invalid, 0 if it is active.
 */
int m_is_freed (int h)
{
	if (h < 1 || !ML)
		return 1;
	int i = (h & 0xffffff) - 1;
	if (i < 0 || i >= ML->l)
		return 1;
	lst_t *l = (lst_t *)lst (ML, i);
	if (!*l)
		return 1;
	if (((h >> 24) & 0x7f) != (**l).uaf_protection)
		return 1;
	if( (**l).free_hdl == 255 ) return 1;
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
	lst_t *lp = get_list (h);
	return (**lp).free_hdl;
}

/**
 * Returns the number of elements in the list associated with a handle.
 *
 * @param m The handle.
 * @return The number of elements.
 */
int m_len (int m)
{
	if (m <= 0)
		return 0;
	lst_t *lp = get_list (m);
	return (**lp).l;
}

/**
 * Returns a pointer to the raw data buffer of the list associated with a handle.
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
void *mls (int m, int i)
{
	if (m <= 0)
		return NULL;
	lst_t *lp = get_list (m);
	void *res = lst_peek (*lp, i);
	if (!res)
		ERR ("Index %d out of bounds for handle %d (len %d)", i, m,
		     (**lp).l);
	return res;
}

/**
 * Reserves space for n new elements in a handle's list.
 *
 * @param m The handle.
 * @param n The number of elements to reserve.
 * @return The index of the first newly reserved element.
 */
int m_new (int m, int n)
{
	if (m <= 0)
		return -1;
	lst_t *lp = get_list (m);
	return lst_new (lp, n);
}

/**
 * Appends one new element to a handle's list and returns a pointer to it.
 *
 * @param m The handle.
 * @return A pointer to the new element.
 */
void *m_add (int m)
{
	if (m <= 0)
		return NULL;
	int p = m_new (m, 1);
	return mls (m, p);
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
	lst_t *lp = get_list (m);
	return lst_next (*lp, p, d);
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
	lst_t *lp = get_list (m);
	return lst_put (lp, data);
}

/**
 * Sets the logical length of a handle's list.
 *
 * @param m The handle.
 * @param len The new length.
 * @return 0 on success.
 */
int m_setlen (int m, int len)
{
	if (m <= 0)
		return -1;
	if (len < 0)
		ERR ("Wrong Arg len=%d", len);
	lst_t *lp = get_list (m);
	if (len > (**lp).max)
		lst_resize (lp, len);
	(**lp).l = len;
	return 0;
}

/**
 * Returns the currently allocated capacity (buffer size) of a handle's list.
 *
 * @param m The handle.
 * @return The maximum number of elements before a realloc is needed.
 */
int m_bufsize (int m)
{
	if (m <= 0)
		return 0;
	lst_t *lp = get_list (m);
	return (**lp).max;
}

/**
 * Returns a pointer to the element at the specified index without bounds checking.
 *
 * @param m The handle.
 * @param i The index.
 * @return A pointer to the element, or NULL if index is out of bounds.
 */
void *m_peek (int m, int i)
{
	if (m <= 0)
		return NULL;
	lst_t *lp = get_list (m);
	return lst_peek (*lp, i);
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
int m_write (int m, int p, const void *data, int n)
{
	if (m <= 0)
		return -1;
	lst_t *lp = get_list (m);
	return lst_write (lp, p, data, n);
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
int m_read (int h, int p, void **data, int n)
{
	if (h <= 0)
		return -1;
	lst_t *lp = get_list (h);
	return lst_read (*lp, p, data, n);
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
	lst_t *lp = get_list (m);
	(**lp).l = 0;
}

/**
 * Deletes an element at the specified index from a handle's list.
 * Remaining elements are shifted to close the gap.
 *
 * @param m The handle.
 * @param p The index of the element to delete.
 */
void m_del (int m, int p)
{
	if (m <= 0)
		return;
	lst_t *lp = get_list (m);
	lst_del (*lp, p);
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
	lst_t *lp = get_list (m);
	if ((**lp).l == 0)
		return NULL;
	(**lp).l--;
	return lst (*lp, (**lp).l);
}

/**
 * Inserts n empty (zero-initialized) elements at a specific index in a handle's list.
 *
 * @param m The handle.
 * @param p The insertion index.
 * @param n The number of elements to insert.
 * @return 1 on success, 0 on failure.
 */
int m_ins (int m, int p, int n)
{
	if (m <= 0)
		return 0;
	lst_t *lp = get_list (m);
	return (lst_ins (lp, p, n) != NULL);
}

/**
 * Returns the width of each element in a handle's list in bytes.
 *
 * @param m The handle.
 * @return The element width.
 */
int m_width (int m)
{
	if (m <= 0)
		return 0;
	lst_t *lp = get_list (m);
	return (**lp).w;
}

/**
 * Resizes the allocated capacity of a handle's list.
 *
 * @param m The handle.
 * @param new_size The new maximum number of elements.
 */
void m_resize (int m, int new_size)
{
	if (m <= 0)
		return;
	lst_t *lp = get_list (m);
	lst_resize (lp, new_size);
}

#ifdef MLS_DEBUG
#define m_create_internal(n, w) _m_create (__LINE__, __FILE__, __FUNCTION__, (n), (w))
#define m_alloc_internal(n, w, h) _m_alloc (__LINE__, __FILE__, __FUNCTION__, (n), (w), (h))
#else
#define m_create_internal(n, w) m_create (n, w)
#define m_alloc_internal(n, w, h) m_alloc (n, w, h)
#endif

/**
 * Extracts a sub-range of elements from one list and appends or writes them into another.
 * Supports negative indices (relative to end of list).
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
	if (!m) {
		if (dest > 0)
			m_setlen (dest, offs);
		return dest;
	}
	int len = m_len (m);
	if (b < 0)
		b += len;
	if (a < 0)
		a += len;
	if (b >= len)
		b = len - 1;
	if (a >= len)
		a = len - 1;
	if (a < 0)
		a = 0;
	int cnt = b - a + 1;
	if (cnt < 0)
		cnt = 0;
	if (dest <= 0) {
                #ifdef MLS_DEBUG
		dest = _m_create (__LINE__, __FILE__, __FUNCTION__, cnt + offs, m_width (m) );
		#else
		dest = m_create (cnt + offs, m_width (m));		
		#endif
	}
	m_setlen (dest, offs);
	if (cnt > 0) {
		m_write (dest, offs, mls (m, a), cnt);
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
void m_remove (int m, int p, int n)
{
	lst_t *lp = get_list (m);
	lst_remove (lp, p, n);
}


/**
 * Zeroes out the entire data buffer of a handle's list.
 *
 * @param m The handle.
 */
void m_bzero (int m)
{
	lst_t *lp = get_list (m);
	memset ((**lp).d, 0, (**lp).l * (**lp).w);
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
 * @return The number of characters scanned, or EOF if no characters were read before EOF.
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
 *
 * @param a The first handle.
 * @param b The second handle.
 * @return <0 if a < b, 0 if a == b, >0 if a > b.
 */
int m_cmp (int a, int b)
{
	if (a == b)
		return 0;
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
 * If not found and NOT_INSERT is 0, the string is duplicated and appended.
 *
 * @param m The handle of the string list.
 * @param key The string to look for.
 * @param NOT_INSERT If non-zero, do not insert the string if not found.
 * @return The index of the string, or -1 if not found and NOT_INSERT is set.
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
int m_putc (int m, char c) { return *(char *)m_add (m) = c; }

/**
 * Appends a single integer to a handle's list.
 *
 * @param m The handle.
 * @param c The integer to append.
 * @return The integer appended.
 */
int m_puti (int m, int c) { return *(int *)m_add (m) = c; }

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
		c &= 0b00011111;                                               \
		goto read;                                                     \
	}                                                                      \
	if ((c & 0x10) == 0) {                                                 \
		len = 2;                                                       \
		c &= 0b00001111;                                               \
		goto read;                                                     \
	}                                                                      \
	if ((c & 0x08) == 0) {                                                 \
		len = 3;                                                       \
		c &= 0b00000111;                                               \
		goto read;                                                     \
	}                                                                      \
	if ((c & 0x04) == 0) {                                                 \
		len = 4;                                                       \
		c &= 0b00000011;                                               \
		goto read;                                                     \
	}                                                                      \
	if ((c & 0x02) == 0) {                                                 \
		len = 5;                                                       \
		c &= 0b00000001;                                               \
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
 * @param buf Buffer to store the UTF-8 byte sequence (null-terminated if len < 6).
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
 * Comparison function for integers, suitable for qsort or binary search.
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
 * Looks up an integer in a sorted list using binary search and inserts it if not found.
 *
 * @param buf The handle of the sorted list.
 * @param key The integer to look for.
 * @param new Optional callback function called when a new element is inserted.
 * @param ctx Context pointer for the callback.
 * @return The index of the integer in the list.
 */
int m_blookup_int (int buf, int key, void (*new) (void *, void *), void *ctx)
{
	void *obj = calloc (1, m_width (buf));
	*(int *)obj = key;
	extern int m_binsert (int buf, const void *data,
			      int (*cmpf) (const void *a, const void *b),
			      int with_duplicates);
	int p = m_binsert (buf, obj, cmp_int, 0);
	free (obj);
	if (p < 0)
		return (-p) - 1;
	if (new)
		new (mls (buf, p), ctx);
	return p;
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


/* Debug implementations */

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
			WARN ("List %d still allocated. Source: %s() in %s:%d",
			      i + 1, o->fun, o->fn, o->ln);
		}
	}
	m_free (DEB);
	DEB = 0;
	m_destruct ();
	debi.me = NULL;
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
 * @return The new handle.
 */
int _m_create (int ln, const char *fn, const char *fun, int n, int w)
{
	lst_owner *lo;
	int len, m, m_uaf;
	_mlsdb_caller (__FUNCTION__, ln, fn, fun, 0, 0, 0, 0);
	m_uaf = m_create (n, w);

	m = m_uaf & 0xffffff;
	len = m_len (DEB);
	if (m > len) {
		m_new (DEB, m - len);
	}
	
	lo = (lst_owner *)mls (DEB, m - 1);

	lo->ln = ln;
	lo->fn = fn;
	lo->fun = fun;
	lo->allocated = 42;
	TRACE (1, "NEW LIST %d (real: %d) allocated by %s:%d in %s", m_uaf, m-1, fun, ln, fn);
	return m_uaf;
}

/**
 * Internal debug version of m_free.
 * Records caller information and marks the list as freed in the debug tracker.
 *
 * @param ln Caller line number.
 * @param fn Caller filename.
 * @param fun Caller function name.
 * @param m The handle to free.
 * @return 0.
 */
int _m_free (int ln, const char *fn, const char *fun, int m)
{
	if (!m)
		return 0;

	_mlsdb_caller (__FUNCTION__, ln, fn, fun, 1, m, 0, 0);
	m_free (m);

	int h = m & 0xffffff;
	lst_owner *o = (lst_owner *)mls (DEB, h - 1);
	o->ln = -ln;
	o->fun = fun;
	o->fn = fn;
	TRACE (1, "DEB Free List %d", h);
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
void *_mls (int ln, const char *fn, const char *fun, int h, int i)
{
	_mlsdb_caller (__FUNCTION__, ln, fn, fun, 3, h, i, 0);
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
 * Internal debug version of m_alloc.
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
int _m_alloc (int ln, const char *fn, const char *fun, int n, int w,
	      uint8_t hfree)
{
	int h = _m_create (ln, fn, fun, n, w);
	lst_t *lp = get_list (h);
	(**lp).free_hdl = hfree;
	return h;
}
