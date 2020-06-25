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

void json_stack( char *arg )
{
    m_put(MEM,&arg);
}

static int stack2 = 0;
static struct json_st *current, *root;
static struct json_st ROOT;

static void d_elem( struct json_st *obj );
static void d_obj(int l)
{
    printf("OBJ %d:", l );
    int p; struct json_st *d;
    m_foreach(l,p,d) {
	d_elem(d); 
    }
    printf("\n");
}

static void d_elem( struct json_st *obj )
{
    printf("e:");
    if( obj->name )
	printf("%s: ",CHARP(obj->name));

    switch( obj->typ ) {
    case JSON_OBJ:
	printf("obj %d\n", obj->d);
	d_obj(obj->d);
	break;
    case JSON_ARR:
	printf("arr %d\n", obj->d);
	d_obj(obj->d);
	break;
    case JSON_NUM:
	printf("num %s", CHARP(obj->d));
	break;
    case JSON_STRING:
	printf("str %s", CHARP(obj->d));
	break;
    case JSON_BOOL:
	printf("bool %s", CHARP(obj->d));
	break;
    case JSON_NULL:
	printf("null");
	break;
    }    
}


static void dump_stack(void)
{
    if(!root) return;
    d_elem(root);
    printf("\n---------------------\n\n");
}

void json_new(char *name, int t)
{
    TRACE(3,"NEW %s", name );
    struct json_st *json;
    if( stack2 == 0 ) {
	stack2=m_create(50,sizeof(struct json_st*));
	root = &ROOT;
	root->d = m_create(10,sizeof(struct json_st));
	root->typ = t;
	root->name = 0;
	current=root;
	m_put(stack2,&current);
	return;
    }

    TRACE(3,"append to list: %d", current->d );
    json = m_add(current->d);
    json->typ = t;
    json->name = 0;
    json->d = 0;
    
    switch( t ) {
    case JSON_OBJ:
    case JSON_ARR:
	m_put(stack2, &current);
	current = json;
	current->d = m_create(10,sizeof(struct json_st));
	return;
    default:
	json->d = s_printf(0,0,"%s", name );
    }
    return;
}

void json_close(void)
{
    current = *(struct json_st**)m_pop(stack2);
    TRACE(3,"current list: %d", current->d );
    dump_stack();
}

void json_name(char*name)
{
    struct json_st *json =
	mls(current->d, m_len(current->d)-1);
    json->name = s_printf(0,0,"%s", name );
}

int json_init( FILE *fp )
{
    MEM = m_create(20,sizeof(char*));
    yyin=fp;
    yyparse();
    m_free_strings( MEM, 0 );

    return ROOT.d;
}
