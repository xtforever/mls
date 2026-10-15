/* handles_demo — the paper's two-minute proof, as a standalone file.
 *
 * Copy mls_full.h next to this file, then build and run with one command:
 *
 *     cc -std=c11 handles_demo.c -o demo -lpthread -lm -ldl
 *     ./demo
 *     ./demo crash       # watch the use-after-free tripwire fire
 *
 * mls_full.h is the single-header amalgam; generate it in the repo root with
 * `python3 generate_amalgam.py`. In under 60 lines this shows the four claims
 * of "You Do Not Know C":
 *   1. one value, not two    — the handle survives every resize (no realloc)
 *   2. teardown is one call  — a cyclic graph freed by one m_free()
 *   3. leaks you can see     — m_count_allocated() proves nothing leaked
 *   4. use-after-free is loud, not silent
 */
#define MLS_FULL_IMPLEMENTATION
#include "mls_full.h"
#include <stdio.h>
#include <string.h>

/* one node = one handle; its payload is another handle */
typedef struct {
	int kind;
	int data;
} node_t;

static int node_hdl; /* id of the registered free handler */
static void free_node (int h) { m_free (((node_t *)mls (h, 0))->data); }
static int payload (int h) { return ((node_t *)mls (h, 0))->data; }

static int node (int kind, int data)
{
	node_t n = {kind, data};
	int h = m_alloc (0, sizeof (node_t), node_hdl);
	m_put (h, &n);
	return h;
}

int main (int argc, char **argv)
{
	m_init ();
	node_hdl = m_reg_freefn (free_node);
	size_t base = m_count_allocated ();

	/* 1. one value, not two: the handle survives every resize */
	int a = m_alloc (2, sizeof (int), MFREE); /* capacity 2 */
	for (int i = 0; i < 8; i++)
		m_put (a, &i); /* grows past 2 */
	for (int i = 0; i < m_len (a); i++)
		printf ("%d%s", INT (a, i), i + 1 < m_len (a) ? " " : "\n");
	m_free (a);

	/* 2. teardown is one call: two nodes that point at each other */
	int g1 = node (0, m_alloc (0, sizeof (int), MFREE_EACH));
	int g2 = node (0, m_alloc (0, sizeof (int), MFREE_EACH));
	m_put (payload (g1), &g2); /* g1 -> g2 */
	m_put (payload (g2), &g1); /* g2 -> g1 (a cycle) */
	m_free (g1);

	/* 3. leaks you can see, in a release build */
	printf ("live handles after teardown: %zu\n",
		m_count_allocated () - base);

	/* 4. use-after-free is loud, not silent: run `./demo crash` */
	if (argc > 1 && strcmp (argv[1], "crash") == 0) {
		int h = m_alloc (1, sizeof (int), MFREE);
		m_free (h);
		printf ("%d\n", INT (h, 0)); /* exits, with a diagnostic */
	}

	m_destruct ();
	return 0;
}
