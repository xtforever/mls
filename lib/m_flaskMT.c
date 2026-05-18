#include "m_flask.h"
#include "m_hdf.h"

#ifndef MLS_THREAD_SAFE
#error "m_flaskMT.c requires MLS_THREAD_SAFE. Build with thread_safe=1."
#endif

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct {
	int client_fd;
} flask_mt_client_t;

static void *flask_mt_worker (void *arg)
{
	flask_mt_client_t *client = arg;
	int client_fd = client->client_fd;
	free (client);

	flask_process_client (client_fd);
	return NULL;
}

static void flask_mt_dispatch_client (int client_fd)
{
	pthread_t thread;
	flask_mt_client_t *client = malloc (sizeof (*client));

	if (!client) {
		close (client_fd);
		return;
	}
	client->client_fd = client_fd;

	if (pthread_create (&thread, NULL, flask_mt_worker, client) != 0) {
		free (client);
		flask_process_client (client_fd);
		return;
	}
	pthread_detach (thread);
}

void flask_run_mt (const char *hdf_path)
{
	int server_node = flask_prepare_run (hdf_path);
	if (server_node <= 0)
		return;

	int port = hdf_get_int (server_node, "port", 8080);
	int server_fd = socket (AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0) {
		WARN ("socket: %s", strerror (errno));
		return;
	}

	struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_addr.s_addr = inet_addr (
			hdf_get_property (server_node, "host") ?: "0.0.0.0"),
		.sin_port = htons (port)};

	setsockopt (server_fd, SOL_SOCKET, SO_REUSEADDR, &(int){1},
		    sizeof (int));
	if (bind (server_fd, (struct sockaddr *)&addr, sizeof (addr)) < 0) {
		WARN ("bind: %s", strerror (errno));
		close (server_fd);
		return;
	}
	if (listen (server_fd, 128) < 0) {
		WARN ("listen: %s", strerror (errno));
		close (server_fd);
		return;
	}

	TRACE (TRACE_FLASK, "Flask MT running on %d", port);
	while (1) {
		int client_fd = accept (server_fd, NULL, NULL);
		if (client_fd < 0) {
			if (errno == EINTR)
				continue;
			WARN ("accept: %s", strerror (errno));
			continue;
		}
		flask_mt_dispatch_client (client_fd);
	}
}
