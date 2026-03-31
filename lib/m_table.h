#ifndef M_TABLE_H
#define M_TABLE_H

#include "m_tool.h" // For s_cstr
#include "mls.h"

// Bit flag for types that should be freed by the table.
#define MLS_TABLE_FREE 0x80
#define MLS_TABLE_TYPE_MASK 0x7F

// Helper macros for type introspection
#define m_table_is_free(t) ((t) & MLS_TABLE_FREE)
#define m_table_type(t) ((t) & MLS_TABLE_TYPE_MASK)
#define m_table_is_str_key(t)                                                  \
	(m_table_type (t) == m_table_type (MLS_TABLE_TYPE_STRING) ||           \
	 m_table_type (t) == m_table_type (MLS_TABLE_TYPE_CONST_STRING))

// Enum for types stored in the table for introspection.
typedef enum {
	MLS_TABLE_TYPE_UNKNOWN = 0,
	MLS_TABLE_TYPE_INT = 1,
	MLS_TABLE_TYPE_STRING = 2 | MLS_TABLE_FREE,
	MLS_TABLE_TYPE_CONST_STRING = 3,
	MLS_TABLE_TYPE_LIST = 4 | MLS_TABLE_FREE,
	MLS_TABLE_TYPE_TABLE = 5 | MLS_TABLE_FREE,
	MLS_TABLE_TYPE_CUSTOM_HANDLE = 6 | MLS_TABLE_FREE
} mls_table_type_t;

// --- Table Management ---
// Creates a new empty table handle.
// Returns a table handle (int) or 0 on error.
int m_table_create ();

// Frees the table and its contents recursively.
// Cleans up all string handles, list handles, and nested table handles.
void m_table_free (int table_h);

// --- Setting Values ---

// Sets a raw integer value by integer key.
void m_table_set_int_val_by_int (int table_h, int key_idx, int raw_int_value);
// Sets a raw C-string value by integer key (creates a dynamic MLS string).
void m_table_set_string_by_int (int table_h, int key_idx,
				const char *raw_c_string);
// Sets a constant C-string value by integer key (uses s_cstr).
void m_table_set_const_string_by_int (int table_h, int key_idx,
				      const char *raw_c_string);
// Sets an MLS list handle by integer key.
void m_table_set_list_by_int (int table_h, int key_idx, int list_h);
// Sets another MLS table handle by integer key.
void m_table_set_table_by_int (int table_h, int key_idx, int nested_table_h);
// Generic setter for int keys, for custom handles or types.
void m_table_set_handle_by_int (int table_h, int key_idx, int value_h,
				mls_table_type_t type);

// Sets a raw integer value by C-string key.
void m_table_set_int_val_by_cstr (int table_h, const char *key_cstr,
				  int raw_int_value);
// Sets a raw C-string value by C-string key (creates a dynamic MLS string).
void m_table_set_string_by_cstr (int table_h, const char *key_cstr,
				 const char *raw_c_string);
// Sets a constant C-string value by C-string key (uses s_cstr).
void m_table_set_const_string_by_cstr (int table_h, const char *key_cstr,
				       const char *raw_c_string);
// Sets an MLS list handle by C-string key.
void m_table_set_list_by_cstr (int table_h, const char *key_cstr, int list_h);
// Sets another MLS table handle by C-string key.
void m_table_set_table_by_cstr (int table_h, const char *key_cstr,
				int nested_table_h);
// Generic setter for cstr keys, for custom handles or types.
void m_table_set_handle_by_cstr (int table_h, const char *key_cstr, int value_h,
				 mls_table_type_t type);

// Sets a raw integer value by MLS string handle key.
void m_table_set_int_val_by_str (int table_h, int key_str_h, int raw_int_value);
// Sets a raw C-string value by MLS string handle key (creates a dynamic MLS
// string).
void m_table_set_string_by_str (int table_h, int key_str_h,
				const char *raw_c_string);
// Sets a constant C-string value by MLS string handle key (uses s_cstr).
void m_table_set_const_string_by_str (int table_h, int key_str_h,
				      const char *raw_c_string);
// Sets an MLS list handle by MLS string handle key.
void m_table_set_list_by_str (int table_h, int key_str_h, int list_h);
// Sets another MLS table handle by MLS string handle key.
void m_table_set_table_by_str (int table_h, int key_str_h, int nested_table_h);
// Generic setter for str handle keys, for custom handles or types.
void m_table_set_handle_by_str (int table_h, int key_str_h, int value_h,
				mls_table_type_t type);

// Old generic setters (still available for advanced/flexible usage)
// 'value' is the handle to the actual data (string, list, table) or a raw int.
// 'type' specifies the type of the value for introspection.
void m_table_set_str_key (int table_h, int key_str_h, int value,
			  mls_table_type_t type);
void m_table_set_str_key_ext (int table_h, int key_str_h,
			      mls_table_type_t key_type, int value,
			      mls_table_type_t type);
void m_table_set_cstr_key (int table_h, const char *key_cstr, int value,
			   mls_table_type_t type);

// --- Getting Values ---

// Gets the value (handle or raw int) by integer key.
// Returns 0 if key not found or error.
int m_table_get_int (int table_h, int key_idx);

// Gets the value (handle or raw int) by string key handle.
// Returns 0 if key not found or error.
int m_table_get_str (int table_h, int key_str_h);

// Convenience function to get a value by C-string key.
// Returns 0 if key not found or error.
int m_table_get_cstr (int table_h, const char *key_cstr);

// --- Introspection ---

// Returns a handle to an MLS list containing the handles of all keys in the
// table. For int keys, it's the raw int. For string keys, it's the string
// handle. User is responsible for freeing the returned list handle
// (m_free(keys)).
int m_table_keys (int table_h);

// Gets the type of the value stored at an integer key.
mls_table_type_t m_table_get_type_int (int table_h, int key_idx);

// Gets the type of the value stored at a string key handle.
mls_table_type_t m_table_get_type_str (int table_h, int key_str_h);

// Convenience function to get the type of the value stored at a C-string key.
mls_table_type_t m_table_get_type_cstr (int table_h, const char *key_cstr);

// --- Simplified API (mt_ prefix) ---

// Sets an integer value by C-string key.
void mt_seti (int table_h, const char *key, int val);
// Sets a dynamic MLS string by C-string key (duplicates the C-string).
void mt_sets (int table_h, const char *key, const char *val);
// Sets a constant MLS string by C-string key (uses s_cstr).
void mt_setc (int table_h, const char *key, const char *val);
// Sets an MLS handle (list, table, etc.) by C-string key.
void mt_seth (int table_h, const char *key, int handle, mls_table_type_t type);
// Gets a value (raw int or handle) by C-string key.
int mt_get (int table_h, const char *key);

#endif // M_TABLE_H
