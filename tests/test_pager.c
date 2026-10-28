#include "pager.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define PAGE PAGER_PAGE_SIZE

static void fill (unsigned char *b, unsigned char v) { memset (b, v, PAGE); }

int main (void)
{
	const char *path = "/tmp/mls_pager_test.db";
	const char *bad = "/tmp/mls_pager_bad.db";
	remove (path);

	unsigned char in[PAGE], out[PAGE];
	pager_t *p = pager_create (path);
	assert (p);
	assert (pager_pages (p) == 0);
	assert (pager_reads (p) == 0 && pager_writes (p) == 0);

	fill (in, 0xA5);
	assert (pager_write (p, 0, in) == 0);
	assert (pager_pages (p) == 1);
	assert (pager_write (p, 2, in) == -1); /* no gap */
	fill (in, 0x5A);
	assert (pager_write (p, 1, in) == 0); /* append */
	assert (pager_pages (p) == 2);
	assert (pager_write (p, 0, NULL) == -1);
	assert (pager_read (p, 0, NULL) == -1);

	/* Writes are cached; reads hit the cache, no physical I/O yet. */
	assert (pager_read (p, 0, out) == 0 && out[0] == 0xA5);
	assert (pager_read (p, 0, out) == 0);
	assert (pager_reads (p) == 0);
	assert (pager_close (p) == 0); /* flushes */

	/* Reopen: persisted data, first read is a miss, second a hit. */
	p = pager_open (path);
	assert (p);
	assert (pager_pages (p) == 2);
	uint64_t r = pager_reads (p);
	assert (pager_read (p, 0, out) == 0 && out[0] == 0xA5);
	assert (pager_read (p, 0, out) == 0);
	assert (pager_reads (p) - r == 1);
	assert (pager_read (p, 2, out) == -1); /* past end */
	assert (pager_write (p, 3, in) == -1);
	assert (pager_write (p, 2, in) == 0);
	assert (pager_pages (p) == 3);
	assert (pager_flush (p) == 0);
	assert (pager_writes (p) >= 1);
	assert (pager_close (p) == 0);

	/* More pages than cache slots: eviction must still persist everything.
	 */
	remove (path);
	p = pager_create (path);
	assert (p);
	for (uint32_t i = 0; i < 20; i++) {
		fill (in, (unsigned char)i);
		assert (pager_write (p, i, in) == 0);
	}
	assert (pager_pages (p) == 20);
	assert (pager_write (p, 21, in) == -1);
	assert (pager_close (p) == 0);

	p = pager_open (path);
	assert (p);
	assert (pager_pages (p) == 20);
	for (uint32_t i = 0; i < 20; i++) {
		assert (pager_read (p, i, out) == 0);
		assert (out[0] == (unsigned char)i);
		assert (out[PAGE - 1] == (unsigned char)i);
	}
	assert (pager_close (p) == 0);

	/* Truncated (not page-aligned) file is rejected. */
	remove (bad);
	FILE *f = fopen (bad, "wb");
	assert (f);
	assert (fwrite (out, 1, 100, f) == 100);
	fclose (f);
	assert (pager_open (bad) == NULL);
	remove (bad);

	assert (pager_close (NULL) == 0);
	assert (pager_pages (NULL) == 0);
	assert (pager_reads (NULL) == 0 && pager_writes (NULL) == 0);

	remove (path);
	puts ("pager selftest: OK");
	return 0;
}
