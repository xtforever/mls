#include "m_fapi.h"

#include "m_fclient.h"
#include "m_fproto.h"
#include "strbuf.h"

#include <stdlib.h>
#include <string.h>

#define FDB_MAX_CONN 32
#define FDB_MAX_RS 128

typedef struct {
	int kind; /* 0 = in-process engine, 1 = socket client */
	m_engine_t *engine;
	m_fclient_t *client;
} fdb_conn_t;

static fdb_conn_t conns[FDB_MAX_CONN];
static m_fproto_result_t *results[FDB_MAX_RS];
static m_engine_t *default_engine;

int fdb_set_default_engine (m_engine_t *e)
{
	default_engine = e;
	return 0;
}

static int register_conn (int kind, m_engine_t *e, m_fclient_t *c)
{
	for (int i = 0; i < FDB_MAX_CONN; i++)
		if (!conns[i].engine && !conns[i].client) {
			conns[i].kind = kind;
			conns[i].engine = e;
			conns[i].client = c;
			return i + 1;
		}
	return -1;
}

int fdb_open_engine (m_engine_t *e)
{
	return e ? register_conn (0, e, NULL) : -1;
}

int fdb_open (const char *socket_path)
{
	if (default_engine)
		return fdb_open_engine (default_engine);
	m_fclient_t *c = m_fclient_connect (socket_path);
	if (!c)
		return -1;
	int conn = register_conn (1, NULL, c);
	if (conn < 0)
		m_fclient_close (c);
	return conn;
}

void fdb_close (int conn)
{
	if (conn < 1 || conn > FDB_MAX_CONN)
		return;
	if (conns[conn - 1].client)
		m_fclient_close (conns[conn - 1].client);
	conns[conn - 1].client = NULL;
	conns[conn - 1].engine = NULL;
	conns[conn - 1].kind = 0;
}

static fdb_conn_t *get_conn (int conn)
{
	if (conn < 1 || conn > FDB_MAX_CONN)
		return NULL;
	fdb_conn_t *c = &conns[conn - 1];
	return (c->engine || c->client) ? c : NULL;
}

static void append_escaped (strbuf_t *b, const char *s)
{
	for (; s && *s; s++) {
		switch (*s) {
		case '"':
			sb_append_s (b, "\\\"");
			break;
		case '\\':
			sb_append_s (b, "\\\\");
			break;
		case '\n':
			sb_append_s (b, "\\n");
			break;
		case '\t':
			sb_append_s (b, "\\t");
			break;
		default:
			sb_append_c (b, *s);
			break;
		}
	}
}

static void script_string (strbuf_t *b, char kind, const char *s)
{
	sb_append_c (b, kind);
	sb_append_c (b, '"');
	sb_append_c (b, ' '); /* S" and Z" must be tokens on their own */
	append_escaped (b, s);
	sb_append_c (b, '"');
	sb_append_c (b, ' ');
}

static int register_result (m_fproto_result_t *r)
{
	for (int i = 0; i < FDB_MAX_RS; i++)
		if (!results[i]) {
			results[i] = r;
			return i + 1;
		}
	return 0;
}

static m_fproto_result_t *get_result (int rs)
{
	if (rs < 1 || rs > FDB_MAX_RS)
		return NULL;
	return results[rs - 1];
}

static const char *exec_conn (int conn, const char *script, int *err)
{
	fdb_conn_t *c = get_conn (conn);
	if (!c) {
		*err = -1;
		return "";
	}
	if (c->kind == 0) {
		const char *r = m_engine_exec (c->engine, script);
		*err = m_engine_error (c->engine);
		return r;
	}
	const char *r = m_fclient_exec (c->client, script);
	*err = m_fclient_error (c->client);
	return r;
}

int fdb_select (int conn, const char *db, const char *table, const char *field,
		const char *value)
{
	(void)db;
	if (!table || !field || !value)
		return 0;
	strbuf_t *b = sb_new ();
	if (!b)
		return 0;
	script_string (b, 'Z', table);
	sb_append_s (b, "DB-TABLE DROP ");
	script_string (b, 'Z', field);
	script_string (b, 'S', value);
	sb_append_s (b, "DB-FIND DROP DB-FETCH DROP");

	int err = -1;
	const char *resp = exec_conn (conn, sb_cstr (b), &err);
	sb_free (b);
	if (err || strncmp (resp, "RESULT", 6) != 0)
		return 0;
	m_fproto_result_t *r = m_fproto_parse (resp);
	if (!r)
		return 0;
	int rs = register_result (r);
	if (!rs)
		m_fproto_result_free (r);
	return rs;
}

int fdb_insert (int conn, const char *db, const char *table, const char *field,
		const char *value)
{
	(void)db;
	if (!table || !field || !value)
		return 0;
	strbuf_t *b = sb_new ();
	if (!b)
		return 0;
	script_string (b, 'Z', table);
	sb_append_s (b, "DB-TABLE DROP ");
	script_string (b, 'Z', field);
	script_string (b, 'S', value);
	sb_append_s (b, "DB-PUT-STR DB-INSERT");

	int err = -1;
	const char *resp = exec_conn (conn, sb_cstr (b), &err);
	sb_free (b);
	if (err)
		return 0;

	fdb_conn_t *c = get_conn (conn);
	if (c && c->kind == 0)
		return (int)m_engine_last_recno (c->engine);

	/* socket backend: the emitted row carries the new record; its first
	   integer column is the PK/recno in the default schema. */
	m_fproto_result_t *r = m_fproto_parse (resp);
	int uid = 1;
	if (r && r->nrows > 0 && r->ncols > 0 &&
	    r->values[0].type == M_FPROTO_INT)
		uid = (int)r->values[0].i;
	m_fproto_result_free (r);
	return uid;
}

int fdb_delete (int conn, const char *db, const char *table, const char *field,
		const char *value)
{
	(void)db;
	if (!table || !field || !value)
		return 0;
	strbuf_t *b = sb_new ();
	if (!b)
		return 0;
	script_string (b, 'Z', table);
	sb_append_s (b, "DB-TABLE DROP ");
	script_string (b, 'Z', field);
	script_string (b, 'S', value);
	sb_append_s (b, "DB-FIND DROP DB-DELETE DROP");

	int err = -1;
	(void)exec_conn (conn, sb_cstr (b), &err);
	sb_free (b);
	return err ? 0 : 1;
}

int fdb_result_cols (int rs)
{
	m_fproto_result_t *r = get_result (rs);
	return r ? r->ncols : 0;
}

const char *fdb_result_colname (int rs, int i)
{
	m_fproto_result_t *r = get_result (rs);
	return r && i >= 0 && i < r->ncols ? r->cols[i].name : NULL;
}

const char *fdb_result_coltype (int rs, int i)
{
	m_fproto_result_t *r = get_result (rs);
	return r && i >= 0 && i < r->ncols ? r->cols[i].type : NULL;
}

int fdb_result_rows (int rs)
{
	m_fproto_result_t *r = get_result (rs);
	return r ? r->nrows : 0;
}

long long fdb_result_int (int rs, int row, int col)
{
	m_fproto_result_t *r = get_result (rs);
	if (!r || row < 0 || row >= r->nrows || col < 0 || col >= r->ncols)
		return 0;
	m_fproto_value_t *v = &r->values[row * r->ncols + col];
	return v->type == M_FPROTO_INT ? (long long)v->i : 0;
}

const char *fdb_result_str (int rs, int row, int col)
{
	m_fproto_result_t *r = get_result (rs);
	if (!r || row < 0 || row >= r->nrows || col < 0 || col >= r->ncols)
		return NULL;
	m_fproto_value_t *v = &r->values[row * r->ncols + col];
	return (v->type == M_FPROTO_STR || v->type == M_FPROTO_ZSTR) ? v->s
								     : NULL;
}

void fdb_result_free (int rs)
{
	if (rs < 1 || rs > FDB_MAX_RS)
		return;
	m_fproto_result_free (results[rs - 1]);
	results[rs - 1] = NULL;
}
