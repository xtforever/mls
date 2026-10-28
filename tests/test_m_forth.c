#include "m_forth.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void expect (m_forth_t *f, const char *src, const char *out, int err)
{
	m_forth_output_clear (f);
	int rc = m_forth_eval (f, src);
	if (rc != err) {
		fprintf (stderr, "eval [%s] -> %d (%s), expected %d\n", src, rc,
			 m_forth_error_msg (f), err);
	}
	assert (rc == err);
	if (strcmp (m_forth_output (f), out) != 0)
		fprintf (stderr, "eval [%s] output [%s], expected [%s]\n", src,
			 m_forth_output (f), out);
	assert (strcmp (m_forth_output (f), out) == 0);
}

int main (void)
{
	m_forth_t *f = m_forth_new ();
	assert (f);
	assert (strcmp (m_forth_output (f), "") == 0);

	/* M0.1 done criterion. */
	expect (f, ": SQ DUP * ; 5 SQ .", "25 ", 0);
	expect (f, "1 2 + .", "3 ", 0);
	expect (f, "-3 ABS .", "3 ", 0);
	expect (f, "10 3 /MOD . .", "3 1 ", 0); /* quot(top) then rem */

	/* Underflow / division by zero are errors, and reset afterwards. */
	expect (f, "DROP", "", -4);
	expect (f, "1 0 /", "", -10);
	expect (f, "1 0 MOD", "", -10);
	expect (f, "42 .", "42 ", 0);

	/* M0.2 done criterion: strings. */
	expect (f, ": HI S\" hi\" ; HI S.", "hi", 0);
	expect (f, "Z\" yo\" Z.", "yo", 0);
	expect (f, ": Q .\" ok\" ; Q", "ok", 0);

	/* DEFER / IS. */
	expect (f, "DEFER X ' + IS X 1 2 X .", "3 ", 0);

	/* Control flow. */
	expect (f, ": T 0 BEGIN 1+ DUP 3 = UNTIL ; T .", "3 ", 0);
	expect (f, ": C 0 5 0 DO I + LOOP ; C .", "10 ", 0);
	expect (f, ": A DUP 0< IF NEGATE THEN ; -5 A .", "5 ", 0);
	expect (f, ": F [ 6 7 + ] LITERAL ; F .", "13 ", 0);
	expect (f,
		": FACT DUP 2 < IF DROP 1 ELSE DUP 1- RECURSE * THEN ; 5 FACT "
		".",
		"120 ", 0);

	/* POSTPONE / IMMEDIATE. */
	expect (f, ": MY-IF POSTPONE IF ; IMMEDIATE : T 1 MY-IF 5 . THEN ; T",
		"5 ", 0);

	/* Execution tokens and :NONAME. */
	expect (f, ":NONAME 7 . ; EXECUTE", "7 ", 0);
	expect (f, "5 ' SQ EXECUTE .", "25 ", 0);

	/* Exceptions. */
	expect (f, "' ABORT CATCH .", "-1 ", 0);
	expect (f, ": BOOM 1 0 / ; ' BOOM CATCH .", "-10 ", 0);
	expect (f, "5 ' ABORT CATCH . .", "-1 5 ",
		0); /* stack restored, result pushed */

	/* Wordlists: per-table override semantics.
	   The first WORDLIST after the built-in global (wid 0) is wid 1. */
	expect (f,
		"WORDLIST DUP SET-CURRENT : X 2 ; 0 SET-CURRENT DROP "
		": X 1 ; X .",
		"1 ", 0);
	assert (m_forth_get_current (f) == 0);
	int order1[2] = {1, 0};
	assert (m_forth_set_order (f, order1, 2) == 0);
	expect (f, "X .", "2 ", 0);
	/* SET-ORDER: stack wid2 wid1 n (wid1 searched first). */
	expect (f, "0 1 SET-ORDER X .", "1 ", 0);
	expect (f, "0 1 2 SET-ORDER X .", "2 ", 0);

	/* Multi-line source and direct string table API. */
	m_forth_output_clear (f);
	int rc = m_forth_eval (f, ": TWO 2 ;\nTWO TWO + .\n");
	assert (rc == 0);
	assert (strcmp (m_forth_output (f), "4 ") == 0);
	m_forth_output_clear (f);
	uint32_t id = m_forth_intern (f, "hello");
	assert (id > 0);
	assert (m_forth_intern (f, "hello") == id);
	assert (strcmp (m_forth_resolve (f, id), "hello") == 0);
	assert (m_forth_resolve (f, 0) == NULL);
	assert (m_forth_resolve (f, 999999) == NULL);

	/* Error state is exposed. */
	m_forth_output_clear (f);
	assert (m_forth_eval (f, "NOSUCHWORD") == -13);
	assert (m_forth_error (f) == -13);
	assert (strlen (m_forth_error_msg (f)) > 0);

	/* Multiple VMs are independent (no global VM state). */
	m_forth_t *g = m_forth_new ();
	assert (g);
	expect (g, "6 7 * .", "42 ", 0);
	assert (strcmp (m_forth_output (f), "") == 0);
	m_forth_free (g);

	m_forth_free (f);
	m_forth_free (NULL);

	puts ("m_forth selftest: OK");
	return 0;
}
