#include "m_fproto.h"

#include "strbuf.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct m_fproto {
	strbuf_t *sb;
};

m_fproto_t *m_fproto_new (void)
{
	m_fproto_t *w = calloc (1, sizeof *w);
	if (!w)
		return NULL;
	w->sb = sb_new ();
	if (!w->sb) {
		free (w);
		return NULL;
	}
	return w;
}

void m_fproto_free (m_fproto_t *w)
{
	if (!w)
		return;
	sb_free (w->sb);
	free (w);
}

static void put_escaped (strbuf_t *sb, const char *s)
{
	for (; *s; s++) {
		switch (*s) {
		case '"':
			sb_append_s (sb, "\\\"");
			break;
		case '\\':
			sb_append_s (sb, "\\\\");
			break;
		case '\n':
			sb_append_s (sb, "\\n");
			break;
		case '\t':
			sb_append_s (sb, "\\t");
			break;
		default:
			sb_append_c (sb, *s);
			break;
		}
	}
}

void m_fproto_result (m_fproto_t *w, int ncols)
{
	char buf[32];
	snprintf (buf, sizeof buf, "RESULT COLS %d", ncols);
	sb_append_s (w->sb, buf);
}

void m_fproto_col (m_fproto_t *w, const char *name, const char *type)
{
	sb_append_s (w->sb, " COL Z\"");
	put_escaped (w->sb, name);
	sb_append_s (w->sb, "\" Z\"");
	put_escaped (w->sb, type);
	sb_append_c (w->sb, '"');
}

void m_fproto_count (m_fproto_t *w, int64_t n)
{
	char buf[48];
	snprintf (buf, sizeof buf, " COUNT %lld", (long long)n);
	sb_append_s (w->sb, buf);
}

void m_fproto_row (m_fproto_t *w) { sb_append_s (w->sb, " ROW"); }

void m_fproto_int (m_fproto_t *w, int64_t v)
{
	char buf[48];
	snprintf (buf, sizeof buf, " %lld", (long long)v);
	sb_append_s (w->sb, buf);
}

void m_fproto_str (m_fproto_t *w, const char *s)
{
	sb_append_s (w->sb, " S\"");
	put_escaped (w->sb, s);
	sb_append_c (w->sb, '"');
}

void m_fproto_zstr (m_fproto_t *w, const char *s)
{
	sb_append_s (w->sb, " Z\"");
	put_escaped (w->sb, s);
	sb_append_c (w->sb, '"');
}

void m_fproto_end (m_fproto_t *w) { sb_append_s (w->sb, " END"); }

void m_fproto_error (m_fproto_t *w, int code, const char *msg)
{
	char buf[32];
	snprintf (buf, sizeof buf, "ERROR %d Z\"", code);
	sb_append_s (w->sb, buf);
	put_escaped (w->sb, msg ? msg : "");
	sb_append_c (w->sb, '"');
}

const char *m_fproto_data (const m_fproto_t *w)
{
	return w ? sb_cstr (w->sb) : "";
}

/* --- parser ------------------------------------------------------------ */

static int is_ws (char c)
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static void skip_ws (const char **p)
{
	while (is_ws (**p))
		(*p)++;
}

static int keyword (const char **p, const char *kw)
{
	skip_ws (p);
	size_t n = strlen (kw);
	if (strncmp (*p, kw, n) != 0)
		return 0;
	char c = (*p)[n];
	if (!is_ws (c) && c != '\0')
		return 0;
	*p += n;
	return 1;
}

static int parse_i64 (const char **p, int64_t *out)
{
	skip_ws (p);
	char *end;
	long long v = strtoll (*p, &end, 10);
	if (end == *p || (!is_ws (*end) && *end != '\0'))
		return 0;
	*out = (int64_t)v;
	*p = end;
	return 1;
}

static char *parse_quoted (const char **p)
{
	skip_ws (p);
	if (!(((*p)[0] == 'S' || (*p)[0] == 'Z') && (*p)[1] == '"'))
		return NULL;
	*p += 2;
	skip_ws (p); /* mirrors the Forth tokenizer */

	strbuf_t *sb = sb_new ();
	if (!sb)
		return NULL;
	while (**p && **p != '"') {
		char c = **p;
		if (c != '\\') {
			sb_append_c (sb, c);
			(*p)++;
			continue;
		}
		char e = (*p)[1];
		if (!e) {
			sb_append_c (sb, '\\');
			(*p)++;
			break;
		}
		switch (e) {
		case '"':
			sb_append_c (sb, '"');
			break;
		case '\\':
			sb_append_c (sb, '\\');
			break;
		case 'n':
			sb_append_c (sb, '\n');
			break;
		case 't':
			sb_append_c (sb, '\t');
			break;
		default:
			sb_append_c (sb, '\\');
			sb_append_c (sb, e);
			break;
		}
		*p += 2;
	}
	if (**p != '"') {
		sb_free (sb);
		return NULL;
	}
	(*p)++;
	char *out = malloc (strlen (sb_cstr (sb)) + 1);
	if (out)
		strcpy (out, sb_cstr (sb));
	sb_free (sb);
	return out;
}

static int parse_value (const char **p, m_fproto_value_t *v)
{
	skip_ws (p);
	if (((*p)[0] == 'S' || (*p)[0] == 'Z') && (*p)[1] == '"') {
		v->type = (*p)[0] == 'S' ? M_FPROTO_STR : M_FPROTO_ZSTR;
		v->s = parse_quoted (p);
		return v->s != NULL;
	}
	v->type = M_FPROTO_INT;
	return parse_i64 (p, &v->i);
}

static void free_values (m_fproto_value_t *v, int n)
{
	if (!v)
		return;
	for (int i = 0; i < n; i++)
		free (v[i].s);
	free (v);
}

void m_fproto_result_free (m_fproto_result_t *r)
{
	if (!r)
		return;
	for (int c = 0; c < r->ncols; c++) {
		free (r->cols[c].name);
		free (r->cols[c].type);
	}
	free (r->cols);
	free_values (r->values, r->nrows * r->ncols);
	free (r);
}

m_fproto_result_t *m_fproto_parse (const char *text)
{
	if (!text)
		return NULL;
	const char *p = text;
	int64_t n64;
	if (!keyword (&p, "RESULT") || !keyword (&p, "COLS") ||
	    !parse_i64 (&p, &n64) || n64 < 0 || n64 > 4096)
		return NULL;

	m_fproto_result_t *r = calloc (1, sizeof *r);
	if (!r)
		return NULL;
	r->ncols = (int)n64;
	r->cols = calloc ((size_t)r->ncols, sizeof *r->cols);
	if (!r->cols && r->ncols > 0)
		goto fail;

	for (int c = 0; c < r->ncols; c++) {
		if (!keyword (&p, "COL"))
			goto fail;
		r->cols[c].name = parse_quoted (&p);
		r->cols[c].type = parse_quoted (&p);
		if (!r->cols[c].name || !r->cols[c].type)
			goto fail;
	}

	skip_ws (&p);
	if (strncmp (p, "COUNT", 5) == 0 && (is_ws (p[5]) || p[5] == '\0')) {
		p += 5;
		if (!parse_i64 (&p, &r->count))
			goto fail;
		r->has_count = 1;
	}

	int cap = 0;
	for (;;) {
		skip_ws (&p);
		if (strncmp (p, "END", 3) == 0 &&
		    (is_ws (p[3]) || p[3] == '\0')) {
			p += 3;
			break;
		}
		if (!keyword (&p, "ROW"))
			goto fail;
		if (r->nrows == cap) {
			cap = cap ? cap * 2 : 8;
			m_fproto_value_t *nv = realloc (
				r->values,
				(size_t)cap * (size_t)r->ncols * sizeof *nv);
			if (!nv)
				goto fail;
			r->values = nv;
		}
		int base = r->nrows * r->ncols;
		memset (&r->values[base], 0,
			(size_t)r->ncols * sizeof *r->values);
		for (int c = 0; c < r->ncols; c++) {
			m_fproto_value_t *v = &r->values[base + c];
			if (!parse_value (&p, v)) {
				r->nrows++; /* free path covers the partial row
					     */
				goto fail;
			}
		}
		r->nrows++;
	}

	skip_ws (&p);
	if (*p != '\0')
		goto fail;
	return r;

fail:
	m_fproto_result_free (r);
	return NULL;
}
