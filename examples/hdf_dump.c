/*
 * hdf_dump — read an HDF file and print it back as an s-expression.
 *
 * This example uses only mls_base (the smallest part of the MLS library) plus
 * the tiny hdf_base parser built on top of it. It is written for someone who
 * has never used MLS before.
 *
 * The one idea you need up front: in MLS you never hold a pointer to an
 * array. A dynamic array is just an `int` handle, handed to you by a create
 * function and passed back to every other function. The library keeps the
 * real memory behind that int, so a wrong or freed handle is a safe error
 * instead of a dangling pointer.
 */

#include "../lib/hdf_base.h"

#include <stdio.h>

/* Print one node recursively.
 *
 * A node is an `hdf_base_node_t`, i.e. a struct { int type; int data; }:
 *
 *   - HDF_BASE_LIST:  `data` is another handle — the list of child nodes.
 *   - other types:    `data` is a handle to the atom's text (a char array).
 *
 * `h` is an int handle, not a pointer.
 */
static void emit (int h)
{
	hdf_base_type_t t = hdf_base_type (h); /* read the node's `type` */

	if (t == HDF_BASE_LIST) {
		/* `data` holds the child list: a handle to an array of ints,
		 * where each int is itself a node handle. */
		int ch = hdf_base_children (h);

		/* m_foreach(list, i, ptr) is a for-loop over `list`.
		 * i starts at -1; each turn it is bumped and `ptr` is set to
		 * point at the next element. Here elements are ints, so ptr is
		 * an `int *` and `*c` is the child node handle. */
		int i, *c;
		fputs ("(", stdout);
		m_foreach (ch, i, c)
		{
			if (i > 0)
				fputc (' ', stdout);
			emit (*c);
		}
		fputs (")", stdout);
	} else if (t == HDF_BASE_NULL) {
		fputs ("null", stdout);
	} else {
		/* For STRING/NUMBER/BOOL nodes, hdf_base_str() returns the atom
		 * text as a plain C string (a `const char *`, not a handle). */
		fputs (hdf_base_str (h), stdout);
	}
}

int main (int argc, char **argv)
{
	if (argc != 2) {
		fprintf (stderr, "usage: %s FILE.hdf\n", argv[0]);
		return 2;
	}

	/* mls_base must be initialised once before any handle is created.
	 * (There is a matching teardown at the bottom of main.) */
	m_base_init ();

	/* Parse the file. Returns the root node as an int handle,
	 * or 0 if the file could not be read or parsed. */
	int root = hdf_base_read_file (argv[1]);
	if (!root) {
		fprintf (stderr, "hdf: cannot parse %s\n", argv[1]);
		return 1;
	}

	/* The root is a LIST whose children are the top-level forms of the
	 * file. Print each one on its own line. */
	int ch = hdf_base_children (root);
	int i, *c;
	m_foreach (ch, i, c)
	{
		if (i > 0)
			fputc ('\n', stdout);
		emit (*c);
	}
	fputc ('\n', stdout);

	/* Free the whole tree (recursively) and shut the library down.
	 * Handles are cheap: one free call per node, no pointer chasing. */
	hdf_base_free (root);
	m_base_destruct ();
	return 0;
}
