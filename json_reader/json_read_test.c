#include "json_read.h"
#include "mls.h"

void v_dump(int opts);

void v_dump_var( int var )
{
    char *n = STR(var,0);
    char *v = STR(var,1);

    printf("%s: ", n );
    if( *v == 0x01 ) /* it's a list */
	{
	    printf("{ ");
	    int list = atoi( v+1 );
	    v_dump(list);
	    printf("}\n");
	    return;
	}

    printf("%s", v );
    for(int i=2; i < m_len(var); i++ )
	printf(",%s", STR(var,i));
    printf("\n");
}

void v_dump(int opts)
{
    TRACE(1,"%d", opts);
    int *d,p;
    m_foreach(opts,p,d) {
	v_dump_var(*d);
    }
}


int main(int argc, char **argv)
{
  FILE *fp;


  trace_level=1;
  m_init();
  TRACE(1,"start");
  ASSERT( (fp=fopen( "settings.json", "r" )));
  int opts = json_init( fp );
  printf("\n***************");
  v_dump(opts);
  fclose(fp);
  m_destruct();


  
  return EXIT_SUCCESS;
}
