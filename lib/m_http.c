#include "m_http.h"
#include "m_tool.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

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

static int parse_hex_chunk_size(int data_h, int start, int *out_size) {
    int pos = start;
    int len = m_len(data_h);
    int result = 0;
    int has_digit = 0;
    
    while (pos < len && pos < start + 16) {
        char c = CHAR(data_h, pos);
        if (c == '\r') break;
        if (c >= '0' && c <= '9') {
            result = result * 16 + (c - '0');
            has_digit = 1;
        } else if (c >= 'a' && c <= 'f') {
            result = result * 16 + (c - 'a' + 10);
            has_digit = 1;
        } else if (c >= 'A' && c <= 'F') {
            result = result * 16 + (c - 'A' + 10);
            has_digit = 1;
        } else if (c == ';' || c == ' ') {
        } else {
            return -1;
        }
        pos++;
    }
    
    if (!has_digit) return -1;
    *out_size = result;
    return pos;
}

static void normalize_header_key(int key_h) {
    s_lower(key_h);
    s_trim(key_h);
}

int http_parse(http_parser_t *p, int data_h) {
    int pos = 0;
    int data_len = m_len(data_h);

    while (pos < data_len && p->state != HTTP_STATE_DONE && p->state != HTTP_STATE_ERROR) {
        if (p->state == HTTP_STATE_REQUEST_LINE || p->state == HTTP_STATE_HEADERS) {
            int next_crlf = find_crlf(data_h, pos);
            if (next_crlf == -1) break; // Need more data

            int line_len = next_crlf - pos;
            if (line_len > p->max_header_line) {
                p->state = HTTP_STATE_ERROR;
                return -1;
            }

            if (p->state == HTTP_STATE_REQUEST_LINE) {
                // Simplified request line parsing: METHOD URI VERSION
                int line_h = m_slice(0, 0, data_h, pos, next_crlf - 1);
                
                // Use a simple split
                int parts = m_alloc(0, sizeof(char *), MFREE_STR);
                m_str_split(parts, m_str(line_h), " ", 1);
                
                if (m_len(parts) >= 3) {
                    p->method = s_strdup_c(STR(parts, 0));
                    p->uri = s_strdup_c(STR(parts, 1));
                    p->version = s_strdup_c(STR(parts, 2));
                    p->state = HTTP_STATE_HEADERS;
                } else {
                    p->state = HTTP_STATE_ERROR;
                }
                m_free(line_h);
                m_free(parts);
            } else if (p->state == HTTP_STATE_HEADERS) {
                if (line_len == 0) {
                    // End of headers
                    // Check for Body presence
                    int te = m_table_get_cstr(p->headers, "transfer-encoding");
                    if (te && strstr(m_str(te), "chunked")) {
                        p->is_chunked = 1;
                        p->state = HTTP_STATE_BODY; // Simplified: don't actually parse chunks in this example
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
                } else {
                    int line_h = m_slice(0, 0, data_h, pos, next_crlf - 1);
                    char *colon = strchr(m_str(line_h), ':');
                    if (colon) {
                        int key_len = colon - m_str(line_h);
                        if (key_len > 0) {
                            p->header_count++;
                            if (p->header_count > p->max_headers) {
                                p->state = HTTP_STATE_ERROR;
                                m_free(line_h);
                                return -1;
                            }
                            
                            int key_h = s_slice(0, 0, line_h, 0, key_len - 1);
                            normalize_header_key(key_h);
                            
                            int val_h = s_slice(0, 0, line_h, key_len + 1, -1);
                            s_trim(val_h);
                            
                            m_table_set_handle_by_str(p->headers, key_h, val_h, MLS_TABLE_TYPE_STRING);
                        }
                    }
                    m_free(line_h);
                }
            }
            pos = next_crlf + 2;
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
                int hex_end = parse_hex_chunk_size(data_h, pos, &chunk_size);
                if (hex_end == -1) break;
                
                int crlf_pos = -1;
                for (int i = hex_end; i < data_len && i < hex_end + 4; i++) {
                    if (i + 1 < data_len && CHAR(data_h, i) == '\r' && CHAR(data_h, i+1) == '\n') {
                        crlf_pos = i;
                        break;
                    }
                }
                if (crlf_pos == -1) break;
                
                pos = crlf_pos + 2;
                
                if (chunk_size == 0) {
                    p->state = HTTP_STATE_DONE;
                } else {
                    size_t body_space = m_len(p->body);
                    size_t needed = body_space + chunk_size;
                    if (needed > p->max_body_size) {
                        p->state = HTTP_STATE_ERROR;
                        return -1;
                    }
                    
                    size_t available = data_len - pos;
                    size_t to_copy = (available < (size_t)chunk_size) ? available : (size_t)chunk_size;
                    m_write(p->body, m_len(p->body), m_buf(data_h) + pos, to_copy);
                    pos += to_copy;
                    
                    if (to_copy == (size_t)chunk_size) {
                        if (pos + 2 <= data_len && CHAR(data_h, pos) == '\r' && CHAR(data_h, pos+1) == '\n') {
                            pos += 2;
                        } else {
                            int trail_crlf = find_crlf(data_h, pos);
                            if (trail_crlf == -1) break;
                            pos = trail_crlf + 2;
                        }
                    }
                }
            }
        }
    }
    return (p->state == HTTP_STATE_ERROR) ? -1 : 0;
}
