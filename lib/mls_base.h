#ifndef MLS_BASE_H
#define MLS_BASE_H

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

/* Default MLS_THREAD_SAFE to 1 on Unix/POSIX platforms where pthreads is
   universally available. Override with -DMLS_THREAD_SAFE=0 to disable.
   Always defined (0 or 1) so value-based `#if MLS_THREAD_SAFE` works
   everywhere, including this umbrella header's consumers. */
#ifndef MLS_THREAD_SAFE
#if defined(__unix__) || defined(__linux__) || defined(__APPLE__) ||           \
	defined(__FreeBSD__)
#define MLS_THREAD_SAFE 1
#else
#define MLS_THREAD_SAFE 0
#endif
#endif

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

/* minimal init/destruct: master list + free list only */
int m_base_init (void);
void m_base_destruct (void);

int m_alloc (size_t max, size_t w, uint8_t hfree);
int m_alloc_safe (size_t max, size_t w, uint8_t hfree);
int m_create (size_t max, size_t w);
int m_create_safe (size_t max, size_t w);
int m_free (int m);

size_t m_len (int m);
void *m_buf (int m);

#define CHARP(m) ((char *)m_buf (m))

void *mls (int m, size_t i);
void *mls_safe (int m, size_t i);
int m_next (int m, int *p, void *d);

int m_put (int m, const void *data);
int m_put_safe (int m, const void *data);
int m_setlen (int m, size_t len);
int m_setlen_safe (int m, size_t len);
int m_write (int m, size_t p, const void *data, size_t n);
int m_write_safe (int m, size_t p, const void *data, size_t n);
size_t m_width (int m);
void m_resize (int m, size_t new_size);
int m_resize_safe (int m, size_t new_size);
void *m_peek (int m, size_t i);

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

static inline int m_put_try (int m, const void *data)
{
	return mls_try (m_put_safe (m, data));
}
static inline int m_setlen_try (int m, size_t len)
{
	return mls_try (m_setlen_safe (m, len));
}

#define m_put_must(m, d) mls_must (m_put_safe (m, d))
#define m_setlen_must(m, len) mls_must (m_setlen_safe (m, len))
#define m_at_must(m, i)                                                        \
	({                                                                     \
		void *_p = mls_safe ((m), (i));                                \
		if (!_p)                                                       \
			_mls_die (__LINE__, __FILE__, __FUNCTION__);           \
		_p;                                                            \
	})

#define MSTR(x) ((char *)mls (x, 0))

#ifdef __plusplus
}
#endif

#endif
