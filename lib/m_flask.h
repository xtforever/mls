#ifndef M_FLASK_H
#define M_FLASK_H

#include "mls.h"
#include "m_http.h"

typedef void (*flask_handler_t)(int req, int res);

/**
 * @brief Initialize the flask application.
 */
int flask_init();

/**
 * @brief Register a C function handler by name.
 * @param name The name to match in HDF (e.g., "hello_handler").
 * @param handler The function pointer.
 */
void flask_register(const char *name, flask_handler_t handler);

/**
 * @brief Run the server using an HDF configuration file.
 * @param hdf_path Path to the .hdf config.
 */
void flask_run(const char *hdf_path);

/**
 * @brief Get the root handle of the HDF configuration.
 */
int flask_get_config();

// --- Request Convenience API ---

const char *flask_method(int req);
const char *flask_uri(int req);
const char *flask_header(int req, const char *key);
const char *flask_body(int req);
const char *flask_arg(int req, const char *key, const char *default_val);

// --- Response Convenience API ---

void flask_status(int res, int code);
void flask_set_header(int res, const char *key, const char *val);
void flask_printf(int res, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void flask_json(int res, int status, const char *json);
void flask_json_h(int res, int status, int json_h);
void flask_file(int res, const char *path);


#endif
