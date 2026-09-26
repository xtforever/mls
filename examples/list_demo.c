#include "../lib/list.h"

#include <stdio.h>

/* Build a nested structure directly out of the universal list node
 * ({ int type; int data }) and print it. No parser, no table — just the
 * list-keeping base. */

static void print (int h, int depth)
{
	for (int d = 0; d < depth; d++)
		fputs ("  ", stdout);
	if (list_type (h) == LIST_LIST) {
		fputs ("(\n", stdout);
		for (int i = 0; i < list_len (h); i++)
			print (list_get (h, i), depth + 1);
		for (int d = 0; d < depth; d++)
			fputs ("  ", stdout);
		fputs (")\n", stdout);
	} else {
		fputs (list_text (h) ? list_text (h) : "null", stdout);
		fputc ('\n', stdout);
	}
}

int main (void)
{
	m_base_init ();

	int root = list_new ();

	int server = list_new ();
	list_add (server, list_string ("server"));

	int port = list_new ();
	list_add (port, list_string ("port"));
	list_add (port, list_string ("8080"));
	list_add (server, port);

	int paths = list_new ();
	list_add (paths, list_string ("paths"));
	int static_ = list_new ();
	list_add (static_, list_string ("static"));
	list_add (static_, list_string ("/var/www"));
	list_add (paths, static_);
	list_add (server, paths);

	list_add (root, server);

	print (root, 0);

	list_free (root);
	m_base_destruct ();
	return 0;
}
