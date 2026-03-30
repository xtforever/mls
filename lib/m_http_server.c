#include "m_http_server.h"
#include "m_http.h"
#include "m_tool.h"
#include "m_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

static int s_copy_cstr(const char *s) {
    if (!s) return 0;
    int h = m_alloc(0, 1, MFREE);
    s_app1(h, (char*)s);
    return h;
}

http_server_config_t http_server_config_load(int h) {
    http_server_config_t conf;
    memset(&conf, 0, sizeof(conf));
    
    int server_node = hdf_find_node(h, "server");
    if (server_node <= 0) server_node = h;

    conf.port = hdf_get_int(server_node, "port", 8080);
    conf.host = s_copy_cstr(hdf_get_property(server_node, "host"));
    if (!conf.host) conf.host = s_copy_cstr("0.0.0.0");
    conf.root_dir = s_copy_cstr(hdf_get_property(server_node, "root"));
    
    conf.routes = m_table_create();
    
    int children = hdf_get_children(server_node);
    if (children > 0) {
        for (int i = 0; i < m_len(children); i++) {
            int child = INT(children, i);
            if (hdf_get_type(child) == HDF_TYPE_LIST) {
                int sc = hdf_get_children(child);
                if (m_len(sc) > 0 && strcmp(hdf_get_value(INT(sc, 0)), "route") == 0) {
                    const char *path = hdf_get_property(child, "path");
                    if (path) {
                        // Create route handle: [type, target_handle]
                        int route_h = m_alloc(2, sizeof(int), MFREE_EACH);
                        int type = ROUTE_TYPE_ECHO;
                        int target = 0;

                        const char *f = hdf_get_property(child, "file");
                        const char *j = hdf_get_property(child, "json");
                        const char *t = hdf_get_property(child, "text");
                        if (f) { type = ROUTE_TYPE_FILE; target = s_copy_cstr(f); }
                        else if (j) { type = ROUTE_TYPE_JSON; target = s_copy_cstr(j); }
                        else if (t) { type = ROUTE_TYPE_TEXT; target = s_copy_cstr(t); }
                        else if (hdf_get_property(child, "echo")) { type = ROUTE_TYPE_ECHO; }
                        
                        m_puti(route_h, type);
                        m_puti(route_h, target);
                        
                        m_table_set_handle_by_str(conf.routes, s_copy_cstr(path), route_h, MLS_TABLE_TYPE_LIST);
                    }
                }
            }
        }
    }
    
    return conf;
}

void http_server_config_free(http_server_config_t *conf) {
    if (conf->host > 0) m_free(conf->host);
    if (conf->root_dir > 0) m_free(conf->root_dir);
    if (conf->routes > 0) m_table_free(conf->routes);
}

static void send_response(int client_fd, int status, const char *status_text, const char *content_type, const char *body, size_t body_len) {
    int resp_h = m_alloc(0, 1, MFREE);
    s_printf(resp_h, 0, "HTTP/1.1 %d %s\r\n"
                               "Content-Type: %s\r\n"
                               "Content-Length: %zu\r\n"
                               "Connection: close\r\n\r\n",
                               status, status_text, content_type, body_len);
    if (body && body_len > 0) {
        m_write(resp_h, m_len(resp_h), body, body_len);
    }
    write(client_fd, m_buf(resp_h), m_len(resp_h));
    m_free(resp_h);
}

static void serve_file(int client_fd, const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        send_response(client_fd, 404, "Not Found", "text/plain", "Not Found", 9);
        return;
    }
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *buf = malloc(fsize);
    fread(buf, 1, fsize, fp);
    fclose(fp);
    send_response(client_fd, 200, "OK", "text/plain", buf, fsize);
    free(buf);
}

static void handle_request(int client_fd, http_server_config_t *conf) {
    char buffer[4096];
    int data_h = m_alloc(0, 1, MFREE);
    http_parser_t p;
    http_parser_init_request(&p);

    while (1) {
        ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer));
        if (bytes_read <= 0) break;
        m_write(data_h, m_len(data_h), buffer, bytes_read);
        if (http_parse(&p, data_h) == -1 || p.state == HTTP_STATE_DONE) break;
    }

    if (p.state == HTTP_STATE_DONE) {
        const char *uri = m_str(p.uri);
        int route_h = m_table_get_cstr(conf->routes, uri);
        if (route_h > 0) {
            int type = INT(route_h, 0);
            int target = INT(route_h, 1);
            switch (type) {
                case ROUTE_TYPE_FILE: serve_file(client_fd, m_str(target)); break;
                case ROUTE_TYPE_JSON: send_response(client_fd, 200, "OK", "application/json", m_str(target), m_len(target)); break;
                case ROUTE_TYPE_TEXT: send_response(client_fd, 200, "OK", "text/plain", m_str(target), m_len(target)); break;
                case ROUTE_TYPE_ECHO: send_response(client_fd, 200, "OK", "text/plain", m_str(p.body), m_len(p.body)); break;
            }
        } else if (conf->root_dir > 0) {
            int full_path = m_alloc(0, 1, MFREE);
            s_app(full_path, m_str(conf->root_dir), uri, NULL);
            serve_file(client_fd, m_str(full_path));
            m_free(full_path);
        } else {
            send_response(client_fd, 404, "Not Found", "text/plain", "Not Found", 9);
        }
    }

    m_free(data_h);
    http_parser_free(&p);
    close(client_fd);
}

void http_server_run(http_server_config_t *conf) {
    int server_fd, client_fd;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) return;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(m_str(conf->host));
    address.sin_port = htons(conf->port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) return;
    if (listen(server_fd, 10) < 0) return;

    printf("HDF Configured Server listening on %s:%d\n", m_str(conf->host), conf->port);

    while (1) {
        if ((client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            if (errno == EINTR) continue;
            break;
        }
        handle_request(client_fd, conf);
    }
    close(server_fd);
}
