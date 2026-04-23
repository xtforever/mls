#ifndef SD_STRINGS_H
#define SD_STRINGS_H

#include "sd.h"
#include <regex.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SD_SENTINEL __attribute__ ((__sentinel__ (0)))

int sd_new (void);
void sd_sfree (int h);
int sd_strlen (int h);
int sd_app1 (int h, const char *s);
int sd_app (int h, ...) SD_SENTINEL;
int sd_strdup (const char *s);
int sd_clone (int h);
int sd_strresize (int h, int len);
void sd_sclear (int h);
int sd_printf (int h, int pos, const char *format, ...)
	__attribute__ ((format (printf, 3, 4)));
int sd_cat (int h, const char *src);

int sd_cmp (int a, int b);
int sd_ncmp (int a, int b, int n);
int sd_chr (int h, int c, int off);
int sd_rchr (int h, int c);
int sd_find (int h, const char *sub);
int sd_has_prefix (int h, const char *prefix);
int sd_has_suffix (int h, const char *suffix);
int sd_subcmp (int s0, int s0a, int s0b, int s1, int s1a, int s1b);
int sd_casecmp (int a, int b);
int sd_ncasecmp (int a, int b, int n);

int sd_sub (int h, int pos, int len);
int sd_left (int h, int n);
int sd_right (int h, int n);
int sd_copy (int m, int first, int last);
int sd_slice (int dest, int offs, int m, int a, int b);

int sd_trim (int h);
int sd_trim_c (int h, const char *chars);
int sd_trim_left_c (int h, const char *chars);
int sd_trim_right_c (int h, const char *chars);
int sd_lower (int h);
int sd_upper (int h);
int sd_reverse (int h);
int sd_replace_c (int h, const char *old, const char *replacement);
int sd_replace (int dest, int src, int pattern, int replace, int count);
int sd_pad_left (int h, int width, char pad);
int sd_pad_right (int h, int width, char pad);

int sd_split (int m, const char *s, int c, int remove_wspace);
int sd_msplit (int dest, int src, int pattern);
int sd_implode (int dest, int srcs, int separator);
int sd_join (const char *sep, ...) SD_SENTINEL;

int sd_from_int (int val);
int sd_to_int (int h);
int sd_from_long (long val);
long sd_to_long (int h);
int sd_from_ulong (unsigned long val);
unsigned long sd_to_ulong (int h);
int sd_from_double (double val);
double sd_to_double (int h);
int sd_from_float (float val);
float sd_to_float (int h);
int sd_from_bool (int val);
int sd_to_bool (int h);
int sd_from_hex (int val);
int sd_to_hex (int h);

uint32_t sd_hash (int h);
int sd_is_numeric (int h);
int sd_is_alpha (int h);
int sd_is_empty (int h);

int sd_regex (int m, const char *regex, const char *s);
int sd_strstr (int m, int offs, int pattern);
int sd_secure_cmp (int a, int b);
int sd_base64_decode (int h);

int sd_dup (int h);
void sd_qsort (int list, int (*compar) (const void *, const void *));
int sd_bsearch (const void *key, int list,
		int (*compar) (const void *, const void *));
int sd_lfind (const void *key, int list,
	      int (*compar) (const void *, const void *));
int sd_binsert (int buf, const void *data,
		int (*cmpf) (const void *, const void *), int with_duplicates);

int sd_iread (int fd, int buffer);
int sd_fscan (int m, char delim, FILE *fp);
int sd_fscan2 (int m, char delim, FILE *fp);

int sd_lookup_obj (int m, void *obj, int size);
int sd_lookup_str (int m, const char *key, int NOT_INSERT);
int sd_blookup_int (int buf, int key, void (*new_fn) (void *, void *),
		    void *ctx);
void *sd_blookup_int_p (int buf, int key, void (*new_fn) (void *, void *),
			void *ctx);
int sd_binsert_int (int buf, int key);
int sd_bsearch_int (int buf, int key);

int sd_utf8char (int buf, int *p);
int utf8char (char **s);
int utf8_getchar (FILE *fp, char buf[6]);

void sd_free_strings (int list, int clear_only);
void sd_free_list (int m);
void sd_clear_list (int m);
void sd_free_stringlist (int m);
void sd_clear_stringlist (int m);

void sd_register_printf (void);

#ifdef __cplusplus
}
#endif

#endif
