#include "../lib/mls.h"
#include "../lib/m_tool.h"
#include "../lib/m_table.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

void print_table_type(mls_table_type_t type) {
    switch (type) {
        case MLS_TABLE_TYPE_UNKNOWN: printf("UNKNOWN"); break;
        case MLS_TABLE_TYPE_INT: printf("INT"); break;
        case MLS_TABLE_TYPE_STRING: printf("STRING"); break;
        case MLS_TABLE_TYPE_CONST_STRING: printf("CONST_STRING"); break;
        case MLS_TABLE_TYPE_LIST: printf("LIST"); break;
        case MLS_TABLE_TYPE_TABLE: printf("TABLE"); break;
        case MLS_TABLE_TYPE_CUSTOM_HANDLE: printf("CUSTOM_HANDLE"); break;
        default: printf("???"); break;
    }
}

void test_m_table_basic() {
    printf("Testing m_table basic operations...\n");
    int table = m_table_create();
    assert(table > 0);

    // Set integer values
    int age = 30;
    m_table_set_int_val_by_cstr(table, "age", age);
    assert(m_table_get_cstr(table, "age") == 30);
    assert(m_table_get_type_cstr(table, "age") == MLS_TABLE_TYPE_INT);

    // Set string handle (dynamic string)
    m_table_set_string_by_cstr(table, "name", "Alice");
    assert(strcmp(m_str(m_table_get_cstr(table, "name")), "Alice") == 0);
    assert(m_table_get_type_cstr(table, "name") == MLS_TABLE_TYPE_STRING);

    // Set constant string handle (s_cstr)
    m_table_set_const_string_by_cstr(table, "status", "active");
    assert(strcmp(m_str(m_table_get_cstr(table, "status")), "active") == 0);
    assert(m_table_get_type_cstr(table, "status") == MLS_TABLE_TYPE_CONST_STRING);

    // Set list handle
    int grades_h = m_alloc(0, sizeof(int), MFREE);
    int g1 = 90, g2 = 85;
    m_put(grades_h, &g1); m_put(grades_h, &g2);
    m_table_set_list_by_cstr(table, "grades", grades_h);
    assert(INT(m_table_get_cstr(table, "grades"), 0) == 90);
    assert(m_table_get_type_cstr(table, "grades") == MLS_TABLE_TYPE_LIST);

    // Test non-existent key
    assert(m_table_get_cstr(table, "city") == 0);
    assert(m_table_get_type_cstr(table, "city") == MLS_TABLE_TYPE_UNKNOWN);

    m_table_free(table);
    printf("m_table basic operations passed.\n");
}

void test_m_table_nesting() {
    printf("Testing m_table nesting...\n");
    int config_table = m_table_create();

    // Nested table
    int db_settings = m_table_create();
    m_table_set_const_string_by_cstr(db_settings, "host", "localhost");
    m_table_set_int_val_by_cstr(db_settings, "port", 5432);
    m_table_set_table_by_cstr(config_table, "database", db_settings);

    // Nested list of strings
    int users_list = m_alloc(0, sizeof(int), MFREE); // Use MFREE for list of s_cstr handles
    m_put(users_list, &(int){s_cstr("admin")});
    m_put(users_list, &(int){s_cstr("guest")});
    m_table_set_list_by_cstr(config_table, "users", users_list);
    
    // Access nested values
    int retrieved_db_h = m_table_get_cstr(config_table, "database");
    assert(retrieved_db_h > 0);
    assert(m_table_get_type_cstr(config_table, "database") == MLS_TABLE_TYPE_TABLE);
    assert(m_table_get_cstr(retrieved_db_h, "port") == 5432);
    assert(m_table_get_type_cstr(retrieved_db_h, "port") == MLS_TABLE_TYPE_INT);
    assert(strcmp(m_str(m_table_get_cstr(retrieved_db_h, "host")), "localhost") == 0);
    assert(m_table_get_type_cstr(retrieved_db_h, "host") == MLS_TABLE_TYPE_CONST_STRING);

    int retrieved_users_h = m_table_get_cstr(config_table, "users");
    assert(retrieved_users_h > 0);
    assert(m_table_get_type_cstr(config_table, "users") == MLS_TABLE_TYPE_LIST);
    assert(strcmp(m_str(INT(retrieved_users_h, 0)), "admin") == 0);

    m_table_free(config_table);
    printf("m_table nesting passed.\n");
}

void test_m_table_update_values() {
    printf("Testing m_table value updates...\n");
    int table = m_table_create();

    m_table_set_int_val_by_cstr(table, "score", 100);
    assert(m_table_get_cstr(table, "score") == 100);

    m_table_set_int_val_by_cstr(table, "score", 200); // Update
    assert(m_table_get_cstr(table, "score") == 200);

    m_table_set_string_by_cstr(table, "message", "old_string"); // type is STRING
    assert(strcmp(m_str(m_table_get_cstr(table, "message")), "old_string") == 0);

    m_table_set_string_by_cstr(table, "message", "new_string"); // Update. old_str_h should be freed because its type was STRING.
    assert(strcmp(m_str(m_table_get_cstr(table, "message")), "new_string") == 0);

    // Test updating a dynamic string with a const string
    m_table_set_const_string_by_cstr(table, "message", "constant_msg"); // old new_str_h should be freed
    assert(strcmp(m_str(m_table_get_cstr(table, "message")), "constant_msg") == 0);
    assert(m_table_get_type_cstr(table, "message") == MLS_TABLE_TYPE_CONST_STRING);
    
    // Update a const string with a dynamic string
    m_table_set_string_by_cstr(table, "message", "another_dynamic"); 
    assert(strcmp(m_str(m_table_get_cstr(table, "message")), "another_dynamic") == 0);
    assert(m_table_get_type_cstr(table, "message") == MLS_TABLE_TYPE_STRING);

    m_table_free(table);
    printf("m_table value updates passed.\n");
}

void test_m_table_int_keys() {
    printf("Testing m_table with integer keys...\n");
    int table = m_table_create();
    assert(table > 0);

    m_table_set_int_val_by_int(table, 1, 100);
    assert(m_table_get_int(table, 1) == 100);
    assert(m_table_get_type_int(table, 1) == MLS_TABLE_TYPE_INT);

    m_table_set_string_by_int(table, 5, "item_five");
    assert(strcmp(m_str(m_table_get_int(table, 5)), "item_five") == 0);
    assert(m_table_get_type_int(table, 5) == MLS_TABLE_TYPE_STRING);

    // Overwrite existing integer key
    m_table_set_int_val_by_int(table, 1, 101);
    assert(m_table_get_int(table, 1) == 101);
    
    // Test non-existent int key
    assert(m_table_get_int(table, 99) == 0);
    assert(m_table_get_type_int(table, 99) == MLS_TABLE_TYPE_UNKNOWN);

    m_table_free(table);
    printf("m_table integer keys passed.\n");
}

void test_m_table_string_key_handles() {
    printf("Testing m_table with string key handles...\n");
    int table = m_table_create();
    assert(table > 0);

    int key1_h = s_printf(0, 0, "dynamic_key");
    m_table_set_int_val_by_str(table, key1_h, 123);
    assert(m_table_get_str(table, key1_h) == 123);
    assert(m_table_get_type_str(table, key1_h) == MLS_TABLE_TYPE_INT);
    
    int key2_h = s_cstr("constant_key"); // Constant key handle
    m_table_set_string_by_str(table, key2_h, "value_str");
    assert(strcmp(m_str(m_table_get_str(table, key2_h)), "value_str") == 0);
    assert(m_table_get_type_str(table, key2_h) == MLS_TABLE_TYPE_STRING);

    // Overwrite key1_h with a new value
    m_table_set_int_val_by_str(table, key1_h, 456);
    assert(m_table_get_str(table, key1_h) == 456);

    // Test non-existent string key handle
    int non_existent_key_h = s_printf(0, 0, "non_existent");
    assert(m_table_get_str(table, non_existent_key_h) == 0);
    assert(m_table_get_type_str(table, non_existent_key_h) == MLS_TABLE_TYPE_UNKNOWN);
    m_free(non_existent_key_h); // Free the temporary search key

    m_table_free(table);
    // key1_h will be freed by m_table_free_handler
    // key2_h (s_cstr) will NOT be freed by m_table_free_handler (correct)
    printf("m_table string key handles passed.\n");
}


void test_m_table_type_overwrites() {
    printf("Testing m_table type overwrites...\n");
    int table = m_table_create();

    m_table_set_int_val_by_cstr(table, "data", 123);
    assert(m_table_get_type_cstr(table, "data") == MLS_TABLE_TYPE_INT);
    assert(m_table_get_cstr(table, "data") == 123);

    m_table_set_string_by_cstr(table, "data", "hello"); // Overwrite int with string handle
    assert(m_table_get_type_cstr(table, "data") == MLS_TABLE_TYPE_STRING);
    assert(strcmp(m_str(m_table_get_cstr(table, "data")), "hello") == 0);

    int list_h = m_alloc(0, sizeof(int), MFREE);
    m_put(list_h, &(int){42});
    m_table_set_list_by_cstr(table, "data", list_h); // Overwrite string with list handle
    assert(m_table_get_type_cstr(table, "data") == MLS_TABLE_TYPE_LIST);
    assert(INT(m_table_get_cstr(table, "data"), 0) == 42);

    // Test that value is correctly freed when type changes
    m_table_set_string_by_cstr(table, "data", "new_string_val"); // Overwrite list with string
    assert(m_table_get_type_cstr(table, "data") == MLS_TABLE_TYPE_STRING);
    assert(strcmp(m_str(m_table_get_cstr(table, "data")), "new_string_val") == 0);

    m_table_free(table);
    printf("m_table type overwrites passed.\n");
}


int main() {
    trace_level = 1;
    m_init();
    conststr_init(); // For s_cstr used in tests

    test_m_table_basic();
    test_m_table_nesting();
    test_m_table_update_values();
    test_m_table_int_keys();
    test_m_table_string_key_handles();
    test_m_table_type_overwrites();

    conststr_free();
    m_destruct();
    printf("All m_table tests completed successfully.\n");
    return 0;
}
