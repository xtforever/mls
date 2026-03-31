#include "mls.h"
#include <stdio.h>
#include <sys/wait.h>
#define EXEC(a)                                                                \
	do {                                                                   \
		puts ("\nStarting: " #a "()\n");                               \
		if (!fork ())                                                  \
			a ();                                                  \
		wait (NULL);                                                   \
		puts ("...\n");                                                \
	} while (0)
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

typedef void (*debug_fn_t) (void);

/**
 * Runs fn() in a child process, captures stdout+stderr into buffer.
 *
 * Returns:
 *   >=0 : child's exit code
 *   -1  : internal error (errno set)
 */
int debug_exec (debug_fn_t fn, int buffer)
{
	int pipefd[2];
	pid_t pid;

	if (buffer < 1 || pipe (pipefd) == -1) {
		return -1;
	}

	pid = fork ();
	if (pid == -1) {
		close (pipefd[0]);
		close (pipefd[1]);
		return -1;
	}

	if (pid == 0) {
		// --- CHILD ---
		close (pipefd[0]); // close read end

		// Redirect stdout and stderr to pipe
		dup2 (pipefd[1], STDOUT_FILENO);
		dup2 (pipefd[1], STDERR_FILENO);
		close (pipefd[1]);
		trace_level = 0;
		m_free (buffer); // inherited from parent
		// Run the function
		fn ();

		// If fn() returns instead of exiting
		_exit (0);
	}

	// --- PARENT ---
	close (pipefd[1]); // close write end

	ssize_t total = 0;
	ssize_t n = 0;
	m_clear (buffer);
	do {
		total += n;
		m_setlen (buffer, total + 4096);
		n = read (pipefd[0], mls (buffer, total), 4096);
	} while (n > 0);
	m_setlen (buffer, total);
	m_putc (buffer, 0);
	close (pipefd[0]);

	int status;
	if (waitpid (pid, &status, 0) == -1) {
		return -1;
	}

	if (WIFEXITED (status)) {
		return WEXITSTATUS (status);
	} else if (WIFSIGNALED (status)) {
		return 128 + WTERMSIG (status); // bash-style
	}

	return -1;
}

int create_string (void)
{
	int mystring = s_printf (0, 0, "%s", "Hello World");
	return mystring;
}

void delete_string (int str) { m_free (str); }

void rep (const char *s)
{
	puts ("\n");
	puts (s);
	puts ("--------------------------------------------------------------");
	fflush (stdout);
}

void error_double_free (void)
{
	rep ("Report that the string was allready freed");
	int mystring = create_string ();
	delete_string (mystring);
	m_free (mystring);
}

void error_out_of_bounds (void)
{
	rep ("Report that the Index 30 is not allocated");
	int mystring = create_string ();
	*(char *)mls (mystring, 30) = 'x';
	m_free (mystring);
}

void error_handle (void)
{
	rep ("Report that there is no List with that number");
	int wrong = 264673;
	*(char *)mls (wrong, 30) = 'x';
}

void error_access_list (void)
{
	rep ("Report that there is a list but it was freed by the function "
	     "delete_string");
	int strlist = m_alloc (10, sizeof (char *), 0);
	delete_string (strlist);
	(void)m_buf (strlist);
}

void uaf_protection (void)
{
	rep ("this handle has been reused, it is not valid");
	int l1 = m_create (10, 1);
	int l2 = m_create (10, 1);
	int l3 = m_create (10, 1);
	m_free (l1);
	int n1 = m_create (10, 1);
	// n1 and l1 have the same handle (excluding the uaf protection)
	m_putc (l1, 0);
}

int main (int argc, char **argv)
{
	m_init ();
	trace_level = 0;
	int buffer = m_alloc (4096, 1, 0);
	trace_level = 0;
	debug_exec (error_access_list, buffer);
	puts (m_str (buffer));
	m_free (buffer);

	EXEC (error_double_free);
	EXEC (error_out_of_bounds);
	EXEC (error_handle);
	EXEC (uaf_protection);

	m_destruct ();
}
