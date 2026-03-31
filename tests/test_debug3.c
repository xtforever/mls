#include "../lib/m_http.h"
#include "../lib/m_tool.h"
#include "../lib/mls.h"
#include <stdio.h>
#include <string.h>

int main ()
{
	trace_level = 1;
	m_init ();
	conststr_init ();

	http_parser_t p;
	http_parser_init (&p);

	const char *raw = "POST /upload HTTP/1.1\r\n"
			  "Host: localhost\r\n"
			  "Content-Length: 10\r\n"
			  "Transfer-Encoding: chunked\r\n"
			  "\r\n"
			  "5\r\nhello\r\n0\r\n\r\n";
	int data = s_strdup_c (raw);

	printf ("Data: %s\n", m_str (data));

	int res = http_parse (&p, data);
	printf ("Result: %d\n", res);
	printf ("State: %s\n", http_state_string (p.state));
	printf ("Error: %s\n", http_error_string (p.error));
	printf ("has_transfer_encoding: %d\n", p.has_transfer_encoding);
	printf ("has_content_length: %d\n", p.has_content_length);
	printf ("is_chunked: %d\n", p.is_chunked);

	int cl = m_table_get_cstr (p.headers, "content-length");
	printf ("CL header: %s\n", cl > 0 ? m_str (cl) : "NULL");
	int te = m_table_get_cstr (p.headers, "transfer-encoding");
	printf ("TE header: %s\n", te > 0 ? m_str (te) : "NULL");

	m_free (data);
	http_parser_free (&p);

	conststr_free ();
	m_destruct ();
	return 0;
}
