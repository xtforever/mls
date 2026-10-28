#ifndef M_FCLIENT_H
#define M_FCLIENT_H

/* Socket backend for query_exec: connect, send one Forth script, receive one
   response script. The server closes after one query (M0 semantics). */
typedef struct m_fclient m_fclient_t;

m_fclient_t *m_fclient_connect (const char *socket_path);
void m_fclient_close (m_fclient_t *c);
const char *m_fclient_exec (m_fclient_t *c, const char *script);
int m_fclient_error (const m_fclient_t *c);

#endif
