#include "m_var.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * m_var - see m_var.h. No dependency on table.c or m_tool.c: only the mls core
 * is used. Free handlers do the recursive work:
 *   - a variable is MFREE_STR, so m_free(var) frees the strdup'd values;
 *   - the system is MFREE_EACH, so m_free(vs) frees every variable.
 */

/* ---- Variable system -------------------------------------------------- */

/**
 * Creates a new, empty variable system.
 *
 * @return The handle of the new variable system.
 */
int vs_init (void) { return m_alloc (100, sizeof (int), MFREE_EACH); }

/**
 * Frees a variable system and all its variables.
 *
 * Equivalent to m_free(vs): the MFREE_EACH / MFREE_STR handlers free the
 * variables and their values recursively, so a `tmls` handle cleans up the
 * whole system at block end.
 *
 * @param vs The variable system handle.
 */
void vs_free (int vs) { m_free (vs); }

/**
 * Finds the index of a variable by name.
 *
 * @param vs The variable system handle.
 * @param name The variable name.
 * @return The index of the variable, or -1 if absent.
 */
int vs_find_key (int vs, const char *name)
{
	char *s;
	int p, *d;
	if (vs <= 0 || !name)
		return -1;
	m_foreach (vs, p, d)
	{
		s = STR (*d, 0);
		if (s && strcmp (s, name) == 0)
			return p;
	}
	return -1;
}

/**
 * Looks up a variable by name, creating an empty one if needed.
 *
 * @param vs The variable system handle.
 * @param name The variable name (must not be empty).
 * @return The variable handle, or -1 on an empty name.
 */
int vs_lookup (int vs, const char *name)
{
	if (is_empty (name))
		return -1;
	int p = vs_find_key (vs, name);
	if (p >= 0)
		return INT (vs, p);

	int var = m_alloc (2, sizeof (char *), MFREE_STR);
	char *s = strdup (name);
	m_put (var, &s);
	m_put (vs, &var);
	return var;
}

/**
 * Sets a value at a position in a variable.
 *
 * Element 0 holds the name; values live at positions 1..n. A negative or
 * out-of-range pos appends the value.
 *
 * @param var The variable handle.
 * @param value The new value (copied; NULL stores NULL).
 * @param pos The value position (use VS_APPEND to append).
 */
void vs_kset (int var, const char *value, int pos)
{
	if (var <= 0)
		return;
	char *val = value ? strdup (value) : NULL;
	if (pos < 0 || pos >= (int)m_len (var)) {
		m_put (var, &val);
	} else {
		char **d = (char **)mls (var, pos);
		free (*d);
		*d = val;
	}
}

/**
 * Sets a value by variable name, creating the variable if needed.
 *
 * @param vs The variable system handle.
 * @param name The variable name.
 * @param value The new value.
 * @param pos The value position (use VS_APPEND to append).
 * @return The handle of the variable.
 */
int vs_set (int vs, const char *name, const char *value, int pos)
{
	int key = vs_lookup (vs, name);
	vs_kset (key, value, pos);
	return key;
}

/**
 * Sets multiple variables from (name, value) pairs, terminated by NULL.
 *
 * @param vs The variable system handle.
 * @param ... Alternating name/value pairs, ending with a NULL name.
 */
void vs_vaset (int vs, ...)
{
	va_list ap;
	char *name, *value;
	va_start (ap, vs);
	while ((name = va_arg (ap, char *)) != NULL) {
		value = va_arg (ap, char *);
		vs_set (vs, name, value, VS_APPEND);
	}
	va_end (ap);
}

/**
 * Drops all values of a variable, keeping its name.
 *
 * @param var The variable handle.
 */
void vs_kclr (int var)
{
	int i = 0;
	char **d;
	if (var <= 0)
		return;
	/* start at 1: element 0 is the name and must survive */
	while (m_next (var, &i, &d)) {
		free (*d);
		*d = NULL;
	}
	m_setlen (var, 1);
}

/**
 * Drops all values of a variable selected by name.
 *
 * @param vs The variable system handle.
 * @param name The variable name.
 */
void vs_clr (int vs, const char *name) { vs_kclr (vs_lookup (vs, name)); }

/**
 * Gets a value from a variable at a position.
 *
 * @param var The variable handle.
 * @param pos The value position.
 * @return The value, or "" if absent.
 */
char *vs_kget (int var, int pos)
{
	if (var <= 0)
		return "";
	int len = (int)m_len (var);
	if (pos < 0 || pos >= len)
		return "";
	char *s = STR (var, pos);
	return s ? s : "";
}

/**
 * Gets a value by variable name.
 *
 * @param vs The variable system handle.
 * @param name The variable name.
 * @param pos The value position.
 * @return The value, or "" if absent.
 */
char *vs_get (int vs, const char *name, int pos)
{
	return vs_kget (vs_lookup (vs, name), pos);
}

/**
 * Returns the number of values in a variable (excluding its name).
 *
 * @param var The variable handle.
 * @return The number of values.
 */
int vs_klen (int var)
{
	int len = var > 0 ? (int)m_len (var) - 1 : 0;
	return len > 0 ? len : 0;
}

/**
 * Returns the number of values of a variable selected by name.
 *
 * @param vs The variable system handle.
 * @param name The variable name.
 * @return The number of values.
 */
int vs_len (int vs, const char *name) { return vs_klen (vs_lookup (vs, name)); }

/**
 * Removes a variable and frees its values.
 *
 * @param vs The variable system handle.
 * @param name The variable name.
 */
void vs_remove (int vs, const char *name)
{
	int pos = vs_find_key (vs, name);
	if (pos >= 0) {
		m_free (INT (vs, pos)); /* frees the values */
		m_del (vs, pos);
	}
}

/* ---- String expansion ------------------------------------------------- */

/**
 * Initializes an expansion context. Every vs_exp_t must start here.
 *
 * @param se The expansion context.
 */
void vs_exp_init (vs_exp_t *se) { memset (se, 0, sizeof *se); }

/**
 * Frees the buffers of an expansion context.
 *
 * @param se The expansion context.
 */
void vs_exp_free (vs_exp_t *se)
{
	m_free (se->splitbuf); /* MFREE_STR frees the token strings */
	m_free (se->values);
	m_free (se->indices);
	m_free (se->buf);
	memset (se, 0, sizeof *se);
}

/* (Re)fills the token buffers, dropping any previous parse. */
static void vs_exp_realloc (vs_exp_t *se)
{
	if (se->splitbuf) {
		m_free (se->splitbuf);
		se->splitbuf = 0;
	}
	if (!se->values)
		se->values = m_create (10, sizeof (char *));
	if (!se->indices)
		se->indices = m_create (10, sizeof (int));
	if (!se->buf)
		se->buf = m_create (100, 1);

	se->splitbuf = m_alloc (10, sizeof (char *), MFREE_STR);
	m_clear (se->values);
	m_clear (se->indices);
	m_clear (se->buf);
	se->max_row = 0;
}

/*
 * Parses a bracketed index at *s: [*] -> 1, [n] -> n+2, none -> 0.
 * The +2 for [n] keeps 0 free as "no bracket, use row".
 */
static int parse_index (const char **s)
{
	int val;
	const char *p = *s;
	if (*p != '[')
		return 0;
	p++;
	if (*p == '*' && p[1] == ']') {
		*s = p + 2;
		return 1;
	}
	val = 0;
	while (isdigit ((unsigned char)*p)) {
		val = val * 10 + (*p - '0');
		p++;
	}
	if (*p == ']') {
		*s = p + 1;
		return val + 2;
	}
	return 0;
}

/**
 * Parses a format string into literal and $-token parts.
 *
 * Supports $name, $name[N], $name[*], quoted $'name' and escaped \$:
 *   - $name        value of the current row (see vs_expand)
 *   - $name[N]     N-th value (0-based among the values)
 *   - $name[*]     every value joined with commas
 *   - $'name'      quoted; a bare quoted name joins every value quoted
 *   - \$           literal dollar sign
 *
 * @param se The expansion context.
 * @param frm The format string.
 */
void vs_exp_parse (vs_exp_t *se, const char *frm)
{
	if (!frm || !se)
		return;
	vs_exp_realloc (se);
	int b = se->splitbuf;
	int v = se->values;
	int idx = se->indices;
	char *cp;
	const char *s = frm, *s0 = frm;

	while (*s) {
		if (*s == '\\' && s[1] == '$') {
			if (s > s0) {
				cp = strndup (s0, (size_t)(s - s0));
				m_put (b, &cp);
			}
			cp = strdup ("$");
			m_put (b, &cp);
			s += 2;
			s0 = s;
			continue;
		}
		if (*s == '$') {
			if (s > s0) {
				cp = strndup (s0, (size_t)(s - s0));
				m_put (b, &cp);
			}
			s0 = s;
			s++;
			if (*s == '\'')
				s++;
			while (isalnum ((unsigned char)*s) || *s == '_')
				s++;

			cp = strndup (s0, (size_t)(s - s0));
			m_put (b, &cp);
			m_put (v, &cp); /* alias into splitbuf, used for
					   matching */

			int index = parse_index (&s);
			m_put (idx, &index);

			if (*s == '\'')
				s++;
			s0 = s;
			continue;
		}
		s++;
	}
	if (s > s0) {
		cp = strndup (s0, (size_t)(s - s0));
		m_put (b, &cp);
	}
}

/* Replaces characters that would break a quoted literal. */
static void repl_char (int buf, char ch)
{
	static const char tab[] = {'\\', '\0', '\n', '\r', '\'', '"', '\x1a'};
	static const char rep[] = {'\\', '0', 'n', 'r', '\'', '"', 'Z'};
	for (size_t i = 0; i < sizeof tab; i++)
		if (tab[i] == ch) {
			m_putc (buf, '\\');
			m_putc (buf, rep[i]);
			return;
		}
	m_putc (buf, ch);
}

static void expand_escaped (int buf, const char *s)
{
	while (*s)
		repl_char (buf, *s++);
}

/* Quote a field. sql mode doubles single quotes (''), which MySQL and SQLite
 * both accept; the default mode backslash-escapes (like s_implode_q's
 * sibling). */
static void field_escape (int buf, const char *s, int quotes, int sql)
{
	if (!quotes) {
		if (sql) {
			for (; *s; s++)
				m_putc (buf, *s);
		} else {
			expand_escaped (buf, s);
		}
		return;
	}
	m_putc (buf, '\'');
	if (sql) {
		for (; *s; s++) {
			m_putc (buf, *s);
			if (*s == '\'')
				m_putc (buf, '\'');
		}
	} else {
		expand_escaped (buf, s);
	}
	m_putc (buf, '\'');
}

/**
 * Expands a parsed format string using the values of a variable system.
 *
 * The result lives in se->buf and stays valid until the next expansion or
 * vs_exp_free. Set se.sql = 1 before parsing/expanding for SQL quoting.
 *
 * @param se The parsed expansion context.
 * @param vs The variable system handle.
 * @param row Row used for a $-token without an explicit index.
 * @return Pointer to the expanded string.
 */
char *vs_expand (vs_exp_t *se, int vs, int row)
{
	int var, index;
	int p, vn, i;
	char **d, *s, **val_ptr;
	int quotes = 0;

	m_clear (se->buf);
	int buf = se->buf;
	vn = 0;
	m_foreach (se->splitbuf, p, d)
	{
		s = *d;
		/* $-tokens are stored by pointer identity; anything else is
		 * a literal part of the format string. */
		if (vn < (int)m_len (se->values) &&
		    (val_ptr = (char **)mls (se->values, vn)) &&
		    *val_ptr == s) {
			int off = 1;
			if (s[1] == '\'') {
				quotes = 1;
				off = 2;
			} else {
				quotes = 0;
			}
			var = vs_lookup (vs, s + off);
			index = INT (se->indices, vn);
			vn++;

			if (index == 1 || (quotes && index == 0)) {
				/* [*], and a quoted bare $'name', join every
				 * value with commas (quoted per value) */
				if (vs_klen (var) > 0) {
					field_escape (buf, STR (var, 1), quotes,
						      se->sql);
					for (i = 2; i < (int)m_len (var); i++) {
						m_putc (buf, ',');
						field_escape (buf, STR (var, i),
							      quotes, se->sql);
					}
				}
			} else {
				/* 0 = no bracket -> current row; [n] -> n */
				if (index == 0)
					index = row;
				else
					index -= 2;
				if (index >= 0 && index < vs_klen (var))
					field_escape (buf, STR (var, index + 1),
						      quotes, se->sql);
			}
		} else {
			m_write (buf, m_len (buf), s, strlen (s));
		}
	}
	m_putc (buf, 0);
	return (char *)mls (buf, 0);
}

static char *vs_string_mode (int vs, const char *frm, int sql)
{
	vs_exp_t se;
	vs_exp_init (&se);
	se.sql = sql;
	vs_exp_parse (&se, frm);
	vs_expand (&se, vs, 0);

	/* Stash the result in the system so the pointer outlives se.buf. */
	int data = vs_set (vs, "vs_result", (char *)mls (se.buf, 0), 1);
	char *res = data > 0 ? STR (data, 1) : "";
	vs_exp_free (&se);
	return res;
}

/**
 * Parses and expands a format string in one call.
 *
 * The result is stored in the variable system (under "vs_result") so the
 * returned pointer outlives the internal buffer.
 *
 * @param vs The variable system handle.
 * @param frm The format string.
 * @return The expanded string.
 */
char *vs_string (int vs, const char *frm)
{
	return vs_string_mode (vs, frm, 0);
}

/**
 * Like vs_string(), but quoted fields use SQL quoting (single quotes doubled).
 *
 * @param vs The variable system handle.
 * @param frm The format string.
 * @return The expanded string.
 */
char *vs_sql_string (int vs, const char *frm)
{
	return vs_string_mode (vs, frm, 1);
}

/* ---- Build helpers ---------------------------------------------------- */

/**
 * Adds n C-strings as the values of a variable.
 *
 * Creates the variable system when vs <= 0; values are appended to any
 * existing ones.
 *
 * @param vs The variable system handle, or 0.
 * @param name The variable name.
 * @param list The C-string array (may be NULL).
 * @param n Number of strings.
 * @return The variable system handle.
 */
int vs_add (int vs, const char *name, const char *const *list, int n)
{
	if (vs <= 0)
		vs = vs_init ();
	int var = vs_lookup (vs, name);
	for (int i = 0; i < n; i++)
		vs_kset (var, list ? list[i] : NULL, VS_APPEND);
	return vs;
}

static int vs_string_builder_mode (int strh, int vs, const char *frm, int sql)
{
	if (strh <= 0)
		strh = m_alloc (16, 1, MFREE);

	vs_exp_t se;
	vs_exp_init (&se);
	se.sql = sql;
	vs_exp_parse (&se, frm);
	const char *expanded = vs_expand (&se, vs, 0);

	/* Append, dropping a trailing NUL left by a previous append. */
	size_t cur = m_len (strh);
	if (cur > 0 && CHAR (strh, cur - 1) == 0)
		cur--;
	m_write (strh, cur, expanded, strlen (expanded) + 1);

	vs_exp_free (&se);
	return strh;
}

/**
 * Expands a format string and appends the result to a string handle.
 *
 * @param strh Destination string handle, or 0 to allocate a new one.
 * @param vs The variable system handle.
 * @param frm The format string.
 * @return The destination string handle.
 */
int vs_string_builder (int strh, int vs, const char *frm)
{
	return vs_string_builder_mode (strh, vs, frm, 0);
}

/**
 * Like vs_string_builder(), but quoted fields use SQL quoting.
 *
 * @param strh Destination string handle, or 0 to allocate a new one.
 * @param vs The variable system handle.
 * @param frm The format string.
 * @return The destination string handle.
 */
int vs_sql_string_builder (int strh, int vs, const char *frm)
{
	return vs_string_builder_mode (strh, vs, frm, 1);
}

/* ---- Field reader ----------------------------------------------------- */

static const char *find_sep (const char *s, const char *sep)
{
	size_t n = sep ? strlen (sep) : 0;
	if (n == 0)
		return NULL; /* empty separator -> whole line is one field */
	for (; *s; s++)
		if (strncmp (s, sep, n) == 0)
			return s;
	return NULL;
}

/**
 * Reads one line and stores its fields as the values of a variable.
 *
 * The line is split on the literal sep and each field is whitespace-trimmed.
 * Previous values are dropped. An empty line yields one empty field; at EOF
 * the variable ends up with 0 values (the loop-end signal, analogous to an
 * empty s_read_fields). vs must already be initialized.
 *
 * @param vs The variable system handle.
 * @param name The variable name.
 * @param fp The input stream.
 * @param sep The literal separator (empty/NULL: whole line is one field).
 * @return The variable handle.
 */
int vs_read_fields (int vs, const char *name, FILE *fp, const char *sep)
{
	int var = vs_lookup (vs, name);
	vs_kclr (var);
	if (!fp)
		return var;

	char *line = NULL;
	size_t cap = 0;
	if (getline (&line, &cap, fp) < 0) {
		free (line);
		return var; /* EOF -> variable keeps 0 values */
	}

	/* Split on sep and trim each field in place. An empty line yields one
	 * empty field, exactly like s_read_fields. */
	char *p = line;
	for (;;) {
		char *q = (char *)find_sep (p, sep);
		char *e = q ? q : p + strlen (p);
		char *start = p;
		while (start < e && isspace ((unsigned char)*start))
			start++;
		while (e > start && isspace ((unsigned char)e[-1]))
			e--;
		char saved = *e;
		*e = 0;
		vs_kset (var, start, VS_APPEND);
		*e = saved;
		if (!q)
			break;
		p = q + strlen (sep);
	}
	free (line);
	return var;
}
