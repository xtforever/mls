#include "mls.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int failed = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); failed++; } \
    else { printf("  ok  %s\n", msg); } \
} while(0)

static void test_errmsg(void)
{
    printf("[test_errmsg]\n");
    CHECK(mls_errmsg(MLS_OK) != NULL, "MLS_OK message");
    CHECK(mls_errmsg(MLS_EINVAL) != NULL, "MLS_EINVAL message");
    CHECK(mls_errmsg(MLS_EBOUNDS) != NULL, "MLS_EBOUNDS message");
    CHECK(mls_errmsg(MLS_ENOMEM) != NULL, "MLS_ENOMEM message");
    CHECK(mls_errmsg(MLS_EUAF) != NULL, "MLS_EUAF message");
    CHECK(mls_errmsg(MLS_EOVERFLOW) != NULL, "MLS_EOVERFLOW message");
    CHECK(mls_errmsg(-1) != NULL, "unknown code message");
    CHECK(strlen(mls_errmsg(MLS_OK)) > 0, "MLS_OK non-empty");
}

static void test_mls_safe_invalid(void)
{
    printf("[test_mls_safe_invalid]\n");
    mls_errno = 0;
    void *p = mls_safe(-1, 0);
    CHECK(p == NULL, "mls_safe(-1, 0) returns NULL");
    CHECK(mls_errno == MLS_EINVAL, "mls_errno == MLS_EINVAL");
}

static void test_mls_safe_oob(void)
{
    printf("[test_mls_safe_oob]\n");
    int h = m_alloc(5, sizeof(int), MFREE);
    m_setlen(h, 3);
    mls_errno = 0;
    void *p = mls_safe(h, 100);
    CHECK(p == NULL, "mls_safe(h, 100) returns NULL");
    CHECK(mls_errno == MLS_EBOUNDS, "mls_errno == MLS_EBOUNDS");
    m_free(h);
}

static void test_mls_safe_uaf(void)
{
    printf("[test_mls_safe_uaf]\n");
    int h = m_alloc(1, sizeof(int), MFREE);
    m_free(h);
    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0) {
        mls_safe(h, 0); /* UAF -> exit(1) */
        _exit(0);       /* not reached */
    }
    int st = 0;
    waitpid(pid, &st, 0);
    CHECK(WIFEXITED(st) && WEXITSTATUS(st) == 1,
          "mls_safe on freed handle exits(1)");
}

static void test_mls_safe_ok(void)
{
    printf("[test_mls_safe_ok]\n");
    int h = m_alloc(5, sizeof(int), MFREE);
    int v = 42;
    m_put(h, &v);
    mls_errno = 0;
    int *p = (int *)mls_safe(h, 0);
    CHECK(p != NULL, "mls_safe(h, 0) returns pointer");
    CHECK(*p == 42, "value == 42");
    CHECK(mls_errno == 0, "mls_errno unchanged on success");
    m_free(h);
}

static void test_put_safe_invalid(void)
{
    printf("[test_put_safe_invalid]\n");
    mls_errno = 0;
    int v = 99;
    int r = m_put_safe(-1, &v);
    CHECK(r == -1, "m_put_safe(-1, &v) returns -1");
    CHECK(mls_errno == MLS_EINVAL, "mls_errno == MLS_EINVAL");
}

static void test_put_safe_ok(void)
{
    printf("[test_put_safe_ok]\n");
    int h = m_alloc(5, sizeof(int), MFREE);
    int v = 77;
    mls_errno = 0;
    int r = m_put_safe(h, &v);
    CHECK(r >= 0, "m_put_safe returns valid index");
    CHECK(mls_errno == 0, "mls_errno not set on success");
    CHECK(INT(h, 0) == 77, "stored value == 77");
    m_free(h);
}

static void test_free_double(void)
{
    printf("[test_free_double]\n");
    int h = m_alloc(1, sizeof(int), MFREE);
    m_free(h);
    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0) {
        m_free(h); /* double free -> exit(1) */
        _exit(0);  /* not reached */
    }
    int st = 0;
    waitpid(pid, &st, 0);
    CHECK(WIFEXITED(st) && WEXITSTATUS(st) == 1,
          "m_free on freed handle exits(1)");
}

static void test_write_safe_invalid(void)
{
    printf("[test_write_safe_invalid]\n");
    mls_errno = 0;
    int v = 0;
    int r = m_write_safe(-1, 0, &v, 1);
    CHECK(r == -1, "m_write_safe(-1, ...) returns -1");
    CHECK(mls_errno == MLS_EINVAL, "mls_errno == MLS_EINVAL");
}

static void test_read_safe_oob(void)
{
    printf("[test_read_safe_oob]\n");
    int h = m_alloc(5, sizeof(int), MFREE);
    m_setlen(h, 3);
    mls_errno = 0;
    int buf;
    void *dst = &buf;
    int r = m_read_safe(h, 10, &dst, 1);
    CHECK(r == -1, "m_read_safe OOB returns -1");
    CHECK(mls_errno == MLS_EBOUNDS || mls_errno == MLS_EINVAL,
          "mls_errno set appropriately");
    m_free(h);
}

static void test_setlen_safe_invalid(void)
{
    printf("[test_setlen_safe_invalid]\n");
    mls_errno = 0;
    int r = m_setlen_safe(-1, 100);
    CHECK(r == -1, "m_setlen_safe(-1, 100) returns -1");
    CHECK(mls_errno == MLS_EINVAL, "mls_errno == MLS_EINVAL");
}

static void test_setlen_safe_ok(void)
{
    printf("[test_setlen_safe_ok]\n");
    int h = m_alloc(5, sizeof(int), MFREE);
    mls_errno = 0;
    int r = m_setlen_safe(h, 3);
    CHECK(r == 0, "m_setlen_safe returns 0");
    CHECK(m_len(h) == 3, "length == 3");
    CHECK(mls_errno == 0, "mls_errno not set");
    m_free(h);
}

static void test_del_safe_oob(void)
{
    printf("[test_del_safe_oob]\n");
    int h = m_alloc(5, sizeof(int), MFREE);
    m_setlen(h, 3);
    mls_errno = 0;
    int r = m_del_safe(h, 10);
    CHECK(r == -1, "m_del_safe OOB returns -1");
    CHECK(mls_errno == MLS_EBOUNDS, "mls_errno == MLS_EBOUNDS");
    m_free(h);
}

static void test_del_safe_ok(void)
{
    printf("[test_del_safe_ok]\n");
    int h = m_alloc(5, sizeof(int), MFREE);
    int a = 1, b = 2, c = 3;
    m_put(h, &a); m_put(h, &b); m_put(h, &c);
    mls_errno = 0;
    int r = m_del_safe(h, 1);
    CHECK(r == 0, "m_del_safe returns 0");
    CHECK(m_len(h) == 2, "length == 2 after delete");
    CHECK(INT(h, 0) == 1 && INT(h, 1) == 3, "remaining values correct");
    m_free(h);
}

static void test_new_safe_variants(void)
{
    printf("[test_new_safe_variants]\n");

    int h = m_create_safe(4, sizeof(int));
    CHECK(h > 0, "m_create_safe returns handle");

    mls_errno = 0;
    int p = m_new_safe(h, 3);
    CHECK(p == 0 && m_len(h) == 3, "m_new_safe reserves 3 elements");

    int *e = (int *)m_add_safe(h);
    CHECK(e != NULL && m_len(h) == 4, "m_add_safe appends one element");

    int ok = m_ins_safe(h, 0, 1);
    CHECK(ok == 1 && m_len(h) == 5, "m_ins_safe inserts one element");

    int r = m_resize_safe(h, 1000);
    CHECK(r == 0 && m_bufsize(h) == 1000, "m_resize_safe grows capacity");

    mls_errno = 0;
    int bad = m_create_safe((size_t)-1, 2);
    CHECK(bad == -1 && mls_errno == MLS_EOVERFLOW,
          "m_create_safe overflow returns -1");

    m_free(h);
}

static void test_reset_errno(void)
{
    printf("[test_reset_errno]\n");
    mls_safe(-1, 0);
    CHECK(mls_errno != 0, "mls_errno set after error");
    mls_errno = 0;
    CHECK(mls_errno == 0, "user can reset mls_errno to 0");
}

int main(void)
{
    m_init();
    trace_level = 0;

    test_errmsg();
    test_mls_safe_invalid();
    test_mls_safe_oob();
    test_mls_safe_uaf();
    test_mls_safe_ok();
    test_put_safe_invalid();
    test_put_safe_ok();
    test_free_double();
    test_write_safe_invalid();
    test_read_safe_oob();
    test_setlen_safe_invalid();
    test_setlen_safe_ok();
    test_del_safe_oob();
    test_del_safe_ok();
    test_new_safe_variants();
    test_reset_errno();

    m_destruct();

    if (failed) {
        printf("\n%d tests FAILED\n", failed);
        return 1;
    }
    printf("\nAll error API tests passed.\n");
    return 0;
}
