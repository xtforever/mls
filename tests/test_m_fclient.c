#include "m_fclient.h"

#include "m_fserver.h"
#include "m_isam.h"

#include <assert.h>
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
	assert (m_isam_put (t, "id", 5) == 0);
	assert (m_isam_put (t, "name", m_isam_intern (t, "Ada")) == 0);
	assert (m_isam_put (t, "age", 36) == 0);
	assert (m_isam_insert (t) == 1);

	char path[128];
	snprintf (path, sizeof path, "/tmp/mls_fclient_%ld.sock",
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

	m_fclient_t *c = m_fclient_connect (path);
	assert (c);
	const char *resp = m_fclient_exec (
		c, "Z\" people\" DB-TABLE DROP "
		   "Z\" name\" S\" Ada\" DB-FIND DROP DB-FETCH DROP");
	assert (m_fclient_error (c) == 0);
	assert (strstr (resp, "RESULT COLS 3"));
	assert (strstr (resp, "ROW 5 S\"Ada\" 36"));
	assert (strstr (resp, "END"));
	m_fclient_close (c);

	int status = 0;
	assert (waitpid (pid, &status, 0) == pid);
	assert (WIFEXITED (status) && WEXITSTATUS (status) == 0);

	m_fserver_free (srv);
	m_isam_db_free (db);

	assert (m_fclient_connect (NULL) == NULL);
	assert (m_fclient_connect ("/tmp/does-not-exist.sock") == NULL);
	assert (m_fclient_error (NULL) == -1);
	assert (strcmp (m_fclient_exec (NULL, "x"), "") == 0);
	m_fclient_close (NULL);

	puts ("m_fclient selftest: OK");
	return 0;
}
