#ifndef MLS_H
#define MLS_H

#ifdef __plusplus
extern "C" {
#endif

/* Portable thread-local storage */
#ifndef MLS_THREAD_LOCAL
#if __STDC_VERSION__ >= 201112L
#define MLS_THREAD_LOCAL _Thread_local
#elif defined(__GNUC__)
#define MLS_THREAD_LOCAL __thread
#else
#define MLS_THREAD_LOCAL
#endif
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef is_empty
#define is_empty(s) (!((s) && *(s)))
#endif

#ifndef ALEN
#define ALEN(x) (sizeof (x) / sizeof (*x))
#endif

#ifndef Max
#define Max(x, y) ((x) > (y) ? (x) : (y))
#endif

#ifndef Min
#define Min(x, y) ((x) > (y) ? (y) : (x))
#endif

#ifndef BIT
#define BIT(x) (1 << (x))
#endif

#define ASERR(n, f, a...)                                                      \
	do {                                                                   \
		if (!(n))                                                      \
			ERR ("ASSERT:" #n "\n" #f, ##a);                       \
	} while (0)
#define ASSERT(n)                                                              \
	do {                                                                   \
		if (!(n))                                                      \
			ERR ("ASSERT " #n);                                    \
	} while (0)
#define ERR(n, a...) deb_err (__LINE__, __FILE__, __FUNCTION__, n, ##a)
#define WARN(n, a...) deb_warn (__LINE__, __FILE__, __FUNCTION__, n, ##a)
#define TRACE(l, n, a...)                                                      \
	do {                                                                   \
		if (((l) & trace_level) != 0)                                  \
			deb_trace (l, __LINE__, __FILE__, __FUNCTION__, n,     \
				   ##a);                                       \
	} while (0)
#define ERREX ERR ("Schwerer Unbekannter Programmfehler")
#define TR(a) TRACE (a, "");

void deb_err (int line, const char *file, const char *function,
	      const char *format, ...) __attribute__ ((format (printf, 4, 5)));
void deb_warn (int line, const char *file, const char *function,
	       const char *format, ...) __attribute__ ((format (printf, 4, 5)));
void deb_trace (int l, int line, const char *file, const char *function,
		const char *format, ...)
	__attribute__ ((format (printf, 5, 6)));

extern int trace_level;

enum mls_error {
	MLS_OK = 0,
	MLS_EINVAL,
	MLS_EBOUNDS,
	MLS_ENOMEM,
	MLS_EUAF,
	MLS_EOVERFLOW,
};

extern MLS_THREAD_LOCAL int mls_errno;
extern MLS_THREAD_LOCAL const char *mls_errfunc;
extern MLS_THREAD_LOCAL const char *mls_errfile;
extern MLS_THREAD_LOCAL int mls_errline;
const char *mls_errmsg (int code);

enum predefined_free_handler {
	MFREE = 0,
	MFREE_STR = 1,	       /* iterate each element and call free() */
	MFREE_EACH = 2,	       /* iterate each element and call m_free() */
	MFREE_NODESTRUCT = 64, /* do not touch at all */
	MFREE_NOALLOC =
		128, /* BITMAP: runtime protection against free/realloc */
	NOHDL = 255, /* do not touch on m_free(), leave alone */
	MFREE_MASK = 63,
};
typedef void (*free_fn_t) (int m);

/* the function free_fn will be called if m_free(this array) is called
   the free_fn can now iterate and clean all elements of the array,
   before this array is removed.
   the returned value is the handle id that you supply to m_alloc.
*/
int m_reg_freefn (free_fn_t free_fn);
int m_alloc (size_t max, size_t w, uint8_t hfree);
int m_alloc_safe (size_t max, size_t w, uint8_t hfree);
int m_free (int m);

int m_is_freed (int h);
int m_is_valid (int h);
int m_free_hdl (int h);
int m_dub (int m);

size_t m_len (int m);
void *m_buf (int m);

#define CHARP(m) ((char *)m_buf (m))

void *mls (int m, size_t i);
void *mls_safe (int m, size_t i);
int m_new (int m, size_t n);
int m_new_safe (int m, size_t n);
void *m_add (int m);
void *m_add_safe (int m);
int m_next (int m, int *p, void *d);
int m_init ();
void m_destruct ();

size_t m_count_allocated (void);
size_t m_total_bytes (void);
size_t m_peak_handles (void);
void m_debug_print (FILE *fp);

int m_create (size_t max, size_t w);
int m_create_safe (size_t max, size_t w);
int m_set_data (int m, size_t len, size_t w, const void *data);
int m_put (int m, const void *data);
int m_put_safe (int m, const void *data);
int m_setlen (int m, size_t len);
int m_setlen_safe (int m, size_t len);
size_t m_bufsize (int m);
void *m_peek (int m, size_t i);
int m_write (int m, size_t p, const void *data, size_t n);
int m_write_safe (int m, size_t p, const void *data, size_t n);
int m_read (int h, size_t p, void **data, size_t n);
int m_read_safe (int h, size_t p, void **data, size_t n);
void m_clear (int m);
void m_del (int m, size_t p);
int m_del_safe (int m, size_t p);
void *m_pop (int m);
int m_ins (int m, size_t p, size_t n);
int m_ins_safe (int m, size_t p, size_t n);
size_t m_width (int m);
void m_resize (int m, size_t new_size);
int m_resize_safe (int m, size_t new_size);
int m_slice (int dest, int offs, int m, int a, int b);
void m_remove (int m, size_t p, size_t n);
static inline char *m_str (int m)
{
	if (m_is_freed (m) || m_len (m) == 0)
		return "";
	char *s = (char *)m_buf (m);
#ifdef MLS_DEBUG
	if (s[m_len (m) - 1] != 0)
		ERR ("handle %d not zero terminated", m);
#endif
	return s;
}

int _m_init ();
void _m_destruct ();
int _m_create (int ln, const char *fn, const char *fun, size_t n, size_t w);
int _m_free (int ln, const char *fn, const char *fun, int m);
void *_mls (int ln, const char *fn, const char *fun, int h, size_t i);
int _m_put (int ln, const char *fn, const char *fun, int h, const void *d);
int _m_next (int ln, const char *fn, const char *fun, int h, int *i, void *d);
void _m_clear (int ln, const char *fn, const char *fun, int h);
void *_m_buf (int ln, const char *fn, const char *fun, int m);
int _m_alloc (int ln, const char *fn, const char *fun, size_t n, size_t w,
	      uint8_t hfree);
int _m_wrapcstr (int ln, const char *fn, const char *fun, char *s);
int _m_wrapints (int ln, const char *fn, const char *fun, int *list, int nelem);
int _m_wrapstrings (int ln, const char *fn, const char *fun, char **list,
		    int nelem);
int _s_cstrdup (int ln, const char *fn, const char *fun, const char *s);
int _s_ccstr (int ln, const char *fn, const char *fun, const char *s);

#define m_foreach(lst, index, ptr) for (index = -1; m_next (lst, &index, &ptr);)
#define STR(x, i) (*(char **)mls ((x), (i)))
#define INT(x, i) (*(int *)mls ((x), (i)))
#define UINT(x, i) (*(unsigned int *)mls ((x), (i)))
#define FLOAT(x, i) (*(float *)mls ((x), (i)))
#define DOUBLE(x, i) (*(double *)mls ((x), (i)))
#define PTR(x, i) (*(void **)mls ((x), (i)))
#define U32(x, i) (*(uint32_t *)mls ((x), (i)))
#define U64(x, i) (*(uint64_t *)mls ((x), (i)))
#define CHAR(x, i) (*(char *)mls ((x), (i)))
#define UCHAR(x, i) (*(unsigned char *)mls ((x), (i)))

/* Unchecked variants — no bounds check, uses m_peek. Fast but unsafe. */
#define INT_UNCHECKED(x, i) (*(int *)m_peek ((x), (i)))
#define UINT_UNCHECKED(x, i) (*(unsigned int *)m_peek ((x), (i)))
#define FLOAT_UNCHECKED(x, i) (*(float *)m_peek ((x), (i)))
#define DOUBLE_UNCHECKED(x, i) (*(double *)m_peek ((x), (i)))
#define PTR_UNCHECKED(x, i) (*(void **)m_peek ((x), (i)))
#define U32_UNCHECKED(x, i) (*(uint32_t *)m_peek ((x), (i)))
#define U64_UNCHECKED(x, i) (*(uint64_t *)m_peek ((x), (i)))
#define CHAR_UNCHECKED(x, i) (*(char *)m_peek ((x), (i)))
#define UCHAR_UNCHECKED(x, i) (*(unsigned char *)m_peek ((x), (i)))
#define STR_UNCHECKED(x, i) (*(char **)m_peek ((x), (i)))

/* Safe variants — report externally handleable errors (bounds, OOM,
   overflow) via 0/NULL and mls_errno instead of aborting. UAF and invalid
   handles (m > 0) are programmer errors and still exit(). */
#define INT_SAFE(x, i)                                                         \
	({                                                                     \
		void *_p = mls_safe ((x), (i));                                \
		_p ? *(int *)_p : 0;                                           \
	})
#define UINT_SAFE(x, i)                                                        \
	({                                                                     \
		void *_p = mls_safe ((x), (i));                                \
		_p ? *(unsigned int *)_p : 0;                                  \
	})
#define FLOAT_SAFE(x, i)                                                       \
	({                                                                     \
		void *_p = mls_safe ((x), (i));                                \
		_p ? *(float *)_p : 0.0f;                                      \
	})
#define DOUBLE_SAFE(x, i)                                                      \
	({                                                                     \
		void *_p = mls_safe ((x), (i));                                \
		_p ? *(double *)_p : 0.0;                                      \
	})
#define PTR_SAFE(x, i)                                                         \
	({                                                                     \
		void *_p = mls_safe ((x), (i));                                \
		_p ? *(void **)_p : NULL;                                      \
	})
#define U32_SAFE(x, i)                                                         \
	({                                                                     \
		void *_p = mls_safe ((x), (i));                                \
		_p ? *(uint32_t *)_p : 0;                                      \
	})
#define U64_SAFE(x, i)                                                         \
	({                                                                     \
		void *_p = mls_safe ((x), (i));                                \
		_p ? *(uint64_t *)_p : 0;                                      \
	})
#define CHAR_SAFE(x, i)                                                        \
	({                                                                     \
		void *_p = mls_safe ((x), (i));                                \
		_p ? *(char *)_p : 0;                                          \
	})
#define UCHAR_SAFE(x, i)                                                       \
	({                                                                     \
		void *_p = mls_safe ((x), (i));                                \
		_p ? *(unsigned char *)_p : 0;                                 \
	})
#define STR_SAFE(x, i)                                                         \
	({                                                                     \
		void *_p = mls_safe ((x), (i));                                \
		_p ? *(char **)_p : NULL;                                      \
	})

/* Run a _safe() expression and handle its error, which it reports through
   mls_errno. Both reset mls_errno first, because safe calls only set it,
   never clear it. Use these instead of the raw _safe call unless you need
   the call's own return value (e.g. mls_safe() results).

   mls_try(expr)  -> returns MLS_OK (0) or the error code; error stays
		     stored in mls_errno/mls_errfunc for later reporting.
   mls_must(expr) -> reports the error with location info and exits(1). */
#define mls_try(expr) (mls_errno = MLS_OK, (expr), mls_errno)

#define mls_must(expr)                                                         \
	do {                                                                   \
		mls_errno = MLS_OK;                                            \
		(expr);                                                        \
		if (mls_errno != MLS_OK)                                       \
			_mls_die (__LINE__, __FILE__, __FUNCTION__);           \
	} while (0)

void _mls_die (int line, const char *file, const char *function)
	__attribute__ ((noreturn));

/* One pair for every _safe() function:
   X_try(...)   real function, returns MLS_OK (0) or the mls_errno code;
		the error details stay stored for later reporting.
   X_must(...)  reports the error with YOUR file:line and exits(1);
		a macro because that is what captures the call site.
   Element access: mls_safe() itself is the try form (returns NULL and
   sets mls_errno); m_at_must() is the matching must form. */
static inline int m_put_try (int m, const void *data)
{
	return mls_try (m_put_safe (m, data));
}
static inline int m_setlen_try (int m, size_t len)
{
	return mls_try (m_setlen_safe (m, len));
}
static inline int m_write_try (int m, size_t p, const void *data, size_t n)
{
	return mls_try (m_write_safe (m, p, data, n));
}
static inline int m_read_try (int h, size_t p, void **data, size_t n)
{
	return mls_try (m_read_safe (h, p, data, n));
}
static inline int m_del_try (int m, size_t p)
{
	return mls_try (m_del_safe (m, p));
}

#define m_put_must(m, d) mls_must (m_put_safe (m, d))
#define m_setlen_must(m, len) mls_must (m_setlen_safe (m, len))
#define m_write_must(m, p, data, n) mls_must (m_write_safe (m, p, data, n))
#define m_read_must(h, p, data, n) mls_must (m_read_safe (h, p, data, n))
#define m_del_must(m, p) mls_must (m_del_safe (m, p))
#define m_at_must(m, i)                                                        \
	({                                                                     \
		void *_p = mls_safe ((m), (i));                                \
		if (!_p)                                                       \
			_mls_die (__LINE__, __FILE__, __FUNCTION__);           \
		_p;                                                            \
	})

#define m_cat(h, s) m_write (h, m_len (h), (s), strlen ((s)))
#define MSTR(x) ((char *)mls (x, 0))

typedef char utf8_char_t[6];
void m_bzero (int m);
void m_skip (int m, int n);
int m_fscan2 (int m, char delim, FILE *fp);
int m_fscan (int m, char delim, FILE *fp);
int m_cmp (int a, int b);
int m_lookup (int m, int key);
int m_lookup_obj (int m, void *obj, int size);
int utf8_getchar (FILE *fp, utf8_char_t buf);
int m_putc (int m, char c);
int m_puti (int m, int c);
int m_lookup_str (int m, const char *key, int NOT_INSERT);
int utf8char (char **s);
int m_utf8char (int buf, int *p);
int cmp_int (const void *a0, const void *b0);
int m_blookup_int (int buf, int key, void (*new) (void *, void *), void *ctx);
void *m_blookup_int_p (int buf, int key, void (*new) (void *, void *),
		       void *ctx);
int m_binsert_int (int buf, int key);
int m_bsearch_int (int buf, int key);

/* handle immuteable zero copy array */
int s_ccstr (const char *s);
int s_cstrdup (const char *s);
int m_wrapstrings (char **list, int nelem);
int m_wrapints (int *list, int nelem);
int m_wrapcstr (char *s);

#ifdef __plusplus
}
#endif

#endif

#if defined(MLS_DEBUG) && !defined(MLS_DEBUG_DISABLE)
#define m_init() _m_init ()
#define m_destruct() _m_destruct ()
#define mls(m, i) _mls (__LINE__, __FILE__, __FUNCTION__, (m), (i))
#define m_create(n, w) _m_create (__LINE__, __FILE__, __FUNCTION__, (n), (w))
#define m_alloc(n, w, h)                                                       \
	_m_alloc (__LINE__, __FILE__, __FUNCTION__, (n), (w), (h))
#define m_free(m) _m_free (__LINE__, __FILE__, __FUNCTION__, (m))
#define m_buf(m) _m_buf (__LINE__, __FILE__, __FUNCTION__, (m))
#define m_put(m, d) _m_put (__LINE__, __FILE__, __FUNCTION__, (m), (d))
#define m_next(m, i, d)                                                        \
	_m_next (__LINE__, __FILE__, __FUNCTION__, (m), (i), (d))
#define m_clear(m) _m_clear (__LINE__, __FILE__, __FUNCTION__, (m))

#define m_wrapcstr(s) _m_wrapcstr (__LINE__, __FILE__, __FUNCTION__, (s))

#define m_wrapints(s, n)                                                       \
	_m_wrapints (__LINE__, __FILE__, __FUNCTION__, (s), (n))

#define m_wrapstrings(s, n)                                                    \
	_m_wrapstrings (__LINE__, __FILE__, __FUNCTION__, (s), (n))

#define s_cstrdup(s) _s_cstrdup (__LINE__, __FILE__, __FUNCTION__, (s))

#define s_ccstr(s) _s_ccstr (__LINE__, __FILE__, __FUNCTION__, (s))

#endif
