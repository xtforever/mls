#ifndef M_HTTP_SERVER_H
#define M_HTTP_SERVER_H

#include "m_hdf.h"
#include "mls.h"

typedef struct {
	int port;
	int host;     // string handle
	int root_dir; // string handle
	int routes;   // table handle: path -> route_info handle
} http_server_config_t;

typedef enum {
	ROUTE_TYPE_FILE,
	ROUTE_TYPE_JSON,
	ROUTE_TYPE_TEXT,
	ROUTE_TYPE_ECHO
} route_type_t;

typedef struct {
	route_type_t type;
	int target; // string handle (filename or content)
} http_route_t;

/**
 * @brief Load server configuration from an HDF node.
 * @param h HDF node handle (the (server ...) list).
 * @return Configuration struct.
 */
http_server_config_t http_server_config_load (int h);

/**
 * @brief Free server configuration.
 */
void http_server_config_free (http_server_config_t *conf);

/**
 * @brief Start the HTTP server with the given configuration.
 * This function blocks.
 */
void http_server_run (http_server_config_t *conf);

#endif
