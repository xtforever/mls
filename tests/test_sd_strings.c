#include "sd.h"
#include "sd_strings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cmp_int (const void *a, const void *b);

static int tests_run = 0;
static int tests_passed = 0;

#define PASS(msg, ...)                                                         \
	do {                                                                   \
		tests_passed++;                                                \
		printf ("  PASS: " msg "\n", ##__VA_ARGS__);                   \
	} while (0)

#define FAIL(msg, ...)                                                         \
	do {                                                                   \
		printf ("  FAIL: " msg "\n", ##__VA_ARGS__);                   \
		exit (1);                                                      \
	} while (0)

#define RUN_TEST(name)                                                         \
	do {                                                                   \
		printf ("\nRunning %s...\n", #name);                           \
		tests_run++;                                                   \
		name ();                                                       \
		printf ("  RESULT: PASS\n");                                   \
	} while (0)

static void test_new_free (void)
{
	int h = sd_new ();
	PASS ("sd_new returns valid handle");
	if (h <= 0)
		FAIL ("sd_new returned invalid handle %d", h);
	if (sd_len (h) != 0)
		FAIL ("new buffer should have length 0, got %d", sd_len (h));
	sd_sfree (h);
	PASS ("sd_sfree works");
}

static void test_strlen (void)
{
	int h = sd_strdup ("hello");
	if (sd_strlen (h) != 5)
		FAIL ("expected strlen 5, got %d", sd_strlen (h));
	PASS ("sd_strlen works for 'hello'");
	sd_sfree (h);

	h = sd_new ();
	if (sd_strlen (h) != 0)
		FAIL ("empty string should have strlen 0, got %d",
		      sd_strlen (h));
	PASS ("sd_strlen works for empty string");
	sd_sfree (h);
}

static void test_dup (void)
{
	int h = sd_strdup ("test");
	if (h <= 0)
		FAIL ("sd_strdup returned invalid handle");
	PASS ("sd_strdup creates valid handle");

	if (sd_strlen (h) != 4)
		FAIL ("expected strlen 4, got %d", sd_strlen (h));
	PASS ("sd_strdup content correct");

	if (strcmp (sd_buf (h), "test") != 0)
		FAIL ("expected 'test', got '%s'", (char *)sd_buf (h));
	PASS ("sd_strdup content matches");

	sd_sfree (h);

	h = sd_strdup ("");
	if (sd_strlen (h) != 0)
		FAIL ("empty string dup should have strlen 0");
	PASS ("sd_strdup handles empty string");
	sd_sfree (h);

	h = sd_strdup (NULL);
	if (sd_strlen (h) != 0)
		FAIL ("NULL string dup should have strlen 0");
	PASS ("sd_strdup handles NULL");
	sd_sfree (h);
}

static void test_clone (void)
{
	int h1 = sd_strdup ("original");
	int h2 = sd_clone (h1);
	if (h2 <= 0)
		FAIL ("sd_clone returned invalid handle");
	PASS ("sd_clone creates valid handle");

	if (sd_strlen (h1) != sd_strlen (h2))
		FAIL ("cloned string length mismatch");
	PASS ("sd_clone preserves length");

	if (strcmp (sd_buf (h1), sd_buf (h2)) != 0)
		FAIL ("cloned string content mismatch");
	PASS ("sd_clone preserves content");

	sd_sfree (h1);
	sd_sfree (h2);
}

static void test_app (void)
{
	int h = sd_new ();
	sd_app1 (h, "hello");
	if (sd_strlen (h) != 5)
		FAIL ("expected strlen 5 after app1, got %d", sd_strlen (h));
	PASS ("sd_app1 works");

	sd_app1 (h, " world");
	if (sd_strlen (h) != 11)
		FAIL ("expected strlen 11 after second app1, got %d",
		      sd_strlen (h));
	PASS ("sd_app1 appends correctly");

	if (strcmp (sd_buf (h), "hello world") != 0)
		FAIL ("expected 'hello world', got '%s'", (char *)sd_buf (h));
	PASS ("sd_app1 content correct after append");

	sd_sfree (h);

	h = sd_app (0, "a", "b", "c", NULL);
	if (sd_strlen (h) != 3)
		FAIL ("expected strlen 3, got %d", sd_strlen (h));
	PASS ("sd_app works with multiple strings");

	if (strcmp (sd_buf (h), "abc") != 0)
		FAIL ("expected 'abc', got '%s'", (char *)sd_buf (h));
	PASS ("sd_app content correct");
	sd_sfree (h);
}

static void test_printf (void)
{
	int h = sd_new ();
	sd_printf (h, 0, "hello %d", 42);
	if (sd_strlen (h) != 8)
		FAIL ("expected strlen 8, got %d", sd_strlen (h));
	PASS ("sd_printf works");

	if (strcmp (sd_buf (h), "hello 42") != 0)
		FAIL ("expected 'hello 42', got '%s'", (char *)sd_buf (h));
	PASS ("sd_printf content correct");

	sd_sfree (h);
}

static void test_cat (void)
{
	int h = sd_strdup ("hello");
	sd_cat (h, " world");
	if (sd_strlen (h) != 11)
		FAIL ("expected strlen 11, got %d", sd_strlen (h));
	PASS ("sd_cat works");

	if (strcmp (sd_buf (h), "hello world") != 0)
		FAIL ("expected 'hello world', got '%s'", (char *)sd_buf (h));
	PASS ("sd_cat content correct");
	sd_sfree (h);
}

static void test_cmp (void)
{
	int h1 = sd_strdup ("abc");
	int h2 = sd_strdup ("abc");
	int h3 = sd_strdup ("abd");
	int h4 = sd_strdup ("ab");

	if (sd_cmp (h1, h2) != 0)
		FAIL ("identical strings should be equal");
	PASS ("sd_cmp works for equal strings");

	if (sd_cmp (h1, h3) >= 0)
		FAIL ("'abc' should be less than 'abd'");
	PASS ("sd_cmp works for less than");

	if (sd_cmp (h3, h1) <= 0)
		FAIL ("'abd' should be greater than 'abc'");
	PASS ("sd_cmp works for greater than");

	sd_sfree (h1);
	sd_sfree (h2);
	sd_sfree (h3);
	sd_sfree (h4);
}

static void test_ncmp (void)
{
	int h1 = sd_strdup ("abcdef");
	int h2 = sd_strdup ("abcxyz");

	if (sd_ncmp (h1, h2, 3) != 0)
		FAIL ("first 3 chars should be equal");
	PASS ("sd_ncmp works for equal prefix");

	if (sd_ncmp (h1, h2, 4) >= 0)
		FAIL ("first 4 chars should differ");
	PASS ("sd_ncmp works for differing prefix");

	sd_sfree (h1);
	sd_sfree (h2);
}

static void test_chr (void)
{
	int h = sd_strdup ("hello");

	if (sd_chr (h, 'l', 0) != 2)
		FAIL ("expected 2, got %d", sd_chr (h, 'l', 0));
	PASS ("sd_chr finds 'l' at position 2");

	if (sd_chr (h, 'x', 0) != -1)
		FAIL ("'x' not found, expected -1");
	PASS ("sd_chr returns -1 when not found");

	if (sd_chr (h, 'l', 3) != 3)
		FAIL ("expected 3, got %d", sd_chr (h, 'l', 3));
	PASS ("sd_chr respects offset");

	sd_sfree (h);
}

static void test_rchr (void)
{
	int h = sd_strdup ("hello");

	if (sd_rchr (h, 'l') != 3)
		FAIL ("expected 3, got %d", sd_rchr (h, 'l'));
	PASS ("sd_rchr finds last 'l' at position 3");

	if (sd_rchr (h, 'x') != -1)
		FAIL ("'x' not found, expected -1");
	PASS ("sd_rchr returns -1 when not found");

	sd_sfree (h);
}

static void test_find (void)
{
	int h = sd_strdup ("hello world");

	if (sd_find (h, "world") != 6)
		FAIL ("expected 6, got %d", sd_find (h, "world"));
	PASS ("sd_find finds substring");

	if (sd_find (h, "xyz") != -1)
		FAIL ("'xyz' not found, expected -1");
	PASS ("sd_find returns -1 when not found");

	sd_sfree (h);
}

static void test_prefix_suffix (void)
{
	int h = sd_strdup ("hello world");

	if (!sd_has_prefix (h, "hello"))
		FAIL ("should have 'hello' prefix");
	PASS ("sd_has_prefix works for existing prefix");

	if (sd_has_prefix (h, "world"))
		FAIL ("should not have 'world' prefix");
	PASS ("sd_has_prefix works for missing prefix");

	if (!sd_has_suffix (h, "world"))
		FAIL ("should have 'world' suffix");
	PASS ("sd_has_suffix works for existing suffix");

	if (sd_has_suffix (h, "hello"))
		FAIL ("should not have 'hello' suffix");
	PASS ("sd_has_suffix works for missing suffix");

	sd_sfree (h);
}

static void test_sub (void)
{
	int h = sd_strdup ("hello world");
	int sub = sd_sub (h, 0, 5);
	if (strcmp (sd_buf (sub), "hello") != 0)
		FAIL ("expected 'hello', got '%s'", (char *)sd_buf (sub));
	PASS ("sd_sub extracts prefix");

	sd_sfree (sub);
	sub = sd_sub (h, 6, 5);
	if (strcmp (sd_buf (sub), "world") != 0)
		FAIL ("expected 'world', got '%s'", (char *)sd_buf (sub));
	PASS ("sd_sub extracts substring");

	sd_sfree (sub);
	sub = sd_sub (h, -5, 5);
	if (strcmp (sd_buf (sub), "world") != 0)
		FAIL ("expected 'world', got '%s'", (char *)sd_buf (sub));
	PASS ("sd_sub handles negative position");

	sd_sfree (sub);
	sub = sd_sub (h, 0, -6);
	if (strcmp (sd_buf (sub), "hello") != 0)
		FAIL ("expected 'hello', got '%s'", (char *)sd_buf (sub));
	PASS ("sd_sub handles negative length");

	sd_sfree (sub);
	sd_sfree (h);
}

static void test_left_right (void)
{
	int h = sd_strdup ("hello");

	int left = sd_left (h, 3);
	if (strcmp (sd_buf (left), "hel") != 0)
		FAIL ("expected 'hel', got '%s'", (char *)sd_buf (left));
	PASS ("sd_left works");
	sd_sfree (left);

	int right = sd_right (h, 3);
	if (strcmp (sd_buf (right), "llo") != 0)
		FAIL ("expected 'llo', got '%s'", (char *)sd_buf (right));
	PASS ("sd_right works");
	sd_sfree (right);

	sd_sfree (h);
}

static void test_trim (void)
{
	int h = sd_strdup ("  hello  ");
	int trimmed = sd_trim (h);
	if (strcmp (sd_buf (trimmed), "hello") != 0)
		FAIL ("expected 'hello', got '%s'", (char *)sd_buf (trimmed));
	PASS ("sd_trim removes whitespace");
	sd_sfree (trimmed);
	sd_sfree (h);
}

static void test_lower_upper (void)
{
	int h = sd_strdup ("Hello World");
	int lower = sd_lower (sd_clone (h));
	if (strcmp (sd_buf (lower), "hello world") != 0)
		FAIL ("expected 'hello world', got '%s'",
		      (char *)sd_buf (lower));
	PASS ("sd_lower works");
	sd_sfree (lower);

	int upper = sd_upper (h);
	if (strcmp (sd_buf (upper), "HELLO WORLD") != 0)
		FAIL ("expected 'HELLO WORLD', got '%s'",
		      (char *)sd_buf (upper));
	PASS ("sd_upper works");
	sd_sfree (upper);
}

static void test_reverse (void)
{
	int h = sd_strdup ("hello");
	sd_reverse (h);
	if (strcmp (sd_buf (h), "olleh") != 0)
		FAIL ("expected 'olleh', got '%s'", (char *)sd_buf (h));
	PASS ("sd_reverse works");
	sd_sfree (h);
}

static void test_replace (void)
{
	int h = sd_strdup ("hello world");
	int replaced = sd_replace_c (h, "world", "there");
	if (strcmp (sd_buf (replaced), "hello there") != 0)
		FAIL ("expected 'hello there', got '%s'",
		      (char *)sd_buf (replaced));
	PASS ("sd_replace_c works");
	sd_sfree (replaced);
	sd_sfree (h);
}

static void test_split (void)
{
	int parts = sd_split (0, "a,b,c", ',', 0);
	if (sd_len (parts) != 3)
		FAIL ("expected 3 parts, got %d", sd_len (parts));
	PASS ("sd_split creates correct number of parts");

	char **p0 = sd_get (parts, 0);
	if (strcmp (*p0, "a") != 0)
		FAIL ("expected 'a', got '%s'", *p0);
	PASS ("sd_split first part correct");

	char **p1 = sd_get (parts, 1);
	if (strcmp (*p1, "b") != 0)
		FAIL ("expected 'b', got '%s'", *p1);
	PASS ("sd_split second part correct");

	sd_free_strings (parts, 0);
	sd_sfree (parts);
}

static void test_join (void)
{
	int h = sd_join (",", "a", "b", "c", NULL);
	if (strcmp (sd_buf (h), "a,b,c") != 0)
		FAIL ("expected 'a,b,c', got '%s'", (char *)sd_buf (h));
	PASS ("sd_join works");
	sd_sfree (h);
}

static void test_from_long (void)
{
	int h = sd_from_long (12345);
	if (strcmp (sd_buf (h), "12345") != 0)
		FAIL ("expected '12345', got '%s'", (char *)sd_buf (h));
	PASS ("sd_from_long works");
	sd_sfree (h);
}

static void test_to_long (void)
{
	int h = sd_strdup ("67890");
	long val = sd_to_long (h);
	if (val != 67890)
		FAIL ("expected 67890, got %ld", val);
	PASS ("sd_to_long works");
	sd_sfree (h);
}

static void test_hash (void)
{
	int h = sd_strdup ("test");
	uint32_t hash = sd_hash (h);
	if (hash == 0)
		FAIL ("hash should not be 0");
	PASS ("sd_hash returns non-zero");
	sd_sfree (h);
}

static void test_is_numeric (void)
{
	int h = sd_strdup ("12345");
	if (!sd_is_numeric (h))
		FAIL ("'12345' should be numeric");
	PASS ("sd_is_numeric works for numeric string");

	sd_clear (h);
	sd_cat (h, "123ab");
	if (sd_is_numeric (h))
		FAIL ("'123ab' should not be numeric");
	PASS ("sd_is_numeric works for non-numeric string");
	sd_sfree (h);
}

static void test_is_alpha (void)
{
	int h = sd_strdup ("hello");
	if (!sd_is_alpha (h))
		FAIL ("'hello' should be alpha");
	PASS ("sd_is_alpha works for alphabetic string");

	sd_clear (h);
	sd_cat (h, "hello1");
	if (sd_is_alpha (h))
		FAIL ("'hello1' should not be alpha");
	PASS ("sd_is_alpha works for non-alphabetic string");
	sd_sfree (h);
}

static void test_is_empty (void)
{
	int h = sd_new ();
	if (!sd_is_empty (h))
		FAIL ("empty buffer should be empty");
	PASS ("sd_is_empty works for empty buffer");

	sd_cat (h, "");
	if (!sd_is_empty (h))
		FAIL ("'' should be empty");
	PASS ("sd_is_empty works for zero-length string");

	sd_cat (h, "a");
	if (sd_is_empty (h))
		FAIL ("'a' should not be empty");
	PASS ("sd_is_empty works for non-empty buffer");
	sd_sfree (h);
}

static void test_casecmp (void)
{
	int h1 = sd_strdup ("Hello");
	int h2 = sd_strdup ("hello");
	if (sd_casecmp (h1, h2) != 0)
		FAIL ("case-insensitive compare should be equal");
	PASS ("sd_casecmp works");
	sd_sfree (h1);
	sd_sfree (h2);
}

static void test_pad (void)
{
	int h = sd_strdup ("ab");
	int padded = sd_pad_left (h, 5, '0');
	if (strcmp (sd_buf (padded), "000ab") != 0)
		FAIL ("expected '000ab', got '%s'", (char *)sd_buf (padded));
	PASS ("sd_pad_left works");
	sd_sfree (padded);

	padded = sd_pad_right (h, 5, ' ');
	if (strcmp (sd_buf (padded), "ab   ") != 0)
		FAIL ("expected 'ab   ', got '%s'", (char *)sd_buf (padded));
	PASS ("sd_pad_right works");
	sd_sfree (padded);
	sd_sfree (h);
}

static void test_slice (void)
{
	int src = sd_strdup ("hello world");
	int dest = sd_new ();
	sd_slice (dest, 0, src, 0, 4);
	if (strcmp (sd_buf (dest), "hello") != 0)
		FAIL ("expected 'hello', got '%s'", (char *)sd_buf (dest));
	PASS ("sd_slice copies correctly");

	sd_sfree (src);
	sd_sfree (dest);
}

static void test_dup_array (void)
{
	int arr = sd_alloc (10, sizeof (int), 0);
	int vals[] = {5, 2, 8, 1, 9};
	for (int i = 0; i < 5; i++) {
		sd_put (arr, &vals[i]);
	}

	int copy = sd_dup (arr);
	if (sd_len (copy) != sd_len (arr))
		FAIL ("length mismatch after dup");
	PASS ("sd_dup preserves length");

	for (int i = 0; i < 5; i++) {
		int *orig = sd_get (arr, i);
		int *c = sd_get (copy, i);
		if (*orig != *c)
			FAIL ("element %d mismatch: %d vs %d", i, *orig, *c);
	}
	PASS ("sd_dup preserves content");

	sd_free (arr);
	sd_free (copy);
}

static void test_qsort (void)
{
	int arr = sd_alloc (5, sizeof (int), 0);
	int vals[] = {5, 2, 8, 1, 9};
	for (int i = 0; i < 5; i++)
		sd_put (arr, &vals[i]);

	sd_qsort (arr, cmp_int);

	if (*(int *)sd_get (arr, 0) != 1)
		FAIL ("first element should be 1, got %d",
		      *(int *)sd_get (arr, 0));
	PASS ("sd_qsort sorts correctly");
	sd_free (arr);
}

int cmp_int (const void *a, const void *b) { return (*(int *)a) - (*(int *)b); }

static void test_bsearch (void)
{
	int arr = sd_alloc (5, sizeof (int), 0);
	int vals[] = {1, 3, 5, 7, 9};
	for (int i = 0; i < 5; i++)
		sd_put (arr, &vals[i]);

	int key = 5;
	int idx = sd_bsearch (&key, arr, cmp_int);
	if (idx != 2)
		FAIL ("expected 2, got %d", idx);
	PASS ("sd_bsearch finds element");

	key = 4;
	idx = sd_bsearch (&key, arr, cmp_int);
	if (idx != -1)
		FAIL ("expected -1, got %d", idx);
	PASS ("sd_bsearch returns -1 for missing element");

	sd_free (arr);
}

static void test_free_strings (void)
{
	int arr = sd_alloc (3, sizeof (char *), SD_FREE_STRINGS);
	char *s1 = strdup ("hello");
	char *s2 = strdup ("world");
	char *s3 = strdup ("test");

	sd_put (arr, &s1);
	sd_put (arr, &s2);
	sd_put (arr, &s3);

	sd_free_strings (arr, 0);
	PASS ("sd_free_strings frees and releases array");
}

int main (void)
{
	printf ("SDS String Library Test Suite\n");
	printf ("================================\n\n");

	sd_init ();

	RUN_TEST (test_new_free);
	RUN_TEST (test_strlen);
	RUN_TEST (test_dup);
	RUN_TEST (test_clone);
	RUN_TEST (test_app);
	RUN_TEST (test_printf);
	RUN_TEST (test_cat);
	RUN_TEST (test_cmp);
	RUN_TEST (test_ncmp);
	RUN_TEST (test_chr);
	RUN_TEST (test_rchr);
	RUN_TEST (test_find);
	RUN_TEST (test_prefix_suffix);
	RUN_TEST (test_sub);
	RUN_TEST (test_left_right);
	RUN_TEST (test_trim);
	RUN_TEST (test_lower_upper);
	RUN_TEST (test_reverse);
	RUN_TEST (test_replace);
	RUN_TEST (test_split);
	RUN_TEST (test_join);
	RUN_TEST (test_from_long);
	RUN_TEST (test_to_long);
	RUN_TEST (test_hash);
	RUN_TEST (test_is_numeric);
	RUN_TEST (test_is_alpha);
	RUN_TEST (test_is_empty);
	RUN_TEST (test_casecmp);
	RUN_TEST (test_pad);
	RUN_TEST (test_slice);
	RUN_TEST (test_dup_array);
	RUN_TEST (test_qsort);
	RUN_TEST (test_bsearch);
	RUN_TEST (test_free_strings);

	sd_destruct ();

	printf ("\n================================\n");
	printf ("All %d tests PASSED\n", tests_run);

	return 0;
}
