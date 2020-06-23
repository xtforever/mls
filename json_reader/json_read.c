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


