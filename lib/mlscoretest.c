#include "mls.h"

int h;
void verify_trace_info(void)
{
	static char *s = "hello";
	m_wrapcstr(s); /* no alloc */

	m_destruct();
	m_init();
	
	h = s_cstrdup(s); /* copy string -> alloc memory */
	printf("REAL ALLOC: %d %s\n", h, m_str(h) );
	
	s_ccstr("world"); /* no alloc: because constant c string */

	h = s_ccstr(s); /* "hello" is already alloced,no alloc */

	int ii[] = { 1 };
	m_wrapints(ii,1);

	char *ss[] = {"HELLO" };
	m_wrapstrings(ss,1);

	
	
}


int main(int argc,char**argv)
{
	(void) argc; (void) argv;
	m_init();
	trace_level = 1;
	verify_trace_info();
	printf( "%d:%s\n", h,m_str(h));
	m_destruct();
}
