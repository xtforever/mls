#ifndef M_HTTP_H
#define M_HTTP_H

#include "m_table.h"
#include "mls.h"

typedef enum { HTTP_TYPE_REQUEST, HTTP_TYPE_RESPONSE } http_type_t;

typedef enum {
	HTTP_STATE_REQUEST_LINE,
	HTTP_STATE_RESPONSE_LINE,
	HTTP_STATE_HEADERS,
	HTTP_STATE_BODY,
	HTTP_STATE_TRAILER,
	HTTP_STATE_DONE,
	HTTP_STATE_ERROR
} http_state_t;

typedef enum {
	HTTP_ERROR_NONE = 0,
	HTTP_ERROR_INVALID_METHOD,
	HTTP_ERROR_INVALID_URI,
	HTTP_ERROR_INVALID_VERSION,
	HTTP_ERROR_HEADER_OVERFLOW,
	HTTP_ERROR_HEADER_COUNT,
	HTTP_ERROR_BODY_SIZE,
	HTTP_ERROR_CL_TE_CONFLICT,
	HTTP_ERROR_HEADER_INJECTION,
	HTTP_ERROR_INVALID_STATUS,
	HTTP_ERROR_PARSING
} http_error_t;

typedef struct {
	http_state_t state;
	http_type_t type;
	http_error_t error;
	int method;
	int uri;
	int version;
	int status_code;
	int headers;
	int body;
	size_t content_length;
	int is_chunked;
	int header_count;
	int has_transfer_encoding;
	int has_content_length;
	int is_keepalive;
	size_t max_header_line;
	size_t max_body_size;
	size_t max_uri_length;
	int max_headers;
} http_parser_t;

void http_parser_init (http_parser_t *p);
void http_parser_init_request (http_parser_t *p);
void http_parser_init_response (http_parser_t *p);
void http_parser_free (http_parser_t *p);

int http_parse (http_parser_t *p, int data_h);
int http_parse_bytes (http_parser_t *p, int data_h);

const char *http_error_string (http_error_t error);
const char *http_state_string (http_state_t state);

int http_validate_uri (int uri_h);
int http_validate_header_value (int val_h);

#endif
