#include "../lib/mls.h"
#include "../lib/m_tool.h"
#include "../lib/m_http.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

void test_http_basic() {
    printf("Testing basic HTTP GET...\n");
    http_parser_t p;
    http_parser_init(&p);

    const char *raw = "GET /index.html HTTP/1.1\r\nHost: localhost\r\nUser-Agent: test\r\n\r\n";
    int data = s_strdup_c(raw);
    
    int res = http_parse(&p, data);
    assert(res == 0);
    assert(p.state == HTTP_STATE_DONE);
    assert(strcmp(m_str(p.method), "GET") == 0);
    assert(strcmp(m_str(p.uri), "/index.html") == 0);
    assert(strcmp(m_str(p.version), "HTTP/1.1") == 0);
    
    int host = m_table_get_cstr(p.headers, "host");
    assert(host > 0);
    assert(strcmp(m_str(host), "localhost") == 0);

    m_free(data);
    http_parser_free(&p);
    printf("Basic HTTP GET passed.\n");
}

void test_http_post_body() {
    printf("Testing HTTP POST with body...\n");
    http_parser_t p;
    http_parser_init(&p);

    const char *raw = "POST /api/data HTTP/1.1\r\nContent-Length: 11\r\nContent-Type: text/plain\r\n\r\nhello world";
    int data = s_strdup_c(raw);
    
    int res = http_parse(&p, data);
    assert(res == 0);
    assert(p.state == HTTP_STATE_DONE);
    assert(strcmp(m_str(p.method), "POST") == 0);
    assert(m_len(p.body) == 11);
    assert(memcmp(m_buf(p.body), "hello world", 11) == 0);

    m_free(data);
    http_parser_free(&p);
    printf("HTTP POST with body passed.\n");
}

int main() {
    trace_level = 1;
    m_init();
    conststr_init();

    test_http_basic();
    test_http_post_body();

    conststr_free();
    m_destruct();
    printf("All HTTP tests completed successfully.\n");
    return 0;
}
