#define _POSIX_C_SOURCE 200809L

#include "m_ftransport.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static void put_u32 (unsigned char *p, uint32_t v)
{
	p[0] = (unsigned char)(v & 0xFF);
	p[1] = (unsigned char)((v >> 8) & 0xFF);
	p[2] = (unsigned char)((v >> 16) & 0xFF);
	p[3] = (unsigned char)((v >> 24) & 0xFF);
}

static uint32_t get_u32 (const unsigned char *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
	       ((uint32_t)p[3] << 24);
}

static int write_all (int fd, const void *buf, size_t len)
{
	const unsigned char *p = buf;
	while (len) {
		ssize_t n = write (fd, p, len);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		p += n;
		len -= (size_t)n;
	}
	return 0;
}

static int read_all (int fd, void *buf, size_t len)
{
	unsigned char *p = buf;
	while (len) {
		ssize_t n = read (fd, p, len);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		if (n == 0)
			return -1;
		p += n;
		len -= (size_t)n;
	}
	return 0;
}

int m_ftransport_listen (const char *path)
{
	if (!path)
		return -1;
	struct sockaddr_un addr;
	memset (&addr, 0, sizeof addr);
	addr.sun_family = AF_UNIX;
	if (strlen (path) >= sizeof addr.sun_path)
		return -1;
	strcpy (addr.sun_path, path);
	unlink (path); /* stale socket from a previous run */

	int fd = socket (AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0)
		return -1;
	if (bind (fd, (struct sockaddr *)&addr, sizeof addr) != 0 ||
	    listen (fd, 8) != 0) {
		close (fd);
		return -1;
	}
	return fd;
}

int m_ftransport_accept (int listen_fd)
{
	for (;;) {
		int fd = accept (listen_fd, NULL, NULL);
		if (fd >= 0)
			return fd;
		if (errno != EINTR)
			return -1;
	}
}

int m_ftransport_connect (const char *path)
{
	if (!path)
		return -1;
	struct sockaddr_un addr;
	memset (&addr, 0, sizeof addr);
	addr.sun_family = AF_UNIX;
	if (strlen (path) >= sizeof addr.sun_path)
		return -1;
	strcpy (addr.sun_path, path);

	int fd = socket (AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0)
		return -1;
	if (connect (fd, (struct sockaddr *)&addr, sizeof addr) != 0) {
		close (fd);
		return -1;
	}
	return fd;
}

int m_ftransport_send (int fd, const void *payload, uint32_t len)
{
	if (fd < 0 || (len && !payload) || len > M_FTRANSPORT_MAX_FRAME)
		return -1;
	unsigned char hdr[4];
	put_u32 (hdr, len);
	if (write_all (fd, hdr, sizeof hdr) != 0)
		return -1;
	if (len && write_all (fd, payload, len) != 0)
		return -1;
	return 0;
}

int m_ftransport_recv (int fd, void *buf, uint32_t cap, uint32_t *out_len)
{
	if (fd < 0 || !buf || !out_len)
		return -1;
	unsigned char hdr[4];
	if (read_all (fd, hdr, sizeof hdr) != 0)
		return -1;
	uint32_t len = get_u32 (hdr);
	if (len > M_FTRANSPORT_MAX_FRAME || len > cap)
		return -1; /* oversized or truncated frame */
	if (len && read_all (fd, buf, len) != 0)
		return -1;
	*out_len = len;
	return 0;
}

int m_ftransport_recv_alloc (int fd, char **out, uint32_t *out_len)
{
	if (!out || !out_len)
		return -1;
	unsigned char hdr[4];
	if (read_all (fd, hdr, sizeof hdr) != 0)
		return -1;
	uint32_t len = get_u32 (hdr);
	if (len > M_FTRANSPORT_MAX_FRAME)
		return -1;
	char *buf = malloc ((size_t)len + 1);
	if (!buf)
		return -1;
	if (len && read_all (fd, buf, len) != 0) {
		free (buf);
		return -1;
	}
	buf[len] = '\0';
	*out = buf;
	*out_len = len;
	return 0;
}

void m_ftransport_close (int fd)
{
	if (fd >= 0)
		close (fd);
}
