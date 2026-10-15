/*
 * mdb_srv — fork-per-request MariaDB demo service (PHP-FPM style) on m_flask.
 *
 * The master accepts an HTTP request, forks a worker process, and the worker
 * runs a small fixed program of MariaDB queries and exits.  Each forked child
 * gets a private copy-on-write snapshot of the address space, so no mls handle
 * is ever shared between workers.  The master only supervises: it reaps
 * children, enforces a per-request timeout and a per-child memory limit, and
 * logs everything to syslog (rsyslog facility LOG_LOCAL0).
 *
 * Modes:
 *   run [cfg]          start the supervisor (default; cfg default mdb_srv.hdf)
 *   dump-config [cfg]  parse + validate the config, print it, exit
 *   selftest           built-in checks (`make check`); needs no database
 *
 * Build: see Makefile.  libmariadb is optional; without it the SQL demo is
 * compiled as an echo stub.
 */

#define MLS_FULL_IMPLEMENTATION
#include "mls_full.h" /* must come first: the header sets _GNU_SOURCE */

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "mdb_query.h"

/* ------------------------------------------------------------------ */
/* syslog: one call straight to /dev/log, child PID tagged via LOG_PID. */
/* ------------------------------------------------------------------ */

static void srv_log (int prio, const char *fmt, ...)
{
	va_list ap;
	va_start (ap, fmt);
	vsyslog (prio, fmt, ap);
	va_end (ap);
}

/* ------------------------------------------------------------------ */
/* config                                                             */
/* ------------------------------------------------------------------ */

#define MDB_BACKLOG_DEFAULT 1024
#define MDB_TIMEOUT_MS_DEFAULT 5000
#define MDB_MEM_LIMIT_KB_DEFAULT 65536
#define MDB_MAX_CHILDREN_DEFAULT 8
#define MDB_KILL_GRACE_MS_DEFAULT 1000

typedef struct {
	int port; /* 0 = ephemeral (selftest only) */
	char host[64];
	int backlog; /* listen() accept-queue depth (>= expected concurrency) */
	int timeout_ms;
	int mem_limit_kb;
	int max_children;
	int kill_grace_ms;
	/* MariaDB DSN (localhost demo) */
	char db_host[64];
	int db_port;
	char db_user[64];
	char db_pass[128];
	char db_name[64];
} server_config_t;

static server_config_t g_cfg;

static void copy_str (char *dst, size_t n, const char *src)
{
	if (!src || !src[0]) {
		dst[0] = 0;
		return;
	}
	snprintf (dst, n, "%s", src);
}

static void load_config (int node, server_config_t *c)
{
	memset (c, 0, sizeof *c);
	c->port = hdf_get_int (node, "port", 8080);
	copy_str (c->host, sizeof c->host, hdf_get_property (node, "host"));
	if (!c->host[0])
		copy_str (c->host, sizeof c->host, "0.0.0.0");
	c->backlog = hdf_get_int (node, "backlog", MDB_BACKLOG_DEFAULT);
	c->timeout_ms =
		hdf_get_int (node, "timeout_ms", MDB_TIMEOUT_MS_DEFAULT);
	c->mem_limit_kb =
		hdf_get_int (node, "mem_limit_kb", MDB_MEM_LIMIT_KB_DEFAULT);
	c->max_children =
		hdf_get_int (node, "max_children", MDB_MAX_CHILDREN_DEFAULT);
	c->kill_grace_ms =
		hdf_get_int (node, "kill_grace_ms", MDB_KILL_GRACE_MS_DEFAULT);
	copy_str (c->db_host, sizeof c->db_host,
		  hdf_get_property (node, "db_host"));
	c->db_port = hdf_get_int (node, "db_port", 3306);
	copy_str (c->db_user, sizeof c->db_user,
		  hdf_get_property (node, "db_user"));
	copy_str (c->db_pass, sizeof c->db_pass,
		  hdf_get_property (node, "db_pass"));
	copy_str (c->db_name, sizeof c->db_name,
		  hdf_get_property (node, "db_name"));
}

/* trust boundary: reject anything that would misconfigure the server. */
static int validate_config (const server_config_t *c, char *err, size_t errn)
{
#define BAD(...)                                                               \
	do {                                                                   \
		snprintf (err, errn, __VA_ARGS__);                             \
		return -1;                                                     \
	} while (0)

	if (c->port < 0 || c->port > 65535)
		BAD ("port out of range: %d", c->port);
	if (c->backlog <= 0)
		BAD ("backlog must be > 0: %d", c->backlog);
	if (c->timeout_ms <= 0)
		BAD ("timeout_ms must be > 0: %d", c->timeout_ms);
	if (c->mem_limit_kb <= 0)
		BAD ("mem_limit_kb must be > 0: %d", c->mem_limit_kb);
	if (c->max_children <= 0)
		BAD ("max_children must be > 0: %d", c->max_children);
	if (c->kill_grace_ms < 0)
		BAD ("kill_grace_ms must be >= 0: %d", c->kill_grace_ms);
	if (c->db_port <= 0 || c->db_port > 65535)
		BAD ("db_port out of range: %d", c->db_port);
	if (!c->db_host[0])
		BAD ("db_host missing");
	if (!c->db_user[0])
		BAD ("db_user missing");
	if (!c->db_name[0])
		BAD ("db_name missing");
	if (strcmp (c->host, "0.0.0.0") != 0) {
		struct in_addr a;
		if (inet_pton (AF_INET, c->host, &a) != 1)
			BAD ("host is not a numeric IPv4 address: %s", c->host);
	}
	return 0;
#undef BAD
}

static void dump_config (const server_config_t *c)
{
	printf ("mdb_srv config:\n");
	printf ("  bind         = %s:%d\n", c->host, c->port);
	printf ("  backlog      = %d\n", c->backlog);
	printf ("  timeout_ms   = %d\n", c->timeout_ms);
	printf ("  mem_limit_kb = %d\n", c->mem_limit_kb);
	printf ("  max_children = %d\n", c->max_children);
	printf ("  kill_grace_ms= %d\n", c->kill_grace_ms);
	printf ("  dsn          = %s@%s:%d/%s\n", c->db_user, c->db_host,
		c->db_port, c->db_name);
}

/* ------------------------------------------------------------------ */
/* handlers                                                           */
/* ------------------------------------------------------------------ */

/* The request only picks a named demo from a fixed whitelist; no SQL ever
 * travels over the wire. */
static void demo_handler (int req, int res)
{
	const char *op = flask_arg (req, "op", "select");
	int pid = (int)getpid ();

	if (strcmp (op, "slow") == 0)
		sleep (3600); /* must be killed by request_terminate_timeout */
	if (strcmp (op, "dbdelay") == 0) {
		/* simulate a slow MariaDB response, then run the queries */
		int ms = atoi (flask_arg (req, "ms", "1000"));
		if (ms < 0)
			ms = 0;
		if (ms > 10000)
			ms = 10000;
		struct timespec ts = {ms / 1000, (long)(ms % 1000) * 1000000L};
		nanosleep (&ts, NULL);
		op = "select";
	}
	if (strcmp (op, "memhog") == 0) {
		/* leak 1 MiB chunks to grow RSS until the supervisor kills us
		 */
		for (;;) {
			unsigned char *p = malloc (1u << 20);
			if (!p) {
				sleep (1); /* allocation refused */
				continue;
			}
			memset (p, 0xAA, 1u << 20);
			(void)p[0];
		}
	}
#ifdef HAVE_MARIADB
	if (strcmp (op, "select") == 0 || strcmp (op, "dbsleep") == 0) {
		int sleep_ms = 0;
		if (strcmp (op, "dbsleep") == 0) {
			sleep_ms = atoi (flask_arg (req, "ms", "1000"));
			if (sleep_ms < 0)
				sleep_ms = 0;
			if (sleep_ms > 10000)
				sleep_ms = 10000;
		}
		char err[256];
		char *json = mdb_query_demo_json (g_cfg.db_host, g_cfg.db_port,
						  g_cfg.db_user, g_cfg.db_pass,
						  g_cfg.db_name, sleep_ms, err,
						  sizeof err);
		if (!json) {
			srv_log (LOG_ERR, "worker %d: mariadb failed: %s", pid,
				 err);
			flask_json (res, 500,
				    "{\"error\":\"mariadb query failed\"}");
			return;
		}
		srv_log (LOG_INFO, "worker %d: demo queries done", pid);
		flask_json (res, 200, json);
		free (json);
		return;
	}
#else
	if (strcmp (op, "select") == 0) {
		flask_status (res, 200);
		flask_printf (res,
			      "{\"worker\":%d,\"note\":\"HAVE_MARIADB off\"}\n",
			      pid);
		return;
	}
#endif
	if (strcmp (op, "ping") == 0) {
		flask_status (res, 200);
		flask_printf (res, "{\"worker\":%d,\"op\":\"ping\"}\n", pid);
		return;
	}
	flask_status (res, 400);
	flask_printf (res, "{\"error\":\"unknown op\"}\n");
}

/* ------------------------------------------------------------------ */
/* startup                                                            */
/* ------------------------------------------------------------------ */

static int prepare (const char *hdf_path)
{
	flask_init ();
	flask_register ("demo_handler", demo_handler);

	int server_node = flask_prepare_run (hdf_path);
	if (server_node <= 0) {
		srv_log (LOG_ERR, "cannot parse config %s", hdf_path);
		return 0;
	}
	load_config (server_node, &g_cfg);

	char err[160];
	if (validate_config (&g_cfg, err, sizeof err) != 0) {
		srv_log (LOG_ERR, "invalid config: %s", err);
		fprintf (stderr, "mdb_srv: invalid config: %s\n", err);
		return 0;
	}
	return 1;
}

/* ------------------------------------------------------------------ */
/* supervisor: accept -> fork -> watch -> reap                        */
/* ------------------------------------------------------------------ */

#define MDB_POLL_TICK_MS 100
#define MDB_MEM_SCAN_MS 100

enum { CH_RUNNING = 0, CH_TERM_SENT, CH_KILL_SENT };

typedef struct {
	pid_t pid;
	long deadline_ms;  /* CLOCK_MONOTONIC deadline while CH_RUNNING */
	long term_sent_at; /* when SIGTERM was sent (CH_TERM_SENT) */
	int state;
	const char *reason; /* "timeout" or "memory", for the log */
} child_t;

static child_t *g_children;
static int g_nchildren;
static int g_listen_fd = -1;

/* selftest hook: when set, run_server reports the bound port and closes it */
static int g_notify_fd = -1;

/* supervisor counters (checked by the selftest) */
static long g_served;
static long g_timeout_kills;
static long g_mem_kills;

/* set by SIGINT/SIGTERM; the loop tears down after poll() returns */
static volatile sig_atomic_t g_stop = 0;

static void on_stop (int sig)
{
	(void)sig;
	g_stop = 1; /* async-signal-safe: only set the flag */
}

static long now_ms (void)
{
	struct timespec ts;
	clock_gettime (CLOCK_MONOTONIC, &ts);
	return (long)ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

static int find_child (pid_t pid)
{
	for (int i = 0; i < g_nchildren; i++)
		if (g_children[i].pid == pid)
			return i;
	return -1;
}

static void remove_child_at (int idx)
{
	if (idx < 0 || idx >= g_nchildren)
		return;
	g_children[idx] = g_children[--g_nchildren];
}

static void add_child (pid_t pid)
{
	if (g_nchildren >= g_cfg.max_children)
		return; /* admission is checked before fork */
	child_t *c = &g_children[g_nchildren++];
	c->pid = pid;
	c->deadline_ms = now_ms () + g_cfg.timeout_ms;
	c->term_sent_at = 0;
	c->state = CH_RUNNING;
	c->reason = NULL;
}

static int too_many_children (void)
{
	return g_nchildren >= g_cfg.max_children;
}

static void reap_children (void)
{
	int status;
	pid_t pid;
	while ((pid = waitpid (-1, &status, WNOHANG)) > 0) {
		int idx = find_child (pid);
		int code = WIFEXITED (status) ? WEXITSTATUS (status) : -1;
		int sig = WIFSIGNALED (status) ? WTERMSIG (status) : 0;
		if (idx >= 0) {
			if (g_children[idx].state == CH_RUNNING &&
			    WIFEXITED (status) && code == 0)
				g_served++;
			remove_child_at (idx);
		}
		srv_log (LOG_INFO,
			 "child %d exited (code=%d signal=%d, active=%d)", pid,
			 code, sig, g_nchildren);
	}
}

/* SIGTERM once, then SIGKILL after kill_grace_ms.  No sleeping in the loop:
 * the state machine is advanced on each poll wake-up. */
static void start_terminate (child_t *c, const char *reason, long now)
{
	c->state = CH_TERM_SENT;
	c->reason = reason;
	c->term_sent_at = now;
	kill (c->pid, SIGTERM);
	srv_log (LOG_WARNING, "child %d: %s -> SIGTERM", c->pid, reason);
}

static void escalate_if_due (child_t *c, long now)
{
	if (c->state == CH_TERM_SENT &&
	    now - c->term_sent_at >= g_cfg.kill_grace_ms) {
		c->state = CH_KILL_SENT;
		kill (c->pid, SIGKILL);
		srv_log (LOG_WARNING, "child %d: %s -> SIGKILL", c->pid,
			 c->reason);
	}
}

static void check_deadlines (long now)
{
	for (int i = 0; i < g_nchildren; i++) {
		child_t *c = &g_children[i];
		if (c->state == CH_RUNNING && now >= c->deadline_ms) {
			start_terminate (c, "timeout", now);
			g_timeout_kills++;
		}
		escalate_if_due (c, now);
	}
}

/* earliest thing the loop must wake for: a deadline, a grace expiry, or the
 * reap/memory tick. */
static int next_poll_ms (long now)
{
	long wait = MDB_POLL_TICK_MS;
	for (int i = 0; i < g_nchildren; i++) {
		child_t *c = &g_children[i];
		long d;
		if (c->state == CH_RUNNING)
			d = c->deadline_ms - now;
		else if (c->state == CH_TERM_SENT)
			d = c->term_sent_at + g_cfg.kill_grace_ms - now;
		else
			continue;
		if (d < wait)
			wait = d;
	}
	if (wait < 0)
		wait = 0;
	if (wait > MDB_POLL_TICK_MS)
		wait = MDB_POLL_TICK_MS;
	return (int)wait;
}

/* Resident set size of a live child, in kB, or -1 if it is already gone.
 * ponytail: RSS polling can lose the race against a fast allocator memset,
 * so a worker may OOM the box before the scan fires; the upgrade path is a
 * hard RLIMIT_AS backstop set in the child (used by the mem-hog selftest). */
static long read_vmrss_kb (pid_t pid)
{
	char path[64];
	snprintf (path, sizeof path, "/proc/%d/status", (int)pid);
	FILE *fp = fopen (path, "r");
	if (!fp)
		return -1; /* child already exited */
	long kb = -1;
	char line[256];
	while (fgets (line, sizeof line, fp)) {
		if (strncmp (line, "VmRSS:", 6) == 0) {
			char *p = line + 6;
			while (*p == ' ' || *p == '\t')
				p++;
			kb = strtol (p, NULL, 10);
			break;
		}
	}
	fclose (fp);
	return kb;
}

static void check_memory (long now)
{
	static long last_scan;
	if (now - last_scan < MDB_MEM_SCAN_MS)
		return;
	last_scan = now;
	for (int i = 0; i < g_nchildren; i++) {
		child_t *c = &g_children[i];
		if (c->state != CH_RUNNING)
			continue;
		long kb = read_vmrss_kb (c->pid);
		if (kb < 0)
			continue; /* reap_children will clean it up */
		if (kb > g_cfg.mem_limit_kb) {
			srv_log (LOG_WARNING,
				 "child %d: VmRSS %ldkB > limit %dkB", c->pid,
				 kb, g_cfg.mem_limit_kb);
			start_terminate (c, "memory", now);
			g_mem_kills++;
		}
	}
}

static void write_all (int fd, const void *buf, size_t n)
{
	const char *p = buf;
	while (n) {
		ssize_t w = write (fd, p, n);
		if (w <= 0)
			return;
		p += w;
		n -= (size_t)w;
	}
}

static void send_503 (int fd)
{
	static const char body[] = "Too Many Requests";
	char hdr[160];
	int n = snprintf (hdr, sizeof hdr,
			  "HTTP/1.1 503 Service Unavailable\r\n"
			  "Content-Type: text/plain\r\n"
			  "Content-Length: %zu\r\n"
			  "Connection: close\r\n\r\n",
			  sizeof body - 1);
	if (n > 0)
		write_all (fd, hdr, (size_t)n);
	write_all (fd, body, sizeof body - 1);
	close (fd);
}

/*
 * Fork only at this clean point: the master is single-threaded and holds no
 * mls lock, so the child inherits a consistent snapshot.  The child serves
 * the request and _exit()s; it never runs the master's teardown or atexit.
 */
static void spawn_child (int client_fd)
{
	pid_t pid = fork ();
	if (pid < 0) {
		srv_log (LOG_ERR, "fork failed: %s", strerror (errno));
		send_503 (client_fd);
		return;
	}
	if (pid == 0) {
		/* worker: reset inherited dispositions, serve, leave */
		signal (SIGTERM, SIG_DFL);
		signal (SIGINT, SIG_DFL);
		signal (SIGCHLD, SIG_DFL);
		signal (SIGHUP, SIG_DFL);
		if (g_listen_fd >= 0)
			close (g_listen_fd);
		flask_process_client (client_fd);
		_exit (0);
	}
	/* master */
	close (client_fd);
	add_child (pid);
	srv_log (LOG_INFO, "forked child %d (active=%d)", pid, g_nchildren);
}

/* stop accepting, ask workers to leave, then SIGKILL and reap the stubborn. */
static void shutdown_children (void)
{
	if (g_nchildren == 0)
		return;
	srv_log (LOG_NOTICE, "shutting down: %d worker(s)", g_nchildren);

	for (int i = 0; i < g_nchildren; i++) {
		child_t *c = &g_children[i];
		if (c->state == CH_RUNNING) {
			c->state = CH_TERM_SENT;
			c->term_sent_at = now_ms ();
			c->reason = "shutdown";
			kill (c->pid, SIGTERM);
		}
	}

	long deadline = now_ms () + g_cfg.kill_grace_ms;
	while (g_nchildren > 0 && now_ms () < deadline) {
		poll (NULL, 0, 10); /* 10 ms nap, then reap */
		reap_children ();
	}

	for (int i = 0; i < g_nchildren; i++) {
		srv_log (LOG_WARNING, "child %d: shutdown -> SIGKILL",
			 g_children[i].pid);
		kill (g_children[i].pid, SIGKILL);
	}
	for (int i = 0; i < g_nchildren; i++) {
		int status;
		while (waitpid (g_children[i].pid, &status, 0) < 0 &&
		       errno == EINTR) {
		}
	}
	g_nchildren = 0;
}

static int run_server (void)
{
	g_children = calloc ((size_t)g_cfg.max_children, sizeof *g_children);
	if (!g_children) {
		srv_log (LOG_ERR, "out of memory for child table");
		return 1;
	}

	g_listen_fd = socket (AF_INET, SOCK_STREAM, 0);
	if (g_listen_fd < 0) {
		srv_log (LOG_ERR, "socket: %s", strerror (errno));
		return 1;
	}
	int one = 1;
	setsockopt (g_listen_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);

	struct sockaddr_in addr;
	memset (&addr, 0, sizeof addr);
	addr.sin_family = AF_INET;
	addr.sin_port = htons ((uint16_t)g_cfg.port);
	if (inet_pton (AF_INET, g_cfg.host, &addr.sin_addr) != 1) {
		srv_log (LOG_ERR, "bad bind address %s", g_cfg.host);
		close (g_listen_fd);
		return 1;
	}
	if (bind (g_listen_fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
		srv_log (LOG_ERR, "bind: %s", strerror (errno));
		close (g_listen_fd);
		return 1;
	}
	if (listen (g_listen_fd, g_cfg.backlog) < 0) {
		srv_log (LOG_ERR, "listen: %s", strerror (errno));
		close (g_listen_fd);
		return 1;
	}

	socklen_t alen = sizeof addr;
	if (getsockname (g_listen_fd, (struct sockaddr *)&addr, &alen) == 0)
		g_cfg.port = ntohs (addr.sin_port);
	if (g_notify_fd >=
	    0) { /* selftest: hand the bound port to the driver */
		int p = g_cfg.port;
		ssize_t w = write (g_notify_fd, &p, sizeof p);
		(void)w;
		close (g_notify_fd);
		g_notify_fd = -1;
	}
	srv_log (LOG_NOTICE, "listening on %s:%d (max_children=%d)", g_cfg.host,
		 g_cfg.port, g_cfg.max_children);
	printf ("listening on %s:%d\n", g_cfg.host, g_cfg.port);
	fflush (stdout);

	while (!g_stop) {
		long now = now_ms ();
		reap_children ();
		check_deadlines (now);
		check_memory (now);
		struct pollfd pfd = {.fd = g_listen_fd, .events = POLLIN};
		int pr = poll (&pfd, 1, next_poll_ms (now_ms ()));
		if (pr < 0) {
			if (errno != EINTR)
				srv_log (LOG_WARNING, "poll: %s",
					 strerror (errno));
			continue; /* EINTR: loop re-checks g_stop */
		}
		if (pr == 0 || !(pfd.revents & POLLIN))
			continue;
		int cfd = accept (g_listen_fd, NULL, NULL);
		if (cfd < 0) {
			if (errno != EINTR)
				srv_log (LOG_WARNING, "accept: %s",
					 strerror (errno));
			continue;
		}
		if (too_many_children ()) {
			srv_log (LOG_WARNING,
				 "max_children (%d) reached, rejecting",
				 g_cfg.max_children);
			send_503 (cfd);
			continue;
		}
		spawn_child (cfd);
	}

	close (g_listen_fd);
	shutdown_children ();
	srv_log (LOG_NOTICE, "stopped");
	printf ("stopped\n");
	return 0;
}

/* ------------------------------------------------------------------ */
/* selftest: drives the real supervisor over a loopback socket.        */
/* Needs no database (uses ?op=ping/slow/memhog).                      */
/* ------------------------------------------------------------------ */

#define CHECK(cond)                                                            \
	do {                                                                   \
		if (!(cond)) {                                                 \
			fprintf (stderr, "selftest FAILED: %s:%d: %s\n",       \
				 __FILE__, __LINE__, #cond);                   \
			return 1;                                              \
		}                                                              \
	} while (0)

static void valid_test_config (server_config_t *c)
{
	memset (c, 0, sizeof *c);
	c->port = 8090;
	copy_str (c->host, sizeof c->host, "127.0.0.1");
	c->backlog = 1024;
	c->timeout_ms = 5000;
	c->mem_limit_kb = 65536;
	c->max_children = 8;
	c->kill_grace_ms = 1000;
	copy_str (c->db_host, sizeof c->db_host, "127.0.0.1");
	c->db_port = 3306;
	copy_str (c->db_user, sizeof c->db_user, "u");
	copy_str (c->db_name, sizeof c->db_name, "d");
}

/* --- tiny single-shot HTTP client --- */

static int connect_port (int port)
{
	int fd = socket (AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		return -1;
	struct sockaddr_in a;
	memset (&a, 0, sizeof a);
	a.sin_family = AF_INET;
	a.sin_port = htons ((uint16_t)port);
	if (inet_pton (AF_INET, "127.0.0.1", &a.sin_addr) != 1 ||
	    connect (fd, (struct sockaddr *)&a, sizeof a) != 0) {
		close (fd);
		return -1;
	}
	return fd;
}

static void send_get (int fd, const char *path)
{
	char req[256];
	int n = snprintf (
		req, sizeof req,
		"GET %s HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n",
		path);
	if (n > 0)
		write_all (fd, req, (size_t)n);
}

static int read_until_eof (int fd, char *buf, size_t cap, int ms, long *elapsed)
{
	long t0 = now_ms ();
	size_t total = 0;
	for (;;) {
		long rem = ms - (now_ms () - t0);
		if (rem < 0)
			rem = 0;
		struct pollfd p = {.fd = fd, .events = POLLIN};
		int pr = poll (&p, 1, (int)rem);
		if (pr == 0)
			break; /* read timeout */
		if (pr < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		ssize_t r = read (fd, buf + total, cap - 1 - total);
		if (r <= 0)
			break;
		total += (size_t)r;
		if (total >= cap - 1)
			break;
	}
	buf[total] = 0;
	if (elapsed)
		*elapsed = now_ms () - t0;
	return (int)total;
}

static int http_get (int port, const char *path, char *buf, size_t cap, int ms,
		     long *elapsed)
{
	int fd = connect_port (port);
	if (fd < 0)
		return -1;
	send_get (fd, path);
	int n = read_until_eof (fd, buf, cap, ms, elapsed);
	close (fd);
	return n;
}

static int extract_worker (const char *buf)
{
	const char *p = strstr (buf, "\"worker\":");
	return p ? atoi (p + 9) : -1;
}

/* --- server under test --- */

typedef struct {
	long served;
	long timeout_kills;
	long mem_kills;
} test_counters_t;

static test_counters_t *g_test_counters;

static pid_t start_test_server (int *out_port)
{
	int fds[2];
	if (pipe (fds) != 0)
		return -1;
	pid_t pid = fork ();
	if (pid < 0) {
		close (fds[0]);
		close (fds[1]);
		return -1;
	}
	if (pid == 0) {
		prctl (PR_SET_PDEATHSIG, SIGTERM); /* die with the driver */
		signal (SIGPIPE, SIG_IGN);
		signal (SIGINT, on_stop);
		signal (SIGTERM, on_stop);
		int devnull = open ("/dev/null", O_WRONLY);
		if (devnull >= 0) {
			dup2 (devnull, STDOUT_FILENO);
			close (devnull);
		}
		close (fds[0]);
		g_notify_fd = fds[1];
		run_server ();
		if (g_test_counters) {
			g_test_counters->served = g_served;
			g_test_counters->timeout_kills = g_timeout_kills;
			g_test_counters->mem_kills = g_mem_kills;
		}
		_exit (0);
	}
	close (fds[1]);
	int port = 0;
	if (read (fds[0], &port, sizeof port) != (ssize_t)sizeof port)
		port = 0;
	close (fds[0]);
	*out_port = port;
	return pid;
}

static int selftest (const char *hdf)
{
	server_config_t c;
	char err[160];

	valid_test_config (&c);
	CHECK (validate_config (&c, err, sizeof err) == 0);
	c.timeout_ms = 0;
	CHECK (validate_config (&c, err, sizeof err) != 0);
	c.timeout_ms = 5000;
	c.mem_limit_kb = 0;
	CHECK (validate_config (&c, err, sizeof err) != 0);
	c.mem_limit_kb = 65536;
	c.max_children = 0;
	CHECK (validate_config (&c, err, sizeof err) != 0);
	c.max_children = 8;
	copy_str (c.host, sizeof c.host, "not-an-ip");
	CHECK (validate_config (&c, err, sizeof err) != 0);
	copy_str (c.host, sizeof c.host, "0.0.0.0");
	CHECK (validate_config (&c, err, sizeof err) == 0);
	copy_str (c.db_user, sizeof c.db_user, "");
	CHECK (validate_config (&c, err, sizeof err) != 0);

	CHECK (prepare (hdf)); /* registers the /demo route */

	/* small, fast supervisor config on an ephemeral port */
	g_cfg.port = 0;
	g_cfg.timeout_ms = 700;
	g_cfg.mem_limit_kb = 8192;
	g_cfg.max_children = 3;
	g_cfg.kill_grace_ms = 150;

	g_test_counters =
		mmap (NULL, sizeof *g_test_counters, PROT_READ | PROT_WRITE,
		      MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	CHECK (g_test_counters != MAP_FAILED);
	memset (g_test_counters, 0, sizeof *g_test_counters);

	signal (SIGPIPE, SIG_IGN);
	alarm (30); /* hard cap: a regression fails instead of hanging */

	int port = 0;
	pid_t srv = start_test_server (&port);
	CHECK (srv > 0);
	CHECK (port > 0);

	char buf[4096];
	long el;

	/* (a) each request runs in its own forked worker */
	int seen[8], nseen = 0;
	for (int i = 0; i < 3; i++) {
		int n = http_get (port, "/demo?op=ping", buf, sizeof buf, 3000,
				  NULL);
		CHECK (n > 0);
		int w = extract_worker (buf);
		CHECK (w > 0);
		for (int j = 0; j < nseen; j++)
			CHECK (seen[j] != w);
		seen[nseen++] = w;
	}

	/* (b) a hung worker is killed at the timeout */
	int n = http_get (port, "/demo?op=slow", buf, sizeof buf, 5000, &el);
	CHECK (n == 0); /* no response: connection died with the worker */
	CHECK (el >= g_cfg.timeout_ms - 150);
	CHECK (el < g_cfg.timeout_ms + 2000);

	/* (c) a memory hog is killed by the memory scan */
	n = http_get (port, "/demo?op=memhog", buf, sizeof buf, 8000, &el);
	CHECK (n == 0);

	/* (d) while max_children is busy, the next request is rejected */
	int occ[8], nocc = 0;
	for (int i = 0; i < g_cfg.max_children; i++) {
		int fd = connect_port (port);
		CHECK (fd >= 0);
		send_get (fd, "/demo?op=slow");
		occ[nocc++] = fd;
	}
	poll (NULL, 0, 200); /* let the master accept and fork them */
	n = http_get (port, "/demo?op=ping", buf, sizeof buf, 3000, NULL);
	CHECK (n > 0);
	CHECK (strstr (buf, "503") != NULL);
	for (int i = 0; i < nocc; i++)
		close (occ[i]);

	/* stop, then verify the supervisor counters */
	kill (srv, SIGTERM);
	int status;
	while (waitpid (srv, &status, 0) < 0 && errno == EINTR) {
	}
	CHECK (WIFEXITED (status) && WEXITSTATUS (status) == 0);
	CHECK (g_test_counters->served >= 3);
	CHECK (g_test_counters->timeout_kills >= 1);
	CHECK (g_test_counters->mem_kills >= 1);

	printf ("selftest: OK\n");
	return 0;
}

/* ------------------------------------------------------------------ */

int main (int argc, char **argv)
{
	const char *mode = argc > 1 ? argv[1] : "run";
	const char *hdf = argc > 2 ? argv[2] : "mdb_srv.hdf";

	m_init ();
	openlog ("mdb_srv", LOG_PID | LOG_NDELAY, LOG_LOCAL0);
	srv_log (LOG_NOTICE, "starting (mode=%s config=%s)", mode, hdf);

	if (strcmp (mode, "selftest") == 0) {
		int rc = selftest (hdf);
		m_destruct ();
		return rc;
	}

	if (!prepare (hdf)) {
		m_destruct ();
		return 1;
	}

	if (strcmp (mode, "dump-config") == 0 ||
	    strcmp (mode, "--dump-config") == 0) {
		dump_config (&g_cfg);
		m_destruct ();
		return 0;
	}

	/* a client that disconnects mid-response must not kill us with SIGPIPE
	 */
	signal (SIGPIPE, SIG_IGN);
	signal (SIGINT, on_stop);  /* Ctrl-C */
	signal (SIGTERM, on_stop); /* kill / systemctl stop */

	int rc = run_server ();
	m_destruct ();
	return rc;
}
