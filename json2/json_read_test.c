#include "json_read.h"
#include "mls.h"

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

int  json_read_file ( char *inf)
{
   FILE *fp;
   ASSERT( (fp=fopen( inf, "r" )));
   int opts = json_from_file( fp );
   fclose(fp);
   return  opts;
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



int main(int argc, char **argv)
{
  trace_level=3;
  m_init();
  TRACE(1,"start");

  int nodes = json_read_file( "nodes.json" );
  
  

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
