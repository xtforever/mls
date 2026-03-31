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

	unsigned char raw[] =
		"GET / HTTP/1.1\r\nHost: localhost\r\nX-Injection: "
		"value\x0D\x0Ainjected\r\n\r\n";
	int data = s_strdup_c ((const char *)raw);

	int val = s_strdup_c ("value\x0D\x0Ainjected");
	printf ("Val len: %d\n", m_len (val));
	printf ("Val: ");
	for (int i = 0; i < m_len (val); i++) {
		unsigned char c = ((unsigned char *)m_buf (val))[i];
		if (c == '\r')
			printf ("\\r");
		else if (c == '\n')
			printf ("\\n");
		else if (c < 32)
			printf ("\\x%02x", c);
		else
			printf ("%c", c);
	}
	printf ("\n");

	int valid = http_validate_header_value (val);
	printf ("Validation: %d\n", valid);

	m_free (data);
	m_free (val);

	conststr_free ();
	m_destruct ();
	return 0;
}
