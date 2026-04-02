#include "m_flask.h"
#include "m_hdf.h"
#include "m_http_server.h"
#include "m_table.h"
#include "m_tool.h"
#include "m_extra.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <sys/socket.h>

typedef struct {
	int parser_h, method, uri, body, headers, args;
} flask_req_t;
typedef struct {
	int status, headers, body, client_fd;
} flask_res_t;

static int handler_registry = 0, flask_config_root = 0, route_table = 0;

int flask_init ()
{
	if (!handler_registry)
		handler_registry = m_table_create ();
	if (!route_table)
		route_table = m_table_create ();
	return 0;
}

int flask_get_config () { return flask_config_root; }
void flask_status (int res_h, int status)
{
	((flask_res_t *)m_buf (res_h))->status = status;
}
void flask_set_header (int res_h, const char *key, const char *val)
{
	m_table_set_string_by_cstr (((flask_res_t *)m_buf (res_h))->headers,
				    key, val);
}

void flask_register (const char *name, flask_handler_t handler)
{
	int h_ptr = m_alloc (sizeof (flask_handler_t), 1, MFREE);
	*(flask_handler_t *)m_buf (h_ptr) = handler;
	m_table_set_handle_by_cstr (handler_registry, name, h_ptr,
				    MLS_TABLE_TYPE_CUSTOM_HANDLE);
}

static void parse_args (int args_table, const char *uri)
{
	char *q = strchr (uri, '?');
	if (!q++)
		return;
	int p, parts = m_alloc (0, sizeof (char *), MFREE_STR);
	char **s;
	m_str_split (parts, q, "&", 1);
	m_foreach (parts, p, s)
	{
		char *eq = strchr (*s, '=');
		if (eq) {
			*eq = 0;
			m_table_set_string_by_cstr (args_table, *s, eq + 1);
		} else
			m_table_set_string_by_cstr (args_table, *s, "");
	}
	m_free (parts);
}

static void send_full_response (int fd, flask_res_t *res)
{
	int resp_h = s_printf (0, 0, "HTTP/1.1 %d OK\r\n", res->status);
	int i, keys = m_table_keys (res->headers);
	m_foreach (keys, i, (int *){0})
	{
		const char *k = m_str (INT (keys, i));
		int v_h = m_table_get_cstr (res->headers, k);
		s_printf (resp_h, -1, "%s: %s\r\n", k, m_str (v_h));
	}
	m_free (keys);
	if (!m_table_get_cstr (res->headers, "Content-Type"))
		s_app (resp_h, "Content-Type: text/plain\r\n", NULL);
	s_printf (resp_h, -1, "Content-Length: %d\r\nConnection: close\r\n\r\n",
		  (int)s_strlen (res->body));
	write (fd, m_buf (resp_h), s_strlen (resp_h));
	if (s_strlen (res->body) > 0)
		write (fd, m_buf (res->body), s_strlen (res->body));
	m_free (resp_h);
}

static void compile_routes (int server_node)
{
	int p, children = hdf_get_children (server_node);
	m_foreach (children, p, (int *){0})
	{
		int child = INT (children, p);
		const char *path = hdf_get_property (child, "path");
		const char *call = hdf_get_property (child, "call");
		if (path && call) {
			int h_ptr = m_table_get_cstr (handler_registry, call);
			if (h_ptr > 0)
				m_table_set_handle_by_cstr (
					route_table, path, h_ptr,
					MLS_TABLE_TYPE_CUSTOM_HANDLE);
		}
	}
}

static void process_client (int client_fd)
{
	http_parser_t parser;
	http_parser_init_request (&parser);
	int data_h = m_alloc (0, 1, MFREE);
	char buf[4096];
	while (1) {
		int n = read (client_fd, buf, sizeof (buf));
		if (n <= 0)
			break;
		m_write (data_h, m_len (data_h), buf, n);
		if (http_parse (&parser, data_h) == -1 ||
		    parser.state == HTTP_STATE_DONE)
			break;
	}
	if (parser.state == HTTP_STATE_DONE) {
		const char *uri_full = m_str (parser.uri);
		char path[1024];
		strncpy (path, uri_full, sizeof (path));
		char *q = strchr (path, '?');
		if (q)
			*q = 0;
		int h_ptr = m_table_get_cstr (route_table, path);
		if (h_ptr > 0) {
			int req_h = m_alloc (sizeof (flask_req_t), 1, MFREE);
			flask_req_t *req = m_buf (req_h);
			req->method = parser.method;
			req->uri = parser.uri;
			req->body = parser.body;
			req->headers = parser.headers;
			req->args = m_table_create ();
			parse_args (req->args, uri_full);
			int res_h = m_alloc (sizeof (flask_res_t), 1, MFREE);
			flask_res_t *res = m_buf (res_h);
			res->status = 200;
			res->headers = m_table_create ();
			res->body = m_alloc (0, 1, MFREE);
			(*(flask_handler_t *)m_buf (h_ptr)) (req_h, res_h);
			send_full_response (client_fd, res);
			m_table_free (req->args);
			m_table_free (res->headers);
			m_free (res->body);
			m_free (req_h);
			m_free (res_h);
		} else
			write (client_fd,
			       "HTTP/1.1 404 Not Found\r\nContent-Length: "
			       "9\r\n\r\nNot Found",
			       53);
	}
	m_free (data_h);
	http_parser_free (&parser);
	close (client_fd);
}

void flask_run (const char *hdf_path)
{
	int root = hdf_parse_file (hdf_path);
	if (root <= 0)
		return;
	flask_config_root = root;
	int server_node = hdf_find_node (root, "server");
	if (server_node <= 0)
		server_node = root;
	compile_routes (server_node);
	int port = hdf_get_int (server_node, "port", 8080),
	    s_fd = socket (AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_addr.s_addr = inet_addr (
			hdf_get_property (server_node, "host") ?: "0.0.0.0"),
		.sin_port = htons (port)};
	setsockopt (s_fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof (int));
	bind (s_fd, (struct sockaddr *)&addr, sizeof (addr));
	listen (s_fd, 10);
	printf ("Flask running on %d\n", port);
	while (1) {
		int c_fd = accept (s_fd, NULL, NULL);
		if (c_fd >= 0)
			process_client (c_fd);
	}
}

const char *flask_method (int req_h)
{
	return m_str (((flask_req_t *)m_buf (req_h))->method);
}
const char *flask_uri (int req_h)
{
	return m_str (((flask_req_t *)m_buf (req_h))->uri);
}
const char *flask_body (int req_h)
{
	return m_str (((flask_req_t *)m_buf (req_h))->body);
}
const char *flask_arg (int req_h, const char *key, const char *def)
{
	int v_h = m_table_get_cstr (((flask_req_t *)m_buf (req_h))->args, key);
	return v_h > 0 ? m_str (v_h) : def;
}
const char *flask_header (int req_h, const char *key)
{
	int k_h = s_lower (s_strdup_c (key)),
	    v_h = m_table_get_cstr (((flask_req_t *)m_buf (req_h))->headers,
				    m_str (k_h));
	const char *v = v_h > 0 ? m_str (v_h) : NULL;
	m_free (k_h);
	return v;
}
void flask_printf (int res_h, const char *fmt, ...)
{
	va_list ap;
	va_start (ap, fmt);
	vas_printf (((flask_res_t *)m_buf (res_h))->body, -1, fmt, ap);
	va_end (ap);
}
void flask_render (int req_h, int res_h, const char *fmt)
{
	flask_req_t *req = m_buf (req_h);
	flask_res_t *res = m_buf (res_h);
	char *expanded = se_string (req->args, fmt);
	s_app (res->body, expanded, NULL);
}
void flask_json_h (int res_h, int status, int json_h)
{
	flask_res_t *res = m_buf (res_h);
	res->status = status;
	m_table_set_string_by_cstr (res->headers, "Content-Type",
				    "application/json");
	m_clear (res->body);
	m_slice (res->body, 0, json_h, 0, -1);
}
void flask_json (int res_h, int status, const char *json)
{
	int h = s_strdup_c (json);
	flask_json_h (res_h, status, h);
	m_free (h);
}
