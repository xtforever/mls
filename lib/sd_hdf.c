#include "sd_hdf.h"
#include "sd_thread.h"
#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static __thread sd_hdf_error_t g_error = {0, "", "", 0, NULL};

static void set_error (int code, const char *msg, int line, const char *field)
{
	g_error.code = code;
	snprintf (g_error.message, sizeof (g_error.message), "%s", msg);
	g_error.line = line;
	g_error.field_name = field;
}

void sd_hdf_clear_error (void) { memset (&g_error, 0, sizeof (g_error)); }

sd_hdf_error_t *sd_hdf_get_error (void) { return &g_error; }

const char *sd_hdf_error_string (void)
{
	static char buf[512];
	if (g_error.field_name)
		snprintf (buf, sizeof (buf), "[%s:%d] %s (field: %s)",
			  g_error.node_path, g_error.line, g_error.message,
			  g_error.field_name);
	else if (g_error.node_path[0])
		snprintf (buf, sizeof (buf), "[%s:%d] %s", g_error.node_path,
			  g_error.line, g_error.message);
	else if (g_error.line > 0)
		snprintf (buf, sizeof (buf), "[line %d] %s", g_error.line,
			  g_error.message);
	else
		snprintf (buf, sizeof (buf), "%s",
			  g_error.message[0] ? g_error.message
					     : "unknown error");
	return buf;
}

static int create_node (hdf_node_type_t type, int value)
{
	int n = sd_alloc (2, sizeof (int), 0);
	int *data = sd_buf (n);
	data[0] = (int)type;
	data[1] = value;
	sd_setlen (n, 2);
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
	p++;
	int equals = 0;
	while (*p == '=') {
		equals++;
		p++;
	}
	if (*p != '[')
		return NULL;
	p++;

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
				int s_h = sd_alloc (len + 1, 1, SD_FREE);
				char *buf = sd_buf (s_h);
				memcpy (buf, start, len);
				buf[len] = 0;
				sd_setlen (s_h, len + 1);
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
	p++;
	int s_h = sd_new ();
	while (*p && *p != '"') {
		if (*p == '\\') {
			p++;
			switch (*p) {
			case 'n':
				sd_put (s_h, &(char){'\n'});
				break;
			case 'r':
				sd_put (s_h, &(char){'\r'});
				break;
			case 't':
				sd_put (s_h, &(char){'\t'});
				break;
			case '\\':
				sd_put (s_h, &(char){'\\'});
				break;
			case '"':
				sd_put (s_h, &(char){'"'});
				break;
			default:
				sd_put (s_h, p);
				break;
			}
		} else {
			sd_put (s_h, p);
		}
		p++;
	}
	if (*p == '"')
		p++;
	sd_put (s_h, &(char){0});
	*node = create_node (HDF_TYPE_STRING, s_h);
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
		*node = create_node (HDF_TYPE_BOOLEAN, sd_strdup ("true"));
	} else if (strcmp (buf, "false") == 0) {
		*node = create_node (HDF_TYPE_BOOLEAN, sd_strdup ("false"));
	} else if (strcmp (buf, "null") == 0) {
		*node = create_node (HDF_TYPE_NULL, 0);
	} else {
		char *endptr;
		strtod (buf, &endptr);
		if (*endptr == '\0') {
			*node = create_node (HDF_TYPE_NUMBER, sd_strdup (buf));
		} else {
			*node = create_node (HDF_TYPE_STRING, sd_strdup (buf));
		}
	}
	free (buf);
	return p;
}

static const char *parse_list (const char *p, int *node)
{
	p++;
	int children = sd_alloc (0, sizeof (int), SD_FREE);
	p = skip_ws (p);
	while (*p && *p != ')') {
		int child;
		p = parse_element (p, &child);
		if (!p) {
			sd_free (children);
			return NULL;
		}

		if (child != -2 && hdf_get_type (child) == HDF_TYPE_STRING) {
			const char *val = hdf_get_value (child);
			if (sd_len (children) == 0 && val &&
			    strcmp (val, "rem") == 0) {
				hdf_free (child);
				int depth = 1;
				while (*p && depth > 0) {
					if (*p == '(')
						depth++;
					else if (*p == ')')
						depth--;
					p++;
				}
				sd_free (children);
				*node = -2;
				return p;
			}
		}

		if (child != -2) {
			int *pchild = sd_put (children, &child);
			(void)pchild;
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
	int root_list = sd_alloc (0, sizeof (int), SD_FREE);
	p = skip_ws (p);
	while (*p) {
		int node;
		p = parse_element (p, &node);
		if (!p)
			break;
		if (node != -2) {
			int *pnode = sd_put (root_list, &node);
			(void)pnode;
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
	int *data = sd_buf (h);
	hdf_node_type_t type = (hdf_node_type_t)data[0];
	int val = data[1];
	if (type == HDF_TYPE_LIST) {
		int *children = sd_buf (val);
		for (int i = 0; i < sd_len (val); i++) {
			hdf_free (children[i]);
		}
		sd_free (val);
	} else if (val > 0) {
		sd_free (val);
	}
	sd_free (h);
}

hdf_node_type_t hdf_get_type (int h)
{
	if (h <= 0)
		return HDF_TYPE_NULL;
	int *data = sd_buf (h);
	return (hdf_node_type_t)data[0];
}

const char *hdf_get_value (int h)
{
	if (h <= 0)
		return NULL;
	hdf_node_type_t type = hdf_get_type (h);
	if (type == HDF_TYPE_LIST)
		return NULL;
	if (type == HDF_TYPE_NULL)
		return "null";
	int *data = sd_buf (h);
	int val = data[1];
	return val > 0 ? sd_buf (val) : NULL;
}

int hdf_get_children (int h)
{
	if (hdf_get_type (h) != HDF_TYPE_LIST)
		return -1;
	int *data = sd_buf (h);
	return data[1];
}

static int find_node_recursive (int h, const char *key)
{
	int children = hdf_get_children (h);
	if (children <= 0)
		return -1;
	int *child_data = sd_buf (children);
	for (int i = 0; i < sd_len (children); i++) {
		int child = child_data[i];
		if (hdf_get_type (child) == HDF_TYPE_LIST) {
			int sub_children = hdf_get_children (child);
			if (sd_len (sub_children) > 0) {
				int *sub_data = sd_buf (sub_children);
				int first = sub_data[0];
				const char *val = hdf_get_value (first);
				if (val && strcmp (val, key) == 0)
					return child;
				int found = find_node_recursive (child, key);
				if (found > 0)
					return found;
			}
		}
	}
	return -1;
}

int hdf_find_node (int h, const char *key)
{
	return find_node_recursive (h, key);
}

const char *hdf_get_property (int h, const char *key)
{
	int children = hdf_get_children (h);
	if (children <= 0)
		return NULL;
	int len = sd_len (children);
	if (len < 2)
		return NULL;
	int *data = sd_buf (children);
	for (int i = 0; i < len; i++) {
		int child = data[i];
		if (hdf_get_type (child) == HDF_TYPE_LIST) {
			int prop_children = hdf_get_children (child);
			if (sd_len (prop_children) >= 2) {
				int *pc = sd_buf (prop_children);
				int key_type = hdf_get_type (pc[0]);
				if (key_type == HDF_TYPE_STRING ||
				    key_type == HDF_TYPE_NUMBER) {
					const char *k = hdf_get_value (pc[0]);
					if (k && strcmp (k, key) == 0) {
						int val_type =
							hdf_get_type (pc[1]);
						if (val_type ==
							    HDF_TYPE_STRING ||
						    val_type ==
							    HDF_TYPE_NUMBER ||
						    val_type ==
							    HDF_TYPE_BOOLEAN ||
						    val_type == HDF_TYPE_NULL) {
							return hdf_get_value (
								pc[1]);
						}
					}
				}
			}
		}
	}
	return NULL;
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

int hdf_set_property (int h, const char *key, const char *value)
{
	int node = hdf_find_node (h, key);
	if (node > 0) {
		int children = hdf_get_children (node);
		if (sd_len (children) >= 2) {
			int *data = sd_buf (children);
			int val_node = data[1];
			if (hdf_get_type (val_node) == HDF_TYPE_STRING) {
				sd_sfree (val_node);
			}
			int new_val = sd_strdup (value);
			data[1] = new_val;
			return 0;
		}
	}

	return hdf_add_property (h, key, value);
}

int hdf_set_int (int h, const char *key, int value)
{
	char buf[32];
	snprintf (buf, sizeof (buf), "%d", value);
	return hdf_set_property (h, key, buf);
}

int hdf_set_bool (int h, const char *key, int value)
{
	return hdf_set_property (h, key, value ? "true" : "false");
}

int hdf_add_child (int h, const char *name)
{
	int name_node = create_node (HDF_TYPE_STRING, sd_strdup (name));
	int children_list = sd_alloc (0, sizeof (int), SD_FREE);
	int node = create_node (HDF_TYPE_LIST, children_list);

	int *p = sd_buf (children_list);
	p[0] = name_node;
	sd_setlen (children_list, 1);

	int children = hdf_get_children (h);
	int *np = sd_put (children, &node);
	(void)np;

	return node;
}

int hdf_add_property (int h, const char *key, const char *value)
{
	int key_node = create_node (HDF_TYPE_STRING, sd_strdup (key));
	int val_node = create_node (HDF_TYPE_STRING, sd_strdup (value));

	int children_list = sd_alloc (0, sizeof (int), SD_FREE);
	int *p = sd_buf (children_list);
	p[0] = key_node;
	p[1] = val_node;
	sd_setlen (children_list, 2);

	int prop_node = create_node (HDF_TYPE_LIST, children_list);

	int children = hdf_get_children (h);
	sd_put (children, &prop_node);

	return prop_node;
}
