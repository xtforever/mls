#include "list.h"

#include <string.h>

int list_str (const char *s, int len)
{
	int h = m_create ((size_t)len + 1, 1);
	if (len > 0)
		memcpy (m_buf (h), s, (size_t)len);
	((char *)m_buf (h))[len] = 0;
	m_setlen (h, (size_t)len + 1);
	return h;
}

int list_node (list_type_t type, int data)
{
	list_node_t n = {(int)type, data};
	int h = m_create (0, sizeof (list_node_t));
	m_put (h, &n);
	return h;
}

int list_new (void)
{
	int children = m_create (0, sizeof (int));
	return list_node (LIST_LIST, children);
}

int list_string (const char *s)
{
	return list_node (LIST_STRING, list_str (s, (int)strlen (s)));
}

void list_free (int h)
{
	if (h <= 0)
		return;
	list_node_t *n = mls (h, 0);
	if (n->type == LIST_LIST) {
		int ch = n->data;
		int i, *c;
		m_foreach (ch, i, c) list_free (*c);
		m_free (ch);
	} else if (n->data > 0) {
		m_free (n->data);
	}
	m_free (h);
}

list_type_t list_type (int h)
{
	if (h <= 0)
		return LIST_NULL;
	return (list_type_t)((list_node_t *)mls (h, 0))->type;
}

const char *list_text (int h)
{
	list_type_t t = list_type (h);
	if (t == LIST_LIST || t == LIST_NULL || t == LIST_HANDLE)
		return NULL;
	int d = ((list_node_t *)mls (h, 0))->data;
	return d > 0 ? (const char *)m_buf (d) : NULL;
}

int list_data (int h)
{
	list_type_t t = list_type (h);
	if (t == LIST_LIST || t == LIST_NULL)
		return 0;
	return ((list_node_t *)mls (h, 0))->data;
}

int list_children (int h)
{
	if (list_type (h) != LIST_LIST)
		return 0;
	return ((list_node_t *)mls (h, 0))->data;
}

int list_len (int h)
{
	int ch = list_children (h);
	return ch > 0 ? (int)m_len (ch) : 0;
}

int list_get (int h, int i)
{
	int ch = list_children (h);
	return ch > 0 ? INT (ch, i) : 0;
}

int list_add (int h, int child)
{
	int ch = list_children (h);
	if (ch <= 0)
		return -1;
	return m_put (ch, &child);
}
