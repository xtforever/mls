#ifndef TABLE_H
#define TABLE_H

#include "list.h"

/* Sorted key->value table built on the universal list structure.
 *
 * A table is a list node whose children are (key value) entry nodes, kept
 * sorted by key so lookups are O(log n). Keys are strings; a value is either
 * a string or an owned mls handle (list, buffer, ...). The table frees all
 * keys and owned values on tbl_free. */

int tbl_create (void);
void tbl_free (int t);
int tbl_is_table (int t);

/* Set an owned string value by C-string key. */
int tbl_set (int t, const char *key, const char *value);
/* Set an owned mls handle value by C-string key. */
int tbl_set_handle (int t, const char *key, int value_h);
/* Set an owned mls handle value by an existing string-handle key. The table
 * takes ownership of both key_h and value_h. */
int tbl_set_handle_str (int t, int key_h, int value_h);

const char *tbl_get (int t, const char *key); /* string value, NULL if absent */
int tbl_get_handle (int t, const char *key);  /* value handle, 0 if absent */
int tbl_del (int t, const char *key);	      /* 0 removed, -1 absent */

int tbl_len (int t);
const char *tbl_key (int t, int i);   /* i-th key, in sorted order */
const char *tbl_value (int t, int i); /* i-th string value */

#endif
