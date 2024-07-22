#include "mls.h"

/*
       strstr(3)    - s_strstr
       strtok(3)    - s_split
*/

void m_clear_mlist(int m)
{
        int p,*d;
        m_foreach( m, p, d ) m_free(*d);
	m_clear(m);
}

void m_free_mlist(int m)
{
        int p,*d;
        m_foreach( m, p, d ) m_free(*d);
	m_free(m);
}

/**
 * Splits the string `s` at each occurrence of the character `c` and copies the handles to the resulting substrings into the array list `m`.
 * 
 *
 * - An empty string results in an entry with a string of length zero.
 * - A string containing only the separator results in a string list with two entries, both empty.
 * - If `m` is set to 0, a new string list is created. Otherwise, the existing list is cleared and used.
 * - If `remove_wspace` is non-zero, leading and trailing whitespace characters in the resulting substrings are removed.
 *
 * @param m The string list to which the substrings are copied. If set to 0, a new list is created.
 * @param s The input string to be split.
 * @param c The character used as the delimiter for splitting the string.
 * @param remove_wspace Flag to indicate whether leading and trailing whitespace should be removed from the substrings.
 * @return The generated string list.
 */
int xs_split(int m, const char *str, const char *sep, int remove_wspace)
{
	if(!m) m=m_create(10,sizeof(int)); else m_clear_mlist(m);

	if( is_empty(str) || is_empty(sep) ) {
		goto nothing_found;
	}

	const char* stop = str + strlen(str);
	int skip = strlen(sep);
	int w=0;
	const char *end;
	
	while( str < stop ) {
		end = strstr(str,sep);
		if( !end ) end = stop;		
		const char *start = str;
		str = end + skip;
		
		if( remove_wspace ) {
			while( start < end && isspace( start[0] ) ) start++;
			while( end > start && isspace( end[-1] )) end--;
		}

		w = m_create( end-start+1, 1 );
		if(end > start) {
			m_write(w,0,start,end-start);
		} 
		m_putc(w,0);
		m_puti( m, w );
	}

	if( m_len(m) == 0 ) {
	nothing_found:
		m_puti(m, s_printf(0,0,""));
	}

	return m;
}


/**
 * The xs_strstr() function finds the first occurrence of the substring needle in the mstring haystack.
 *
 * @param m The input mstring to be split.
 * @param p The starting position in m
 * @param needle The string to search for
 * @return The postion of needle or -1 if not found
 */
int xs_strstr(int m, int p, const char *needle)
{
	if( ! (m && m_len(m) && needle && needle[0] && p >=0 && p < m_len(m) && m_width(m)==1 ) )
		return -1;
	
	const char * hay = mls(m,p);
	char *pos = strstr(hay,needle);
	if( ! pos ) return  -1;
	return p+ (pos - hay);
}





/**
 * reads from fp to buf until newline 
 * the buffer is cleared before used
 *
 * @return -1 end of file, or number of bytes read until delimeter found 
 */
int xs_fgetline( FILE *fp, int buf )
{
	if( m_len(buf) ) m_clear(buf);
	int l = m_fscan2( buf, 10, fp );
	return ( l == 10 ) ? m_len(buf)-1 : -1;
}





 

int main()
{
	m_init();
	trace_level=1;

	int hay = s_printf(0,0, "hello is world is is:isisrest" );
	int needle = s_printf(0,0, "is" );

	int p = 0;
	while( (p=xs_strstr(hay,p,CHARP(needle))) >= 0 ) {
		printf("Found at %d\n", p );
		p+=s_strlen(needle);
	}


	int m = xs_split(0, CHARP(hay), CHARP(needle), 1 );
	int *d;
        m_foreach( m, p, d ) {
		printf( "%d. %s\n", p, CHARP(*d) );
	}
	
	s_printf(hay,0, "" );
	xs_split(m, CHARP(hay), CHARP(needle), 1 );
	m_foreach( m, p, d ) {
		printf( "%d. %s\n", p, CHARP(*d) );
	}
	

	m_free_mlist(m);
	m_free(hay);
	m_free(needle);




	
	m_destruct();
	
	
	
	
}
