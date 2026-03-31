#include "../lib/m_tool.h"
#include "../lib/mls.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void test_m_slice_basic ()
{
	printf ("Testing m_slice basic...\n");
	int m = m_alloc (10, sizeof (int), MFREE);
	for (int i = 0; i < 10; i++) {
		m_put (m, &i);
	}

	// Slice from index 2 to 5 (inclusive: 2, 3, 4, 5)
	int s = m_slice (0, 0, m, 2, 5);
	assert (m_len (s) == 4);
	assert (INT (s, 0) == 2);
	assert (INT (s, 1) == 3);
	assert (INT (s, 2) == 4);
	assert (INT (s, 3) == 5);

	m_free (s);
	m_free (m);
	printf ("m_slice basic test passed.\n");
}

void test_m_slice_negative ()
{
	printf ("Testing m_slice with negative indices...\n");
	int m = m_alloc (10, sizeof (int), MFREE);
	for (int i = 0; i < 10; i++) {
		m_put (m, &i);
	}

	// -1 is the last element (9), -3 is 10-3=7.
	// Slice from 7 to 9
	int s = m_slice (0, 0, m, -3, -1);
	assert (m_len (s) == 3);
	assert (INT (s, 0) == 7);
	assert (INT (s, 1) == 8);
	assert (INT (s, 2) == 9);

	m_free (s);
	m_free (m);
	printf ("m_slice negative indices test passed.\n");
}

void test_s_slice_basic ()
{
	printf ("Testing s_slice basic...\n");
	int m = m_alloc (10, 1, MFREE);
	const char *text = "Hello World";
	for (int i = 0; text[i]; i++) {
		m_put (m, &text[i]);
	}
	// Add null terminator to source
	char zero = 0;
	m_put (m, &zero);

	// Slice "World" (index 6 to 10)
	int s = s_slice (0, 0, m, 6, 10);
	assert (strcmp (m_buf (s), "World") == 0);
	assert (m_len (s) == 6); // "World" + null terminator

	m_free (s);
	m_free (m);
	printf ("s_slice basic test passed.\n");
}

void test_m_slice_inplace ()
{
	printf ("Testing m_slice in-place (reuse destination)...\n");
	int m = m_alloc (10, sizeof (int), MFREE);
	for (int i = 0; i < 10; i++) {
		m_put (m, &i);
	}

	int dest = m_alloc (20, sizeof (int), MFREE);
	int val = 99;
	m_put (dest, &val); // Add something to dest

	// Slice into dest at offset 1
	m_slice (dest, 1, m, 0, 2);
	// m_slice currently does m_setlen(dest, offs) before loop.
	// So dest length should be 1 + 3 = 4.

	assert (m_len (dest) == 4);
	assert (INT (dest, 0) == 99);
	assert (INT (dest, 1) == 0);
	assert (INT (dest, 2) == 1);
	assert (INT (dest, 3) == 2);

	m_free (dest);
	m_free (m);
	printf ("m_slice in-place test passed.\n");
}

int main ()
{
	trace_level = 1;
	m_init ();

	test_m_slice_basic ();
	test_m_slice_negative ();
	test_s_slice_basic ();
	test_m_slice_inplace ();

	m_destruct ();
	printf ("All slice tests completed successfully.\n");
	return 0;
}
