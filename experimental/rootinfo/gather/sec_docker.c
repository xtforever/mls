#include "gather.h"
#include "m_types.h"
#include "m_subproc.h"
#include "m_tool.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int gather_docker(cfg_t cfg)
{
	(void)cfg;
	int lines = subproc_lines("docker ps --format '{{.Names}}|{{.Image}}|{{.Ports}}|{{.Status}}' 2>/dev/null");
	if (STRTAB_EMPTY(lines)) { m_free(lines); return 0; }

	int sec_h = section_new("DOCKER", 2);
	section_t *sec = (section_t *)m_buf(sec_h);

	int th = table_new(4, (const char *[]){"Name", "Image", "Ports", "Status"});
	data_t *t = (data_t *)m_buf(th);
	int toks = m_alloc(8, sizeof(char *), MFREE_STR);
	int p, *d;
	m_foreach(lines, p, d) {
		if (s_isempty(*d)) continue;
		s_split(toks, m_buf(*d), '|', 0);
		char **tk = (char **)m_buf(toks);
		int ncols = (int)m_len(toks);
		if (ncols > 4) ncols = 4;
		int row = m_create(4, sizeof(field_t));
		for (int c = 0; c < 4; c++)
			FIELD_ADD(row, c < ncols ? tk[c] : "");
		m_put(t->rows, &row);
	}
	m_free(toks);

	add_entry(sec->entries, th);
	m_free(lines);

	return sec_h;
}
