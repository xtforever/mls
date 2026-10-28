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

	assert (m_isam_count (NULL) == 0);
	assert (m_isam_field (NULL, "x") == -1);
	m_isam_db_free (db);
	m_isam_db_free (NULL);

	puts ("m_isam selftest: OK");
	return 0;
}
