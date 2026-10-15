#ifndef M_VAR_H
#define M_VAR_H

/*
 * m_var - standalone variable system + string expansion.
 *
 * This is the isolated successor of the v_* / se_* system that still lives in
 * m_tool.[ch]. It is deliberately self-contained on top of the mls core: it
 * does NOT include or call table.c. The vs_ prefix lets both systems coexist
 * (no linker clashes) until this one is integrated and the old one drops out.
 *
 * A "variable system" (vs) is an m-array of variable handles. A variable is an
 * m-array of strings whose element 0 is the name and elements 1..n are the
 * values (one per row). String expansion interpolates those values into a
 * format string: $name, $name[N], $name[*] and quoted $'name'.
 */

#include "mls.h"
#include <stdarg.h>
#include <stdio.h>

/* Position argument for vs_set/vs_kset. */
enum { VS_APPEND = -1, VS_RENAME = 0, VS_SET = 1 };

/* ---- Variable system -------------------------------------------------- */

int vs_init (void);    /* new variable system */
void vs_free (int vs); /* == m_free(vs): MFREE_EACH/STR free recursively,
			  so a `tmls` handle auto-frees the whole system */

int vs_find_key (int vs, const char *name); /* index or -1 */
int vs_lookup (int vs, const char *name);   /* find or create variable */

void vs_kset (int var, const char *value, int pos);
int vs_set (int vs, const char *name, const char *value, int pos);
void vs_vaset (int vs, ...); /* (name, value) ..., NULL */

void vs_kclr (int var); /* drop values, keep name */
void vs_clr (int vs, const char *name);
void vs_remove (int vs, const char *name); /* drop variable */

char *vs_kget (int var, int pos); /* "" when absent */
char *vs_get (int vs, const char *name, int pos);
int vs_klen (int var); /* number of values */
int vs_len (int vs, const char *name);

/* ---- String expansion ------------------------------------------------- */

typedef struct vs_exp_st {
	int splitbuf; /* alternating literal / $-token strings */
	int max_row;
	int values;  /* handles/pointers of the $-tokens in splitbuf */
	int indices; /* parsed index per $-token */
	int buf;     /* result buffer */
	int sql;     /* 0 = C-ish backslash quoting, 1 = SQL quoting */
} vs_exp_t;

void vs_exp_init (vs_exp_t *se);
void vs_exp_free (vs_exp_t *se);
void vs_exp_parse (vs_exp_t *se, const char *frm);
char *vs_expand (vs_exp_t *se, int vs, int row);
char *vs_string (int vs, const char *frm); /* parse+expand+store in one call */

/* Like vs_string(), but $'name' quotes the SQL way: single quotes are doubled
 * ('' ) instead of backslash-escaped. This is what both MySQL and SQLite
 * accept, so the result is a valid SQL string literal / list. Backslashes are
 * left untouched (SQLite has no backslash escapes; MySQL in
 * NO_BACKSLASH_ESCAPES mode agrees). Set se.sql = 1 when using the raw
 * vs_exp_* API directly. */
char *vs_sql_string (int vs, const char *frm);

/* Build helpers for the csv2insert style usage. Both reuse the $-syntax
 * above, they only differ in where the input values / result live.
 *
 *   int vs = vs_add(0, "cols", cols, n);   // list of C-strings
 *   int h  = vs_string_builder(0, vs, "insert into t ( $'cols' ) ...");
 *
 * A quoted bare variable ($'name', no [index]) expands to a quoted,
 * comma-separated list of all its values. */

/* Add n C-strings as values of variable `name` (appends to existing values).
 * Creates a variable system when vs <= 0. Returns the system handle. */
int vs_add (int vs, const char *name, const char *const *list, int n);

/* Expand frm and append the result to string handle strh (allocated when
 * strh <= 0). Returns strh. */
int vs_string_builder (int strh, int vs, const char *frm);

/* vs_string_builder() with SQL quoting (see vs_sql_string). */
int vs_sql_string_builder (int strh, int vs, const char *frm);

/* Read one line from fp, split it on sep and store the trimmed fields as the
 * values of variable `name` (previous values are dropped). vs must already be
 * initialized. Returns the variable handle; at EOF it has 0 values, which is
 * the loop end condition (analogous to an empty s_read_fields). */
int vs_read_fields (int vs, const char *name, FILE *fp, const char *sep);

#endif
