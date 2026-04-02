#include "../lib/mls.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

void my_recursive_free (lst_t l)
{
	int p = -1, *d;
	printf ("my_recursive_free calling\n");
	while (lst_next (l, &p, &d)) {
		if (*d > 0 && !m_is_freed (*d)) {
			m_free (*d);
		}
	}
}

int main ()
{
	m_init ();
	// Use a custom free handler index to avoid collision with predefined ones
	int hdl = m_reg_freefn (MFREE_MAX + 1, my_recursive_free);

	int l1 = m_alloc (10, sizeof (int), hdl);
	m_put (l1, &l1);

	printf ("Freeing list with circular reference...\n");
	m_free (l1);
	assert (m_is_freed (l1));

	m_destruct ();
	printf ("Circular reference test passed.\n");
	return 0;
}
