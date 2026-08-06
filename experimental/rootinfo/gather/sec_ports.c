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
	int ipv6 = cfg_bool(cfg, "ports", "ipv6", 1);
	int is_root = (geteuid() == 0);
	int ncols = is_root ? 3 : 2;

	const char *cmd = ipv6 ? "ss -tlpn 2>/dev/null" : "ss -tlp4n 2>/dev/null";
	int lines = subproc_lines(cmd);
	if (STRTAB_EMPTY(lines)) { m_free(lines); return 0; }

	int sec_h = section_new("OPEN PORTS", 2);
	section_t *sec = (section_t *)m_buf(sec_h);

	int th = table_new(ncols, (const char *[]){"Address", "Port", "Process"});
	data_t *t = (data_t *)m_buf(th);
	int toks = m_alloc(8, sizeof(char *), MFREE_STR);
	int count = 0;
	int p, *d;
	m_foreach(lines, p, d) {
		if (count >= max) break;
		int line = *d;
		if (s_has_prefix(line, "State") || s_has_prefix(line, "Netid")) continue;

		s_split(toks, m_buf(line), ' ', 1);
		int tok = m_alloc(8, sizeof(int), MFREE_EACH);
		int n = 0;
		for (int j = 0; j < (int)m_len(toks) && n < 8; j++)
			if (STR(toks, j)[0]) {
				int h = s_dup(STR(toks, j));
				m_put(tok, &h);
				n++;
			}
		if (n < 5) { m_free(tok); continue; }

		char *local = m_str(INT(tok, 3));
		char *colon = strrchr(local, ':');
		if (!colon || !colon[1]) { m_free(tok); continue; }

		int ah = s_dup(local);
		int a2 = s_left(ah, (int)(colon - local));
		m_free(ah);
		ah = a2;
		int ph = s_dup(colon + 1);
		m_free(tok);

		int proc_h = 0;
		if (is_root) {
			int name = s_find(line, "((\"");
			if (name >= 0) {
				int start = name + 3;
				int end = s_find(line, "\",pid=");
				if (end >= 0)
					proc_h = s_slice(0, 0, line, start, end - 1);
			}
		}

		int row = m_create((size_t)ncols, sizeof(field_t));
		FIELD_ADD_H(row, ah);
		FIELD_ADD_H(row, ph);
		if (is_root)
			FIELD_ADD_H(row, proc_h);

		m_put(t->rows, &row);
		count++;
	}
	m_free(toks);

	add_entry(sec->entries, th);
	m_free(lines);

	return sec_h;
}
