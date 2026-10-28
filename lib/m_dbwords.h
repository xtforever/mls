#ifndef M_DBWORDS_H
#define M_DBWORDS_H

#include "m_forth.h"
#include "m_isam.h"

/* Binds DB-* words to a Forth VM. The context is stored as the VM user
   pointer, so several VMs/tables stay independent. */
typedef struct m_dbwords m_dbwords_t;

m_dbwords_t *m_dbwords_new (m_forth_t *f, m_isam_db_t *db);
void m_dbwords_free (m_dbwords_t *dw);

/* Read-only state for the engine / result emitter. */
m_isam_table_t *m_dbwords_table (const m_dbwords_t *dw);
int64_t m_dbwords_recno (const m_dbwords_t *dw);
int m_dbwords_have_current (const m_dbwords_t *dw);

#endif
