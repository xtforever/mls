#ifndef M_SQL_H
#define M_SQL_H

/*
 * m_sql - lean SQL statement building on top of the mls strings and the
 * m_var "$" expansion.  This is not an SQL parser: a terse column spec
 * ("x y z" or "x,y,z") plus a table name becomes a SELECT or INSERT
 * statement.  Values for INSERT are taken from the caller's variable system
 * (see sql_insert), so quoting stays in one place (vs_sql_string_builder).
 *
 * Everything here is pure string work: no database, no schema introspection.
 */

#include "mls.h"

/**
 * @brief Split a terse column spec into a string list.
 *
 * Separators are whitespace and commas; runs and leading/trailing separators
 * are ignored, so "x y z", "x,y,z" and " x , y  z " all give [x, y, z].
 * Returns an m-array of char* (MFREE_STR): free it with m_free().
 */
int sql_cols (const char *spec);

/**
 * @brief Build a SELECT statement from a table and a terse column spec.
 *
 * @param table The table name (used verbatim).
 * @param cols The terse column spec, e.g. "id name qty".
 * @param where Optional WHERE clause without the keyword; NULL or "" omits it.
 * @return A string handle "SELECT ... FROM ..."; free it with m_free().
 */
int sql_select (const char *table, const char *cols, const char *where);

/**
 * @brief Build an INSERT statement, with values expanded from a variable
 * system.
 *
 * @param vs The variable system holding the values under the variable "vals".
 * @param table The table name (used verbatim).
 * @param cols The terse column spec, matching the values in "vals".
 * @return A string handle "INSERT INTO ... (...) VALUES (...)"; free it with
 *         m_free(). The values are SQL-quoted (single quotes doubled).
 */
int sql_insert (int vs, const char *table, const char *cols);

#endif
