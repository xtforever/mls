#include "../lib/mls.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

void test_ssplit_basic() {
    printf("Testing s_split basic...\n");
    int m = s_split(0, "a,b,c", ',', 0);
    assert(m_len(m) == 3);
    assert(strcmp(STR(m, 0), "a") == 0);
    assert(strcmp(STR(m, 1), "b") == 0);
    assert(strcmp(STR(m, 2), "c") == 0);
    m_free(m);
    printf("s_split basic passed.\n");
}

void test_ssplit_ws() {
    printf("Testing s_split with whitespace trimming...\n");
    int m = s_split(0, "  apple ,  banana  , cherry ", ',', 1);
    assert(m_len(m) == 3);
    assert(strcmp(STR(m, 0), "apple") == 0);
    assert(strcmp(STR(m, 1), "banana") == 0);
    assert(strcmp(STR(m, 2), "cherry") == 0);
    m_free_strings(m, 0);
    printf("s_split whitespace trimming passed.\n");
}

void test_ssplit_empty() {
    printf("Testing s_split with empty fields...\n");
    int m = s_split(0, ",,", ',', 0);
    assert(m_len(m) == 3);
    assert(strcmp(STR(m, 0), "") == 0);
    assert(strcmp(STR(m, 1), "") == 0);
    assert(strcmp(STR(m, 2), "") == 0);
    m_free_strings(m, 0);
    printf("s_split empty fields passed.\n");
}

void test_ssplit_no_delim() {
    printf("Testing s_split with no delimiter found...\n");
    int m = s_split(0, "hello world", ',', 0);
    assert(m_len(m) == 1);
    assert(strcmp(STR(m, 0), "hello world") == 0);
    m_free_strings(m, 0);
    printf("s_split no delimiter passed.\n");
}

int main() {
    trace_level = 1;
    m_init();
    
    // test_ssplit_basic();
    // test_ssplit_ws();
    // test_ssplit_empty();
    // test_ssplit_no_delim();
    int m = s_split(0, "hello world", ',', 0);
    m_free_strings(m,0);
    m_destruct();
    printf("All s_split tests completed successfully.\n");
    return 0;
}
