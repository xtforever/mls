/* Hostile-input / incremental-resume selftest for the m_http parser.
 *
 * Proves the parser rejects adversarial framing and assembles messages that
 * arrive split across reads. Run under ASan (default) to catch OOB/UAF;
 * build with -fsanitize=undefined too to catch the old parse_hex overflow.
 *
 * Deliberately framework-free: assert() and one main(). */
#include "../lib/m_http.h"
#include "../lib/m_tool.h"
#include "../lib/mls.h"
#include "../lib/table.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Append raw bytes (no NUL) to an accumulating request buffer, exactly like
 * m_http_server.c's read loop does. */
static void feed (int data_h, const char *s)
{
	m_write (data_h, m_len (data_h), s, strlen (s));
}

static void expect_error (const char *raw)
{
	http_parser_t p;
	http_parser_init (&p);
	int data = s_strdup_c (raw);
	int r = http_parse (&p, data);
	assert (r == -1);
	assert (p.state == HTTP_STATE_ERROR);
	m_free (data);
	http_parser_free (&p);
}

/* A Content-Length body split across two reads must be assembled byte-exact
 * (the old parser re-read from the body start and produced "hhell"). */
static void test_cl_body_split ()
{
	printf ("Testing Content-Length body split across reads...\n");
	http_parser_t p;
	http_parser_init (&p);

	int data = m_alloc (0, 1, MFREE);
	feed (data, "POST /x HTTP/1.1\r\nContent-Length: 5\r\n\r\nhel");
	assert (http_parse (&p, data) == 0);
	assert (p.state == HTTP_STATE_BODY);
	assert (m_len (p.body) == 3);

	feed (data, "lo");
	assert (http_parse (&p, data) == 0);
	assert (p.state == HTTP_STATE_DONE);
	assert (m_len (p.body) == 5);
	assert (memcmp (m_buf (p.body), "hello", 5) == 0);

	m_free (data);
	http_parser_free (&p);
	printf ("  ok\n");
}

/* A chunk split across reads must not be re-read as a chunk-size line. */
static void test_chunked_body_split ()
{
	printf ("Testing chunked body split across reads...\n");
	http_parser_t p;
	http_parser_init (&p);

	int data = m_alloc (0, 1, MFREE);
	feed (data,
	      "POST /x HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhel");
	assert (http_parse (&p, data) == 0);
	assert (p.state == HTTP_STATE_BODY);
	assert (m_len (p.body) == 0); /* chunk not consumed until complete */

	feed (data, "lo\r\n0\r\n\r\n");
	assert (http_parse (&p, data) == 0);
	assert (p.state == HTTP_STATE_DONE);
	assert (m_len (p.body) == 5);
	assert (memcmp (m_buf (p.body), "hello", 5) == 0);

	m_free (data);
	http_parser_free (&p);
	printf ("  ok\n");
}

/* Multiple chunks across reads, plus the terminating chunk. */
static void test_chunked_multi_split ()
{
	printf ("Testing multi-chunk body across reads...\n");
	http_parser_t p;
	http_parser_init (&p);

	int data = m_alloc (0, 1, MFREE);
	feed (data, "POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n");
	assert (http_parse (&p, data) == 0);
	assert (p.state == HTTP_STATE_BODY);

	feed (data, "5\r\nhello\r\n");
	assert (http_parse (&p, data) == 0);
	assert (m_len (p.body) == 5);

	feed (data, "6\r\n world\r\n0\r\n\r\n");
	assert (http_parse (&p, data) == 0);
	assert (p.state == HTTP_STATE_DONE);
	assert (m_len (p.body) == 11);
	assert (memcmp (m_buf (p.body), "hello world", 11) == 0);

	m_free (data);
	http_parser_free (&p);
	printf ("  ok\n");
}

/* 9 hex digits = 2^32. Must be rejected as too large, not wrap to 0 (which
 * the old int accumulator did, terminating the body early). */
static void test_chunk_size_no_wrap ()
{
	printf ("Testing chunk size that would overflow 32 bits...\n");
	expect_error ("POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n"
		      "100000000\r\nx\r\n0\r\n\r\n");
	printf ("  ok\n");
}

/* 16 hex digits = UINT64_MAX. Must fail the (wrap-safe) size check. */
static void test_chunk_size_uint64_max ()
{
	printf ("Testing chunk size UINT64_MAX...\n");
	expect_error ("POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n"
		      "FFFFFFFFFFFFFFFF\r\nx\r\n0\r\n\r\n");
	printf ("  ok\n");
}

/* 17 hex digits: more than a uint64 can hold, must be a parse error. */
static void test_chunk_size_too_many_digits ()
{
	printf ("Testing 17-digit chunk size...\n");
	expect_error ("POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n"
		      "10000000000000000\r\nx\r\n0\r\n\r\n");
	printf ("  ok\n");
}

/* Non-hex chunk size must error, not stall (return pos unchanged forever). */
static void test_chunk_size_invalid_hex ()
{
	printf ("Testing invalid hex chunk size...\n");
	expect_error ("POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n"
		      "zz\r\nx\r\n0\r\n\r\n");
	printf ("  ok\n");
}

static void test_content_length_rejects ()
{
	printf ("Testing malformed/oversized Content-Length...\n");
	expect_error ("POST / HTTP/1.1\r\nContent-Length: -1\r\n\r\n");
	expect_error ("POST / HTTP/1.1\r\nContent-Length: 12abc\r\n\r\n");
	expect_error ("POST / HTTP/1.1\r\nContent-Length: "
		      "99999999999999999999\r\n\r\n");
	expect_error ("POST / HTTP/1.1\r\nContent-Length: "
		      "18446744073709551615\r\n\r\n");
	printf ("  ok\n");
}

/* A too-short body is incomplete, not an error: parser stays in BODY. */
static void test_short_content_length ()
{
	printf ("Testing truncated Content-Length body...\n");
	http_parser_t p;
	http_parser_init (&p);

	int data = m_alloc (0, 1, MFREE);
	feed (data, "POST / HTTP/1.1\r\nContent-Length: 20\r\n\r\nhello");
	int r = http_parse (&p, data);
	assert (r == 0);
	assert (p.state == HTTP_STATE_BODY);
	assert (m_len (p.body) == 5);
	assert (memcmp (m_buf (p.body), "hello", 5) == 0);

	m_free (data);
	http_parser_free (&p);
	printf ("  ok\n");
}

/* A zero-length body finishes immediately. */
static void test_zero_content_length ()
{
	printf ("Testing Content-Length: 0...\n");
	http_parser_t p;
	http_parser_init (&p);

	int data = s_strdup_c ("POST / HTTP/1.1\r\nContent-Length: 0\r\n\r\n");
	assert (http_parse (&p, data) == 0);
	assert (p.state == HTTP_STATE_DONE);
	assert (m_len (p.body) == 0);

	m_free (data);
	http_parser_free (&p);
	printf ("  ok\n");
}

static void test_validators ()
{
	printf ("Testing header-injection and path-traversal validators...\n");
	int good = s_strdup_c ("normal value");
	int bad = s_strdup_c ("bad\r\nInjected: 1");
	assert (http_validate_header_value (good) == 1);
	assert (http_validate_header_value (bad) == 0);
	m_free (good);
	m_free (bad);

	assert (http_validate_uri_path ("/a/b") == 1);
	assert (http_validate_uri_path ("/a/../b") == 0);
	assert (http_validate_uri_path ("/../etc/passwd") == 0);
	assert (http_validate_uri_path ("..") == 0);
	assert (http_validate_uri_path (NULL) == 0);
	printf ("  ok\n");
}

/* The parser must read a caller-owned (MFREE_NOALLOC) wrapped buffer safely. */
static void test_wrapped_buffer ()
{
	printf ("Testing MFREE_NOALLOC wrapped buffer...\n");
	char raw[] = "GET / HTTP/1.1\r\nHost: x\r\n\r\n";
	int data = m_wrapcstr (raw);

	http_parser_t p;
	http_parser_init (&p);
	int r = http_parse (&p, data);
	assert (r == 0);
	assert (p.state == HTTP_STATE_DONE);
	assert (strcmp (m_str (p.uri), "/") == 0);

	m_free (data); /* safe: MFREE_NOALLOC leaves the caller's memory alone
			*/
	http_parser_free (&p);
	printf ("  ok\n");
}

/* Reuse after DONE must not leak the previous message's handles. */
static void test_reset_reuse ()
{
	printf ("Testing http_parser_reset reuse...\n");
	http_parser_t p;
	http_parser_init_request (&p);

	int a = s_strdup_c ("GET /a HTTP/1.1\r\nHost: x\r\n\r\n");
	assert (http_parse (&p, a) == 0);
	assert (p.state == HTTP_STATE_DONE);
	assert (strcmp (m_str (p.uri), "/a") == 0);

	http_parser_reset (&p);
	assert (p.state == HTTP_STATE_REQUEST_LINE);

	int b = s_strdup_c ("GET /b HTTP/1.1\r\nHost: y\r\n\r\n");
	assert (http_parse (&p, b) == 0);
	assert (p.state == HTTP_STATE_DONE);
	assert (strcmp (m_str (p.uri), "/b") == 0);

	m_free (a);
	m_free (b);
	http_parser_free (&p);
	http_parser_free (&p); /* idempotent: must not double-free */
	printf ("  ok\n");
}

int main ()
{
	trace_level = 0;
	m_init ();
	conststr_init ();

	test_cl_body_split ();
	test_chunked_body_split ();
	test_chunked_multi_split ();
	test_chunk_size_no_wrap ();
	test_chunk_size_uint64_max ();
	test_chunk_size_too_many_digits ();
	test_chunk_size_invalid_hex ();
	test_content_length_rejects ();
	test_short_content_length ();
	test_zero_content_length ();
	test_validators ();
	test_wrapped_buffer ();
	test_reset_reuse ();

	conststr_free ();
	m_destruct ();
	printf ("All hostile HTTP tests passed.\n");
	return 0;
}
