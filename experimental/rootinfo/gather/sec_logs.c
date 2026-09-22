#include "gather.h"
#include "m_subproc.h"
#include "m_tool.h"
#include "m_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef struct {
	double mtime;
	unsigned long long size;
	int path;
} logfile_t;

/* parse one "mtime|size|path" find line; e->path is a fresh handle */
static int parse_line (int line, logfile_t *e)
{
	int p1 = s_chr (line, '|', 0);
	if (p1 < 0)
		return 0;
	int p2 = s_chr (line, '|', p1 + 1);
	if (p2 < 0)
		return 0;
	int ts = s_slice (0, 0, line, 0, p1 - 1);
	int ss = s_slice (0, 0, line, p1 + 1, p2 - 1);
	e->mtime = strtod (m_str (ts), NULL);
	e->size = strtoull (m_str (ss), NULL, 10);
	e->path = s_slice (0, 0, line, p2 + 1, -1);
	m_free (ts);
	m_free (ss);
	return 1;
}

static int fmt_date (double mtime)
{
	time_t t = (time_t)mtime;
	struct tm tm;
	char buf[32];
	localtime_r (&t, &tm);
	strftime (buf, sizeof (buf), "%Y-%m-%d %H:%M", &tm);
	return s_dup (buf);
}

/* kind, size, date, path */
static void add_row (data_t *t, const char *kind, unsigned long long size,
		     double mtime, int path)
{
	int row = m_create (4, sizeof (field_t));
	FIELD_ADD (row, kind);
	FIELD_ADD_H_R (row, human_size (size));
	FIELD_ADD_H (row, fmt_date (mtime));
	FIELD_ADD_H (row, path);
	m_put (t->rows, &row);
}

int gather_logs (cfg_t cfg)
{
	(void)cfg;

	int out = subproc_read (
		"find /var/log -type f -printf '%T@|%s|%p\\n' 2>/dev/null || true");
	if (STRTAB_EMPTY (out)) {
		m_free (out);
		return 0;
	}

	logfile_t oldest = {0};
	int have_oldest = 0;
	logfile_t big[2] = {{0}};

	int lines = s_msplit (0, out, s_cstr ("\n"));
	m_free (out);
	int p, *d;
	m_foreach (lines, p, d)
	{
		if (s_isempty (*d))
			continue;
		logfile_t e;
		if (!parse_line (*d, &e))
			continue;

		if (!have_oldest || e.mtime < oldest.mtime) {
			if (have_oldest)
				m_free (oldest.path);
			oldest.mtime = e.mtime;
			oldest.size = e.size;
			oldest.path = s_mdup (e.path);
			have_oldest = 1;
		}

		/* keep the two largest, replacing the smaller slot */
		int slot = -1;
		if (!big[0].path)
			slot = 0;
		else if (!big[1].path)
			slot = 1;
		else if (e.size > big[0].size && e.size > big[1].size)
			slot = big[0].size <= big[1].size ? 0 : 1;
		else if (e.size > big[0].size)
			slot = 0;
		else if (e.size > big[1].size)
			slot = 1;
		if (slot >= 0) {
			if (big[slot].path)
				m_free (big[slot].path);
			big[slot].mtime = e.mtime;
			big[slot].size = e.size;
			big[slot].path = s_mdup (e.path);
		}

		m_free (e.path);
	}
	m_free (lines);

	if (!have_oldest && !big[0].path && !big[1].path)
		return 0;

	int sec_h = section_new ("LOGS", 2);
	section_t *sec = (section_t *)m_buf (sec_h);

	int th = table_new (4, (const char *[]){"", "Size", "Date", "Path"});
	data_t *t = (data_t *)m_buf (th);

	/* add_row takes ownership of the path handle */
	if (have_oldest) {
		add_row (t, "oldest", oldest.size, oldest.mtime, oldest.path);
		oldest.path = 0;
	}
	for (int i = 0; i < 2; i++) {
		if (big[i].path) {
			add_row (t, "largest", big[i].size, big[i].mtime,
				 big[i].path);
			big[i].path = 0;
		}
	}

	add_entry (sec->entries, th);

	return sec_h;
}
