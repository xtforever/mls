#include "gather.h"
#include "m_types.h"
#include "m_subproc.h"
#include "m_tool.h"
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

static int read_file(const char *path)
{
	FILE *fp = fopen(path, "r");
	if (!fp) return 0;
	char buf[4096];
	int h = s_new();
	while (fgets(buf, sizeof(buf), fp))
		s_printf(h, -1, "%s", buf);
	fclose(fp);
	return h;
}

static int kv_pair(const char *key, const char *val)
{
	int h = m_create(2, sizeof(field_t));
	FIELD_ADD(h, key, ALIGN_LEFT);
	FIELD_ADD(h, val, ALIGN_LEFT);
	return h;
}

static void add_entry(int entries, const char *type, int data_h)
{
	entry_t e = {0};
	e.type_h = s_dup(type);
	e.data_h = data_h;
	m_put(entries, &e);
}

static void gather_osinfo(int entries, const char *host,
			  const char *sysname, const char *release, const char *machine)
{
	int h = m_alloc(1, sizeof(table_t), 0);
	table_t *t = (table_t *)m_buf(h);
	*t = (table_t){0};
	t->header = m_create(2, sizeof(field_t));
	FIELD_ADD(t->header, "Key", ALIGN_LEFT);
	FIELD_ADD(t->header, "Value", ALIGN_LEFT);
	t->rows = m_create(4, sizeof(int));
	int r;
	r = kv_pair("Hostname", host); m_put(t->rows, &r);
	r = kv_pair("OS", sysname);    m_put(t->rows, &r);
	r = kv_pair("Kernel", release);  m_put(t->rows, &r);
	r = kv_pair("Arch", machine);    m_put(t->rows, &r);
	add_entry(entries, "table", h);
}

static void gather_cpuinfo(int entries)
{
	int cpuinfo = read_file("/proc/cpuinfo");
	if (!cpuinfo) return;

	int buf_h = s_new();
	for (int i = 0; ; i++) {
		char c = CHAR(cpuinfo, i);
		if (!c) break;
		m_putc(buf_h, c);
	}
	m_putc(buf_h, 0);
	m_free(cpuinfo);
	const char *src = m_str(buf_h);

	int cores = 0;
	for (const char *p = src; (p = strstr(p, "model name")); p++)
		cores++;
	char buf[64];
	snprintf(buf, sizeof(buf), "%d (sysconf %ld online)", cores,
		 sysconf(_SC_NPROCESSORS_ONLN));

	int h = m_alloc(1, sizeof(table_t), 0);
	table_t *t = (table_t *)m_buf(h);
	*t = (table_t){0};
	t->header = m_create(2, sizeof(field_t));
	FIELD_ADD(t->header, "Key", ALIGN_LEFT);
	FIELD_ADD(t->header, "Value", ALIGN_LEFT);
	t->rows = m_create(4, sizeof(int));

	int r = kv_pair("Processors", buf);
	m_put(t->rows, &r);

	const char *hit = strstr(src, "model name");
	const char *model = "n/a";
	char *free_me = NULL;
	if (hit) { hit = strchr(hit, ':'); hit = hit ? hit + 1 : NULL;
		while (hit && (*hit == ' ' || *hit == '\t')) hit++;
		if (hit) { const char *nl = strchr(hit, '\n');
			free_me = nl ? strndup(hit, (size_t)(nl - hit)) : strdup(hit);
			model = free_me; } }
	r = kv_pair("Model", model);
	m_put(t->rows, &r);
	free(free_me);

	double av[3] = {0};
	if (getloadavg(av, 3) == 3) {
		snprintf(buf, sizeof(buf), "%.2f / %.2f / %.2f (1/5/15 min)",
			 av[0], av[1], av[2]);
		r = kv_pair("Load", buf);
		m_put(t->rows, &r);
	}

	add_entry(entries, "table", h);
	m_free(buf_h);
}

static void gather_meminfo(int entries)
{
	int meminfo = read_file("/proc/meminfo");
	if (!meminfo) return;

	int h = m_alloc(1, sizeof(table_t), 0);
	table_t *t = (table_t *)m_buf(h);
	*t = (table_t){0};
	t->header = m_create(2, sizeof(field_t));
	FIELD_ADD(t->header, "Key", ALIGN_LEFT);
	FIELD_ADD(t->header, "Value", ALIGN_LEFT);
	t->rows = m_create(4, sizeof(int));

	int buf_h = s_new();
	for (int i = 0; ; i++) {
		char c = CHAR(meminfo, i);
		if (!c) break;
		m_putc(buf_h, c);
	}
	m_putc(buf_h, 0);
	m_free(meminfo);
	const char *src = m_str(buf_h);

	const char *keys[] = {"MemTotal", "MemAvailable", "SwapTotal", "SwapFree"};
	for (int i = 0; i < 4; i++) {
		const char *hit = strstr(src, keys[i]);
		const char *val = "n/a";
		if (hit) { hit = strchr(hit, ':'); hit = hit ? hit + 1 : NULL;
			while (hit && (*hit == ' ' || *hit == '\t')) hit++;
			val = hit ? hit : val; }
		long long kb = 0;
		sscanf(val, "%lld", &kb);
		double gb = (double)kb / (1024.0 * 1024.0);
		char vbuf[32];
		snprintf(vbuf, sizeof(vbuf), "%.1f GB", gb);
		int r = kv_pair(keys[i], vbuf);
		m_put(t->rows, &r);
	}

	add_entry(entries, "table", h);
	m_free(buf_h);
}

static void gather_disk(int entries)
{
	struct statvfs vf;
	if (statvfs("/", &vf) != 0) return;

	double total = (double)vf.f_blocks * vf.f_frsize;
	double avail = (double)vf.f_bavail * vf.f_frsize;
	double used = total - avail;
	double frac = total > 0 ? used / total : 0.0;
	char label[128];
	snprintf(label, sizeof(label),
		 "Disk: %.1fG/%.1fG used (%.0f%%)",
		 used / (1024.0*1024.0*1024.0),
		 total / (1024.0*1024.0*1024.0),
		 frac * 100.0);

	int h = m_alloc(1, sizeof(field_t), 0);
	field_t *f = (field_t *)m_buf(h);
	*f = (field_t){ .str_h = s_dup(label), .frac = frac };
	add_entry(entries, "bar", h);
}

static void gather_proc_uptime(int entries)
{
	DIR *d = opendir("/proc");
	int nproc = 0;
	if (d) {
		struct dirent *de;
		while ((de = readdir(d))) {
			if (isdigit((unsigned char)de->d_name[0])) nproc++;
		}
		closedir(d);
	}

	int ut = read_file("/proc/uptime");
	char ubuf[64] = "";
	if (ut) {
		double up = 0;
		sscanf(m_str(ut), "%lf", &up);
		int hh = (int)up / 3600;
		int mm = ((int)up % 3600) / 60;
		int dd = hh / 24;
		hh %= 24;
		if (dd)
			snprintf(ubuf, sizeof(ubuf), "up %dd %dh %dm", dd, hh, mm);
		else
			snprintf(ubuf, sizeof(ubuf), "up %dh %dm", hh, mm);
		m_free(ut);
	}

	char buf[128];
	snprintf(buf, sizeof(buf), "%d processes, %s", nproc, ubuf);

	int h = m_alloc(1, sizeof(list_t), 0);
	list_t *l = (list_t *)m_buf(h);
	*l = (list_t){0};
	l->items = m_create(1, sizeof(field_t));
	FIELD_ADD(l->items, buf, ALIGN_LEFT);
	add_entry(entries, "list", h);
}

static void gather_users(int entries)
{
	int who_lines = subproc_lines("who 2>/dev/null");
	if (STRTAB_EMPTY(who_lines)) { m_free(who_lines); return; }
	int unames = m_create(8, sizeof(int));
	int p,*d,buf=0;
	m_foreach(who_lines,p,d) {
		buf=copy_word(buf,*d);
		if( m_binsert(unames, &buf, _cmp_mstr, 0)  >= 0 ) buf=0;
	}
        m_free(buf);
	m_free(who_lines);

	int ucount = (int)m_len(unames);
	if (!ucount) { m_free(unames); return; }

	char line[256];
	int off = snprintf(line, sizeof(line), "Users (%d): ", ucount);
	for (int i = 0; i < ucount; i++) {
		if (i) off += snprintf(line + off, sizeof(line) - (size_t)off, ", ");
		char user[64] = "";
		sscanf(m_str(INT(unames, i)), "%63s", user);
		off += snprintf(line + off, sizeof(line) - (size_t)off, "%s", user);
	}

	int h = m_alloc(1, sizeof(text_t), 0);
	text_t *t = (text_t *)m_buf(h);
	*t = (text_t){0};
	t->text_h = s_dup(line);
	add_entry(entries, "text", h);
	m_free(unames);
}

static void gather_network(int entries)
{
	DIR *d = opendir("/sys/class/net");
	if (!d) return;

	char buf[512];
	size_t off = 0;
	struct dirent *de;
	while ((de = readdir(d))) {
		if (de->d_name[0] == '.') continue;
		char devpath[512];
		snprintf(devpath, sizeof(devpath), "/sys/class/net/%s/device", de->d_name);
		struct stat st;
		if (stat(devpath, &st) != 0) continue;

		char cmd[256];
		snprintf(cmd, sizeof(cmd), "ip -br addr show %.200s 2>/dev/null", de->d_name);
		int out_h = subproc_read(cmd);
		if (STRTAB_EMPTY(out_h)) { m_free(out_h); continue; }

		char out[256];
		int oi = 0;
		for (int k = 0; oi < (int)sizeof(out) - 1; k++) {
			char c = CHAR(out_h, k);
			if (c == 0 || c == '\n') break;
			out[oi++] = c;
		}
		out[oi] = 0;
		m_free(out_h);

		char ifname[64] = "", state[16] = "", ip4[64] = "";
		sscanf(out, "%63s %15s", ifname, state);
		const char *ip = out;
		int toks = 0;
		while (*ip) {
			while (*ip == ' ' || *ip == '\t') ip++;
			if (!*ip) break;
			while (*ip && *ip != ' ' && *ip != '\t' && *ip != '\n') ip++;
			toks++;
			if (toks == 2) break;
		}
		while (*ip == ' ' || *ip == '\t') ip++;
		if (*ip && toks == 2) {
			char *slash = strchr(ip, '/');
			char *end = strchr(ip, ' ');
			if (!end) end = strchr(ip, '\n');
			if (slash && (!end || slash < end)) {
				size_t len = (size_t)(slash - ip);
				if (len >= sizeof(ip4)) len = sizeof(ip4) - 1;
				memcpy(ip4, ip, len);
			}
		}

		if (off) off += snprintf(buf + off, sizeof(buf) - off, ", ");
		if (ip4[0])
			off += snprintf(buf + off, sizeof(buf) - off, "%s (%s, %s)", ifname, state, ip4);
		else
			off += snprintf(buf + off, sizeof(buf) - off, "%s (%s)", ifname, state);
	}
	closedir(d);

	if (!off) return;

	char line[544];
	snprintf(line, sizeof(line), "Network: %s", buf);
	int h = m_alloc(1, sizeof(text_t), 0);
	text_t *t = (text_t *)m_buf(h);
	*t = (text_t){0};
	t->text_h = s_dup(line);
	add_entry(entries, "text", h);
}

int gather_system(cfg_t cfg)
{
	(void)cfg;
	struct utsname u;
	uname(&u);
	char host[256] = "n/a";
	gethostname(host, sizeof(host));

	int sec_h = m_alloc(1, sizeof(section_t), 0);
	section_t *sec = (section_t *)m_buf(sec_h);
	*sec = (section_t){0};
	sec->title = s_dup("SYSTEM");
	sec->entries = m_create(8, sizeof(entry_t));

	gather_osinfo(sec->entries, host, u.sysname, u.release, u.machine);
	gather_cpuinfo(sec->entries);
	gather_meminfo(sec->entries);
	gather_disk(sec->entries);
	gather_proc_uptime(sec->entries);
	gather_users(sec->entries);
	gather_network(sec->entries);

	return sec_h;
}

int gather_all(cfg_t cfg)
{
	int sections = m_create(8, sizeof(int));
	int sh;

	if (cfg_bool(cfg, "section", "system", 1))
		if ((sh = gather_system(cfg))) m_put(sections, &sh);
	if (cfg_bool(cfg, "section", "ports", 1))
		if ((sh = gather_ports(cfg))) m_put(sections, &sh);
	if (cfg_bool(cfg, "section", "proc", 1))
		if ((sh = gather_proc(cfg))) m_put(sections, &sh);
	if (cfg_bool(cfg, "section", "cron", 1))
		if ((sh = gather_cron(cfg))) m_put(sections, &sh);
	if (cfg_bool(cfg, "section", "firewall", 1))
		if ((sh = gather_firewall(cfg))) m_put(sections, &sh);
	if (cfg_bool(cfg, "section", "stack", 1))
		if ((sh = gather_stack(cfg))) m_put(sections, &sh);
	if (cfg_bool(cfg, "section", "lvm", 1))
		if ((sh = gather_lvm(cfg))) m_put(sections, &sh);
	if (cfg_bool(cfg, "section", "zfs", 1))
		if ((sh = gather_zfs(cfg))) m_put(sections, &sh);

	return sections;
}
