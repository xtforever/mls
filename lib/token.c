#include "token.h"

#include <string.h>

int tok_init (tokenizer_t *t, const char *line)
{
	t->input = line ? line : "";
	t->pos = 0;
	t->len = (int)strlen (t->input);
	t->scratch = sb_new ();
	return t->scratch ? 0 : -1;
}

void tok_free (tokenizer_t *t)
{
	if (!t)
		return;
	sb_free (t->scratch);
	t->scratch = NULL;
}

int tok_has_more (tokenizer_t *t) { return t->pos < t->len; }

static int tok_is_ws (char c)
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

void tok_skip_ws (tokenizer_t *t)
{
	while (t->pos < t->len && tok_is_ws (t->input[t->pos]))
		t->pos++;
}

char *tok_next (tokenizer_t *t)
{
	for (;;) {
		tok_skip_ws (t);
		if (t->pos >= t->len)
			return NULL;

		/* ( ... ) comment */
		if (t->input[t->pos] == '(') {
			t->pos++;
			while (t->pos < t->len && t->input[t->pos] != ')')
				t->pos++;
			if (t->pos < t->len)
				t->pos++;
			continue;
		}

		/* \ line comment */
		if (t->input[t->pos] == '\\') {
			t->pos = t->len;
			return NULL;
		}
		break;
	}

	int start = t->pos;
	while (t->pos < t->len && !tok_is_ws (t->input[t->pos]))
		t->pos++;

	sb_clear (t->scratch);
	sb_append_n (t->scratch, t->input + start, t->pos - start);
	return (char *)sb_cstr (t->scratch);
}

/* Decode \" \\ \n \t; an unknown escape keeps backslash and char (round-trip
 * safe). */
static void tok_append_escaped (strbuf_t *b, const char *s, int n)
{
	for (int i = 0; i < n; i++) {
		char c = s[i];
		if (c != '\\' || i + 1 >= n) {
			sb_append_c (b, c);
			continue;
		}
		char e = s[++i];
		switch (e) {
		case '"':
			sb_append_c (b, '"');
			break;
		case '\\':
			sb_append_c (b, '\\');
			break;
		case 'n':
			sb_append_c (b, '\n');
			break;
		case 't':
			sb_append_c (b, '\t');
			break;
		default:
			sb_append_c (b, '\\');
			sb_append_c (b, e);
			break;
		}
	}
}

char *tok_parse_string (tokenizer_t *t, char quote)
{
	tok_skip_ws (t);
	if (t->pos < t->len && t->input[t->pos] == quote)
		t->pos++;

	int start = t->pos;
	int end = start;
	while (t->pos < t->len) {
		char c = t->input[t->pos];
		if (c == '\\' && t->pos + 1 < t->len) {
			t->pos += 2; /* escaped char cannot close the string */
			end = t->pos;
			continue;
		}
		if (c == quote)
			break;
		t->pos++;
		end = t->pos;
	}

	sb_clear (t->scratch);
	int n = end - start;
	tok_append_escaped (t->scratch, t->input + start, n < 0 ? 0 : n);

	if (t->pos < t->len && t->input[t->pos] == quote)
		t->pos++; /* closing quote */
	return (char *)sb_cstr (t->scratch);
}
