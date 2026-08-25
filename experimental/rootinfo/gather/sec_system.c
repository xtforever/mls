#include "gather.h"
#include "m_subproc.h"
#include "m_tool.h"
#include "m_types.h"
#include <ctype.h>
#include <dirent.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <unistd.h>

static int kv_pair (const char *key, const char *val)
{
	int h = m_create (2, sizeof (field_t));
	FIELD_ADD (h, key);
	FIELD_ADD (h, val);
	return h;
}

static void gather_osinfo (int rows, const char *host, const char *sysname,
			   const char *release, const char *machine)
{
	int r;
	r = kv_pair ("Hostname", host);
	m_put (rows, &r);
	r = kv_pair ("OS", sysname);
	m_put (rows, &r);
	r = kv_pair ("Kernel", release);
	m_put (rows, &r);
	r = kv_pair ("Arch", machine);
	m_put (rows, &r);
}

static void gather_cpuinfo (int rows)
{
	int cpuinfo = m_str_from_file ("/proc/cpuinfo");
	if (cpuinfo < 0)
		return;

	int cores = 0;
	int pat = s_cstr ("model name");
	int off = 0;
	while ((off = s_strstr (cpuinfo, off, pat)) >= 0) {
		cores++;
		off++;
	}

	int ph = s_printf (0, 0, "%d (sysconf %ld online)", cores,
			   sysconf (_SC_NPROCESSORS_ONLN));

	int r = kv_pair ("Processors", m_str (ph));
	m_put (rows, &r);
	m_free (ph);

	const char *model = "n/a";
	int mh = 0;
	int mi = s_find (cpuinfo, "model name");
	if (mi >= 0) {
		int c = s_chr (cpuinfo, ':', mi);
		if (c >= 0) {
			int start = c + 1;
			while (CHAR (cpuinfo, start) == ' ' ||
			       CHAR (cpuinfo, start) == '\t')
				start++;
			int nl = s_chr (cpuinfo, '\n', start);
			int end = nl < 0 ? s_strlen (cpuinfo) - 1 : nl - 1;
			mh = s_slice (0, 0, cpuinfo, start, end);
			model = m_str (mh);
		}
	}
	r = kv_pair ("Model", model);
	m_put (rows, &r);
	if (mh)
		m_free (mh);

	double av[3] = {0};
	if (getloadavg (av, 3) == 3) {
		int lh = s_printf (0, 0, "%.2f / %.2f / %.2f (1/5/15 min)",
				   av[0], av[1], av[2]);
		r = kv_pair ("Load", m_str (lh));
		m_free (lh);
		m_put (rows, &r);
	}

	m_free (cpuinfo);
}

static void gather_meminfo (int rows)
{
	int meminfo = m_str_from_file ("/proc/meminfo");
	if (meminfo < 0)
		return;

	const char *keys[] = {"MemTotal", "MemAvailable", "SwapTotal",
			      "SwapFree"};
	for (int i = 0; i < 4; i++) {
		int vstr = 0;
		int hit = s_find (meminfo, keys[i]);
		if (hit >= 0) {
			int c = s_chr (meminfo, ':', hit);
			int start = c >= 0 ? c + 1 : hit;
			while (CHAR (meminfo, start) == ' ' ||
			       CHAR (meminfo, start) == '\t')
				start++;
			int nl = s_chr (meminfo, '\n', start);
			int end = nl < 0 ? s_strlen (meminfo) - 1 : nl - 1;
			int vh = s_slice (0, 0, meminfo, start, end);
			double gb = (double)s_to_long (vh) / (1024.0 * 1024.0);
			vstr = s_printf (0, 0, "%.1f GB", gb);
			m_free (vh);
		} else {
			vstr = s_printf (0, 0, "n/a");
		}
		int r = kv_pair (keys[i], m_str (vstr));
		m_free (vstr);
		m_put (rows, &r);
	}

	m_free (meminfo);
}

/* filesystem types df itself ignores plus the ones the old `df -x ...`
   call filtered out */
static int skip_fs_type (const char *type)
{
	static const char *skip[] = {
		"autofs", "binfmt_misc", "bpf", "cgroup", "cgroup2",
		"configfs", "debugfs", "devpts", "devtmpfs", "efivarfs",
		"fusectl", "hugetlbfs", "mqueue", "nsfs", "nfsd",
		"overlay", "proc", "pstore", "ramfs", "rpc_pipefs",
		"securityfs", "selinuxfs", "squashfs", "sysfs", "tmpfs",
		"tracefs",
	};
	for (size_t i = 0; i < sizeof (skip) / sizeof (skip[0]); i++)
		if (strcmp (type, skip[i]) == 0)
			return 1;
	return 0;
}

/* /proc/mounts escapes spaces and friends as \ooo */
static int mount_unescape (int h)
{
	int out = s_new ();
	for (int i = 0;; i++) {
		char c = CHAR (h, i);
		if (!c)
			break;
		if (c == '\\' && isdigit ((unsigned char)CHAR (h, i + 1))
		    && isdigit ((unsigned char)CHAR (h, i + 2))
		    && isdigit ((unsigned char)CHAR (h, i + 3))) {
			int v = (CHAR (h, i + 1) - '0') * 64 +
				(CHAR (h, i + 2) - '0') * 8 +
				(CHAR (h, i + 3) - '0');
			m_putc (out, (char)v);
			i += 3;
		} else {
			m_putc (out, c);
		}
	}
	m_putc (out, 0);
	return out;
}

static int shell_quote (int h)
{
	int q = s_new ();
	m_putc (q, '\'');
	for (int i = 0;; i++) {
		char c = CHAR (h, i);
		if (!c)
			break;
		if (c == '\'')
			s_cat (q, "'\\''");
		else
			m_putc (q, c);
	}
	m_putc (q, '\'');
	m_putc (q, 0);
	return q;
}

static void df_add_rows (int rows, int out_h)
{
	int lines = s_msplit (0, out_h, s_cstr ("\n"));
	int toks = m_alloc (16, sizeof (int), MFREE_EACH);
	int p, *d;
	m_foreach (lines, p, d)
	{
		if (s_has_prefix (*d, "Filesystem"))
			continue;
		s_msplit (toks, *d, s_cstr (" "));
		int row = m_create (6, sizeof (field_t));
		int n = 0, mnt = 0;
		for (int j = 0; j < (int)m_len (toks); j++) {
			int h = INT (toks, j);
			if (mstr_empty (h)) {
				m_free (h);
				INT (toks, j) = 0;
				continue;
			}
			if (n >= 1 && n <= 3) {
				int gb = s_printf (0, 0, "%.2fG",
						   (double)s_to_long (h) /
							   (1024.0 * 1024.0));
				m_free (h);
				h = gb;
			}
			if (n < 5) {
				field_t f_ = {.str_h = h,
					      .align = n ? ALIGN_RIGHT
							 : ALIGN_LEFT};
				m_put (row, &f_);
				INT (toks, j) = 0;
			} else {
				if (mnt)
					s_cat (mnt, " ");
				else
					mnt = s_new ();
				s_mcat (mnt, h);
				m_free (h);
				INT (toks, j) = 0;
			}
			n++;
		}
		if (n < 6) {
			for (int j = 0; j < (int)m_len (row); j++)
				m_free (((field_t *)m_buf (row) + j)->str_h);
			m_free (row);
			m_free (mnt);
			continue;
		}
		FIELD_ADD_H (row, mnt);
		m_put (rows, &row);
	}
	m_free (toks);
	m_free (lines);
}

static void gather_disk (int entries)
{
	struct statvfs vf;
	if (statvfs ("/", &vf) != 0)
		return;

	double total = (double)vf.f_blocks * vf.f_frsize;
	double avail = (double)vf.f_bavail * vf.f_frsize;
	double used = total - avail;
	double frac = total > 0 ? used / total : 0.0;

	add_entry (entries,
		   bar_new (s_printf (0, 0, "Disk: %.1fG/%.1fG used (%.0f%%)",
					      used / (1024.0 * 1024.0 * 1024.0),
					      total / (1024.0 * 1024.0 * 1024.0),
					      frac * 100.0),
			    frac));

	int mounts = m_str_from_file ("/proc/mounts");
	if (mounts < 0)
		return;

	int rows = m_create (16, sizeof (int));
	int lines = s_msplit (0, mounts, s_cstr ("\n"));
	m_free (mounts);
	int toks = m_alloc (8, sizeof (int), MFREE_EACH);
	int p, *d;
	m_foreach (lines, p, d)
	{
		s_msplit (toks, *d, s_cstr (" "));
		int skip = 1, out = 0;
		if (m_len (toks) >= 3
		    && !skip_fs_type (m_str (INT (toks, 2)))) {
			int path = mount_unescape (INT (toks, 1));
			int q = shell_quote (path);
			int cmd = s_printf (0, 0, "LC_ALL=C df -kP %s 2>/dev/null",
					    m_str (q));
			m_free (q);
			m_free (path);
			/* one df per mountpoint: an unreachable NFS server
			   costs only this row, not the whole section */
			skip = (subproc_run (m_str (cmd), &out, NULL, 3000) != 0);
			m_free (cmd);
		}
		for (int j = 0; j < (int)m_len (toks); j++) {
			m_free (INT (toks, j));
			INT (toks, j) = 0;
		}
		if (skip || !out || s_isempty (out)) {
			m_free (out);
			continue;
		}
		df_add_rows (rows, out);
		m_free (out);
	}
	m_free (toks);
	m_free (lines);

	if (m_len (rows) == 0) {
		m_free (rows);
		return;
	}

	int th = table_new (6, (const char *[]){ "Filesystem", "Size", "Used",
						 "Avail", "Use%", "Mounted on" });
	data_t *t = (data_t *)m_buf (th);
	m_free (t->rows);
	t->rows = rows;

	add_entry (entries, th);
}

static void gather_proc_uptime (int entries)
{
	DIR *d = opendir ("/proc");
	int nproc = 0;
	if (d) {
		struct dirent *de;
		while ((de = readdir (d))) {
			if (isdigit ((unsigned char)de->d_name[0]))
				nproc++;
		}
		closedir (d);
	}

	int ut = m_str_from_file ("/proc/uptime");
	int up_h = 0;
	if (ut > 0) {
		double up = 0;
		sscanf (m_str (ut), "%lf", &up);
		int hh = (int)up / 3600;
		int mm = ((int)up % 3600) / 60;
		int dd = hh / 24;
		hh %= 24;
		if (dd)
			up_h = s_printf (0, 0, "up %dd %dh %dm", dd, hh, mm);
		else
			up_h = s_printf (0, 0, "up %dh %dm", hh, mm);
		m_free (ut);
	}

	int h = data_new (DT_LIST);
	data_t *l = (data_t *)m_buf (h);
	l->items = m_create (1, sizeof (field_t));
	FIELD_ADD_H (l->items, s_printf (0, 0, "%d processes, %s", nproc,
					 up_h ? m_str (up_h) : ""));
	if (up_h)
		m_free (up_h);
	add_entry (entries, h);
}

static void gather_users (int entries)
{
	int who_lines = subproc_lines ("who 2>/dev/null");
	if (STRTAB_EMPTY (who_lines)) {
		m_free (who_lines);
		return;
	}
	int unames = m_alloc (8, sizeof (int), MFREE_EACH);
	int p, *d, buf = 0;
	m_foreach (who_lines, p, d)
	{
		buf = copy_word (buf, *d);
		if (m_binsert (unames, &buf, _cmp_mstr, 0) >= 0)
			buf = 0;
	}
	m_free (buf);
	m_free (who_lines);

	int ucount = (int)m_len (unames);
	if (!ucount) {
		m_free (unames);
		return;
	}

	int line = s_printf (0, 0, "Users (%d): ", ucount);
	for (int i = 0; i < ucount; i++) {
		if (i)
			s_cat (line, ", ");
		s_mcat (line, INT (unames, i));
	}

	add_entry (entries, text_new (line));
	m_free (unames);
}

static void gather_network (int entries)
{
	DIR *d = opendir ("/sys/class/net");
	if (!d)
		return;

	int line = s_new ();
	int off = 0;
	int sp = s_dup (" ");
	struct dirent *de;
	while ((de = readdir (d))) {
		if (de->d_name[0] == '.')
			continue;
		int devpath =
			s_printf (0, 0, "/sys/class/net/%s/device", de->d_name);
		struct stat st;
		if (stat (m_str (devpath), &st) != 0) {
			m_free (devpath);
			continue;
		}
		m_free (devpath);

		int cmd = s_printf (0, 0, "ip -br addr show %.200s 2>/dev/null",
				    de->d_name);
		int out_h = subproc_read (m_str (cmd));
		m_free (cmd);
		if (STRTAB_EMPTY (out_h)) {
			m_free (out_h);
			continue;
		}

		s_trim (out_h);
		if (s_isempty (out_h)) {
			m_free (out_h);
			continue;
		}

		int toks = m_alloc (4, sizeof (int), MFREE_EACH);
		s_msplit (toks, out_h, sp);
		int state_h = 0, ip4 = 0;
		int seen = 0, p, *d;
		m_foreach (toks, p, d)
		{
			if (mstr_empty (*d))
				continue;
			seen++;
			if (seen == 2)
				state_h = s_clone (*d);
			else if (seen == 3) {
				ip4 = s_clone (*d);
				break;
			}
		}
		m_free (toks);
		if (!state_h) {
			m_free (out_h);
			continue;
		}

		int slash = s_chr (ip4, '/', 0);
		if (slash >= 0) {
			int cut = s_left (ip4, slash);
			m_free (ip4);
			ip4 = cut;
		}

		if (off)
			s_cat (line, ", ");
		off = 1;
		s_printf (line, -1, "%s (%s", de->d_name, m_str (state_h));
		if (mstr_empty (ip4))
			s_cat (line, ")");
		else
			s_printf (line, -1, ", %s)", m_str (ip4));
		m_free (state_h);
		m_free (ip4);

		m_free (out_h);
	}
	closedir (d);
	m_free (sp);

	if (!off) {
		m_free (line);
		return;
	}

	int text_h = s_printf (0, 0, "Network: %s", m_str (line));
	m_free (line);
	add_entry (entries, text_new (text_h));

	/* default gateway + DNS */
	int gw = subproc_read ("ip route show default 2>/dev/null");
	if (gw > 0 && !s_isempty (gw)) {
		int via = s_find (gw, "via ");
		if (via >= 0) {
			int start = via + 4;
			int end = start;
			while (CHAR (gw, end) && CHAR (gw, end) != ' ')
				end++;
			int ip = s_slice (0, 0, gw, start, end - 1);
			add_entry (entries,
				   text_new (s_printf (0, 0, "Gateway: %s",
						       m_str (ip))));
			m_free (ip);
		}
	}
	m_free (gw);

	int dns = subproc_read (
		"sed -n 's/^nameserver //p' /etc/resolv.conf 2>/dev/null");
	if (dns > 0 && !s_isempty (dns)) {
		int dns_line = s_printf (0, 0, "DNS: %s", m_str (dns));
		s_trim (dns_line);
		add_entry (entries, text_new (dns_line));
	}
	m_free (dns);
}

int gather_system (cfg_t cfg)
{
	(void)cfg;
	struct utsname u;
	uname (&u);
	char host[256] = "n/a";
	gethostname (host, sizeof (host));

	int sec_h = section_new ("SYSTEM", 8);
	section_t *sec = (section_t *)m_buf (sec_h);

	int th = table_new (2, (const char *[]){"Key", "Value"});
	data_t *t = (data_t *)m_buf (th);
	gather_osinfo (t->rows, host, u.sysname, u.release, u.machine);
	gather_cpuinfo (t->rows);
	gather_meminfo (t->rows);
	add_entry (sec->entries, th);

	gather_disk (sec->entries);
	gather_proc_uptime (sec->entries);
	gather_users (sec->entries);
	gather_network (sec->entries);

	return sec_h;
}

int gather_all (cfg_t cfg)
{
	int sections = m_create (8, sizeof (int));
	int sh;

	if (cfg_bool (cfg, "section", "system", 1))
		if ((sh = gather_system (cfg)))
			m_put (sections, &sh);
	if (cfg_bool (cfg, "section", "ports", 1))
		if ((sh = gather_ports (cfg)))
			m_put (sections, &sh);
	if (cfg_bool (cfg, "section", "proc", 1))
		if ((sh = gather_proc (cfg)))
			m_put (sections, &sh);
	if (cfg_bool (cfg, "section", "cron", 1))
		if ((sh = gather_cron (cfg)))
			m_put (sections, &sh);
	if (cfg_bool (cfg, "section", "firewall", 1))
		if ((sh = gather_firewall (cfg)))
			m_put (sections, &sh);
	if (cfg_bool (cfg, "section", "stack", 1))
		if ((sh = gather_stack (cfg)))
			m_put (sections, &sh);
	if (cfg_bool (cfg, "section", "lvm", 1))
		if ((sh = gather_lvm (cfg)))
			m_put (sections, &sh);
	if (cfg_bool (cfg, "section", "zfs", 1))
		if ((sh = gather_zfs (cfg)))
			m_put (sections, &sh);
	if (cfg_bool (cfg, "section", "docker", 1))
		if ((sh = gather_docker (cfg)))
			m_put (sections, &sh);
	if (cfg_bool (cfg, "section", "health", 1))
		if ((sh = gather_health (cfg)))
			m_put (sections, &sh);

	return sections;
}
