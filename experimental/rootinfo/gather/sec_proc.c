#include "gather.h"
#include "m_types.h"
#include "m_subproc.h"
#include "m_tool.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int gather_proc(cfg_t cfg)
{
	int top = cfg_int(cfg, "proc", "top", 5);
	int cmd = s_printf(0, 0, "ps -eo user,pid,%%cpu,%%mem,comm --sort=-%%cpu 2>/dev/null");

	int lines = subproc_lines(m_str(cmd));
	m_free(cmd);
	if (STRTAB_EMPTY(lines)) { m_free(lines); return 0; }

	int sec_h = section_new("TOP PROCESSES", 2);
	section_t *sec = (section_t *)m_buf(sec_h);

	const char *cols[] = {"USER", "PID", "%CPU", "%MEM", "COMMAND"};
	align_t aligns[] = {ALIGN_LEFT, ALIGN_RIGHT, ALIGN_RIGHT, ALIGN_RIGHT, ALIGN_LEFT};
	int th = table_new_a(5, cols, aligns);
	data_t *t = (data_t *)m_buf(th);
	int count = 0;
	int p, *d;
	int toks = m_alloc(16, sizeof(char *), MFREE_STR);
	m_foreach(lines, p, d) {
		if (count >= top) break;
		if (s_has_prefix(*d, "USER") || s_has_prefix(*d, "PID")) continue;

		s_split(toks, m_buf(*d), ' ', 1);
		char **tk = (char **)m_buf(toks);
		int n = m_len(toks);
		char *fields[4] = {0};
		int fcnt = 0;
		int cmd_h = s_new();
		for (int j = 0; j < n; j++) {
			if (!tk[j][0]) continue;
			if (fcnt < 4) fields[fcnt++] = tk[j];
			else {
				if (s_strlen(cmd_h)) s_cat(cmd_h, " ");
				s_cat(cmd_h, tk[j]);
			}
		}
		if (fcnt < 4) { m_free(cmd_h); continue; }

		int row = m_create(5, sizeof(field_t));
		const char *cols2[] = {fields[0], fields[1], fields[2], fields[3],
				       m_str(cmd_h)};
		for (int j = 0; j < 5; j++) {
			field_t f_ = { .str_h = s_dup(cols2[j]),
				       .align = (j >= 1 && j <= 3) ? ALIGN_RIGHT : ALIGN_LEFT };
			m_put(row, &f_);
		}
		m_free(cmd_h);
		m_put(t->rows, &row);
		count++;
	}
	m_free(toks);

	add_entry(sec->entries, th);
	m_free(lines);

	return sec_h;
}
