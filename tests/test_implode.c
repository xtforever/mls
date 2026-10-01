/* Tests for m_wrapstrhandles(), s_implode() and s_implode_q(): wrap a C-string
 * array into an m-array of string handles, then join it with an optional
 * per-element quote character. */
#include "../lib/m_tool.h"
#include "../lib/mls.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main (void)
{
	m_init ();

	int sep = s_dup (",");
	int out;

	/* m_wrapstrhandles: C array -> MFREE_EACH list of owned string handles
	 */
	const char *abc[] = {"a", "b", "c"};
	int l = m_wrapstrhandles (abc, 3);
	assert (m_len (l) == 3);
	assert (strcmp (m_str (INT (l, 0)), "a") == 0);
	assert (strcmp (m_str (INT (l, 2)), "c") == 0);

	/* plain join, no quoting */
	out = s_implode (0, l, sep);
	assert (strcmp (m_str (out), "a,b,c") == 0);
	m_free (out);

	/* single quotes */
	out = s_implode_q (0, l, sep, '\'');
	assert (strcmp (m_str (out), "'a','b','c'") == 0);
	m_free (out);

	/* double quotes */
	out = s_implode_q (0, l, sep, '"');
	assert (strcmp (m_str (out), "\"a\",\"b\",\"c\"") == 0);
	m_free (out);

	/* empty separator concatenates (and still quotes) */
	int nsep = s_dup ("");
	out = s_implode_q (0, l, nsep, '\'');
	assert (strcmp (m_str (out), "'a''b''c'") == 0);
	m_free (out);
	m_free (nsep);
	m_free (l);

	/* embedded quote is doubled */
	const char *ob[] = {"a", "O'Brien", "c"};
	l = m_wrapstrhandles (ob, 3);
	out = s_implode_q (0, l, sep, '\'');
	assert (strcmp (m_str (out), "'a','O''Brien','c'") == 0);
	m_free (out);
	m_free (l);

	/* NULL entry -> empty string */
	const char *withnull[] = {"a", NULL, "c"};
	l = m_wrapstrhandles (withnull, 3);
	assert (m_len (l) == 3);
	assert (strcmp (m_str (INT (l, 1)), "") == 0);
	out = s_implode_q (0, l, sep, '\'');
	assert (strcmp (m_str (out), "'a','','c'") == 0);
	m_free (out);
	m_free (l);

	/* zero-copy: elements alias the caller's buffers (no copy) */
	char mutable[][8] = {"aa", "bb"};
	const char *mv[] = {mutable[0], mutable[1]};
	l = m_wrapstrhandles (mv, 2);
	mutable[0][0] = 'Z'; /* change after wrapping -> handle sees it */
	assert (strcmp (m_str (INT (l, 0)), "Za") == 0);
	out = s_implode_q (0, l, sep, '\'');
	assert (strcmp (m_str (out), "'Za','bb'") == 0);
	m_free (out);
	m_free (l);

	/* edge cases: NULL list or nelem <= 0 -> empty list */
	l = m_wrapstrhandles (NULL, 0);
	assert (m_len (l) == 0);
	m_free (l);
	l = m_wrapstrhandles (abc, 0);
	assert (m_len (l) == 0);
	m_free (l);

	/* empty array -> empty string */
	out = s_implode_q (0, 0, sep, '\'');
	assert (strcmp (m_str (out), "") == 0);
	m_free (out);

	/* dest reuse: old content is replaced, same handle returned */
	out = s_implode_q (0, 0, sep, 0);
	int again = s_implode_q (out, 0, sep, '"');
	assert (again == out && strcmp (m_str (again), "") == 0);
	m_free (out);

	m_free (sep);
	m_destruct ();
	puts ("implode ok");
	return 0;
}
