#ifndef M_FPROTO_H
#define M_FPROTO_H

#include <stdint.h>

/* Wire result protocol:
   RESULT COLS <n> (COL Z"name" Z"type")* [COUNT <n>] (ROW <cell>*) END
   cell := <int64> | S" text" | Z" text"
   Escapes: \" \\ \n \t. */
typedef struct m_fproto m_fproto_t;

m_fproto_t *m_fproto_new (void);
void m_fproto_free (m_fproto_t *w);

void m_fproto_result (m_fproto_t *w, int ncols);
void m_fproto_col (m_fproto_t *w, const char *name, const char *type);
void m_fproto_count (m_fproto_t *w, int64_t n);
void m_fproto_row (m_fproto_t *w);
void m_fproto_int (m_fproto_t *w, int64_t v);
void m_fproto_str (m_fproto_t *w, const char *s);
void m_fproto_zstr (m_fproto_t *w, const char *s);
void m_fproto_end (m_fproto_t *w);
void m_fproto_error (m_fproto_t *w, int code, const char *msg);
const char *m_fproto_data (const m_fproto_t *w);

enum { M_FPROTO_INT = 0, M_FPROTO_STR = 1, M_FPROTO_ZSTR = 2 };

typedef struct {
	int type;
	int64_t i;
	char *s;
} m_fproto_value_t;

typedef struct {
	char *name;
	char *type;
} m_fproto_col_t;

typedef struct {
	int ncols;
	m_fproto_col_t *cols;
	int nrows;
	m_fproto_value_t *values; /* nrows * ncols */
	int has_count;
	int64_t count;
} m_fproto_result_t;

m_fproto_result_t *m_fproto_parse (const char *text);
void m_fproto_result_free (m_fproto_result_t *r);

#endif
