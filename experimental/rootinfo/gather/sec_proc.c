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
	char cmd[128];
	snprintf(cmd, sizeof(cmd), "ps -eo user,pid,%%cpu,%%mem,comm --sort=-%%cpu 2>/dev/null");

	int lines = subproc_lines(cmd);
	if (STRTAB_EMPTY(lines)) { m_free(lines); return 0; }

	int sec_h = m_alloc(1, sizeof(section_t), 0);
	section_t *sec = (section_t *)m_buf(sec_h);
	*sec = (section_t){0};
	sec->title = s_dup("TOP PROCESSES");
	sec->entries = m_create(2, sizeof(entry_t));

	int th = m_alloc(1, sizeof(table_t), 0);
	table_t *t = (table_t *)m_buf(th);
	*t = (table_t){0};

	t->header = m_create(5, sizeof(field_t));
	const char *cols[] = {"USER", "PID", "%CPU", "%MEM", "COMMAND"};
	align_t aligns[] = {ALIGN_LEFT, ALIGN_RIGHT, ALIGN_RIGHT, ALIGN_RIGHT, ALIGN_LEFT};
	for (int i = 0; i < 5; i++)
		FIELD_ADD(t->header, cols[i], aligns[i]);

	t->rows = m_create((size_t)(top + 1), sizeof(int));
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
			align_t a = (j >= 1 && j <= 3) ? ALIGN_RIGHT : ALIGN_LEFT;
			FIELD_ADD(row, cols2[j], a);
		}
		m_free(cmd_h);
		m_put(t->rows, &row);
		count++;
	}
	m_free(toks);

	entry_t e = { .type_h = s_dup("table"), .data_h = th };
	m_put(sec->entries, &e);
	m_free(lines);

	return sec_h;
}
