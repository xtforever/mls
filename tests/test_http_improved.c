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

void test_chunked_encoding() {
    printf("Testing chunked transfer encoding...\n");
    http_parser_t p;
    http_parser_init(&p);

    const char *raw = "POST /upload HTTP/1.1\r\n"
                      "Host: localhost\r\n"
                      "Transfer-Encoding: chunked\r\n"
                      "\r\n"
                      "5\r\n"
                      "hello\r\n"
                      "6\r\n"
                      " world\r\n"
                      "0\r\n"
                      "\r\n";
    int data = s_strdup_c(raw);
    
    int res = http_parse(&p, data);
    assert(res == 0);
    assert(p.state == HTTP_STATE_DONE);
    assert(p.is_chunked == 1);
    assert(m_len(p.body) == 11);
    assert(memcmp(m_buf(p.body), "hello world", 11) == 0);

    m_free(data);
    http_parser_free(&p);
    printf("Chunked encoding test passed.\n");
}

void test_memory_safety() {
    printf("Testing memory safety features...\n");
    http_parser_t p1, p2;
    
    http_parser_init(&p1);
    const char *raw1 = "GET /first HTTP/1.1\r\nHost: localhost\r\n\r\n";
    int data1 = s_strdup_c(raw1);
    http_parse(&p1, data1);
    
    http_parser_init(&p2);
    const char *raw2 = "GET /second HTTP/1.1\r\nContent-Length: 5\r\n\r\nhello";
    int data2 = s_strdup_c(raw2);
    http_parse(&p2, data2);
    
    assert(strcmp(m_str(p1.uri), "/first") == 0);
    assert(strcmp(m_str(p2.uri), "/second") == 0);
    assert(strcmp(m_str(p1.method), "GET") == 0);
    assert(strcmp(m_str(p2.method), "GET") == 0);
    
    m_free(data1);
    m_free(data2);
    
    m_free(p1.method);
    m_free(p2.method);
    m_free(p1.uri);
    m_free(p2.uri);
    m_free(p1.version);
    m_free(p2.version);
    m_free(p1.body);
    m_free(p2.body);
    m_table_free(p1.headers);
    m_table_free(p2.headers);
    
    printf("Memory safety test passed.\n");
}

void test_incremental_parsing() {
    printf("Testing incremental parsing (streaming)...\n");
    http_parser_t p;
    http_parser_init(&p);

    const char *part1 = "GET /api/data HTTP/1.1\r\n";
    const char *part2 = "Host: localhost\r\n";
    const char *part3 = "Content-Length: 5\r\n\r\n";
    const char *part4 = "hello";
    
    int data1 = s_strdup_c(part1);
    int data2 = s_strdup_c(part2);
    int data3 = s_strdup_c(part3);
    int data4 = s_strdup_c(part4);
    
    int combined = m_alloc(0, 1, MFREE);
    m_write(combined, 0, m_buf(data1), m_len(data1));
    m_write(combined, m_len(combined), m_buf(data2), m_len(data2));
    m_write(combined, m_len(combined), m_buf(data3), m_len(data3));
    
    int res = http_parse(&p, combined);
    assert(res == 0);
    assert(p.state == HTTP_STATE_BODY || p.state == HTTP_STATE_DONE);
    
    if (p.state == HTTP_STATE_BODY) {
        m_write(combined, m_len(combined), m_buf(data4), m_len(data4));
        res = http_parse(&p, combined);
        assert(res == 0);
        assert(p.state == HTTP_STATE_DONE);
        assert(m_len(p.body) == 5);
    }
    
    m_free(data1);
    m_free(data2);
    m_free(data3);
    m_free(data4);
    m_free(combined);
    m_free(p.method);
    m_free(p.uri);
    m_free(p.version);
    m_free(p.body);
    m_table_free(p.headers);
    
    printf("Incremental parsing test passed.\n");
}

void test_cl_te_conflict() {
    printf("Testing Content-Length/Transfer-Encoding conflict...\n");
    http_parser_t p;
    http_parser_init(&p);

    const char *raw = "POST /upload HTTP/1.1\r\n"
                      "Host: localhost\r\n"
                      "Content-Length: 10\r\n"
                      "Transfer-Encoding: chunked\r\n"
                      "\r\n"
                      "5\r\nhello\r\n0\r\n\r\n";
    int data = s_strdup_c(raw);
    
    int res = http_parse(&p, data);
    assert(res == -1);
    assert(p.state == HTTP_STATE_ERROR);
    assert(p.error == HTTP_ERROR_CL_TE_CONFLICT);

    m_free(data);
    http_parser_free(&p);
    printf("CL/TE conflict test passed.\n");
}

void test_header_injection() {
    printf("Testing header value validation...\n");
    
    int valid_val = s_strdup_c("normal value");
    int invalid_val = s_strdup_c("bad\x0D\x0Avalue");
    
    assert(http_validate_header_value(valid_val) == 1);
    assert(http_validate_header_value(invalid_val) == 0);
    
    m_free(valid_val);
    m_free(invalid_val);
    printf("Header value validation test passed.\n");
}

void test_response_parsing() {
    printf("Testing HTTP response parsing...\n");
    http_parser_t p;
    http_parser_init_response(&p);

    const char *raw = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: 13\r\n\r\nHello, World!";
    int data = s_strdup_c(raw);
    
    int res = http_parse(&p, data);
    assert(res == 0);
    assert(p.state == HTTP_STATE_DONE);
    assert(strcmp(m_str(p.version), "HTTP/1.1") == 0);
    assert(p.status_code == 200);
    
    int ct = m_table_get_cstr(p.headers, "content-type");
    assert(ct > 0);
    assert(strstr(m_str(ct), "text/html") != NULL);

    m_free(data);
    http_parser_free(&p);
    printf("Response parsing test passed.\n");
}

void test_http_parse_bytes() {
    printf("Testing http_parse_bytes for pipelining...\n");
    http_parser_t p;
    http_parser_init(&p);

    const char *raw = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
    int data = s_strdup_c(raw);
    
    int bytes = http_parse_bytes(&p, data);
    assert(bytes == m_len(data));

    m_free(data);
    http_parser_free(&p);
    printf("http_parse_bytes test passed.\n");
}

void test_error_string() {
    printf("Testing error string functions...\n");
    assert(strcmp(http_error_string(HTTP_ERROR_NONE), "None") == 0);
    assert(strcmp(http_error_string(HTTP_ERROR_CL_TE_CONFLICT), "Content-Length/Transfer-Encoding conflict") == 0);
    assert(strcmp(http_state_string(HTTP_STATE_DONE), "Done") == 0);
    assert(strcmp(http_state_string(HTTP_STATE_BODY), "Body") == 0);
    printf("Error string test passed.\n");
}

int main() {
    trace_level = 1;
    m_init();
    conststr_init();

    test_empty_keys();
    test_malformed_headers();
    test_max_headers_limit();
    test_max_header_line_limit();
    test_chunked_encoding();
    test_memory_safety();
    test_incremental_parsing();
    test_cl_te_conflict();
    test_header_injection();
    test_response_parsing();
    test_http_parse_bytes();
    test_error_string();

    conststr_free();
    m_destruct();
    printf("All improved HTTP tests completed successfully.\n");
    return 0;
}
