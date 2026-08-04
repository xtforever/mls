#include "gather.h"
#include "m_types.h"
#include "m_subproc.h"
#include "m_tool.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void human_size(char *buf, size_t sz, unsigned long long bytes)
{
	static const char *units[] = {"B", "K", "M", "G", "T", "P"};
	double v = (double)bytes;
	int ui = 0;
	while (v >= 1024.0 && ui < 5) { v /= 1024.0; ui++; }
	if (ui == 0)
		snprintf(buf, sz, "%llu%s", bytes, units[ui]);
	else
		snprintf(buf, sz, "%.1f%s", v, units[ui]);
}

typedef struct {
	char pv_name[256];
	char lv_name[256];
	char vg_name[256];
	unsigned long long lv_size;
	unsigned long long vg_free;
	unsigned long long vg_size;
	char mount[256];
	char lv_path[256];
} lv_row_t;

int gather_lvm(cfg_t cfg)
{
	(void)cfg;

	int lvs_out = subproc_read("lvs --noheadings --units B --separator '|' -o lv_name,vg_name,lv_size,lv_path 2>/dev/null");
	int vgs_out = subproc_read("vgs --noheadings --units B --separator '|' -o vg_name,vg_size,vg_free 2>/dev/null");
	int pvs_out = subproc_read("pvs --noheadings  --separator '|' -o pv_name,vg_name 2>/dev/null");

	if (!lvs_out) return 0;

	int nl_h = s_dup("\n");
	int lv_lines = s_msplit(0, lvs_out, nl_h);
	m_free(lvs_out);
	m_free(nl_h);

	int n_lv = (int)m_len(lv_lines);
	if (!n_lv) { m_free(lv_lines); return 0; }

	int n_vg = 0;
	char vg_names[64][256];
	unsigned long long vg_sizes[64];
	unsigned long long vg_frees[64];
	if (vgs_out) {
		nl_h = s_dup("\n");
		int vg_lines = s_msplit(0, vgs_out, nl_h);
		m_free(vgs_out);
		m_free(nl_h);
		n_vg = (int)m_len(vg_lines);
		for (int i = 0; i < n_vg && i < 64; i++) {
			int *h = (int *)m_peek(vg_lines, (size_t)i);
			const char *line = h ? m_str(*h) : "";
			char *copy = strdup(line);
			char *save = NULL;
			char *tok = strtok_r(copy, "|", &save);
			int col = 0;
			while (tok && col < 3) {
				while (*tok == ' ' || *tok == '\t') tok++;
				switch (col) {
				case 0: snprintf(vg_names[i], sizeof(vg_names[i]), "%s", tok); break;
				case 1: vg_sizes[i] = strtoull(tok, NULL, 10); break;
				case 2: vg_frees[i] = strtoull(tok, NULL, 10); break;
				}
				tok = strtok_r(NULL, "|", &save);
				col++;
			}
			free(copy);
		}
		m_free(vg_lines);
	}

	int n_pv = 0;
	char pv_names[64][256];
	char pv_vgs[64][256];
	if (pvs_out) {
		nl_h = s_dup("\n");
		int pv_lines = s_msplit(0, pvs_out, nl_h);
		m_free(pvs_out);
		m_free(nl_h);
		n_pv = (int)m_len(pv_lines);
		for (int i = 0; i < n_pv && i < 64; i++) {
			int *h = (int *)m_peek(pv_lines, (size_t)i);
			const char *line = h ? m_str(*h) : "";
			char *copy = strdup(line);
			char *save = NULL;
			char *tok = strtok_r(copy, "|", &save);
			if (tok) {
				while (*tok == ' ') tok++;
				snprintf(pv_names[i], sizeof(pv_names[i]), "%s", tok);
				tok = strtok_r(NULL, "|", &save);
				if (tok) {
					while (*tok == ' ') tok++;
					snprintf(pv_vgs[i], sizeof(pv_vgs[i]), "%s", tok);
				}
			}
			free(copy);
		}
		m_free(pv_lines);
	}

	lv_row_t rows[256];
	int n_rows = 0;
	for (int i = 0; i < n_lv && n_rows < 256; i++) {
		int *h = (int *)m_peek(lv_lines, (size_t)i);
		const char *line = h ? m_str(*h) : "";
		if (!line[0]) continue;

		lv_row_t *r = &rows[n_rows];
		memset(r, 0, sizeof(*r));

		char *copy = strdup(line);
		char *save = NULL;
		char *tok = strtok_r(copy, "|", &save);
		int col = 0;
		while (tok && col < 4) {
			while (*tok == ' ' || *tok == '\t') tok++;
			switch (col) {
			case 0: snprintf(r->lv_name, sizeof(r->lv_name), "%s", tok); break;
			case 1: snprintf(r->vg_name, sizeof(r->vg_name), "%s", tok); break;
			case 2: r->lv_size = strtoull(tok, NULL, 10); break;
			case 3: snprintf(r->lv_path, sizeof(r->lv_path), "%s", tok); break;
			}
			tok = strtok_r(NULL, "|", &save);
			col++;
		}
		free(copy);

		/* lookup VG free/size */
		for (int v = 0; v < n_vg; v++) {
			if (!strcmp(r->vg_name, vg_names[v])) {
				r->vg_size = vg_sizes[v];
				r->vg_free = vg_frees[v];
				break;
			}
		}

		/* lookup PV name for this VG */
		for (int p = 0; p < n_pv; p++) {
			if (!strcmp(r->vg_name, pv_vgs[p])) {
				snprintf(r->pv_name, sizeof(r->pv_name), "%s", pv_names[p]);
				break;
			}
		}

		/* find mountpoint from /proc/mounts using device path or dm-crypt variant */
		if (r->lv_path[0]) {
			/* build dm-crypt device name: /dev/mapper/<vg>-<lv> */
			char dm_name[256];
			int n = snprintf(dm_name, sizeof(dm_name), "/dev/mapper/");
			snprintf(dm_name + n, sizeof(dm_name) - (size_t)n, "%.100s-%.100s", r->vg_name, r->lv_name);
			FILE *mt = fopen("/proc/mounts", "r");
			if (mt) {
				char mline[512];
				while (fgets(mline, sizeof(mline), mt)) {
					char dev[256], mnt[256];
					if (sscanf(mline, "%255s %255s", dev, mnt) == 2) {
						if (!strcmp(dev, r->lv_path) || !strcmp(dev, dm_name)) {
							snprintf(r->mount, sizeof(r->mount), "%s", mnt);
							break;
						}
					}
				}
				fclose(mt);
			}
		}

		n_rows++;
	}
	m_free(lv_lines);

	if (!n_rows) return 0;

	int sec_h = m_alloc(1, sizeof(section_t), 0);
	section_t *sec = (section_t *)m_buf(sec_h);
	*sec = (section_t){0};
	sec->title = s_dup("LVM");
	sec->entries = m_create(2, sizeof(entry_t));

	int th = m_alloc(1, sizeof(table_t), 0);
	table_t *t = (table_t *)m_buf(th);
	*t = (table_t){0};

	t->header = m_create(7, sizeof(field_t));
	const char *cols[] = {"PV", "VG", "LV", "Size", "Free", "Mount", ""};
	for (int i = 0; i < 7; i++) {
		field_t f = { .str_h = s_dup(cols[i]), .fmt = FMT_NONE, .align = ALIGN_LEFT };
		m_put(t->header, &f);
	}

	t->rows = m_create((size_t)n_rows, sizeof(int));

	const char *prev_pv = "";
	const char *prev_vg = "";

	for (int i = 0; i < n_rows; i++) {
		lv_row_t *r = &rows[i];

		char size_buf[32], free_buf[32];
		human_size(size_buf, sizeof(size_buf), r->lv_size);

		int free_row = m_create(7, sizeof(field_t));

		/* PV */
		const char *pv_show = r->pv_name;
		if (i > 0 && !strcmp(r->pv_name, prev_pv) && r->pv_name[0])
			pv_show = "\"";
		field_t f = { .str_h = s_dup(pv_show), .fmt = FMT_NONE, .align = ALIGN_LEFT };
		m_put(free_row, &f);
		prev_pv = r->pv_name;

		/* VG */
		const char *vg_show = r->vg_name;
		if (i > 0 && !strcmp(r->vg_name, prev_vg) && r->vg_name[0])
			vg_show = "\"";
		f.str_h = s_dup(vg_show);
		m_put(free_row, &f);
		prev_vg = r->vg_name;

		/* LV */
		f.str_h = s_dup(r->lv_name);
		f.align = ALIGN_LEFT;
		m_put(free_row, &f);

		/* Size */
		f.str_h = s_dup(size_buf);
		f.align = ALIGN_RIGHT;
		m_put(free_row, &f);

		/* Free */
		if (r->vg_free) {
			human_size(free_buf, sizeof(free_buf), r->vg_free);
			f.str_h = s_dup(free_buf);
		} else {
			f.str_h = s_dup("-");
		}
		f.align = ALIGN_RIGHT;
		m_put(free_row, &f);

		/* Mount */
		f.str_h = s_dup(r->mount[0] ? r->mount : "-");
		f.align = ALIGN_LEFT;
		m_put(free_row, &f);

		/* Bar column */
		f.str_h = s_dup("");
		f.align = ALIGN_LEFT;
		f.is_bar = 1;
		f.frac = r->vg_size ? (double)(r->vg_size - r->vg_free) / (double)r->vg_size : 0.0;
		f.len = 0;
		m_put(free_row, &f);

		m_put(t->rows, &free_row);
	}

	entry_t e = { .type_h = s_dup("table"), .data_h = th };
	m_put(sec->entries, &e);

	return sec_h;
}
