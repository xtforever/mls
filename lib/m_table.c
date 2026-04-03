#include "m_table.h"
#include "m_tool.h"
#include "mls.h"
#include <stdio.h> // For TRACE
#include <string.h>

// --- Internal Data Structures ---

// A single entry in the table
typedef struct m_table_entry {
	int key;   // If is_str, this is a handle to the string. Else, it's the
		   // raw int key.
	uint64_t value; // Handle to actual data (string, list, table), raw int or pointer.
	unsigned char type;	// mls_table_type_t of the 'value'.
	unsigned char key_type; // mls_table_type_t of the 'key'.
} m_table_entry_t;

// --- Internal Helpers ---

static int m_table_entry_cmp (const void *a, const void *b)
{
	const m_table_entry_t *ea = a;
	const m_table_entry_t *eb = b;

	int a_is_str = m_table_is_str_key (ea->key_type);
	int b_is_str = m_table_is_str_key (eb->key_type);

	if (a_is_str != b_is_str) {
		return a_is_str - b_is_str;
	}

	if (a_is_str) {
		return strcmp (m_str (ea->key), m_str (eb->key));
	} else {
		return ea->key - eb->key;
	}
}

static int m_table_entry_cmp_cstr (const void *key, const void *element)
{
	const char *search_cstr = key;
	const m_table_entry_t *entry = element;

	int entry_is_str = m_table_is_str_key (entry->key_type);
	if (!entry_is_str)
		return 1; // Ints are "lower" than strings in our cmp

	return strcmp (search_cstr, m_str (entry->key));
}

// Custom free handler for the table itself
static void m_table_free_handler ( int m )
{
	m_table_entry_t *entry;
	int p;
	TRACE (1, "m_table_free_handler called");
	m_foreach( m, p, entry ) {
		if (m_table_is_free (entry->type) && entry->value > 0 && entry->value < 0xFFFFFFFF && !m_is_freed((int)entry->value)) {
			m_free ((int)entry->value);
		}
		if (m_table_is_free (entry->key_type) && entry->key > 0 && !m_is_freed(entry->key)) {
			m_free (entry->key);
		}
	}
}


// Global variable for the custom free handler ID
static int MFREE_TABLE_ENTRIES_HDLR = 0;

// --- Table Management ---

/**
 * Creates a new empty table handle.
 * A table stores key-value pairs where keys can be integers or strings, 
 * and values can be various types (ints, strings, lists, other tables).
 *
 * @return The handle of the new table.
 */
int m_table_create ()
{
	if (!MFREE_TABLE_ENTRIES_HDLR) {
		MFREE_TABLE_ENTRIES_HDLR = m_reg_freefn ( m_table_free_handler );
	}

	return m_alloc (0, sizeof (m_table_entry_t), MFREE_TABLE_ENTRIES_HDLR);
}

/**
 * Checks if a handle refers to a table.
 *
 * @param table_h The handle to check.
 * @return 1 if it is a table, 0 otherwise.
 */
int m_is_table (int table_h)
{
	if (table_h <= 0 || m_is_freed (table_h))
		return 0;
	return m_free_hdl (table_h) == MFREE_TABLE_ENTRIES_HDLR;
}

/**
 * Frees a table and all its contents recursively.
 *
 * @param table_h The handle of the table to free.
 */
void m_table_free (int table_h) { m_free (table_h); }

// --- Internal Lookup Helpers ---

static m_table_entry_t *m_table_find_int_key_entry (int table_h, int key_idx)
{
	m_table_entry_t search = {.key = key_idx,
				  .key_type = MLS_TABLE_TYPE_INT};
	int p = m_bsearch (&search, table_h, m_table_entry_cmp);
	return p >= 0 ? mls (table_h, p) : NULL;
}

static m_table_entry_t *m_table_find_str_key_entry (int table_h, int key_str_h)
{
	m_table_entry_t search = {.key = key_str_h,
				  .key_type = MLS_TABLE_TYPE_STRING};
	int p = m_bsearch (&search, table_h, m_table_entry_cmp);
	return p >= 0 ? mls (table_h, p) : NULL;
}

static m_table_entry_t *m_table_find_cstr_key_entry (int table_h,
						     const char *key_cstr)
{
	int p = m_bsearch (key_cstr, table_h, m_table_entry_cmp_cstr);
	return p >= 0 ? mls (table_h, p) : NULL;
}

/**
 * Removes a table entry by string handle key.
 * Frees both the key (if dynamic) and the value (if it's an MLS handle).
 *
 * @param table_h The handle of the table.
 * @param key_str_h The handle of the string key to remove.
 */
void m_table_remove_by_str(int table_h, int key_str_h)
{
    if (table_h <= 0 || key_str_h <= 0 || m_is_freed(key_str_h)) return;
    
    // 1. Try to find the exact handle first
    m_table_entry_t search = { .key = key_str_h, .key_type = MLS_TABLE_TYPE_STRING };
    int p = m_bsearch(&search, table_h, m_table_entry_cmp);
    
    // 2. If handle not found, try searching by C-string content
    if (p < 0) {
        const char *ks = m_str(key_str_h);
        p = m_bsearch(ks, table_h, m_table_entry_cmp_cstr);
    }

    if (p >= 0) {
        m_table_entry_t *entry = mls(table_h, p);
        if (m_table_is_free (entry->type) && entry->value > 0 && entry->value < 0xFFFFFFFF && !m_is_freed((int)entry->value)) {
            m_free ((int)entry->value);
        }
        if (m_table_is_free (entry->key_type) && entry->key > 0 && !m_is_freed(entry->key)) {
            m_free (entry->key);
        }
        m_del(table_h, p);
    }
}

static void m_table_set_entry (int table_h, m_table_entry_t *new_data)
{
	if (table_h <= 0)
		return;
	int p = m_binsert (table_h, new_data, m_table_entry_cmp, 0);
	if (p < 0) {
		// Entry already exists at pos (-p)-1
		int pos = (-p) - 1;
		m_table_entry_t *entry = mls (table_h, pos);

		// Free old value if needed
		if (m_table_is_free (entry->type) && entry->value > 0 && entry->value < 0xFFFFFFFF && !m_is_freed((int)entry->value)) {
			m_free ((int)entry->value);
		}

		// Key management: ensure we don't leak or use-after-free
		if (entry->key != new_data->key) {
			if (!m_table_is_free (new_data->key_type)) {
				// New key is NOT owned. Use it and free old if
				// it was owned.
				if (m_table_is_free (entry->key_type) &&
				    entry->key > 0) {
					m_free (entry->key);
				}
			} else {
				// New key IS owned.
				// If old key exists (owned or not), prefer it
				// to avoid churn.
				m_free (new_data->key);
				new_data->key = entry->key;
				new_data->key_type = entry->key_type;
			}
		}

		*entry = *new_data;
	}
}

// --- Generic Setters ---

/**
 * Sets a value for an integer key in the table.
 *
 * @param table_h Handle of the table.
 * @param key_idx Integer key.
 * @param value Value (raw int or handle).
 * @param type Type of the value.
 */
void m_table_set_int_key (int table_h, int key_idx, uint64_t value,
			  mls_table_type_t type)
{
	if (table_h <= 0)
		return;
	m_table_entry_t entry = {.key = key_idx,
				 .value = value,
				 .type = type,
				 .key_type = MLS_TABLE_TYPE_INT};
	m_table_set_entry (table_h, &entry);
}

/**
 * Sets a value for a string handle key with a specific key type.
 *
 * @param table_h Handle of the table.
 * @param key_str_h Handle of the key string.
 * @param key_type Type of the key (e.g. MLS_TABLE_TYPE_STRING or MLS_TABLE_TYPE_CONST_STRING).
 * @param value Value (raw int or handle).
 * @param type Type of the value.
 */
void m_table_set_str_key_ext (int table_h, int key_str_h,
			      mls_table_type_t key_type, uint64_t value,
			      mls_table_type_t type)
{
	if (table_h <= 0 || key_str_h <= 0)
		return;
	m_table_entry_t entry = {.key = key_str_h,
				 .value = value,
				 .type = type,
				 .key_type = key_type};
	m_table_set_entry (table_h, &entry);
}

/**
 * Sets a value for a string handle key (defaults key type to MLS_TABLE_TYPE_STRING).
 *
 * @param table_h Handle of the table.
 * @param key_str_h Handle of the key string.
 * @param value Value (raw int or handle).
 * @param type Type of the value.
 */
void m_table_set_str_key (int table_h, int key_str_h, uint64_t value,
			  mls_table_type_t type)
{
	m_table_set_str_key_ext (table_h, key_str_h, MLS_TABLE_TYPE_STRING,
				 value, type);
}

/**
 * Sets a value for a C-style string key.
 *
 * @param table_h Handle of the table.
 * @param key_cstr The C-style string key.
 * @param value Value (raw int or handle).
 * @param type Type of the value.
 */
void m_table_set_cstr_key (int table_h, const char *key_cstr, uint64_t value,
			   mls_table_type_t type)
{
	if (table_h <= 0 || !key_cstr)
		return;

	m_table_entry_t new_entry = {.key = s_strdup_c (key_cstr),
				     .value = value,
				     .type = type,
				     .key_type = MLS_TABLE_TYPE_STRING};
	m_table_set_entry (table_h, &new_entry);
}

// --- New Simplified API (mt_ prefix) ---

/**
 * Sets an integer value for a C-string key.
 *
 * @param table_h Handle of the table.
 * @param key Key name.
 * @param val Integer value.
 */
void mt_seti (int table_h, const char *key, int64_t val)
{
	m_table_set_cstr_key (table_h, key, (uint64_t)val, MLS_TABLE_TYPE_INT);
}

/**
 * Sets a string value for a C-string key.
 *
 * @param table_h Handle of the table.
 * @param key Key name.
 * @param val String value.
 */
void mt_sets (int table_h, const char *key, const char *val)
{
	m_table_set_cstr_key (table_h, key, (uint64_t)s_strdup_c (val),
			      MLS_TABLE_TYPE_STRING);
}

/**
 * Sets a constant string value for a C-string key.
 *
 * @param table_h Handle of the table.
 * @param key Key name.
 * @param val String value.
 */
void mt_setc (int table_h, const char *key, const char *val)
{
	m_table_set_cstr_key (table_h, key, (uint64_t)s_cstr (val),
			      MLS_TABLE_TYPE_CONST_STRING);
}

/**
 * Sets a handle value for a C-string key.
 *
 * @param table_h Handle of the table.
 * @param key Key name.
 * @param handle The handle to store.
 * @param type The type of the handle.
 */
void mt_seth (int table_h, const char *key, uint64_t handle, mls_table_type_t type)
{
	m_table_set_cstr_key (table_h, key, handle, type);
}

/**
 * Gets a value from the table by C-string key.
 *
 * @param table_h Handle of the table.
 * @param key Key name.
 * @return The value (int or handle).
 */
uint64_t mt_get (int table_h, const char *key)
{
	return m_table_get_cstr (table_h, key);
}

void mt_setp(int table_h, const char *key, void *ptr)
{
	m_table_set_cstr_key(table_h, key, (uint64_t)(uintptr_t)ptr, MLS_TABLE_TYPE_PTR);
}

void *mt_getp(int table_h, const char *key)
{
	return (void *)(uintptr_t)m_table_get_cstr(table_h, key);
}

// --- Old API Forwarding ---

void m_table_set_int_val_by_int (int table_h, int key_idx, int64_t raw_int_value)
{
	m_table_set_int_key (table_h, key_idx, (uint64_t)raw_int_value,
			     MLS_TABLE_TYPE_INT);
}
void m_table_set_string_by_int (int table_h, int key_idx,
				const char *raw_c_string)
{
	m_table_set_int_key (table_h, key_idx, (uint64_t)s_strdup_c (raw_c_string),
			     MLS_TABLE_TYPE_STRING);
}
void m_table_set_const_string_by_int (int table_h, int key_idx,
				      const char *raw_c_string)
{
	m_table_set_int_key (table_h, key_idx, (uint64_t)s_cstr (raw_c_string),
			     MLS_TABLE_TYPE_CONST_STRING);
}
void m_table_set_list_by_int (int table_h, int key_idx, int list_h)
{
	m_table_set_int_key (table_h, key_idx, (uint64_t)list_h, MLS_TABLE_TYPE_LIST);
}
void m_table_set_table_by_int (int table_h, int key_idx, int nested_table_h)
{
	m_table_set_int_key (table_h, key_idx, (uint64_t)nested_table_h,
			     MLS_TABLE_TYPE_TABLE);
}
void m_table_set_handle_by_int (int table_h, int key_idx, uint64_t value_h,
				mls_table_type_t type)
{
	m_table_set_int_key (table_h, key_idx, value_h, type);
}

void m_table_set_int_val_by_cstr (int table_h, const char *key_cstr,
				  int64_t raw_int_value)
{
	m_table_set_cstr_key (table_h, key_cstr, (uint64_t)raw_int_value,
			      MLS_TABLE_TYPE_INT);
}
void m_table_set_string_by_cstr (int table_h, const char *key_cstr,
				 const char *raw_c_string)
{
	m_table_set_cstr_key (table_h, key_cstr, (uint64_t)s_strdup_c (raw_c_string),
			      MLS_TABLE_TYPE_STRING);
}
void m_table_set_const_string_by_cstr (int table_h, const char *key_cstr,
				       const char *raw_c_string)
{
	m_table_set_cstr_key (table_h, key_cstr, (uint64_t)s_cstr (raw_c_string),
			      MLS_TABLE_TYPE_CONST_STRING);
}
void m_table_set_list_by_cstr (int table_h, const char *key_cstr, int list_h)
{
	m_table_set_cstr_key (table_h, key_cstr, (uint64_t)list_h, MLS_TABLE_TYPE_LIST);
}
void m_table_set_table_by_cstr (int table_h, const char *key_cstr,
				int nested_table_h)
{
	m_table_set_cstr_key (table_h, key_cstr, (uint64_t)nested_table_h,
			      MLS_TABLE_TYPE_TABLE);
}
void m_table_set_handle_by_cstr (int table_h, const char *key_cstr, uint64_t value_h,
				 mls_table_type_t type)
{
	m_table_set_cstr_key (table_h, key_cstr, value_h, type);
}

void m_table_set_int_val_by_str (int table_h, int key_str_h, int64_t raw_int_value)
{
	m_table_set_str_key (table_h, key_str_h, (uint64_t)raw_int_value,
			     MLS_TABLE_TYPE_INT);
}
void m_table_set_string_by_str (int table_h, int key_str_h,
				const char *raw_c_string)
{
	m_table_set_str_key (table_h, key_str_h, (uint64_t)s_strdup_c (raw_c_string),
			     MLS_TABLE_TYPE_STRING);
}
void m_table_set_const_string_by_str (int table_h, int key_str_h,
				      const char *raw_c_string)
{
	m_table_set_str_key (table_h, key_str_h, (uint64_t)s_cstr (raw_c_string),
			     MLS_TABLE_TYPE_CONST_STRING);
}
void m_table_set_list_by_str (int table_h, int key_str_h, int list_h)
{
	m_table_set_str_key (table_h, key_str_h, (uint64_t)list_h, MLS_TABLE_TYPE_LIST);
}
void m_table_set_table_by_str (int table_h, int key_str_h, int nested_table_h)
{
	m_table_set_str_key (table_h, key_str_h, (uint64_t)nested_table_h,
			     MLS_TABLE_TYPE_TABLE);
}
void m_table_set_handle_by_str (int table_h, int key_str_h, uint64_t value_h,
				mls_table_type_t type)
{
	m_table_set_str_key (table_h, key_str_h, value_h, type);
}

// --- Getting Values ---

/**
 * Gets a value from the table by integer key.
 *
 * @param table_h Handle of the table.
 * @param key_idx Integer key.
 * @return The value (int or handle), or 0 if not found.
 */
uint64_t m_table_get_int (int table_h, int key_idx)
{
	if (table_h <= 0)
		return 0;
	m_table_entry_t *entry = m_table_find_int_key_entry (table_h, key_idx);
	return entry ? entry->value : 0;
}

/**
 * Gets a value from the table by string handle key.
 *
 * @param table_h Handle of the table.
 * @param key_str_h String handle key.
 * @return The value (int or handle), or 0 if not found.
 */
uint64_t m_table_get_str (int table_h, int key_str_h)
{
	if (table_h <= 0)
		return 0;
	m_table_entry_t *entry =
		m_table_find_str_key_entry (table_h, key_str_h);
	return entry ? entry->value : 0;
}

/**
 * Gets a value from the table by C-string key.
 *
 * @param table_h Handle of the table.
 * @param key_cstr C-string key.
 * @return The value (int or handle), or 0 if not found.
 */
uint64_t m_table_get_cstr (int table_h, const char *key_cstr)
{
	if (table_h <= 0 || !key_cstr)
		return 0;
	int tmp_key = s_dup(key_cstr);
	m_table_entry_t *entry =
		m_table_find_str_key_entry (table_h, tmp_key);
	m_free(tmp_key);
	return entry ? entry->value : 0;
}

// --- Introspection ---

/**
 * Returns a list of all keys in the table.
 *
 * @param table_h Handle of the table.
 * @return Handle of the m-array containing the keys. 
 *         User is responsible for freeing the list (but not the keys themselves).
 */
int m_table_keys (int table_h)
{
	if (table_h <= 0)
		return 0;
	int keys_h = m_alloc (0, sizeof (int), MFREE);
	m_table_entry_t *entry;
	int p;
	m_foreach (table_h, p, entry) { m_put (keys_h, &entry->key); }
	return keys_h;
}

/**
 * Gets the type of a value by integer key.
 *
 * @param table_h Handle of the table.
 * @param key_idx Integer key.
 * @return The type of the value.
 */
mls_table_type_t m_table_get_type_int (int table_h, int key_idx)
{
	if (table_h <= 0)
		return MLS_TABLE_TYPE_UNKNOWN;
	m_table_entry_t *entry = m_table_find_int_key_entry (table_h, key_idx);
	return entry ? (mls_table_type_t)entry->type : MLS_TABLE_TYPE_UNKNOWN;
}

/**
 * Gets the type of a value by string handle key.
 *
 * @param table_h Handle of the table.
 * @param key_str_h String handle key.
 * @return The type of the value.
 */
mls_table_type_t m_table_get_type_str (int table_h, int key_str_h)
{
	if (table_h <= 0)
		return MLS_TABLE_TYPE_UNKNOWN;
	m_table_entry_t *entry =
		m_table_find_str_key_entry (table_h, key_str_h);
	return entry ? (mls_table_type_t)entry->type : MLS_TABLE_TYPE_UNKNOWN;
}

/**
 * Gets the type of a value by C-string key.
 *
 * @param table_h Handle of the table.
 * @param key_cstr C-string key.
 * @return The type of the value.
 */
mls_table_type_t m_table_get_type_cstr (int table_h, const char *key_cstr)
{
	if (table_h <= 0 || !key_cstr)
		return MLS_TABLE_TYPE_UNKNOWN;
	m_table_entry_t *entry =
		m_table_find_cstr_key_entry (table_h, key_cstr);
	return entry ? (mls_table_type_t)entry->type : MLS_TABLE_TYPE_UNKNOWN;
}
