#include "strbuf.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main (void)
{
	strbuf_t *b = sb_new ();
	assert (b);
	assert (strcmp (sb_cstr (b), "") == 0);

	assert (strcmp (sb_cstr (b), "") == 0);
	sb_append_c (b, 'a');
	assert (strcmp (sb_cstr (b), "a") == 0);
	sb_append_s (b, "bc");
	assert (strcmp (sb_cstr (b), "abc") == 0);
	sb_append_n (b, "defg", 2);
	assert (strcmp (sb_cstr (b), "abcde") == 0);
	sb_append_n (b, "ignored", 0);
	sb_append_n (b, "ignored", -1);
	sb_append_s (b, NULL);
	assert (strcmp (sb_cstr (b), "abcde") == 0);

	sb_append_int (b, -42);
	assert (strcmp (sb_cstr (b), "abcde-42") == 0);
	sb_append_cell (b, (intptr_t)-1234567890);
	assert (strcmp (sb_cstr (b), "abcde-42-1234567890") == 0);
	sb_clear (b);
	assert (strcmp (sb_cstr (b), "") == 0);
	sb_append_hex (b, (intptr_t)0x2a);
	assert (strcmp (sb_cstr (b), "2a") == 0);

	/* Growth far beyond the initial capacity, content intact. */
	sb_clear (b);
	for (int i = 0; i < 10000; i++)
		sb_append_c (b, 'x');
	assert (strlen (sb_cstr (b)) == 10000);
	for (int i = 0; i < 10000; i++)
		assert (sb_cstr (b)[i] == 'x');
	assert (sb_cstr (b)[10000] == '\0');

	/* append_n preserves embedded NULs by length (C string stops early). */
	sb_clear (b);
	sb_append_n (b, "a\0b", 3);
	assert (b->len == 3);
	assert (sb_cstr (b)[0] == 'a');
	assert (sb_cstr (b)[2] == 'b');
	assert (sb_cstr (b)[3] == '\0');

	sb_free (b);
	sb_free (NULL);

	puts ("strbuf selftest: OK");
	return 0;
}
