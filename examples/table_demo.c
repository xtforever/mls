#include "../lib/table.h"

#include <stdio.h>

/* Sorted table demo. Entries are inserted out of order and stored sorted
 * by key; iteration and lookup reflect the sorted order. */

int main (void)
{
	m_base_init ();
	int t = tbl_create ();

	tbl_set (t, "zeta", "26");
	tbl_set (t, "alpha", "1");
	tbl_set (t, "beta", "2");
	tbl_set (t, "alpha", "one"); /* overwrite */
	tbl_del (t, "beta");

	printf ("sorted entries:\n");
	for (int i = 0; i < tbl_len (t); i++)
		printf ("  %s = %s\n", tbl_key (t, i), tbl_value (t, i));

	printf ("lookup alpha -> %s\n", tbl_get (t, "alpha"));
	printf ("lookup beta  -> %s\n",
		tbl_get (t, "beta") ? tbl_get (t, "beta") : "(absent)");

	tbl_free (t);
	m_base_destruct ();
	return 0;
}
