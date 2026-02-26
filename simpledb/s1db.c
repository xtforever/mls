#include "m_tool.h"
#include "var5.h"


     

int  DB=0;
int s_rindex(int n, char ch)
{
	int p = m_len(n);
	while( p-- ) {
		if( CHAR(n,p) == ch ) break;
	}
	return p;
}




int  readfile()
{
	int tmp = m_create(100,1);
	FILE *fp = fopen("movies-db", "r");
	int SERIEN =  mvar_parse_string( "serien",VAR_VSET );
	int FILME  =  mvar_parse_string( "filme", VAR_VSET );
	int s_serien = s_cstr("serien");
	TRACE(3, "SERIEN %d, FILME %d", SERIEN, FILME );
	while( s_readln(tmp,fp) != EOF ) {
		int p = s_rindex(tmp,'/');
		if( p <= 0 ) continue;
		int key = s_slice(0,0,tmp,p+1,-1);
		for( p=0; p < m_len(key); p++ ) {			
			char ch=CHAR(key,p);
			if( ! isalnum(ch) ) {
				m_del(key,p--);
			}
		}
		int v = FILME;
		if( s_strstr(tmp,0,s_serien) != -1 ) v  = SERIEN;
		int q = mvar_lookup(v, m_str(key), VAR_STRING );
		mvar_put_string( q, m_str(tmp), -1 );
		m_free(key);
	}
	fclose(fp);
	m_free(tmp);
	return 0;
}

void var_dump(int id)
{
    int varname = mvar_path(id, 0);
    int t = mvar_type(id) ;
    char *typename = mvar_name_of_type( t );
    printf("varname: %s\n", m_str(varname)  );
    m_free(varname);
    printf("typename: %s\n", typename  );
    int l = mvar_length(id);
    printf("length: %d\n", l );
    printf("content: ");

    if( t == VAR_VSET ) {
        for(int i=0;i<l;i++) {
            int x = mvar_get_integer(id,i);
            printf("dumping var: %d\n", x );
            var_dump(x);
        }
    }
    else {
        int ch = 32;
        for(int i=0;i<l;i++) {
            char *s = mvar_get_string(id,i);
            printf("%c%s", ch, s); ch=',';
        }
        putchar(10);
    }

    putchar(10);

}


void read_db(void)
{
	int p; int *d;
	readfile();
	int q = mvar_parse_string( "#0.database", 0 );
	int q1 = mvar_lookup( q, "filme", -1 );
	var_dump(q1);
	
	
}


int main()
{
	m_init(); mvar_init(); conststr_init(); trace_level=3;
	readfile();
	int q1 = mvar_parse_string( "filme", VAR_VSET );
	int q2 = mvar_parse_string( "serien", VAR_VSET );
	printf("%d %d\n", q2,q1 );
/*
	int q1 = mvar_parse_string( "test1.test2.test3", VAR_VSET );
	int q2 = mvar_parse_string( "test1.test2", VAR_VSET );
	int q3 = mvar_parse_string( "test1", VAR_VSET );
	int q4 = mvar_lookup( q3, "test2", VAR_VSET );
	printf("%d %d\n", q4,q2 );
	var_dump( q3 );
*/

	m_destruct();
}
