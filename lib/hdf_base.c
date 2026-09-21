#include "hdf_base.h"
#include "hdf_parse.h"

#include <string.h>

int hdf_base_read (const char *text) { return hdf_parse (text); }

int hdf_base_read_file (const char *path) { return hdf_parse_file (path); }

void hdf_base_free (int h) { list_free (h); }

hdf_base_type_t hdf_base_type (int h) { return (hdf_base_type_t)list_type (h); }

int hdf_base_children (int h) { return list_children (h); }

const char *hdf_base_str (int h) { return list_text (h); }

int hdf_base_find (int h, const char *key)
{
	int ch = list_children (h);
	if (ch <= 0)
		return 0;
	int i, *c;
	m_foreach (ch, i, c)
	{
		int child = *c;
		if (list_type (child) == LIST_LIST) {
			int sub = list_children (child);
			if (m_len (sub) > 0) {
				const char *v = list_text (list_get (child, 0));
				if (v && strcmp (v, key) == 0)
					return child;
			}
		}
	}
	return 0;
}

const char *hdf_base_prop (int h, const char *key)
{
	int node = hdf_base_find (h, key);
	if (node <= 0)
		return NULL;
	int ch = list_children (node);
	if (m_len (ch) < 2)
		return NULL;
	return list_text (list_get (node, 1));
}
