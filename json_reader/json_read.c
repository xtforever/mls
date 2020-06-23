/*
  ini file reader
  Jens Harms, 05.05.2019, <au1064> <at> <gmail.com>
*/

#include "mls.h"
#include "json_parse.tab.h"
#include "json_lex.lex.h"
#include <limits.h>
#include <stdlib.h>

int yyparse(void);

/* _vname == OBJ */
static int LIST = 0;
static int STACK = 0;
static int ARG = 0;
static int MEM = 0;

static int create_arg(char *first)
{
    char *null = NULL;
    char * v = strdup(first);
    int a = m_create(2,sizeof(char*));
    m_put(a,&null); m_put(a,&v);
    return a;
}

int json_init( FILE *fp )
{
    STACK = m_create(20,sizeof(int));
    LIST = 0;
    ARG = 0;
    MEM = m_create(20,sizeof(char*));
    yyin=fp;
    yyparse();
    m_free_strings( MEM, 0 );
    m_free(STACK);
    m_free_strings(ARG,0); /* root arg object */ 
    return LIST;
}


/* push current LIST, create a new LIST */
void json_object_push(void)
{
    TRACE(1,"OLD: %d", LIST);
    m_put( STACK, &LIST );
    LIST = v_init();
    TRACE(1,"CUR: %d", LIST);
}

    
/*  set ARG to current list-obj, pop LIST from stack */
void json_object_pop(void)
{
    char num[20];
    snprintf(num, sizeof num, "%c%d",0x01, LIST );
    ARG = create_arg( num );

    int * v = m_pop(STACK); 
    if( v && *v ) LIST = *v; /* do not pop last ITEM */
    TRACE(1,"CUR: %d", LIST);
}


/* create ARG if not exists, prepend ARG arguments with arg */
void json_array( char *arg )
{
    TRACE(1,"%s", arg );
    if( !ARG ) {
	ARG = create_arg( arg );
	return;
    }
    char * v = strdup(arg);
    m_ins( ARG, 1, 1 );
    m_write( ARG,1, &v, 1);
}

void json_pair( char *name, char *val )
{
    TRACE(1,"%s:%s", name,val );
    if( !ARG ) {
	ARG = create_arg( val );
    }
    char *v = strdup(name);
    m_write( ARG,0, &v, 1);

    TRACE(1,"%d,%d", LIST, ARG );
    m_put( LIST, &ARG );
    ARG = 0;
}

void json_stack( char *arg )
{
    m_put(MEM,&arg);
}



enum json_typ {
	       JSON_STRING,
	       JSON_NUM,
	       JSON_BOOL,
	       JSON_NULL,
	       JSON_ARR,
	       JSON_OBJ
};

static int stack2 = 0;
struct json_st {
    int name;
    int typ;
    int d;
};
static struct json_st *current, *root;

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
	root = malloc(sizeof(struct json_st));
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
