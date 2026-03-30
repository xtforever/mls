#ifndef M_HDF_H
#define M_HDF_H

#include "mls.h"

typedef enum {
    HDF_TYPE_LIST,
    HDF_TYPE_STRING,
    HDF_TYPE_NUMBER,
    HDF_TYPE_BOOLEAN,
    HDF_TYPE_NULL
} hdf_node_type_t;

/**
 * @brief Parse an HDF string into an mls tree.
 * @param data The HDF string to parse.
 * @return Handle to the root list, or -1 on error.
 */
int hdf_parse_string(const char *data);

/**
 * @brief Parse an HDF file into an mls tree.
 * @param path Path to the HDF file.
 * @return Handle to the root list, or -1 on error.
 */
int hdf_parse_file(const char *path);

/**
 * @brief Free an HDF tree.
 * @param h Handle to the HDF tree root.
 */
void hdf_free(int h);

/**
 * @brief Get the type of an HDF node.
 * @param h Node handle.
 * @return The type of the node.
 */
hdf_node_type_t hdf_get_type(int h);

/**
 * @brief Get the string value of a node (if it's a string/number/boolean).
 * @param h Node handle.
 * @return Pointer to the string, or NULL if not a value node.
 */
const char *hdf_get_value(int h);

/**
 * @brief Get the children of a list node.
 * @param h Node handle.
 * @return Handle to the mls list of children, or -1 if not a list.
 */
int hdf_get_children(int h);

/**
 * @brief Find a child node by its first element (keyword).
 * Useful for (keyword ... ) forms.
 * @param h Node handle (list).
 * @param key The keyword to look for.
 * @return Handle to the matching node, or -1 if not found.
 */
int hdf_find_node(int h, const char *key);

/**
 * @brief Get the value of a property node: (key value).
 * @param h Node handle (list).
 * @param key The keyword.
 * @return String value of the second element, or NULL.
 */
const char *hdf_get_property(int h, const char *key);

/**
 * @brief Get the integer value of a property node.
 * @param h Node handle (list).
 * @param key The keyword.
 * @param default_val Value to return if property is missing.
 * @return Integer value.
 */
int hdf_get_int(int h, const char *key, int default_val);

/**
 * @brief Get the boolean value of a property node.
 * @param h Node handle (list).
 * @param key The keyword.
 * @param default_val Value to return if property is missing.
 * @return 1 for true, 0 for false.
 */
int hdf_get_bool(int h, const char *key, int default_val);

#endif
