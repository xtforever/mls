#include "m_fserver.h"

#include "m_ftransport.h"
#include "m_isam.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int main (void)
{
	m_isam_db_t *db = m_isam_db_new ();
	assert (db);
	m_isam_col_t cols[] = {{"id", 'i'}, {"name", 's'}, {"age", 'i'}};
	const char *pk[] = {"id"};
	m_isam_table_t *t = m_isam_create_table (db, "people", cols, 3, pk, 1);
	assert (t);
	uint32_t john = m_isam_intern (t, "John");
	assert (m_isam_put (t, "id", 1) == 0);
	assert (m_isam_put (t, "name", john) == 0);
	assert (m_isam_put (t, "age", 30) == 0);
	assert (m_isam_insert (t) == 1);

	char path[128];
	snprintf (path, sizeof path, "/tmp/mls_fserver_%ld.sock",
		  (long)getpid ());
	m_fserver_t *srv = m_fserver_new (path, db);
	assert (srv);

	pid_t pid = fork ();
	assert (pid >= 0);
	if (pid == 0) {
		int rc = m_fserver_serve_one (srv);
		m_fserver_free (srv);
		m_isam_db_free (db);
		_exit (rc == 0 ? 0 : 1);
	}

	int cfd = m_ftransport_connect (path);
	assert (cfd >= 0);
	const char *script = "Z\" people\" DB-TABLE DROP "
			     "Z\" name\" S\" John\" DB-FIND DROP DB-FETCH DROP";
	assert (m_ftransport_send (cfd, script, (uint32_t)strlen (script)) ==
		0);

	char *resp = NULL;
	uint32_t len = 0;
	assert (m_ftransport_recv_alloc (cfd, &resp, &len) == 0);
	assert (strstr (resp, "RESULT COLS 3"));
	assert (strstr (resp, "ROW 1 S\"John\" 30"));
	assert (strstr (resp, "END"));
	free (resp);
	m_ftransport_close (cfd);

	int status = 0;
	assert (waitpid (pid, &status, 0) == pid);
	assert (WIFEXITED (status) && WEXITSTATUS (status) == 0);

	m_fserver_free (srv);
	m_isam_db_free (db);

	assert (m_fserver_new (NULL, db) == NULL);
	assert (m_fserver_serve_one (NULL) == -1);
	m_fserver_free (NULL);

	puts ("m_fserver selftest: OK");
	return 0;
}
