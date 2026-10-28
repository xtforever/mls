#include "m_ftransport.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void put_u32 (unsigned char *p, uint32_t v)
{
	p[0] = (unsigned char)(v & 0xFF);
	p[1] = (unsigned char)((v >> 8) & 0xFF);
	p[2] = (unsigned char)((v >> 16) & 0xFF);
	p[3] = (unsigned char)((v >> 24) & 0xFF);
}

int main (void)
{
	/* Framing roundtrip over a socketpair. */
	int sv[2];
	assert (socketpair (AF_UNIX, SOCK_STREAM, 0, sv) == 0);
	const char *msg = "hello\0world"; /* payload with an embedded NUL */
	uint32_t n = 11;
	assert (m_ftransport_send (sv[0], msg, n) == 0);
	char buf[64];
	uint32_t got = 0;
	assert (m_ftransport_recv (sv[1], buf, sizeof buf, &got) == 0);
	assert (got == n && memcmp (buf, msg, n) == 0);

	/* Empty frame. */
	assert (m_ftransport_send (sv[0], NULL, 0) == 0);
	assert (m_ftransport_recv (sv[1], buf, sizeof buf, &got) == 0);
	assert (got == 0);
	m_ftransport_close (sv[0]);
	m_ftransport_close (sv[1]);

	/* Oversized length is rejected before allocating. */
	assert (socketpair (AF_UNIX, SOCK_STREAM, 0, sv) == 0);
	unsigned char hdr[4];
	put_u32 (hdr, M_FTRANSPORT_MAX_FRAME + 1);
	assert (write (sv[0], hdr, 4) == 4);
	assert (m_ftransport_recv (sv[1], buf, sizeof buf, &got) == -1);
	m_ftransport_close (sv[0]);
	m_ftransport_close (sv[1]);

	/* Truncated payload: header promises 10, only 3 arrive. */
	assert (socketpair (AF_UNIX, SOCK_STREAM, 0, sv) == 0);
	put_u32 (hdr, 10);
	assert (write (sv[0], hdr, 4) == 4);
	assert (write (sv[0], "abc", 3) == 3);
	close (sv[0]);
	assert (m_ftransport_recv (sv[1], buf, sizeof buf, &got) == -1);
	m_ftransport_close (sv[1]);

	/* recv cap smaller than the frame is an error. */
	assert (socketpair (AF_UNIX, SOCK_STREAM, 0, sv) == 0);
	assert (m_ftransport_send (sv[0], "0123456789", 10) == 0);
	assert (m_ftransport_recv (sv[1], buf, 4, &got) == -1);
	m_ftransport_close (sv[0]);
	m_ftransport_close (sv[1]);

	/* Listening socket + connect + alloc receive. */
	const char *path = "/tmp/mls_ftransport_test.sock";
	unlink (path);
	int lfd = m_ftransport_listen (path);
	assert (lfd >= 0);
	int cfd = m_ftransport_connect (path);
	assert (cfd >= 0);
	int sfd = m_ftransport_accept (lfd);
	assert (sfd >= 0);

	assert (m_ftransport_send (cfd, "ping", 4) == 0);
	char *frame = NULL;
	assert (m_ftransport_recv_alloc (sfd, &frame, &got) == 0);
	assert (got == 4 && memcmp (frame, "ping", 4) == 0);
	free (frame);

	assert (m_ftransport_send (sfd, "pong", 4) == 0);
	assert (m_ftransport_recv (cfd, buf, sizeof buf, &got) == 0);
	assert (got == 4 && memcmp (buf, "pong", 4) == 0);

	m_ftransport_close (cfd);
	m_ftransport_close (sfd);
	m_ftransport_close (lfd);
	unlink (path);

	/* Invalid arguments. */
	assert (m_ftransport_listen (NULL) == -1);
	assert (m_ftransport_connect (NULL) == -1);
	assert (m_ftransport_send (-1, "x", 1) == -1);
	assert (m_ftransport_recv (-1, buf, sizeof buf, &got) == -1);
	assert (m_ftransport_recv (0, buf, sizeof buf, NULL) == -1);
	m_ftransport_close (-1);

	puts ("m_ftransport selftest: OK");
	return 0;
}
