#include "../lib/mls.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

void my_recursive_free_deep (lst_t l)
{
	int p = -1, *d;
	while (lst_next (l, &p, &d)) {
		if (*d > 0 && !m_is_freed (*d)) {
			m_free (*d);
		}
	}
}

void test_deep_recursion ()
{
	printf ("Testing deep recursion with MFREE_EACH...\n");
	int h = m_alloc (10, sizeof (int), MFREE_EACH);
	int current = h;
	for (int i = 0; i < 1000; i++) {
		int next = m_alloc (10, sizeof (int), MFREE_EACH);
		m_put (current, &next);
		current = next;
	}
	m_free (h);
	assert (m_is_freed (h));
	printf ("Deep recursion test passed.\n");
}

void test_complex_custom_free ()
{
	printf ("Testing complex custom free handler...\n");
	int hdl = m_reg_freefn (MFREE_MAX + 1, my_recursive_free_deep);
	int h = m_alloc (10, sizeof (int), hdl);
	int inner = m_alloc (10, sizeof (int), MFREE);
	m_put (h, &inner);

	m_free (h);
	assert (m_is_freed (h));
	assert (m_is_freed (inner));
	printf ("Complex custom free handler test passed.\n");
}

int main ()
{
	m_init ();
	test_deep_recursion ();
	test_complex_custom_free ();
	m_destruct ();
	printf ("Memory advanced tests passed.\n");
	return 0;
}
