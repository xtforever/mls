#ifndef M_FTRANSPORT_H
#define M_FTRANSPORT_H

#include <stdint.h>

/* Length-prefixed Unix-socket transport: uint32 little-endian length + payload.
   Max frame 64 MiB. All functions return 0 on success, -1 on error. */
#define M_FTRANSPORT_MAX_FRAME (64u * 1024u * 1024u)

int m_ftransport_listen (const char *path);  /* bind+listen, returns fd */
int m_ftransport_accept (int listen_fd);     /* returns connection fd */
int m_ftransport_connect (const char *path); /* returns connection fd */

int m_ftransport_send (int fd, const void *payload, uint32_t len);
int m_ftransport_recv (int fd, void *buf, uint32_t cap, uint32_t *out_len);
int m_ftransport_recv_alloc (int fd, char **out, uint32_t *out_len);

void m_ftransport_close (int fd);

#endif
