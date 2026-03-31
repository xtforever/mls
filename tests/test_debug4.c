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

	const char *raw = "GET / HTTP/1.1\r\nHost: localhost\r\nX-Injection: "
			  "value\r\ninjected\r\n\r\n";
	int data = s_strdup_c (raw);

	printf ("Data: ");
	for (int i = 0; i < m_len (data); i++) {
		char c = ((char *)m_buf (data))[i];
		if (c == '\r')
			printf ("\\r");
		else if (c == '\n')
			printf ("\\n");
		else
			printf ("%c", c);
	}
	printf ("\n");

	int res = http_parse (&p, data);
	printf ("Result: %d\n", res);
	printf ("State: %s\n", http_state_string (p.state));
	printf ("Error: %s\n", http_error_string (p.error));

	m_free (data);
	http_parser_free (&p);

	conststr_free ();
	m_destruct ();
	return 0;
}
