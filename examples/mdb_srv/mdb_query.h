#ifndef MDB_QUERY_H
#define MDB_QUERY_H

#include <stddef.h>

/*
 * Run the fixed demo query program against MariaDB and return a malloc'd
 * JSON string (caller frees), or NULL on failure with a message in err.
 *
 * Declared without any <mysql.h> types: that header's ma_list.h defines
 * list_add()/list_free(), which collide with MLS's list_* symbols when both
 * are included in the same translation unit.  The MariaDB client therefore
 * lives alone in mdb_query.c.
 */
char *mdb_query_demo_json (const char *host, int port, const char *user,
			   const char *pass, const char *db, int sleep_ms,
			   char *err, size_t errn);

#endif
