#ifndef M_ENGINE_H
#define M_ENGINE_H

#include "m_isam.h"

/* The API boundary: script in, response script out (in-process monolith). */
typedef struct m_engine m_engine_t;

m_engine_t *m_engine_new (m_isam_db_t *db);
void m_engine_free (m_engine_t *e);
const char *m_engine_exec (m_engine_t *e, const char *script);
int m_engine_error (const m_engine_t *e);
int64_t m_engine_last_recno (const m_engine_t *e);

#endif
