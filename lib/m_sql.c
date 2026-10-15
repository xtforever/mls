#include "m_sql.h"
#include "m_tool.h"
#include "m_var.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/*
 * m_sql - see m_sql.h. Pure string building on the mls core: an explicit
 * tokenizer for the terse column spec (s_split()/m_str_split() do not
 * collapse runs of a whitespace delimiter) and the m_var $-expansion for the
 * INSERT value list, so quoting stays in vs_sql_string_builder().
 */

/* Separators of a terse column spec: whitespace and commas. */
static int sql_is_sep (unsigned char c) { return isspace (c) || c == ','; }

/* Append the identifiers of an MFREE_STR list as "a, b, c". */
static void sql_join_cols (int out, int list)
{
	int p, first = 1;
	char **d;
	m_foreach (list, p, d)
	{
		if (!first)
			s_app (out, ", ", NULL);
		first = 0;
		s_cat (out, *d);
	}
}

/**
 * @brief Split a terse column spec into a string list.
 *
 * Separators are whitespace and commas; runs and leading/trailing separators
 * are ignored, so "x y z", "x,y,z" and " x , y  z " all give [x, y, z].
 *
 * @param spec The column spec (may be NULL or empty -> empty list).
 * @return An m-array of char* (MFREE_STR): free it with m_free().
 */
int sql_cols (const char *spec)
{
	int list = m_alloc (8, sizeof (char *), MFREE_STR);
	const char *p = spec ? spec : "";

	while (*p) {
		while (*p && sql_is_sep ((unsigned char)*p))
			p++;
		if (!*p)
			break;
		const char *start = p;
		while (*p && !sql_is_sep ((unsigned char)*p))
			p++;
		char *w = strndup (start, (size_t)(p - start));
		m_put (list, &w);
	}
	return list;
}

/**
 * @brief Build a SELECT statement from a table and a terse column spec.
 *
 * The column spec is expanded by sql_cols() and joined with ", "; the table
 * name and the optional where clause are emitted verbatim.
 *
 * @param table The table name (used verbatim).
 * @param cols The terse column spec, e.g. "id name qty".
 * @param where Optional WHERE clause without the keyword; NULL or "" omits it.
 * @return A string handle "SELECT ... FROM ..."; free it with m_free().
 */
int sql_select (const char *table, const char *cols, const char *where)
{
	int list = sql_cols (cols);
	int out = s_printf (0, 0, "SELECT ");

	sql_join_cols (out, list);
	s_printf (out, -1, " FROM %s", table ? table : "");
	if (where && *where)
		s_printf (out, -1, " WHERE %s", where);

	m_free (list);
	return out;
}

/**
 * @brief Build an INSERT statement, with values expanded from a variable
 * system.
 *
 * The column list is built like sql_select(); the VALUES list is the
 * expansion of "$'vals[*]'" against vs, so every value is SQL-quoted by
 * vs_sql_string_builder() (single quotes doubled: O'Brien -> 'O''Brien').
 * Populate the values with vs_add(vs, "vals", list, n) or
 * vs_read_fields(vs, "vals", ...).
 *
 * @param vs The variable system holding the values under the variable "vals".
 * @param table The table name (used verbatim).
 * @param cols The terse column spec, matching the values in "vals".
 * @return A string handle "INSERT INTO ... (...) VALUES (...)"; free it with
 *         m_free().
 */
int sql_insert (int vs, const char *table, const char *cols)
{
	int list = sql_cols (cols);
	int out = s_printf (0, 0, "INSERT INTO %s (", table ? table : "");

	sql_join_cols (out, list);
	s_app (out, ") VALUES ( ", NULL);
	vs_sql_string_builder (out, vs, "$'vals[*]' )");

	m_free (list);
	return out;
}
