#include "gather.h"
#include "m_types.h"
#include "m_subproc.h"
#include "m_tool.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <ctype.h>

static void add_cron_table(int entries, const char *title, int lines, int max_lines)
{
	if (STRTAB_EMPTY(lines)) { m_free(lines); return; }

	int th = table_new(2, (const char *[]){"Schedule", "Command"});
	data_t *t = (data_t *)m_buf(th);
	int count = 0;
	int p, *d;
	m_foreach(lines, p, d) {
		if (count >= max_lines) break;
		char line[512];
		STR_COPY(line, sizeof(line), *d);
		const char *s = line;
		if (!s[0] || s[0] == '#') continue;

		while (*s == ' ' || *s == '\t') s++;
		if (!*s) continue;

		int row = m_create(2, sizeof(field_t));

		if (isdigit((unsigned char)*s) || *s == '*' || *s == '@') {
			const char *p = s;
			int sp = 0;
			while (*p && sp < 5) {
				if (*p == ' ' || *p == '\t') sp++;
				p++;
			}
			char *cmd = (char *)p;
			while (*cmd == ' ' || *cmd == '\t') cmd++;
			char saved = *cmd;
			*cmd = 0;
			FIELD_ADD(row, s);
			*cmd = saved;
			FIELD_ADD(row, cmd);
		} else {
			FIELD_ADD(row, "");
			FIELD_ADD(row, s);
		}
		m_put(t->rows, &row);
		count++;
	}
	if (count > 0) {
		add_entry(entries, th);
	} else {
		m_free(th);
	}
	m_free(lines);
}

static void add_cron_list_line(int entries, const char *label, const char *dir)
{
	DIR *d = opendir(dir);
	if (!d) return;

	int line = s_printf(0, 0, "%s: ", label);
	int off = 0;
	struct dirent *de;
	while ((de = readdir(d))) {
		if (de->d_name[0] == '.') continue;
		if (off) s_cat(line, ", ");
		off = 1;
		s_cat(line, de->d_name);
	}
	closedir(d);

	if (!off) { m_free(line); return; }

	add_entry(entries, text_new(line));
}

int gather_cron(cfg_t cfg)
{
	int max_lines = cfg_int(cfg, "cron", "max_lines", 10);

	int sec_h = section_new("CRON", 8);
	section_t *sec = (section_t *)m_buf(sec_h);

	add_cron_table(sec->entries, "Crontab",
		subproc_lines("crontab -l 2>/dev/null"), max_lines);

	add_cron_list_line(sec->entries, "Hourly",  "/etc/cron.hourly");
	add_cron_list_line(sec->entries, "Daily",   "/etc/cron.daily");
	add_cron_list_line(sec->entries, "Weekly",  "/etc/cron.weekly");
	add_cron_list_line(sec->entries, "Monthly", "/etc/cron.monthly");
	add_cron_list_line(sec->entries, "Users",   "/var/spool/cron/crontabs");

	return sec_h;
}
