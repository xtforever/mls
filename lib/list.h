#ifndef LIST_H
#define LIST_H

#include "mls_base.h"

/* Universal list/node structure — the base shared by the HDF reader (unsorted)
 * and the table API (sorted). Only mls_base is required.
 *
 * A node is one mls handle holding a single list_node_t:
 *
 *   struct { int type; int data; }
 *
 *   LIST_*  -> data is a handle to a child list (an mls int array of node
 *              handles)
 *   atoms   -> data is a handle to a char array (the text)
 *
 * Everything is linked by int handles; no pointers escape. */

typedef enum {
	LIST_LIST,
	LIST_STRING,
	LIST_NUMBER,
	LIST_BOOL,
	LIST_NULL,
	LIST_HANDLE, /* owned mls handle (not text); m_free'd by list_free */
} list_type_t;

typedef struct {
	int type;
	int data;
} list_node_t;

/* Raw constructor: a node of `type` whose data handle is `data`. */
int list_node (list_type_t type, int data);

/* New empty LIST node. */
int list_new (void);

/* Raw char-array handle: a copy of `len` bytes of s, NUL-terminated. */
int list_str (const char *s, int len);

/* New STRING node, copying s. */
int list_string (const char *s);

/* Recursively free a node and everything it refers to. */
void list_free (int h);

list_type_t list_type (int h);
const char *list_text (int h); /* atom text, NULL for LIST/NULL */
int list_data (
	int h); /* raw data handle (STRING/HANDLE/...); 0 for LIST/NULL */
int list_children (int h); /* child-list handle, 0 if not a list */

int list_len (int h);		 /* number of children */
int list_get (int h, int i);	 /* i-th child handle */
int list_add (int h, int child); /* append child, returns index */

#endif
