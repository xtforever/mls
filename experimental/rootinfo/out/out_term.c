#include "out.h"
#include "cfg.h"
#include "m_tool.h"
#include <stdio.h>
#include <string.h>
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

static void free_table(int data_h)
{
	table_t *t = (table_t *)m_buf(data_h);
	m_free(t->title_h);
	free_field_list(t->header);
	int *row_h;
	int i;
	m_foreach(t->rows, i, row_h) {
		free_field_list(*row_h);
	}
	m_free(t->rows);
	m_free(data_h);
}

static void free_list(int data_h)
{
	list_t *l = (list_t *)m_buf(data_h);
	m_free(l->title_h);
	free_field_list(l->items);
	m_free(data_h);
}

static void free_text(int data_h)
{
	text_t *t = (text_t *)m_buf(data_h);
	m_free(t->text_h);
	m_free(t->footer_h);
	m_free(data_h);
}

void out_bar(int data_h, void *cfg)
{
	field_t *f = (field_t *)m_buf(data_h);
	f->is_bar = 1;
	printf("  ");
	out_field(f, cfg);
	printf("\n");
}

static void free_bar(int data_h)
{
	field_t *f = (field_t *)m_buf(data_h);
	m_free(f->str_h);
	m_free(data_h);
}

void out_init(void *cfg)
{
	(void)cfg;
	datatype_t t;
	t = (datatype_t){ .name = "table", .render = out_table, .free = free_table };
	dt_register(&t);
	t = (datatype_t){ .name = "list",  .render = out_list,  .free = free_list };
	dt_register(&t);
	t = (datatype_t){ .name = "text",  .render = out_text,  .free = free_text };
	dt_register(&t);
	t = (datatype_t){ .name = "bar",   .render = out_bar,   .free = free_bar };
	dt_register(&t);
}

int out_render(int sections_h, void *cfg)
{
	int i;
	int *sec_h;
	m_foreach(sections_h, i, sec_h) {
		section_t *s = (section_t *)m_buf(*sec_h);
		out_section(s, cfg);
	}
	return 0;
}

void free_sections(int sections_h)
{
	int si, *sec_h;
	m_foreach(sections_h, si, sec_h) {
		section_t *s = (section_t *)m_buf(*sec_h);
		m_free(s->title);
		m_free(s->footer);
		int ei;
		entry_t *e;
		m_foreach(s->entries, ei, e) {
			m_free(e->title);
			m_free(e->footer);
			dt_free(e);
			m_free(e->type_h);
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
	entry_t *e;
	m_foreach(s->entries, i, e) {
		dt_render(e, cfg);
	}

	if (s->footer)
		printf("  %s\n", m_str(s->footer));
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

void out_table(int data_h, void *cfg)
{
	table_t *t = (table_t *)m_buf(data_h);
	if (!t) return;

	if (t->title_h)
		printf("  %s\n", m_str(t->title_h));

	int ncols = (int)m_len(t->header);
	if (!ncols) return;

	int *colw = (int *)calloc((size_t)ncols, sizeof(int));
	field_t *h0 = (field_t *)mls(t->header, 0);
	field_t *h1 = (field_t *)mls(t->header, 1);
	int kv_header = (ncols == 2 && h0 && h1 && h0->str_h && h1->str_h
			&& s_strcmp_c(h0->str_h, "Key") == 0
			&& s_strcmp_c(h1->str_h, "Value") == 0);
	int c;
	field_t *hc;
	m_foreach(t->header, c, hc) {
		int vw = hc->len ? hc->len : (hc->str_h ? (int)s_strlen(hc->str_h) : 0);
		colw[c] = vw;
	}

	int r;
	int *rh;
	m_foreach(t->rows, r, rh) {
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
	m_foreach(t->header, c, hc) {
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

	m_foreach(t->rows, r, rh) {
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

void out_list(int data_h, void *cfg)
{
	list_t *l = (list_t *)m_buf(data_h);
	if (!l) return;

	if (l->title_h)
		printf("  %s\n", m_str(l->title_h));

	int i;
	field_t *f;
	m_foreach(l->items, i, f) {
		printf("  ");
		out_field(f, cfg);
		printf("\n");
	}
}

void out_text(int data_h, void *cfg)
{
	(void)cfg;
	text_t *t = (text_t *)m_buf(data_h);
	if (!t) return;

	if (t->text_h) {
		const char *txt = m_str(t->text_h);
		printf("  %s\n", txt);
	}
	if (t->footer_h)
		printf("  %s\n", m_str(t->footer_h));
}
