/*



  issues:
  - section rules must preceed section files to create nodes correctly




  functions:
  rule:bison:.y .tab.c .tab.h
  rule:flex:.l .c .h

  gen-node:rec:in:out:dep
  - falls rec bekannt kann out weggelassen werden und wird intern berechnet
  - dep ist ebenfalls optional

  gen-each:rec:in:dep
  - create node for each file 'in' 
    out is created if a rule for 'rec' is defined
    dep is used for each created node
    
  rc:dep:func
  runtime create: execute 'func' falls 'dep' created
  e.g: rc:stage1:gen-each:ccompile:*.c
  - da erst nach stage1 alle .c files erstellt wurden darf diese regel nicht zu frueh
    ausgefuehrt werden
    
  

  




  
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

  li  - list of int
  lli - list of list if int

  struct stat s
  
  l_stat list of struct stat
  l_long list of long
  l_int  list of int

  list of handles to struct stat
  l_hstat

  handle to const element:
     typedef uint64_t mconst_t;
     mconst_t m;

     void (*mlsc) (uint64_t m) {
        int m = m & ((1 << 32) -1 );
	int p = m >> 32;
	return mls(m,p);
     }
  
  string-list:
    list of msc === list of ( list of ( char ) )   llc
    msc destructor:   m_free                       
    list desctructor: m_free
    

  
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
#include <stdint.h>
typedef uint32_t u32;



/* mls extensions */
int m_slice(int dest, int offs, int m, int a, int b );
int m_app( int l1, int l2 );
void m_nputs(int str, int len);





/* rule system */
static int RULES = 0;
typedef struct {
	int name_cs;
	int suffix_list;
} rule_t;
void rule_init(void);
void rule_free(void);
void rule_store( int rule_name, int suffix_list );
int rule_fetch( int rule_name );
int rule_expand_files( int rule, int inf, int OUT );
int rule_compile( int rule_str, int *p, int rule );
int rule_search_ext(int pat);
rule_t* rule_get( int p );
int rule_name( int p );

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
static inline int s_dub(const char *s)
{
	int len = strlen(s)+1;
	int x = m_create(len,1);
	m_write(x,0,s,len);
	return x;
}


/* Import a binary file */
#define IMPORT_BIN(sect, file, sym) asm (\
    ".section " #sect "\n"                  /* Change section */\
    ".balign 4\n"                           /* Word alignment */\
    ".global " #sym "\n"                    /* Export the object address */\
    #sym ":\n"                              /* Define the object label */\
    ".incbin \"" file "\"\n"                /* Import the file */\
    #sym "_end:\n"                              /* Define the object label */\
    ".byte 0\n" \
    ".global " #sym "_end\n"            /* Export the object size */ \
    ".global _sizeof_" #sym "\n"            /* Export the object size */\
    ".set _sizeof_" #sym ", . - " #sym "\n" /* Define the object size */\
    ".balign 4\n"                           /* Word alignment */\
    ".section \".text\"\n")                 /* Restore section */


/* Allocates foo.bin in constant section and reffered in FooBin */
IMPORT_BIN(".rodata", "compile.sh", COMPILE_SCRIPT );

/* Declaration of symbols (any type can be used) */
extern const unsigned char COMPILE_SCRIPT[], _sizeof_COMPILE_SCRIPT[], COMPILE_SCRIPT_end[];




int bf_scan_set(int bf, int *p);
void bf_set( int bf, int n );
void bf_clr( int bf, int n );
int bf_test( int bf, int n );
int bf_max(int bf);
int bf_min(int bf);
int bf_intersect_empty(int bf1, int bf2);
int bf_new(int len);



int bf_scan_set(int bf, int *p)
{
	int end = bf_max(bf);
	while( *p < end ) {
		(*p)++;
		if( bf_test(bf,*p) ) return 1;
	}
	return 0;
}


void bf_set( int bf, int n )
{
	int pos = n / 64;
	if( m_len(bf) <= pos )
		m_setlen(bf,pos+1);
	uint64_t *w = mls(bf,pos);
	n -= (64 * pos);
	*w |= ( 1L << n );	
}

void bf_clr( int bf, int n )
{
	int pos = n / 64;
	if( m_len(bf) <= pos )
		m_setlen(bf,pos+1);
	uint64_t *w = mls(bf,pos);
	n -= (64 * pos);
	*w &= ~( 1L << n );	
}

int bf_test( int bf, int n )
{
	int pos = n / 64;
	if( m_len(bf) <= pos )
		return 0;
	uint64_t *w = mls(bf,pos);
	n -= (64 * pos);
	return *w & ( 1L << n );
}

int bf_max(int bf)
{
	int n = 63;
	int p = m_len(bf);
	while( p-- ) {
		uint64_t w = * (uint64_t*) mls(bf,p);
		while( w ) {
			if( w & (1L<<63) ) return p*64 + n;
			n--; w<<=1;
		}
	}
	return -1;
}

int bf_min(int bf)
{
	int len = m_len(bf);
	int p = 0;
	int n = 0;
	while( p < len ) {
		uint64_t w = * (uint64_t*) mls(bf,p);
		while( w ) {
			if( w & 1 ) return p*64 + n;
			n++; w >>= 1;
		}
		p++;
	}
	return -1;
}



int bf_intersect_empty(int bf1, int bf2)
{
	int end = Min( bf_max(bf1), bf_max(bf2) );
	if( end < 0 ) return 1;

	int start = Max( bf_min(bf1), bf_min(bf2) );
	int p = start / 64;
	end /= 64;
	while( p <= end ) {

		uint64_t w1 = * (uint64_t*) mls(bf1,p);
		if( w1 ) {
			uint64_t w2 = * (uint64_t*) mls(bf2,p);
			if( w2 ) {
				w1 &= w2;
				if( w1 ) return 0;
			}
		}
		p++;
	}
	return 1;
}


int bf_new(int len)
{
	return m_create(len,sizeof(uint64_t));
}


void dump_bf_names(int bf)
{
	int t=-1;
	while(  bf_scan_set(bf,&t) )
	{
		TRACE(4, "%d %s", t, CHARP(t));
	}	
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




static void m_map( int m, void (*func) ( void *d, void *ctx ), void *ctx )
{
	int p; void *d;
	m_foreach( m, p, d ) func(d,ctx);
}

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
	if(in_list) gen_string_list( nodes, s_cstr("IN"),  in_list );
	if(out_list) gen_string_list( nodes, s_cstr("OUT"), out_list );
	if( dep_list) gen_string_list( nodes, s_cstr("DEP"), dep_list );
}

/* parse:
   recname target: file1 file2 file3\
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
	  if(p) putchar(',');
	  dump_element(d);
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
int search_obj_data_cs( int cs, int opts )
{
  struct njson_st *j;
  int p;
  m_foreach(opts,p,j) {
    if( cs == j->name ) {
      return j->d;
    }
  }
  return 0;
}


int search_obj_data( const char *s, int opts )
{
	int k = s_cstr(s);
	return  search_obj_data_cs(k,opts);
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


int s_integer( int val )
{
	char buf[20];
	snprintf(buf,sizeof buf,"%d", val );
	return s_cstr(buf);
}


static int create_node_cs(  int obj, int key_cs, int val_cs )
{
	struct njson_st nj;
	nj.typ = NJSON_STRING;
	nj.name =  key_cs;
	nj.d = val_cs;
	m_put( obj, &nj );
	return nj.d;
}

/*
  static int create_node_str(  int obj, const char *key, int val )
{
	int key_cs = s_cstr( is_empty(key) ? "" : key );  
	return create_node_cs( obj, key_cs, s_integer(val) );
}
*/



/* if node with name exists, return the value of the node data field
   else create the node as njson_string, convert val to m-string and save this in the node data field,
   then return the value of the node data field
*/
int fetch_node_cs( int obj, int k, int v )
{
  int d =  search_obj_data_cs( k, obj  );
  if( !d ) d=create_node_cs(obj,k,v );
  return d;
}

int fetch_node( int obj, const char *key, int default_val )
{
	int key_cs = s_cstr( is_empty(key) ? "" : key );
	int val_cs = s_integer(default_val);
	return fetch_node_cs(obj,key_cs,val_cs);
}

int fetch_node_int( int obj, const char *key, int default_val )
{
  int d = fetch_node(obj,key,default_val);
  return atoi( CHARP(d) );
}


/* if the node does not exists, create the node */
int set_node_cs( int opts, int key, int val )
{
  struct njson_st *j;
  int p;
  m_foreach(opts,p,j) {
    if( key == j->name ) {
	    return j->d = val;
    }
  }
  return create_node_cs(opts,key,val);
}

/* if the node does not exists, create the node */
int set_node( int opts, const char *key, int val )
{
	int k = s_cstr(key);
	int v = s_integer(val);
	return set_node_cs( opts, k, v );
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



int checksum_changed(int cs,int p);

void dump_checksum(int cs)
{
	cs_t *d; int p;
	m_foreach( cs, p, d ) {
		fprintf(stderr, "%d %s %s\n",p, d->fn, ctime( & d->mtim.tv_sec ) );
	}
	fprintf(stderr,"-------------\n");
	for(int i=0;i<m_len(cs);i++) {
		checksum_changed(cs,i);
	}
}

int read_checksum( const char *fn )
{
  int inc = 2;
  int len = 0;
  int rd;
  int cs = m_create( inc, sizeof( cs_t));
  FILE *fp = fopen(fn, "rb" );
  if(!fp) return cs;
  do {
	  inc*=2; /* OPTIMIZE */		  
	  m_setlen( cs,len + inc );
	  rd=fread( mls(cs,len), m_width(cs), inc, fp );
	  if( rd < 0 ) {
		  WARN("I/O ERROR READING %s", fn );
		  break;
	  }	  
	  len+=rd;
  } while( rd == inc );

  TRACE(4,"READ:%d INC:%d MAX:%d", rd,inc,len );
  m_setlen( cs,len );

  dump_checksum(cs);
  return cs;
}


/* read file modified date and store in checksum list */
void update_checksum_file( int cs, int fn )
{
  cs_t *d;
  struct stat sb = { 0 };
  if( s_empty(fn) ) return;
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


/* return true if entry p in checksum file has changed */
int checksum_changed( int cs, int pos )
{
	struct stat sb = { 0 };
	cs_t *d = mls(cs,pos);
	int err = stat( d->fn, &sb );
	if( err ) return 1;
	err =  memcmp(& d->mtim.tv_sec, & sb.st_mtim, sizeof  sb.st_mtim ) ;
	if( err ) {	  
		TRACE(4,"check: %d %s", pos, d->fn);
		TRACE(4,"cache: %s", ctime( &d->mtim.tv_sec));
		TRACE(4,"cur:   %s", ctime(&sb.st_mtim.tv_sec));
	}
	return err;
}


/* return true if there is no entry for fn or modifed date has changed
   or fn does not exists

   return false if modified date is unchanged
 */
int ismodified_checksum( int cs, int fn )
{
  int pos = m_bsearch( &fn, cs, cscmpmstr );
  if( pos < 0 ) return 1;
  return checksum_changed( cs, pos);
}




/* return true if all files in njson list jd are unchanged */
int filesunchanged_checksum(int cs, int jd )
{
  int p;
  struct njson_st *j;
  
  m_foreach( jd,p,j ) {
	  if(  ismodified_checksum(cs, j->d) ) {
		  TRACE(4,"changed: %s. LEN=%d", CHARP(j->d), m_len(cs));
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


int find_list_int( int lst, int obj )
{
	int p; int *d;
	m_foreach(lst,p,d) {
		if( *d == obj ) return p;
	}
	return -1;
}

int find_node_depending( int prep, int obj )
{
	int p; int *d;
	m_foreach(prep,p,d) {
		if( find_list_int( *d, obj ) >= 0 ) return p;
	}
	return -1;
}

void dump_list_int( int lst )
{
	int p; int *d;
	m_foreach(lst,p,d) {
		printf("%d ", *d );
	}
	printf("\n");
}

/** find node that depends on file `f`  
 *  @returns node position
 **/ 
int find_file_in_prep(int f, int prep)
{
	int p,i,*d,*n;
	m_foreach(prep,p,d) {
		m_foreach( *d, i, n ) {
			if( *n == f ) return p;
		}		
	}
	return -1;		
}

void add_outfiles(int nodes, int p, int cf)
{
	struct njson_st *j ;
	j = mls(nodes,p);
	int opts = search_obj_data( "OUT", j->d );
	if( opts <= 0 ) {
		TRACE(4,"OUT not found in node %d", p);
		return;
	}
	m_foreach(opts,p,j) {
		if( j->d == 0 || j->typ != NJSON_STRING ) continue;	  
		m_puti( cf, j->d );
		// TRACE(4, "ADD file %s", CHARP(j->d));
	}
}

/* search for entry 's' in tree opts0 and append all string member
   to the list conststr-list m
*/
void app_names_bf( int bf, char *s, int opts0 )
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
	  bf_set( bf , id );
  }
}


/* 1) find all files in cache that have changed, put files in 'newfn'
   2) repeat until nothing changes:
   find all nodes that are dependend on files in  'newfn'
   and put out file list in 'newfn'
   

 */
int find_dependend_nodes(int nodes, int cs )
{
#if 0
	dump_checksum( cs );
	{	int p; int *d;
		m_foreach(prep,p,d) {
			printf("%d: ", p );
			dump_list_int(*d);
		}
	}
#endif
	int p;
	struct njson_st *j;
	
	/* create a bitfiled for the in+dep file list of each node */
	int deps = m_create(10,sizeof(int));
	m_foreach(nodes,p,j) {
		int p1 = bf_new(2);
		m_puti(deps,p1);
		app_names_bf(p1, "IN",  j->d);
		app_names_bf(p1, "DEP", j->d);
	}

	/* create an index for all changed files */
	int newfn = bf_new(2);	
	cs_t *cd;
	m_foreach( cs, p, cd ) {
		if( checksum_changed( cs, p) ) {
			TRACE(4, "%d. changed: %s", p, cd->fn );
			int f =  s_cstr(cd->fn);
			bf_set(newfn, f );
		}
	}

	/* search nodes that contain files from `newfn`
	   append out file list of node to `newfn`
	   repeat as long as new nodes are found
	*/
	int found = 1;
	int checked_nodes = bf_new(2);
	int *d;
	while( found ) {
		found = 0;
		m_foreach(deps,p,d) {
			if( bf_test(checked_nodes,p) ) continue;
			if(! bf_intersect_empty(*d,newfn) ) {
				TRACE(4,"ADD %d", p );
				bf_set(checked_nodes,p);
				j = mls(nodes,p);
				app_names_bf(newfn, "OUT", j->d);
				found=1;
			}
		}
	}

	m_free_list(deps);
	m_free(newfn);
	return checked_nodes;	
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
	  int id = search_obj_data( "id", *d  );
	  printf("node%s() {\n",CHARP(id));
	  dump_string_array( "IN", *d );
	  dump_string_array( "OUT", *d );    
	  int lst = search_obj_data( "REC", *d  );
	  printf( "%s\nstore-result %s $?\n}\n", CHARP(lst), CHARP(id) );
  }
}

int s_atoi(int m)
{
	return s_empty(m) ? -1 : atoi(CHARP(m));
}

void export_bash_list(int g, int comp_ready, int ec)
{
	int p, *d, node, id;

	// do not export nodes found in ec
	p = m_len(comp_ready);
	while( p-- ) {
		node = INT(comp_ready,p);
		id =  fetch_node_int(node, "id", 0 );
		if( !id ||  get_exit_code( id, ec ) != -1 ) m_del(comp_ready,p);
	}
	
	/* export node definitions */
	export_bash_command(g,comp_ready);

	// list of ready nodes: nodes_ready=(id1 id2 ... )	
	printf( "nodes_ready=(" );	
	m_foreach(comp_ready,p,d) {
		int id = s_atoi(search_obj_data( "id", *d  )); if(id<=0) continue;	  
		if( get_exit_code( id, ec ) == -1 )
			printf( "%u ", id );
	}
	printf( ")\n" );
}


void create_rule_from_str(int name, int str, int offs)
{
	int words = m_create(2,sizeof(int));
	if( get_word_list(words,str,&offs) > 1 ) {
		rule_store(name,words);
	} else m_free(words);
}


/* compile a rule for use in node-generate
   rule:name:suffix-in suffix-out1 suffix-out2..  
*/
void cmd_rule(int nodes, int d )
{
	/* get first entry until space and return its cs id */
	int p=0;
	int ch;
	int str = s_new(20);
	ch = p_word(str, d,&p, " " );
	if( ch == 0 || m_len(str) < 2 ) goto leave;
	int id = s_mstr(str);
	create_rule_from_str(id,d,p);	
leave:
	m_free(str);
}

/* compile all rules in this object */
void cmd_rules(int nodes, int d)
{
	struct njson_st *j;
	int p;
	m_foreach(d,p,j) {
		if( j->typ == NJSON_STRING ) {
			create_rule_from_str(j->name,j->d,0);
		}
	}
}

int filename_ext( int fn )
{
	int p = s_index(fn,0,'.');
	if( p >= 0 ) return s_cstr( (char*) mls(fn,p+1) );
	return  s_cstr("");	
}

void auto_gen_for_file( int nodes, char *fn1 )
{

	TRACE(4,"%s", fn1 );
	int files=s_split( 0, fn1,':', 1 );
	int len_files = m_len(files);
	char * fn = STR(files,0);
	char *dot = NULL;

	int i = strlen(fn);
	if( i < 3 ) goto leave;
	while( i )
	{
		i--;
		if( fn[i] == '/' ) break;
		if( fn[i] == '.' ) dot = fn+i;
	}

	if( ! dot ) goto leave;

	int rule = rule_search_ext(s_cstr( dot ));
	if( rule < 0 ) goto leave;
	TRACE(4, "found: %s compile by %s", fn, CHARP( rule_get(rule)->name_cs ) );

	/* IN: fn1
	   REC: rule
	   DEP: files
	   OUT: rule
	*/
	int inf =  s_cstr(fn);
	int inf_list = m_create(1, sizeof(int));
	m_puti( inf_list, inf );	
	int out_list = m_create(2, sizeof(int));
	int suffix_list = rule_get(rule)->suffix_list;
	rule_expand_files( suffix_list, inf, out_list );
	int dep_list=0;
	if( len_files > 1 ) {
		dep_list = m_create( len_files -1 , sizeof(int));
		for(int i=1;i<len_files;i++) {
			m_puti(dep_list, s_cstr( STR(files,i)));
		}
	}
	gen_nodex( nodes, rule_name(rule), inf_list, out_list, dep_list ); 
	m_free(inf_list);
	m_free(out_list);
	m_free(dep_list);

leave:
	TRACE(4,"" );
	m_free_strings(files,0);
	TRACE(4,"" );
}



/* compile a list of files using rules
*/
void cmd_files(int nodes, int d )
{
	if( m_len(d) < 2 ) {
		WARN("wrong arg list %d", d  );
	}
	int names = m_split(0, m_buf(d),' ', 1 );
	char **str;
	int p=0;
	m_foreach( names,p,str) {
		
		if(! **str ) {
			WARN("empty str");
		} else {		
			auto_gen_for_file( nodes, *str );
		}	       
	}
	TRACE(4,"leaving");
	m_free_strings(names,0);
}


void cmd_rec(int nodes, int d )
{
	nodes=d=0;
}


struct internal_recs {
	int cs;
	char *name;
	int typ;
	void (*func) (int nodes, int d );
};

struct internal_recs INT_REC[] = {
	{ 0, "gen-each", NJSON_STRING,  gen_nodes },
	{ 0, "gen-rule", NJSON_STRING,  apply_rule },
	{ 0, "gen-node", NJSON_STRING,  gen_node_from_list },
	{ 0, "import",   NJSON_STRING,  importmm },
	{ 0, "rec",      NJSON_STRING,  cmd_rec },
	{ 0, "rule",     NJSON_STRING,  cmd_rule },
	{ 0, "files",    NJSON_STRING,  cmd_files },
	{ 0, "rules",    NJSON_OBJ,     cmd_rules },
	{ 0, NULL, 0, NULL }
};

static int INIT_INT_REC = 1;



/**
   params: n - name of node, t - typ, 
 * returns -1 if rule is unknown
 **/
int apply_named_rule( int nodes, int name, int typ, int d )
{
	TRACE(4,"%s", CHARP(name) );
	struct internal_recs *rec = INT_REC;
	if( INIT_INT_REC ) {
		INIT_INT_REC=0;
		while( rec->name ) {
			rec->cs = s_cstr(rec->name);
			rec++; 
		}
		qsort(  INT_REC, ALEN(INT_REC), sizeof(*rec), cmp_int );
	}

	rec = bsearch( &name, INT_REC, ALEN(INT_REC), sizeof(*rec), cmp_int );	
	if( !rec || rec->typ != typ ) {
		return -1; 
	}
	rec->func(nodes, d);	
	return 0;
}


void nj_del_obj(struct njson_st *j)
{
	if( j->typ == NJSON_OBJ ) {
		int p;
		struct njson_st *j1;
		m_foreach( j->d, p, j1 ) {
			nj_del_obj(j1);	
		}
		m_free(j->d);
		j->d=0;
		return;
	}
	j->d=s_cstr("");
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
  
	m_foreach(global_state,p,j) {
		if( j->d==0 ) continue;
		if( apply_named_rule(nodes,j->name,j->typ, j->d) == 0 ) {
			nj_del_obj(j);	
		}
	}
	TRACE(4,"update nodes and cache");

	
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
		int p1 = bf_new(2);
		m_puti(prep,p1);
		app_names_bf(p1, "IN",  j->d);
		app_names_bf(p1, "DEP", j->d);
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

	int in_cache = bf_new(2);


       	/* 1) ndep: all OUT-files with exit_code != 0
	   i.e. known but not yet generated files.
	   2) if in-files and out-files are in the cache and are unchanged,
	   assume out-files are generated amd store node position in_cache[]
	 */
	int csdeps = find_dependend_nodes(nodes,checksum);
	int ndep = bf_new(2);
	m_foreach(nodes,p,j) {
		int err = fetch_node_int(j->d, "exit_code", -1 );
		if( err ==0 ) continue;
		if( ! bf_test(csdeps,p) && node_is_cached(checksum,j->d) ) {
			bf_set( in_cache, p );
			TRACE(4,"Node is cached: %d", p );
			continue;
		} else TRACE(4,"Node NOT in cached: %d", p );
		
		app_names_bf(ndep, "OUT",  j->d);
	}
	
	/* check all files dependend on changed files in checksum and put them in ndep */
	// update_ndep_from_checksum( nodes, checksum, ndep, prep );
	
         /* RULE1) comp_ready:= for all nodes with:
	   1) prep list (in+dep) contains nothing from ndep and
	   2) exit_code != 0
	   3) not to compile if (exit_code == -1 and node_cached())
	*/
	dump_bf_names(ndep);
	TRACE(4,"deps of node0");
	dump_bf_names( INT(prep,0) );
	
	
	int comp_ready = m_create(100,sizeof(int));
	int unsatisifed_deps = m_create(100,sizeof(int));
	m_foreach(nodes,p,j) {
		if( fetch_node_int(j->d, "exit_code", -1 ) == 0 ) continue;
		if( bf_test( in_cache, p ) ) {
			TRACE(4,"Node is cached: %d", p );		       
			continue;
		}
		int p1 = INT(prep,p);		
		if(  bf_intersect_empty( p1, ndep ) ) {
			m_puti(comp_ready,j->d);
			TRACE(4,"todo node nr:%d", p );
		} else {
			m_puti(	unsatisifed_deps, j->d );
		}
	}
	m_free(csdeps);
      
	/* fallback method: REDO
	   if no node is ready, but there are nodes with exit_code!=0
	   put those in comp_ready
	*/
	if( m_len(comp_ready) == 0 ) {
		m_free(comp_ready);
		comp_ready = unsatisifed_deps;
		unsatisifed_deps = 0;
	}
	
	// export_bash_command(global_state,comp_ready);
	export_bash_list(global_state,comp_ready,ec);
	
	m_free(ndep);
	m_free(comp_ready);
	m_free_list(prep);
	m_free(in_cache);
	m_free(unsatisifed_deps);
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
		apply_named_rule(nodes, r_name, NJSON_STRING, r_text );
		m_free(r_text);
	}
	m_free(w);
}



/* 1) read exit codes and global state
   2) find nodes, print nodes to stdout
   3) create shell script to run

   -b : build makefile
   
*/


void m_nputs(int str, int len)
{
	int p; char *ch;
	m_foreach(str,p,ch) {
		if(! len-- ) break;
		putchar( *ch );
	}
	putchar(10);
}

/* append list l2 to list l1, if l1 is zero create l1
   returns the list l1
*/
int m_app( int l1, int l2 )
{
	int len = 0;
	if( l1>0 ) len = m_len(l1);
	return m_slice( l1, len, l2, 0, -1 );
}

/*
int m_dub(int l2)
{
	return m_app(0,l2);
}
*/


/**
 * @brief Copies a portion of the list `m` starting at index `a` and ending at index `b` to a new list at position `offs`, and returns the new list.
 *
 * - If `dest` is zero, a new destination list is created.
 * - Indices can be positive or negative:
 *   - Negative indices count from the end to the start of the list.
 *   - The first element is 0, and the last element is -1.
 *
 * Example:
 * @code
 *   m:    0    1    2    3    4
 *       -5   -4   -3   -2   -1
 * @endcode
 *
 * @param dest The destination list where the portion is copied. If set to 0, a new list is created.
 * @param offs The offset position in the destination list where the copied portion is placed.
 * @param m The source list from which the portion is copied.
 * @param a The starting index of the portion to be copied.
 * @param b The ending index of the portion to be copied.
 * @return The new list with the copied portion.
 */
int m_slice(int dest, int offs, int m, int a, int b )
{
	int len = m_len(m);
	if( b < 0 ) { b+= len; }
	if( a < 0 ) { a+= len; }
	if( b < a ) { int t = b; b=a; a=t; }
	if( b >= len ) b = len -1;
	int cnt = b-a +1;
	if( dest <= 0 ) dest = m_create(cnt+offs, m_width(m));
	m_setlen(dest,offs);
	for( int i = a; i <= b; i++ ) m_put(dest, mls(m,i) );
	return dest;
}

/* slice for strings: add a zero byte to the end */
int s_slice(int dest, int offs, int m, int a, int b )
{
	int ret = m_slice(dest,offs,m,a,b);
	if( m_len(ret) > 0 && CHAR( ret, m_len(ret)-1 ) != 0 ) m_putc(ret,0);
	return ret;
}


/* expand a rule by adding suffixes to basename of 'inf'
   'inf' must have matching suffix
   created strings are handle by conststr
   e.g:
   rule:[.y .tab.c .tab.h]
   inf:'parser.y'
   OUT=[ parser.tab.c, parser.tab.h ]
   @returns -1 if suffix does not match rule
*/
int rule_expand_files( int rule, int inf, int OUT )
{
	/* create basename by removing suffix rule[0] from in-file, 
	   then for each suffix rule[1..n] create a new filename and put it in OUT
	*/
	int suff = INT(rule,0);
	int sl = m_len(suff);
	int p=m_len(inf);

	while(p-- && sl-- && (CHAR(inf,p) == CHAR(suff,sl))) ;
	if( p < 1 || sl >= 0 ) return -1;

	for( int i = 1; i < m_len(rule); i++ ) {
		int fn=m_slice(0,0, inf, 0, p);
		m_app( fn, INT(rule,i) );		
		m_puti( OUT, s_mstr(fn) );
		m_free(fn);
	}
	
	return 0;
}

int rule_compile( int rule_str, int *p, int rule )		
{
	get_word_list(rule,rule_str,p);
	if( m_len(rule) < 2 ) {
		m_clear(rule);
		return -1;
	}
	return 0;
}


void rule_init(void)
{
	RULES = m_create(10,sizeof(rule_t));
}

void rule_free_x(void *element, void *dummy)
{
	rule_t *r = element;
	m_free(r->suffix_list);
	(void)dummy;
}

void rule_free(void)
{
	m_map(RULES,rule_free_x,0);
	m_free(RULES);
	RULES=0;
}

void rule_store( int rule_name, int suffix_list )
{
	rule_t *r;
	/* sort lookup works on strings because conststr garanties that
	   identical strings have identical handles  */
	r = mls(RULES, sort_lookup( RULES, rule_name ));
	r->suffix_list=suffix_list;
	TRACE(4,"Rule Created: %s", CHARP(rule_name) );
}

int rule_search_ext(int pat)
{
	rule_t *r;
	int p;
	m_foreach( RULES,p,r ) {
		TRACE(4, "%d %d %s %s",
		      INT(r->suffix_list,0),
		      pat,
		      CHARP( INT(r->suffix_list,0) ),
		      CHARP( pat ) );
		      
		      
		if( INT(r->suffix_list,0) == pat ) return p;
	}
	return -1;	
}

rule_t* rule_get( int p )
{
	return  mls(RULES,p);
}


int rule_fetch( int rule_name )
{
	rule_t *r;
	r = mls(RULES, sort_lookup( RULES, rule_name ));
	return r->suffix_list;
}

int rule_name( int p )
{
	return rule_get(p) ->name_cs; 
}


void test_mapp(void)
{
	int in = s_printf(0,0, "Hello World" );
	int out = m_slice(0,0,in,0,-8); /* trailing zero is -1 */ 
	m_putc(out,0);

	int p; char *d;
	m_foreach(out,p,d) {
		printf("%d: %02x %c\n", p, *((unsigned char*)d), *d );
	}
	
	m_free(in);
	m_free(out);
	
}



void test_rule_compile(void)
{
  TRACE(1,"start");
  
	test_mapp();
	
  int inf = cs_printf(0,0, "parser.y" );
  int rule_str = cs_printf(0,0, ".y .tab.c .tab.h" );
  int rule  = m_create( 10, sizeof(int));
  int out = m_create( 10, sizeof(int));

  int id =  s_cstr("bison");
  
  rule_init();
  int p=0;
  rule_compile( rule_str,&p, rule );
  rule_store( id, rule ); rule=0;
  
  int r = rule_fetch( id );
  rule_expand_files(r, inf, out );

  m_free(out);
  rule_free();

  printf("\nEXIT\n");
  conststr_free(); m_destruct(); exit(1);
}

const char *BM_HEAD=""							
"		(							\n"
"		(* define what files those bash functions generate *)	\n"
"		rules: (						\n"
"			sbison: '.y .c .h'				\n"
"			sflex: '.l .c .h'				\n"
"			ccompile: '.c .o'				\n"
"			)						\n"
"";

int repl_suffix( int s, const char *fn, const char *suffix )
{
	int dot = 0;
	int i = strlen(fn);	
	if( i < 3 ) return 0;

	while( i )
	{
		i--;
		if( fn[i] == '/' ) break;
		if( fn[i] == '.' ) dot = i;
	}
	if( ! dot ) return 0;
	if( !s ) s=s_new( dot + strlen(suffix) + 1);
	m_clear(s);
	m_write( s,0, fn, dot );
	m_write( s,dot,suffix,strlen(suffix)+1);
	return s;
}

void autogen(int c, char**v)
{


	int fnlist = m_create(10,sizeof(int));
	int p,*d;
	FILE *fp ;
	TRACE(4,"");
	fp = fopen( "compile.sh", "w" );	
	fputs( (char*)COMPILE_SCRIPT, fp );
	fclose(fp);

	fp = fopen( "bm.nodes", "w" );
	fputs( BM_HEAD, fp );

	fputs("files: '", fp );
	for( int i = 3; i<c; i++ ) {
		fprintf(fp, "%s ", v[i] );		
		int fn = repl_suffix( 0, v[i], ".o" );
		if( fn ) m_puti(fnlist,fn );

	}
	
	fputs("'\n", fp );

	fputs("nodes: (\n(\n", fp );
	fprintf(fp, "OUT: ('%s')\n", v[2] );

	fprintf(fp, "IN:  (" );
	m_foreach(fnlist,p,d) {
		fprintf(fp,"'%s' ", CHARP(*d));
	};	
	fprintf(fp, ")\n" );
	fprintf(fp, "REC:  slink\n)))" );
	
	
	fclose(fp);
	m_free_list( fnlist );
}



int main(int argc, char **argv)
{
  trace_level=4;
  m_init();
  conststr_init();
  rule_init();
  int add_rules = m_create(10,sizeof(int));
  int rule = m_create(100,1);
  int ec = m_create(10,sizeof(int));  
  int new_cs = m_create(10,sizeof(int));

  if( argc > 1 ) {
	  if( strcmp(argv[1],"autogen" ) == 0 ) autogen(argc,argv);
	  goto leave;	  
  }
  
  
  
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
  int x; while( scanf("%u", &x)==1 ) { TRACE(4, "error codes: %d", x ); m_puti(ec,x); }

  int opts = njson_from_file(stdin);
  int nodes = search_obj_data( "nodes", opts );
  if( !nodes ) {
	  nj_add_obj(opts, s_cstr("nodes"), 1 );
  }
  
  
  apply_prepended_rules( opts, add_rules );	  

  int checksum = read_checksum( CHECKSUM_FN );
  find_nodes(opts,ec, checksum, new_cs);
  update_checksum_list(checksum, new_cs);
  save_checksum( checksum, CHECKSUM_FN );
  m_free(checksum);

leave:
  m_free_list(add_rules);
  m_free(rule);  
  m_free(new_cs);  
  m_free(ec);
  njson_free(opts);
  conststr_stats();
  conststr_free();
  rule_free();
  m_destruct();
  return EXIT_SUCCESS;
}
