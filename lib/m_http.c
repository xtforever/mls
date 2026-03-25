#include "m_http.h"
#include "m_tool.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define CRLF_LEN 2

void http_parser_init(http_parser_t *p) {
    memset(p, 0, sizeof(http_parser_t));
    p->state = HTTP_STATE_REQUEST_LINE;
    p->headers = m_table_create();
    p->body = m_alloc(0, 1, MFREE);
    p->max_header_line = 8192;
    p->max_body_size = 10 * 1024 * 1024;
    p->max_headers = 100;
}

void http_parser_free(http_parser_t *p) {
    if (p->method > 0) m_free(p->method);
    if (p->uri > 0) m_free(p->uri);
    if (p->version > 0) m_free(p->version);
    if (p->headers > 0) m_table_free(p->headers);
    if (p->body > 0) m_free(p->body);
}

static int find_crlf(int data_h, int start) {
    int len = m_len(data_h);
    for (int i = start; i < len - 1; i++) {
        if (CHAR(data_h, i) == '\r' && CHAR(data_h, i+1) == '\n') {
            return i;
        }
    }
    return -1;
}

static int parse_hex(int data_h, int start, int max_chars, int *out) {
    int result = 0;
    int pos = start;
    int len = m_len(data_h);
    int has_digit = 0;

    while (pos < len && pos < start + max_chars) {
        char c = CHAR(data_h, pos);
        if (c == '\r' || c == '\n') break;

        int nibble = -1;
        if (c >= '0' && c <= '9') nibble = c - '0';
        else if (c >= 'a' && c <= 'f') nibble = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') nibble = c - 'A' + 10;
        else if (c == ';' || c == ' ') { pos++; continue; }
        else return -1;

        result = result * 16 + nibble;
        has_digit = 1;
        pos++;
    }

    if (!has_digit) return -1;
    *out = result;
    return pos;
}

static int parse_header(int data_h, int start, int end, int *key_h, int *val_h) {
    int line_h = m_slice(0, 0, data_h, start, end);
    char *colon = strchr(m_str(line_h), ':');
    if (!colon) {
        m_free(line_h);
        return -1;
    }

    int key_len = colon - m_str(line_h);
    if (key_len <= 0) {
        m_free(line_h);
        return -1;
    }

    *key_h = s_slice(0, 0, line_h, 0, key_len - 1);
    s_lower(*key_h);
    s_trim(*key_h);

    *val_h = s_slice(0, 0, line_h, key_len + 1, -1);
    s_trim(*val_h);

    m_free(line_h);
    return 0;
}

static int parse_request(int data_h, int start, int end,
                       int *method, int *uri, int *version) {
    int line_h = m_slice(0, 0, data_h, start, end);
    int parts = m_alloc(0, sizeof(char*), MFREE_STR);
    m_str_split(parts, m_str(line_h), " ", 1);

    int ok = 0;
    if (m_len(parts) >= 3) {
        *method = s_strdup_c(STR(parts, 0));
        *uri = s_strdup_c(STR(parts, 1));
        *version = s_strdup_c(STR(parts, 2));
        ok = 1;
    }

    m_free(line_h);
    m_free(parts);
    return ok;
}

int http_parse(http_parser_t *p, int data_h) {
    int pos = 0;
    int data_len = m_len(data_h);

    while (pos < data_len && p->state != HTTP_STATE_DONE && p->state != HTTP_STATE_ERROR) {

        if (p->state == HTTP_STATE_REQUEST_LINE) {
            int crlf = find_crlf(data_h, pos);
            if (crlf == -1) break;

            if (crlf - pos > p->max_header_line) {
                p->state = HTTP_STATE_ERROR;
                return -1;
            }

            if (!parse_request(data_h, pos, crlf - 1, &p->method, &p->uri, &p->version)) {
                p->state = HTTP_STATE_ERROR;
                return -1;
            }
            p->state = HTTP_STATE_HEADERS;
            pos = crlf + CRLF_LEN;

        } else if (p->state == HTTP_STATE_HEADERS) {
            int crlf = find_crlf(data_h, pos);
            if (crlf == -1) break;

            int line_len = crlf - pos;
            if (line_len > p->max_header_line) {
                p->state = HTTP_STATE_ERROR;
                return -1;
            }

            if (line_len == 0) {
                int te = m_table_get_cstr(p->headers, "transfer-encoding");
                if (te && strstr(m_str(te), "chunked")) {
                    p->is_chunked = 1;
                    p->state = HTTP_STATE_BODY;
                } else {
                    int cl = m_table_get_cstr(p->headers, "content-length");
                    if (cl) {
                        p->content_length = atoi(m_str(cl));
                        if (p->content_length > p->max_body_size) {
                            p->state = HTTP_STATE_ERROR;
                            return -1;
                        }
                        p->state = (p->content_length > 0) ? HTTP_STATE_BODY : HTTP_STATE_DONE;
                    } else {
                        p->state = HTTP_STATE_DONE;
                    }
                }
                pos = crlf + CRLF_LEN;
            } else {
                int key_h, val_h;
                if (parse_header(data_h, pos, crlf - 1, &key_h, &val_h) == 0) {
                    p->header_count++;
                    if (p->header_count > p->max_headers) {
                        m_free(key_h);
                        m_free(val_h);
                        p->state = HTTP_STATE_ERROR;
                        return -1;
                    }
                    m_table_set_handle_by_str(p->headers, key_h, val_h, MLS_TABLE_TYPE_STRING);
                }
                pos = crlf + CRLF_LEN;
            }

        } else if (p->state == HTTP_STATE_BODY) {
            if (!p->is_chunked) {
                size_t remaining = p->content_length - m_len(p->body);
                size_t available = data_len - pos;
                size_t to_copy = (available < remaining) ? available : remaining;

                m_write(p->body, m_len(p->body), m_buf(data_h) + pos, to_copy);
                pos += to_copy;

                if (m_len(p->body) == p->content_length) {
                    p->state = HTTP_STATE_DONE;
                }
            } else {
                int chunk_size = 0;
                int hex_end = parse_hex(data_h, pos, 16, &chunk_size);
                if (hex_end == -1) break;

                int crlf = find_crlf(data_h, hex_end);
                if (crlf == -1) break;

                pos = crlf + CRLF_LEN;

                if (chunk_size == 0) {
                    p->state = HTTP_STATE_DONE;
                } else {
                    if (m_len(p->body) + chunk_size > p->max_body_size) {
                        p->state = HTTP_STATE_ERROR;
                        return -1;
                    }

                    size_t available = data_len - pos;
                    size_t to_copy = (available < (size_t)chunk_size) ? available : (size_t)chunk_size;

                    m_write(p->body, m_len(p->body), m_buf(data_h) + pos, to_copy);
                    pos += to_copy;

                    if (to_copy == (size_t)chunk_size) {
                        int trail_crlf = find_crlf(data_h, pos);
                        if (trail_crlf != -1) {
                            pos = trail_crlf + CRLF_LEN;
                        }
                    }
                }
            }
        }
    }

    return (p->state == HTTP_STATE_ERROR) ? -1 : 0;
}
