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
	if (!lines) return 0;

	int n = (int)m_len(lines);
	if (!n) { m_free(lines); return 0; }

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
	for (int i = 0; i < ncols; i++) {
		field_t f = { .str_h = s_dup(cols[i]), .fmt = FMT_NONE, .align = ALIGN_LEFT };
		m_put(t->header, &f);
	}

	t->rows = m_create((size_t)(max + 1), sizeof(int));
	int count = 0;
	for (int i = 0; i < n && count < max; i++) {
		int *lh = (int *)m_peek(lines, (size_t)i);
		const char *line = lh ? m_str(*lh) : "";
		if (!line[0] || strstr(line, "State") || strstr(line, "Netid")) continue;

		char addr_buf[64] = "";
		char port_buf[16] = "";
		char proc_buf[128] = "";

		const char *colon = strchr(line, ':');
		if (colon) {
			const char *a = colon;
			while (a > line && *(a-1) != ' ') a--;
			size_t alen = (size_t)(colon - a);
			if (alen >= sizeof(addr_buf)) alen = sizeof(addr_buf) - 1;
			memcpy(addr_buf, a, alen);

			const char *p = colon + 1;
			while (*p >= '0' && *p <= '9') {
				if (p - colon - 1 < (int)sizeof(port_buf) - 1)
					port_buf[p - colon - 1] = *p;
				p++;
			}
		}

		if (is_root) {
			const char *second_colon = colon ? strchr(colon + 1, ':') : NULL;
			if (second_colon) {
				const char *proc = second_colon + 1;
				while (*proc && *proc != ' ') proc++;
				while (*proc == ' ') proc++;
				if (*proc) {
					size_t plen = strlen(proc);
					if (plen >= sizeof(proc_buf)) plen = sizeof(proc_buf) - 1;
					memcpy(proc_buf, proc, plen);
				}
			}
			char *name = strstr(proc_buf, "((\"");
			if (name) {
				name += 3;
				char *end = strstr(name, "\",pid=");
				if (end) {
					size_t len = (size_t)(end - name);
					memmove(proc_buf, name, len);
					proc_buf[len] = 0;
				}
			}
		}

		int row = m_create((size_t)ncols, sizeof(field_t));
		field_t f = { .str_h = s_dup(addr_buf), .fmt = FMT_NONE, .align = ALIGN_LEFT };
		m_put(row, &f);
		f.str_h = s_dup(port_buf);
		m_put(row, &f);
		if (is_root) {
			f.str_h = s_dup(proc_buf);
			m_put(row, &f);
		}

		m_put(t->rows, &row);
		count++;
	}

	entry_t e = { .type_h = s_dup("table"), .data_h = th };
	m_put(sec->entries, &e);
	m_free(lines);

	return sec_h;
}
