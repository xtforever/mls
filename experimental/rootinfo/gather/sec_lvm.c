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

	if (STRTAB_EMPTY(lvs_out)) { m_free(lvs_out); m_free(vgs_out); m_free(pvs_out); return 0; }

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
		int vgp, *vgd;
		m_foreach(vg_lines, vgp, vgd) {
			if (vgp >= 64) break;
			char line[512];
			STR_COPY(line, sizeof(line), *vgd);
			char *copy = strdup(line);
			char *save = NULL;
			char *tok = strtok_r(copy, "|", &save);
			int col = 0;
			while (tok && col < 3) {
				while (*tok == ' ' || *tok == '\t') tok++;
				switch (col) {
				case 0: snprintf(vg_names[vgp], sizeof(vg_names[vgp]), "%s", tok); break;
				case 1: vg_sizes[vgp] = strtoull(tok, NULL, 10); break;
				case 2: vg_frees[vgp] = strtoull(tok, NULL, 10); break;
				}
				tok = strtok_r(NULL, "|", &save);
				col++;
			}
			free(copy);
		}
		n_vg = (int)m_len(vg_lines);
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
		int pvp, *pvd;
		m_foreach(pv_lines, pvp, pvd) {
			if (pvp >= 64) break;
			char line[512];
			STR_COPY(line, sizeof(line), *pvd);
			char *copy = strdup(line);
			char *save = NULL;
			char *tok = strtok_r(copy, "|", &save);
			if (tok) {
				while (*tok == ' ') tok++;
				snprintf(pv_names[pvp], sizeof(pv_names[pvp]), "%s", tok);
				tok = strtok_r(NULL, "|", &save);
				if (tok) {
					while (*tok == ' ') tok++;
					snprintf(pv_vgs[pvp], sizeof(pv_vgs[pvp]), "%s", tok);
				}
			}
			free(copy);
		}
		n_pv = (int)m_len(pv_lines);
		m_free(pv_lines);
	}

	lv_row_t rows[256];
	int n_rows = 0;
	int lvp, *lvd;
	m_foreach(lv_lines, lvp, lvd) {
		if (n_rows >= 256) break;
		char line[512];
		STR_COPY(line, sizeof(line), *lvd);
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
	for (int i = 0; i < 7; i++)
		FIELD_ADD(t->header, cols[i], ALIGN_LEFT);

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
		FIELD_ADD(free_row, pv_show, ALIGN_LEFT);
		prev_pv = r->pv_name;

		/* VG */
		const char *vg_show = r->vg_name;
		if (i > 0 && !strcmp(r->vg_name, prev_vg) && r->vg_name[0])
			vg_show = "\"";
		FIELD_ADD(free_row, vg_show, ALIGN_LEFT);
		prev_vg = r->vg_name;

		/* LV */
		FIELD_ADD(free_row, r->lv_name, ALIGN_LEFT);

		/* Size */
		FIELD_ADD(free_row, size_buf, ALIGN_RIGHT);

		/* Free */
		{
			if (r->vg_free) {
				human_size(free_buf, sizeof(free_buf), r->vg_free);
				FIELD_ADD(free_row, free_buf, ALIGN_RIGHT);
			} else {
				FIELD_ADD(free_row, "-", ALIGN_RIGHT);
			}
		}

		/* Mount */
		FIELD_ADD(free_row, r->mount[0] ? r->mount : "-", ALIGN_LEFT);

		/* Bar column */
		{
			field_t f = { .str_h = s_dup(""), .is_bar = 1,
				      .frac = r->vg_size ? (double)(r->vg_size - r->vg_free) / (double)r->vg_size : 0.0 };
			m_put(free_row, &f);
		}

		m_put(t->rows, &free_row);
	}

	entry_t e = { .type_h = s_dup("table"), .data_h = th };
	m_put(sec->entries, &e);

	return sec_h;
}
