#include "../lib/mls.h"
#include "../lib/m_tool.h"
#include "../lib/m_http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#define PORT 19999
#define BUFFER_SIZE 4096

void handle_client(int client_fd) {
    char buffer[BUFFER_SIZE];
    int data_h = m_alloc(0, 1, MFREE);
    http_parser_t p;
    http_parser_init(&p);

    while (1) {
        ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer));
        if (bytes_read <= 0) break;

        m_write(data_h, m_len(data_h), buffer, bytes_read);
        
        if (http_parse(&p, data_h) == -1) {
            const char *error_resp = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
            write(client_fd, error_resp, strlen(error_resp));
            break;
        }

        if (p.state == HTTP_STATE_DONE) {
            const char *uri = m_str(p.uri);
            int resp_h = m_alloc(0, 1, MFREE);
            
            if (strcmp(uri, "/") == 0 || strcmp(uri, "/api") == 0) {
                const char *json = "{\"status\":\"ok\",\"message\":\"Welcome to MLS HTTP Server\"}";
                s_printf(resp_h, 0, "HTTP/1.1 200 OK\r\n"
                                   "Content-Type: application/json\r\n"
                                   "Content-Length: %d\r\n"
                                   "Connection: close\r\n\r\n%s",
                                   (int)strlen(json), json);
            } else {
                // Try to serve a file if it exists (very basic)
                FILE *fp = fopen(uri + 1, "r"); // Skip leading '/'
                if (fp) {
                    fseek(fp, 0, SEEK_END);
                    long fsize = ftell(fp);
                    fseek(fp, 0, SEEK_SET);
                    
                    char *content = malloc(fsize);
                    if (content) {
                        fread(content, 1, fsize, fp);
                        fclose(fp);
                        
                        s_printf(resp_h, 0, "HTTP/1.1 200 OK\r\n"
                                           "Content-Type: text/plain\r\n"
                                           "Content-Length: %ld\r\n"
                                           "Connection: close\r\n\r\n",
                                           fsize);
                        m_write(resp_h, m_len(resp_h), content, fsize);
                        free(content);
                    } else {
                        fclose(fp);
                        const char *err_resp = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                        m_write(resp_h, 0, err_resp, strlen(err_resp));
                    }
                } else {
                    const char *not_found = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                    m_write(resp_h, 0, not_found, strlen(not_found));
                }
            }
            
            write(client_fd, m_buf(resp_h), m_len(resp_h));
            m_free(resp_h);
            break;
        }
    }

    m_free(data_h);
    http_parser_free(&p);
    close(client_fd);
}

int main() {
    m_init();
    conststr_init();
    trace_level = 1;

    int server_fd, client_fd;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);

    while (1) {
        if ((client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }
        handle_client(client_fd);
    }

    conststr_free();
    m_destruct();
    return 0;
}
