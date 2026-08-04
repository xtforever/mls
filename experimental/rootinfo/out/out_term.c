#include "out.h"
#include "cfg.h"
#include "m_tool.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static void free_table(int data_h)
{
	table_t *t = (table_t *)m_buf(data_h);
	m_free(t->title_h);
	m_free(t->header);
	m_free(t->rows);
	m_free(data_h);
}

static void free_list(int data_h)
{
	list_t *l = (list_t *)m_buf(data_h);
	m_free(l->title_h);
	m_free(l->items);
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
	for (int i = 0; i < (int)m_len(sections_h); i++) {
		int *sec_h = (int *)m_peek(sections_h, (size_t)i);
		if (!sec_h) continue;
		section_t *s = (section_t *)m_buf(*sec_h);
		out_section(s, cfg);
	}
	return 0;
}

void out_section(section_t *s, void *cfg)
{
	if (!s) return;
	if (s->title)
		printf("\n  ---- \033[1m%s\033[0m ----\n", m_str(s->title));

	for (int i = 0; i < (int)m_len(s->entries); i++) {
		entry_t *e = (entry_t *)m_peek(s->entries, (size_t)i);
		if (!e) continue;
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

static void render_field_value(const field_t *f, char *buf, size_t sz)
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
			snprintf(buf, sz, "%.1f%s%s", dv, suf, unit);
		} else if (f->fmt == FMT_HEX) {
			snprintf(buf, sz, "%llx", v);
		} else {
			snprintf(buf, sz, "%s%s", str, unit);
		}
		break;
	}
	case FMT_FLOAT:
		val = strtod(str, NULL);
		if (f->human) {
			const char *suf = human_suffix(&val);
			snprintf(buf, sz, "%.1f%s%s", val, suf, f->unit_h ? m_str(f->unit_h) : "");
		} else {
			snprintf(buf, sz, "%.*f%s", f->prec ? f->prec : 1, val,
				 f->unit_h ? m_str(f->unit_h) : "");
		}
		break;
	default:
		snprintf(buf, sz, "%s", str);
		break;
	}
}

void out_field(const field_t *f, void *cfg)
{
	(void)cfg;
	char vbuf[128];
	render_field_value(f, vbuf, sizeof(vbuf));
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
}

void out_table(int data_h, void *cfg)
{
	table_t *t = (table_t *)m_buf(data_h);
	if (!t) return;

	if (t->title_h)
		printf("  %s\n", m_str(t->title_h));

	int ncols = (int)m_len(t->header);
	if (!ncols) return;

	int nrows = (int)m_len(t->rows);
	int *colw = (int *)calloc((size_t)ncols, sizeof(int));
	field_t *hdr = (field_t *)m_buf(t->header);
	int kv_header = (ncols == 2 && hdr[0].str_h && hdr[1].str_h
			&& !strcmp(m_str(hdr[0].str_h), "Key")
			&& !strcmp(m_str(hdr[1].str_h), "Value"));
	for (int c = 0; c < ncols; c++) {
		int vw = hdr[c].len ? hdr[c].len : (hdr[c].str_h ? (int)strlen(m_str(hdr[c].str_h)) : 0);
		colw[c] = vw;
	}

	for (int r = 0; r < nrows; r++) {
		int *rh = (int *)m_peek(t->rows, (size_t)r);
		int row_h = rh ? *rh : 0;
		if (!row_h) continue;
		field_t *row = (field_t *)m_buf(row_h);
		for (int c = 0; c < ncols && c < (int)m_len(row_h); c++) {
			char vbuf[128];
			render_field_value(&row[c], vbuf, sizeof(vbuf));
			int vw = (int)strlen(vbuf);
			if (vw > colw[c]) colw[c] = vw;
		}
	}

	cfg_t c = cfg ? *(cfg_t *)cfg : 0;
	int maxw = cfg_int(c, "table", "max_col_width", 24);
	for (int i = 0; i < ncols; i++)
		if (colw[i] > maxw) colw[i] = maxw;

	if (!kv_header) {
	printf("  ");
	for (int c = 0; c < ncols; c++) {
		if (c) printf("  ");
		field_t fh = hdr[c];
		int orig = fh.len;
		fh.len = colw[c];
		fh.align = ALIGN_LEFT;
		out_field(&fh, cfg);
		fh.len = orig;
	}
	printf("\n");

	printf("  ");
	for (int c = 0; c < ncols; c++) {
		if (c) printf("  ");
		for (int i = 0; i < colw[c]; i++) putchar('-');
	}
	printf("\n");
	}

	for (int r = 0; r < nrows; r++) {
		int *rh = (int *)m_peek(t->rows, (size_t)r);
		int row_h = rh ? *rh : 0;
		if (!row_h) continue;
		field_t *row = (field_t *)m_buf(row_h);
		printf("  ");
		for (int c = 0; c < ncols && c < (int)m_len(row_h); c++) {
			if (c) printf("  ");
			field_t fr = row[c];
			int orig = fr.len;
			fr.len = colw[c];
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

	for (int i = 0; i < (int)m_len(l->items); i++) {
		field_t *f = (field_t *)m_peek(l->items, (size_t)i);
		if (!f) continue;
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
