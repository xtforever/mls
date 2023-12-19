#include "json_read.h"
#include "../lib/mls.h"

void printm(int s)
{
  if( s >0 && m_len(s) )
    puts( (char*) m_buf(s) );
  else
    puts("null");
}

void d_obj2(FILE *fp, int opts, char *c1, char *c2);
void d_obj(int opts, char *c1, char *c2);
void dump_element(struct json_st *obj)
{
  if( obj->name )
	printf("\"%s\":",CHARP(obj->name));

  switch( obj->typ ) {
  case JSON_OBJ:
    d_obj(obj->d," { "," } " );
    break;
  case JSON_ARR:
    d_obj(obj->d," [ "," ] " );
    break;
  case JSON_NUM:
    printf("%s", CHARP(obj->d));
    break;
  case JSON_STRING:
    printf("\"%s\"", CHARP(obj->d));
    break;
  case JSON_BOOL:
    printf("%s", CHARP(obj->d));
    break;
  case JSON_NULL:
    printf("null");
    break;
  }
}
void dump_element2(FILE *fp, struct json_st *obj)
{
  if( obj->name )
      fprintf(fp,"\"%s\":",CHARP(obj->name));

  switch( obj->typ ) {
  case JSON_OBJ:
      d_obj2(fp,obj->d," { "," } " );
      break;
  case JSON_ARR:
      d_obj2(fp,obj->d," [ "," ] " );
      break;
  case JSON_NUM:
      fprintf(fp,"%s", CHARP(obj->d));
      break;
  case JSON_STRING:
      fprintf(fp,"\"%s\"", CHARP(obj->d));
      break;
  case JSON_BOOL:
      fprintf(fp,"%s", CHARP(obj->d));
      break;
  case JSON_NULL:
      fprintf(fp,"null");
      break;
  }
}

void d_obj_list(int opts)
{
  int p;
  struct json_st *d;
  m_foreach(opts,p,d) {
    dump_element(d);
    if( p < m_len(opts)-1 ) putchar(',');
  }
}

void d_obj_list2( FILE *fp, int opts)
{
  int p;
  struct json_st *d;
  m_foreach(opts,p,d) {
      dump_element2(fp,d);
      if( p < m_len(opts)-1 ) fputc(',', fp);
  }
}

void d_obj(int opts, char *c1, char *c2)
{
  printf("%s",c1);
  d_obj_list(opts);
  printf("%s",c2);
}

void d_obj2(FILE *fp, int opts, char *c1, char *c2 )
{
    fprintf(fp,"%s",c1);
    d_obj_list2(fp, opts);
    fprintf(fp, "%s",c2);
}




int json_cmp(char *name, struct json_st *j)
{
  if( j->name ) return mstrcmp(j->name,0,name);
  return 1;
}

struct json_st *json_find_data(int opts, char *name)
{
  int p;
  struct json_st *j;
  m_foreach(opts,p,j) {
    if( !json_cmp(name,j) ) return j;
    if( j->typ >= JSON_ARR ) {
      j=json_find_data(j->d,name);
      if( j ) return j;
    }
  }
  return NULL;
}

char *json_value( struct json_st *j, int p )
{
  if( (!j) || (j->typ  == JSON_OBJ) || (!j->d)) { return ""; }
  if(j->typ  == JSON_ARR) {
    if( m_len(j->d) > p ) return json_value( mls(j->d,p), 0 );
    return "";
  }
  return CHARP(j->d);
}

char *json_find(int opts, char *name, int p)
{
  struct json_st *j = json_find_data(opts, name);
  return json_value(j,p);
}


void test_read_write( char *inf, char *outf)
{
   FILE *fp;
   ASSERT( (fp=fopen( inf, "r" )));
   int opts = json_from_file( fp );
   fclose(fp);
   ASSERT( (fp=fopen( outf, "w" )));
   d_obj2(fp, opts, "{\n", "\n}\n" );
   fclose(fp);
   json_free(opts);
}

struct json_ctx {
  int init;
  int data;
  int obj;
  int pos;
  int copy;
  int stack;
};

int JSON_CTX = 0;

int ctx_init(int *CTX, int length, int size)
{
    if(! *CTX) {
	 *CTX = m_create( length, size);
    }

    /* find empty element */
    int *d;
    int p;
    m_foreach(*CTX,p,d) {
	if( *d == 0 ) goto found;
    }
    p = m_new(*CTX,1);
    d = mls(*CTX, p);
 found:
    *d=1;
    return p;
 }

struct json_ctx *json_get( int ctx )
{
  return mls( JSON_CTX, ctx );
}


int j_create( int data, int is_ref )
{
  int js = ctx_init( &JSON_CTX, 10, sizeof(struct json_ctx));
  struct json_ctx *jj =json_get( js );
  jj->data = data; /* list of level 1 entries */
  jj->copy=is_ref;
  jj->obj=data; jj->pos=0;
  jj->stack=0;
  return js;
}

int j_new(void)
{
  return j_create( m_create(1,sizeof(struct json_st)), 0);
}

int j_free( int js )
{
  struct json_ctx *jj =json_get( js );
  jj->init = 0;
  if(! jj->copy ) json_free(jj->data);
  if( jj->stack ) m_free(jj->stack);
  return 0;
}


int j_fromfile( char *inf )
{
   FILE *fp;
   if( ! (fp=fopen( inf, "r" ))) {
     return j_new();
   }

   int opts = json_from_file( fp );
   fclose(fp);
   return j_create(opts,0);
}

/* j_begin(), j_set(csv); j_get(i) */

/** return the top element on the stack as integer */
int m_popi(int m) { return *(int *)m_pop(m); }


int j_set (int js )
{
  struct json_ctx *jj = json_get( js );
  struct json_st *jd  = mls( jj->obj, jj->pos );

  if( jd->typ < JSON_ARR ) return 1;

  jj->obj = jd->d;
  jj->pos = 0;
  return 0;
}

/* push and set current to current->nodes */
int j_push(int js)
{
    struct json_ctx *jj = json_get( js );
    if(!jj->stack) jj->stack = m_create(4,sizeof(int));
    m_puti( jj->stack, jj->obj);
    m_puti( jj->stack, jj->pos);
    return j_set(js);
}

int j_pop(int js)
{
    struct json_ctx *jj = json_get( js );
    if( !jj->stack || m_len(jj->stack) < 2 ) return 1;
    jj->pos = m_popi( jj->stack );
    jj->obj = m_popi(jj->stack);
    return 0;
}

int j_step(int js)
{
    struct json_ctx *jj = json_get( js );
    if( jj->pos+1 >= m_len(jj->obj) ) return 1;

    jj->pos++;
    return 0;
}

int j_data(int js)
{
  struct json_ctx *jj = json_get( js );
  struct json_st *jd  = mls( jj->obj, jj->pos );
  return jd->d;
}
int j_type(int js)
{
  struct json_ctx *jj = json_get( js );
  struct json_st *jd  = mls( jj->obj, jj->pos );
  return jd->typ;
}
int j_name(int js)
{
  struct json_ctx *jj = json_get( js );
  struct json_st *jd  = mls( jj->obj, jj->pos );
  return jd->name;
}



int j_begin (int js )
{
  struct json_ctx *jj = json_get( js );
  jj->obj = jj->data;
  jj->pos = 0;
  if( jj->stack ) m_clear(jj->stack);
  return 0;
}

/** search entire current obj/arry for element name */
int j_goto( int js, char *name )
{
  struct json_ctx *jj =json_get( js );
  struct json_st *jd;
  m_foreach( jj->obj, jj->pos, jd )
    {
      if( !json_cmp(name,jd) ) return 0;
    }

  return 1;
}

/** get a new context for this object, ctx needs to be removed with j_free(obj) */
int j_getnextobj(int js)
{
  struct json_ctx *jj = json_get( js );
  if( jj->pos >= m_len(jj->obj) ) return 0;
  struct json_st *jd  = mls( jj->obj, jj->pos++ );
  if( jd->typ < JSON_ARR ) return 0;
  return j_create( jd->d, 1 );
}


/** lookup name on top level csv node */
int j_findname(int js, char *name)
{
  int p;
  struct json_ctx *jj =json_get( js );
  struct json_st *jd;
  m_foreach( jj->data, p, jd )
    {
      if( !json_cmp(name,jd) ) {
        jj->obj = jj->data;
        jj->pos = p;
        return 1;
      }
    }

  return 0;
}

int j_dump(int js)
{
  struct json_ctx *jj =json_get( js );
  struct json_st *jd = mls( jj->obj, jj->pos );
  puts("dumping obj");
  printf("********************************** %d %d\n", jj->obj, jj->pos );
  dump_element( jd );
  return 0;
}




void j_print_simple( FILE *fp, struct json_st *obj )
{
  switch( obj->typ ) {
  case JSON_NUM:
      fprintf(fp,"%s", CHARP(obj->d));
      break;
  case JSON_STRING:
      fprintf(fp,"\"%s\"", CHARP(obj->d));
      break;
  case JSON_BOOL:
      fprintf(fp,"%s", CHARP(obj->d));
      break;
  case JSON_NULL:
      fprintf(fp,"null");
      break;
  default:
      fprintf(fp,"*ERROR:not an json simple type*\n");
      break;
  }
}

void j_print_node(int js)
{
  struct json_ctx *jj =json_get( js );
  struct json_st *jd = mls( jj->obj, jj->pos );
  if( jd->name ) {
    printm( jd->name );
    printf(":");
  }
  j_print_simple(stdout,jd);
  printf("\n");
}

/** dump an array of array of simple elements */
void j_dump_csv(int js)
{
  struct json_st *jd1,*jd2;
  int row,col;
  struct json_ctx *jj =json_get( js );

  /* this should be an array of arrays */
  jd1 = mls( jj->obj, jj->pos );
  if( jd1->typ !=  JSON_ARR ) return;
  int list = jd1->d; /* this is my arry of rows */
  m_foreach( list, row, jd1 ) {
    if( jd1->typ !=  JSON_ARR ) return; /* we want an array of elements */
    int comma=10;
    m_foreach( jd1->d, col, jd2 ) {
      fprintf(stdout,"%c",comma ); j_print_simple( stdout, jd2 ); comma=',';
    }
  }
  fprintf(stdout,"%c",10 );

}



void test_json_csv(void)
{

  int line;
  int js = j_fromfile( "csv1.json" );

  if( j_findname( js, "csv" ) )
    j_dump_csv( js );


  j_begin(js);
  j_goto(js,"csv");
  j_set(js); /* iterate over childs of csv node */

  while( (line = j_getnextobj(js))   ) {
    j_dump(line);
    j_free(line);
  }


  printf("\n********************\n new test \n");
  j_begin(js);

  while(1) { /* for each node on current level */
    j_print_node( js );

    if( j_type(js) >= JSON_ARR ) {
      j_push(js);
      continue; /* skip j_step() */
    }

  step_again:
    if( ! j_step(js) ) continue;
    if( ! j_pop(js) ) goto step_again;
    break;
  }

  j_free(js);
}

int main(int argc, char **argv)
{

  trace_level=3;
  m_init();
  TRACE(1,"start");
  // test_read_write( "ex1.json", "ex1-o.json");

  test_json_csv();
  m_destruct();
  exit(0);

  FILE *fp;
  ASSERT( (fp=fopen( "settings.json", "r" )));
  int opts = json_from_file( fp );
  fclose(fp);

  TRACE(3,"opts %d", opts);
  if( !opts ) ERR("could not read settings.json");
  printf("\n***************\n");
  d_obj(opts, "{\n", "\n}\n");
  char *s = json_find(opts,"o2",2 );
  printf("\no2{2}=%s\n", s );
  json_free(opts);


  char *json_test;
  json_test =
    "{ \"top\": [  { \"tobj\": { \"a\":6 }  }  ] ,\"o2\": [ \"x\", { \"num\":5,\"cat\":6 } ,\"y\" ] }";

  for(int i=0;i<10;i++) {
    opts = json_from_string(json_test);
    ASSERT(opts); d_obj(opts, "{\n", "\n}\n");
    s = json_find(opts,"o2",2 );
    printf("\no2{2}=%s\n", s );
    json_free(opts);
  }

  m_destruct();



  return EXIT_SUCCESS;
}
