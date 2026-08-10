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
	int toks = m_alloc(8, sizeof(int), MFREE_EACH);
	int count = 0;
	int p, *d;
	m_foreach(lines, p, d) {
		if (count >= max) break;
		int line = *d;
		if (s_has_prefix(line, "State") || s_has_prefix(line, "Netid")) continue;

		s_msplit(toks, line, s_cstr(" "));
		int n = 0, local = 0;
		for (int j = 0; j < (int)m_len(toks); j++) {
			int h = INT(toks, j);
			if (mstr_empty(h)) { m_free(h); INT(toks, j) = 0; continue; }
			n++;
			if (n == 4) { local = h; INT(toks, j) = 0; }
			else { m_free(h); INT(toks, j) = 0; }
		}
		if (n < 5) { m_free(local); continue; }

		int colon = s_rchr(local, ':');
		if (colon < 0 || colon + 1 >= s_strlen(local)) { m_free(local); continue; }
		int ah = s_left(local, colon);
		int ph = s_slice(0, 0, local, colon + 1, -1);
		m_free(local);

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
