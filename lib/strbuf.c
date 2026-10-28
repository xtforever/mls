#include "strbuf.h"

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SB_INIT_CAP 64

strbuf_t *sb_new (void)
{
	strbuf_t *b = malloc (sizeof *b);
	if (!b)
		return NULL;
	b->buf = malloc (SB_INIT_CAP);
	if (!b->buf) {
		free (b);
		return NULL;
	}
	b->buf[0] = '\0';
	b->len = 0;
	b->cap = SB_INIT_CAP;
	return b;
}

void sb_free (strbuf_t *b)
{
	if (!b)
		return;
	free (b->buf);
	free (b);
}

/* Ensure room for `extra` more bytes (including the trailing NUL).
   On failure the buffer is left unchanged; the append becomes a no-op. */
static int sb_reserve (strbuf_t *b, int extra)
{
	if (extra < 0 || b->len > INT_MAX - extra)
		return -1;
	int need = b->len + extra;
	if (need <= b->cap)
		return 0;

	int cap = b->cap > 0 ? b->cap : SB_INIT_CAP;
	while (cap < need) {
		if (cap > INT_MAX / 2) {
			cap = need;
			break;
		}
		cap *= 2;
	}

	char *p = realloc (b->buf, (size_t)cap);
	if (!p)
		return -1;
	b->buf = p;
	b->cap = cap;
	return 0;
}

void sb_append_c (strbuf_t *b, char c)
{
	if (sb_reserve (b, 2) != 0)
		return;
	b->buf[b->len++] = c;
	b->buf[b->len] = '\0';
}

void sb_append_s (strbuf_t *b, const char *s)
{
	if (!s)
		return;
	size_t n = strlen (s);
	if (n > (size_t)INT_MAX)
		return;
	sb_append_n (b, s, (int)n);
}

void sb_append_n (strbuf_t *b, const char *s, int n)
{
	if (!s || n < 0)
		return;
	if (sb_reserve (b, n + 1) != 0)
		return;
	memcpy (b->buf + b->len, s, (size_t)n);
	b->len += n;
	b->buf[b->len] = '\0';
}

void sb_append_int (strbuf_t *b, int v)
{
	char tmp[32];
	snprintf (tmp, sizeof tmp, "%d", v);
	sb_append_s (b, tmp);
}

void sb_append_cell (strbuf_t *b, intptr_t v)
{
	char tmp[32];
	snprintf (tmp, sizeof tmp, "%" PRIdPTR, v);
	sb_append_s (b, tmp);
}

void sb_append_hex (strbuf_t *b, intptr_t v)
{
	char tmp[32];
	snprintf (tmp, sizeof tmp, "%" PRIxPTR, (uintptr_t)v);
	sb_append_s (b, tmp);
}

void sb_clear (strbuf_t *b)
{
	b->len = 0;
	b->buf[0] = '\0';
}

const char *sb_cstr (strbuf_t *b) { return b->buf; }
