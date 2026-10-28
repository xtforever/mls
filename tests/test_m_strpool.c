#include "m_strpool.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main (void)
{
	const char *path = "/tmp/mls_strpool_test.str";
	const char *bad = "/tmp/mls_strpool_bad.str";
	remove (path);
	remove (bad);

	m_strpool_t *sp = m_strpool_create (path);
	assert (sp);
	assert (m_strpool_count (sp) == 0);

	uint32_t a = m_strpool_intern (sp, "alpha");
	uint32_t b = m_strpool_intern (sp, "beta");
	uint32_t e = m_strpool_intern (sp, "");
	assert (a == 1 && b == 2 && e == 3);
	assert (m_strpool_intern (sp, "alpha") == a); /* stable */
	assert (m_strpool_lookup (sp, "beta") == b);
	assert (m_strpool_lookup (sp, "missing") == 0);
	assert (strcmp (m_strpool_resolve (sp, a), "alpha") == 0);
	assert (strcmp (m_strpool_resolve (sp, e), "") == 0);
	assert (m_strpool_resolve (sp, 0) == NULL);
	assert (m_strpool_resolve (sp, 4) == NULL);
	assert (m_strpool_count (sp) == 3);
	assert (m_strpool_close (sp) == 0);

	/* IDs and contents survive reopen. */
	sp = m_strpool_open (path);
	assert (sp);
	assert (m_strpool_count (sp) == 3);
	assert (m_strpool_lookup (sp, "alpha") == a);
	assert (m_strpool_lookup (sp, "beta") == b);
	assert (m_strpool_lookup (sp, "") == e);
	assert (strcmp (m_strpool_resolve (sp, b), "beta") == 0);

	/* New IDs keep counting from the persisted state. */
	uint32_t g = m_strpool_intern (sp, "gamma");
	assert (g == 4);
	assert (m_strpool_intern (sp, "") == e);
	assert (m_strpool_close (sp) == 0);

	sp = m_strpool_open (path);
	assert (sp);
	assert (m_strpool_count (sp) == 4);
	assert (m_strpool_lookup (sp, "gamma") == g);
	assert (strcmp (m_strpool_resolve (sp, g), "gamma") == 0);

	/* Many strings + reopen: growth of darray and hashmap. */
	for (int i = 0; i < 1000; i++) {
		char buf[32];
		snprintf (buf, sizeof buf, "word-%d", i);
		assert (m_strpool_intern (sp, buf) == (uint32_t)(5 + i));
	}
	assert (m_strpool_count (sp) == 1004);
	assert (m_strpool_close (sp) == 0);

	sp = m_strpool_open (path);
	assert (sp);
	assert (m_strpool_count (sp) == 1004);
	for (int i = 0; i < 1000; i++) {
		char buf[32];
		snprintf (buf, sizeof buf, "word-%d", i);
		uint32_t id = m_strpool_lookup (sp, buf);
		assert (id == (uint32_t)(5 + i));
		assert (strcmp (m_strpool_resolve (sp, id), buf) == 0);
	}
	assert (m_strpool_close (sp) == 0);
	sp = NULL;

	/* Corrupt / truncated files are rejected. */
	FILE *f = fopen (bad, "wb");
	assert (f);
	fputs ("not a string pool at all", f);
	fclose (f);
	assert (m_strpool_open (bad) == NULL);
	remove (bad);

	f = fopen (bad, "wb");
	assert (f);
	fwrite ("MLSSTR1", 1, 7, f); /* too short */
	fclose (f);
	assert (m_strpool_open (bad) == NULL);
	remove (bad);

	/* NULL handling. */
	assert (m_strpool_intern (NULL, "x") == 0);
	assert (m_strpool_intern (sp, NULL) == 0);
	assert (m_strpool_lookup (NULL, "x") == 0);
	assert (m_strpool_lookup (NULL, NULL) == 0);
	assert (m_strpool_resolve (NULL, 1) == NULL);
	assert (m_strpool_count (NULL) == 0);
	assert (m_strpool_close (NULL) == 0);

	remove (path);
	puts ("m_strpool selftest: OK");
	return 0;
}
