#include "mls.h"


int cmp_char(const void *a,const void *b )
{
    const char *ch1, *ch2;
    ch1 = a; ch2 = b;
    return (*ch1) - (*ch2);
}

void mputs( int buf )
{
    fwrite( m_buf(buf), m_width(buf), m_len(buf), stdout );
    putchar(10);
}


int is_unsorted(int buf, int (*cmpf) (const void *a,const void *b ) )
{
    int p=0;
    while( p < m_len(buf)-1 ) {
	if( cmpf( mls(buf,p), mls(buf,p+1)) > 0 )
	    return -1;
	p++;
    }
    return 0;
}

void one_test(int buf, char *a )
{
    m_binsert( buf, a, cmp_char );
    if( is_unsorted(buf, cmp_char) ) ERR("list is not sorted");
    mputs(buf);
}


void some_test(int buf)
{
    char nr[1];
    if( m_len(buf) )
	printf("Using %.*s\n", m_len(buf), (char*) m_buf(buf));
    
    for(int i=0;i<10;i++) {
	nr[0]=i+48;
	printf("insert %c: ", *nr );
	one_test(buf, nr );
    }
}

void copy_chars(int buf, char *s)
{
    m_clear(buf);
    m_write(buf,0,s,strlen(s));
}


int  main(int argc, char ** argv)
{
    char nr[1];
    m_init();
    trace_level=2;

    
    int buf = m_create(100, 1);
    some_test(buf);

    copy_chars(buf,"3");
    some_test(buf);

    copy_chars(buf,"39");
    some_test(buf);
    
    copy_chars(buf,"33");
    some_test(buf);

    copy_chars(buf,"358" );
    some_test(buf);


    // string einer max. laenge von 8
    for(int check=5000;check--;) {
	m_clear(buf);
	int len = rand() % 100;
	while(len--) {
	    nr[0] = 48 + (rand() % 10);
	    one_test( buf, nr  );
	}
    }
    
    m_free(buf);
    m_destruct();
    
}
