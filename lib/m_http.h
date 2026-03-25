#ifndef M_HTTP_H
#define M_HTTP_H

#include "mls.h"
#include "m_table.h"

typedef enum {
    HTTP_STATE_REQUEST_LINE,
    HTTP_STATE_HEADERS,
    HTTP_STATE_BODY,
    HTTP_STATE_DONE,
    HTTP_STATE_ERROR
} http_state_t;

typedef struct {
    http_state_t state;
    int method; // s_cstr
    int uri;    // dynamic string handle
    int version; // dynamic string handle
    int headers; // m_table handle
    int body;    // dynamic string handle
    size_t content_length;
    int is_chunked;
    int header_count; // internal counter for headers parsed
    
    // Limits
    size_t max_header_line;
    size_t max_body_size;
    int max_headers;
} http_parser_t;

void http_parser_init(http_parser_t *p);
void http_parser_free(http_parser_t *p);

/**
 * Parses raw data from an mls handle.
 * Returns 0 on success, -1 on error.
 */
int http_parse(http_parser_t *p, int data_h);

#endif // M_HTTP_H
