#include "out.h"
#include "cfg.h"
#include "m_tool.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

static void free_field_list(int list_h)
{
	field_t *f;
	int i;
	m_foreach(list_h, i, f) {
		m_free(f->str_h);
		m_free(f->unit_h);
	}
	m_free(list_h);
}

static void free_data(const data_t *d)
{
	m_free(d->title_h);
	m_free(d->footer_h);
	int i;
	switch (d->type) {
	case DT_TABLE:
		free_field_list(d->header);
		{
			int *rh;
			m_foreach(d->rows, i, rh) free_field_list(*rh);
		}
		m_free(d->rows);
		break;
	case DT_LIST:
		free_field_list(d->items);
		break;
	case DT_TEXT:
		m_free(d->text_h);
		break;
	case DT_BAR: {
		field_t *f = (field_t *)m_buf(d->bar_h);
		m_free(f->str_h);
		m_free(f->unit_h);
		m_free(d->bar_h);
		break;
	}
	}
}

void out_bar(const data_t *d, void *cfg)
{
	printf("  ");
	out_field((const field_t *)m_buf(d->bar_h), cfg);
	printf("\n");
}

void out_data(const data_t *d, void *cfg)
{
	switch (d->type) {
	case DT_TABLE: out_table(d, cfg); break;
	case DT_LIST:  out_list(d, cfg); break;
	case DT_TEXT:  out_text(d, cfg); break;
	case DT_BAR:   out_bar(d, cfg); break;
	}
}

void out_render(int sections_h, void *cfg)
{
	int i;
	int *sec_h;
	m_foreach(sections_h, i, sec_h) {
		section_t *s = (section_t *)m_buf(*sec_h);
		out_section(s, cfg);
	}
}

void free_sections(int sections_h)
{
	int si, *sec_h;
	m_foreach(sections_h, si, sec_h) {
		section_t *s = (section_t *)m_buf(*sec_h);
		m_free(s->title);
		int ei, *dh;
		m_foreach(s->entries, ei, dh) {
			free_data((const data_t *)m_buf(*dh));
			m_free(*dh);
		}
		m_free(s->entries);
		m_free(*sec_h);
	}
	m_free(sections_h);
}

void out_section(section_t *s, void *cfg)
{
	if (!s) return;
	if (s->title)
		printf("\n  ---- \033[1m%s\033[0m ----\n", m_str(s->title));

	int i;
	int *dh;
	m_foreach(s->entries, i, dh) {
		out_data((const data_t *)m_buf(*dh), cfg);
	}
}

static const char *human_suffix(double *v)
{
	static const char *suf[] = {"B", "K", "M", "G", "T", "P"};
	int i = 0;
	while (*v >= 1024.0 && i < 5) { *v /= 1024.0; i++; }
	return suf[i];
}

static int render_field_value(const field_t *f)
{
	const char *str = f->str_h ? m_str(f->str_h) : "";
	double val = 0;
	switch (f->fmt) {
	case FMT_INT:
	case FMT_HEX: {
		long long v = 0;
		sscanf(str, "%lld", &v);
		double dv = (double)v;
		const char *unit = f->unit_h ? m_str(f->unit_h) : "";
		if (f->human) {
			const char *suf = human_suffix(&dv);
			return s_printf(0, 0, "%.1f%s%s", dv, suf, unit);
		} else if (f->fmt == FMT_HEX) {
			return s_printf(0, 0, "%llx", v);
		} else {
			return s_printf(0, 0, "%s%s", str, unit);
		}
	}
	case FMT_FLOAT:
		val = strtod(str, NULL);
		if (f->human) {
			const char *suf = human_suffix(&val);
			return s_printf(0, 0, "%.1f%s%s", val, suf, f->unit_h ? m_str(f->unit_h) : "");
		} else {
			return s_printf(0, 0, "%.*f%s", f->prec ? f->prec : 1, val,
					f->unit_h ? m_str(f->unit_h) : "");
		}
	default:
		return s_printf(0, 0, "%s", str);
	}
}

void out_field(const field_t *f, void *cfg)
{
	(void)cfg;
	int vh = render_field_value(f);
	const char *vbuf = m_str(vh);
	int w = f->len ? f->len : (int)strlen(vbuf);

	switch (f->align) {
	case ALIGN_RIGHT:  printf("%*s", w, vbuf); break;
	case ALIGN_CENTER: {
		int pad = (w - (int)strlen(vbuf)) / 2;
		printf("%*s%s%*s", pad, "", vbuf, w - pad - (int)strlen(vbuf), "");
		break;
	}
	default: printf("%-*s", w, vbuf); break;
	}

	if (f->is_bar) {
		cfg_t c = cfg ? *(cfg_t *)cfg : 0;
		int bw = cfg_int(c, "bar", "width", 20);
		const char *full = cfg_str(c, "bar", "full", "▓");
		const char *empty = cfg_str(c, "bar", "empty", "░");
		int filled = (int)round(f->frac * bw);
		if (filled < 0) filled = 0;
		if (filled > bw) filled = bw;
		printf(" ");
		for (int i = 0; i < filled; i++) fputs(full, stdout);
		for (int i = filled; i < bw; i++) fputs(empty, stdout);
	}
	m_free(vh);
}

void out_table(const data_t *d, void *cfg)
{
	if (!d) return;

	if (d->title_h)
		printf("  %s\n", m_str(d->title_h));

	int ncols = (int)m_len(d->header);
	if (!ncols) return;

	int *colw = (int *)calloc((size_t)ncols, sizeof(int));
	field_t *h0 = (field_t *)mls(d->header, 0);
	field_t *h1 = (field_t *)mls(d->header, 1);
	int kv_header = (ncols == 2 && h0 && h1 && h0->str_h && h1->str_h
			&& s_strcmp_c(h0->str_h, "Key") == 0
			&& s_strcmp_c(h1->str_h, "Value") == 0);
	int c;
	field_t *hc;
	m_foreach(d->header, c, hc) {
		int vw = hc->len ? hc->len : (hc->str_h ? (int)s_strlen(hc->str_h) : 0);
		colw[c] = vw;
	}

	int r;
	int *rh;
	m_foreach(d->rows, r, rh) {
		int row_h = *rh;
		if (!row_h) continue;
		int c2;
		field_t *fc;
		m_foreach(row_h, c2, fc) {
			if (c2 >= ncols) break;
			int vh = render_field_value(fc);
			int vw = (int)strlen(m_str(vh));
			m_free(vh);
			if (vw > colw[c2]) colw[c2] = vw;
		}
	}

	cfg_t cc = cfg ? *(cfg_t *)cfg : 0;
	int maxw = cfg_int(cc, "table", "max_col_width", 24);
	for (int i = 0; i < ncols; i++)
		if (colw[i] > maxw) colw[i] = maxw;

	if (!kv_header) {
	printf("  ");
	m_foreach(d->header, c, hc) {
		if (c) printf("  ");
		field_t fh = *hc;
		int orig = fh.len;
		fh.len = colw[c];
		fh.align = ALIGN_LEFT;
		out_field(&fh, cfg);
		fh.len = orig;
	}
	printf("\n");

	printf("  ");
	for (int i = 0; i < ncols; i++) {
		if (i) printf("  ");
		for (int j = 0; j < colw[i]; j++) putchar('-');
	}
	printf("\n");
	}

	m_foreach(d->rows, r, rh) {
		int row_h = *rh;
		if (!row_h) continue;
		printf("  ");
		int c2;
		field_t *fc;
		m_foreach(row_h, c2, fc) {
			if (c2 >= ncols) break;
			if (c2) printf("  ");
			field_t fr = *fc;
			int orig = fr.len;
			fr.len = colw[c2];
			out_field(&fr, cfg);
			fr.len = orig;
		}
		printf("\n");
	}

	free(colw);
}

void out_list(const data_t *d, void *cfg)
{
	if (!d) return;

	if (d->title_h)
		printf("  %s\n", m_str(d->title_h));

	int i;
	field_t *f;
	m_foreach(d->items, i, f) {
		printf("  ");
		out_field(f, cfg);
		printf("\n");
	}
}

void out_text(const data_t *d, void *cfg)
{
	(void)cfg;
	if (!d) return;

	if (d->text_h) {
		const char *txt = m_str(d->text_h);
		printf("  %s\n", txt);
	}
	if (d->footer_h)
		printf("  %s\n", m_str(d->footer_h));
}
