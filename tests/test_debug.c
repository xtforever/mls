#include "../lib/mls.h"
#include "../lib/m_tool.h"
#include "../lib/m_http.h"
#include <stdio.h>
#include <string.h>

int main() {
    trace_level = 1;
    m_init();
    conststr_init();

    http_parser_t p;
    http_parser_init(&p);

    const char *raw = "GET /index.html HTTP/1.1\r\nHost: localhost\r\nUser-Agent: test\r\n\r\n";
    int data = s_strdup_c(raw);
    
    printf("Data length: %d\n", m_len(data));
    printf("Data: %.*s\n", m_len(data), m_buf(data));
    
    int res = http_parse(&p, data);
    printf("Result: %d\n", res);
    printf("State: %s\n", http_state_string(p.state));
    printf("Error: %s\n", http_error_string(p.error));
    
    m_free(data);
    http_parser_free(&p);
    
    conststr_free();
    m_destruct();
    return 0;
}
