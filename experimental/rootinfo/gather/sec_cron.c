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

	int th = m_alloc(1, sizeof(table_t), 0);
	table_t *t = (table_t *)m_buf(th);
	*t = (table_t){0};
	t->title_h = 0;

	t->header = m_create(2, sizeof(field_t));
	FIELD_ADD(t->header, "Schedule", ALIGN_LEFT);
	FIELD_ADD(t->header, "Command", ALIGN_LEFT);

	t->rows = m_create((size_t)(max_lines + 1), sizeof(int));
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
			FIELD_ADD(row, s, ALIGN_LEFT);
			*cmd = saved;
			FIELD_ADD(row, cmd, ALIGN_LEFT);
		} else {
			FIELD_ADD(row, "", ALIGN_LEFT);
			FIELD_ADD(row, s, ALIGN_LEFT);
		}
		m_put(t->rows, &row);
		count++;
	}
	if (count > 0) {
		entry_t e = { .type_h = s_dup("table"), .data_h = th };
		m_put(entries, &e);
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

	int th = m_alloc(1, sizeof(text_t), 0);
	text_t *t = (text_t *)m_buf(th);
	*t = (text_t){0};
	t->text_h = line;
	entry_t e = { .type_h = s_dup("text"), .data_h = th };
	m_put(entries, &e);
}

int gather_cron(cfg_t cfg)
{
	int max_lines = cfg_int(cfg, "cron", "max_lines", 10);

	int sec_h = m_alloc(1, sizeof(section_t), 0);
	section_t *sec = (section_t *)m_buf(sec_h);
	*sec = (section_t){0};
	sec->title = s_dup("CRON");
	sec->entries = m_create(8, sizeof(entry_t));

	add_cron_table(sec->entries, "Crontab",
		subproc_lines("crontab -l 2>/dev/null"), max_lines);

	add_cron_list_line(sec->entries, "Hourly",  "/etc/cron.hourly");
	add_cron_list_line(sec->entries, "Daily",   "/etc/cron.daily");
	add_cron_list_line(sec->entries, "Weekly",  "/etc/cron.weekly");
	add_cron_list_line(sec->entries, "Monthly", "/etc/cron.monthly");
	add_cron_list_line(sec->entries, "Users",   "/var/spool/cron/crontabs");

	return sec_h;
}
