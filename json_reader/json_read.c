/*
  ini file reader
  Jens Harms, 05.05.2019, <au1064> <at> <gmail.com>
*/

#include "mls.h"
#include "json_read.h"
#include "json_parse.tab.h"
#include "json_lex.lex.h"
#include <limits.h>
#include <stdlib.h>
int yyparse(void);

static int MEM = 0;
static int stack2 = 0;
static int current, root;

inline static void* m_last(int d)
{
  return mls(d, m_len(d)-1);
}

void json_stack( char *arg )
{
    m_put(MEM,&arg);
}

inline static struct json_st *add_element(int list, int typ)
{
  struct json_st *e = m_add(list);
  *e = (struct json_st){ .typ = typ }; 
  return e;
}

void json_new(char *value, int typ)
{
    TRACE(3,"NEW %s", value );
    struct json_st *json;
    if( stack2 == 0 ) {
	stack2=m_create(50,sizeof(int));
    }
    if(m_len(stack2)==0) {
      root = current = m_create(10,sizeof(struct json_st));
      m_put(stack2,&current);
      return;
    }

    TRACE(3,"append to list: %d", current );
    json = add_element(current,typ);
    if((json->typ == JSON_OBJ) || (json->typ == JSON_ARR)) {
	m_put(stack2, &current);
	current = json->d = m_create(10,sizeof(struct json_st));
	return;
    }
    json->d = s_printf(0,0,"%s", value );
}

void json_close(void)
{
  if(!m_len(stack2)) {
    fprintf(stderr,"json syntax error. closing braces without opening");
    return;
  }
  current = *(int*)m_pop(stack2);
  TRACE(3,"current list: %d", current );
}

void json_name(char*name)
{
  struct json_st *json = m_last(current);
  json->name = s_printf(0,0,"%s", name );
}

int json_init( FILE *fp )
{
    MEM = m_create(20,sizeof(char*));
    yyin=fp;
    int ret = yyparse();
    m_free_strings( MEM, 0 );
    m_free(stack2);
    if(ret) { json_free(root); root=0; WARN("json read error"); } 
    return root;
}

void json_free(int opt)
{
  if(!root) return;
  int p;  struct json_st *j; 
  m_foreach( opt, p, j ) {
    if( j->name ) m_free(j->name);
    if( (j->typ == JSON_OBJ) || (j->typ == JSON_ARR) )
      json_free(j->d);
    else m_free(j->d);
  }
  m_free(opt);
}
