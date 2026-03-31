#include "../lib/mls.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

void test_m_is_freed_basic ()
{
	printf ("Testing m_is_freed basic behavior...\n");
	int m = m_alloc (10, sizeof (int), MFREE);
	int h = m;
	assert (!m_is_freed (h));
	m_free (m);
	assert (m_is_freed (h));
	printf ("m_is_freed basic behavior test passed.\n");
}

static int recursive_call_count = 0;
void my_recursive_free_deep (int m)
{
	int p, *d;
	recursive_call_count++;
	// We expect this to be called for each list in the cycle
	for (p = -1; m_next (m, &p, &d);) {
		if (*d > 0 && !m_is_freed (*d)) {
			m_free (*d);
		}
	}
}

void test_complex_circular_reference ()
{
	printf ("Testing complex circular reference (A -> B -> C -> A)...\n");
	int hdl = m_reg_freefn (0, my_recursive_free_deep);
	int a = m_alloc (10, sizeof (int), hdl);
	int b = m_alloc (10, sizeof (int), hdl);
	int c = m_alloc (10, sizeof (int), hdl);

	m_put (a, &b);
	m_put (b, &c);
	m_put (c, &a);

	recursive_call_count = 0;
	m_free (a);

	assert (m_is_freed (a));
	assert (m_is_freed (b));
	assert (m_is_freed (c));
	assert (recursive_call_count == 3);
	printf ("Complex circular reference test passed.\n");
}

int main ()
{
	trace_level = 1;
	m_init ();

	test_m_is_freed_basic ();
	test_complex_circular_reference ();

	m_destruct ();
	printf ("All advanced memory tests completed successfully.\n");
	return 0;
}
