#include "ini_read2.h"
#include "mls.h"

#define INI_FILE "settings.ini"

int main(int argc, char **argv)
{
    char *port1;

    trace_level=1;
    m_init();
    
    rc_init( INI_FILE );
    rc_dump();
    

    port1 = rc_key_value( "global", "port" );
    printf("global port setting %s\n", port1 );

    port1 = rc_key_value( "httpd", "port" );
    printf("httpd  port setting %s\n", port1 );


  
    rc_free();
  
    m_destruct();
    return EXIT_SUCCESS;
}
