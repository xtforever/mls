#include "gather.h"
#include "m_types.h"
#include "m_subproc.h"
#include "m_tool.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int gather_zfs(cfg_t cfg)
{
	(void)cfg;
	int lines = subproc_lines("zfs list -H -o name,used,avail,refer,mountpoint 2>/dev/null");
	if (!lines) return 0;

	int n = (int)m_len(lines);
	if (!n) { m_free(lines); return 0; }

	int sec_h = m_alloc(1, sizeof(section_t), 0);
	section_t *sec = (section_t *)m_buf(sec_h);
	*sec = (section_t){0};
	sec->title = s_dup("ZFS");
	sec->entries = m_create(2, sizeof(entry_t));

	int th = m_alloc(1, sizeof(table_t), 0);
	table_t *t = (table_t *)m_buf(th);
	*t = (table_t){0};

	t->header = m_create(5, sizeof(field_t));
	const char *cols[] = {"Name", "Used", "Avail", "Refer", "Mount"};
	for (int i = 0; i < 5; i++) {
		field_t f = { .str_h = s_dup(cols[i]), .fmt = FMT_NONE, .align = ALIGN_LEFT };
		m_put(t->header, &f);
	}

	t->rows = m_create((size_t)n, sizeof(int));
	for (int i = 0; i < n; i++) {
		int *lh = (int *)m_peek(lines, (size_t)i);
		const char *line = lh ? m_str(*lh) : "";
		if (!line[0]) continue;

		int row = m_create(5, sizeof(field_t));
		const char *p = line;
		for (int c = 0; c < 5; c++) {
			while (*p == ' ' || *p == '\t') p++;
			if (!*p) break;
			const char *end = p;
			while (*end && *end != '\t') end++;
			char *s = strndup(p, (size_t)(end - p));
			field_t f = { .str_h = s_dup(s), .fmt = FMT_NONE, .align = ALIGN_LEFT };
			free(s);
			m_put(row, &f);
			p = *end ? end + 1 : end;
		}
		m_put(t->rows, &row);
	}

	entry_t e = { .type_h = s_dup("table"), .data_h = th };
	m_put(sec->entries, &e);
	m_free(lines);

	return sec_h;
}
