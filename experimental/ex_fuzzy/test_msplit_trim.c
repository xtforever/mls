#include "mls.h"
#include "m_tool.h"

#include <stdio.h>

static int failed = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); failed++; } \
    else { printf("  ok  %s\n", msg); } \
} while(0)

#define CHECK_STR(h, want, msg) \
    CHECK(s_strcmp_c(INT(h, 0), want) == 0, msg)

static void test_no_trim(void)
{
    printf("[test_no_trim]\n");
    int src = s_dup("  a  | b |c ");
    int toks = s_msplit_trim(0, src, s_cstr("|"), 0);
    CHECK(m_len(toks) == 3, "3 tokens");
    CHECK_STR(toks, "  a  ", "token0 untrimmed");
    toks = s_msplit_trim(toks, src, s_cstr("|"), 0);
    CHECK(m_len(toks) == 3, "3 tokens (reused)");
    CHECK_STR(toks, "  a  ", "reused token0 untrimmed");
    m_free(toks);
    m_free(src);
}

static void test_trim(void)
{
    printf("[test_trim]\n");
    int src = s_dup("  a  | b |c ");
    int toks = s_msplit_trim(0, src, s_cstr("|"), 1);
    CHECK(m_len(toks) == 3, "3 tokens");
    CHECK(s_strcmp_c(INT(toks, 0), "a") == 0, "token0 trimmed");
    CHECK(s_strcmp_c(INT(toks, 1), "b") == 0, "token1 trimmed");
    CHECK(s_strcmp_c(INT(toks, 2), "c") == 0, "token2 trimmed");
    m_free(toks);
    m_free(src);
}

static void test_empty_fields(void)
{
    printf("[test_empty_fields]\n");
    int src = s_dup("a||b");
    int toks = s_msplit_trim(0, src, s_cstr("|"), 1);
    CHECK(m_len(toks) == 3, "empty field kept");
    CHECK(s_strcmp_c(INT(toks, 0), "a") == 0, "token0");
    CHECK(s_strcmp_c(INT(toks, 1), "") == 0, "token1 empty");
    CHECK(s_strcmp_c(INT(toks, 2), "b") == 0, "token2");
    m_free(toks);
    m_free(src);
}

int main(void)
{
    m_init();
    test_no_trim();
    test_trim();
    test_empty_fields();
    if (failed) { printf("%d FAILURES\n", failed); return 1; }
    printf("all ok\n");
    return 0;
}
