#ifndef TOKEN_H
#define TOKEN_H

#include "strbuf.h"

/* Whitespace tokenizer with `( ... )` and `\` comments. Returned pointers are
   valid until the next call (they point into scratch). */
typedef struct {
	const char *input;
	int pos;
	int len;
	strbuf_t *scratch;
} tokenizer_t;

int tok_init (tokenizer_t *t, const char *line); /* 0 ok, -1 OOM */
void tok_free (tokenizer_t *t);
int tok_has_more (tokenizer_t *t);
void tok_skip_ws (tokenizer_t *t);
char *tok_next (tokenizer_t *t);		     /* NULL at end */
char *tok_parse_string (tokenizer_t *t, char quote); /* decodes escapes */

#endif
