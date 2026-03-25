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
#include <assert.h>

#define PORT 19999
#define BUFFER_SIZE 4096

void send_get_request(const char *uri) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        close(sock);
        return;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        close(sock);
        return;
    }

    int req_h = m_alloc(0, 1, MFREE);
    s_printf(req_h, 0, "GET %s HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n", uri);
    
    send(sock, m_buf(req_h), m_len(req_h), 0);
    m_free(req_h);

    int resp_data_h = m_alloc(0, 1, MFREE);
    ssize_t valread;
    while ((valread = read(sock, buffer, BUFFER_SIZE)) > 0) {
        m_write(resp_data_h, m_len(resp_data_h), buffer, valread);
    }

    printf("Received response (%d bytes):\n%.*s\n", m_len(resp_data_h), m_len(resp_data_h), (char*)m_buf(resp_data_h));

    http_parser_t p;
    http_parser_init(&p);
    if (http_parse(&p, resp_data_h) == 0) {
        printf("Response parsed successfully.\n");
        printf("Method: %s, URI: %s, Version: %s\n", 
               m_str(p.method), m_str(p.uri), m_str(p.version));
        if (strcmp(m_str(p.uri), "200") == 0) {
            printf("Success: Status 200 OK\n");
        } else {
            printf("Status: %s\n", m_str(p.uri));
        }
        
        if (m_len(p.body) > 0) {
            printf("Body (%d bytes): %.*s\n", m_len(p.body), m_len(p.body), (char*)m_buf(p.body));
        }
    } else {
        printf("Failed to parse response (this is normal for HTTP responses - parser expects requests).\n");
    }

    http_parser_free(&p);
    m_free(resp_data_h);
    close(sock);
}

int main() {
    m_init();
    conststr_init();
    trace_level = 1;

    printf("Sending request to / ...\n");
    send_get_request("/");

    printf("\nSending request to /nonexistent ...\n");
    send_get_request("/nonexistent");

    conststr_free();
    m_destruct();
    return 0;
}
