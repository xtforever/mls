/* Tests for the keyword-index demo.
   - synthetic CSV: exact build/parse/query results
   - real CSV (argv[1]): brute-force scalar cross-check of the bitmap queries */
#include "kwdb.h"

#include "bm.h"
#include "mls.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void write_file (const char *path, const char *txt)
{
	FILE *f = fopen (path, "w");
	assert (f);
	fputs (txt, f);
	fclose (f);
}

/* ---- synthetic ---------------------------------------------------------- */

static uint32_t mask_of (kwdb *db, const char *q)
{
	char err[256];
	int bs = kwdb_query (db, q, err, sizeof err);
	if (bs < 0) {
		fprintf (stderr, "query '%s' failed: %s\n", q, err);
		assert (0);
	}
	uint32_t m = 0;
	int64_t b = bm_next1 (bs, 0);
	while (b >= 0) {
		m |= 1u << (unsigned)b;
		if (b == (int64_t)UINT32_MAX)
			break;
		b = bm_next1 (bs, (bm_bit_t)(b + 1));
	}
	bm_destroy (bs);
	return m;
}

static void test_synth (void)
{
	const char *csv = "/tmp/opencode/kwdb_synth.csv";
	const char *kw = "/tmp/opencode/kwdb_synth.kw";

	write_file (csv,
		    "Text#,Type,Issued,Title,Language,Authors,Subjects,LoCC,"
		    "Bookshelves\n"
		    "1,Text,2020-01-01,Book A,en,Alice,\"Love stories; "
		    "Vampires\",,\"Romance\"\n"
		    "2,Text,2020-01-01,Book B,en,Bob,\"Love stories\",,"
		    "\"Romance\"\n"
		    "3,Text,2020-01-01,Book C,en,Carol,\"Science fiction\",,"
		    "\"Science Fiction\"\n"
		    "4,Text,2020-01-01,\"Book, D\",en,Dave,\"Detective and "
		    "mystery stories\",,\"Crime\"\n"
		    "5,Text,2020-01-01,\"Book \"\"E\"\"\",en,Erin,\"Poetry\",,"
		    "\"\"\n"
		    "6,Text,2020-01-01,Book F,en,Frank,\"\",,\"\"\n");

	write_file (kw, "love-story\tLove stories\n"
			"vampire\tVampires\n"
			"science-fiction\tScience fiction\n"
			"detective\tDetective and mystery stories\n"
			"poetry\tPoetry\n"
			"romance\tRomance\n");

	kwdb *db = kwdb_build (csv, kw, 0);
	assert (db);
	assert (kwdb_books (db) == 6);
	assert (kwdb_keywords (db) == 6);

	/* CSV quoting: embedded comma and escaped quote */
	assert (strcmp (kwdb_title (db, 3), "Book, D") == 0);
	assert (strcmp (kwdb_title (db, 4), "Book \"E\"") == 0);
	assert (strcmp (kwdb_author (db, 4), "Erin") == 0);

	assert (kwdb_keyword_count (db, "love-story") == 2);
	assert (kwdb_keyword_count (db, "vampire") == 1);
	assert (kwdb_keyword_count (db, "romance") == 2);
	assert (kwdb_keyword_count (db, "poetry") == 1);
	assert (kwdb_keyword_count (db, "bogus") == (size_t)-1);

	/* query semantics + precedence */
	assert (mask_of (db, "love-story") == ((1u << 0) | (1u << 1)));
	assert (mask_of (db, "love-story and not vampire") == (1u << 1));
	assert (mask_of (db, "love-story or poetry") ==
		((1u << 0) | (1u << 1) | (1u << 4)));
	assert (mask_of (db, "(science-fiction or detective) and not poetry") ==
		((1u << 2) | (1u << 3)));
	/* and binds tighter than or: poetry or (science-fiction and detective)
	 */
	assert (mask_of (db, "poetry or science-fiction and detective") ==
		(1u << 4));
	assert (mask_of (db, "not poetry") ==
		((1u << 0) | (1u << 1) | (1u << 2) | (1u << 3) | (1u << 5)));
	assert (mask_of (db, "NOT LOVE-STORY") ==
		((1u << 2) | (1u << 3) | (1u << 4) | (1u << 5)));

	/* errors */
	char err[256];
	assert (kwdb_query (db, "bogus", err, sizeof err) < 0);
	assert (strstr (err, "unknown") != NULL);
	assert (kwdb_query (db, "love-story and", err, sizeof err) < 0);
	assert (kwdb_query (db, "poetry poetry", err, sizeof err) < 0);
	assert (kwdb_query (db, "(poetry", err, sizeof err) < 0);
	assert (kwdb_query (db, "love-story $ roman", err, sizeof err) < 0);
	assert (kwdb_query (db, "", err, sizeof err) < 0);

	kwdb_free (db);
	printf ("synthetic: all ok\n");
}

/* ---- real data brute-force cross-check ---------------------------------- */

static int has (kwdb *db, size_t id, const char *name)
{
	int i = kwdb_keyword_index (db, name);
	if (i < 0)
		return 0;
	return kwdb_match (kwdb_subjects (db, id),
			   kwdb_keyword_patterns (db, i));
}

typedef int (*pred_fn) (kwdb *, size_t);

static void compare (kwdb *db, const char *q, pred_fn p)
{
	char err[256];
	int bs = kwdb_query (db, q, err, sizeof err);
	if (bs < 0) {
		fprintf (stderr, "query failed: %s: %s\n", q, err);
		assert (0);
	}
	size_t expected = 0;
	for (size_t id = 0; id < kwdb_books (db); id++) {
		int e = p (db, id);
		int got = bm_test (bs, (bm_bit_t)id);
		if (e != got) {
			fprintf (stderr,
				 "MISMATCH query '%s' id=%zu expected=%d "
				 "got=%d\n",
				 q, id, e, got);
			assert (0);
		}
		expected += e;
	}
	assert (kwdb_bitmap_count (bs) == expected);
	printf ("  ok  %-52s %zu hits\n", q, expected);
	bm_destroy (bs);
}

static int p_love_roman_not_vampire (kwdb *db, size_t id)
{
	return has (db, id, "love-story") && has (db, id, "roman") &&
	       !has (db, id, "vampire");
}
static int p_sf_and_det_or_adv (kwdb *db, size_t id)
{
	return has (db, id, "science-fiction") &&
	       (has (db, id, "detective") || has (db, id, "adventure"));
}
static int p_poetry_or_drama (kwdb *db, size_t id)
{
	return has (db, id, "poetry") || has (db, id, "drama");
}
static int p_not_romance (kwdb *db, size_t id)
{
	return !has (db, id, "romance");
}
static int p_fiction_not_child_poetry (kwdb *db, size_t id)
{
	return has (db, id, "fiction") &&
	       !(has (db, id, "children") || has (db, id, "poetry"));
}

static void test_real (const char *csv, const char *kwfile)
{
	kwdb *db = kwdb_build (csv, kwfile, 3000);
	assert (db);
	printf ("brute-force cross-check on %zu books:\n", kwdb_books (db));
	compare (db, "love-story and roman and not vampire",
		 p_love_roman_not_vampire);
	compare (db, "science-fiction and (detective or adventure)",
		 p_sf_and_det_or_adv);
	compare (db, "poetry or drama", p_poetry_or_drama);
	compare (db, "not romance", p_not_romance);
	compare (db, "fiction and not (children or poetry)",
		 p_fiction_not_child_poetry);
	kwdb_free (db);
}

int main (int argc, char **argv)
{
	test_synth ();
	if (argc > 1) {
		const char *kwfile = argc > 2 ? argv[2] : "keywords.txt";
		test_real (argv[1], kwfile);
	} else {
		printf ("(no csv given; skipping brute-force cross-check)\n");
	}
	m_destruct ();
	printf ("all kwdb tests passed\n");
	return 0;
}
