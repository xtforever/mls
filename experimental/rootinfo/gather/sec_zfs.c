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
	int toks = m_alloc(10, sizeof(int), MFREE_EACH);
	int p, *d;
	m_foreach(lines, p, d) {
		if (s_isempty(*d)) continue;
		s_msplit(toks, *d, s_cstr("\t"));
		int row = m_create(5, sizeof(field_t));
		for (int c = 0; c < (int)m_len(toks); c++) {
			if (c < 5) {
				field_t f_ = { .str_h = INT(toks, c) };
				m_put(row, &f_);
			} else {
				m_free(INT(toks, c));
			}
			INT(toks, c) = 0;
		}
		while ((int)m_len(row) < 5) FIELD_ADD(row, "");
		m_put(t->rows, &row);
	}
	m_free(toks);

	add_entry(sec->entries, th);
	m_free(lines);

	int zstat = subproc_read("zpool status 2>/dev/null");
	if (zstat > 0 && !s_isempty(zstat)) {
		int pool_h = 0, state_h = 0, scan_h = 0;
		int th = table_new(5, (const char *[]){"NAME", "STATE", "READ", "WRITE", "CKSUM"});
		data_t *t = (data_t *)m_buf(th);
		int p, *d;
		int toks = m_alloc(8, sizeof(int), MFREE_EACH);
		int dtoks = m_alloc(8, sizeof(int), MFREE_EACH);
		s_msplit(toks, zstat, s_cstr("\n"));
		int cfg = 0;
		m_foreach(toks, p, d) {
			int l = *d;
			s_trim(l);
			if (s_isempty(l)) continue;
			if (s_has_prefix(l, "pool: ")) { m_free(pool_h); pool_h = s_slice(0, 0, l, 6, -1); continue; }
			if (s_has_prefix(l, "state: ")) { m_free(state_h); state_h = s_slice(0, 0, l, 7, -1); continue; }
			if (s_has_prefix(l, "scan: ")) { m_free(scan_h); scan_h = s_slice(0, 0, l, 6, -1); continue; }
			if (s_has_prefix(l, "config:")) { cfg = 1; continue; }
			if (!cfg || s_has_prefix(l, "NAME") || s_has_prefix(l, "errors:")) continue;
			/* ponytail: bare "logs"/"cache"/"spares" section labels inside
			   config are dropped (they split on < 5 tokens); their device
			   rows still appear. */

			s_msplit(dtoks, l, s_cstr(" "));
			int row = m_create(5, sizeof(field_t));
			int c = 0;
			for (int j = 0; j < (int)m_len(dtoks); j++) {
				int h = INT(dtoks, j);
				if (mstr_empty(h)) { m_free(h); INT(dtoks, j) = 0; continue; }
				if (c < 5) {
					field_t f_ = { .str_h = h, .align = c ? ALIGN_RIGHT : ALIGN_LEFT };
					m_put(row, &f_);
					INT(dtoks, j) = 0;
				} else {
					m_free(h);
					INT(dtoks, j) = 0;
				}
				c++;
			}
			if (c < 5) {
				for (int j = 0; j < (int)m_len(row); j++)
					m_free(((field_t *)m_buf(row) + j)->str_h);
				m_free(row);
				continue;
			}
			m_put(t->rows, &row);
		}
		m_free(toks);
		m_free(dtoks);

		int sum = s_printf(0, 0, "pool: %s", pool_h ? m_str(pool_h) : "(unknown)");
		if (state_h) s_printf(sum, -1, ", state: %s", m_str(state_h));
		if (scan_h) s_printf(sum, -1, ", scan: %s", m_str(scan_h));
		m_free(pool_h);
		m_free(state_h);
		m_free(scan_h);
		add_entry(sec->entries, text_new(sum));
		if (m_len(t->rows))
			add_entry(sec->entries, th);
		else
			m_free(th);
	}
	m_free(zstat);

	return sec_h;
}
