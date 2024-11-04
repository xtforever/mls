#include "m_tool.h"


int  DB=0;




void readfile()
{
	int tmp = m_create(100,1);
	FILE *fp = fopen("movies-db", "r");

	while( m_readline(tmp,fp) != EOF ) {
		int p = s_rindex(tmp,'/');
		printf("%d\n", p );
		int key = s_slice(0,0,tmp,p+1,-1);
		v_set(DB, m_str(key), m_str(tmp), -1 );
		m_free(key);
	}
	fclose(fp);
	m_free(tmp);
}

void read_db(void)
{
	int p; int *d;
	readfile();
	m_foreach(DB ,p,d) {
		if( m_len(*d) > 3 ) {
			printf( "%s\n", *(char**) mls(*d,1) ); 
		}		
	}
	
}


int main()
{
	m_init();
	DB=v_init();
	read_db();
	v_free(DB);
	m_destruct();
}
