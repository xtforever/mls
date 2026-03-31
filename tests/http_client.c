#include "../lib/m_http.h"
#include "../lib/m_tool.h"
#include "../lib/mls.h"
#include <arpa/inet.h>
#include <assert.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 19999
#define BUFFER_SIZE 4096

void test_get (const char *uri)
{
	printf ("\n=== GET %s ===\n", uri);

	int sock = socket (AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		printf ("Socket creation error\n");
		return;
	}

	struct sockaddr_in serv_addr;
	memset (&serv_addr, 0, sizeof (serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons (PORT);

	if (inet_pton (AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
		printf ("Invalid address\n");
		close (sock);
		return;
	}

	if (connect (sock, (struct sockaddr *)&serv_addr, sizeof (serv_addr)) <
	    0) {
		printf ("Connection failed\n");
		close (sock);
		return;
	}

	int req_h = m_alloc (0, 1, MFREE);
	s_printf (req_h, 0,
		  "GET %s HTTP/1.1\r\nHost: localhost\r\nConnection: "
		  "close\r\n\r\n",
		  uri);

	send (sock, m_buf (req_h), m_len (req_h), 0);
	m_free (req_h);

	int resp_data_h = m_alloc (0, 1, MFREE);
	char buffer[BUFFER_SIZE];
	ssize_t bytes_read;

	while ((bytes_read = read (sock, buffer, BUFFER_SIZE)) > 0) {
		m_write (resp_data_h, m_len (resp_data_h), buffer, bytes_read);
	}
	close (sock);

	http_parser_t p;
	http_parser_init_response (&p);

	printf ("Raw response (%d bytes):\n%.*s\n", m_len (resp_data_h),
		m_len (resp_data_h), (char *)m_buf (resp_data_h));

	int res = http_parse (&p, resp_data_h);
	if (res == 0 && p.state == HTTP_STATE_DONE) {
		printf ("\nParsed response:\n");
		printf ("  Version: %s\n", m_str (p.version));
		printf ("  Status: %d\n", p.status_code);
		printf ("  Headers: %d\n", p.header_count);
		printf ("  Body: %d bytes\n", m_len (p.body));

		if (m_len (p.body) > 0) {
			printf ("  Body content: %.*s\n", m_len (p.body),
				(char *)m_buf (p.body));
		}

		if (p.status_code == 200) {
			printf ("  -> SUCCESS\n");
		} else {
			printf ("  -> ERROR: %d\n", p.status_code);
		}
	} else {
		printf ("\nParse failed: %s\n", http_error_string (p.error));
	}

	http_parser_free (&p);
	m_free (resp_data_h);
}

void test_post (const char *uri, const char *body)
{
	printf ("\n=== POST %s ===\n", uri);

	int sock = socket (AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		printf ("Socket creation error\n");
		return;
	}

	struct sockaddr_in serv_addr;
	memset (&serv_addr, 0, sizeof (serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons (PORT);

	if (inet_pton (AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
		printf ("Invalid address\n");
		close (sock);
		return;
	}

	if (connect (sock, (struct sockaddr *)&serv_addr, sizeof (serv_addr)) <
	    0) {
		printf ("Connection failed\n");
		close (sock);
		return;
	}

	int req_h = m_alloc (0, 1, MFREE);
	s_printf (req_h, 0,
		  "POST %s HTTP/1.1\r\nHost: localhost\r\nContent-Length: "
		  "%d\r\nConnection: close\r\n\r\n",
		  uri, (int)strlen (body));
	m_write (req_h, m_len (req_h), body, strlen (body));

	send (sock, m_buf (req_h), m_len (req_h), 0);
	m_free (req_h);

	int resp_data_h = m_alloc (0, 1, MFREE);
	char buffer[BUFFER_SIZE];
	ssize_t bytes_read;

	while ((bytes_read = read (sock, buffer, BUFFER_SIZE)) > 0) {
		m_write (resp_data_h, m_len (resp_data_h), buffer, bytes_read);
	}
	close (sock);

	http_parser_t p;
	http_parser_init_response (&p);

	printf ("Raw response (%d bytes):\n%.*s\n", m_len (resp_data_h),
		m_len (resp_data_h), (char *)m_buf (resp_data_h));

	int res = http_parse (&p, resp_data_h);
	if (res == 0 && p.state == HTTP_STATE_DONE) {
		printf ("\nParsed response:\n");
		printf ("  Status: %d\n", p.status_code);
		printf ("  Body: %.*s\n", m_len (p.body),
			(char *)m_buf (p.body));
	}

	http_parser_free (&p);
	m_free (resp_data_h);
}

void test_security_header_injection ()
{
	printf ("\n=== Security: Header Injection Test ===\n");

	int sock = socket (AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		printf ("Socket creation error\n");
		return;
	}

	struct sockaddr_in serv_addr;
	memset (&serv_addr, 0, sizeof (serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons (PORT);

	inet_pton (AF_INET, "127.0.0.1", &serv_addr.sin_addr);
	connect (sock, (struct sockaddr *)&serv_addr, sizeof (serv_addr));

	unsigned char malicious[] = "GET / HTTP/1.1\r\n"
				    "Host: localhost\r\n"
				    "X-Injection: value\r\n"
				    "\r\n";

	send (sock, malicious, sizeof (malicious) - 1, 0);

	char buffer[256];
	ssize_t bytes = read (sock, buffer, sizeof (buffer) - 1);
	if (bytes > 0) {
		buffer[bytes] = '\0';
		printf ("Response: %s", buffer);
		if (strncmp (buffer, "HTTP/1.1 400", 11) == 0) {
			printf ("-> BLOCKED (as expected)\n");
		}
	}
	close (sock);
}

int main ()
{
	m_init ();
	conststr_init ();
	trace_level = 0;

	printf ("HTTP Client Test Suite\n");
	printf ("======================\n");

	test_get ("/");
	test_get ("/api");
	test_get ("/health");
	test_get ("/echo/hello-world");
	test_get ("/nonexistent");
	test_post ("/api/echo", "Hello from client!");

	printf ("\n");
	test_security_header_injection ();

	printf ("\n=== All tests completed ===\n");

	conststr_free ();
	m_destruct ();
	return 0;
}
