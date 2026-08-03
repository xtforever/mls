#include "mls.h"
#include "m_tool.h"

#include <ctype.h>
#include <dirent.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <unistd.h>

static int out = 0;

static void section (const char *title)
{
	s_printf (out, -1, "\n  %s\n  %s\n", title,
		  "------------------------------------------");
}

static void kv (const char *key, const char *val)
{
	s_printf (out, -1, "  %-18s %s\n", key, val);
}

/* Read a whole file into a fresh MLS string handle; 0 on error. */
static int read_file (const char *path)
{
	FILE *fp = fopen (path, "r");
	if (!fp)
		return 0;
	char buf[4096];
	int h = s_new ();
	while (fgets (buf, sizeof (buf), fp))
		s_printf (h, -1, "%s", buf);
	fclose (fp);
	return h;
}

/* Value after "key:" in a /proc-style block, or "n/a". */
static char *kv_value (const char *src, const char *key)
{
	const char *hit = strstr (src, key);
	if (!hit)
		return strdup ("n/a");
	hit = strchr (hit, ':');
	if (!hit)
		return strdup ("n/a");
	hit++;
	while (*hit == ' ' || *hit == '\t')
		hit++;
	const char *nl = strchr (hit, '\n');
	if (!nl)
		nl = hit + strlen (hit);
	return strndup (hit, (size_t)(nl - hit));
}

static char *fmt_gb (double bytes)
{
	char *r = malloc (64);
	snprintf (r, 64, "%.1f GB", bytes / (1024.0 * 1024.0 * 1024.0));
	return r;
}

static void system_section (void)
{
	struct utsname u;
	uname (&u);
	char host[256] = "n/a";
	gethostname (host, sizeof (host));
	section ("SYSTEM");
	kv ("Hostname", host);
	kv ("OS", u.sysname);
	kv ("Kernel", u.release);
	kv ("Arch", u.machine);
}

static void cpu_section (int cpuinfo)
{
	section ("CPU");
	const char *src = m_str (cpuinfo);
	int cores = 0;
	for (const char *p = src; (p = strstr (p, "model name")); p++)
		cores++;
	char buf[64];
	snprintf (buf, sizeof (buf), "%d (sysconf %ld online)", cores,
		  sysconf (_SC_NPROCESSORS_ONLN));
	kv ("Processors", buf);
	char *model = kv_value (src, "model name");
	kv ("Model", model);
	free (model);
	double av[3] = {0};
	if (getloadavg (av, 3) == 3) {
		snprintf (buf, sizeof (buf), "%.2f / %.2f / %.2f (1/5/15 min)",
			  av[0], av[1], av[2]);
		kv ("Load", buf);
	}
}

static void mem_section (int meminfo)
{
	section ("MEMORY");
	const char *src = m_str (meminfo);
	char *t = kv_value (src, "MemTotal");
	char *a = kv_value (src, "MemAvailable");
	kv ("Total", t);
	kv ("Available", a);
	free (t);
	free (a);
	char *st = kv_value (src, "SwapTotal");
	char *sf = kv_value (src, "SwapFree");
	kv ("Swap", st);
	kv ("Swap free", sf);
	free (st);
	free (sf);
}

static void disk_section (void)
{
	section ("DISK");
	struct statvfs v;
	if (statvfs ("/", &v) != 0)
		return;
	double total = (double)v.f_blocks * v.f_frsize;
	double avail = (double)v.f_bavail * v.f_frsize;
	char *t = fmt_gb (total);
	char *u = fmt_gb (total - avail);
	char *f = fmt_gb (avail);
	kv ("Root total", t);
	kv ("Root used", u);
	kv ("Root free", f);
	free (t);
	free (u);
	free (f);
}

static void proc_section (void)
{
	section ("PROCESSES");
	DIR *d = opendir ("/proc");
	if (!d)
		return;
	int n = 0;
	struct dirent *e;
	while ((e = readdir (d))) {
		if (!isdigit ((unsigned char)e->d_name[0]))
			continue;
		n++;
	}
	closedir (d);
	char buf[32];
	snprintf (buf, sizeof (buf), "%d running", n);
	kv ("Processes", buf);
}

static void uptime_section (int ut)
{
	section ("UPTIME");
	double up = 0;
	sscanf (m_str (ut), "%lf", &up);
	int h = (int)up / 3600;
	int m = ((int)up % 3600) / 60;
	char buf[64];
	snprintf (buf, sizeof (buf), "%d h %d min", h, m);
	kv ("Up", buf);
}

static void user_section (void)
{
	section ("USER");
	struct passwd *pw = getpwuid (getuid ());
	if (!pw)
		return;
	kv ("Name", pw->pw_name);
	kv ("Home", pw->pw_dir);
}

int main (void)
{
	m_init ();
	out = s_new ();

	system_section ();
	int cpuinfo = read_file ("/proc/cpuinfo");
	if (cpuinfo)
		cpu_section (cpuinfo);
	int meminfo = read_file ("/proc/meminfo");
	if (meminfo)
		mem_section (meminfo);
	disk_section ();
	proc_section ();
	int uptime = read_file ("/proc/uptime");
	if (uptime)
		uptime_section (uptime);
	user_section ();

	puts (m_str (out));

	if (cpuinfo)
		m_free (cpuinfo);
	if (meminfo)
		m_free (meminfo);
	if (uptime)
		m_free (uptime);
	m_free (out);
	m_destruct ();
	return 0;
}
