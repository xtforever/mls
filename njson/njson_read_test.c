#include "njson_read.h"
#include "mls.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>





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


int mscmp(const void *a, const void *b)
{
	int k1 = *(const int *)a;
	int k2 = *(const int *)b;
	TRACE(1,"cmp %s %s", CHARP(k1), CHARP(k2));
	
	return m_cmp(k1,k2);
}


void verify_sort_lookup(int m)
{
	
}


int sort_lookup(int m, int key)
{
	int key_cp = m_dub(key);
	int p = m_binsert( m, &key_cp, mscmp, 0 );
	if( p < 0 ) {
		m_free(key_cp);
		p=(-p)-1;
	}
	TRACE(1,"%d", p);
	return p;
}

void dump_list(char *s, int id, int fn)
{
	puts(s);
	int p,*d;
	m_foreach(id,p,d) {
		int s = INT(fn,*d);
		printf("%s\n", CHARP(s) );
	}
	puts("");
}

void dump_mstrarray(int m)
{
	int p,*d;
	m_foreach(m,p,d) {
		printf("%d %s\n",p, CHARP(*d) );
	}
}


#if 0
void test_sort_lookup(void)
{
	int p1,p2;
	int id = m_create(100,sizeof(int));

	p1 = s_printf(0,0, "test1" );
	p2 = sort_lookup( id, p1 );
	TRACE(4,"pos: %d", p2 );
	dump_list( id);	
	
	p1 = s_printf(0,0, "test1" );
	p2 = sort_lookup( id, p1 );
	TRACE(4,"pos: %d", p2 );
	dump_list( id);

	p1 = s_printf(0,0, "test2" );
	p2 = sort_lookup( id, p1 );
	TRACE(4,"pos: %d", p2 );
	dump_list( id);
	
	p1 = s_printf(0,0, "test3" );
	p2 = sort_lookup( id, p1 );
	TRACE(4,"pos: %d", p2 );
	dump_list( id);
	
	p1 = s_printf(0,0, "aaaaa" );
	p2 = sort_lookup( id, p1 );
	TRACE(4,"pos: %d", p2 );
	dump_list( id);

	exit(1);
}
#endif


void app_names( int m, int fnid, char *s, int opts0 )
{
  struct njson_st *j ;
  int p;
  int opts = search_obj_data( s,opts0 );
  if( opts <= 0 ) {
    TRACE(2,"not found");
    return;
  }
  
  m_foreach(opts,p,j) {
	  if( j->d == 0 ) continue;	  
	  int id = sort_lookup( fnid, j->d );
	  TRACE(1, "ADD %s %d %d", CHARP(j->d), id, m_len(fnid) );
	  m_puti( m, id );
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
    if( p >= m_len(ec) ) return -1;
    if( *d == id ) {
	    return INT(ec,p);
    }
  }
  return -1;
}




typedef struct cs_st {
  char fn[256];
  struct timespec mtim;
} cs_t;

void dump_checksum(int cs)
{
	cs_t *d; int p;
	m_foreach( cs, p, d ) {
		printf("%d %s %s\n",p, d->fn, ctime( & d->mtim.tv_sec ) );
	}
	printf("-------------\n");
}

int read_checksum( const char *fn )
{
  int inc = 10;
  int len = 0;
  int rd;
  int cs = m_create( inc, sizeof( cs_t));
  m_setlen( cs,inc );
  
  FILE *fp = fopen(fn, "rb" );
  if(!fp) return cs;

  while( (rd=fread( mls(cs,len), sizeof(cs_t), inc, fp )) == inc ) {
    len+=inc; 
    m_setlen( cs,len+inc ); 
  }
  len+=rd; 
  m_setlen( cs,len );
  return cs;
}


int cscmpmstr( const void *a,const void *b )
{
  const int *fn = a;
  const cs_t *d = b;
  return mstrcmp( *fn, 0, d->fn );
}
int cscmp( const void *a,const void *b )
{
  const cs_t *a0 = a;
  const cs_t *b0  = b;
  return strncmp(a0->fn,b0->fn,sizeof( a0->fn) );
}


/* read file modified date and store in checksum list */
void update_checksum_file( int cs, int fn )
{
  cs_t *d;
  struct stat sb = { 0 };
  int err = stat( CHARP(fn), &sb );
  int pos = m_bsearch( &fn, cs, cscmpmstr );
  if( err ) {		   /* if no file then remove node if exists */
    m_del( cs, pos );
    return;
  }

  TRACE(4, "%s:%s", CHARP(fn), ctime( & (sb.st_mtim.tv_sec) ));
  
  if(pos<0) {			/* if no node then create node */
    cs_t tmp;
    tmp.mtim = sb.st_mtim;
    snprintf(tmp.fn,sizeof(tmp.fn), "%s", CHARP(fn));
    pos = m_binsert( cs, &tmp, cscmp, 0 );
    return;
  }

  /* update node */
  d=mls(cs,pos);
  d->mtim = sb.st_mtim;
}

void update_checksum_node( int cs, int node )
{
  int p;
  struct njson_st *j;
  m_foreach( node,p,j ) {
    update_checksum_file(cs,j->d);
  }
}

void save_checksum( int cs, const char *fn )
{
  FILE *fp = fopen(fn, "wb" );
  ASERR(fp, "could not create/open for writing: %s" );
  int w = fwrite( m_buf(cs), m_width(cs), m_len(cs), fp );
  ASERR( w==m_len(cs), "checksum file not written" );
  ASERR( fclose(fp)==0, "checksum file error on close()");
}

/* return true if there is no entry for fn or modifed date has changed
   or fn does not exists

   return false if modified date is unchanged
 */
int ismodified_checksum( int cs, int fn )
{
  struct stat sb = { 0 };
  int pos = m_bsearch( &fn, cs, cscmpmstr );
  

  
  if( pos < 0 ) return 1;
  cs_t *d = mls(cs,pos);
  int err = stat( CHARP(fn), &sb );
  if( err ) return 1;
  

  
  err =  memcmp(& d->mtim.tv_sec, & sb.st_mtim, sizeof  sb.st_mtim ) ;
  if( err ) {	  
	  TRACE(4,"check: %d %s", pos, CHARP(fn));
	  TRACE(4,"cache: %s", ctime( &d->mtim.tv_sec));
	  TRACE(4,"cur:   %s", ctime(&sb.st_mtim.tv_sec));
  }

  return err;
}

/* return true if all files in njson list jd are unchanged */
int filesunchanged_checksum(int cs, int jd )
{
  int p;
  struct njson_st *j;
  
  m_foreach( jd,p,j ) {
	  if(  ismodified_checksum(cs, j->d) ) {
		  TRACE(4,"changed: %s", CHARP(j->d));
		  return 0;
	  }	  
  }
  return 1;
}





int isempty_list(char *name, int jd)
{
	int lst = search_obj_data( name, jd  );
	return (lst==0) || (m_len(lst) == 0); 
}



int filenames_part_of( int nodes, int fnlst )
{
  int p;
  struct njson_st *j;	
  m_foreach( nodes,p,j) {	  
      int filename =  j->d;     
      if( m_bsearch( &filename, fnlst, cmp_mstr ) < 0 ) {
	      return 0;
      }
  }
  return 1; 
}

#define CHECKSUM_FN "TMP.checksum"

void export_bash_command(int g, int comp_ready)
{
  int p;
  int *d;
  d_obj(g, "global_state='(", ")'\n" );
  printf( "nr_jobs=%d\n",  m_len(comp_ready) );
  TRACE(4,"starting %d jobs", m_len(comp_ready) );
  m_foreach(comp_ready,p,d) {
    dump_string_array( "IN", *d );
    dump_string_array( "OUT", *d );    
    int lst = search_obj_data( "REC", *d  );
    TRACE(4,"%s", CHARP(lst));
    int id = search_obj_data( "id", *d  );
    printf( "%s\nstore-result %s $?\n\n", CHARP(lst), CHARP(id) );
  }
}






void prepare_nodes(int checksum, int global_state, int ec)
{
  int p;
  struct njson_st *j;
  int loop_count;
  int nodes = search_obj_data( "nodes", global_state );
  if( !nodes ) {
    ERR("no member named 'nodes'");
  }

  loop_count = fetch_node_int( global_state, "loop_count", 0 );
  set_node(global_state, "loop_count", loop_count+1 );
  /* RULE 1) update cache for IN+OUT if node was compiled with exit_code zero */ 
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
		  if( x>=0 ) {
			  TRACE(4,"set exit code %d on node %d", x, id );
			  set_node( j->d, "exit_code", x );
		  }
		  if( x == 0 ) {
			  update_checksum_node( checksum, search_obj_data("OUT", j->d ));
			  update_checksum_node( checksum, search_obj_data("IN", j->d ));
		  }      
	  }
  }
}


void dump_int(int a)
{
	int p,*d;
	m_foreach(a,p,d) {
		printf("%d, \n", *d );	
	}
	puts("");
}


int intersection_empty(int a, int b)
{
	void *obj1, *obj2;
	int p;
	int u;

	m_foreach(a,p,obj1) {
		m_foreach(b,u,obj2)
			if( memcmp(obj1,obj2,m_width(a)) == 0 ) {
				return 0;
			}
	}
	return 1;		
}

void find_nodes( int global_state, int ec )
{
	
	struct njson_st *j;
	int p,nodes = search_obj_data( "nodes", global_state );
	int *d;
	int fnid = m_create(100,sizeof(int));
	int cnt = m_len(nodes);
	int prep = m_create(cnt,sizeof(int));

	/* INIT
	 * --------------------------------
	 * create a list of all filenames to
	 * get an uuid, read checksums and
	 * update nodes
	 */ 
	TRACE(3, "UUID STRING FOR STRINGS" );
	int p1 = m_create(5,sizeof(int));
	m_foreach(nodes,p,j) {
		app_names(p1, fnid, "IN",  j->d);
		app_names(p1, fnid, "DEP", j->d);
		app_names(p1, fnid, "OUT", j->d);
	}
	m_free(p1);
	int checksum = read_checksum( CHECKSUM_FN );
	prepare_nodes(checksum,global_state,ec);

	/* START
	 * --------------------------------
	 * ndep : { node.out | node.ec!=0 }
	 * comp_ready: { node | node.in+node.dep not
	 * in(ndep)}
	 */
	/*OPTIMIZE: all filenames in IN+DEP */
	TRACE(3, "BUILD IN+DEP LIST" );
	m_foreach(nodes,p,j) {
		int p1 = m_create(5,sizeof(int));
		m_puti(prep,p1);
		app_names(p1, fnid, "IN",  j->d);
		app_names(p1, fnid, "DEP", j->d);
	}	
	/*OPTIMIZE: set exit code to zero on each node with checksum
	 * unchanged for IN+OUT+not Empty(IN) */
	m_foreach(nodes,p,j) {
		if( fetch_node_int(j->d,"exit_code",-1) == 0 ) continue;
		int lst_in = search_obj_data( "IN", j->d);
		int lst_out = search_obj_data( "OUT", j->d);
		if( lst_in && m_len(lst_in)
		    && filesunchanged_checksum(checksum, lst_in)
		    && filesunchanged_checksum(checksum, lst_out ) )
		{
			TRACE(4,"node %d cached", p);
			set_node( j->d, "exit_code", 0 );  
		}		    
	}
	/* ndep: all OUT-files with exit_code != 0 */
	int ndep = m_create(100,sizeof(int));
	m_foreach(nodes,p,j) {
		int err = fetch_node_int(j->d, "exit_code", -1 );
		if( ! err ) continue;
		app_names(ndep, fnid, "OUT",  j->d);
	}
	/* RULE1) comp_ready:= for all nodes with:
	   1) prep list contains nothing from ndep and
	   2) exit_code != 0
	*/
	int comp_ready = m_create(100,sizeof(int));
	m_foreach(nodes,p,j) {
		if( fetch_node_int(j->d, "exit_code", -1 ) == 0 ) continue;
		int p1 = INT(prep,p);
		if( intersection_empty( p1, ndep ) ) {
			m_puti(comp_ready,j->d);
			TRACE(4,"Ready: %d", p );
		}
	}

	export_bash_command(global_state,comp_ready);
	save_checksum( checksum, CHECKSUM_FN );
	m_free(ndep);
	m_free(comp_ready);
	m_foreach( prep, p, d ) m_free(*d);
	m_free(prep);

	m_foreach( fnid, p, d ) m_free(*d);
	m_free(fnid);	

	m_free(checksum);
}




void list_nodes( int global_state, int ec )
{
	int *d, id, p;
	struct njson_st *j;
	int checksum = read_checksum( CHECKSUM_FN );
	prepare_nodes(checksum,global_state,ec);
	int nodes = search_obj_data( "nodes", global_state );
	

  /* RULE 3) if all IN+DEP+OUT is in cache and is unchanged set exit code to zero
     special case: if there are no files to check - ignore the node 
   */
  // dump_checksum(checksum);
  TRACE(4,"Check if some nodes do not need to be executed");
  m_foreach(nodes,p,j) {
	  int id = fetch_node_int(j->d, "id", -1 );	  
	  if( id < 0 ) continue;
	  int err = fetch_node_int(j->d, "exit_code", -1 );
	  if( err == 0 ) continue;
	  if( isempty_list("IN",j->d) && isempty_list( "OUT",j->d ) && isempty_list("DEP",j->d) )
		  continue;
      
	  if( filesunchanged_checksum( checksum,  search_obj_data( "IN",j->d  ) ) &&
	      filesunchanged_checksum( checksum,  search_obj_data( "OUT",j->d  ) ) &&
	      filesunchanged_checksum( checksum,  search_obj_data( "DEP",j->d  ) ) ) {
		  TRACE(4,"cached result; set exit_code=0 for node %d", id );
		  set_node( j->d, "exit_code", 0 );
	  }
  }

  
  
  int all_gen = m_create( 100, sizeof(int) );
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
    else append_list( all_gen, "OUT", j->d );
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

  TRACE(4,"\n-----------------------------\nfind compile ready nodes");  
   /* find nodes where the 'IN' list does not contain members of 'all_notgen'
      and 'exit_code' is zero
   */
  int comp_ready =  m_create( 100, sizeof(int) );
  /* RULE 2) if exit_code in node is zero, skip this node  */
  


  



  m_foreach(nodes,p,j) {    
    struct njson_st *j0;

    id = fetch_node_int(j->d, "id", 0 );
    TRACE(4, "checking node %d", id );

    /* if exit_code is zero or IN/DEP has not change then skip */
    int i,lst = search_obj_data( "exit_code",j->d  );
    if( *(CHARP(lst)) == '0' ) {
      TRACE(4, "skip b/c exit_code zero: %d", id ); 
      continue;
    }

    lst = search_obj_data( "IN",j->d  );
    /* not all files IN are in the list of allready generated files */
    if(! filenames_part_of( lst, all_gen ) ) {	    
    m_foreach( lst,i,j0) {

      int filename =  j0->d;
      
      if( m_bsearch( &filename, all_notgen, cmp_mstr ) >= 0 ) {
	TRACE(4, "%s found in all_notgen", CHARP(filename));
	goto skip_node;
      }

      if( !file_exists(filename) ) {
	TRACE(4, "file %s not found", CHARP(filename));
	goto skip_node;
      }
    }
    }
    
    TRACE(4, "Checking DEP" );
    lst = search_obj_data( "DEP",j->d  );
    if(! filenames_part_of( lst, all_gen ) ) {	    

    m_foreach( lst,i,j0) {

      int filename =  j0->d;
      
      if( m_bsearch( &filename, all_notgen, cmp_mstr ) >= 0 ) {
	TRACE(4, "%s found in all_notgen", CHARP(filename));
	goto skip_node;
      }

      if( !file_exists(filename) ) {
	TRACE(4, "file %s not found", CHARP(filename));
	goto skip_node;
      }
    }
    }
    
    TRACE(4,"adding node %d", id );
    m_put( comp_ready, & j->d );
  skip_node:
  }

  /* if exit_code!=0 and all in IN+DEP are in all_gen then add this node */ 


  
  /* rule: if there is nothing to do, retry nodes with exit_code!=0 */
  if( m_len(comp_ready) == 0 ) {
	  m_foreach(nodes,p,j) {
		  int err = fetch_node_int(j->d, "exit_code", -1 );
		  if( err == 0 ) continue;
		  TRACE(4,"adding node b/c exit_code!=0" );
		  m_put( comp_ready, & j->d );
	  }
  }
  
  /* dump comp ready nodes */
  //m_foreach(comp_ready,p,d) {
  //  d_obj2(stderr, *d,"(",")\n" );
  //}
  
  /* compile with rec and save the exit code, keep trying until exit code is zero or max_try_count exceeded */


  /* OPTIMIZE RULE: if files in out exists and files in in+dep+out have not changed, do not compile */


  export_bash_command(global_state,comp_ready);
  
  save_checksum( checksum, CHECKSUM_FN );
  
  /* if exit code is zero remove all files in out-list from all_out and rebuild the comp_ready list */
  m_free(all_dep);
  m_free(all_out);
  m_free(all_in);
  m_free(in_wo_out);
  m_free(comp_ready);
  m_free(all_notgen);
  m_free(all_gen);
  m_free(checksum);

  
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

int test_bsearch(void)
{
	int cs = read_checksum(CHECKSUM_FN);
	int fn = s_printf( 0,0, "%s", "njson_lex.lex.c" );
	dump_checksum(cs);


	int pos = m_bsearch( &fn, cs, cscmpmstr );
	TRACE(4,"%d", pos );
	m_free(fn);
	
	exit(1);
	
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


  // test_sort_lookup();
  find_nodes(opts,ec);
  // list_nodes(opts,ec);

  
  m_free(ec);
  njson_free(opts);
  m_destruct();
  return EXIT_SUCCESS;
}
