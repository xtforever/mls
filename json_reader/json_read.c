/*
  json reader
  Jens Harms, 05.05.2019, <au1064> <at> <gmail.com>
*/

#include "mls.h"
#include "json_read.h"
#include "json_parse.tab.h"
#include "json_lex.lex.h"
#include <limits.h>
#include <stdlib.h>
int yyparse(void);
void set_input_string(const char* in);
void end_lexical_scan(void);



static int stack2 = 0;
static int current;

inline static void* m_last(int d)
{
  return mls(d, m_len(d)-1);
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
    if(m_len(stack2)==0) {
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
    /* number, boolean, null, string --  */
    /* It's all the same, only the names will change */
    /* Every day, it seems we're wastin' away */
    json->d = s_printf(0,0,"%s", value );
}

void json_close(void)
{
  if(!m_len(stack2)) {
      WARN("json syntax error. closing braces without opening");
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

int json_from_file( FILE *fp )
{
    yyin=fp;
    return json_parse();
}

int json_from_string(char *s)
{
  set_input_string(s);
  int ret = json_parse();
  return ret;
}

int json_parse(void)
{
    int root;
    stack2=m_create(50,sizeof(int));
    root = current = m_create(10,sizeof(struct json_st));
    int ret = yyparse(); /* sets global int root */
    m_free(stack2); stack2=0;
    if(ret) { json_free(root); root=0; WARN("json read error"); }
    end_lexical_scan();
    return root;
}

void json_free(int opt)
{
  if(!opt) return;
  int p;  struct json_st *j; 
  m_foreach( opt, p, j ) {
    m_free(j->name);
    if( (j->typ == JSON_OBJ) || (j->typ == JSON_ARR) )
      json_free(j->d);
    else m_free(j->d);
  }
  m_free(opt);
}
