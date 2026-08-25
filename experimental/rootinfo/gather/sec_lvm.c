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

static void free_toks(int toks, int keep0, int keep1, int keep2)
{
	for (int j = 0; j < (int)m_len(toks); j++) {
		if (j != keep0 && j != keep1 && j != keep2) m_free(INT(toks, j));
		INT(toks, j) = 0;
	}
}

static int lvm_vg_index(int vg_names, int vg_name)
{
	int p, *vn;
	m_foreach(vg_names, p, vn) {
		if (s_cmp(vg_name, *vn) == 0)
			return p;
	}
	return -1;
}

static int lvm_pv_name(int pv_vgs, int pv_names, int vg_name)
{
	int p, *pv;
	m_foreach(pv_vgs, p, pv) {
		if (s_cmp(vg_name, *pv) == 0)
			return s_mdup(INT(pv_names, p));
	}
	return 0;
}

/* find mountpoint for an lv device path; falls back to the dm-crypt
   /dev/mapper/<vg>-<lv> variant */
static int lvm_find_mount(int lv_path, int vg_name, int lv_name)
{
	int dm_h = s_printf(0, 0, "/dev/mapper/%.100s-%.100s",
			    m_str(vg_name), m_str(lv_name));
	int mounts = m_str_from_file("/proc/mounts");
	int ret = 0;
	if (mounts >= 0) {
		int mlines = s_msplit(0, mounts, s_cstr("\n"));
		m_free(mounts);
		int toks2 = m_alloc(8, sizeof(int), MFREE_EACH);
		int mp, *md;
		m_foreach(mlines, mp, md) {
			s_msplit(toks2, *md, s_cstr(" "));
			int dev = 0, mnt = 0, k = 0;
			for (int j = 0; j < (int)m_len(toks2); j++) {
				int h = INT(toks2, j);
				if (mstr_empty(h)) { m_free(h); INT(toks2, j) = 0; continue; }
				if (k == 0) { dev = h; INT(toks2, j) = 0; }
				else if (k == 1) { mnt = h; INT(toks2, j) = 0; }
				else { m_free(h); INT(toks2, j) = 0; }
				k++;
			}
			if (!mnt) { m_free(dev); continue; }
			if (s_cmp(lv_path, dev) == 0 || s_cmp(dm_h, dev) == 0) {
				ret = mnt;
				m_free(dev);
				break;
			}
			m_free(dev);
			m_free(mnt);
		}
		m_free(toks2);
		m_free(mlines);
	}
	m_free(dm_h);
	return ret;
}

static void lvm_parse_vgs(int vgs_out, int vg_names, int vg_sizes, int vg_frees)
{
	int vg_lines = s_msplit(0, vgs_out, s_cstr("\n"));
	m_free(vgs_out);
	int toks = m_alloc(10, sizeof(int), MFREE_EACH);
	int p, *d;
	m_foreach(vg_lines, p, d) {
		s_msplit_trim(toks, *d, s_cstr("|"), 1);
		int keep = m_len(toks) >= 3;
		if (keep) {
			int nh = INT(toks, 0);
			m_put(vg_names, &nh);
			unsigned long long sz = strtoull(m_str(INT(toks, 1)), NULL, 10);
			m_put(vg_sizes, &sz);
			unsigned long long fr = strtoull(m_str(INT(toks, 2)), NULL, 10);
			m_put(vg_frees, &fr);
		}
		free_toks(toks, keep ? 0 : -1, -1, -1);
	}
	m_free(toks);
	m_free(vg_lines);
}

static void lvm_parse_pvs(int pvs_out, int pv_names, int pv_vgs)
{
	int pv_lines = s_msplit(0, pvs_out, s_cstr("\n"));
	m_free(pvs_out);
	int toks = m_alloc(10, sizeof(int), MFREE_EACH);
	int p, *d;
	m_foreach(pv_lines, p, d) {
		s_msplit_trim(toks, *d, s_cstr("|"), 1);
		int keep0 = m_len(toks) >= 1, keep1 = m_len(toks) >= 2;
		int nh = keep0 ? INT(toks, 0) : s_dup("");
		m_put(pv_names, &nh);
		int vh = keep1 ? INT(toks, 1) : s_dup("");
		m_put(pv_vgs, &vh);
		free_toks(toks, keep0 ? 0 : -1, keep1 ? 1 : -1, -1);
	}
	m_free(toks);
	m_free(pv_lines);
}

static void lvm_render(data_t *t, int rows)
{
	int prev_pv = 0;
	int prev_vg = 0;

	int ri;
	lv_row_t *r;
	m_foreach(rows, ri, r) {
		int free_row = m_create(7, sizeof(field_t));

		/* PV */
		if (ri > 0 && r->pv_name && prev_pv && s_cmp(r->pv_name, prev_pv) == 0) {
			FIELD_ADD(free_row, "\"");
			m_free(r->pv_name);
			r->pv_name = 0;
		} else {
			prev_pv = r->pv_name;
			if (r->pv_name) {
				FIELD_ADD_H(free_row, r->pv_name);
				r->pv_name = 0;
			} else
				FIELD_ADD(free_row, "");
		}

		/* VG */
		if (ri > 0 && r->vg_name && prev_vg && s_cmp(r->vg_name, prev_vg) == 0) {
			FIELD_ADD(free_row, "\"");
			m_free(r->vg_name);
			r->vg_name = 0;
		} else {
			prev_vg = r->vg_name;
			if (r->vg_name) {
				FIELD_ADD_H(free_row, r->vg_name);
				r->vg_name = 0;
			} else
				FIELD_ADD(free_row, "");
		}

		/* LV */
		if (r->lv_name) {
			FIELD_ADD_H(free_row, r->lv_name);
			r->lv_name = 0;
		} else
			FIELD_ADD(free_row, "");

		/* Size */
		FIELD_ADD_H_R(free_row, human_size(r->lv_size));

		/* Free */
		if (r->vg_free)
			FIELD_ADD_H_R(free_row, human_size(r->vg_free));
		else
			FIELD_ADD_R(free_row, "-");

		/* Mount */
		if (r->mount) {
			FIELD_ADD_H(free_row, r->mount);
			r->mount = 0;
		} else
			FIELD_ADD(free_row, "-");

		/* Bar column */
		{
			field_t f = { .str_h = s_dup(""), .is_bar = 1,
				      .frac = r->vg_size ? (double)(r->vg_size - r->vg_free) / (double)r->vg_size : 0.0 };
			m_put(free_row, &f);
		}

		m_put(t->rows, &free_row);
	}
}

static void lvm_free_index(int vg_names, int vg_sizes, int vg_frees,
			   int pv_names, int pv_vgs)
{
	int p, *h;
	m_foreach(vg_names, p, h) m_free(*h);
	m_foreach(pv_names, p, h) m_free(*h);
	m_foreach(pv_vgs, p, h) m_free(*h);
	m_free(vg_names);
	m_free(vg_sizes);
	m_free(vg_frees);
	m_free(pv_names);
	m_free(pv_vgs);
}

int gather_lvm(cfg_t cfg)
{
	(void)cfg;

	int lvs_out = subproc_read("lvs --noheadings --units B --separator '|' -o lv_name,vg_name,lv_size,lv_path 2>/dev/null");
	int vgs_out = subproc_read("vgs --noheadings --units B --separator '|' -o vg_name,vg_size,vg_free 2>/dev/null");
	int pvs_out = subproc_read("pvs --noheadings  --separator '|' -o pv_name,vg_name 2>/dev/null");

	if (STRTAB_EMPTY(lvs_out)) { m_free(lvs_out); m_free(vgs_out); m_free(pvs_out); return 0; }

	int lv_lines = s_msplit(0, lvs_out, s_cstr("\n"));
	m_free(lvs_out);

	int n_lv = (int)m_len(lv_lines);
	if (!n_lv) { m_free(lv_lines); m_free(vgs_out); m_free(pvs_out); return 0; }

	int vg_names = m_create(8, sizeof(int));
	int vg_sizes = m_create(8, sizeof(unsigned long long));
	int vg_frees = m_create(8, sizeof(unsigned long long));
	if (vgs_out) lvm_parse_vgs(vgs_out, vg_names, vg_sizes, vg_frees);

	int pv_names = m_create(8, sizeof(int));
	int pv_vgs = m_create(8, sizeof(int));
	if (pvs_out) lvm_parse_pvs(pvs_out, pv_names, pv_vgs);

	int rows = m_create(16, sizeof(lv_row_t));
	int toks = m_alloc(10, sizeof(int), MFREE_EACH);
	int p, *d;
	m_foreach(lv_lines, p, d) {
		s_msplit_trim(toks, *d, s_cstr("|"), 1);
		if (m_len(toks) < 4) { free_toks(toks, -1, -1, -1); continue; }

		lv_row_t r = {0};
		r.lv_name = INT(toks, 0);
		r.vg_name = INT(toks, 1);
		r.lv_size = strtoull(m_str(INT(toks, 2)), NULL, 10);
		r.lv_path = INT(toks, 3);
		free_toks(toks, 0, 1, 3);

		int vp = lvm_vg_index(vg_names, r.vg_name);
		if (vp >= 0) {
			r.vg_size = U64_SAFE(vg_sizes, vp);
			r.vg_free = U64_SAFE(vg_frees, vp);
		}
		r.pv_name = lvm_pv_name(pv_vgs, pv_names, r.vg_name);
		if (r.lv_path) r.mount = lvm_find_mount(r.lv_path, r.vg_name, r.lv_name);
		m_free(r.lv_path);

		m_put(rows, &r);
	}
	m_free(toks);
	m_free(lv_lines);

	int n_rows = (int)m_len(rows);
	if (!n_rows) {
		m_free(rows);
		lvm_free_index(vg_names, vg_sizes, vg_frees, pv_names, pv_vgs);
		return 0;
	}

	int sec_h = section_new("LVM", 2);
	section_t *sec = (section_t *)m_buf(sec_h);

	int th = table_new(7, (const char *[]){"PV", "VG", "LV", "Size", "Free", "Mount", ""});
	data_t *t = (data_t *)m_buf(th);

	lvm_render(t, rows);

	m_free(rows);
	lvm_free_index(vg_names, vg_sizes, vg_frees, pv_names, pv_vgs);

	add_entry(sec->entries, th);

	return sec_h;
}
