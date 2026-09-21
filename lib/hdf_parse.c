#include "hdf_parse.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int put_ch (int h, char c) { return m_put (h, &c); }

static const char *skip_ws (const char *p)
{
	while (*p && isspace ((unsigned char)*p))
		p++;
	if (*p == ';') {
		while (*p && *p != '\n')
			p++;
		return skip_ws (p);
	}
	return p;
}

static int is_keyword_char (char c, int initial)
{
	if (isalpha ((unsigned char)c) || c == '_')
		return 1;
	if (!initial && (isdigit ((unsigned char)c) || strchr (".:/-", c)))
		return 1;
	return 0;
}

static char unescape (char c)
{
	switch (c) {
	case 'n':
		return '\n';
	case 'r':
		return '\r';
	case 't':
		return '\t';
	case '\\':
		return '\\';
	case '"':
		return '"';
	default:
		return c;
	}
}

/* is p the closing "]" + n*"=" + "]" of a [[..]] raw string? */
static int match_close (const char *p, int n)
{
	int i;
	if (*p != ']')
		return 0;
	for (i = 1; i <= n; i++)
		if (p[i] != '=')
			return 0;
	return p[n + 1] == ']';
}

static list_type_t word_type (const char *buf)
{
	if (strcmp (buf, "true") == 0)
		return LIST_BOOL;
	if (strcmp (buf, "false") == 0)
		return LIST_BOOL;
	if (strcmp (buf, "null") == 0)
		return LIST_NULL;
	char *end;
	strtod (buf, &end);
	return *end == '\0' ? LIST_NUMBER : LIST_STRING;
}

static int word_node (const char *s, int len)
{
	char *buf = malloc ((size_t)len + 1);
	memcpy (buf, s, (size_t)len);
	buf[len] = 0;
	list_type_t t = word_type (buf);
	int data = t == LIST_NULL ? 0 : list_str (buf, len);
	free (buf);
	return list_node (t, data);
}

/* skip a balanced ( ... ) group; p points just inside the '(' */
static const char *skip_parens (const char *p)
{
	int depth = 1;
	while (*p && depth) {
		if (*p == '(')
			depth++;
		else if (*p == ')')
			depth--;
		p++;
	}
	return p;
}

static int is_rem (int child, int children)
{
	const char *v;
	if (child == -2 || m_len (children) != 0)
		return 0;
	if (list_type (child) != LIST_STRING)
		return 0;
	v = list_text (child);
	return v && strcmp (v, "rem") == 0;
}

static const char *fail_list (int children)
{
	m_free (children);
	return NULL;
}

static const char *do_rem (const char *p, int child, int children, int *node)
{
	list_free (child);
	m_free (children);
	*node = -2;
	return skip_parens (p);
}

static const char *parse_element (const char *p, int *node);

static const char *parse_raw (const char *p, int *node)
{
	int n = 0;
	while (p[1 + n] == '=')
		n++;
	if (p[1 + n] != '[')
		return NULL;
	const char *start = p + 2 + n;
	int len = 0;
	while (start[len] && !match_close (start + len, n))
		len++;
	if (!start[len])
		return NULL;
	*node = list_node (LIST_STRING, list_str (start, len));
	return start + len + n + 2;
}

static const char *parse_quoted (const char *p, int *node)
{
	int s_h = m_create (0, 1);
	p++;
	while (*p && *p != '"') {
		char c = *p++;
		if (c == '\\')
			c = unescape (*p++);
		put_ch (s_h, c);
	}
	if (*p == '"')
		p++;
	put_ch (s_h, 0);
	*node = list_node (LIST_STRING, s_h);
	return p;
}

static const char *parse_word (const char *p, int *node)
{
	const char *start = p;
	while (*p && is_keyword_char (*p, 0))
		p++;
	*node = word_node (start, (int)(p - start));
	return p;
}

static const char *parse_list (const char *p, int *node)
{
	int children = m_create (0, sizeof (int));
	p = skip_ws (p + 1);
	while (*p && *p != ')') {
		int child;
		p = parse_element (p, &child);
		if (!p)
			return fail_list (children);
		if (is_rem (child, children))
			return do_rem (p, child, children, node);
		if (child != -2)
			m_put (children, &child);
		p = skip_ws (p);
	}
	if (*p == ')')
		p++;
	*node = list_node (LIST_LIST, children);
	return p;
}

static const char *parse_element (const char *p, int *node)
{
	p = skip_ws (p);
	if (*p == '(')
		return parse_list (p, node);
	if (*p == '"')
		return parse_quoted (p, node);
	if (*p == '[')
		return parse_raw (p, node);
	if (is_keyword_char (*p, 1) || *p == '-' || isdigit ((unsigned char)*p))
		return parse_word (p, node);
	return NULL;
}

int hdf_parse (const char *text)
{
	if (!text)
		return 0;
	int root_list = m_create (0, sizeof (int));
	text = skip_ws (text);
	while (*text) {
		int node;
		text = parse_element (text, &node);
		if (!text)
			break;
		if (node != -2)
			m_put (root_list, &node);
		text = skip_ws (text);
	}
	return list_node (LIST_LIST, root_list);
}

int hdf_parse_file (const char *path)
{
	FILE *fp = fopen (path, "r");
	if (!fp)
		return 0;
	fseek (fp, 0, SEEK_END);
	long len = ftell (fp);
	fseek (fp, 0, SEEK_SET);
	char *buf = malloc ((size_t)len + 1);
	size_t got = fread (buf, 1, (size_t)len, fp);
	buf[got] = 0;
	fclose (fp);
	int res = hdf_parse (buf);
	free (buf);
	return res;
}
