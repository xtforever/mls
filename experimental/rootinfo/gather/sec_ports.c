#include "gather.h"
#include "m_types.h"
#include "m_subproc.h"
#include "m_tool.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

int gather_ports(cfg_t cfg)
{
	int max = cfg_int(cfg, "ports", "max", 5);
	int ipv4_only = cfg_bool(cfg, "ports", "ipv4", 1);
	int is_root = (geteuid() == 0);
	int ncols = is_root ? 3 : 2;

	const char *cmd = ipv4_only ? "ss -tlp4n 2>/dev/null" : "ss -tlpn 2>/dev/null";
	int lines = subproc_lines(cmd);
	if (STRTAB_EMPTY(lines)) { m_free(lines); return 0; }

	int sec_h = m_alloc(1, sizeof(section_t), 0);
	section_t *sec = (section_t *)m_buf(sec_h);
	*sec = (section_t){0};
	sec->title = s_dup("OPEN PORTS");
	sec->entries = m_create(2, sizeof(entry_t));

	int th = m_alloc(1, sizeof(table_t), 0);
	table_t *t = (table_t *)m_buf(th);
	*t = (table_t){0};

	t->header = m_create((size_t)ncols, sizeof(field_t));
	const char *cols[] = {"Address", "Port", "Process"};
	for (int i = 0; i < ncols; i++)
		FIELD_ADD(t->header, cols[i], ALIGN_LEFT);

	t->rows = m_create((size_t)(max + 1), sizeof(int));
	int count = 0;
	int p, *d;
	m_foreach(lines, p, d) {
		if (count >= max) break;
		int line = *d;
		if (s_has_prefix(line, "State") || s_has_prefix(line, "Netid")) continue;

		int ah = 0, ph = 0, proc_h = 0;

		int c0 = s_chr(line, ':', 0);
		if (c0 >= 0) {
			int a = c0;
			while (a > 0 && CHAR(line, a - 1) != ' ') a--;
			ah = s_slice(0, 0, line, a, c0 - 1);

			int pe = c0 + 1;
			while (isdigit((unsigned char)CHAR(line, pe))) pe++;
			ph = s_slice(0, 0, line, c0 + 1, pe - 1);

			if (is_root) {
				int sc = s_chr(line, ':', c0 + 1);
				if (sc >= 0) {
					int pp = sc + 1;
					while (CHAR(line, pp) && CHAR(line, pp) != ' ') pp++;
					while (CHAR(line, pp) == ' ') pp++;
					if (CHAR(line, pp)) {
						int end = pp;
						while (CHAR(line, end) && CHAR(line, end) != ' ') end++;
						proc_h = s_slice(0, 0, line, pp, end - 1);
					}
				}
				if (proc_h) {
					int name = s_find(proc_h, "((\"");
					if (name >= 0) {
						int start = name + 3;
						int end = s_find(proc_h, "\",pid=");
						if (end >= 0) {
							int nh = s_slice(0, 0, proc_h, start, end - 1);
							m_free(proc_h);
							proc_h = nh;
						} else {
							m_free(proc_h);
							proc_h = 0;
						}
					}
				}
			}
		}

		int row = m_create((size_t)ncols, sizeof(field_t));
		FIELD_ADD_H(row, ah, ALIGN_LEFT);
		FIELD_ADD_H(row, ph, ALIGN_LEFT);
		if (is_root)
			FIELD_ADD_H(row, proc_h, ALIGN_LEFT);

		m_put(t->rows, &row);
		count++;
	}

	entry_t e = { .type_h = s_dup("table"), .data_h = th };
	m_put(sec->entries, &e);
	m_free(lines);

	return sec_h;
}
