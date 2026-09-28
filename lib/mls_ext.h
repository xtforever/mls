#ifndef MLS_EXT_H
#define MLS_EXT_H

#include "mls_base.h"

#ifdef __plusplus
extern "C" {
#endif

int m_init ();
void m_destruct ();

/* the function free_fn will be called if m_free(this array) is called
   the free_fn can now iterate and clean all elements of the array,
   before this array is removed.
   the returned value is the handle id that you supply to m_alloc.
*/
int m_reg_freefn (free_fn_t free_fn);

int m_is_freed (int h);
int m_is_valid (int h);
int m_free_hdl (int h);
int m_dub (int m);

int m_set_data (int m, size_t len, size_t w, const void *data);
int m_new (int m, size_t n);
int m_new_safe (int m, size_t n);
void *m_add (int m);
void *m_add_safe (int m);

size_t m_count_allocated (void);
size_t m_total_bytes (void);
size_t m_peak_handles (void);
void m_debug_print (FILE *fp);

size_t m_bufsize (int m);
int m_read (int h, size_t p, void **data, size_t n);
int m_read_safe (int h, size_t p, void **data, size_t n);
void m_clear (int m);
void m_del (int m, size_t p);
int m_del_safe (int m, size_t p);
void *m_pop (int m);
int m_ins (int m, size_t p, size_t n);
int m_ins_safe (int m, size_t p, size_t n);
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
int _m_alloc (int ln, const char *fn, const char *fun, size_t n, size_t w,
	      uint8_t hfree);
int _m_free (int ln, const char *fn, const char *fun, int m);
void *_mls (int ln, const char *fn, const char *fun, int h, size_t i);
int _m_put (int ln, const char *fn, const char *fun, int h, const void *d);
int _m_next (int ln, const char *fn, const char *fun, int h, int *i, void *d);
void _m_clear (int ln, const char *fn, const char *fun, int h);
void *_m_buf (int ln, const char *fn, const char *fun, int m);
int _m_wrapcstr (int ln, const char *fn, const char *fun, char *s);
int _m_wrapints (int ln, const char *fn, const char *fun, int *list, int nelem);
int _m_wrapstrings (int ln, const char *fn, const char *fun, char **list,
		    int nelem);
int _m_wrapstrhandles (int ln, const char *fn, const char *fun,
		       const char **list, int nelem);
int _s_cstrdup (int ln, const char *fn, const char *fun, const char *s);
int _s_ccstr (int ln, const char *fn, const char *fun, const char *s);

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

#define m_write_must(m, p, data, n) mls_must (m_write_safe (m, p, data, n))
#define m_read_must(h, p, data, n) mls_must (m_read_safe (h, p, data, n))
#define m_del_must(m, p) mls_must (m_del_safe (m, p))

#define m_cat(h, s) m_write (h, m_len (h), (s), strlen ((s)))

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
int m_wrapstrhandles (const char **list, int nelem);

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

#define m_wrapstrhandles(s, n)                                                 \
	_m_wrapstrhandles (__LINE__, __FILE__, __FUNCTION__, (s), (n))

#define s_cstrdup(s) _s_cstrdup (__LINE__, __FILE__, __FUNCTION__, (s))

#define s_ccstr(s) _s_ccstr (__LINE__, __FILE__, __FUNCTION__, (s))

#endif
