#include "mls.h"

void parse_line(int vs, int ln)
{
    char **d;
    int i;

    int words = m_split(0, m_buf(ln), 32, 1 );
    if( m_len(words) < 2 ) return;

    char *name = STR(words,0);
    i=0; 
    while( m_next(words,&i,&d) ) {
	if( *d && **d ) {
	    v_set( vs, name, *d, -1 );
	}
    }
    
    m_free_strings(words,0);
}


int read_stat()
{
    TRACE(1,"START");
    int buf=m_create(100,1);
    FILE *fp;
    fp=fopen("/proc/stat","r");
    if (!fp)
	return -1;
    int vs = v_init();

    while( m_fscan(buf,'\n', fp ) == '\n' ) {
	parse_line(vs, buf);
	m_clear(buf);
    }
    m_free(buf);
    return vs;
}

void print_var(int k)
{
    char **s;
    int i;
    m_foreach( k, i, s ) {
	printf("%s ", *s );
    }
    printf("\n");
}

	    

int main()
{
    m_init();
    trace_level=1;

    int vs = read_stat();

    int *d,i;
    m_foreach( vs, i, d ) {
	print_var(*d);
    }
    v_free(vs);
    m_destruct();
}
