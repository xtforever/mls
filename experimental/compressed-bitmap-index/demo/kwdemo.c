/* CLI for the keyword-index demo. */
#include "kwdb.h"

#include "bm.h"
#include "mls.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now_s (void)
{
	struct timespec ts;
	clock_gettime (CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void usage (const char *prog)
{
	fprintf (
		stderr,
		"usage: %s <command> <pg_catalog.csv> <keywords.txt> [args]\n"
		"  stats    <csv> <kw> [cap]           build and print stats\n"
		"  keywords <csv> <kw> [cap]           list keywords + counts\n"
		"  match    <csv> <kw> <keyword> [cap] list books for one "
		"keyword\n"
		"  query    <csv> <kw> \"<expr>\" [cap]  evaluate a boolean "
		"query\n"
		"\n"
		"expr grammar: keyword | not expr | expr and expr | expr or "
		"expr\n"
		"              | ( expr )        precedence: not > and > or\n",
		prog);
}

static void print_books (const kwdb *db, int bs, size_t limit)
{
	size_t shown = 0;
	int64_t b = bm_next1 (bs, 0);
	while (b >= 0 && shown < limit) {
		printf ("  %6ld  %-50.50s  %s\n", (long)b,
			kwdb_title (db, (size_t)b),
			kwdb_author (db, (size_t)b));
		shown++;
		if (b == (int64_t)UINT32_MAX)
			break;
		b = bm_next1 (bs, (bm_bit_t)(b + 1));
	}
}

int main (int argc, char **argv)
{
	if (argc < 4) {
		usage (argv[0]);
		return 2;
	}
	const char *cmd = argv[1], *csv = argv[2], *kwfile = argv[3];
	size_t cap = 0;

	double t0 = now_s ();
	kwdb *db = kwdb_build (csv, kwfile, cap);
	if (!db) {
		fprintf (stderr, "failed to build index from %s / %s\n", csv,
			 kwfile);
		return 1;
	}
	double t1 = now_s ();

	if (strcmp (cmd, "stats") == 0) {
		size_t total = 0, best = 0, nonempty = 0;
		for (size_t k = 0; k < kwdb_keywords (db); k++) {
			size_t c = kwdb_bitmap_count (kwdb_keyword_bitmap (
				db, kwdb_keyword_name (db, k)));
			total += c;
			if (c > best)
				best = c;
			if (c)
				nonempty++;
		}
		printf ("books:            %zu\n", kwdb_books (db));
		printf ("keywords:         %zu (%zu non-empty)\n",
			kwdb_keywords (db), nonempty);
		printf ("total memberships:%zu (avg %.1f/keyword)\n", total,
			kwdb_keywords (db) ? (double)total / kwdb_keywords (db)
					   : 0.0);
		printf ("largest keyword:  %zu books\n", best);
		printf ("build time:       %.2fs\n", t1 - t0);
	} else if (strcmp (cmd, "keywords") == 0) {
		for (size_t k = 0; k < kwdb_keywords (db); k++) {
			int bs = kwdb_keyword_bitmap (
				db, kwdb_keyword_name (db, k));
			printf ("  %6zu  %s\n", kwdb_bitmap_count (bs),
				kwdb_keyword_name (db, k));
		}
	} else if (strcmp (cmd, "match") == 0) {
		if (argc < 5) {
			usage (argv[0]);
			kwdb_free (db);
			return 2;
		}
		const char *name = argv[4];
		int bs = kwdb_keyword_bitmap (db, name);
		if (bs < 0) {
			fprintf (stderr, "unknown keyword '%s'\n", name);
			kwdb_free (db);
			return 1;
		}
		printf ("%s: %zu books\n", name, kwdb_bitmap_count (bs));
		print_books (db, bs, 20);
	} else if (strcmp (cmd, "query") == 0) {
		if (argc < 5) {
			usage (argv[0]);
			kwdb_free (db);
			return 2;
		}
		const char *expr = argv[4];
		char err[256];
		double q0 = now_s ();
		int bs = kwdb_query (db, expr, err, sizeof err);
		double q1 = now_s ();
		if (bs < 0) {
			fprintf (stderr, "query error: %s\n", err);
			kwdb_free (db);
			return 1;
		}
		printf ("query: %s\n", expr);
		printf ("hits:  %zu   (eval %.3f ms)\n", kwdb_bitmap_count (bs),
			(q1 - q0) * 1000.0);
		print_books (db, bs, 20);
		bm_destroy (bs);
	} else {
		usage (argv[0]);
		kwdb_free (db);
		return 2;
	}

	kwdb_free (db);
	return 0;
}
