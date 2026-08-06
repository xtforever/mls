#include "gather.h"
#include "m_types.h"
#include "m_subproc.h"
#include "m_tool.h"
#include <stdio.h>
#include <stdlib.h>

static int human_size(unsigned long long bytes)
{
	static const char *units[] = {"B", "K", "M", "G", "T", "P"};
	double v = (double)bytes;
	int ui = 0;
	while (v >= 1024.0 && ui < 5) { v /= 1024.0; ui++; }
	if (ui == 0)
		return s_printf(0, 0, "%llu%s", bytes, units[ui]);
	return s_printf(0, 0, "%.1f%s", v, units[ui]);
}

typedef struct {
	int pv_name;
	int lv_name;
	int vg_name;
	unsigned long long lv_size;
	unsigned long long vg_free;
	unsigned long long vg_size;
	int mount;
	int lv_path;
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

	int toks = m_alloc(10, sizeof(char *), MFREE_STR);
	int vg_names = m_create(8, sizeof(int));
	int vg_sizes = m_create(8, sizeof(unsigned long long));
	int vg_frees = m_create(8, sizeof(unsigned long long));
	if (vgs_out) {
		nl_h = s_dup("\n");
		int vg_lines = s_msplit(0, vgs_out, nl_h);
		m_free(vgs_out);
		m_free(nl_h);
		int vgp, *vgd;
		m_foreach(vg_lines, vgp, vgd) {
			s_split(toks, m_buf(*vgd), '|', 1);
			if (m_len(toks) >= 3) {
				int nh = s_dup(STR(toks, 0));
				m_put(vg_names, &nh);
				unsigned long long sz = strtoull(STR(toks, 1), NULL, 10);
				m_put(vg_sizes, &sz);
				unsigned long long fr = strtoull(STR(toks, 2), NULL, 10);
				m_put(vg_frees, &fr);
			}
		}
		m_free(vg_lines);
	}

	int pv_names = m_create(8, sizeof(int));
	int pv_vgs = m_create(8, sizeof(int));
	if (pvs_out) {
		nl_h = s_dup("\n");
		int pv_lines = s_msplit(0, pvs_out, nl_h);
		m_free(pvs_out);
		m_free(nl_h);
		int pvp, *pvd;
		m_foreach(pv_lines, pvp, pvd) {
			s_split(toks, m_buf(*pvd), '|', 1);
			int nh = m_len(toks) >= 1 ? s_dup(STR(toks, 0)) : s_dup("");
			m_put(pv_names, &nh);
			int vh = m_len(toks) >= 2 ? s_dup(STR(toks, 1)) : s_dup("");
			m_put(pv_vgs, &vh);
		}
		m_free(pv_lines);
	}

	int rows = m_create(16, sizeof(lv_row_t));
	int lvp, *lvd;
	m_foreach(lv_lines, lvp, lvd) {
		s_split(toks, m_buf(*lvd), '|', 1);
		if (m_len(toks) < 4) continue;

		lv_row_t r = {0};
		r.lv_name = s_dup(STR(toks, 0));
		r.vg_name = s_dup(STR(toks, 1));
		r.lv_size = strtoull(STR(toks, 2), NULL, 10);
		r.lv_path = s_dup(STR(toks, 3));

		/* lookup VG free/size */
		int vp;
		int *vn;
		m_foreach(vg_names, vp, vn) {
			if (s_cmp(r.vg_name, *vn) == 0) {
				r.vg_size = *(unsigned long long *)mls(vg_sizes, vp);
				r.vg_free = *(unsigned long long *)mls(vg_frees, vp);
				break;
			}
		}

		/* lookup PV name for this VG */
		int pp;
		int *pv;
		m_foreach(pv_vgs, pp, pv) {
			if (s_cmp(r.vg_name, *pv) == 0) {
				m_free(r.pv_name);
				r.pv_name = s_mdup(INT(pv_names, pp));
				break;
			}
		}

		/* find mountpoint from /proc/mounts using device path or dm-crypt variant */
		if (r.lv_path) {
			/* build dm-crypt device name: /dev/mapper/<vg>-<lv> */
			int dm_h = s_printf(0, 0, "/dev/mapper/%.100s-%.100s",
					    m_str(r.vg_name), m_str(r.lv_name));
			int mounts = m_str_from_file("/proc/mounts");
			if (mounts >= 0) {
				nl_h = s_dup("\n");
				int mlines = s_msplit(0, mounts, nl_h);
				m_free(nl_h);
				m_free(mounts);
				int toks2 = m_alloc(8, sizeof(char *), MFREE_STR);
				int mp;
				int *md;
				m_foreach(mlines, mp, md) {
				s_split(toks2, m_buf(*md), ' ', 1);
				if (m_len(toks2) < 2) continue;
				if (s_strcmp_c(r.lv_path, STR(toks2, 0)) == 0 ||
				    s_strcmp_c(dm_h, STR(toks2, 0)) == 0) {
					m_free(r.mount);
					r.mount = s_dup(STR(toks2, 1));
					break;
					}
				}
				m_free(toks2);
				m_free(mlines);
			}
			m_free(dm_h);
		}

		m_put(rows, &r);
	}
	m_free(toks);
	m_free(lv_lines);

	int n_rows = (int)m_len(rows);
	if (!n_rows) { m_free(rows); return 0; }

	int sec_h = section_new("LVM", 2);
	section_t *sec = (section_t *)m_buf(sec_h);

	int th = table_new(7, (const char *[]){"PV", "VG", "LV", "Size", "Free", "Mount", ""});
	data_t *t = (data_t *)m_buf(th);

	int prev_pv = 0;
	int prev_vg = 0;

	int ri;
	lv_row_t *r;
	m_foreach(rows, ri, r) {
		int free_row = m_create(7, sizeof(field_t));

		/* PV */
		const char *pv_show = r->pv_name ? m_str(r->pv_name) : "";
		if (ri > 0 && r->pv_name && prev_pv && s_cmp(r->pv_name, prev_pv) == 0)
			pv_show = "\"";
		FIELD_ADD(free_row, pv_show);
		prev_pv = r->pv_name;

		/* VG */
		const char *vg_show = r->vg_name ? m_str(r->vg_name) : "";
		if (ri > 0 && prev_vg && s_cmp(r->vg_name, prev_vg) == 0)
			vg_show = "\"";
		FIELD_ADD(free_row, vg_show);
		prev_vg = r->vg_name;

		/* LV */
		FIELD_ADD(free_row, r->lv_name ? m_str(r->lv_name) : "");

		/* Size */
		FIELD_ADD_H_R(free_row, human_size(r->lv_size));

		/* Free */
		if (r->vg_free)
			FIELD_ADD_H_R(free_row, human_size(r->vg_free));
		else
			FIELD_ADD_R(free_row, "-");

		/* Mount */
		FIELD_ADD(free_row, r->mount ? m_str(r->mount) : "-");

		/* Bar column */
		{
			field_t f = { .str_h = s_dup(""), .is_bar = 1,
				      .frac = r->vg_size ? (double)(r->vg_size - r->vg_free) / (double)r->vg_size : 0.0 };
			m_put(free_row, &f);
		}

		m_put(t->rows, &free_row);
	}

	/* free per-row string handles */
	int fi;
	lv_row_t *fr;
	m_foreach(rows, fi, fr) {
		m_free(fr->pv_name);
		m_free(fr->lv_name);
		m_free(fr->vg_name);
		m_free(fr->mount);
		m_free(fr->lv_path);
	}
	m_free(rows);

	/* free vg/pv name handles */
	int i;
	int *h;
	m_foreach(vg_names, i, h) m_free(*h);
	m_foreach(pv_names, i, h) m_free(*h);
	m_foreach(pv_vgs, i, h) m_free(*h);
	m_free(vg_names);
	m_free(vg_sizes);
	m_free(vg_frees);
	m_free(pv_names);
	m_free(pv_vgs);

	add_entry(sec->entries, th);

	return sec_h;
}
