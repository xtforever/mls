#include "mls.h"
#include <stdarg.h>
/* alphabetisch sortierte liste von mstr */
static int CONSTSTR_DATA=0;

static void m_free_list(int m)
{
	int p,*d;
	m_foreach( m, p, d ) {
	  TRACE(1, "STR: %s", CHARP(*d));
	  m_free(*d);
	}
	m_free(m);
}

void  conststr_free(void)
{
  m_free_list( CONSTSTR_DATA);
  CONSTSTR_DATA=0;
}

void conststr_init(void)
{
  if( !  CONSTSTR_DATA )
    CONSTSTR_DATA=m_create(10,sizeof(int));
}

static int mscmp(const void *a, const void *b)
{
	int k1 = *(const int *)a;
	int k2 = *(const int *)b;
	TRACE(1,"cmp %s %s", CHARP(k1), CHARP(k2));
	return m_cmp(k1,k2);
}
static int mscmpc( const void *a,const void *b )
{
  const char * const *s = a;
  const int *d = b; 
  return -mstrcmp( *d, 0, *s );
}

/* schau nach ob s schon vorhanden ist, wenn ja liefere die vorhandene kopie, sonst fuege ein kopie von s hinzu
   IDEE: x=m_create(); m_put(x,data); z= conststr_lookup(x); free(x); 
 */
int conststr_lookup(int s)
{
  int p = m_binsert( CONSTSTR_DATA, &s, mscmp, 0 );
  if( p < 0 ) { // schon vorhanden
    return INT( CONSTSTR_DATA, (-p)-1 );
  }
  TRACE(4,"ADD %d %s", p, CHARP(s) );
  return INT(CONSTSTR_DATA,p) = m_dub(s);
}

int conststr_lookup_c(const char *s)
{
  union { const char *s; int key; } key;
  key.s = s;
  int p = m_binsert( CONSTSTR_DATA, &key, mscmpc, 0 );
  if( p < 0 ) {
    return INT( CONSTSTR_DATA, (-p)-1 );
  }
  int k = s_printf(0,0, "%s", s );
  INT(CONSTSTR_DATA,p) = k;
  return k;
}


int cs_printf( int m, int p, char *format, ... )
{
    va_list ap;
    va_start(ap,format);
    m = vas_printf( m,p, format, ap );
    va_end(ap);

    p = m_binsert( CONSTSTR_DATA, &m, mscmp, 0 );
    if( p < 0 ) { // schon vorhanden
      m_free(m);
      return INT( CONSTSTR_DATA, (-p)-1 );
    }
    return m;
}


#ifdef WITH_CONSTR_MAIN

void test_sort_lookup(void)
{
	char *s[] = { "prepare","test10.b", "test1.a", "test1.b" };
	for(int i=0;i<10;i++) {
	  conststr_lookup_c(s[i % 4]);
	}

	for( int i=0; i< 20; i++ ) {
	  int k  = s_printf(0,0, "%s", s[ i % 4] );
	  int k2 = conststr_lookup( k );
	  TRACE(1, "%d != %d", k, k2 );
	  m_free(k);
	}
}

	

int main()
{
  trace_level=1;
  m_init();
  TRACE(1,"start");

  conststr_init();
  test_sort_lookup();
  conststr_free();
  
  m_destruct();
 
}
#endif

