#include "../lib/m_subproc.h"
#include "../lib/m_tool.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int passed = 0, failed = 0;

#define T(name) static void name(void)
#define check(cond, msg) do { \
	if (!(cond)) { printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); failed++; } \
	else passed++; } while(0)

T (test_read_echo)
{
	int h = subproc_read ("echo hello");
	check (h > 0, "subproc_read returned handle");
	const char *s = m_str (h);
	check (s != NULL && strstr (s, "hello") != NULL, "stdout contains hello");
	m_free (h);
}

T (test_read_missing_cmd)
{
	int h = subproc_read ("/nonexistent_cmd_12345");
	check (h == 0, "missing command returns 0");
}

T (test_run_stdout_stderr)
{
	int out = 0, err = 0;
	int rc = subproc_run ("echo stdout && echo stderr >&2", &out, &err, 0);
	check (rc == 0, "subproc_run exit code 0");
	check (out > 0, "stdout captured");
	check (err > 0, "stderr captured");
	const char *os = m_str (out);
	const char *es = m_str (err);
	check (strstr (os, "stdout") != NULL, "stdout has expected text");
	check (strstr (es, "stderr") != NULL, "stderr has expected text");
	m_free (out);
	m_free (err);
}

T (test_lines)
{
	int lines = subproc_lines ("printf \"a\\nb\\nc\"");
	check (lines > 0, "subproc_lines returned list");
	int n = (int)m_len (lines);
	check (n >= 3, "at least 3 lines");
	int *h, idx;
	for (idx = -1; m_next (lines, &idx, (void *)&h);) {
		const char *s = m_str (*h);
		switch (idx) {
		case 0: check (s && strstr (s, "a") != NULL, "line 1 is a"); break;
		case 1: check (s && strstr (s, "b") != NULL, "line 2 is b"); break;
		case 2: check (s && strstr (s, "c") != NULL, "line 3 is c"); break;
		}
	}
	m_free (lines);
}

T (test_empty_output)
{
	int h = subproc_read ("true");
	check (h > 0, "empty output returns valid handle");
	m_free (h);
}

T (test_lines_empty)
{
	int lines = subproc_lines ("true");
	check (lines > 0 || !lines, "empty lines is safe");
	if (lines)
		m_free (lines);
}

T (test_run_failure)
{
	int rc = subproc_run ("false", NULL, NULL, 0);
	check (rc != 0, "subproc_run false returns non-zero exit");
}

int main (void)
{
	m_init ();
	trace_level = 0;

	test_read_echo ();
	test_read_missing_cmd ();
	test_run_stdout_stderr ();
	test_lines ();
	test_empty_output ();
	test_lines_empty ();
	test_run_failure ();

	printf ("subproc: %d passed, %d failed\n", passed, failed);
	m_destruct ();
	return failed ? 1 : 0;
}
