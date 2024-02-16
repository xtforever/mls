#include "njson_read.h"
#include "mls.h"

void d_obj2(FILE *fp, int opts, char *c1, char *c2);
void d_obj(int opts, char *c1, char *c2);
void dump_element(struct njson_st *obj)
{
  if( obj->name )
	printf("%s:",CHARP(obj->name));
  
  switch( obj->typ ) {
  case NJSON_OBJ:
    d_obj(obj->d," ( "," ) " );
    break;
  case NJSON_ARR:
    d_obj(obj->d," ( "," ) " );
    break;
  case NJSON_NUM:
    printf("%s", CHARP(obj->d));
    break;
  case NJSON_STRING:
    printf("\"%s\"", CHARP(obj->d));
    break;
  case NJSON_BOOL:
    printf("%s", CHARP(obj->d));
    break;
  case NJSON_NULL:
    printf("null");
    break;
  }    
}
void dump_element2(FILE *fp, struct njson_st *obj)
{
  if( obj->name )
      fprintf(fp,"%s:",CHARP(obj->name));
  
  switch( obj->typ ) {
  case NJSON_OBJ:
      d_obj2(fp,obj->d," ( "," ) " );
      break;
  case NJSON_ARR:
      d_obj2(fp,obj->d," ( "," ) " );
      break;
  case NJSON_NUM:
      fprintf(fp,"%s", CHARP(obj->d));
      break;
  case NJSON_STRING:
      fprintf(fp,"\"%s\"", CHARP(obj->d));
      break;
  case NJSON_BOOL:
      fprintf(fp,"%s", CHARP(obj->d));
      break;
  case NJSON_NULL:
      fprintf(fp,"null");
      break;
  }    
}

void d_obj_list(int opts)
{
  int p;
  struct njson_st *d;
  m_foreach(opts,p,d) {
    dump_element(d);
    if( p < m_len(opts)-1 ) putchar(',');
  }
}

void d_obj_list2( FILE *fp, int opts)
{
  int p;
  struct njson_st *d;
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


int njson_cmp(const char *name, struct njson_st *j)
{
  if( j->name ) return mstrcmp(j->name,0,name);
  return 1;
}

struct njson_st *njson_find_data(int opts, char *name)
{
  int p;
  struct njson_st *j;
  m_foreach(opts,p,j) {
    if( !njson_cmp(name,j) ) return j;
    if( j->typ >= NJSON_ARR ) {
      j=njson_find_data(j->d,name);
      if( j ) return j;
    }
  }
  return NULL;
}

char *njson_value( struct njson_st *j, int p )
{
  if( (!j) || (j->typ  == NJSON_OBJ) || (!j->d)) { return ""; }
  if(j->typ  == NJSON_ARR) {
    if( m_len(j->d) > p ) return njson_value( mls(j->d,p), 0 );
    return "";
  }  
  return CHARP(j->d);
}

char *njson_find(int opts, char *name, int p)
{
  struct njson_st *j = njson_find_data(opts, name);  
  return njson_value(j,p);
}


void test_read_write( char *inf, char *outf)
{
   FILE *fp;
   ASSERT( (fp=fopen( inf, "r" )));
   int opts = njson_from_file( fp );
   fclose(fp);
   ASSERT( (fp=fopen( outf, "w" )));
   d_obj2(fp, opts, "{\n", "\n}\n" );
   fclose(fp);
   njson_free(opts);
}

int  njson_read_file ( char *inf)
{
   FILE *fp;
   ASSERT( (fp=fopen( inf, "r" )));
   int opts = njson_from_file( fp );
   fclose(fp);
   return  opts;
}

int file_exists_c(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file != NULL) {
        fclose(file);
        return 1;
    }
    return 0;
}

int file_exists(int filename) {
  return file_exists_c( CHARP(filename) );
}



void dump_node( int opts, int p )
{
  struct njson_st *j = mls(opts,p);
  dump_element(j);
}


/* returns the data of a given object s or zero */
int search_obj_data( const char *s, int opts )
{
  struct njson_st *j;
  int p;
  m_foreach(opts,p,j) {
    if( njson_cmp( s, j ) == 0 ) {
      return j->d;
    }
  }
  return 0;
}

void append_list( int m, char *s, int opts0 )
{
  struct njson_st *j ;
  int p;

  /* seek for member s in this node obj data */
  int opts = search_obj_data( s,opts0 );
  if( opts <= 0 ) {
    TRACE(2,"not found");
    return;
  }
  
  m_foreach(opts,p,j) {
    if( j->d ) {
      TRACE(2,"%s", CHARP(j->d) );
      m_put( m, & j->d );
    }
  }
}

void dump_string_array(char *s, int opts)
{
  struct njson_st *j;
  int p,lst;
  printf("%s=(", s);
  lst = search_obj_data( s, opts  );
  m_foreach( lst,p,j) {
    printf("%s ", CHARP(j->d) );
  }
  printf(")\n");
}


int cmp_int( const void *a0, const void *b0 )
{
  const int *a = a0;
  const int *b = b0;
  return (*a) - (*b);
}
int cmp_mstr( const void *a0, const void *b0 )
{
  const int *a1 = a0;
  const int *b1 = b0;
  const char *a = CHARP( *a1 );
  const char *b = CHARP( *b1 );
  return strcmp( a,b );
}



static int create_node_str(  int obj, const char *key, int val )
{
  struct njson_st nj;
  nj.typ = NJSON_STRING;
  nj.name = s_printf(0,0,"%s", key );
  nj.d = s_printf(0,0,"%d", val );
  m_put( obj, &nj );
  return nj.d;
}

/* if node with name exists, return the value of the node data field
   else create the node as njson_string, convert val to m-string and save this in the node data field,
   then return the value of the node data field
*/
int fetch_node( int obj, const char *key    , int default_val )
{
  int d =  search_obj_data( key, obj  );
  if( !d ) d=create_node_str(obj,key, default_val );
  return d;
}

int fetch_node_int( int obj, const char *key, int default_val )
{
  int d = fetch_node(obj,key,default_val);
  return atoi( CHARP(d) );
}


/* if the node does not exists, create the node */
int set_node( int obj, const char *key, int val )
{
  int d =  search_obj_data( key, obj  );
  if( !d ) {
    return create_node_str(obj,key, val );
  }
  s_printf(d,0,"%d", val );
  return d;
}


void skip_ws(int m, int *p)
{
  
  while( isspace(CHAR(m,*p)) )
    {
      (*p)++;
    }
}
 
int get_num(int m, int *p)
{
  int ch;
  int val=0;
  while( isdigit(ch=CHAR(m,*p)) )
    {
      val *= 10; val+= ch - '0';
      (*p)++;
    }
  return val;
}


int parse_number_list(int str)
{
  int num,p0=0,p=0;
  int m = m_create(10,sizeof(int));
  
  while( p < m_len(str) ) {
    skip_ws( str, &p );
    p0=p;
    num=get_num(str,&p);
    if( p0==p ) break;
    m_put( m, &num );
  }
  return m;
}

/* can be hashed */
int get_exit_code(int id, int ec)
{
  int p,*d;
  m_foreach(ec,p,d) {
    p++;
    if( *d == id ) return INT(ec,p);
  }
  return -1;
}


void list_nodes( int global_state, int ec )
{
  int p;
  int *d;
  struct njson_st *j;
  int loop_count = 0;
  
  int nodes = search_obj_data( "nodes", global_state );
  if( !nodes ) {
    ERR("no member named 'nodes'");
  }

  loop_count = fetch_node_int( global_state, "loop_count", 0 );
  set_node(global_state, "loop_count", loop_count+1 );


  // m_foreach( ec,p,d ) {
  //  int id = *d++;
  //  p++;
  //  int e  = *d;
  //  TRACE(4, "id:%d e:%d", id, e );
  //}
  
  
  /* verify valid nodes and  create unique id entries */
  m_foreach(nodes,p,j) {
    if( j->typ < NJSON_ARR ) {
      TRACE(2, "Node %d invalid", p );
      return;
    }
    if( loop_count == 0 ) {
      set_node( j->d, "exit_code", 9 );
      set_node( j->d, "id", p+1 );
    } else {
      int id = fetch_node_int(j->d, "id", -1 );
      int x = get_exit_code(id,ec);
      if( x>=0 ) set_node( j->d, "exit_code", x );      
    }
    
  }

  /* get the nodes in list 'ec' and create checksums for our database, so if we
     restart we do not have to recompile them */
  
  
  int all_out   =  m_create( 100, sizeof(int) );
  int all_notgen   =  m_create( 100, sizeof(int) );
  int all_dep   =  m_create( 100, sizeof(int) );
  int all_in    =  m_create( 100, sizeof(int) );
  TRACE(2,"nr Nodes: %d", m_len(nodes));

  m_foreach(nodes,p,j) {
    TRACE(2,"OBJ %d", p );    
    append_list( all_out, "OUT", j->d );
    append_list( all_dep, "DEP", j->d );
    append_list( all_in,  "IN",  j->d );

    /* explicit declared not generated content */
    if( fetch_node_int( j->d, "exit_code", 1 ) ) {
      append_list( all_notgen, "OUT", j->d );
    }    
  }

  
  
  
  int in_wo_out = m_create( 100, sizeof(int) );
  m_qsort( all_in, cmp_mstr ); /* we like bsearch */
  m_qsort( all_out, cmp_mstr );
  m_qsort( all_notgen, cmp_mstr );

  /* generate list of files that are member of IN but not in OUT i.e. not generated */
  m_foreach( all_in,p,d) {
    if( m_bsearch( d, all_out, cmp_mstr ) < 0 )
      m_put(in_wo_out,d);     
  }

  TRACE(2,"\n-----------------------------\ndumping not available files");
  int *mstr;
  m_foreach(all_notgen,p,mstr) {
    TRACE(2,"%s", CHARP( *mstr ) );
  }
   TRACE(2,"\n-----------------------------\nfind compile ready nodes");  
   /* find nodes where the 'IN' list does not contain members of 'all_notgen'
      and 'exit_code' is zero
   */
  int comp_ready =  m_create( 100, sizeof(int) );
  /* RULE 2) if exit_code in node is zero, skip this node  */    
  int id;
  m_foreach(nodes,p,j) {    
    struct njson_st *j0;

    id = fetch_node_int(j->d, "id", 0 );
    TRACE(2, "checking node %d", id );

    int i,lst = search_obj_data( "exit_code",j->d  );
    if( *(CHARP(lst)) == '0' ) {
      TRACE(2, "exit_code zero: %d", p ); 
      continue;
    }

    
    
    lst = search_obj_data( "IN",j->d  );
    m_foreach( lst,i,j0) {

      int filename =  j0->d;
      
      if( m_bsearch( &filename, all_notgen, cmp_mstr ) >= 0 ) {
	TRACE(2, "%s found in all_notgen", CHARP(filename));
	goto skip_node;
      }

      if( !file_exists(filename) ) {
	TRACE(2, "file %s not found", CHARP(filename));
	goto skip_node;
      }
    }

    TRACE(2, "Checking DEP" );
    lst = search_obj_data( "DEP",j->d  );
    m_foreach( lst,i,j0) {

      int filename =  j0->d;
      
      if( m_bsearch( &filename, all_notgen, cmp_mstr ) >= 0 ) {
	TRACE(2, "%s found in all_notgen", CHARP(filename));
	goto skip_node;
      }

      if( !file_exists(filename) ) {
	TRACE(2, "file %s not found", CHARP(filename));
	goto skip_node;
      }
    }

    

    
    
    TRACE(2,"adding node %d", id );
    m_put( comp_ready, & j->d );
  skip_node:
  }

  /* dump comp ready nodes */
  //m_foreach(comp_ready,p,d) {
  //  d_obj2(stderr, *d,"(",")\n" );
  //}
  
  /* compile with rec and save the exit code, keep trying until exit code is zero or max_try_count exceeded */


  /* OPTIMIZE RULE: if files in out exists and files in in+dep have not changed, do not compile */



  d_obj(global_state, "global_state='(", ")'\n" );
  printf( "nr_jobs=%d\n",  m_len(comp_ready) );
  
  m_foreach(comp_ready,p,d) {
    int lst;
    dump_string_array( "IN", *d );
    dump_string_array( "OUT", *d );    
    lst = search_obj_data( "REC", *d  );
    int id = search_obj_data( "id", *d  );
    printf( "%s\nstore-result %s $?\n\n", CHARP(lst), CHARP(id) );
  }
  
  
  /* if exit code is zero remove all files in out-list from all_out and rebuild the comp_ready list */
  m_free(all_dep);
  m_free(all_out);
  m_free(all_in);
  m_free(in_wo_out);
  m_free(comp_ready);
  m_free(all_notgen);
  

  
};


void test_num(void)
{
  int *d,p;
  int m = s_printf(0,0, "1 2 3 4 5 6 7 8 9 a b c" );
  int s = parse_number_list( m );
  m_free(m);
  m_foreach( s,p,d ) {
    printf( "%d\n", *d );
  }
  
  m_free(s);
}


int main(int argc, char **argv)
{
  trace_level=4;
  m_init();
  TRACE(1,"start");
  int opts = njson_from_file(stdin);
  int buf = m_create(1000,1);

  if( argc > 1 ) {
    s_printf( buf,0, "%s", argv[1] );
  }
  
  int ec = parse_number_list(buf);
  m_free(buf);
  list_nodes(opts,ec);
  m_free(ec);
  njson_free(opts);
  m_destruct();
  return EXIT_SUCCESS;
}
