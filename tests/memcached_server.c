#include "m_http.h"
#include "m_table.h"
#include "m_tool.h"
#include "m_extra.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

static int kv_store = 0;
static int lru_list = 0;
static size_t current_memory = 0;
static size_t peak_memory = 0;
#define MAX_MEMORY (1024 * 1024) // 1MB limit for testing

static void send_response(int fd, int status, const char *status_text, const char *body, size_t body_len) {
    char header[1024];
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n\r\n",
        status, status_text, body_len);
    
    if (write(fd, header, header_len) < 0) return;
    if (body && body_len > 0) {
        if (write(fd, body, body_len) < 0) return;
    }
}

static void memory_check_evict() {
    while (current_memory > MAX_MEMORY && m_len(lru_list) > 0) {
        int key_h = *(int *)mls(lru_list, 0);
        m_del(lru_list, 0);

        if (m_is_freed(key_h)) continue;

        int value_h = m_table_get_str(kv_store, key_h);
        if (value_h > 0) {
            current_memory -= m_len(value_h);
            m_table_remove_by_str(kv_store, key_h);
        }
        m_free(key_h);
    }
}

static void update_lru(int key_h) {
    if (key_h <= 0 || m_is_freed(key_h)) return;
    const char *ks = m_str(key_h);

    for (int i = 0; i < m_len(lru_list); i++) {
        int existing = *(int *)mls(lru_list, i);
        if (!m_is_freed(existing) && strcmp(m_str(existing), ks) == 0) {
            m_del(lru_list, i);
            m_free(existing);
            break;
        }
    }
    int dup = s_dup(ks);
    m_put(lru_list, &dup);
}

static void handle_set(int fd, http_parser_t *p) {
    int key_header_h = m_table_get_cstr(p->headers, "x-key");
    if (key_header_h <= 0) {
        send_response(fd, 400, "Bad Request", "Missing X-Key header", 20);
        return;
    }

    m_putc(p->body, 0);
    m_setlen(p->body, m_len(p->body) - 1);

    int value = m_dub(p->body);
    int body_tmp = s_dup(m_str(p->body));
    if (s_has_prefix(body_tmp, "base64:")) {
        int b64_part = s_sub(body_tmp, 7, -1);
        int decoded = s_base64_decode(b64_part);
        m_free(value); m_free(b64_part);
        value = decoded;
    } else {
        int b64_h = m_table_get_cstr(p->headers, "x-base64");
        if (b64_h > 0 && strcasecmp(m_str(b64_h), "true") == 0) {
            int decoded = s_base64_decode(body_tmp);
            m_free(value);
            value = decoded;
        }
    }
    m_free(body_tmp);

    int old_val = m_table_get_str(kv_store, key_header_h);
    if (old_val > 0) current_memory -= m_len(old_val);
    current_memory += m_len(value);
    if (current_memory > peak_memory) peak_memory = current_memory;

    m_table_set_handle_by_str(kv_store, s_dup(m_str(key_header_h)), value, MLS_TABLE_TYPE_STRING);
    update_lru(key_header_h);
    memory_check_evict();
    send_response(fd, 200, "OK", "Stored", 6);
}

static void handle_get(int fd, http_parser_t *p) {
    int key_header_h = m_table_get_cstr(p->headers, "x-key");
    if (key_header_h <= 0) {
        send_response(fd, 400, "Bad Request", "Missing X-Key header", 20);
        return;
    }

    int value = m_table_get_str(kv_store, key_header_h);
    if (value <= 0) {
        send_response(fd, 404, "Not Found", "Key not found", 13);
        return;
    }

    update_lru(key_header_h);
    send_response(fd, 200, "OK", m_str(value), m_len(value));
}

static void handle_client(int fd) {
    struct timeval tv;
    tv.tv_sec = 1; tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    http_parser_t p;
    http_parser_init_request(&p);
    int data_h = m_alloc(0, 1, MFREE);
    char buf[4096];
    
    while (1) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n <= 0) break;
        m_write(data_h, m_len(data_h), buf, n);
        if (http_parse(&p, data_h) == -1) break;
        if (p.state == HTTP_STATE_DONE) break;
    }

    if (p.state == HTTP_STATE_DONE) {
        const char *uri = m_str(p.uri);
        if (strcmp(uri, "/set") == 0) handle_set(fd, &p);
        else if (strcmp(uri, "/get") == 0) handle_get(fd, &p);
        else if (strcmp(uri, "/stats") == 0) {
            char stats[256];
            snprintf(stats, sizeof(stats), "Keys: %d, Mem: %zu\n", m_len(kv_store), current_memory);
            send_response(fd, 200, "OK", stats, strlen(stats));
        } else if (strcmp(uri, "/memory") == 0) {
            char mem[512];
            snprintf(mem, sizeof(mem), "Cur:%zu, Peak:%zu, Lim:%zu, LRU:%d\n", 
                     current_memory, peak_memory, (size_t)MAX_MEMORY, m_len(lru_list));
            send_response(fd, 200, "OK", mem, strlen(mem));
        } else send_response(fd, 404, "Not Found", "Err", 3);
    } else send_response(fd, 400, "Bad Request", "Err", 3);

    m_free(data_h);
    http_parser_free(&p);
    close(fd);
}

int main(int argc, char **argv) {
    int port = 8081;
    if (argc > 1) port = atoi(argv[1]);

    m_init(); conststr_init();
    trace_level = 0; 

    kv_store = m_table_create();
    lru_list = m_alloc(0, sizeof(int), MFREE);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) perror("socket"), exit(1);

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); exit(1);
    }

    if (listen(server_fd, 5) < 0) {
        perror("listen"); exit(1);
    }

    printf("Memcached server (LRU + jemalloc) on %d\n", port);
    
    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            break;
        }
        handle_client(client_fd);
    }

    m_table_free(kv_store); m_free(lru_list);
    conststr_free(); return 0;
}
