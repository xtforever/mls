#include "json_read.h"
#include "mls.h"

void v_dump_object(int opts);

void v_dump_element( int var )
{
    char *n = STR(var,0);
    char *v = STR(var,1);
 
    printf("%s: ", n );

    if( *v == 0x01 ) { /* dump object */
	int list = atoi( v+1 );
	v_dump_object(list);
	return;
    }

    printf("%s", v );
    
    for(int i=2; i < m_len(var); i++ )
	printf(",%s", STR(var,i));
    printf("\n");
}

void v_dump_object(int opts)
{
    TRACE(1,"%d", opts);

    printf("{\n");
    int *d,p;
    m_foreach(opts,p,d) {
	v_dump_element(*d);
    }
    v_free(opts);
    printf("}\n");
}


int main(int argc, char **argv)
{
  FILE *fp;


  trace_level=3;
  m_init();
  TRACE(1,"start");
  ASSERT( (fp=fopen( "settings.json", "r" )));
  int opts = json_init( fp );
  printf("\n***************");
  // v_dump_object(opts);
  fclose(fp);

  m_destruct();


  
  return EXIT_SUCCESS;
}
