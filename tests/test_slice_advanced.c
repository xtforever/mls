#include "../lib/mls.h"
#include "../lib/m_tool.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

void test_m_slice_clamping() {
    printf("Testing m_slice clamping...\n");
    int m = m_alloc(5, sizeof(int), MFREE);
    for (int i = 0; i < 5; i++) {
        m_put(m, &i);
    }

    // Start too small
    int s1 = m_slice(0, 0, m, -100, 2);
    assert(m_len(s1) == 3); // 0, 1, 2
    assert(INT(s1, 0) == 0);
    m_free(s1);

    // End too large
    int s2 = m_slice(0, 0, m, 3, 100);
    assert(m_len(s2) == 2); // 3, 4
    assert(INT(s2, 0) == 3);
    assert(INT(s2, 1) == 4);
    m_free(s2);

    // Both too large/small
    int s3 = m_slice(0, 0, m, -100, 100);
    assert(m_len(s3) == 5);
    m_free(s3);

    m_free(m);
    printf("m_slice clamping test passed.\n");
}

void test_m_slice_empty_or_invalid_range() {
    printf("Testing m_slice empty or invalid range...\n");
    int m = m_alloc(5, sizeof(int), MFREE);
    for (int i = 0; i < 5; i++) {
        m_put(m, &i);
    }

    // start > end
    int s1 = m_slice(0, 0, m, 3, 2);
    assert(m_len(s1) == 0);
    m_free(s1);

    // start == end (should return 1 element)
    int s2 = m_slice(0, 0, m, 2, 2);
    assert(m_len(s2) == 1);
    assert(INT(s2, 0) == 2);
    m_free(s2);

    // Empty list
    int empty = m_alloc(0, sizeof(int), MFREE);
    int s3 = m_slice(0, 0, empty, 0, 5);
    assert(m_len(s3) == 0);
    m_free(s3);
    m_free(empty);

    m_free(m);
    printf("m_slice empty/invalid range test passed.\n");
}

void test_m_slice_null_handle() {
    printf("Testing m_slice null handle safety...\n");
    // Slicing from handle 0 should return handle 0 or an empty list depending on implementation.
    // Usually mls tools return 0 for invalid input handles.
    int s = m_slice(0, 0, 0, 0, 10);
    assert(s == 0);
    printf("m_slice null handle safety test passed.\n");
}

void test_s_slice_advanced() {
    printf("Testing s_slice advanced (negative and clamping)...\n");
    int m = s_cstr("Hello World");
    
    // "Hello World\0" has length 12.
    // Index of 'W' is 6. Index of 'd' is 10.
    // -6 is 12-6=6. -2 is 12-2=10.
    // s_slice(m, -6, -2) should give "World" (indices 6, 7, 8, 9, 10).
    int s1 = s_slice(0, 0, m, -6, -2);
    assert(strcmp(m_str(s1), "World") == 0);
    m_free(s1);

    // Clamping: -100 to 100
    int s2 = s_slice(0, 0, m, -100, 100);
    assert(strcmp(m_str(s2), "Hello World") == 0);
    m_free(s2);

    // start > end
    int s3 = s_slice(0, 0, m, 5, 2);
    assert(strcmp(m_str(s3), "") == 0);
    m_free(s3);

    m_free(m);
    printf("s_slice advanced test passed.\n");
}

int main() {
    trace_level = 1;
    m_init();
    conststr_init();
    
    test_m_slice_clamping();
    test_m_slice_empty_or_invalid_range();
    test_m_slice_null_handle();
    test_s_slice_advanced();
    
    conststr_free();
    m_destruct();
    printf("All advanced slice tests completed successfully.\n");
    return 0;
}
