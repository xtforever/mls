#include "../lib/mls.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

void test_mfree() {
    printf("Testing MFREE (standard m_free)...\n");
    int m = m_alloc(10, sizeof(int), MFREE);
    int val = 42;
    m_put(m, &val);
    assert(m_len(m) == 1);
    m_free(m);
    assert(m_is_freed(m));
    printf("MFREE test passed.\n");
}

void test_mfree_str() {
    printf("Testing MFREE_STR (frees strings)...\n");
    int m = m_alloc(10, sizeof(char*), MFREE_STR);
    char *s1 = strdup("hello");
    char *s2 = strdup("world");
    m_put(m, &s1);
    m_put(m, &s2);
    assert(m_len(m) == 2);
    // m_free should call m_free_strings(m, 0)
    m_free(m);
    assert(m_is_freed(m));
    printf("MFREE_STR test passed.\n");
}

void test_mfree_each() {
    printf("Testing MFREE_EACH (nested lists)...\n");
    // Outer list containing handles to inner lists
    int outer = m_alloc(10, sizeof(int), MFREE_EACH);
    
    // Inner list 1: strings
    int inner1 = m_alloc(10, sizeof(char*), MFREE_STR);
    char *s1 = strdup("nested1");
    m_put(inner1, &s1);
    
    // Inner list 2: integers
    int inner2 = m_alloc(10, sizeof(int), MFREE);
    int val = 123;
    m_put(inner2, &val);
    
    m_put(outer, &inner1);
    m_put(outer, &inner2);
    
    assert(m_len(outer) == 2);
    
    // This should free outer, which calls xfree_impl for each element.
    // inner1 has MFREE_STR, inner2 has MFREE.
    m_free(outer);
    assert(m_is_freed(outer));
    assert(m_is_freed(inner1));
    assert(m_is_freed(inner2));
    printf("MFREE_EACH test passed.\n");
}

static int custom_free_called = 0;
void my_custom_free(int m) {
    custom_free_called++;
}

void test_custom_free() {
    printf("Testing custom free handler...\n");
    int hdl = m_reg_freefn(0, my_custom_free);
    int m = m_alloc(10, sizeof(int), hdl);
    int val = 99;
    m_put(m, &val);
    
    m_free(m);
    assert(custom_free_called == 1);
    assert(m_is_freed(m));
    printf("Custom free handler test passed.\n");
}

void test_mis_freed_logic() {
    printf("Testing m_is_freed logic...\n");
    int m = m_create(10, sizeof(int));
    assert(m_is_freed(m) == 0); // Should be false for active list
    m_free(m);
    assert(m_is_freed(m) == 1); // Should be true after free
    printf("m_is_freed logic test passed.\n");
}

int main() {
    trace_level = 1;
    m_init();
    
    test_mis_freed_logic();
    test_mfree();
    test_mfree_str();
    test_mfree_each();
    test_custom_free();
    
    m_destruct();
    printf("All auto-free tests completed successfully.\n");
    return 0;
}
