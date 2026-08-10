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
	int toks = m_alloc(16, sizeof(int), MFREE_EACH);
	m_foreach(lines, p, d) {
		if (count >= top) break;
		if (s_has_prefix(*d, "USER") || s_has_prefix(*d, "PID")) continue;

		s_msplit(toks, *d, s_cstr(" "));
		int row = m_create(5, sizeof(field_t));
		int fcnt = 0, cmd_h = 0;
		for (int j = 0; j < (int)m_len(toks); j++) {
			int h = INT(toks, j);
			if (mstr_empty(h)) { m_free(h); INT(toks, j) = 0; continue; }
			if (fcnt < 4) {
				field_t f_ = { .str_h = h,
					       .align = (fcnt >= 1 && fcnt <= 3) ? ALIGN_RIGHT : ALIGN_LEFT };
				m_put(row, &f_);
				INT(toks, j) = 0;
				fcnt++;
			} else {
				if (cmd_h) s_cat(cmd_h, " ");
				else cmd_h = s_new();
				s_mcat(cmd_h, h);
				m_free(h);
				INT(toks, j) = 0;
			}
		}
		if (fcnt < 4) {
			for (int j = 0; j < (int)m_len(row); j++)
				m_free(((field_t *)m_buf(row) + j)->str_h);
			m_free(row);
			m_free(cmd_h);
			continue;
		}
		field_t f_ = { .str_h = cmd_h, .align = ALIGN_LEFT };
		m_put(row, &f_);
		m_put(t->rows, &row);
		count++;
	}
	m_free(toks);

	add_entry(sec->entries, th);
	m_free(lines);

	return sec_h;
}
