#include "../lib/mls.h"
#include "../lib/m_tool.h"
#include "../lib/m_http.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

void test_empty_keys() {
    printf("Testing headers with empty keys...\n");
    http_parser_t p;
    http_parser_init(&p);

    // Header with empty key (should be ignored or handled gracefully)
    const char *raw = "GET / HTTP/1.1\r\n: emptykey\r\nHost: localhost\r\n\r\n";
    int data = s_strdup_c(raw);
    
    int res = http_parse(&p, data);
    assert(res == 0);
    assert(p.state == HTTP_STATE_DONE);
    
    int host = m_table_get_cstr(p.headers, "host");
    assert(host > 0);
    assert(strcmp(m_str(host), "localhost") == 0);

    m_free(data);
    http_parser_free(&p);
    printf("Empty keys test passed.\n");
}

void test_malformed_headers() {
    printf("Testing malformed headers...\n");
    http_parser_t p;
    http_parser_init(&p);

    // Header without colon
    const char *raw = "GET / HTTP/1.1\r\nMalformedHeaderNoColon\r\nHost: localhost\r\n\r\n";
    int data = s_strdup_c(raw);
    
    int res = http_parse(&p, data);
    assert(res == 0); // Current implementation ignores lines without colon
    assert(p.state == HTTP_STATE_DONE);
    
    int host = m_table_get_cstr(p.headers, "host");
    assert(host > 0);
    assert(strcmp(m_str(host), "localhost") == 0);

    m_free(data);
    http_parser_free(&p);
    printf("Malformed headers test passed.\n");
}

void test_max_headers_limit() {
    printf("Testing max headers limit...\n");
    http_parser_t p;
    http_parser_init(&p);
    p.max_headers = 5;

    const char *raw = "GET / HTTP/1.1\r\nH1: v1\r\nH2: v2\r\nH3: v3\r\nH4: v4\r\nH5: v5\r\nH6: v6\r\n\r\n";
    int data = s_strdup_c(raw);
    
    int res = http_parse(&p, data);
    assert(res == -1);
    assert(p.state == HTTP_STATE_ERROR);

    m_free(data);
    http_parser_free(&p);
    printf("Max headers limit test passed.\n");
}

void test_max_header_line_limit() {
    printf("Testing max header line limit...\n");
    http_parser_t p;
    http_parser_init(&p);
    p.max_header_line = 10;

    const char *raw = "GET / HTTP/1.1\r\nVeryLongHeaderLine: value\r\n\r\n";
    int data = s_strdup_c(raw);
    
    int res = http_parse(&p, data);
    assert(res == -1);
    assert(p.state == HTTP_STATE_ERROR);

    m_free(data);
    http_parser_free(&p);
    printf("Max header line limit test passed.\n");
}

int main() {
    trace_level = 1;
    m_init();
    conststr_init();

    test_empty_keys();
    test_malformed_headers();
    test_max_headers_limit();
    test_max_header_line_limit();

    conststr_free();
    m_destruct();
    printf("All improved HTTP tests completed successfully.\n");
    return 0;
}
