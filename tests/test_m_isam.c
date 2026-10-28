#include "m_isam.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main (void)
{
	m_isam_db_t *db = m_isam_db_new ();
	assert (db);

	m_isam_col_t cols[] = {{"a", 'i'}, {"b", 'i'}, {"val", 'i'}};
	const char *pk[] = {"a", "b"};
	m_isam_table_t *t = m_isam_create_table (db, "pos", cols, 3, pk, 2);
	assert (t);
	assert (m_isam_table (db, "pos") == t);
	assert (m_isam_table (db, "missing") == NULL);
	assert (m_isam_create_table (db, "pos", cols, 3, pk, 2) == NULL);
	assert (m_isam_field (t, "a") == 0);
	assert (m_isam_field (t, "val") == 2);
	assert (m_isam_field (t, "nope") == -1);
	assert (m_isam_count (t) == 0);

	/* Three records, composite PK (a,b). */
	assert (m_isam_put (t, "a", 1) == 0);
	assert (m_isam_put (t, "b", 2) == 0);
	assert (m_isam_put (t, "val", 20) == 0);
	int64_t r1 = m_isam_insert (t);
	assert (r1 == 1);

	assert (m_isam_put (t, "a", 1) == 0);
	assert (m_isam_put (t, "b", 1) == 0);
	assert (m_isam_put (t, "val", 10) == 0);
	int64_t r2 = m_isam_insert (t);
	assert (r2 == 2);

	assert (m_isam_put (t, "a", 2) == 0);
	assert (m_isam_put (t, "b", 0) == 0);
	assert (m_isam_put (t, "val", 99) == 0);
	int64_t r3 = m_isam_insert (t);
	assert (r3 == 3);
	assert (m_isam_count (t) == 3);

	/* Seek + fetch. */
	cell_t key[2] = {1, 2};
	int64_t found = 0;
	assert (m_isam_seek (t, key, 2, &found) && found == r1);
	cell_t out[3];
	assert (m_isam_fetch (t, r1, out) && out[2] == 20);
	cell_t v;
	assert (m_isam_get (t, r2, "val", &v) && v == 10);
	assert (!m_isam_get (t, r2, "nope", &v));

	/* Duplicate PK is rejected, nothing is appended. */
	assert (m_isam_put (t, "a", 1) == 0);
	assert (m_isam_put (t, "b", 2) == 0);
	assert (m_isam_put (t, "val", 21) == 0);
	assert (m_isam_insert (t) == 0);
	assert (m_isam_count (t) == 3);

	/* Delete = tombstone; PK leaves the index, recno is never reused. */
	assert (m_isam_delete (t, r2) == 1);
	assert (m_isam_delete (t, r2) == 0);
	cell_t key2[2] = {1, 1};
	assert (!m_isam_seek (t, key2, 2, &found));
	assert (!m_isam_fetch (t, r2, out));
	assert (!m_isam_get (t, r2, "val", &v));
	assert (m_isam_count (t) == 2);

	assert (m_isam_put (t, "a", 1) == 0);
	assert (m_isam_put (t, "b", 1) == 0);
	assert (m_isam_put (t, "val", 11) == 0);
	int64_t r4 = m_isam_insert (t);
	assert (r4 == 4 && r4 != r2);
	assert (m_isam_count (t) == 3);
	assert (m_isam_seek (t, key2, 2, &found) && found == r4);

	/* String PK + per-db string pool. */
	m_isam_col_t ucols[] = {{"name", 's'}, {"age", 'i'}};
	const char *upk[] = {"name"};
	m_isam_table_t *u = m_isam_create_table (db, "users", ucols, 2, upk, 1);
	assert (u);
	uint32_t alice = m_isam_intern (u, "alice");
	uint32_t bob = m_isam_intern (u, "bob");
	assert (alice > 0 && bob > 0 && alice != bob);
	assert (m_isam_intern (u, "alice") == alice); /* stable */
	assert (strcmp (m_isam_resolve (u, alice), "alice") == 0);
	assert (strcmp (m_isam_resolve (u, 0), "") == 0);

	assert (m_isam_put (u, "name", alice) == 0);
	assert (m_isam_put (u, "age", 30) == 0);
	assert (m_isam_insert (u) == 1);
	assert (m_isam_put (u, "name", bob) == 0);
	assert (m_isam_put (u, "age", 40) == 0);
	assert (m_isam_insert (u) == 2);
	assert (m_isam_count (u) == 2);

	cell_t skey[1] = {(cell_t)alice};
	assert (m_isam_seek (u, skey, 1, &found) && found == 1);
	assert (m_isam_get (u, found, "age", &v) && v == 30);
	assert (m_isam_delete (u, 2) == 1);
	cell_t bkey[1] = {(cell_t)bob};
	assert (!m_isam_seek (u, bkey, 1, &found));
	assert (m_isam_put (u, "name", bob) == 0);
	assert (m_isam_put (u, "age", 41) == 0);
	assert (m_isam_insert (u) == 3); /* next recno */

	/* RRDS table: no primary index. */
	m_isam_col_t rcols[] = {{"x", 'i'}};
	m_isam_table_t *r = m_isam_create_table (db, "rrd", rcols, 1, NULL, 0);
	assert (r);
	assert (m_isam_put (r, "x", 7) == 0);
	assert (m_isam_insert (r) == 1);
	assert (m_isam_count (r) == 1);
	assert (!m_isam_seek (r, skey, 0, &found));

	/* Invalid definitions are rejected. */
	m_isam_col_t bad[] = {{"a", 'q'}};
	assert (m_isam_create_table (db, "bad", bad, 1, NULL, 0) == NULL);
	const char *not_prefix[] = {"b"};
	assert (m_isam_create_table (db, "bad2", cols, 3, not_prefix, 1) ==
		NULL);

	/* Schema helpers: enumerate, rename, copy, drop. */
	assert (m_isam_ntables (db) == 3);
	assert (m_isam_table_at (db, 0) != NULL);
	assert (m_isam_table_at (db, -1) == NULL);
	assert (m_isam_table_at (db, 3) == NULL);

	assert (m_isam_rename_table (db, "users", "users_renamed") == 1);
	assert (m_isam_table (db, "users") == NULL);
	assert (m_isam_table (db, "users_renamed") != NULL);
	assert (m_isam_rename_table (db, "users_renamed", "pos") ==
		0); /* dup */
	assert (m_isam_rename_table (db, "missing", "x") == 0);

	/* Copy live records into an empty clone (same schema). */
	m_isam_col_t u2cols[] = {{"name", 's'}, {"age", 'i'}};
	const char *u2pk[] = {"name"};
	m_isam_table_t *u2 =
		m_isam_create_table (db, "users_copy", u2cols, 2, u2pk, 1);
	assert (u2);
	assert (m_isam_copy_table (db, "users_renamed", "users_copy") == 1);
	assert (m_isam_count (u2) ==
		m_isam_count (m_isam_table (db, "users_renamed")));
	uint32_t alice2 = m_isam_intern (u2, "alice");
	cell_t ck[1] = {(cell_t)alice2};
	int64_t cf = 0;
	assert (m_isam_seek (u2, ck, 1, &cf));
	assert (m_isam_get (u2, cf, "age", &v) && v == 30);
	assert (m_isam_copy_table (db, "users_renamed", "users_copy") == 0);

	/* Copy into a table with an extra column: missing source -> 0. */
	m_isam_col_t dcols[] = {{"name", 's'}, {"age", 'i'}, {"extra", 'i'}};
	const char *dpk[] = {"name"};
	m_isam_table_t *d =
		m_isam_create_table (db, "copy_extra", dcols, 3, dpk, 1);
	assert (d);
	assert (m_isam_copy_table (db, "users_renamed", "copy_extra") == 1);
	int64_t dr = 0;
	for (int64_t rr = 1; rr <= m_isam_nrecs (d); rr++)
		if (m_isam_get (d, rr, "extra", &v)) {
			dr = rr;
			break;
		}
	assert (dr > 0 && v == 0);

	assert (m_isam_drop_table (db, "copy_extra") == 1);
	assert (m_isam_table (db, "copy_extra") == NULL);
	assert (m_isam_drop_table (db, "copy_extra") == 0);
	assert (m_isam_drop_table (NULL, "x") == 0);

	assert (m_isam_count (NULL) == 0);
	assert (m_isam_field (NULL, "x") == -1);
	m_isam_db_free (db);
	m_isam_db_free (NULL);

	puts ("m_isam selftest: OK");
	return 0;
}
