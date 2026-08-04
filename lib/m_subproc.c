#include "m_subproc.h"
#include "m_tool.h"
#include <signal.h>
#include <unistd.h>
#include <poll.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>

static int exit_code (int status)
{
	if (WIFEXITED (status))
		return WEXITSTATUS (status);
	if (WIFSIGNALED (status))
		return 128 + WTERMSIG (status);
	return -1;
}

static pid_t subproc_fork (const char *cmd, int out_pipe[2], int err_pipe[2])
{
	if (out_pipe && pipe (out_pipe) < 0)
		return -1;
	if (err_pipe && pipe (err_pipe) < 0) {
		close (out_pipe[0]);
		close (out_pipe[1]);
		return -1;
	}
	pid_t pid = fork ();
	if (pid < 0) {
		close (out_pipe ? out_pipe[0] : -1);
		close (out_pipe ? out_pipe[1] : -1);
		close (err_pipe ? err_pipe[0] : -1);
		close (err_pipe ? err_pipe[1] : -1);
		return -1;
	}
	if (pid == 0) {
		if (out_pipe) {
			close (out_pipe[0]);
			dup2 (out_pipe[1], STDOUT_FILENO);
			close (out_pipe[1]);
		}
		if (err_pipe) {
			close (err_pipe[0]);
			dup2 (err_pipe[1], STDERR_FILENO);
			close (err_pipe[1]);
		}
		close_range (3, ~0U, 0);
		execl ("/bin/sh", "sh", "-c", cmd, (char *)NULL);
		_exit (127);
	}
	if (out_pipe)
		close (out_pipe[1]);
	if (err_pipe)
		close (err_pipe[1]);
	return pid;
}

int subproc_run (const char *cmd, int *stdout_h, int *stderr_h, int timeout_ms)
{
	int out_p[2] = { -1, -1 }, err_p[2] = { -1, -1 };
	int buffers[2] = { 0, 0 }, rc = -1;
	pid_t pid = subproc_fork (cmd, out_p, err_p);
	if (pid < 0)
		goto done;

	fcntl (out_p[0], F_SETFL, O_NONBLOCK);
	fcntl (err_p[0], F_SETFL, O_NONBLOCK);
	buffers[0] = s_new ();
	buffers[1] = s_new ();

	struct pollfd fds[2] = {
		{ out_p[0], POLLIN, 0 },
		{ err_p[0], POLLIN, 0 }
	};
	int active = 2;
	while (active > 0) {
		int ret = poll (fds, 2, timeout_ms > 0 ? timeout_ms : -1);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		if (ret == 0) {
			kill (pid, SIGKILL);
			waitpid (pid, NULL, 0);
			pid = -1;
			rc = -1;
			goto done;
		}
		char buf[8192];
		for (int i = 0; i < 2; i++) {
			if (fds[i].revents & (POLLIN | POLLHUP)) {
				ssize_t n = read (fds[i].fd, buf, sizeof (buf));
				if (n > 0)
					m_write (buffers[i], m_len (buffers[i]), buf, n);
				if (n <= 0) {
					close (fds[i].fd);
					fds[i].fd = -1;
					active--;
				}
			}
		}
	}
	m_putc (buffers[0], 0);
	m_putc (buffers[1], 0);
	{
		int status;
		if (pid > 0) {
			waitpid (pid, &status, 0);
			rc = exit_code (status);
		}
	}

done:
	if (pid < 0)
		rc = -1;
	if (out_p[0] >= 0)
		close (out_p[0]);
	if (err_p[0] >= 0)
		close (err_p[0]);
	if (rc < 0) {
		m_free (buffers[0]);
		m_free (buffers[1]);
		buffers[0] = buffers[1] = 0;
	}
	if (stdout_h)
		*stdout_h = buffers[0];
	else
		m_free (buffers[0]);
	if (stderr_h)
		*stderr_h = buffers[1];
	else
		m_free (buffers[1]);
	return rc;
}

int subproc_read (const char *cmd)
{
	int h = 0;
	int rc = subproc_run (cmd, &h, NULL, 0);
	if (rc != 0) { m_free (h); return 0; }
	return h;
}

int subproc_lines (const char *cmd)
{
	int out_h = 0, err = 0;
	int rc = subproc_run (cmd, &out_h, &err, 0);
	m_free (err);
	if (rc != 0 || !out_h) {
		m_free (out_h);
		return 0;
	}
	int nl = s_dup ("\n");
	int list = s_msplit (0, out_h, nl);
	m_free (out_h);
	m_free (nl);
	return list;
}
