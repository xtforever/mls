#include "sd.h"
#include "sd_hdf.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void test_hdf_basic (void)
{
	const char *data = "(server\n"
			   "  (port 8080)\n"
			   "  (host \"localhost\")\n"
			   "  (debug true)\n"
			   "  (raw [[multi\nline]])\n"
			   ")";

	int root = hdf_parse_string (data);
	if (root <= 0)
		FAIL ("failed to parse HDF string");
	PASS ("hdf_parse_string returns valid root");

	if (hdf_get_type (root) != HDF_TYPE_LIST)
		FAIL ("root should be LIST type");
	PASS ("root type is LIST");

	int children = hdf_get_children (root);
	if (children <= 0)
		FAIL ("failed to get children");
	PASS ("hdf_get_children returns valid handle");

	if (sd_len (children) != 1)
		FAIL ("expected 1 child (server), got %d", sd_len (children));
	PASS ("correct number of root children (1)");

	int *child_data = sd_buf (children);
	int server = child_data[0];
	if (hdf_get_type (server) != HDF_TYPE_LIST)
		FAIL ("server should be LIST type");
	PASS ("server type is LIST");

	int server_children = hdf_get_children (server);
	if (sd_len (server_children) != 5)
		FAIL ("expected 5 server children, got %d",
		      sd_len (server_children));
	PASS ("correct number of server children (5)");

	const char *port = hdf_get_property (server, "port");
	if (!port)
		FAIL ("port property is NULL");
	if (strcmp (port, "8080") != 0)
		FAIL ("port should be '8080', got '%s'", port);
	PASS ("hdf_get_property returns port '8080'");

	const char *host = hdf_get_property (server, "host");
	if (!host)
		FAIL ("host property is NULL");
	if (strcmp (host, "localhost") != 0)
		FAIL ("host should be 'localhost', got '%s'", host);
	PASS ("hdf_get_property returns host 'localhost'");

	const char *debug = hdf_get_property (server, "debug");
	if (!debug)
		FAIL ("debug property is NULL");
	if (strcmp (debug, "true") != 0)
		FAIL ("debug should be 'true', got '%s'", debug);
	PASS ("hdf_get_property returns debug 'true'");

	int raw_node = hdf_find_node (server, "raw");
	if (raw_node <= 0)
		FAIL ("raw node not found");
	PASS ("hdf_find_node finds raw node");

	int raw_children = hdf_get_children (raw_node);
	if (raw_children <= 0)
		FAIL ("raw node has no children");
	int *raw_data = sd_buf (raw_children);
	const char *raw_val = hdf_get_value (raw_data[1]);
	if (!raw_val)
		FAIL ("raw value is NULL");
	if (strcmp (raw_val, "multi\nline") != 0)
		FAIL ("raw should be 'multi\\nline', got '%s'", raw_val);
	PASS ("raw string parsing works");

	hdf_free (root);
	PASS ("hdf_free completes without error");
}

static void test_hdf_comments (void)
{
	const char *data = "; comment at start\n"
			   "(config\n"
			   "  (name \"test\")\n"
			   "  ; inline comment\n"
			   "  (value 42)\n"
			   ")";

	int root = hdf_parse_string (data);
	if (root <= 0)
		FAIL ("failed to parse HDF with comments");
	PASS ("hdf_parse_string handles comments");

	int config = hdf_find_node (root, "config");
	if (config <= 0)
		FAIL ("config node not found");
	PASS ("hdf_find_node works with comments");

	const char *name = hdf_get_property (config, "name");
	if (!name || strcmp (name, "test") != 0)
		FAIL ("name should be 'test'");
	PASS ("comments are properly skipped");

	int value = hdf_get_int (config, "value", 0);
	if (value != 42)
		FAIL ("value should be 42, got %d", value);
	PASS ("hdf_get_int works");

	hdf_free (root);
	PASS ("hdf_free completes");
}

static void test_hdf_nested (void)
{
	const char *data = "(root\n"
			   "  (parent\n"
			   "    (child1 (x 1))\n"
			   "    (child2 (y 2))\n"
			   "  )\n"
			   ")";

	int root = hdf_parse_string (data);
	if (root <= 0)
		FAIL ("failed to parse nested HDF");
	PASS ("nested HDF parsed successfully");

	int children = hdf_get_children (root);
	if (sd_len (children) != 1)
		FAIL ("expected 1 root child");
	PASS ("root has correct number of children");

	int *cdata = sd_buf (children);
	int root_node = cdata[0];

	int parent = hdf_find_node (root_node, "parent");
	if (parent <= 0)
		FAIL ("parent node not found");
	PASS ("hdf_find_node finds nested node");

	int child1 = hdf_find_node (parent, "child1");
	if (child1 <= 0)
		FAIL ("child1 node not found");
	PASS ("hdf_find_node works in nested context");

	int x = hdf_get_int (child1, "x", 0);
	if (x != 1)
		FAIL ("x should be 1, got %d", x);
	PASS ("hdf_get_int works in nested context");

	hdf_free (root);
	PASS ("nested hdf_free completes");
}

static void test_hdf_boolean_null (void)
{
	const char *data = "(test\n"
			   "  (flag true)\n"
			   "  (disabled false)\n"
			   "  (empty null)\n"
			   ")";

	int root = hdf_parse_string (data);
	if (root <= 0)
		FAIL ("failed to parse boolean/null HDF");
	PASS ("HDF with booleans parsed");

	int children = hdf_get_children (root);
	int *cdata = sd_buf (children);
	int test_node = cdata[0];

	int flag = hdf_get_bool (test_node, "flag", -1);
	if (flag != 1)
		FAIL ("flag should be true (1), got %d", flag);
	PASS ("hdf_get_bool returns 1 for true");

	int disabled = hdf_get_bool (test_node, "disabled", -1);
	if (disabled != 0)
		FAIL ("disabled should be false (0), got %d", disabled);
	PASS ("hdf_get_bool returns 0 for false");

	const char *empty = hdf_get_property (test_node, "empty");
	if (!empty)
		FAIL ("empty property is NULL");
	if (strcmp (empty, "null") != 0)
		FAIL ("empty should be 'null', got '%s'", empty);
	PASS ("null literal parsed correctly");

	hdf_free (root);
	PASS ("boolean/null hdf_free completes");
}

static void test_hdf_numbers (void)
{
	const char *data = "(nums\n"
			   "  (int -42)\n"
			   "  (float 3.14)\n"
			   "  (zero 0)\n"
			   ")";

	int root = hdf_parse_string (data);
	if (root <= 0)
		FAIL ("failed to parse number HDF");
	PASS ("HDF with numbers parsed");

	int children = hdf_get_children (root);
	int *cdata = sd_buf (children);
	int nums = cdata[0];

	int int_val = hdf_get_int (nums, "int", 0);
	if (int_val != -42)
		FAIL ("int should be -42, got %d", int_val);
	PASS ("negative integers work");

	int float_val = hdf_get_int (nums, "float", 0);
	if (float_val != 3)
		FAIL ("float should truncate to 3, got %d", float_val);
	PASS ("floating point numbers work");

	int zero_val = hdf_get_int (nums, "zero", -1);
	if (zero_val != 0)
		FAIL ("zero should be 0, got %d", zero_val);
	PASS ("zero works");

	hdf_free (root);
	PASS ("number hdf_free completes");
}

static void test_hdf_quoted_strings (void)
{
	const char *data = "(test\n"
			   "  (escape \"hello\\nworld\\t!\")\n"
			   "  (quote \"say \\\"hi\\\"\")\n"
			   ")";

	int root = hdf_parse_string (data);
	if (root <= 0)
		FAIL ("failed to parse escaped strings");
	PASS ("escaped strings parsed");

	int children = hdf_get_children (root);
	int *cdata = sd_buf (children);
	int test = cdata[0];

	const char *escape = hdf_get_property (test, "escape");
	if (!escape)
		FAIL ("escape property is NULL");
	if (strcmp (escape, "hello\nworld\t!") != 0)
		FAIL ("escape mismatch: got '%s'", escape);
	PASS ("escape sequences work (\\n, \\t)");

	const char *quote = hdf_get_property (test, "quote");
	if (!quote)
		FAIL ("quote property is NULL");
	if (strcmp (quote, "say \"hi\"") != 0)
		FAIL ("quote mismatch: got '%s'", quote);
	PASS ("quoted strings with escaped quotes work");

	hdf_free (root);
	PASS ("escaped string hdf_free completes");
}

static void test_hdf_rem (void)
{
	const char *data = "(rem \"This is a comment\")\n"
			   "(config\n"
			   "  (rem skip this)\n"
			   "  (value 123)\n"
			   ")";

	int root = hdf_parse_string (data);
	if (root <= 0)
		FAIL ("failed to parse HDF with rem");
	PASS ("HDF with rem parsed");

	int children = hdf_get_children (root);
	if (sd_len (children) != 1)
		FAIL ("rem should be skipped at root, expected 1 child, got %d",
		      sd_len (children));
	PASS ("rem at root level is properly skipped");

	int config = hdf_find_node (root, "config");
	if (config <= 0)
		FAIL ("config not found");
	PASS ("config node found");

	int value = hdf_get_int (config, "value", 0);
	if (value != 123)
		FAIL ("value should be 123, got %d", value);
	PASS ("properties after rem are accessible");

	hdf_free (root);
	PASS ("rem hdf_free completes");
}

static void test_hdf_not_found (void)
{
	const char *data = "(test (a 1))";

	int root = hdf_parse_string (data);
	if (root <= 0)
		FAIL ("failed to parse simple HDF");

	const char *missing = hdf_get_property (root, "nonexistent");
	if (missing != NULL)
		FAIL ("nonexistent property should return NULL");
	PASS ("hdf_get_property returns NULL for missing property");

	int node = hdf_find_node (root, "nonexistent");
	if (node > 0)
		FAIL ("nonexistent node should return -1");
	PASS ("hdf_find_node returns -1 for missing node");

	int missing_int = hdf_get_int (root, "nonexistent", 999);
	if (missing_int != 999)
		FAIL ("missing int should return default 999, got %d",
		      missing_int);
	PASS ("hdf_get_int returns default for missing property");

	int missing_bool = hdf_get_bool (root, "nonexistent", 42);
	if (missing_bool != 42)
		FAIL ("missing bool should return default 42, got %d",
		      missing_bool);
	PASS ("hdf_get_bool returns default for missing property");

	hdf_free (root);
	PASS ("not-found hdf_free completes");
}

static void test_hdf_type (void)
{
	const char *data = "(test\n"
			   "  (str \"hello\")\n"
			   "  (num 42)\n"
			   "  (bool true)\n"
			   "  (lst (a 1) (b 2))\n"
			   ")";

	int root = hdf_parse_string (data);
	if (root <= 0)
		FAIL ("failed to parse HDF");
	PASS ("HDF parsed for type tests");

	int str_node = hdf_find_node (root, "str");
	if (hdf_get_type (str_node) != HDF_TYPE_LIST)
		FAIL ("str node should be LIST type");
	PASS ("hdf_find_node returns LIST for string property");

	int str_children = hdf_get_children (str_node);
	int *str_data = sd_buf (str_children);
	if (hdf_get_type (str_data[1]) != HDF_TYPE_STRING)
		FAIL ("str value should be STRING type");
	PASS ("HDF_TYPE_STRING works");

	int num_node = hdf_find_node (root, "num");
	if (hdf_get_type (num_node) != HDF_TYPE_LIST)
		FAIL ("num node should be LIST type");
	PASS ("hdf_find_node returns LIST for number property");

	int num_children = hdf_get_children (num_node);
	int *num_data = sd_buf (num_children);
	if (hdf_get_type (num_data[1]) != HDF_TYPE_NUMBER)
		FAIL ("num value should be NUMBER type");
	PASS ("HDF_TYPE_NUMBER works");

	int bool_node = hdf_find_node (root, "bool");
	if (hdf_get_type (bool_node) != HDF_TYPE_LIST)
		FAIL ("bool node should be LIST type");
	PASS ("hdf_find_node returns LIST for boolean property");

	int bool_children = hdf_get_children (bool_node);
	int *bool_data = sd_buf (bool_children);
	if (hdf_get_type (bool_data[1]) != HDF_TYPE_BOOLEAN)
		FAIL ("bool value should be BOOLEAN type");
	PASS ("HDF_TYPE_BOOLEAN works");

	int lst_node = hdf_find_node (root, "lst");
	if (hdf_get_type (lst_node) != HDF_TYPE_LIST)
		FAIL ("lst should be LIST type");
	PASS ("HDF_TYPE_LIST works");

	hdf_free (root);
	PASS ("type hdf_free completes");
}

static void test_hdf_file_basic (void)
{
	int root = hdf_parse_file ("tests/sd_hdf_basic.hdf");
	if (root <= 0)
		FAIL ("failed to parse sd_hdf_basic.hdf");
	PASS ("hdf_parse_file loads basic.hdf");

	int children = hdf_get_children (root);
	int *cdata = sd_buf (children);
	int server = cdata[0];

	const char *port = hdf_get_property (server, "port");
	if (!port || strcmp (port, "8080") != 0)
		FAIL ("port should be '8080'");
	PASS ("port property loaded correctly");

	const char *host = hdf_get_property (server, "host");
	if (!host || strcmp (host, "localhost") != 0)
		FAIL ("host should be 'localhost'");
	PASS ("host property loaded correctly");

	int debug = hdf_get_bool (server, "debug", -1);
	if (debug != 1)
		FAIL ("debug should be true");
	PASS ("debug property loaded correctly");

	hdf_free (root);
	PASS ("basic file hdf_free completes");
}

static void test_hdf_file_nested (void)
{
	int root = hdf_parse_file ("tests/sd_hdf_nested.hdf");
	if (root <= 0)
		FAIL ("failed to parse sd_hdf_nested.hdf");
	PASS ("hdf_parse_file loads nested.hdf");

	int config = hdf_find_node (root, "config");
	if (config <= 0)
		FAIL ("config node not found");
	PASS ("config node found");

	int db = hdf_find_node (config, "database");
	if (db <= 0)
		FAIL ("database node not found");
	PASS ("database node found");

	const char *db_host = hdf_get_property (db, "host");
	if (!db_host || strcmp (db_host, "db.example.com") != 0)
		FAIL ("database host mismatch");
	PASS ("database host loaded correctly");

	int port = hdf_get_int (db, "port", 0);
	if (port != 5432)
		FAIL ("database port should be 5432");
	PASS ("database port loaded correctly");

	int creds = hdf_find_node (db, "credentials");
	if (creds <= 0)
		FAIL ("credentials node not found");
	PASS ("credentials node found");

	const char *user = hdf_get_property (creds, "user");
	if (!user || strcmp (user, "admin") != 0)
		FAIL ("credentials user mismatch");
	PASS ("credentials user loaded correctly");

	hdf_free (root);
	PASS ("nested file hdf_free completes");
}

static void test_hdf_file_comments (void)
{
	int root = hdf_parse_file ("tests/sd_hdf_comments.hdf");
	if (root <= 0)
		FAIL ("failed to parse sd_hdf_comments.hdf");
	PASS ("hdf_parse_file loads comments.hdf");

	const char *name = hdf_get_property (root, "app_name");
	if (!name || strcmp (name, "Test App") != 0)
		FAIL ("app_name mismatch");
	PASS ("app_name loaded correctly");

	const char *ver = hdf_get_property (root, "version");
	if (!ver || strcmp (ver, "1.0.0") != 0)
		FAIL ("version mismatch");
	PASS ("version loaded correctly");

	int db = hdf_find_node (root, "database");
	if (db <= 0)
		FAIL ("database node not found");
	PASS ("database node found (comments skipped)");

	hdf_free (root);
	PASS ("comments file hdf_free completes");
}

static void test_hdf_file_arrays (void)
{
	int root = hdf_parse_file ("tests/sd_hdf_arrays.hdf");
	if (root <= 0)
		FAIL ("failed to parse sd_hdf_arrays.hdf");
	PASS ("hdf_parse_file loads arrays.hdf");

	int users = hdf_find_node (root, "users");
	if (users <= 0)
		FAIL ("users node not found");
	PASS ("users node found");

	int alice = hdf_find_node (users, "alice");
	if (alice <= 0)
		FAIL ("alice node not found");
	PASS ("alice node found");

	const char *role = hdf_get_property (alice, "role");
	if (!role || strcmp (role, "admin") != 0)
		FAIL ("alice role should be admin");
	PASS ("alice role loaded correctly");

	int routes = hdf_find_node (root, "routes");
	if (routes <= 0)
		FAIL ("routes node not found");
	PASS ("routes node found");

	int api = hdf_find_node (routes, "api");
	if (api <= 0)
		FAIL ("api route not found");
	PASS ("api route found");

	const char *path = hdf_get_property (api, "path");
	if (!path || strcmp (path, "/api/v1") != 0)
		FAIL ("api path mismatch");
	PASS ("api path loaded correctly");

	hdf_free (root);
	PASS ("arrays file hdf_free completes");
}

static void test_hdf_file_special (void)
{
	int root = hdf_parse_file ("tests/sd_hdf_special.hdf");
	if (root <= 0)
		FAIL ("failed to parse sd_hdf_special.hdf");
	PASS ("hdf_parse_file loads special.hdf");

	int settings = hdf_find_node (root, "settings");
	if (settings <= 0)
		FAIL ("settings node not found");
	PASS ("settings node found");

	int enabled = hdf_get_bool (settings, "enabled", -1);
	if (enabled != 1)
		FAIL ("enabled should be true");
	PASS ("enabled = true");

	int disabled = hdf_get_bool (settings, "disabled", -1);
	if (disabled != 0)
		FAIL ("disabled should be false");
	PASS ("disabled = false");

	int timeout = hdf_get_int (settings, "timeout", 0);
	if (timeout != 30)
		FAIL ("timeout should be 30");
	PASS ("timeout = 30");

	const char *empty = hdf_get_property (settings, "empty");
	if (!empty || strcmp (empty, "null") != 0)
		FAIL ("empty should be 'null'");
	PASS ("empty = null");

	hdf_free (root);
	PASS ("special file hdf_free completes");
}

static void test_hdf_parse_all_files (void)
{
	const char *files[] = {
		"tests/sd_hdf_basic.hdf",    "tests/sd_hdf_nested.hdf",
		"tests/sd_hdf_comments.hdf", "tests/sd_hdf_arrays.hdf",
		"tests/sd_hdf_special.hdf",  "tests/flask_test.hdf",
		"tests/flask_adv.hdf",	     NULL};

	for (int i = 0; files[i] != NULL; i++) {
		int root = hdf_parse_file (files[i]);
		if (root <= 0)
			FAIL ("failed to parse %s", files[i]);
		hdf_free (root);
	}
	PASS ("all %d HDF files parsed successfully", 7);
}

int main (void)
{
	printf ("SDS HDF Library Test Suite\n");
	printf ("============================\n\n");

	sd_init ();

	RUN_TEST (test_hdf_basic);
	RUN_TEST (test_hdf_comments);
	RUN_TEST (test_hdf_nested);
	RUN_TEST (test_hdf_boolean_null);
	RUN_TEST (test_hdf_numbers);
	RUN_TEST (test_hdf_quoted_strings);
	RUN_TEST (test_hdf_rem);
	RUN_TEST (test_hdf_not_found);
	RUN_TEST (test_hdf_type);
	RUN_TEST (test_hdf_file_basic);
	RUN_TEST (test_hdf_file_nested);
	RUN_TEST (test_hdf_file_comments);
	RUN_TEST (test_hdf_file_arrays);
	RUN_TEST (test_hdf_file_special);
	RUN_TEST (test_hdf_parse_all_files);

	sd_destruct ();

	printf ("\n============================\n");
	printf ("All %d tests PASSED\n", tests_run);

	return 0;
}
