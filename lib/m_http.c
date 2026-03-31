#include "m_http.h"
#include "m_tool.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define CRLF_LEN 2

static const char *error_strings[] = {
	"None",
	"Invalid method",
	"Invalid URI",
	"Invalid version",
	"Header line overflow",
	"Too many headers",
	"Body size exceeded",
	"Content-Length/Transfer-Encoding conflict",
	"Header injection detected",
	"Invalid status code",
	"Parsing error"};

static const char *state_strings[] = {
	"Request line", "Response line", "Headers", "Body",
	"Trailer",	"Done",		 "Error"};

const char *http_error_string (http_error_t error)
{
	if (error >= 0 && error <= HTTP_ERROR_PARSING) {
		return error_strings[error];
	}
	return "Unknown error";
}

const char *http_state_string (http_state_t state)
{
	if (state >= 0 && state <= HTTP_STATE_ERROR) {
		return state_strings[state];
	}
	return "Unknown state";
}

void http_parser_init (http_parser_t *p)
{
	memset (p, 0, sizeof (http_parser_t));
	p->state = HTTP_STATE_REQUEST_LINE;
	p->type = HTTP_TYPE_REQUEST;
	p->headers = m_table_create ();
	p->body = m_alloc (0, 1, MFREE);
	p->max_header_line = 8192;
	p->max_body_size = 10 * 1024 * 1024;
	p->max_uri_length = 4096;
	p->max_headers = 100;
}

void http_parser_init_request (http_parser_t *p)
{
	http_parser_init (p);
	p->type = HTTP_TYPE_REQUEST;
	p->state = HTTP_STATE_REQUEST_LINE;
}

void http_parser_init_response (http_parser_t *p)
{
	http_parser_init (p);
	p->type = HTTP_TYPE_RESPONSE;
	p->state = HTTP_STATE_RESPONSE_LINE;
}

void http_parser_free (http_parser_t *p)
{
	if (p->method > 0)
		m_free (p->method);
	if (p->uri > 0)
		m_free (p->uri);
	if (p->version > 0)
		m_free (p->version);
	if (p->headers > 0)
		m_table_free (p->headers);
	if (p->body > 0)
		m_free (p->body);
}

int http_validate_uri (int uri_h)
{
	if (uri_h <= 0)
		return 0;
	const char *uri = m_str (uri_h);
	size_t len = m_len (uri_h);

	if (len == 0)
		return 0;
	if (len > 4096)
		return 0;

	for (size_t i = 0; i < len; i++) {
		unsigned char c = (unsigned char)uri[i];
		if (c < 32 && c != 0)
			return 0;
		if (c == 127)
			return 0;
	}
	return 1;
}

int http_validate_header_value (int val_h)
{
	if (val_h <= 0)
		return 0;
	const char *val = m_str (val_h);
	size_t len = m_len (val_h);

	for (size_t i = 0; i < len; i++) {
		unsigned char c = (unsigned char)val[i];
		if (c == '\r' || c == '\n')
			return 0;
	}
	return 1;
}

static int find_crlf (int data_h, int start)
{
	int len = m_len (data_h);
	for (int i = start; i < len - 1; i++) {
		if (CHAR (data_h, i) == '\r' && CHAR (data_h, i + 1) == '\n') {
			return i;
		}
	}
	return -1;
}

static int parse_hex (int data_h, int start, int max_chars, int *out)
{
	int result = 0;
	int pos = start;
	int len = m_len (data_h);
	int has_digit = 0;

	while (pos < len && pos < start + max_chars) {
		char c = CHAR (data_h, pos);
		if (c == '\r' || c == '\n')
			break;

		int nibble = -1;
		if (c >= '0' && c <= '9')
			nibble = c - '0';
		else if (c >= 'a' && c <= 'f')
			nibble = c - 'a' + 10;
		else if (c >= 'A' && c <= 'F')
			nibble = c - 'A' + 10;
		else if (c == ';' || c == ' ') {
			pos++;
			continue;
		} else
			return -1;

		result = result * 16 + nibble;
		has_digit = 1;
		pos++;
	}

	if (!has_digit)
		return -1;
	*out = result;
	return pos;
}

static int parse_header (int data_h, int start, int end, int *key_h, int *val_h)
{
	int line_h = m_slice (0, 0, data_h, start, end);
	char *colon = strchr (m_str (line_h), ':');
	if (!colon) {
		m_free (line_h);
		return -1;
	}

	int key_len = colon - m_str (line_h);
	if (key_len <= 0) {
		m_free (line_h);
		return -1;
	}

	*key_h = s_slice (0, 0, line_h, 0, key_len - 1);
	s_lower (*key_h);
	s_trim (*key_h);

	*val_h = s_slice (0, 0, line_h, key_len + 1, -1);
	s_trim (*val_h);

	m_free (line_h);
	return 0;
}

static int parse_request (int data_h, int start, int end, int *method, int *uri,
			  int *version)
{
	int line_h = m_slice (0, 0, data_h, start, end);
	int parts = m_alloc (0, sizeof (char *), MFREE_STR);
	m_str_split (parts, m_str (line_h), " ", 1);

	int ok = 0;
	if (m_len (parts) >= 3) {
		*method = s_strdup_c (STR (parts, 0));
		*uri = s_strdup_c (STR (parts, 1));
		*version = s_strdup_c (STR (parts, 2));
		ok = 1;
	}

	m_free (line_h);
	m_free (parts);
	return ok;
}

static int parse_response (int data_h, int start, int end, int *version,
			   int *status_code, int *reason)
{
	int line_h = m_slice (0, 0, data_h, start, end);
	int parts = m_alloc (0, sizeof (char *), MFREE_STR);
	m_str_split (parts, m_str (line_h), " ", 1);

	int ok = 0;
	if (m_len (parts) >= 2) {
		*version = s_strdup_c (STR (parts, 0));
		int code_h = s_strdup_c (STR (parts, 1));
		*status_code = atoi (m_str (code_h));
		m_free (code_h);

		if (m_len (parts) >= 3) {
			*reason = s_strdup_c (STR (parts, 2));
		}
		ok = 1;
	}

	m_free (line_h);
	m_free (parts);
	return ok;
}

static void set_error (http_parser_t *p, http_error_t error)
{
	p->state = HTTP_STATE_ERROR;
	p->error = error;
}

static void prepare_body_state (http_parser_t *p)
{
	int te = m_table_get_cstr (p->headers, "transfer-encoding");
	int cl = m_table_get_cstr (p->headers, "content-length");

	if (te) {
		p->has_transfer_encoding = 1;
		if (strstr (m_str (te), "chunked")) {
			p->is_chunked = 1;
		}
	}

	if (cl) {
		p->has_content_length = 1;
		if (!p->is_chunked) {
			p->content_length = atoi (m_str (cl));
		}
	}

	if (p->has_transfer_encoding && p->has_content_length) {
		set_error (p, HTTP_ERROR_CL_TE_CONFLICT);
		return;
	}

	if (p->is_chunked) {
		p->state = HTTP_STATE_BODY;
		return;
	}

	if (p->has_content_length) {
		if (p->content_length > p->max_body_size) {
			set_error (p, HTTP_ERROR_BODY_SIZE);
			return;
		}
		p->state = (p->content_length > 0) ? HTTP_STATE_BODY
						   : HTTP_STATE_DONE;
	} else {
		p->state = HTTP_STATE_DONE;
	}
}

static int parse_body_content (http_parser_t *p, int data_h, int pos,
			       int data_len)
{
	size_t remaining = p->content_length - m_len (p->body);
	size_t available = data_len - pos;
	size_t to_copy = (available < remaining) ? available : remaining;

	m_write (p->body, m_len (p->body), m_buf (data_h) + pos, to_copy);
	pos += to_copy;

	if (m_len (p->body) == p->content_length) {
		p->state = HTTP_STATE_DONE;
	}

	return pos;
}

static int parse_body_chunked (http_parser_t *p, int data_h, int pos,
			       int data_len)
{
	int chunk_size = 0;
	int hex_end = parse_hex (data_h, pos, 16, &chunk_size);
	if (hex_end == -1)
		return pos;

	int crlf = find_crlf (data_h, hex_end);
	if (crlf == -1)
		return pos;

	pos = crlf + CRLF_LEN;

	if (chunk_size == 0) {
		p->state = HTTP_STATE_TRAILER;
		return pos;
	}

	if (m_len (p->body) + chunk_size > p->max_body_size) {
		set_error (p, HTTP_ERROR_BODY_SIZE);
		return -1;
	}

	size_t available = data_len - pos;
	size_t to_copy = (available < (size_t)chunk_size) ? available
							  : (size_t)chunk_size;

	m_write (p->body, m_len (p->body), m_buf (data_h) + pos, to_copy);
	pos += to_copy;

	if (to_copy == (size_t)chunk_size) {
		int trail_crlf = find_crlf (data_h, pos);
		if (trail_crlf != -1) {
			pos = trail_crlf + CRLF_LEN;
		}
	}

	return pos;
}

int http_parse (http_parser_t *p, int data_h)
{
	int pos = 0;
	int data_len = m_len (data_h);

	while (pos < data_len && p->state != HTTP_STATE_DONE &&
	       p->state != HTTP_STATE_ERROR) {

		if (p->state == HTTP_STATE_REQUEST_LINE) {
			int crlf = find_crlf (data_h, pos);
			if (crlf == -1)
				break;

			if (crlf - pos > p->max_header_line) {
				set_error (p, HTTP_ERROR_HEADER_OVERFLOW);
				return -1;
			}

			if (!parse_request (data_h, pos, crlf - 1, &p->method,
					    &p->uri, &p->version)) {
				set_error (p, HTTP_ERROR_INVALID_METHOD);
				return -1;
			}

			if (!http_validate_uri (p->uri)) {
				set_error (p, HTTP_ERROR_INVALID_URI);
				return -1;
			}

			p->state = HTTP_STATE_HEADERS;
			pos = crlf + CRLF_LEN;
		} else if (p->state == HTTP_STATE_RESPONSE_LINE) {
			int crlf = find_crlf (data_h, pos);
			if (crlf == -1)
				break;

			if (crlf - pos > p->max_header_line) {
				set_error (p, HTTP_ERROR_HEADER_OVERFLOW);
				return -1;
			}

			int reason;
			if (!parse_response (data_h, pos, crlf - 1, &p->version,
					     &p->status_code, &reason)) {
				set_error (p, HTTP_ERROR_INVALID_STATUS);
				return -1;
			}
			if (reason > 0)
				m_free (reason);

			p->state = HTTP_STATE_HEADERS;
			pos = crlf + CRLF_LEN;
		} else if (p->state == HTTP_STATE_HEADERS) {
			int crlf = find_crlf (data_h, pos);
			if (crlf == -1)
				break;

			int line_len = crlf - pos;
			if (line_len > p->max_header_line) {
				set_error (p, HTTP_ERROR_HEADER_OVERFLOW);
				return -1;
			}

			if (line_len == 0) {
				prepare_body_state (p);
				pos = crlf + CRLF_LEN;
			} else {
				int key_h, val_h;
				if (parse_header (data_h, pos, crlf - 1, &key_h,
						  &val_h) == 0) {
					if (!http_validate_header_value (
						    val_h)) {
						m_free (key_h);
						m_free (val_h);
						set_error (
							p,
							HTTP_ERROR_HEADER_INJECTION);
						return -1;
					}

					p->header_count++;
					if (p->header_count > p->max_headers) {
						m_free (key_h);
						m_free (val_h);
						set_error (
							p,
							HTTP_ERROR_HEADER_COUNT);
						return -1;
					}
					m_table_set_handle_by_str (
						p->headers, key_h, val_h,
						MLS_TABLE_TYPE_STRING);
				}
				pos = crlf + CRLF_LEN;
			}
		} else if (p->state == HTTP_STATE_BODY) {
			if (p->is_chunked) {
				int new_pos = parse_body_chunked (
					p, data_h, pos, data_len);
				if (new_pos == -1)
					return -1;
				pos = new_pos;
			} else {
				pos = parse_body_content (p, data_h, pos,
							  data_len);
			}
		} else if (p->state == HTTP_STATE_TRAILER) {
			int crlf = find_crlf (data_h, pos);
			if (crlf == -1)
				break;

			if (crlf == pos) {
				p->state = HTTP_STATE_DONE;
			}
			pos = crlf + CRLF_LEN;
		}
	}

	return (p->state == HTTP_STATE_ERROR) ? -1 : 0;
}

int http_parse_bytes (http_parser_t *p, int data_h)
{
	int result = http_parse (p, data_h);
	if (result == 0) {
		return m_len (data_h);
	}
	return result;
}
