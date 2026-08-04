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
	if (STRTAB_EMPTY(lines)) { m_free(lines); return 0; }

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
	for (int i = 0; i < 5; i++)
		FIELD_ADD(t->header, cols[i], ALIGN_LEFT);

	t->rows = m_create(m_len(lines), sizeof(int));
	int p, *d;
	m_foreach(lines, p, d) {
		char line[512];
		STR_COPY(line, sizeof(line), *d);
		if (!line[0]) continue;

		int row = m_create(5, sizeof(field_t));
		const char *p = line;
		for (int c = 0; c < 5; c++) {
			while (*p == ' ' || *p == '\t') p++;
			if (!*p) break;
			const char *end = p;
			while (*end && *end != '\t') end++;
			char *s = strndup(p, (size_t)(end - p));
			FIELD_ADD(row, s, ALIGN_LEFT);
			free(s);
			p = *end ? end + 1 : end;
		}
		m_put(t->rows, &row);
	}

	entry_t e = { .type_h = s_dup("table"), .data_h = th };
	m_put(sec->entries, &e);
	m_free(lines);

	return sec_h;
}
