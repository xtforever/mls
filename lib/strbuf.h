#ifndef STRBUF_H
#define STRBUF_H

#include <stdint.h>

/* Growable, always NUL-terminated string builder. */
typedef struct {
	char *buf;
	int len;
	int cap;
} strbuf_t;

strbuf_t *sb_new (void);
void sb_free (strbuf_t *b);
void sb_append_c (strbuf_t *b, char c);
void sb_append_s (strbuf_t *b, const char *s);
void sb_append_n (strbuf_t *b, const char *s, int n);
void sb_append_int (strbuf_t *b, int v);
void sb_append_cell (strbuf_t *b, intptr_t v);
void sb_append_hex (strbuf_t *b, intptr_t v);
void sb_clear (strbuf_t *b);
const char *sb_cstr (strbuf_t *b);

#endif
