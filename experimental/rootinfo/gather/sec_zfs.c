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

	int sec_h = section_new("ZFS", 2);
	section_t *sec = (section_t *)m_buf(sec_h);

	int th = table_new(5, (const char *[]){"Name", "Used", "Avail", "Refer", "Mount"});
	data_t *t = (data_t *)m_buf(th);
	int toks = m_alloc(10, sizeof(char *), MFREE_STR);
	int p, *d;
	m_foreach(lines, p, d) {
		if (s_isempty(*d)) continue;
		s_split(toks, m_buf(*d), '\t', 1);
		char **tk = (char **)m_buf(toks);
		int ncols = (int)m_len(toks);
		if (ncols > 5) ncols = 5;
		int row = m_create(5, sizeof(field_t));
		for (int c = 0; c < 5; c++)
			FIELD_ADD(row, c < ncols ? tk[c] : "");
		m_put(t->rows, &row);
	}
	m_free(toks);

	add_entry(sec->entries, th);
	m_free(lines);

	return sec_h;
}
