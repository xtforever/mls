#ifndef HDF_BASE_H
#define HDF_BASE_H

#include "list.h"

/* HDF reader API (unsorted data) built on the universal list structure.
 *
 * This is a thin layer over:
 *   - list.h      : the universal { type, data } node/list container
 *   - hdf_parse.h : the s-expression parser producing those nodes
 *
 * HDF keeps children in append order and looks up keys by linear scan
 * (hdf_base_find). For a sorted/binary-searched map, see table.h.
 *
 * The type/struct names below alias list.h so existing hdf_base callers
 * keep compiling unchanged. */

typedef list_type_t hdf_base_type_t;
typedef list_node_t hdf_base_node_t;

#define HDF_BASE_LIST LIST_LIST
#define HDF_BASE_STRING LIST_STRING
#define HDF_BASE_NUMBER LIST_NUMBER
#define HDF_BASE_BOOL LIST_BOOL
#define HDF_BASE_NULL LIST_NULL

int hdf_base_read (const char *text);
int hdf_base_read_file (const char *path);
void hdf_base_free (int h);
hdf_base_type_t hdf_base_type (int h);
int hdf_base_children (int h);
const char *hdf_base_str (int h);
int hdf_base_find (int h, const char *key);
const char *hdf_base_prop (int h, const char *key);

#endif
