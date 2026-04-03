#include "mls.h"
#include "m_tool.h"
#include <signal.h>
#include <setjmp.h>

#undef ASSERT
#include "greatest.h"

SUITE (string_robustness_suite);

/* Edge Case: Handle 0 */
TEST test_handle_zero (void)
{
    /* Many s_ functions should handle h=0 gracefully by returning 0, a new handle, or -1 */
    ASSERT_EQ(0, s_strlen(0));
    ASSERT_EQ(1, s_isempty(0));
    
    int h = s_clone(0);
    ASSERT(h == 0); /* s_clone of 0 returns 0 */
    
    h = s_sub(0, 0, 10);
    ASSERT(h > 0); /* s_sub of 0 returns a new empty string */
    ASSERT_EQ(0, s_strlen(h));
    s_free(h);
    
    h = s_dup(NULL);
    ASSERT(h > 0);
    ASSERT_EQ(0, s_strlen(h));
    s_free(h);

    PASS();
}
#include <sys/wait.h>
#include <unistd.h>

/* Helper to check if a function calls exit(1) */
static int check_if_exit(void (*fn)(int), int arg) {
    pid_t pid = fork();
    if (pid == 0) {
        /* child */
        fn(arg);
        exit(0); /* should not reach here if fn calls ERR */
    }
    int status;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 1;
}

static void call_m_str(int h) {
    m_str(h);
}

/* Edge Case: Not Null-Terminated Strings */
TEST test_non_terminated (void)
{
    /* Create a buffer manually that is NOT null-terminated */
    int h = m_alloc(5, 1, MFREE);
    m_setlen(h, 5);
    memcpy(m_buf(h), "ABCDE", 5);

    /* m_str(h) should now trigger ERR and exit(1) because of the new check */
    ASSERT(check_if_exit(call_m_str, h));

    /* s_clone should fix termination */
    int c = s_clone(h);
    ASSERT_EQ(5, s_strlen(c));
    ASSERT_EQ(0, CHAR(c, 5));
    ASSERT_STR_EQ("ABCDE", m_str(c));
    
    /* s_sub should fix termination */
    int sub = s_sub(h, 1, 3); /* "BCD" */
    ASSERT_EQ(3, s_strlen(sub));
    ASSERT_EQ(0, CHAR(sub, 3));
    ASSERT_STR_EQ("BCD", m_str(sub));
    
    s_free(h);
    s_free(c);
    s_free(sub);
    PASS();
}

/* Edge Case: Freed Handles */
/* Note: Accessing a freed handle via mls() or m_buf() triggers ERR and exits.
   We can only safely check them with m_is_freed(). 
   Testing if s_functions crash on freed handles is hard without fork/wait
   because the library calls exit(1).
*/
TEST test_freed_handle (void)
{
    int h = s_dup("Freed");
    s_free(h);
    ASSERT(m_is_freed(h));
    
    /* We don't call s_strlen(h) here because it would trigger exit(1) 
       inside get_list() in debug mode. 
       A robust application should check m_is_freed() before passing 
       untrusted handles.
    */
    PASS();
}

/* Edge Case: Very large slices and negative indices */
TEST test_slice_extremes (void)
{
    int h = s_dup("Robustness");
    
    /* b < a should return empty */
    int s = s_slice(0, 0, h, 5, 2);
    ASSERT_EQ(0, s_strlen(s));
    s_free(s);
    
    /* b far out of bounds */
    s = s_slice(0, 0, h, 0, 1000);
    ASSERT_STR_EQ("Robustness", m_str(s));
    s_free(s);
    
    /* a out of bounds */
    s = s_slice(0, 0, h, 50, 60);
    ASSERT_EQ(0, s_strlen(s));
    s_free(s);
    
    s_free(h);
    PASS();
}

/* Edge Case: s_replace with non-terminated sources */
TEST test_replace_robustness (void)
{
    int src = m_alloc(10, 1, MFREE);
    m_setlen(src, 10);
    memcpy(m_buf(src), "A B A B A ", 10); /* No null terminator */
    
    int pat = s_dup("A");
    int rep = s_dup("XXX");
    
    int res = s_replace(0, src, pat, rep, -1);
    ASSERT(res > 0);
    ASSERT_EQ(0, CHAR(res, m_len(res)-1)); /* Ensure null terminated */
    ASSERT(s_strlen(res) > 10);
    
    s_free(src);
    s_free(pat);
    s_free(rep);
    s_free(res);
    PASS();
}

/* Edge Case: Multiple Null Bytes at the end */
TEST test_multiple_nulls (void)
{
    int h = s_dup("Hello");
    m_putc(h, 0);
    m_putc(h, 0);
    m_putc(h, 0);
    
    /* Original string "Hello" (5) + 4 null bytes = 9 total len */
    ASSERT_EQ(9, m_len(h));
    ASSERT_EQ(5, s_strlen(h));
    
    s_free(h);
    PASS();
}

/* Test for s_subcmp */
TEST test_s_subcmp (void)
{
    int s0 = s_dup("Hello World");
    int s1 = s_dup("Gems Hello World");
    
    /* Compare "Hello" in both */
    ASSERT_EQ(0, s_subcmp(s0, 0, 4, s1, 5, 9));
    
    /* Compare "World" in both */
    ASSERT_EQ(0, s_subcmp(s0, 6, 10, s1, 11, 15));
    
    /* Compare "Hello" vs "World" */
    ASSERT(s_subcmp(s0, 0, 4, s0, 6, 10) < 0);
    
    /* Handle 0 cases */
    ASSERT_EQ(0, s_subcmp(0, 0, 0, 0, 0, 0));
    ASSERT(s_subcmp(0, 0, 0, s0, 0, 0) < 0);
    
    /* Negative indices */
    ASSERT_EQ(0, s_subcmp(s0, 0, 4, s1, 5, -7));
    
    s_free(s0);
    s_free(s1);
    PASS();
}

GREATEST_SUITE (string_robustness_suite)
{
    RUN_TEST (test_handle_zero);
    RUN_TEST (test_non_terminated);
    RUN_TEST (test_freed_handle);
    RUN_TEST (test_slice_extremes);
    RUN_TEST (test_replace_robustness);
    RUN_TEST (test_multiple_nulls);
    RUN_TEST (test_s_subcmp);
}

GREATEST_MAIN_DEFS ();

int main (int argc, char **argv)
{
    GREATEST_MAIN_BEGIN ();
    m_init ();
    conststr_init();
    RUN_SUITE (string_robustness_suite);
    conststr_free();
    GREATEST_MAIN_END ();
}
