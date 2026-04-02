#ifndef M_TOOL_H
#define M_TOOL_H
/* experimental tools */

#include "mls.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

/* how man strings before forced exit(122) to prevent OOM Killer */
#define CONSTSTR_MAX 9000

int s_new (void);
void s_free (int h);
int s_dup (const char *s);
int s_clone (int h);
int s_resize (int h, int len);
void s_clear (int h);
int s_has_prefix (int h, const char *prefix);
int s_has_suffix (int h, const char *suffix);
int s_from_long (long val);
uint32_t s_hash (int h);
int s_join (const char *sep, ...) __attribute__ ((__sentinel__ (0)));
int s_cmp (int a, int b);
int s_ncmp (int a, int b, int n);
int s_chr (int h, int c, int off);
int s_rchr (int h, int c);
int s_find (int h, const char *sub);
int s_spn (int h, const char *accept);
int s_cspn (int h, const char *reject);
int s_cat (int h, const char *src);
int s_ncat (int h, const char *src, int n);
int s_sub (int h, int pos, int len);
int s_left (int h, int n);
int s_right (int h, int n);
int s_replace_c (int h, const char *old, const char *replacement);
int s_trim_c (int h, const char *chars);
void m_register_printf (void);

/* m string (ms) : m-array of (char*) */
int m_str_va_app (int ms, va_list ap);
int m_str_app (int ms, ...);
int m_str_split (int ms, char *s, char *delim, int trimws);

int s_strcmp_c (int mstr, const char *s);
int m_strncpy (int dst, int src, int max);
int m_mcopy (int dest, int destp, int src, int srcp, int src_count);
int m_binsert (int buf, const void *data,
	       int (*cmpf) (const void *a, const void *b), int with_duplicates);
int compare_int (const void *a, const void *b);
int cmp_mstr_fast (const void *a, const void *b);
int cmp_mstr_cstr_fast (const void *a, const void *b);
int m_str_from_file (char *filename);

void m_free_ptr (void *d);
void m_free_user (int m, void (*free_h) (void *), int only_clear);
void m_clear_user (int m, void (*free_h) (void *));
void m_free_list (int m);
void m_clear_list (int m);
void m_clear_stringlist (int m);
void m_free_stringlist (int m);

void m_concat (int a, int b);
int m_split_list (const char *s, const char *delm);

int leftstr (int buf, int p, const char *s, int ch);
int cmp_mstr (const void *a, const void *b);
int lookup_int (int m, int key);
int s_slice (int dest, int offs, int m, int a, int b);
int s_replace (int dest, int src, int pattern, int replace, int count);
void s_puts (int m);
int s_strstr (int m, int offs, int pattern);
int s_strncmp (int m, int offs, int pattern, int n);
void s_write (int m, int n);
int s_isempty (int m);
int s_strdup_c (const char *s);
int s_strcpy_c (int out, const char *s);
int s_trim (int m);
int s_lower (int m);
int s_upper (int m);
int s_msplit (int dest, int src, int pattern);
int s_implode (int dest, int srcs, int seperator);

void m_map (int m, int (*fn) (int m, int p, void *ctx), void *ctx);
void m_memset (int ln, char c, int w);
int s_strncmp2 (int s0, int p0, int s1, int p1, int len);
int s_strncmpr (int str, int suffix);

int s_readln (int buf, FILE *fp);
int s_regex (int res, char *regex, int buf);
bool glob_match (char const *pat, char const *str, const char **a,
		 const char **b) __attribute__ ((const));

/* imported from conststr */
void conststr_stats (void);
void conststr_init (void);
void conststr_free (void);
int conststr_lookup_c (const char *s);
int conststr_lookup (int s);

/*
  s_cstr - returns a handle to a constant string equal to the given c-style
  string s_mstr - returns a handle to a constant string equal to the given array
  cs_printf - creates a string and returns a handle to a constant string equal
  to the created one
*/
int cs_printf (const char *format, ...);
inline static int s_cstr (const char *s) { return conststr_lookup_c (s); }
inline static int s_mstr (int m) { return conststr_lookup (m); }

/* Relocated from mls.h */
int m_dub (int m);
int m_regex (int m, const char *regex, const char *s);
void m_qsort (int list, int (*compar) (const void *, const void *));
int m_bsearch (const void *key, int list,
	       int (*compar) (const void *, const void *));
int m_lfind (const void *key, int list, int (*compar) (const void *, const void *));
int ioread_all (int fd, int buffer);

/* String Core Moved from mls.h */
int s_strlen (int m);
int s_app (int m, ...) __attribute__ ((__sentinel__ (0)));
int s_app1 (int m, char *s);
int vas_printf (int m, int p, const char *format, va_list argptr);
int s_printf (int m, int p, char *format, ...);
int s_lastchar (int m);
int s_copy (int m, int first_char, int last_char);
int s_index (int buf, int p, int ch);
int s_split (int m, const char *s, int c, int remove_wspace);

/* Variable System Moved from mls.h */
#define VAR_NAME(x) STR (x, 0)
int v_init (void);
void v_free (int vs);
int v_set (int vs, const char *name, const char *value, int pos);
void v_vaset (int vs, ...);
void v_clr (int vs, const char *name);
char *v_get (int vs, const char *name, int pos);
int v_len (int vs, const char *name);
void v_remove (int vs, const char *name);
int v_lookup (int vs, const char *name);
int v_find_key (int vs, const char *name);
void v_kset (int key, const char *value, int pos);
void v_kclr (int key);
char *v_kget (int key, int pos);
int v_klen (int key);

/* String Expansion System Moved from mls.h */
typedef struct str_exp_st {
	int splitbuf;
	int max_row;
	int values;
	int indices;
	int buf;
} str_exp_t;

void se_init (str_exp_t *se);
void se_free (str_exp_t *se);
void se_realloc_buffers (str_exp_t *se);
void se_parse (str_exp_t *se, const char *frm);
char *se_expand (str_exp_t *se, int vl, int row);
char *se_string (int vl, const char *frm);

int escape_str (int buf, char *src);
void escape_buf (int buf, char *src);

enum { VAR_APPEND = -1, VAR_RENAME = 0, VAR_SET = 1 };


void ring_free (int r);
int ring_get (int r);
int ring_put (int r, int data);
int ring_full (int r);
int ring_empty (int r);
int ring_create (int size);

void m_free_strings (int list, int CLEAR_ONLY);


#endif
