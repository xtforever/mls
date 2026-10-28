#include "token.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main (void)
{
	tokenizer_t t;
	assert (tok_init (&t, "foo bar\tbaz\r\nqux") == 0);
	assert (strcmp (tok_next (&t), "foo") == 0);
	assert (strcmp (tok_next (&t), "bar") == 0);
	assert (strcmp (tok_next (&t), "baz") == 0);
	assert (strcmp (tok_next (&t), "qux") == 0);
	assert (tok_next (&t) == NULL);
	assert (!tok_has_more (&t));
	tok_free (&t);

	/* Comments: ( ... ) may repeat; \ ends the line. */
	assert (tok_init (&t, "a (not a token) b (x) c") == 0);
	assert (strcmp (tok_next (&t), "a") == 0);
	assert (strcmp (tok_next (&t), "b") == 0);
	assert (strcmp (tok_next (&t), "c") == 0);
	assert (tok_next (&t) == NULL);
	tok_free (&t);

	assert (tok_init (&t, "keep \\ drop me") == 0);
	assert (strcmp (tok_next (&t), "keep") == 0);
	assert (tok_next (&t) == NULL);
	tok_free (&t);

	assert (tok_init (&t, "(unterminated") == 0);
	assert (tok_next (&t) == NULL);
	tok_free (&t);

	/* String literals with escapes. */
	assert (tok_init (&t, "  \"a\\\"b\\\\c\\nd\\te\\q\"") == 0);
	char *s = tok_parse_string (&t, '"');
	assert (strcmp (s, "a\"b\\c\nd\te\\q") == 0);
	tok_free (&t);

	assert (tok_init (&t, "\"\"") == 0);
	assert (strcmp (tok_parse_string (&t, '"'), "") == 0);
	tok_free (&t);

	/* Missing closing quote consumes the rest. */
	assert (tok_init (&t, "\"abc") == 0);
	assert (strcmp (tok_parse_string (&t, '"'), "abc") == 0);
	tok_free (&t);

	/* Escaped quote does not terminate early. */
	assert (tok_init (&t, "\"x\\\"y\" rest") == 0);
	assert (strcmp (tok_parse_string (&t, '"'), "x\"y") == 0);
	assert (strcmp (tok_next (&t), "rest") == 0);
	tok_free (&t);

	/* NULL input is an empty line; double free is safe. */
	assert (tok_init (&t, NULL) == 0);
	assert (tok_next (&t) == NULL);
	tok_free (&t);
	tok_free (&t);

	puts ("token selftest: OK");
	return 0;
}
