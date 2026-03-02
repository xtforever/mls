#include "../lib/mls.h"
#include "../lib/m_tool.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

void test_partial_free_nested();
void test_circular_reference();
void test_deep_nesting();

void test_nested_3_levels() {
    printf("Testing nested lists (3 levels deep)...\n");
    // Level 1: Outer list
    int l1 = m_alloc(10, sizeof(int), MFREE_EACH);
    
    // Level 2: Middle list
    int l2 = m_alloc(10, sizeof(int), MFREE_EACH);
    
    // Level 3: Inner list (strings)
    int l3 = m_alloc(10, sizeof(char*), MFREE_STR);
    char *s1 = strdup("deepest");
    m_put(l3, &s1);
    
    m_put(l2, &l3);
    m_put(l1, &l2);
    
    assert(m_len(l1) == 1);
    assert(m_len(l2) == 1);
    assert(m_len(l3) == 1);
    
    m_free(l1);
    assert(m_is_freed(l1));
    assert(m_is_freed(l2));
    assert(m_is_freed(l3));
    printf("3-level nested list test passed.\n");
}

void test_mixed_nested_elements() {
    printf("Testing mixed nested elements...\n");
    int outer = m_alloc(10, sizeof(int), MFREE_EACH);
    
    int l_str = m_alloc(10, sizeof(char*), MFREE_STR);
    char *s1 = strdup("string1");
    char *s2 = strdup("string2");
    m_put(l_str, &s1);
    m_put(l_str, &s2);
    
    int l_int = m_alloc(10, sizeof(int), MFREE);
    int val1 = 1, val2 = 2;
    m_put(l_int, &val1);
    m_put(l_int, &val2);
    
    m_put(outer, &l_str);
    m_put(outer, &l_int);
    
    assert(m_len(outer) == 2);
    assert(m_len(l_str) == 2);
    assert(m_len(l_int) == 2);
    
    m_free(outer);
    assert(m_is_freed(outer));
    assert(m_is_freed(l_str));
    assert(m_is_freed(l_int));
    printf("Mixed nested elements test passed.\n");
}

void test_null_in_nested() {
    printf("Testing 0/NULL in nested lists...\n");
    int outer = m_alloc(10, sizeof(int), MFREE_EACH);
    int h0 = 0;
    m_put(outer, &h0); // 0 should be ignored by m_free_each
    
    int inner = m_alloc(10, sizeof(int), MFREE);
    m_put(outer, &inner);
    
    assert(m_len(outer) == 2);
    m_free(outer);
    assert(m_is_freed(outer));
    assert(m_is_freed(inner));
    printf("0/NULL in nested lists test passed.\n");
}

int main() {
    trace_level = 1;
    m_init();
    conststr_init();
    
    test_nested_3_levels();
    test_mixed_nested_elements();
    test_null_in_nested();
    test_partial_free_nested();
    test_circular_reference();
    test_deep_nesting();
    
    conststr_free();
    m_destruct();
    printf("All nested list tests completed successfully.\n");
    return 0;
}

void test_circular_reference() {
    printf("Testing circular reference (should not crash/infinite loop)...\n");
    // Note: m_free_each is NOT recursive in a way that handles cycles 
    // unless we track visited lists. Currently it just calls m_free.
    // If l1 contains l1, m_free(l1) -> free_list_wrap(l1) -> m_free(l1) 
    // This could be problematic if not handled.
    
    int l1 = m_alloc(10, sizeof(int), MFREE_EACH);
    m_put(l1, &l1);
    
    // This will call m_free(l1) for the element.
    // m_free(l1) will see it's ALREADY being freed? 
    // Actually, m_free_simple(l1) is called AFTER the loop.
    // So m_free(l1) (inner) will see l1 is NOT YET in FR.
    // This might cause infinite recursion if not careful.
    
    printf("Warning: Circular references are dangerous with MFREE_EACH!\n");
    // For now, let's see what happens.
    m_free(l1);
    assert(m_is_freed(l1));
    printf("Circular reference test finished.\n");
}

void test_deep_nesting() {
    printf("Testing deep nesting (10 levels)...\n");
    // Bottom list: simple integers
    int current = m_alloc(10, sizeof(int), MFREE);
    int val = 42;
    m_put(current, &val);
    
    for (int i = 0; i < 10; i++) {
        int next = m_alloc(1, sizeof(int), MFREE_EACH);
        m_put(next, &current);
        current = next;
    }
    
    m_free(current);
    printf("Deep nesting test passed.\n");
}

void test_partial_free_nested() {
    printf("Testing partial free in nested lists...\n");
    int outer = m_alloc(10, sizeof(int), MFREE_EACH);
    
    int inner1 = m_alloc(10, sizeof(int), MFREE);
    int inner2 = m_alloc(10, sizeof(int), MFREE);
    
    m_put(outer, &inner1);
    m_put(outer, &inner2);
    
    // Explicitly free inner1
    m_free(inner1);
    assert(m_is_freed(inner1));
    
    // Now free outer. It should handle already freed inner1 gracefully.
    m_free(outer);
    assert(m_is_freed(outer));
    assert(m_is_freed(inner2));
    printf("Partial free in nested lists test passed.\n");
}
