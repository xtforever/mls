#include "gather.h"
#include "m_subproc.h"
#include "m_tool.h"
#include "m_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/statvfs.h>

typedef struct {
	int pv_name;
	int lv_name;
	int vg_name;
	unsigned long long lv_size;
	unsigned long long vg_free;
	unsigned long long vg_size;
	int mount;
	int lv_path;
	int has_fs;		    /* mount has a usable statvfs */
	double fs_frac;		    /* filesystem used fraction */
	unsigned long long fs_free; /* filesystem free bytes */
} lv_row_t;

static void free_toks (int toks, int keep0, int keep1, int keep2)
{
	for (int j = 0; j < (int)m_len (toks); j++) {
		if (j != keep0 && j != keep1 && j != keep2)
			m_free (INT (toks, j));
		INT (toks, j) = 0;
	}
}

/* LVM may prefix an approximate size with '<' or '>'; skip it so
   strtoull sees the digits. */
static unsigned long long parse_bytes (int h)
{
	const char *s = h ? m_str (h) : "";
	while (*s == '<' || *s == '>' || *s == ' ')
		s++;
	return strtoull (s, NULL, 10);
}

static int lvm_pv_name (int pv_vgs, int pv_names, int vg_name)
{
	int p, *pv;
	m_foreach (pv_vgs, p, pv)
	{
		if (s_cmp (vg_name, *pv) == 0)
			return s_mdup (INT (pv_names, p));
	}
	return 0;
}

/* /dev/mapper names escape '-' as '--' in both VG and LV name, e.g.
   vg "admin0-vg" + lv "root" -> /dev/mapper/admin0--vg-root */
static int lvm_dm_path (int vg_name, int lv_name)
{
	int vg = s_replace_c (vg_name, "-", "--");
	int lv = s_replace_c (lv_name, "-", "--");
	int out = s_printf (0, 0, "/dev/mapper/%s-%s", m_str (vg), m_str (lv));
	m_free (vg);
	m_free (lv);
	return out;
}

/* find mountpoint for an lv device path; also tries the
   /dev/mapper/<vg>-<lv> variant with proper dm name escaping */
static int lvm_find_mount (int lv_path, int vg_name, int lv_name)
{
	int dm_h = lvm_dm_path (vg_name, lv_name);
	int mounts = m_str_from_file ("/proc/mounts");
	int ret = 0;
	if (mounts >= 0) {
		int mlines = s_msplit (0, mounts, s_cstr ("\n"));
		m_free (mounts);
		int toks2 = m_alloc (8, sizeof (int), MFREE_EACH);
		int mp, *md;
		m_foreach (mlines, mp, md)
		{
			s_msplit (toks2, *md, s_cstr (" "));
			int dev = 0, mnt = 0, k = 0;
			for (int j = 0; j < (int)m_len (toks2); j++) {
				int h = INT (toks2, j);
				if (mstr_empty (h)) {
					m_free (h);
					INT (toks2, j) = 0;
					continue;
				}
				if (k == 0) {
					dev = h;
					INT (toks2, j) = 0;
				} else if (k == 1) {
					mnt = h;
					INT (toks2, j) = 0;
				} else {
					m_free (h);
					INT (toks2, j) = 0;
				}
				k++;
			}
			if (!mnt) {
				m_free (dev);
				continue;
			}
			if (s_cmp (lv_path, dev) == 0 ||
			    s_cmp (dm_h, dev) == 0) {
				ret = mnt;
				m_free (dev);
				break;
			}
			m_free (dev);
			m_free (mnt);
		}
		m_free (toks2);
		m_free (mlines);
	}
	m_free (dm_h);
	return ret;
}

static void lvm_parse_pvs (int pvs_out, int pv_names, int pv_vgs)
{
	int pv_lines = s_msplit (0, pvs_out, s_cstr ("\n"));
	m_free (pvs_out);
	int toks = m_alloc (10, sizeof (int), MFREE_EACH);
	int p, *d;
	m_foreach (pv_lines, p, d)
	{
		s_msplit_trim (toks, *d, s_cstr ("|"), 1);
		int keep0 = m_len (toks) >= 1, keep1 = m_len (toks) >= 2;
		int nh = keep0 ? INT (toks, 0) : s_dup ("");
		m_put (pv_names, &nh);
		int vh = keep1 ? INT (toks, 1) : s_dup ("");
		m_put (pv_vgs, &vh);
		free_toks (toks, keep0 ? 0 : -1, keep1 ? 1 : -1, -1);
	}
	m_free (toks);
	m_free (pv_lines);
}

static void lvm_render (data_t *t, int rows)
{
	int prev_pv = 0;
	int prev_vg = 0;

	int ri;
	lv_row_t *r;
	m_foreach (rows, ri, r)
	{
		int free_row = m_create (7, sizeof (field_t));

		/* PV */
		if (ri > 0 && r->pv_name && prev_pv &&
		    s_cmp (r->pv_name, prev_pv) == 0) {
			FIELD_ADD (free_row, "\"");
			m_free (r->pv_name);
			r->pv_name = 0;
		} else {
			prev_pv = r->pv_name;
			if (r->pv_name) {
				FIELD_ADD_H (free_row, r->pv_name);
				r->pv_name = 0;
			} else
				FIELD_ADD (free_row, "");
		}

		/* VG */
		if (ri > 0 && r->vg_name && prev_vg &&
		    s_cmp (r->vg_name, prev_vg) == 0) {
			FIELD_ADD (free_row, "\"");
			m_free (r->vg_name);
			r->vg_name = 0;
		} else {
			prev_vg = r->vg_name;
			if (r->vg_name) {
				FIELD_ADD_H (free_row, r->vg_name);
				r->vg_name = 0;
			} else
				FIELD_ADD (free_row, "");
		}

		/* LV */
		if (r->lv_name) {
			FIELD_ADD_H (free_row, r->lv_name);
			r->lv_name = 0;
		} else
			FIELD_ADD (free_row, "");

		/* Size */
		FIELD_ADD_H_R (free_row, human_size (r->lv_size));

		/* Free: filesystem free when mounted, else VG free */
		FIELD_ADD_H_R (free_row, human_size (r->has_fs ? r->fs_free
							       : r->vg_free));

		/* Mount */
		if (r->mount) {
			FIELD_ADD_H (free_row, r->mount);
			r->mount = 0;
		} else
			FIELD_ADD (free_row, "-");

		/* Bar column: filesystem usage when mounted, else VG usage */
		{
			double frac;
			if (r->has_fs)
				frac = r->fs_frac;
			else
				frac = r->vg_size ? (double)(r->vg_size -
							     r->vg_free) /
							    (double)r->vg_size
						  : 0.0;
			field_t f = {
				.str_h = s_dup (""), .is_bar = 1, .frac = frac};
			m_put (free_row, &f);
		}

		m_put (t->rows, &free_row);
	}
}

static void lvm_free_index (int pv_names, int pv_vgs)
{
	int p, *h;
	m_foreach (pv_names, p, h) m_free (*h);
	m_foreach (pv_vgs, p, h) m_free (*h);
	m_free (pv_names);
	m_free (pv_vgs);
}

int gather_lvm (cfg_t cfg)
{
	(void)cfg;

	int lvs_out = subproc_read (
		"lvs --noheadings --units B --separator '|' -o "
		"lv_name,vg_name,lv_size,lv_path,vg_size,vg_free 2>/dev/null");
	int pvs_out = subproc_read ("pvs --noheadings  --separator '|' -o "
				    "pv_name,vg_name 2>/dev/null");

	if (STRTAB_EMPTY (lvs_out)) {
		m_free (lvs_out);
		m_free (pvs_out);
		return 0;
	}

	int lv_lines = s_msplit (0, lvs_out, s_cstr ("\n"));
	m_free (lvs_out);

	int n_lv = (int)m_len (lv_lines);
	if (!n_lv) {
		m_free (lv_lines);
		m_free (pvs_out);
		return 0;
	}

	int pv_names = m_create (8, sizeof (int));
	int pv_vgs = m_create (8, sizeof (int));
	if (pvs_out)
		lvm_parse_pvs (pvs_out, pv_names, pv_vgs);

	int rows = m_create (16, sizeof (lv_row_t));
	int toks = m_alloc (10, sizeof (int), MFREE_EACH);
	int p, *d;
	m_foreach (lv_lines, p, d)
	{
		s_msplit_trim (toks, *d, s_cstr ("|"), 1);
		if (m_len (toks) < 6) {
			free_toks (toks, -1, -1, -1);
			continue;
		}

		lv_row_t r = {0};
		r.lv_name = INT (toks, 0);
		r.vg_name = INT (toks, 1);
		r.lv_size = parse_bytes (INT (toks, 2));
		r.lv_path = INT (toks, 3);
		r.vg_size = parse_bytes (INT (toks, 4));
		r.vg_free = parse_bytes (INT (toks, 5));
		free_toks (toks, 0, 1, 3);

		r.pv_name = lvm_pv_name (pv_vgs, pv_names, r.vg_name);
		if (r.lv_path)
			r.mount = lvm_find_mount (r.lv_path, r.vg_name,
						  r.lv_name);
		m_free (r.lv_path);

		/* mounted LV: bar reflects filesystem usage, not VG usage */
		if (r.mount) {
			struct statvfs vf;
			if (statvfs (m_str (r.mount), &vf) == 0) {
				double total =
					(double)vf.f_blocks * vf.f_frsize;
				double avail =
					(double)vf.f_bavail * vf.f_frsize;
				if (total > 0) {
					r.fs_frac = (total - avail) / total;
					r.fs_free = (unsigned long long)avail;
					r.has_fs = 1;
				}
			}
		}

		m_put (rows, &r);
	}
	m_free (toks);
	m_free (lv_lines);

	int n_rows = (int)m_len (rows);
	if (!n_rows) {
		m_free (rows);
		lvm_free_index (pv_names, pv_vgs);
		return 0;
	}

	int sec_h = section_new ("LVM", 2);
	section_t *sec = (section_t *)m_buf (sec_h);

	int th = table_new (7, (const char *[]){"PV", "VG", "LV", "Size",
						"Free", "Mount", ""});
	data_t *t = (data_t *)m_buf (th);

	lvm_render (t, rows);

	m_free (rows);
	lvm_free_index (pv_names, pv_vgs);

	add_entry (sec->entries, th);

	return sec_h;
}
