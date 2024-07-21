#include "mls.h"

/* expand a rule
   suffix: new suffixes
   e.g: .y : .tab.c .tab.h
*/
int expand_file_names( int rule, int inf, int OUT )
{

  return 0;
}

int get_word_list(int m, int buf, int *p0)
{
	int ch, str = s_new(20);
	int p=*p0;
	while(p < m_len(buf)) {
		TRACE(2,"%d scan: %s",p, (char*)mls(buf,p));
		ch = p_word(str, buf,&p, ": \n\\" );
		if( ! s_empty(str) ) {
			m_puti(m, s_mstr(str) );
		}
		m_clear(str);
		if( ch == ':' || ch == 0 ) break;
		p++;
	}
	m_free(str);
	*p0=p;
	return m_len(m);
}

void skip_ws(int m, int *p)
{
  
  while( isspace(CHAR(m,*p)) )
    {
      (*p)++;
    }
}
 
int rule_compile( int str, int rule_ret )
{
  int p; char *ch;
  m_foreach( str, p, ch ) {
    if(! isspace(*ch) ) break;
  }
  
  
}

void main()
{
  m_init();
  trace_level=1;
  int inf = s_printf(0,0, "parser.y" );
  int rule_str = s_printf(0,0, ".y .tab.c .tab.h" );
  int rule  = m_create( 10, sizeof(int));
  int out = m_create( 10, sizeof(int));
  expand_file_names(rule, inf, out );
  m_destruct();
}
