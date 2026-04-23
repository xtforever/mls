#include "sd.h"
#include "sd_ser.h"
#include "sd_strings.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

static void test_ser_strings (void)
{
	int s1 = sd_strdup ("hello");
	int s2 = sd_strdup ("world");
	int s3 = sd_strdup ("test");

	int original = sd_alloc (0, sizeof (int), 0);
	sd_put (original, &s1);
	sd_put (original, &s2);
	sd_put (original, &s3);

	if (sd_len (original) != 3)
		FAIL ("original has wrong length");
	PASS ("original list created with 3 strings");

	int hdf = hdf_parse_string ("(root)");
	if (hdf <= 0)
		FAIL ("failed to create HDF root");

	if (sd_serialize (original, hdf, "strings") < 0)
		FAIL ("serialize failed");
	PASS ("serialize to HDF succeeded");

	int hdf_out = sd_serialize_to_string (original);
	if (hdf_out <= 0)
		FAIL ("serialize to string failed");
	printf ("    Serialized:\n%s\n", (char *)sd_buf (hdf_out));
	sd_sfree (hdf_out);

	int restored = 0;
	if (sd_deserialize (hdf, "strings", &restored) < 0)
		FAIL ("deserialize failed");
	PASS ("deserialize from HDF succeeded");

	if (sd_len (restored) != 3)
		FAIL ("restored has wrong length %d", sd_len (restored));
	PASS ("restored list has correct length");

	int *p = sd_get (restored, 0);
	int r1 = *(int *)p;
	p = sd_get (restored, 1);
	int r2 = *(int *)p;
	p = sd_get (restored, 2);
	int r3 = *(int *)p;

	if (sd_cmp (r1, s1) != 0)
		FAIL ("first element mismatch");
	PASS ("first element matches");

	if (sd_cmp (r2, s2) != 0)
		FAIL ("second element mismatch");
	PASS ("second element matches");

	if (sd_cmp (r3, s3) != 0)
		FAIL ("third element mismatch");
	PASS ("third element matches");

	sd_free (restored);
	hdf_free (hdf);
	sd_free (original);
	sd_sfree (s1);
	sd_sfree (s2);
	sd_sfree (s3);
	PASS ("strings serialization test cleanup");
}

static void test_ser_file_roundtrip (void)
{
	int s1 = sd_strdup ("file");
	int s2 = sd_strdup ("test");
	int s3 = sd_strdup ("data");

	int original = sd_alloc (0, sizeof (int), 0);
	sd_put (original, &s1);
	sd_put (original, &s2);
	sd_put (original, &s3);

	if (sd_serialize_file (original, "/tmp/test_ser.hdf") < 0)
		FAIL ("serialize to file failed");
	PASS ("serialize to file succeeded");

	int restored = sd_deserialize_file ("/tmp/test_ser.hdf");
	if (restored <= 0)
		FAIL ("deserialize from file failed");
	PASS ("deserialize from file succeeded");

	if (sd_len (restored) != sd_len (original))
		FAIL ("length mismatch");
	PASS ("restored has correct length");

	for (int i = 0; i < sd_len (original); i++) {
		int *rp = sd_get (restored, i);
		int *op = sd_get (original, i);
		int rval = *(int *)rp;
		int oval = *(int *)op;
		if (sd_cmp (rval, oval) != 0)
			FAIL ("element %d mismatch", i);
	}
	PASS ("all elements match");

	sd_free (restored);
	sd_free (original);
	sd_sfree (s1);
	sd_sfree (s2);
	sd_sfree (s3);
	PASS ("file roundtrip test cleanup");
}

static void test_ser_nested (void)
{
	int si1 = sd_strdup ("inner1");
	int si2 = sd_strdup ("inner2");

	int inner = sd_alloc (0, sizeof (int), 0);
	sd_put (inner, &si1);
	sd_put (inner, &si2);

	int outer = sd_alloc (0, sizeof (int), 0);
	sd_put (outer, &inner);

	int hdf = hdf_parse_string ("(root)");
	if (hdf <= 0)
		FAIL ("failed to create HDF root");

	if (sd_serialize (outer, hdf, "nested") < 0)
		FAIL ("serialize nested failed");
	PASS ("serialize nested list succeeded");

	int restored = 0;
	if (sd_deserialize (hdf, "nested", &restored) < 0)
		FAIL ("deserialize nested failed");
	PASS ("deserialize nested succeeded");

	if (sd_len (restored) != 1)
		FAIL ("restored nested should have 1 element");
	PASS ("restored has correct length");

	int *rp = sd_get (restored, 0);
	int inner_restored = *rp;
	if (sd_len (inner_restored) != 2)
		FAIL ("inner list should have 2 elements");
	PASS ("inner list has correct length");

	sd_free (inner_restored);
	sd_free (restored);
	hdf_free (hdf);
	sd_free (outer);
	sd_sfree (si1);
	sd_sfree (si2);
	PASS ("nested serialization test cleanup");
}

static void test_ser_raw_bytes (void)
{
	int raw = sd_alloc (3, 4, SD_FREE);
	int v1 = 0x12345678;
	int v2 = 0xABCDEF00;
	int v3 = 0xDEADBEEF;

	sd_put (raw, &v1);
	sd_put (raw, &v2);
	sd_put (raw, &v3);

	int hdf = hdf_parse_string ("(root)");
	if (hdf <= 0)
		FAIL ("failed to create HDF root");

	if (sd_serialize (raw, hdf, "bytes") < 0)
		FAIL ("serialize raw bytes failed");
	PASS ("serialize raw bytes succeeded");

	int restored = 0;
	if (sd_deserialize (hdf, "bytes", &restored) < 0)
		FAIL ("deserialize raw bytes failed");
	PASS ("deserialize raw bytes succeeded");

	if (sd_len (restored) != 3)
		FAIL ("wrong length");
	PASS ("restored has correct length");

	for (int i = 0; i < 3; i++) {
		int *orig = sd_get (raw, i);
		int *rest = sd_get (restored, i);
		if (*orig != *rest)
			FAIL ("element %d mismatch: 0x%08x vs 0x%08x", i, *orig,
			      *rest);
	}
	PASS ("all raw bytes match");

	sd_free (restored);
	hdf_free (hdf);
	sd_free (raw);
	PASS ("raw bytes serialization test cleanup");
}

static void test_ser_multiple (void)
{
	int s1 = sd_strdup ("one");
	int list1 = sd_alloc (0, sizeof (int), 0);
	sd_put (list1, &s1);

	int s2 = sd_strdup ("two");
	int s3 = sd_strdup ("three");
	int list2 = sd_alloc (0, sizeof (int), 0);
	sd_put (list2, &s2);
	sd_put (list2, &s3);

	int hdf = hdf_parse_string ("(root)");
	if (hdf <= 0)
		FAIL ("failed to create HDF root");

	if (sd_serialize (list1, hdf, "first") < 0)
		FAIL ("serialize first list failed");
	PASS ("serialize first list succeeded");

	if (sd_serialize (list2, hdf, "second") < 0)
		FAIL ("serialize second list failed");
	PASS ("serialize second list succeeded");

	int rest1 = 0;
	if (sd_deserialize (hdf, "first", &rest1) < 0)
		FAIL ("deserialize first failed");
	PASS ("deserialize first succeeded");

	int rest2 = 0;
	if (sd_deserialize (hdf, "second", &rest2) < 0)
		FAIL ("deserialize second failed");
	PASS ("deserialize second succeeded");

	if (sd_len (rest1) != 1)
		FAIL ("first should have 1 element");
	PASS ("first has correct length");

	if (sd_len (rest2) != 2)
		FAIL ("second should have 2 elements");
	PASS ("second has correct length");

	sd_free (rest1);
	sd_free (rest2);
	hdf_free (hdf);
	sd_free (list1);
	sd_free (list2);
	sd_sfree (s1);
	sd_sfree (s2);
	sd_sfree (s3);
	PASS ("multiple lists serialization test cleanup");
}

int main (void)
{
	printf ("SDS Serialization Library Test Suite\n");
	printf ("=====================================\n\n");

	sd_init ();
	sd_register_printf ();

	RUN_TEST (test_ser_strings);
	RUN_TEST (test_ser_file_roundtrip);
	RUN_TEST (test_ser_nested);
	RUN_TEST (test_ser_raw_bytes);
	RUN_TEST (test_ser_multiple);

	sd_destruct ();

	printf ("\n=====================================\n");
	printf ("All %d tests PASSED\n", tests_run);

	unlink ("/tmp/test_ser.hdf");

	return 0;
}
