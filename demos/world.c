/* world — one int handle per list, any element type, one generic graph.
 *
 * Every mls list/array is an `int` handle. That means a single generic
 * container (here: a GROUP's int array of node handles) can hold lists of
 * ANY element type — ints, doubles, strings and nested groups — without the
 * container knowing anything about them. Only when a leaf is resolved does
 * the concrete type matter, and a per-node `kind` field tells which cast
 * (INT / DOUBLE / m_str) to apply. So the whole simulated world, with all
 * object kinds and nesting depths, lives in mls as handles.
 *
 * The structure is not limited to trees: a GROUP may reference a node that is
 * already reachable (a shared node or a cycle). The traversal guards against
 * revisits with a visited set, and cleanup still works with a single
 * m_free(root): each node registers a custom free handler (m_reg_freefn) that
 * frees its payload, and a GROUP's child array uses MFREE_EACH so m_free()
 * recurses into the children. Because m_free() marks a handle as being freed
 * *before* it invokes the free handler, a back-reference into an ancestor is
 * seen as already freed and skipped — no infinite recursion, no double free.
 *
 * Modes:
 *   (default)  print the demo world as an indented text graph
 *   dot        emit Graphviz DOT on stdout
 *              (render: ./world dot | dot -Tsvg -o world.svg)
 *   selftest   built-in checks; `make world.svg` renders the demo
 *
 * See docs/guide/handles-vs-pointers.md for the traditional-C comparison.
 *
 * Build: make world.exe   (see examples/makefile; `make world.svg` renders)
 */
#include "../lib/m_tool.h" /* pulls mls.h; s_* string API  */
#include "../lib/mls.h"	   /* INT/DOUBLE, m_alloc/m_free  */

#include <stdio.h>
#include <string.h>

/* A node tags its payload, so a generic walker knows which cast to apply
 * when it finally resolves a leaf. */
typedef enum { NODE_GROUP, NODE_INTS, NODE_DOUBLES, NODE_STRING } node_kind;
typedef struct {
	int kind;
	int data;
} node_t;

/* Custom free handler, registered once in main() after m_init(). Frees a
 * node's payload handle; m_free then frees the node_t buffer itself. For a
 * GROUP the payload is an MFREE_EACH child list, so this recurses. */
static int node_hdl;

static void free_world_node (int h) { m_free (((node_t *)mls (h, 0))->data); }

static int node (int kind, int data)
{
	node_t n = {kind, data};
	int h = m_alloc (0, sizeof (node_t), node_hdl);
	m_put (h, &n);
	return h;
}

static int ngroup (void)
{
	/* Child array of node handles; MFREE_EACH m_free()s each child. */
	return node (NODE_GROUP, m_alloc (0, sizeof (int), MFREE_EACH));
}
static int nints (void) { return node (NODE_INTS, m_create (0, sizeof (int))); }
static int ndoubles (void)
{
	return node (NODE_DOUBLES, m_create (0, sizeof (double)));
}
static int nstring (const char *s) { return node (NODE_STRING, s_dup (s)); }

static int node_data (int h) { return ((node_t *)mls (h, 0))->data; }
static int group_add (int g, int child)
{
	return m_put (node_data (g), &child);
}

static int nints_of (const int *v, int n)
{
	int h = nints ();
	for (int i = 0; i < n; i++)
		m_put (node_data (h), &v[i]);
	return h;
}
static int ndoubles_of (const double *v, int n)
{
	int h = ndoubles ();
	for (int i = 0; i < n; i++)
		m_put (node_data (h), &v[i]);
	return h;
}

/* Graph-traversal guard: a node may be reached again through a shared
 * reference or a cycle. Returns 1 the first time a handle is seen. */
static int first_visit (int seen, int h)
{
	if (m_bsearch_int (seen, h) >= 0)
		return 0;
	m_binsert_int (seen, h);
	return 1;
}

/* Generic recursive printer: follows int handles without knowing the
 * types; casts only in the kind-guarded leaf branches. Revisits (a cycle
 * or a shared node) are marked instead of recursing. */
static void walk (int seen, int h, int depth)
{
	for (int d = 0; d < depth; d++)
		fputs ("  ", stdout);

	if (!first_visit (seen, h)) {
		fputs ("(-> already shown)\n", stdout);
		return;
	}

	node_t *n = mls (h, 0);
	int kind = n->kind, data = n->data;

	switch (kind) {
	case NODE_GROUP:
		fputs ("group [\n", stdout);
		for (int i = 0; i < (int)m_len (data); i++)
			walk (seen, INT (data, i), depth + 1);
		for (int d = 0; d < depth; d++)
			fputs ("  ", stdout);
		fputs ("]\n", stdout);
		break;
	case NODE_INTS:
		fputs ("ints [", stdout);
		for (int i = 0; i < (int)m_len (data); i++)
			printf ("%s%d", i ? " " : "", INT (data, i));
		fputs ("]\n", stdout);
		break;
	case NODE_DOUBLES:
		fputs ("doubles [", stdout);
		for (int i = 0; i < (int)m_len (data); i++)
			printf ("%s%g", i ? " " : "", DOUBLE (data, i));
		fputs ("]\n", stdout);
		break;
	case NODE_STRING:
		printf ("string \"%s\"\n", m_str (data));
		break;
	}
}

/* Generic numeric visitor over mixed, nested nodes (exercises the leaf
 * casts): sums every int and double, ignores strings. Each distinct node
 * is visited once, so shared nodes/cycles cannot loop. */
static double sum_numbers (int seen, int h)
{
	if (!first_visit (seen, h))
		return 0;

	node_t *n = mls (h, 0);
	int kind = n->kind, data = n->data;
	double s = 0;

	if (kind == NODE_GROUP) {
		for (int i = 0; i < (int)m_len (data); i++)
			s += sum_numbers (seen, INT (data, i));
		return s;
	}
	if (kind == NODE_INTS) {
		for (int i = 0; i < (int)m_len (data); i++)
			s += INT (data, i);
		return s;
	}
	if (kind == NODE_DOUBLES) {
		for (int i = 0; i < (int)m_len (data); i++)
			s += DOUBLE (data, i);
		return s;
	}
	return 0; /* STRING */
}

static int count_nodes (int seen, int h)
{
	if (!first_visit (seen, h))
		return 0;

	node_t *n = mls (h, 0);
	if (n->kind != NODE_GROUP)
		return 1;
	int c = 1;
	for (int i = 0; i < (int)m_len (n->data); i++)
		c += count_nodes (seen, INT (n->data, i));
	return c;
}

/* Comma-separated label for a numeric payload, as a string handle. */
static int ints_label (int data)
{
	int s = s_new ();
	for (int i = 0; i < (int)m_len (data); i++) {
		if (i)
			s_app (s, ", ", NULL);
		s_printf (s, -1, "%d", INT (data, i));
	}
	return s;
}
static int doubles_label (int data)
{
	int s = s_new ();
	for (int i = 0; i < (int)m_len (data); i++) {
		if (i)
			s_app (s, ", ", NULL);
		s_printf (s, -1, "%g", DOUBLE (data, i));
	}
	return s;
}

/* Emit Graphviz DOT for the tree. The mls handle itself is the node id, so
 * the generic handle doubles as identity across the graph.
 * ponytail: labels are emitted unescaped — string-node text must not contain
 * `"`, `\` or a newline (the demo uses simple names). Upgrade: escape them. */
static void emit_dot_node (FILE *out, int seen, int h)
{
	if (!first_visit (seen, h))
		return; /* already emitted; the back-edge was drawn by the
			   caller */

	node_t *n = mls (h, 0);
	int kind = n->kind, data = n->data;

	switch (kind) {
	case NODE_GROUP:
		fprintf (out,
			 "  n%d [shape=box, style=filled, "
			 "fillcolor=\"#cfe2f3\", "
			 "label=\"group\"];\n",
			 h);
		for (int i = 0; i < (int)m_len (data); i++) {
			int c = INT (data, i);
			fprintf (out, "  n%d -> n%d [label=\"%d\"];\n", h, c,
				 i);
			emit_dot_node (out, seen, c);
		}
		break;
	case NODE_INTS: {
		int lbl = ints_label (data);
		fprintf (out,
			 "  n%d [shape=ellipse, style=filled, "
			 "fillcolor=\"#d9ead3\", label=\"ints [%s]\"];\n",
			 h, m_str (lbl));
		m_free (lbl);
		break;
	}
	case NODE_DOUBLES: {
		int lbl = doubles_label (data);
		fprintf (out,
			 "  n%d [shape=diamond, style=filled, "
			 "fillcolor=\"#fce5cd\", label=\"doubles [%s]\"];\n",
			 h, m_str (lbl));
		m_free (lbl);
		break;
	}
	case NODE_STRING:
		fprintf (out,
			 "  n%d [shape=note, style=filled, "
			 "fillcolor=\"#fff2cc\", "
			 "label=\"%s\"];\n",
			 h, m_str (data));
		break;
	}
}

static void emit_dot (FILE *out, int seen, int world)
{
	fprintf (out, "digraph world {\n");
	fprintf (out, "  graph [rankdir=LR];\n");
	fprintf (out, "  node [fontname=\"Courier\"];\n");
	emit_dot_node (out, seen, world);
	fprintf (out, "}\n");
}

/* A small simulated world: groups, positions (ints), masses (doubles) and
 * names (strings), nested to several depths. earth also links back to the
 * whole world, so the scene is a graph with a cycle
 * (world -> actors -> earth -> world), not a tree. */
static int build_demo_world (void)
{
	int world = ngroup ();

	group_add (world, nstring ("sim"));
	group_add (world, nints_of ((int[]){2026, 1}, 2));

	int actors = ngroup ();
	group_add (actors, nstring ("sun"));
	group_add (actors, nints_of ((int[]){0, 0}, 2));
	group_add (actors, ndoubles_of ((double[]){1.989e30}, 1));

	int earth = ngroup ();
	group_add (earth, nstring ("earth"));
	group_add (earth, nints_of ((int[]){1, 0}, 2));
	group_add (earth, ndoubles_of ((double[]){5.972e24}, 1));
	group_add (actors, earth);

	group_add (world, actors);
	group_add (world, ndoubles_of ((double[]){3.14, 2.71}, 2));

	group_add (earth, world); /* circular reference back to the root */
	return world;
}

/* selftest helper: explicit check that stays active without NDEBUG. */
#define CHECK(cond)                                                            \
	do {                                                                   \
		if (!(cond)) {                                                 \
			fprintf (stderr, "selftest FAILED: %s (%s:%d)\n",      \
				 #cond, __FILE__, __LINE__);                   \
			return 1;                                              \
		}                                                              \
	} while (0)

static int selftest (void)
{
	/* Programmatic leak check via introspection: m_count_allocated()
	 * reports live handles, so the world can be verified to free back to
	 * the baseline without any debug dump or ASan. */
	size_t base = m_count_allocated ();

	int seen = m_alloc (0, sizeof (int), MFREE);
	CHECK (m_count_allocated () == base + 1); /* the visited set */

	int root = ngroup ();
	group_add (root, nints_of ((int[]){1, 2, 3}, 3));
	group_add (root, ndoubles_of ((double[]){0.5, 1.25}, 2));
	group_add (root, nstring ("probe"));
	int g = ngroup ();
	group_add (g, nints_of ((int[]){4}, 1));
	group_add (g, ndoubles_of ((double[]){2.5}, 1));
	group_add (root, g);
	/* 7 nodes, each one node handle + one payload handle */
	CHECK (m_count_allocated () == base + 1 + 14);

	/* Generic traversal resolves mixed leaf types by kind. */
	CHECK (sum_numbers (seen, root) == 14.25); /* 6 + 1.75 + 4 + 2.5 */
	m_clear (seen);
	CHECK (count_nodes (seen, root) == 7);

	/* A cycle must not loop traversal, and m_free must tear it down. */
	m_clear (seen);
	int ca = ngroup (), cb = ngroup ();
	group_add (ca, cb);
	group_add (cb, ca);		     /* ca <-> cb */
	CHECK (count_nodes (seen, ca) == 2); /* each node once */

	/* DOT emitter smoke test on the cyclic graph (also runs under ASan). */
	m_clear (seen);
	FILE *f = tmpfile ();
	CHECK (f != NULL);
	emit_dot (f, seen, ca);
	fflush (f);
	rewind (f);
	int saw_digraph = 0, saw_edge = 0;
	char line[256];
	while (fgets (line, sizeof line, f))
		if (strstr (line, "digraph world"))
			saw_digraph = 1;
		else if (strstr (line, "->"))
			saw_edge = 1;
	fclose (f);
	CHECK (saw_digraph && saw_edge);

	m_free (ca); /* the 2-cycle is freed cleanly, in one call */

	m_clear (seen);
	int self = ngroup ();
	group_add (self, self); /* self reference */
	CHECK (count_nodes (seen, self) == 1);
	m_free (self); /* the self-reference too */

	m_free (seen);
	m_free (root); /* one call frees the whole tree via the free handlers */
	CHECK (m_count_allocated () == base); /* nothing leaked */
	printf ("selftest OK\n");
	return 0;
}

int main (int argc, char **argv)
{
	m_init ();
	node_hdl = m_reg_freefn (free_world_node);

	int rc = 0;
	if (argc > 1 && strcmp (argv[1], "selftest") == 0) {
		rc = selftest ();
	} else if (argc > 1 && strcmp (argv[1], "dot") == 0) {
		int world = build_demo_world ();
		int seen = m_alloc (0, sizeof (int), MFREE);
		emit_dot (stdout, seen, world);
		m_free (seen);
		m_free (world);
	} else {
		int world = build_demo_world ();
		int seen = m_alloc (0, sizeof (int), MFREE);
		walk (seen, world, 0);
		m_free (seen);
		m_free (world);
	}

	m_destruct ();
	return rc;
}
