/*
 * procwatch — MLS showcase: a learned process allowlist.
 *
 * Modes:
 *   learn (default) — every second scan /proc for running process names, keep
 *                     them in a string-set, and save the sorted set every
 *                     minute (one name per line).
 *   exec            — load the allowlist, then scan /proc every second and
 *                     report each process that is NOT in the allowlist to
 *                     stdout (one line each, pipeline-friendly), with its
 *                     identity: pid/uid/gid/ppid/comm. Each unknown process is
 *                     reported once; a hidden name (empty comm) as comm=-.
 *   selftest        — built-in checks (run via `make check`).
 *
 * Quiet by default (Unix filter): only data on stdout, errors on stderr.
 * `-v` adds progress/status on stderr.
 *
 * SIGINT (Ctrl-C), SIGTERM (kill / systemctl stop) and SIGUSR1 stop the scan
 * loop cleanly: learn writes a final checkpoint, then m_destruct() runs.
 * SIGHUP reloads the allowlist in exec mode (daemon convention).
 *
 * Build (single-header amalgam; no per-module .c files):
 *   make          # gcc -I../.. procwatch.c -o procwatch -lpthread -lm -ldl
 *   make check
 *
 * The point: this stays close to the Python original because the library
 * supplies the same comforts as small primitives —
 *   os.listdir()          -> m_dir_list()
 *   open(f).read().strip()-> m_str_from_file() + s_trim()
 *   set()                 -> m_lookup_mstr() (dedup + membership)
 *   open(p).read().split()-> m_strlist_load()
 *   "\n".join(sorted(s))  -> m_qsort(cmp_mstr) + m_strlist_save()
 * — while keeping C's direct /proc reads (no fork/exec of `ps`).
 */

#define MLS_CORE_IMPLEMENTATION
#include "mls_core.h" /* must come first: the header sets _GNU_SOURCE */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SCAN_INTERVAL 1 /* seconds between /proc scans */
#define WRITE_EVERY 60	/* scans between checkpoints (≈1 minute) */
#define DEFAULT_FILE "procs.allow"

/* -v/--verbose: progress/status on stderr. Off by default (Unix filter should
 * be quiet; errors go to stderr regardless). */
static int verbose = 0;

/* SIGINT (Ctrl-C) / SIGTERM / SIGUSR1 request a clean stop: the scan loops
 * notice the flag, learn writes a final checkpoint, then main tears the library
 * down. SIGHUP (exec only) asks to reload the allowlist. */
static volatile sig_atomic_t stop = 0;
static volatile sig_atomic_t reload = 0;

static void on_stop (int sig)
{
	(void)sig;
	stop = 1; /* async-signal-safe: only set the flag */
}

static void on_reload (int sig)
{
	(void)sig;
	reload = 1;
}

static void install_stop_handlers (void)
{
	signal (SIGINT, on_stop);  /* Ctrl-C */
	signal (SIGTERM, on_stop); /* kill / systemctl stop / docker stop */
	signal (SIGUSR1, on_stop); /* explicit checkpoint-and-stop */
}

/* selftest helper: explicit check that stays active without NDEBUG. */
#define CHECK(cond)                                                            \
	do {                                                                   \
		if (!(cond)) {                                                 \
			fprintf (stderr, "selftest FAILED: %s (%s:%d)\n",      \
				 #cond, __FILE__, __LINE__);                   \
			return 1;                                              \
		}                                                              \
	} while (0)

/* First number on the "<key>:" line of a /proc status buffer, or 0. */
static unsigned status_field (const char *buf, const char *key)
{
	size_t klen = strlen (key);
	const char *p = buf;
	while (p && *p) {
		if (strncmp (p, key, klen) == 0 && p[klen] == ':')
			return (unsigned)strtoul (p + klen + 1, NULL, 10);
		p = strchr (p, '\n');
		if (p)
			p++;
	}
	return 0;
}

/* Report one process identity on stdout: pid, uid, gid, ppid, comm. `comm` may
 * be NULL for a process that hid its name (reported as comm=-). */
static void report_proc (const char *pid, const char *comm)
{
	char path[64];
	unsigned uid = 0, gid = 0, ppid = 0;
	tmls status;

	snprintf (path, sizeof path, "/proc/%s/status", pid);
	status = m_str_from_file (path);
	if (status >= 0) {
		uid = status_field (m_str (status), "Uid");
		gid = status_field (m_str (status), "Gid");
		ppid = status_field (m_str (status), "PPid");
	}
	printf ("pid=%s uid=%u gid=%u ppid=%u comm=%s\n", pid, uid, gid, ppid,
		comm && *comm ? comm : "-");
	fflush (stdout);
}

/* Scan /proc and fold every running process name into `list` (a sorted
 * string-set). With `report` set (exec mode), a name not yet in `list` is
 * reported once as "pid=<pid> uid=<uid> gid=<gid> ppid=<ppid> comm=<name>".
 * A process that cleared its comm (prctl(PR_SET_NAME, "")) has no name to
 * allowlist: in exec mode it is reported the same way with comm=-, once,
 * deduplicated by PID in `seen` (a sorted int list). */
static void scan_procs (int list, int report, int seen)
{
	int i;
	char **pp;
	tmls dir = m_dir_list ("/proc");
	m_foreach (dir, i, pp)
	{
		char *pid = *pp;
		if (!pid[0] || strspn (pid, "0123456789") != strlen (pid))
			continue;

		tmls path = s_printf (0, 0, "/proc/%s/comm", pid);
		tmls comm = m_str_from_file (m_str (path));
		if (comm < 0)
			continue;
		s_trim (comm);

		if (s_isempty (comm)) { /* hidden name: anomaly, not
					   allowlistable */
			int pnum = atoi (pid);
			if (!report || m_bsearch_int (seen, pnum) >= 0)
				continue;
			report_proc (pid, NULL); /* comm=- */
			/* ponytail: dedup per PID; a recycled PID could hide a
			 * later hidden process. Upgrade: key on pid+starttime
			 * (/proc/<pid>/stat field 22). */
			m_binsert_int (seen, pnum);
			continue;
		}

		if (m_lookup_mstr (list, comm, 1) < 0) { /* unknown */
			if (report)
				report_proc (pid, m_str (comm));
			m_lookup_mstr (list, comm, 0); /* remember */
		}
	}
}

/* learn: build a cumulative allowlist, checkpointing every minute, and once
 * more on SIGINT/SIGTERM/SIGUSR1 for a clean exit. */
static int learn (const char *path)
{
	int list = m_alloc (0, sizeof (char *), MFREE_STR);

	int sec = 0;
	for (;;) {
		scan_procs (list, 0, 0);
		if (stop || sec++ % WRITE_EVERY == 0) {
			/* first scan, every minute, and on stop; the list stays
			 * sorted via m_lookup_mstr, so the file is sorted too
			 */
			if (m_strlist_save (list, path) != 0)
				fprintf (stderr, "learn: cannot write %s\n",
					 path);
			else if (verbose)
				fprintf (stderr,
					 "learn: %d known processes -> %s\n",
					 (int)m_len (list), path);
			if (stop)
				break; /* final checkpoint is written */
		}
		sleep (SCAN_INTERVAL);
	}
	return 0;
}

/* exec: report each unknown process name once, one per line on stdout.
 * SIGINT/SIGTERM/SIGUSR1 stop cleanly; SIGHUP reloads the allowlist. */
static int exec_mode (const char *path)
{
	int list = m_strlist_load (path);
	if (list < 0) {
		fprintf (stderr, "exec: cannot read %s (run learn first)\n",
			 path);
		return 1;
	}
	m_qsort (list, cmp_mstr); /* normalize a possibly unsorted file */
	if (verbose)
		fprintf (stderr, "exec: %d allowed processes from %s\n",
			 (int)m_len (list), path);
	signal (SIGHUP, on_reload); /* reload the allowlist */

	int seen = m_alloc (0, sizeof (int), MFREE); /* PIDs reported no-name */
	for (;;) {
		scan_procs (list, 1, seen);
		if (stop)
			break;
		if (reload) { /* SIGHUP: re-read the allowlist */
			reload = 0;
			int fresh = m_strlist_load (path);
			if (fresh > 0) {
				m_qsort (fresh, cmp_mstr);
				m_free (list);
				list = fresh;
				if (verbose)
					fprintf (stderr,
						 "exec: reloaded %d processes "
						 "from %s\n",
						 (int)m_len (list), path);
			} else {
				fprintf (stderr,
					 "exec: reload failed, keeping %d "
					 "processes\n",
					 (int)m_len (list));
			}
		}
		sleep (SCAN_INTERVAL);
	}
	return 0;
}

/* selftest: exercise the string-set, the file round-trip and the scanner. */
static int selftest (void)
{
	const char *tmp = "/tmp/mls_procwatch_selftest.txt";

	/* 1) set + save/load round-trip. */
	int a = m_alloc (0, sizeof (char *), MFREE_STR);
	m_lookup_str (a, "zeta", 0); /* inserted out of order on purpose */
	m_lookup_str (a, "bash", 0);
	m_lookup_str (a, "cron", 0);
	m_lookup_str (a, "bash", 0); /* duplicate must not grow the set */
	CHECK (m_len (a) == 3);
	/* m_lookup_str keeps the set sorted -> no m_qsort needed here */
	CHECK (strcmp (STR (a, 0), "bash") == 0);
	CHECK (strcmp (STR (a, 1), "cron") == 0);
	CHECK (strcmp (STR (a, 2), "zeta") == 0);

	CHECK (m_strlist_save (a, tmp) == 0);
	int b = m_strlist_load (tmp);
	CHECK (b > 0);
	CHECK (m_len (b) == 3);
	CHECK (m_lookup_str (b, "bash", 1) >= 0);
	CHECK (m_lookup_str (b, "cron", 1) >= 0);
	CHECK (m_lookup_str (b, "zeta", 1) >= 0);
	CHECK (m_lookup_str (b, "definitely-not-a-process", 1) < 0);
	CHECK (m_lookup_str (b, "", 1) < 0); /* leerer Key: kein Match ... */
	CHECK (m_lookup_str (b, "", 0) <
	       0); /* ... und kein Insert (kein Abbruch) */
	CHECK (m_len (b) == 3);

	/* 1a) m_lookup_mstr: handle key; a duplicate must not grow the set. */
	tmls probe = s_dup ("cron");
	CHECK (m_lookup_mstr (b, probe, 1) >= 0); /* found */
	CHECK (m_lookup_mstr (b, probe, 0) >= 0);
	CHECK (m_len (b) == 3); /* no duplicate appended */

	/* 1a2) leerer Key: kein Match, wird nie eingefügt (kein Abbruch). */
	tmls empty = s_new ();
	CHECK (m_lookup_mstr (b, empty, 1) < 0);
	CHECK (m_lookup_mstr (b, empty, 0) < 0);
	CHECK (m_len (b) == 3);
	remove (tmp);

	/* 1b) a missing file is reported, not silently treated as empty. */
	CHECK (m_strlist_load ("/tmp/mls_procwatch_no_such_file") < 0);

	/* 1c) /proc status field extraction (used by report_proc). */
	const char *pstatus = "Name:\tbash\nUid:\t1000\t1000\t1000\t1000\nGid:"
			      "\t100\t100\t100\t100\nPPid:\t1\n";
	CHECK (status_field (pstatus, "Uid") == 1000);
	CHECK (status_field (pstatus, "Gid") == 100);
	CHECK (status_field (pstatus, "PPid") == 1);
	CHECK (status_field (pstatus, "Nope") == 0);

	/* 2) the scanner must find this very process via /proc. */
	tmls self = m_str_from_file ("/proc/self/comm");
	CHECK (self > 0);
	s_trim (self);
	int list = m_alloc (0, sizeof (char *), MFREE_STR);
	scan_procs (list, 0, 0);
	CHECK (m_lookup_mstr (list, self, 1) >= 0);

	/* 3) stop signals set the flag (handlers installed in main). */
	stop = 0;
	CHECK (raise (SIGINT) == 0 && stop == 1);
	stop = 0;
	CHECK (raise (SIGTERM) == 0 && stop == 1);
	stop = 0;
	CHECK (raise (SIGUSR1) == 0 && stop == 1);
	/* 3b) SIGHUP handler only sets the reload flag (installed in exec). */
	reload = 0;
	on_reload (0);
	CHECK (reload == 1);

	m_free (list);
	m_free (b);
	m_free (a);
	printf ("selftest OK\n");
	return 0;
}

int main (int argc, char **argv)
{
	const char *mode = "learn";
	const char *path = DEFAULT_FILE;
	int i, mode_set = 0;

	for (i = 1; i < argc; i++) {
		const char *a = argv[i];
		if (strcmp (a, "-v") == 0 || strcmp (a, "--verbose") == 0)
			verbose = 1;
		else if (!mode_set) {
			mode = a;
			mode_set = 1;
		} else
			path = a;
	}

	m_init ();
	install_stop_handlers (); /* SIGINT / SIGTERM / SIGUSR1 -> clean stop */

	int rc;
	if (strcmp (mode, "learn") == 0)
		rc = learn (path);
	else if (strcmp (mode, "exec") == 0)
		rc = exec_mode (path);
	else if (strcmp (mode, "selftest") == 0)
		rc = selftest ();
	else {
		fprintf (stderr,
			 "usage: %s [-v] [learn|exec|selftest] [FILE]\n",
			 argv[0]);
		rc = 2;
	}

	m_destruct ();
	return rc;
}
