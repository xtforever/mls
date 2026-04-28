#include "mls.h"
#include "m_tool.h"
#include "m_extra.h"
/* Conflict resolution: mls.h defines ASSERT, so does greatest.h.
   We undefine the one from mls.h before including greatest.h */
#undef ASSERT
#include "greatest.h"
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

/* Define a suite for core memory and list functions */
SUITE (mls_core_suite);

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

static void crash_putc(int m)
{
	m_putc(m,0);
}
static void crash_write(int m)
{
	m_write(m,0, "12345678901234", 4 );
}


TEST test_m_alloc_free (void)
{
	int h = m_alloc (10, 1, MFREE_NOALLOC );
	static const char *str = "constant string";
	m_set_data( h, 10, 1, str ); 
	ASSERT (h > 0);
	ASSERT_EQ (10, m_len (h));
	ASSERT_EQ (1, m_width (h));
	m_free (h);
	int m =	m_set_data( 0, 10, 1, str );
	printf( "%d %d\n", h, m );
	ASSERT( m != h && h == (m & 0xffffff) );
	int x = s_dup(str);
	ASSERT_EQ(0, s_ncasecmp(m,x,10) );
	m_free(x);

	ASSERT( check_if_exit( crash_putc, m ) );
	ASSERT( check_if_exit( crash_write, m ) );
	m_free(m);
	PASS ();
}

TEST test_m_put_get (void)
{
	int h = m_alloc (0, sizeof (int), MFREE);
	int val = 42;
	m_put (h, &val);
	ASSERT_EQ (1, m_len (h));

	int *ptr = (int *)m_peek (h, 0);
	ASSERT_EQ (42, *ptr);

	int retrieved = 0;
	void *p_ret = &retrieved;
	m_read (h, 0, &p_ret, 1);
	ASSERT_EQ (42, retrieved);

	m_free (h);
	PASS ();
}


TEST test_m_del_remove (void)
{
	int h = m_alloc (0, sizeof (int), MFREE);
	for (int i = 0; i < 5; i++)
		m_puti (h, i);

	/* [0, 1, 2, 3, 4] */
	m_del (h, 2); /* Delete element at index 2 (value 2) */
	/* Should be [0, 1, 3, 4] */
	ASSERT_EQ (4, m_len (h));
	ASSERT_EQ (3, *(int *)m_peek (h, 2));

	m_remove (h, 1, 2); /* Remove 2 elements starting at index 1 */
	/* Should be [0, 4] */
	ASSERT_EQ (2, m_len (h));
	ASSERT_EQ (4, *(int *)m_peek (h, 1));

	m_free (h);
	PASS ();
}

TEST test_wrappers1 (void)
{
	trace_level=1;
	int h1 = s_ccstr("hello\n");
	int h2 = s_ccstr("hello\n");
	int h3 = s_ccstr("world\n");
	ASSERT_EQ (h1,h2);
	char *s1 = m_str(h1);
	char *s2 = m_str(h3);
	ASSERT_EQ(0, strcmp(s1,"hello\n") );
	ASSERT_EQ(0, strcmp(s2,"world\n") );
	PASS ();
}

TEST test_wrappers2 (void)
{
	int ii[] = { 1,2,3 };
	int h =  m_wrapints( ii, 3 );

	ASSERT_EQ( 1, INT(h,0) );
	ASSERT_EQ( 3, INT(h,2) );
	m_free(h);

	char *w[] = { "hello\n", "world\n", NULL };
	h =  m_wrapstrings( w, 3 );

	char *s1,*s2;
	s1 = STR( h, 0 );
	s2 = STR( h, 1 );
	
	ASSERT_EQ( 3, m_len(h) );
	ASSERT_EQ(0, strcmp(s1,"hello\n") );
	ASSERT_EQ(0, strcmp(s2,"world\n") );

	char mod[] =  "hello world\n";
	int h2 = s_cstrdup(mod);
	int h3 = m_wrapcstr(mod);
	mod[0]='x';
	ASSERT_EQ( 0, strcmp(m_str(h2),"hello world\n") );
	ASSERT_EQ( 0, strcmp(m_str(h3),"xello world\n") );
	m_free(h2);
	m_free(h3);
	
	PASS ();
	

	
}

/* Suite definitions */
GREATEST_SUITE (mls_core_suite)
{
	RUN_TEST (test_m_alloc_free);
	RUN_TEST (test_m_put_get);
	RUN_TEST (test_m_del_remove);
	RUN_TEST (test_wrappers1);
	RUN_TEST (test_wrappers2);
}

/* Main function */
GREATEST_MAIN_DEFS ();

int main (int argc, char **argv)
{
	GREATEST_MAIN_BEGIN ();
	m_init ();

	RUN_SUITE (mls_core_suite);

	m_destruct();
	GREATEST_MAIN_END ();
}
