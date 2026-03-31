#include "m_hdf.h"
#include "m_tool.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Internal structure for a node:
 * A node is an mls list with 2 elements:
 * index 0: (int) type (hdf_node_type_t)
 * index 1: (int) value (string handle or list handle of children)
 */

static int create_node (hdf_node_type_t type, int value)
{
	int n = m_alloc (2, sizeof (int), MFREE);
	m_puti (n, (int)type);
	m_puti (n, value);
	return n;
}

static const char *skip_ws (const char *p)
{
	while (*p && isspace (*p))
		p++;
	if (*p == ';') {
		while (*p && *p != '\n')
			p++;
		return skip_ws (p);
	}
	return p;
}

static const char *parse_element (const char *p, int *node);

static const char *parse_raw_string (const char *p, int *node)
{
	p++; // skip '['
	int equals = 0;
	while (*p == '=') {
		equals++;
		p++;
	}
	if (*p != '[')
		return NULL; // error
	p++;		     // skip second '['

	const char *start = p;
	while (*p) {
		if (*p == ']') {
			const char *end_cand = p;
			p++;
			int end_equals = 0;
			while (*p == '=') {
				end_equals++;
				p++;
			}
			if (*p == ']' && end_equals == equals) {
				int len = end_cand - start;
				int s_h = m_alloc (len + 1, 1, MFREE);
				m_write (s_h, 0, start, len);
				m_putc (s_h, 0);
				*node = create_node (HDF_TYPE_STRING, s_h);
				return p + 1;
			}
		} else {
			p++;
		}
	}
	return NULL;
}

static const char *parse_quoted_string (const char *p, int *node)
{
	p++; // skip '"'
	int s_h = m_alloc (0, 1, MFREE);
	while (*p && *p != '"') {
		if (*p == '\\') {
			p++;
			switch (*p) {
			case 'n':
				m_putc (s_h, '\n');
				break;
			case 'r':
				m_putc (s_h, '\r');
				break;
			case 't':
				m_putc (s_h, '\t');
				break;
			case '\\':
				m_putc (s_h, '\\');
				break;
			case '"':
				m_putc (s_h, '"');
				break;
			default:
				m_putc (s_h, *p);
				break;
			}
		} else {
			m_putc (s_h, *p);
		}
		p++;
	}
	if (*p == '"')
		p++;
	m_putc (s_h, 0);
	*node = create_node (HDF_TYPE_STRING, s_h);
	return p;
}

static int is_keyword_char (char c, int initial)
{
	if (isalpha (c) || c == '_')
		return 1;
	if (!initial && (isdigit (c) || strchr (".:/-", c)))
		return 1;
	return 0;
}

static const char *parse_keyword_or_literal (const char *p, int *node)
{
	const char *start = p;
	while (*p && is_keyword_char (*p, 0))
		p++;

	int len = p - start;
	char *buf = malloc (len + 1);
	memcpy (buf, start, len);
	buf[len] = 0;

	if (strcmp (buf, "true") == 0) {
		*node = create_node (HDF_TYPE_BOOLEAN, s_strdup_c ("true"));
	} else if (strcmp (buf, "false") == 0) {
		*node = create_node (HDF_TYPE_BOOLEAN, s_strdup_c ("false"));
	} else if (strcmp (buf, "null") == 0) {
		*node = create_node (HDF_TYPE_NULL, 0);
	} else {
		// Check if it's a number
		char *endptr;
		strtod (buf, &endptr);
		if (*endptr == '\0') {
			*node = create_node (HDF_TYPE_NUMBER, s_strdup_c (buf));
		} else {
			*node = create_node (HDF_TYPE_STRING, s_strdup_c (buf));
		}
	}
	free (buf);
	return p;
}

static const char *parse_list (const char *p, int *node)
{
	p++; // skip '('
	int children = m_alloc (0, sizeof (int), MFREE);
	p = skip_ws (p);
	while (*p && *p != ')') {
		int child;
		p = parse_element (p, &child);
		if (!p) {
			m_free (children);
			return NULL;
		}

		// Check for 'rem'
		if (child != -2 && hdf_get_type (child) == HDF_TYPE_STRING) {
			const char *val = hdf_get_value (child);
			if (m_len (children) == 0 && val &&
			    strcmp (val, "rem") == 0) {
				// This is a rem form. We need to skip until ')'
				hdf_free (child);
				int depth = 1;
				while (*p && depth > 0) {
					if (*p == '(')
						depth++;
					else if (*p == ')')
						depth--;
					p++;
				}
				m_free (children);
				*node = -2; // Marker for "skipped rem"
				return p;
			}
		}

		if (child != -2) {
			m_puti (children, child);
		}
		p = skip_ws (p);
	}
	if (*p == ')')
		p++;
	*node = create_node (HDF_TYPE_LIST, children);
	return p;
}

static const char *parse_element (const char *p, int *node)
{
	p = skip_ws (p);
	if (*p == '(')
		return parse_list (p, node);
	if (*p == '"')
		return parse_quoted_string (p, node);
	if (*p == '[')
		return parse_raw_string (p, node);
	if (is_keyword_char (*p, 1) || *p == '-' || isdigit (*p))
		return parse_keyword_or_literal (p, node);
	return NULL;
}

int hdf_parse_string (const char *data)
{
	const char *p = data;
	int root_list = m_alloc (0, sizeof (int), MFREE);
	p = skip_ws (p);
	while (*p) {
		int node;
		p = parse_element (p, &node);
		if (!p)
			break;
		if (node != -2) {
			m_puti (root_list, node);
		}
		p = skip_ws (p);
	}
	return create_node (HDF_TYPE_LIST, root_list);
}

int hdf_parse_file (const char *path)
{
	FILE *fp = fopen (path, "r");
	if (!fp)
		return -1;
	fseek (fp, 0, SEEK_END);
	long len = ftell (fp);
	fseek (fp, 0, SEEK_SET);
	char *buf = malloc (len + 1);
	fread (buf, 1, len, fp);
	buf[len] = 0;
	fclose (fp);
	int res = hdf_parse_string (buf);
	free (buf);
	return res;
}

void hdf_free (int h)
{
	if (h <= 0)
		return;
	hdf_node_type_t type = (hdf_node_type_t)INT (h, 0);
	int val = INT (h, 1);
	if (type == HDF_TYPE_LIST) {
		for (int i = 0; i < m_len (val); i++) {
			hdf_free (INT (val, i));
		}
		m_free (val);
	} else if (val > 0) {
		m_free (val);
	}
	m_free (h);
}

hdf_node_type_t hdf_get_type (int h)
{
	if (h <= 0)
		return HDF_TYPE_NULL;
	return (hdf_node_type_t)INT (h, 0);
}

const char *hdf_get_value (int h)
{
	if (h <= 0)
		return NULL;
	hdf_node_type_t type = hdf_get_type (h);
	if (type == HDF_TYPE_LIST)
		return NULL;
	int val = INT (h, 1);
	return val > 0 ? m_str (val) : NULL;
}

int hdf_get_children (int h)
{
	if (hdf_get_type (h) != HDF_TYPE_LIST)
		return -1;
	return INT (h, 1);
}

int hdf_find_node (int h, const char *key)
{
	int children = hdf_get_children (h);
	if (children <= 0)
		return -1;
	for (int i = 0; i < m_len (children); i++) {
		int child = INT (children, i);
		if (hdf_get_type (child) == HDF_TYPE_LIST) {
			int sub_children = hdf_get_children (child);
			if (m_len (sub_children) > 0) {
				int first = INT (sub_children, 0);
				const char *val = hdf_get_value (first);
				if (val && strcmp (val, key) == 0)
					return child;
			}
		}
	}
	return -1;
}

const char *hdf_get_property (int h, const char *key)
{
	int node = hdf_find_node (h, key);
	if (node <= 0)
		return NULL;
	int children = hdf_get_children (node);
	if (m_len (children) < 2)
		return NULL;
	return hdf_get_value (INT (children, 1));
}

int hdf_get_int (int h, const char *key, int default_val)
{
	const char *val = hdf_get_property (h, key);
	if (!val)
		return default_val;
	return atoi (val);
}

int hdf_get_bool (int h, const char *key, int default_val)
{
	const char *val = hdf_get_property (h, key);
	if (!val)
		return default_val;
	if (strcmp (val, "true") == 0)
		return 1;
	if (strcmp (val, "false") == 0)
		return 0;
	return default_val;
}
