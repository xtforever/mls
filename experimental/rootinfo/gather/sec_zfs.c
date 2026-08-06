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
		int ncols = (int)m_len(toks);
		if (ncols > 5) ncols = 5;
		int row = m_create(5, sizeof(field_t));
		for (int c = 0; c < 5; c++)
			FIELD_ADD(row, c < ncols ? STR(toks, c) : "");
		m_put(t->rows, &row);
	}
	m_free(toks);

	add_entry(sec->entries, th);
	m_free(lines);

	int zstat = subproc_read("zpool status 2>/dev/null");
	if (zstat > 0 && !s_isempty(zstat)) {
		int line = s_new();
		int off = 0;
		int p, *d;
		int toks = m_alloc(8, sizeof(int), MFREE_EACH);
		s_msplit(toks, zstat, s_cstr("\n"));
		m_foreach(toks, p, d) {
			s_trim(*d);
			if (s_isempty(*d)) continue;
			if (off >= 5) break;
			if (s_has_prefix(*d, "config:") ||
			    s_has_prefix(*d, "NAME") ||
			    s_has_prefix(*d, "  errors:")) continue;
			if (off) s_cat(line, " | ");
			s_cat(line, m_str(*d));
			off++;
		}
		m_free(toks);
		add_entry(sec->entries, text_new(line));
	}
	m_free(zstat);

	return sec_h;
}
