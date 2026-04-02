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
	int value; // Handle to actual data (string, list, table) or a raw int.
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
static void m_table_free_handler (lst_t l)
{
	m_table_entry_t *entry;
	int p = -1;
	TRACE (1, "m_table_free_handler called");
	while (lst_next (l, &p, &entry)) {
		if (m_table_is_free (entry->type) && entry->value > 0 && !m_is_freed(entry->value)) {
			m_free (entry->value);
		}
		if (m_table_is_free (entry->key_type) && entry->key > 0 && !m_is_freed(entry->key)) {
			m_free (entry->key);
		}
	}
}

// Global variable for the custom free handler ID
static int MFREE_TABLE_ENTRIES_HDLR = 0;

// --- Table Management ---

int m_table_create ()
{
	if (!MFREE_TABLE_ENTRIES_HDLR) {
		void (*f) (lst_t) = m_table_free_handler;
		MFREE_TABLE_ENTRIES_HDLR = m_reg_freefn (MFREE_MAX + 1, f);
	}

	return m_alloc (0, sizeof (m_table_entry_t), MFREE_TABLE_ENTRIES_HDLR);
}

int m_is_table (int table_h)
{
	if (table_h <= 0 || m_is_freed (table_h))
		return 0;
	return m_free_hdl (table_h) == MFREE_TABLE_ENTRIES_HDLR;
}

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

// --- Unified Internal Setter ---

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
		if (m_table_is_free (entry->type) && entry->value > 0) {
			m_free (entry->value);
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

void m_table_set_int_key (int table_h, int key_idx, int value,
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

void m_table_set_str_key_ext (int table_h, int key_str_h,
			      mls_table_type_t key_type, int value,
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

void m_table_set_str_key (int table_h, int key_str_h, int value,
			  mls_table_type_t type)
{
	m_table_set_str_key_ext (table_h, key_str_h, MLS_TABLE_TYPE_STRING,
				 value, type);
}

void m_table_set_cstr_key (int table_h, const char *key_cstr, int value,
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

void mt_seti (int table_h, const char *key, int val)
{
	m_table_set_cstr_key (table_h, key, val, MLS_TABLE_TYPE_INT);
}

void mt_sets (int table_h, const char *key, const char *val)
{
	m_table_set_cstr_key (table_h, key, s_strdup_c (val),
			      MLS_TABLE_TYPE_STRING);
}

void mt_setc (int table_h, const char *key, const char *val)
{
	m_table_set_cstr_key (table_h, key, s_cstr (val),
			      MLS_TABLE_TYPE_CONST_STRING);
}

void mt_seth (int table_h, const char *key, int handle, mls_table_type_t type)
{
	m_table_set_cstr_key (table_h, key, handle, type);
}

int mt_get (int table_h, const char *key)
{
	return m_table_get_cstr (table_h, key);
}

// --- Old API Forwarding ---

void m_table_set_int_val_by_int (int table_h, int key_idx, int raw_int_value)
{
	m_table_set_int_key (table_h, key_idx, raw_int_value,
			     MLS_TABLE_TYPE_INT);
}
void m_table_set_string_by_int (int table_h, int key_idx,
				const char *raw_c_string)
{
	m_table_set_int_key (table_h, key_idx, s_strdup_c (raw_c_string),
			     MLS_TABLE_TYPE_STRING);
}
void m_table_set_const_string_by_int (int table_h, int key_idx,
				      const char *raw_c_string)
{
	m_table_set_int_key (table_h, key_idx, s_cstr (raw_c_string),
			     MLS_TABLE_TYPE_CONST_STRING);
}
void m_table_set_list_by_int (int table_h, int key_idx, int list_h)
{
	m_table_set_int_key (table_h, key_idx, list_h, MLS_TABLE_TYPE_LIST);
}
void m_table_set_table_by_int (int table_h, int key_idx, int nested_table_h)
{
	m_table_set_int_key (table_h, key_idx, nested_table_h,
			     MLS_TABLE_TYPE_TABLE);
}
void m_table_set_handle_by_int (int table_h, int key_idx, int value_h,
				mls_table_type_t type)
{
	m_table_set_int_key (table_h, key_idx, value_h, type);
}

void m_table_set_int_val_by_cstr (int table_h, const char *key_cstr,
				  int raw_int_value)
{
	m_table_set_cstr_key (table_h, key_cstr, raw_int_value,
			      MLS_TABLE_TYPE_INT);
}
void m_table_set_string_by_cstr (int table_h, const char *key_cstr,
				 const char *raw_c_string)
{
	m_table_set_cstr_key (table_h, key_cstr, s_strdup_c (raw_c_string),
			      MLS_TABLE_TYPE_STRING);
}
void m_table_set_const_string_by_cstr (int table_h, const char *key_cstr,
				       const char *raw_c_string)
{
	m_table_set_cstr_key (table_h, key_cstr, s_cstr (raw_c_string),
			      MLS_TABLE_TYPE_CONST_STRING);
}
void m_table_set_list_by_cstr (int table_h, const char *key_cstr, int list_h)
{
	m_table_set_cstr_key (table_h, key_cstr, list_h, MLS_TABLE_TYPE_LIST);
}
void m_table_set_table_by_cstr (int table_h, const char *key_cstr,
				int nested_table_h)
{
	m_table_set_cstr_key (table_h, key_cstr, nested_table_h,
			      MLS_TABLE_TYPE_TABLE);
}
void m_table_set_handle_by_cstr (int table_h, const char *key_cstr, int value_h,
				 mls_table_type_t type)
{
	m_table_set_cstr_key (table_h, key_cstr, value_h, type);
}

void m_table_set_int_val_by_str (int table_h, int key_str_h, int raw_int_value)
{
	m_table_set_str_key (table_h, key_str_h, raw_int_value,
			     MLS_TABLE_TYPE_INT);
}
void m_table_set_string_by_str (int table_h, int key_str_h,
				const char *raw_c_string)
{
	m_table_set_str_key (table_h, key_str_h, s_strdup_c (raw_c_string),
			     MLS_TABLE_TYPE_STRING);
}
void m_table_set_const_string_by_str (int table_h, int key_str_h,
				      const char *raw_c_string)
{
	m_table_set_str_key (table_h, key_str_h, s_cstr (raw_c_string),
			     MLS_TABLE_TYPE_CONST_STRING);
}
void m_table_set_list_by_str (int table_h, int key_str_h, int list_h)
{
	m_table_set_str_key (table_h, key_str_h, list_h, MLS_TABLE_TYPE_LIST);
}
void m_table_set_table_by_str (int table_h, int key_str_h, int nested_table_h)
{
	m_table_set_str_key (table_h, key_str_h, nested_table_h,
			     MLS_TABLE_TYPE_TABLE);
}
void m_table_set_handle_by_str (int table_h, int key_str_h, int value_h,
				mls_table_type_t type)
{
	m_table_set_str_key (table_h, key_str_h, value_h, type);
}

// --- Getting Values ---

int m_table_get_int (int table_h, int key_idx)
{
	if (table_h <= 0)
		return 0;
	m_table_entry_t *entry = m_table_find_int_key_entry (table_h, key_idx);
	return entry ? entry->value : 0;
}

int m_table_get_str (int table_h, int key_str_h)
{
	if (table_h <= 0)
		return 0;
	m_table_entry_t *entry =
		m_table_find_str_key_entry (table_h, key_str_h);
	return entry ? entry->value : 0;
}

int m_table_get_cstr (int table_h, const char *key_cstr)
{
	if (table_h <= 0 || !key_cstr)
		return 0;
	m_table_entry_t *entry =
		m_table_find_cstr_key_entry (table_h, key_cstr);
	return entry ? entry->value : 0;
}

// --- Introspection ---

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

mls_table_type_t m_table_get_type_int (int table_h, int key_idx)
{
	if (table_h <= 0)
		return MLS_TABLE_TYPE_UNKNOWN;
	m_table_entry_t *entry = m_table_find_int_key_entry (table_h, key_idx);
	return entry ? (mls_table_type_t)entry->type : MLS_TABLE_TYPE_UNKNOWN;
}

mls_table_type_t m_table_get_type_str (int table_h, int key_str_h)
{
	if (table_h <= 0)
		return MLS_TABLE_TYPE_UNKNOWN;
	m_table_entry_t *entry =
		m_table_find_str_key_entry (table_h, key_str_h);
	return entry ? (mls_table_type_t)entry->type : MLS_TABLE_TYPE_UNKNOWN;
}

mls_table_type_t m_table_get_type_cstr (int table_h, const char *key_cstr)
{
	if (table_h <= 0 || !key_cstr)
		return MLS_TABLE_TYPE_UNKNOWN;
	m_table_entry_t *entry =
		m_table_find_cstr_key_entry (table_h, key_cstr);
	return entry ? (mls_table_type_t)entry->type : MLS_TABLE_TYPE_UNKNOWN;
}
