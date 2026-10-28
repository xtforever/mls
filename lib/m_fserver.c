#include "m_fserver.h"

#include "m_engine.h"
#include "m_ftransport.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct m_fserver {
	int listen_fd;
	char *path;
	m_isam_db_t *db;
};

static char *dup_str (const char *s)
{
	size_t n = strlen (s) + 1;
	char *p = malloc (n);
	if (p)
		memcpy (p, s, n);
	return p;
}

m_fserver_t *m_fserver_new (const char *socket_path, m_isam_db_t *db)
{
	if (!socket_path || !db)
		return NULL;
	m_fserver_t *s = calloc (1, sizeof *s);
	if (!s)
		return NULL;
	s->listen_fd = m_ftransport_listen (socket_path);
	if (s->listen_fd < 0) {
		free (s);
		return NULL;
	}
	s->path = dup_str (socket_path);
	if (!s->path) {
		m_ftransport_close (s->listen_fd);
		free (s);
		return NULL;
	}
	s->db = db;
	return s;
}

void m_fserver_free (m_fserver_t *s)
{
	if (!s)
		return;
	m_ftransport_close (s->listen_fd);
	if (s->path) {
		unlink (s->path);
		free (s->path);
	}
	free (s);
}

int m_fserver_serve_one (m_fserver_t *s)
{
	if (!s)
		return -1;
	int cfd = m_ftransport_accept (s->listen_fd);
	if (cfd < 0)
		return -1;

	m_engine_t *e = m_engine_new (s->db);
	if (!e) {
		m_ftransport_close (cfd);
		return -1;
	}

	char *script = NULL;
	uint32_t len = 0;
	int rc = -1;
	if (m_ftransport_recv_alloc (cfd, &script, &len) == 0) {
		const char *resp = m_engine_exec (e, script);
		rc = m_ftransport_send (cfd, resp, (uint32_t)strlen (resp));
	}
	free (script);
	m_engine_free (e);
	m_ftransport_close (cfd);
	return rc;
}
