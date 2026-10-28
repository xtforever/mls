#include "m_engine.h"

#include "m_dbwords.h"
#include "m_forth.h"
#include "m_fproto.h"

#include <stdlib.h>

struct m_engine {
	m_forth_t *f;
	m_dbwords_t *dw;
	m_fproto_t *w;
	int error;
};

m_engine_t *m_engine_new (m_isam_db_t *db)
{
	if (!db)
		return NULL;
	m_engine_t *e = calloc (1, sizeof *e);
	if (!e)
		return NULL;
	e->f = m_forth_new ();
	if (!e->f) {
		free (e);
		return NULL;
	}
	e->dw = m_dbwords_new (e->f, db);
	if (!e->dw) {
		m_forth_free (e->f);
		free (e);
		return NULL;
	}
	return e;
}

void m_engine_free (m_engine_t *e)
{
	if (!e)
		return;
	m_fproto_free (e->w);
	m_dbwords_free (e->dw);
	m_forth_free (e->f);
	free (e);
}

static void emit_result (m_engine_t *e, m_fproto_t *w)
{
	m_isam_table_t *t = m_dbwords_table (e->dw);
	if (!t) {
		m_fproto_result (w, 0);
		m_fproto_end (w);
		return;
	}

	int ncols = m_isam_ncols (t);
	m_fproto_result (w, ncols);
	for (int c = 0; c < ncols; c++) {
		char ty[2] = {m_isam_coltype (t, c), '\0'};
		m_fproto_col (w, m_isam_colname (t, c), ty);
	}
	m_fproto_count (w, m_isam_count (t));

	int64_t recno = m_dbwords_recno (e->dw);
	cell_t cells[M_ISAM_MAX_COLS];
	if (recno > 0 && m_isam_fetch (t, recno, cells)) {
		m_fproto_row (w);
		for (int c = 0; c < ncols; c++) {
			char ty = m_isam_coltype (t, c);
			if (ty == 's' || ty == 'z') {
				const char *s =
					m_isam_resolve (t, (uint32_t)cells[c]);
				if (ty == 's')
					m_fproto_str (w, s);
				else
					m_fproto_zstr (w, s);
			} else {
				m_fproto_int (w, cells[c]);
			}
		}
	}
	m_fproto_end (w);
}

const char *m_engine_exec (m_engine_t *e, const char *script)
{
	if (!e || !script)
		return "";
	m_forth_output_clear (e->f);
	e->error = m_forth_eval (e->f, script);

	m_fproto_t *w = m_fproto_new ();
	if (!w)
		return "";
	if (e->error)
		m_fproto_error (w, e->error, m_forth_error_msg (e->f));
	else
		emit_result (e, w);
	m_fproto_free (e->w);
	e->w = w;
	return m_fproto_data (w);
}

int m_engine_error (const m_engine_t *e) { return e ? e->error : -1; }

int64_t m_engine_last_recno (const m_engine_t *e)
{
	return e ? m_dbwords_recno (e->dw) : 0;
}
