#include "m_fclient.h"

#include "m_ftransport.h"

#include <stdlib.h>
#include <string.h>

struct m_fclient {
	int fd;
	char *resp;
	int error;
};

m_fclient_t *m_fclient_connect (const char *socket_path)
{
	if (!socket_path)
		return NULL;
	int fd = m_ftransport_connect (socket_path);
	if (fd < 0)
		return NULL;
	m_fclient_t *c = calloc (1, sizeof *c);
	if (!c) {
		m_ftransport_close (fd);
		return NULL;
	}
	c->fd = fd;
	return c;
}

void m_fclient_close (m_fclient_t *c)
{
	if (!c)
		return;
	m_ftransport_close (c->fd);
	free (c->resp);
	free (c);
}

const char *m_fclient_exec (m_fclient_t *c, const char *script)
{
	if (!c || !script) {
		if (c)
			c->error = -1;
		return "";
	}
	c->error = 0;
	free (c->resp);
	c->resp = NULL;
	uint32_t len = 0;
	if (m_ftransport_send (c->fd, script, (uint32_t)strlen (script)) != 0 ||
	    m_ftransport_recv_alloc (c->fd, &c->resp, &len) != 0) {
		c->error = -1;
		return "";
	}
	return c->resp;
}

int m_fclient_error (const m_fclient_t *c) { return c ? c->error : -1; }
