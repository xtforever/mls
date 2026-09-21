#include "../lib/hdf_base.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main (void)
{
	assert (m_base_init () == 0);

	const char *data = "(rem \"skip me\")\n"
			   "(server\n"
			   "  (port 8080)\n"
			   "  (root \"/var/www\")\n"
			   "  (debug true)\n"
			   "  (ratio 3.14)\n"
			   "  (empty null)\n"
			   "  (raw [[multi\nline]])\n"
			   "  (bind (path \"/api\") (call \"api_hello\"))\n"
			   ")";

	int root = hdf_base_read (data);
	assert (root > 0);
	assert (hdf_base_type (root) == HDF_BASE_LIST);

	/* only 'server' remains; top-level 'rem' is gone */
	assert (m_len (hdf_base_children (root)) == 1);

	int server = INT (hdf_base_children (root), 0);
	assert (hdf_base_type (server) == HDF_BASE_LIST);

	/* atom access */
	int port = hdf_base_find (server, "port");
	assert (port > 0);
	assert (hdf_base_type (INT (hdf_base_children (port), 1)) ==
		HDF_BASE_NUMBER);
	assert (strcmp (hdf_base_prop (server, "port"), "8080") == 0);
	assert (strcmp (hdf_base_prop (server, "root"), "/var/www") == 0);
	assert (strcmp (hdf_base_prop (server, "debug"), "true") == 0);
	assert (strcmp (hdf_base_prop (server, "ratio"), "3.14") == 0);

	/* null atom */
	int empty = hdf_base_find (server, "empty");
	assert (hdf_base_type (INT (hdf_base_children (empty), 1)) ==
		HDF_BASE_NULL);

	/* raw string */
	assert (strcmp (hdf_base_prop (server, "raw"), "multi\nline") == 0);

	/* nested list */
	int bind = hdf_base_find (server, "bind");
	assert (bind > 0);
	assert (strcmp (hdf_base_prop (bind, "path"), "/api") == 0);
	assert (strcmp (hdf_base_prop (bind, "call"), "api_hello") == 0);

	/* round-trip count check: "server" keyword + 7 sub-lists */
	assert (m_len (hdf_base_children (server)) == 8);

	hdf_base_free (root);
	m_base_destruct ();

	printf ("hdf_base: all checks passed.\n");
	return 0;
}
