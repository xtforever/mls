/*
  beispiel fuer:

  getline
  m_fscan2
  
  und ini file reader 


 */



#include "mls.h"


#define INI_FILE "settings.ini"

/**
   @returns: -1 end of file
   0 leere zeile gelesen
   1..n anzahl gelesener zeichen
**/
int m_getline( int buf, FILE *fp )
{
  if( m_len(buf) ) m_clear(buf);
  int l = m_fscan2( buf, 10, fp );
  return ( l == 10 ) ? m_len(buf)-1 : -1;
}


void vl_dump(int opts)
{
  int p,*d;
  
  m_foreach( opts, p,d ) {
    printf( "[%s] = [%s]\n", STR( *d, 0 ), STR(*d,1) );
  }

}



/**
   @returns array of string-list -> vars
**/
int ini_read( FILE *fp )
{
  int ln=0;
  int opts = v_init(); // return buffer 
  int sl=m_create(10, sizeof(char*));  // split line buffer
  int line=m_create(100,1); // single line buffer
  ssize_t read;
  while ((read = m_getline(line, fp)) != -1) {
    ln++;
    if(! read || CHAR(line,0)=='#' ) continue; // ignoriere leerzeilen, comments
    m_split(sl, mls(line,0), '=', 1 ); 
    if( m_len(sl) == 2 && strlen( STR(sl,0) ) > 0 )
      v_set( opts, STR(sl,0), STR(sl,1), -1 );  
    else printf( "Error in Line: %d\n", ln );
  }
  m_free_strings(sl,0);
  m_free(line);
  return opts;
}



int main(int argc, char **argv)
{
  FILE *fp;
  int opts;

  trace_level=0;
  m_init();
  
  ASSERT( (fp=fopen( INI_FILE, "r" )));

  opts = ini_read( fp );
  vl_dump(opts);
  v_free(opts);
  fclose(fp);
  m_destruct();

  return EXIT_SUCCESS;
}
