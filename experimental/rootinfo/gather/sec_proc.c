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
	m_foreach(lines, p, d) {
		if (count >= top) break;
		char line[512];
		STR_COPY(line, sizeof(line), *d);
		if (!line[0] || strstr(line, "USER") || strstr(line, "PID")) continue;

		int row = m_create(5, sizeof(field_t));
		const char *fields[5];
		int fc = 0;
		const char *p = line;
		while (*p == ' ') p++;
		for (int j = 0; j < 4 && *p; j++) {
			fields[fc++] = p;
			while (*p && *p != ' ') p++;
			while (*p == ' ') p++;
		}
		if (*p) fields[fc++] = p;

		for (int j = 0; j < fc && j < 5; j++) {
			const char *end = (j < 4) ? strchr(fields[j], ' ') : NULL;
			size_t len = end ? (size_t)(end - fields[j]) : strlen(fields[j]);
			char *v = strndup(fields[j], len);
			align_t a = (j >= 1 && j <= 3) ? ALIGN_RIGHT : ALIGN_LEFT;
			FIELD_ADD(row, v, a);
			free(v);
		}
		m_put(t->rows, &row);
		count++;
	}

	entry_t e = { .type_h = s_dup("table"), .data_h = th };
	m_put(sec->entries, &e);
	m_free(lines);

	return sec_h;
}
