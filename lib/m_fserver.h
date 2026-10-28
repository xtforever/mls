#ifndef M_FSERVER_H
#define M_FSERVER_H

#include "m_isam.h"

/* Unix-socket daemon: accepts one connection, runs one query through a fresh
   per-connection VM, sends the response, closes. */
typedef struct m_fserver m_fserver_t;

m_fserver_t *m_fserver_new (const char *socket_path, m_isam_db_t *db);
void m_fserver_free (m_fserver_t *s);
int m_fserver_serve_one (m_fserver_t *s); /* 0 ok, -1 error */

#endif
