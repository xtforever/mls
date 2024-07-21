/*
  types:
  cstr   - pointer to memory with c-type string (char*)
  ms     - Handle for char-array, zero terminated, length include zero
  msc    - Handle for char-array, zero terminated, length include zero
           This handle is unique for each String (like XrmQuark)
	   This string must not be freed.
	   If you convert mch or cstr to mcc you can free the cstr or mch.
	   all mcc are internaly memory managed

  ml     - list of Handles or Integers
  mll    - list of list of integers
  m      - list of unspecified
  
*/



#include "njson_read.h"
#include "mls.h"
#include "conststr.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <glob.h>
#include <stdarg.h>

void d_obj2(FILE *fp, int opts, char *c1, char *c2);
void d_obj(int opts, char *c1, char *c2);

typedef struct cs_st {
  char fn[256];
  struct timespec mtim;
} cs_t;


static inline int s_new(int len)
{
	return m_create(len,1);
}

/*
     
  int s_cstr(const char*s)   - s wird kopiert, falls noch nicht vorhanden 
  int s_mstr(int m)          - m wird kopiert, falls noch nicht vorhanden
  int s_new(int length)      - create/alloc a string 
  
  s_regex( m, pattern )      - regex search, returns list if matched subexpr
                             - pattern=s_cstr( "^(.*).c$" )
			     - wl = s_regex( m, pattern )
			     - m_len(wl) > 1?  new_str = INT(wl,1); INT(wl,1)=0; s_app(new_str,s_cstr( ".h" ));
                             - pattern=s_cstr( "^.*\.(.*)$" )
			     - wl = s_regex( m, pattern )
			     - m_len(wl) > 1?  ext = s_mstr( INT(wl,1) );
			     - m_free_list( wl );
			     
  int s_strchr(int m, int c)  - -1 or pos
  int s_strrchr(int m, int c) - -1 or pos

  int s_split(int w, int m, int delim)
                                - If m is empty the s_split() function returns 0 and does nothing else.
				  Otherwise, this function finds all tokens in the string m, that are delimited by one of the
				  bytes in the string delim. This tokens are put into the string list w.
				  if w is 0 a new string list will be created
				  returns: w
				  

  

  ( loop_count:0,
    importmm:"true"
    nodes:(...),
  )
 
 */

/*
  strings, ...
  index,...
  access

  gen: wbuild out out out: in in in in

  gen: rec out-list, dep-list: in-list  
*/



static void m_free_list(int m)
{
	int p,*d;
	m_foreach( m, p, d ) m_free(*d);
	m_free(m);
}


/* cmp_

    i   - int
    cs  - cs_t
    ms  - mstring
    
 */


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

int cmp_int( const void *a0, const void *b0 )
{
	const int *a = a0;
	const int *b = b0;
	return (*a) - (*b);
}

int cmp_mstr( const void *a0, const void *b0 )
{
	const int a = *(int *)a0;
	const int b =  *(int *)b0;
	return m_cmp( a,b );
}






void m_put_cstr(int m, const char *s)
{
	if( s==0 || *s==0 ) {
		int w=m_create(1,1);
		m_putc(w,0);
		m_put(m,&w);
		return;
	}
	
	int len=strlen(s)+1;
	int w=m_create(len,1);
	m_write(w,0,s,len);
	m_put(m,&w);
}


int glob_files( const char *s)
{
	int fl = m_create(50,sizeof(int));
	glob_t glob_result;
	int return_value;

	// Perform globbing
	return_value = glob(s, 0, NULL, &glob_result);
	if (return_value != 0) {
		if (return_value == GLOB_NOMATCH) {
			TRACE(1, "No matches found." );
		} else {
			WARN("Error in globbing." );
		}
		return fl;
	}

	// Iterate through the matched files
	for (size_t i = 0; i < glob_result.gl_pathc; i++) {
		m_put_cstr(fl,glob_result.gl_pathv[i]);
	}

	// Free the memory allocated for the glob structure
	globfree(&glob_result);
	return fl;
}

int m_read_file(int m, int max, int fn )
{
	int buf;
	if( m > 0 ) {
		buf=m;
		m_clear(buf);
	} else {
		buf =  m_create( max, 1); 
	}
	TRACE(3,"read %s",  CHARP(fn) );
	FILE *fp = fopen( CHARP(fn), "r" );
	if(!fp) return buf;
	int len = fread( m_buf(buf), 1, max, fp );
	if( len <= 0 ) {
		fclose(fp);
		return buf;
	}
	m_setlen(buf,len);
	fclose(fp);
	return buf;		
}


void nj_add_string( int node, int name_cs, int data )
{
	struct njson_st *j;
	j = m_add(node);
	j->name = name_cs;
	j->typ = NJSON_STRING;
	j->d =  conststr_lookup( data );
}

int nj_add_obj( int node, char name_cs, int len )
{
	struct njson_st *j;
	j = m_add(node);
	j->name = name_cs;
	j->typ = NJSON_OBJ;
	if( len == 0 ) len=1;
	j->d = m_create(len,sizeof(struct njson_st));
	return j->d;
}



/* copy characters from buf[p..] to out until one of *delim is found
   returns: last parsed character
   out will be a zero terminated string
   parsed word will be appended to out
*/
int p_word(int out, int buf, int *p, char *delim )
{
	int ch=0;
	while( *p < m_len(buf) && isspace(ch=CHAR(buf,*p)) ) { (*p)++; }
	while( *p < m_len(buf) && !strchr( delim, ch=CHAR(buf,*p) ) && ch ) { m_putc(out,ch); (*p)++; }
	m_putc(out,0);
	return ch;
}

/* right compare mstr */
int mcmpr(int m, const char *s)
{
	if( !s || ! *s ) return 0;
	int mlen = m_len(m) -1;
	int slen = strlen(s);
	if( slen > mlen ) return 1;
	for( int p = mlen - slen; p<=mlen; p++, s++ ) {
		if( *s != CHAR(m,p) ) return 1;
	}
	return 0;
}



int memberof_list(int nodes, char *s, int m );
int search_obj_data( const char *s, int opts );
void add_member_to_list(int node, char *s, int m );

void add_deps(int nodes, int out, int deps )
{
	int p,x,*d, outp,*outd;
	struct njson_st *j,*j1;
	int found=0;

	if( m_len(deps) == 0 || m_len(out)==0 ) return;
	
	m_foreach( nodes,p,j ) {
		m_foreach( out, outp, outd ) {
			if( memberof_list( j->d, "OUT", *outd ) ) {
				found=1;
				m_foreach( deps,x,d ) {
					add_member_to_list( j->d, "DEP", *d );
				}
			}
		}
	}

	if( found ) return;

	/* if .xxx ending generate xxxcompile rule */
	int d0 = INT(deps,0); int out0 = INT(out,0);
	TRACE(4,"ADD %s %s", CHARP(d0), CHARP(out0) );

	char*cp;
	int ext=0;
	m_foreach(d0,p,cp) {
		if( *cp == '.' ) {
			ext=p+1;
		}
	}
	if( !ext ) return;
	
	j1 = m_add(nodes);
	j1->name = 0;
	j1->typ = NJSON_OBJ;
	j1->d = m_create(3,sizeof(struct njson_st));

	j = m_add(j1->d);
	j->name = conststr_lookup_c( "REC" );
	j->typ = NJSON_STRING;
	j->d = cs_printf(0,0, "run %scompile", (char*) mls(d0,ext) ); 

	j = m_add(j1->d);
	j->name = conststr_lookup_c( "OUT" );
	j->typ = NJSON_OBJ;
	j->d = m_create(m_len(out),sizeof(struct njson_st));

	for(int i=0;i<m_len(out);i++) {
		struct njson_st *j0 = m_add(j->d);
		j0->name = 0;
		j0->typ = NJSON_STRING;
		j0->d =   INT(out,i);
	}
	
	j = m_add(j1->d);
	j->name = conststr_lookup_c( "IN" );
	j->typ = NJSON_OBJ;
	j->d = m_create(1,sizeof(struct njson_st));
	
	j = m_add(j->d);
	j->name = 0;
	j->typ = NJSON_STRING;
	j->d = INT(deps,0);
		
	m_foreach( deps,x,d ) {
		add_member_to_list( j1->d, "DEP", *d );
	}
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

int m_strrchr(int s, int c)
{
	int p = m_len(s);
	while( p-- ) 
		if( CHAR(s,p) == c ) break;
	return p;
}

int m_cmp_part( int w1, int p1, int w2, int p2 )
{
	int ch=0, i=0;
	while( !ch && i+p1 < m_len(w1) && i+p2 < m_len(w2) ) {
		ch = CHAR(w1,p1+i) - CHAR(w2,p2+i);
		i++;
	}
	return ch;		
}

void gen_rec( int nodes, int name, int in_fn, int out_fn )
{
	TRACE(5,"%s %s %s",
	      CHARP(name),CHARP(in_fn),CHARP(out_fn) );	
	int obj;
	
	nodes = nj_add_obj( nodes, 0, 3 );
	nj_add_string( nodes, s_cstr("REC"),  name );
	obj = nj_add_obj(nodes, s_cstr("IN"), 1 );
	nj_add_string( obj, 0, in_fn );
	obj = nj_add_obj(nodes, s_cstr("OUT"), 1 );
	nj_add_string( obj, 0, out_fn );	
}


void syntax_error( const char *format, ... )
{
	va_list argptr;
	printf("SYNTAX ERROR\n");
	va_start(argptr, format);
	vprintf(format, argptr);
	va_end(argptr);
	printf("\n\n");
}



/* gen-each:
 * recept:pat-in pat-out file-list

 file end with .c -> add to infile
         remove extension append .o
	 add to outfile

 file end with .o -> add to outfile 
          remove extension append .c
	  add to infile
	 
 for a in 0,1
 
 do            
     w[a] = copy f
     f remove ext[a]
     f add ext[1-a]
     w[1-a]=f
     goto ready
     a++
while a < 2
w[0] = copy f, add extension ext[0]
w[1] =      f, add extension ext[1]


 
 a=1
 f match ext[a]:
     w[a] = f
 
     
	  
 */
void gen_nodes(int nodes, int cmd )
{
	TRACE(5,"start");
	/* cc:.c .o t1.c t2.c */ 
	int out = m_create( 1, sizeof(int) );
	int p=0,ext,w;
	int buf = m_create( 20, 1 );	
	if( p_word(buf, cmd, &p, ":" ) != ':' ) {
	  m_free(out);
	  m_free(buf);
	  syntax_error( "can not parse gen-each rule '%s'", CHARP(cmd) );
	}
	m_puti(out,buf);
	p++;
	get_word_list(out,cmd,&p);


	for( p=3;p<m_len(out);p++ ) {
		int in_fn=0;
		int out_fn = 0;
		int b1 = 0;
		int b2 = 0;
		int t;
		w=INT(out,p);
		ext = m_strrchr(w,'.');
		if( ext ) {
			/* hat die endung fuer in */ 
			if( m_cmp_part( w,ext, INT(out,1), 0 ) == 0 ) {
				in_fn = w;


				b2 = out_fn = m_dub(w);
				t = INT(out,2);
				m_write( b2,ext, CHARP(t), m_len(t) );
				goto produce;				
			}
			else if( m_cmp_part( w,ext, INT(out,2), 0 ) == 0 ) {
				out_fn = w;
				b1 = in_fn = m_dub(w);
				t = INT(out,1);
				m_write( b1,ext, CHARP(t), m_len(t) );
				goto produce;
			}
		}
		b1 = in_fn = m_dub(w);
		t = INT(out,1);
		m_write( b1, m_len(b1)-1, CHARP(t), m_len(t) );

		b2 = out_fn = m_dub(w);
		t = INT(out,2);
		m_write( b2, m_len(b2)-1, CHARP(t), m_len(t) );
	
	produce:
		gen_rec( nodes, INT(out,0), in_fn, out_fn );
		m_free(b1);
		m_free(b2);
	}
	m_free(out);
	m_free(buf);
}

inline static void gen_string_list(int nodes, int name, int list)
{
	int len = list ? m_len(list) : 1;
	int p,*d,obj = nj_add_obj(nodes, name, len );
	m_foreach( list, p, d ) nj_add_string( obj, 0, *d );
}

void gen_nodex( int nodes, int name, int in_list, int out_list, int dep_list )
{
	nodes = nj_add_obj( nodes, 0, 3 );     
	nj_add_string( nodes, s_cstr("REC"),  name );
	gen_string_list( nodes, s_cstr("IN"),  in_list );
	gen_string_list( nodes, s_cstr("OUT"), out_list );
	gen_string_list( nodes, s_cstr("DEP"), dep_list );
}

/* parse:
   recname target: file1 file2 file3

   output a node:
   ( rec: recname
     out: target
     in: file1 file2 file3
   )


*/

void apply_rule(int nodes, int rule)
{
	if( s_empty(rule) ) return;
	TRACE(5,"rule: %s", CHARP(rule));
	int in=0;
	int out = m_create( 1, sizeof(int) );
	int p=0;
	if( ! get_word_list(out, rule, &p) ) goto cleanup;
	if( CHAR(rule,p) != ':' ) goto cleanup;
	
	in = m_create( 1, sizeof(int) );
	p++;
	if( ! get_word_list(in, rule, &p) ) goto cleanup;

	int name = INT( out, 0 );
	m_del(out,0);
	gen_nodex( nodes, name, in, out, 0 );
	
	TRACE(5,"ready");
	
cleanup:
	m_free(out);
	m_free(in);
}


static inline int int_get(int m, int p)
{
	if(!m || m_len(m) <= p ) return 0;
	return INT(m,p);
}



/* rec:out:in:dep */
void gen_node_from_list(int nodes, int rule)
{
	if( s_empty(rule) ) return;
	int p=0,m[4] = {0};

	m[0] = s_new(20);
	if( p_word( m[0], rule, &p, ":" ) != ':'  || s_empty(m[0]) ) {
		syntax_error( "missing colon or empty name: '%s'", CHARP(rule) );
		goto cleanup;
	}
	TRACE(2,"rec: %s", CHARP(m[0]));
	for(int i=1; i < ALEN(m) && p < m_len(rule) && ( CHAR(rule,p++) == ':' ); i++) {
		TRACE(2,"scan %s", (char*)mls(rule,p)); 
		m[i] = m_create(1,sizeof(int));
		get_word_list(m[i], rule, &p);
	} 
	TRACE(2,"scan stopped at %d", p );
	gen_nodex( nodes, m[0], m[2], m[1], m[3] );

cleanup:
	for(int i=0;i< ALEN(m); i++) {
		m_free(m[i]);
	}
}




void get_depfile(int nodes, int fn )
{
	int buf = m_read_file(0,16384,fn);
	int p=0;
	int out, in;
	out = m_create( 1, sizeof(int) );
	in = m_create(5,sizeof(int)) ;

	get_word_list(out,buf,&p);
	p++;
	get_word_list(in,buf,&p);

	int *d;
	m_foreach( in,p,d) {
		TRACE(4, "IN: %s", CHARP(*d));
	}
	m_foreach( out,p,d) {
		TRACE(4, "OUT: %s", CHARP(*d));
	}
	
	add_deps( nodes, out, in );

	m_free(out);
	m_free(in);
	m_free(buf);
}



void importmm(int nodes, int s_pattern )
{
	TRACE(2,"pattern %s", CHARP(s_pattern));
	int files = glob_files( CHARP(s_pattern) );

	int p,*fn;
	m_foreach(files,p,fn) {
		get_depfile( nodes, *fn );
	}
	
	m_free_list(files);
}


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

int memberof_list(int nodes, char *s, int m )
{
  struct njson_st *j ;
  int p;

  /* seek for member s in this node obj data */
  int opts = search_obj_data( s,nodes );
  if( opts <= 0 ) {
    TRACE(2,"not found");
    return 0;
  }
  
  m_foreach(opts,p,j) {
    if( j->d ) {
	    if( j->d == m ) return 1;
    }
  }
  return 0;
}


void add_member_to_list(int node, char *s, int m )
{
  struct njson_st *j ;
  int p;

  /* seek for member s in this node obj data */
  int opts = search_obj_data( s,node );
  if( opts <= 0 ) {
	  /* keine Node mit Namen <s> vorhanden, dann erstellen */
	  j = m_add(node);
	  j->name = conststr_lookup_c(s);
	  j->typ = NJSON_OBJ;
	  j->d = m_create(2,sizeof(struct njson_st));	  
	  opts = j->d;
  }

  /* keine vorhandenen einsetzen */
  m_foreach(opts,p,j) {
    if( j->d ) {
	    if( j->d == m ) return;
    }
  }

  j = m_add(opts);
  j->name = 0;
  j->typ = NJSON_STRING;
  j->d = m;
}



void verify_sort_lookup(int m)
{
	
}


/* einfuegen von key in das array m falls key noch nicht
   existiert
   return: pos von key
*/
int sort_lookup(int m, int key)
{
	int p = m_binsert( m, &key, cmp_int, 0 );
	if( p < 0 ) {
		return (-p)-1;
	}
	TRACE(1,"ADD pos:%d key:%d", p, key );
	return p;
}


/* add element to array of unique int */
void ulist_int( int m, int key )
{
	sort_lookup(m,key);
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



void test_sort_lookup(void)
{
	int p1,p2;
	int id = m_create(100,sizeof(int));
	int k = m_create(100,sizeof(int));
	
	char *s[] = { "prepare","test10.b", "test1.a", "test1.b" };
	p1=0;
	for(int i=0;i<4;i++) {
		p1 = s_printf(0,0, "%s", s[i] );
		m_puti(k,p1);
	}

	for(int i=0;i<4;i++) {
		p1 = INT(k,i);
		sort_lookup( id, p1 );
	}

	TRACE(1,"DOUBLE %d", m_len(id) );
     
	for(int i=0;i<4;i++) {
		p1 = INT(k,i);
		sort_lookup( id, p1 );
	}


	dump_mstrarray( id);
	
	
	m_free_list(k);
	(void) p2;
	exit(1);
	
	
#if 0
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
#endif
	exit(1);
	
}


/* search for entry 's' in tree opts0 and append all string member
   to the list conststr-list m
*/
void app_names( int m, char *s, int opts0 )
{
  struct njson_st *j ;
  int p;
  int opts = search_obj_data( s,opts0 );
  if( opts <= 0 ) {
    TRACE(2,"not found");
    return;
  }
  
  m_foreach(opts,p,j) {
	  if( j->d == 0 || j->typ != NJSON_STRING ) continue;	  
	  int id = conststr_lookup( j->d );
	  sort_lookup( m, id );
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



static int create_node_str(  int obj, const char *key, int val )
{
  struct njson_st nj;
  
  nj.typ = NJSON_STRING;
  nj.name = is_empty(key) ? 0 : conststr_lookup_c( key );
  char buf[20];
  snprintf(buf,sizeof buf,"%d", val );
  nj.d = conststr_lookup_c (buf );
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
int set_node( int opts, const char *key, int val )
{
  char buf[20];
  snprintf(buf,sizeof buf,"%d", val );	
  struct njson_st *j;
  int p;
  m_foreach(opts,p,j) {
    if( njson_cmp( key, j ) == 0 ) {
	    j->d = conststr_lookup_c(buf);
	    return j->d;
    }
  }
  return create_node_str(opts,key, val );
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
    inc*=2; /* OPTIMIZE */
    m_setlen( cs,len+inc ); 
  }
  len+=rd; 
  m_setlen( cs,len );
  return cs;
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

/* files:  list of mstr */
void update_checksum_list(int checksum, int files)
{
	int p,*d;
	m_foreach( files, p, d) {
		update_checksum_file(checksum, *d);	
	}	
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


/* return true if at least one file in string list has changed */
int has_changed(int cs, int m )
{
	int p,*d;
  
	m_foreach( m,p,d ) {
		TRACE(4,"verify: %s", CHARP(*d));
		if(  ismodified_checksum(cs, *d) ) {
			TRACE(4,"changed: %s", CHARP(*d));
			return 1;
		}	  
	}
	return 0;
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








void prepare_nodes(int checksum, int global_state, int ec, int ret_upd_cs )
{
  int p;
  struct njson_st *j;
  int loop_count;
  int nodes = search_obj_data( "nodes", global_state );
  if( !nodes ) {
    ERR("no member named 'nodes'");
  }

  TRACE(1, "add loop count %d", global_state );
  loop_count = fetch_node_int( global_state, "loop_count", 0 );
  set_node(global_state, "loop_count", loop_count+1 );


  int k_gen = conststr_lookup_c("gen-each");
  int k_rule = conststr_lookup_c("gen-rule");
  int k_node = conststr_lookup_c("gen-node");	  
  int k_import = conststr_lookup_c("import");
  int k_zero = conststr_lookup_c("");
  
  m_foreach(global_state,p,j) {
	  if( s_empty(j->d) ) continue;	  
	  if( j->name == k_node ) {
		  gen_node_from_list(nodes,j->d);
	  } 
	  else if( j->name == k_rule ) {
		  apply_rule(nodes,j->d);
	  }
	  else if( j->name == k_gen ) {
		  gen_nodes(nodes,j->d);
	  }
	  else if( j->name == k_import ) {
		  importmm(nodes,  j->d );  		  
	  }
	  else continue;
	  j->d = k_zero;
   }
		   
  TRACE(2,"update nodes and cache");
	
  /* RULE 1) update cache for IN+OUT if node was compiled with exit_code zero */ 
  /* verify valid nodes and  create unique id entries */
  m_foreach(nodes,p,j) {
	  if( j->typ < NJSON_ARR ) {
		  TRACE(2, "Node %d invalid", p );
		  return;
	  }
	  if( loop_count == 0 ) {
		  set_node( j->d, "exit_code", 1 );
		  set_node( j->d, "id", p+1 );
		  continue;
	  } 

	  int id = fetch_node_int(j->d, "id", -1 );
	  
	  /* check if exit_code for this node is set
	     update cache if exit_code is zero 
	  */
	  int x = get_exit_code(id,ec);
	  if( x>=0 ) {
		  TRACE(4,"set exit code %d on node %d", x, id );
		  set_node( j->d, "exit_code", x );
	  }
	  if( x == 0 ) {
		  app_names( ret_upd_cs, "OUT", j->d );
		  app_names( ret_upd_cs, "IN", j->d );			  
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


void dump_fnid(const char *head, int a, int fnid)
{
	fprintf(stderr,"%s", head );
	int p,*d;
	m_foreach(a,p,d) {
		fprintf(stderr,"%s, ", CHARP( INT(fnid,*d)) );	
	}
	fprintf(stderr,"\n");
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



int node_is_cached(int checksum, int d )
{
	int lst_in = search_obj_data( "IN", d);
	int lst_out = search_obj_data( "OUT", d);
	return ( lst_in && m_len(lst_in) 
	    && filesunchanged_checksum(checksum, lst_in)
		 && filesunchanged_checksum(checksum, lst_out ) );	
}

void find_nodes( int global_state, int ec, int checksum, int ret_cs )
{
	
	struct njson_st *j;
	int p,nodes = search_obj_data( "nodes", global_state );
	int *d;
	int cnt = m_len(nodes) + 1;
	int prep = m_create(cnt,sizeof(int));

	/* import nodes, find cached nodes */
	TRACE(2,"prepare nodes");
	prepare_nodes(checksum,global_state,ec, ret_cs );

 	
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
		app_names(p1, "IN",  j->d);
		app_names(p1, "DEP", j->d);
	}
	
	/* OPTIMIZE: set exit code to zero on each node with checksum
	 * unchanged for IN+OUT+not Empty(IN)
	 */
	/* m_foreach(nodes,p,j) { */
	/* 	if( fetch_node_int(j->d,"exit_code",-1) == 0 ) continue; */
		
	/* 	int lst_in = search_obj_data( "IN", j->d); */
	/* 	int lst_out = search_obj_data( "OUT", j->d); */
	/* 	if( lst_in && m_len(lst_in) */
	/* 	    && filesunchanged_checksum(checksum, lst_in) */
	/* 	    && filesunchanged_checksum(checksum, lst_out ) ) */
	/* 	{ */
	/* 		TRACE(4,"node %d cached", p); */
	/* 		set_node( j->d, "exit_code", 0 ); */
	/* 	} */
	/* } */

	int in_cache = m_create(10,sizeof(int));

       	/* ndep: all OUT-files with exit_code != 0
	   i.e. known but not yet generated files.
	   if in-files and out-files are in the cache and are unchanged, assume out-files are generated
	 */
	int ndep = m_create(100,sizeof(int));
	m_foreach(nodes,p,j) {
		int err = fetch_node_int(j->d, "exit_code", -1 );
		if( err ==0 ) continue;
		if( node_is_cached(checksum,j->d) ) {
			sort_lookup( in_cache, p );
			TRACE(4,"Node is cached: %d", p );
			continue;
		}
		
		app_names(ndep, "OUT",  j->d);
	}



         /* RULE1) comp_ready:= for all nodes with:
	   1) prep list (in+dep) contains nothing from ndep and
	   2) exit_code != 0
	   3) not to compile if (exit_code == -1 and node_cached())
	*/
	int comp_ready = m_create(100,sizeof(int));

	m_foreach(nodes,p,j) {
		if( fetch_node_int(j->d, "exit_code", -1 ) == 0 ) continue;
		if( m_bsearch( &p, in_cache, cmp_int ) >= 0 ) {
			TRACE(4,"Node is cached: %d", p );
			continue;
		}
		int p1 = INT(prep,p);		
		if(  intersection_empty( p1, ndep ) ) {
			m_puti(comp_ready,j->d);
			TRACE(4,"todo node nr:%d", p );
		}
	}

      
	
	
	export_bash_command(global_state,comp_ready);
	
	m_free(ndep);
	m_free(comp_ready);
	m_foreach( prep, p, d ) m_free(*d);
	m_free(prep);

	m_free(in_cache);
}



#if 0
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
#endif


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


int apply_named_rule( int nodes, int n, int buf )
{
	TRACE(4,"%s %s", CHARP(n), CHARP(buf) );
	int k_gen_each = conststr_lookup_c("gen-each");
	int k_rule = conststr_lookup_c("gen-rule");
	int k_gen_node = conststr_lookup_c("gen-node");	  
	int k_import = conststr_lookup_c("import");
	  
	if( n == k_gen_node ) {
		gen_node_from_list(nodes,buf);
	} 
	else if( n == k_rule ) {
		  apply_rule(nodes,buf);
	}
	else if( n == k_gen_each ) {
		  gen_nodes(nodes,buf);
	}
	else if( n == k_import ) {
		importmm(nodes,  buf );  		  
	}
	else {
		syntax_error("Unknown rule '%s'",  CHARP(n) );
		return 1;
	}
	
	return 0;
}

	
int m_copy_new(int n, int p, int len)
{
	if( len < 1 ) {
		len = m_len(n) - p;
		if( len < 1 ) len=1;
		
	}
	
	int ret = m_create(len, m_width(n));
	
	if( p >= m_len(n) || p+len > m_len(n) ) {
		WARN("wrong args len=%d, start=%d, source_len=%d", len,p,m_len(n));
		return ret;
	}

	m_setlen( ret, len );
	void *target = m_buf(ret);
	void *src = mls(n,p);
	unsigned n_bytes = len * m_width(n);
	memcpy( target, src, n_bytes );
	return ret;
}

void apply_prepended_rules(int opts, int rules)
{
	int p,*d, sp;
	int r_name, r_text;
	int nodes = search_obj_data( "nodes", opts );
	int w = m_create(20,1);

	m_foreach( rules, p, d ) {
		if( s_empty(*d) ) continue;
		TRACE(4,"parse: %s", CHARP(*d));
		sp=0; m_clear(w);
		if( p_word(w,*d,&sp, ":") != ':' ) {
			syntax_error( "unknown prepended rule '%s'", CHARP(*d));
			continue;
		}
		r_name = s_mstr(w);
		sp++; skip_ws(*d,&sp);
		r_text = m_copy_new(*d,sp,-1);		
		apply_named_rule(nodes, r_name, r_text );
		m_free(r_text);
	}
	m_free(w);
}



/* 1) read exit codes and global state
   2) find nodes, print nodes to stdout
   3) create shell script to run

   -b : build makefile
   
*/



int main(int argc, char **argv)
{
  trace_level=4;
  m_init();
  conststr_init();

  TRACE(1,"start");


  int add_rules = m_create(10,sizeof(int));
  int rule = m_create(100,1);
  int ec = m_create(10,sizeof(int));  
  int new_cs = m_create(10,sizeof(int));
  
  int ch;
  if( (ch=getchar()) == '|' ) {
	  while( (ch=getchar()) > 0 ) {
		  if( ch != '\n' && ch != '|' ) {
			  m_putc(rule,ch);
			  continue;
		  }
		  m_putc(rule,0);
		  m_puti(add_rules,rule);
		  if( ch == '|' ) { rule=0; break; }
		  rule = m_create(100,1);
	  }
  } else ungetc(ch,stdin);  
  int x; while( scanf("%u", &x)==1 ) { TRACE(1, "%d", x ); m_puti(ec,x); }

  int opts = njson_from_file(stdin);
  int nodes = search_obj_data( "nodes", opts );
  if( !nodes ) {
	  nj_add_obj(opts, s_cstr("nodes"), 1 );
  }
  
  
  apply_prepended_rules( opts, add_rules );	  
  m_free_list(add_rules);
  m_free(rule);

  int checksum = read_checksum( CHECKSUM_FN );
  find_nodes(opts,ec, checksum, new_cs);
  update_checksum_list(checksum, new_cs);
  save_checksum( checksum, CHECKSUM_FN );
  m_free(checksum);
  m_free(new_cs);
  
  m_free(ec);
  njson_free(opts);
  conststr_free();
  m_destruct();
  return EXIT_SUCCESS;
}
