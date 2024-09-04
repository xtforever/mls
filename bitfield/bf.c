#include "mls.h"
#include <stdint.h>
typedef uint32_t u32;


/*

  set-bit in sparse array


  n[] = bit start array


  n[0] = 10/3, 1000/4, 5000/5

  arrays to hold bits 
  3,4,5

  insert new array if distance > 2xArray Size && distance > 100
  
  


  
 */




u32 msbDeBruijn32( u32 v )
{
    static const int MultiplyDeBruijnBitPosition[32] =
    {
        0, 9, 1, 10, 13, 21, 2, 29, 11, 14, 16, 18, 22, 25, 3, 30,
        8, 12, 20, 28, 15, 17, 24, 7, 19, 27, 23, 6, 26, 5, 4, 31
    };

    v |= v >> 1; // first round down to one less than a power of 2
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;

    return MultiplyDeBruijnBitPosition[( u32 )( v * 0x07C4ACDDU ) >> 27];
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


int main()
{
	trace_level=0;
	m_init();

	int bf1 = bf_new(100);
	int bf2 = bf_new(100);


	ASSERT( bf_min(bf1) == -1 );
	ASSERT( bf_max(bf1) == -1 );

	bf_set( bf1, 32 );

	ASSERT( bf_min(bf1) == 32 );
	ASSERT( bf_max(bf1) == 32 );
	
	ASSERT( bf_intersect_empty(bf1,bf2) );
	
	bf_set( bf2, 32 );
	ASSERT(! bf_intersect_empty(bf1,bf2) );


	bf_clr( bf2,32 );
	ASSERT( bf_intersect_empty(bf1,bf2) );

	bf_set(bf2,66);
	ASSERT( bf_intersect_empty(bf1,bf2) );

	bf_set( bf1,66);
	ASSERT(! bf_intersect_empty(bf1,bf2) );

	ASSERT( ! bf_test(bf1,32 ) );
	ASSERT( bf_test(bf1,66 ) );

	

	
	m_free(bf1);
	m_free(bf2);
	
	m_destruct();
	
}
