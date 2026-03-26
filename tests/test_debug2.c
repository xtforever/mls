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
    
    int uri = s_strdup_c("/index.html");
    printf("URI handle: %d\n", uri);
    printf("URI len: %d\n", m_len(uri));
    
    char *uri_str = m_str(uri);
    printf("URI str: %s\n", uri_str);
    
    for (int i = 0; i < m_len(uri); i++) {
        unsigned char c = uri_str[i];
        printf("  [%d] = %d ('%c')\n", i, c, c >= 32 && c < 127 ? c : '?');
        if (c < 32 || c == 127) {
            printf("  -> REJECTED!\n");
        }
    }
    
    int valid = http_validate_uri(uri);
    printf("Validation result: %d\n", valid);
    
    m_free(data);
    m_free(uri);
    http_parser_free(&p);
    
    conststr_free();
    m_destruct();
    return 0;
}
