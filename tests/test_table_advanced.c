#include "../lib/mls.h"
#include "../lib/m_tool.h"
#include "../lib/m_table.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

void test_m_table_deep_nesting() {
    printf("Testing m_table deep nesting...\n");
    int root = m_table_create();
    
    // root -> "list" -> [ table1 ]
    int list = m_alloc(0, sizeof(int), MFREE_EACH);
    int table1 = m_table_create();
    m_table_set_int_val_by_cstr(table1, "val", 42);
    m_put(list, &table1);
    
    m_table_set_list_by_cstr(root, "list", list);

    // root -> "deep" -> table2 -> "inner" -> table3 -> "value" -> 99
    int table2 = m_table_create();
    int table3 = m_table_create();
    m_table_set_int_val_by_cstr(table3, "value", 99);
    m_table_set_table_by_cstr(table2, "inner", table3);
    m_table_set_table_by_cstr(root, "deep", table2);

    // Verify
    int l = m_table_get_cstr(root, "list");
    int t1 = INT(l, 0);
    assert(m_table_get_cstr(t1, "val") == 42);

    int t2 = m_table_get_cstr(root, "deep");
    int t3 = m_table_get_cstr(t2, "inner");
    assert(m_table_get_cstr(t3, "value") == 99);

    m_table_free(root);
    printf("m_table deep nesting test passed.\n");
}

void test_m_table_introspection_comprehensive() {
    printf("Testing m_table introspection comprehensive...\n");
    int table = m_table_create();
    m_table_set_int_val_by_cstr(table, "a", 1);
    m_table_set_string_by_cstr(table, "b", "hello");
    m_table_set_const_string_by_cstr(table, "c", "world");
    m_table_set_list_by_cstr(table, "d", m_alloc(0, 1, MFREE));
    m_table_set_table_by_cstr(table, "e", m_table_create());

    assert(m_table_get_type_cstr(table, "a") == MLS_TABLE_TYPE_INT);
    assert(m_table_get_type_cstr(table, "b") == MLS_TABLE_TYPE_STRING);
    assert(m_table_get_type_cstr(table, "c") == MLS_TABLE_TYPE_CONST_STRING);
    assert(m_table_get_type_cstr(table, "d") == MLS_TABLE_TYPE_LIST);
    assert(m_table_get_type_cstr(table, "e") == MLS_TABLE_TYPE_TABLE);
    assert(m_table_get_type_cstr(table, "non-existent") == MLS_TABLE_TYPE_UNKNOWN);

    m_table_free(table);
    printf("m_table introspection test passed.\n");
}

void test_m_table_get_with_defaults() {
    printf("Testing m_table get with defaults (theoretical)...\n");
    int table = m_table_create();
    
    // Current m_table API doesn't have "get with default" in its basic form,
    // but we can test behavior of non-existent keys.
    assert(m_table_get_cstr(table, "nothing") == 0);
    
    m_table_free(table);
    printf("m_table get with defaults test passed.\n");
}

int main() {
    trace_level = 1;
    m_init();
    conststr_init();

    test_m_table_deep_nesting();
    test_m_table_introspection_comprehensive();
    test_m_table_get_with_defaults();

    conststr_free();
    m_destruct();
    printf("All advanced m_table tests completed successfully.\n");
    return 0;
}
