/*
 * mdb_query.c — the only translation unit that includes <mysql.h>.
 *
 * MariaDB's ma_list.h defines list_add()/list_free(), which collide with the
 * mls list_* API if both headers meet in one TU.  Keeping the client isolated
 * here lets mdb_srv.c stay a plain mls program.
 *
 * A tiny fixed program: connect, run three queries, emit JSON, exit.
 */

#define _GNU_SOURCE
#include "mdb_query.h"

#include <mysql.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void json_escape (FILE *f, const char *s)
{
	for (; *s; s++) {
		unsigned char c = (unsigned char)*s;
		if (c == '"' || c == '\\')
			fprintf (f, "\\%c", c);
		else if (c == '\n')
			fputs ("\\n", f);
		else if (c < 0x20)
			fprintf (f, "\\u%04x", c);
		else
			fputc (c, f);
	}
}

char *mdb_query_demo_json (const char *host, int port, const char *user,
			   const char *pass, const char *db, int sleep_ms,
			   char *err, size_t errn)
{
	MYSQL *m = mysql_init (NULL);
	if (!m) {
		snprintf (err, errn, "mysql_init failed");
		return NULL;
	}
	if (!mysql_real_connect (m, host, user, pass, db, (unsigned)port, NULL,
				 0)) {
		snprintf (err, errn, "connect: %s", mysql_error (m));
		mysql_close (m);
		return NULL;
	}

	/* hold the backend connection, modelling a slow MariaDB response
	 * (SLEEP takes seconds, so convert from ms) */
	if (sleep_ms > 0) {
		char q[64];
		snprintf (q, sizeof q, "SELECT SLEEP(%.3f)", sleep_ms / 1000.0);
		if (mysql_query (m, q)) {
			snprintf (err, errn, "sleep: %s", mysql_error (m));
			mysql_close (m);
			return NULL;
		}
		MYSQL_RES *sr = mysql_store_result (m);
		if (sr)
			mysql_free_result (sr);
	}

	char *buf = NULL;
	size_t len = 0;
	FILE *f = open_memstream (&buf, &len);
	if (!f) {
		snprintf (err, errn, "open_memstream failed");
		mysql_close (m);
		return NULL;
	}

	fprintf (f, "{\"worker\":%d", (int)getpid ());

	MYSQL_RES *r;
	MYSQL_ROW row;

	if (!mysql_query (m, "SELECT COUNT(*) FROM items")) {
		r = mysql_store_result (m);
		row = r ? mysql_fetch_row (r) : NULL;
		fprintf (f, ",\"count\":%s", row && row[0] ? row[0] : "0");
		if (r)
			mysql_free_result (r);
	}
	if (!mysql_query (m, "SELECT COALESCE(SUM(qty),0) FROM items")) {
		r = mysql_store_result (m);
		row = r ? mysql_fetch_row (r) : NULL;
		fprintf (f, ",\"sum_qty\":%s", row && row[0] ? row[0] : "0");
		if (r)
			mysql_free_result (r);
	}

	fputs (",\"items\":[", f);
	if (!mysql_query (
		    m, "SELECT name, qty FROM items ORDER BY id LIMIT 100")) {
		r = mysql_store_result (m);
		int first = 1;
		while (r && (row = mysql_fetch_row (r))) {
			if (!first)
				fputc (',', f);
			first = 0;
			fputs ("{\"name\":\"", f);
			json_escape (f, row[0] ? row[0] : "");
			fprintf (f, "\",\"qty\":%s}", row[1] ? row[1] : "0");
		}
		if (r)
			mysql_free_result (r);
	}
	fputs ("]}", f);

	fclose (f); /* flushes and NUL-terminates buf */
	mysql_close (m);
	return buf;
}
