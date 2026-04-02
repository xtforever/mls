#ifndef MLS_H
#define MLS_H

#ifdef __plusplus
extern "C" {
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

#define increase_by_percent(a, p) calc_percent (a, p + 100)
#define calc_percent(a, p) (((p) > 0) ? (a) * (p) / 100 : 0)

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
		if ((l) >= trace_level && trace_level != 0)                    \
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

typedef struct ls_st {
	int w, l, max;
	char uaf_protection;
	uint8_t free_hdl;
	char d[0] __attribute__ ((aligned (8)));
} *lst_t;

void *lst (lst_t l, int i) __attribute__ ((pure));
lst_t lst_create (int max, int w);
int lst_new (lst_t *LP, int n);
int lst_put (lst_t *LP, const void *d);
int lst_next (lst_t l, int *p, void *data);
int lst_read (lst_t l, int p, void **data, int n);
int lst_write (lst_t *lp, int p, const void *data, int n);
void *lst_peek (lst_t l, int i);
void lst_del (lst_t l, int p);
void *lst_ins (lst_t *lp, int p, int n);
void lst_resize (lst_t *LP, int new_size);

enum predefined_free_handler {
	MFREE = 0,
	MFREE_STR = 1,
	MFREE_EACH = 2,
	MFREE_MAX = 2
};

int m_alloc (int max, int w, uint8_t hfree);
int m_free (int m);
int m_reg_freefn (int n, void (*free_fn) (lst_t l));
int m_is_freed (int h);
int m_free_hdl (int h);

int m_len (int m);
void *m_buf (int m);

#define CHARP(m) ((char *)m_buf (m))

void *mls (int m, int i);
int m_new (int m, int n);
void *m_add (int m);
int m_next (int m, int *p, void *d);
int m_init ();
void m_destruct ();
int m_create (int max, int w);
int m_put (int m, const void *data);
int m_setlen (int m, int len);
int m_bufsize (int m);
void *m_peek (int m, int i);
int m_write (int m, int p, const void *data, int n);
int m_read (int h, int p, void **data, int n);
void m_clear (int m);
void m_del (int m, int p);
void *m_pop (int m);
int m_ins (int m, int p, int n);
int m_width (int m);
void m_resize (int m, int new_size);
int m_slice (int dest, int offs, int m, int a, int b);
void m_remove (int m, int p, int n);
static inline char *m_str (int m) { return !m_is_freed(m) && m_len (m) ? (char *)m_buf (m) : ""; };

int _m_init ();
void _m_destruct ();
int _m_create (int ln, const char *fn, const char *fun, int n, int w);
int _m_free (int ln, const char *fn, const char *fun, int m);
void *_mls (int ln, const char *fn, const char *fun, int h, int i);
int _m_put (int ln, const char *fn, const char *fun, int h, const void *d);
int _m_next (int ln, const char *fn, const char *fun, int h, int *i, void *d);
void _m_clear (int ln, const char *fn, const char *fun, int h);
void *_m_buf (int ln, const char *fn, const char *fun, int m);
int _m_alloc (int ln, const char *fn, const char *fun, int n, int w,
	      uint8_t hfree);

#define m_foreach(lst, index, ptr) for (index = -1; m_next (lst, &index, &ptr);)
#define STR(x, i) (*(char **)mls ((x), (i)))
#define INT(x, i) (*(int *)mls ((x), (i)))
#define UINT(x, i) (*(unsigned int *)mls ((x), (i)))
#define CHAR(x, i) (*(char *)mls ((x), (i)))
#define UCHAR(x, i) (*(unsigned char *)mls ((x), (i)))
#define m_cat(h, s) m_write (h, m_len (h), (s), strlen ((s)))
#define MSTR(x) ((char *)mls (x, 0))

void ring_free (int r);
int ring_get (int r);
int ring_put (int r, int data);
int ring_full (int r);
int ring_empty (int r);
int ring_create (int size);

typedef char utf8_char_t[6];
void m_bzero (int m);
void m_free_strings (int list, int CLEAR_ONLY);
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
int mstrcmp (int m, int p, const char *s);
int cmp_int (const void *a0, const void *b0);
int m_blookup_int (int buf, int key, void (*new) (void *, void *), void *ctx);
void *m_blookup_int_p (int buf, int key, void (*new) (void *, void *),
		       void *ctx);
int m_binsert_int (int buf, int key);
int m_bsearch_int (int buf, int key);

lst_t *exported_get_list (int r);

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
#endif
