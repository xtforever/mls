#include "cfg.h"
#include "m_tool.h"
#include "out.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* zero-width ANSI styling, safe for col-width math */
#define CS_RESET "\033[0m"
#define CS_BOLD "\033[1m"
#define CS_DIM "\033[2m"
#define CS_CYAN "\033[36m"
#define CS_GRN "\033[32m"
#define CS_YEL "\033[33m"
#define CS_RED "\033[31m"

static int color_on (void *cfg)
{
	cfg_t c = cfg ? *(cfg_t *)cfg : 0;
	const char *v = cfg_str (c, "style", "color", "auto");
	if (v && v[0]) {
		if (strcmp (v, "on") == 0 || strcmp (v, "true") == 0 ||
		    strcmp (v, "1") == 0 || strcmp (v, "yes") == 0)
			return 1;
		if (strcmp (v, "off") == 0 || strcmp (v, "false") == 0 ||
		    strcmp (v, "0") == 0 || strcmp (v, "no") == 0)
			return 0;
	}
	return isatty (STDOUT_FILENO);
}

static const char *bar_color (double frac)
{
	if (frac >= 0.85)
		return CS_RED;
	if (frac >= 0.70)
		return CS_YEL;
	return CS_GRN;
}

static void free_field_list (int list_h)
{
	field_t *f;
	int i;
	m_foreach (list_h, i, f)
	{
		m_free (f->str_h);
		m_free (f->unit_h);
	}
	m_free (list_h);
}

static void free_data (const data_t *d)
{
	m_free (d->title_h);
	m_free (d->footer_h);
	int i;
	switch (d->type) {
	case DT_TABLE:
		free_field_list (d->header);
		{
			int *rh;
			m_foreach (d->rows, i, rh) free_field_list (*rh);
		}
		m_free (d->rows);
		break;
	case DT_LIST:
		free_field_list (d->items);
		break;
	case DT_TEXT:
		m_free (d->text_h);
		break;
	case DT_BAR: {
		field_t *f = (field_t *)m_buf (d->bar_h);
		m_free (f->str_h);
		m_free (f->unit_h);
		m_free (d->bar_h);
		break;
	}
	}
}

void out_bar (const data_t *d, void *cfg)
{
	printf ("  ");
	out_field ((const field_t *)m_buf (d->bar_h), cfg);
	printf ("\n");
}

void out_data (const data_t *d, void *cfg)
{
	switch (d->type) {
	case DT_TABLE:
		out_table (d, cfg);
		break;
	case DT_LIST:
		out_list (d, cfg);
		break;
	case DT_TEXT:
		out_text (d, cfg);
		break;
	case DT_BAR:
		out_bar (d, cfg);
		break;
	}
}

void out_render (int sections_h, void *cfg)
{
	int i;
	int *sec_h;
	m_foreach (sections_h, i, sec_h)
	{
		section_t *s = (section_t *)m_buf (*sec_h);
		out_section (s, cfg);
	}
}

void free_sections (int sections_h)
{
	int si, *sec_h;
	m_foreach (sections_h, si, sec_h)
	{
		section_t *s = (section_t *)m_buf (*sec_h);
		m_free (s->title);
		int ei, *dh;
		m_foreach (s->entries, ei, dh)
		{
			free_data ((const data_t *)m_buf (*dh));
			m_free (*dh);
		}
		m_free (s->entries);
		m_free (*sec_h);
	}
	m_free (sections_h);
}

void out_section (section_t *s, void *cfg)
{
	if (!s)
		return;
	if (s->title)
		printf ("\n  %s---- %s ----%s\n",
			color_on (cfg) ? CS_BOLD CS_CYAN : "", m_str (s->title),
			color_on (cfg) ? CS_RESET : "");

	int i;
	int *dh;
	m_foreach (s->entries, i, dh)
	{
		out_data ((const data_t *)m_buf (*dh), cfg);
	}
}

static const char *human_suffix (double *v)
{
	static const char *suf[] = {"B", "K", "M", "G", "T", "P"};
	int i = 0;
	while (*v >= 1024.0 && i < 5) {
		*v /= 1024.0;
		i++;
	}
	return suf[i];
}

static int render_field_value (const field_t *f)
{
	const char *str = f->str_h ? m_str (f->str_h) : "";
	double val = 0;
	switch (f->fmt) {
	case FMT_INT:
	case FMT_HEX: {
		long long v = 0;
		sscanf (str, "%lld", &v);
		double dv = (double)v;
		const char *unit = f->unit_h ? m_str (f->unit_h) : "";
		if (f->human) {
			const char *suf = human_suffix (&dv);
			return s_printf (0, 0, "%.1f%s%s", dv, suf, unit);
		} else if (f->fmt == FMT_HEX) {
			return s_printf (0, 0, "%llx", v);
		} else {
			return s_printf (0, 0, "%s%s", str, unit);
		}
	}
	case FMT_FLOAT:
		val = strtod (str, NULL);
		if (f->human) {
			const char *suf = human_suffix (&val);
			return s_printf (0, 0, "%.1f%s%s", val, suf,
					 f->unit_h ? m_str (f->unit_h) : "");
		} else {
			return s_printf (0, 0, "%.*f%s", f->prec ? f->prec : 1,
					 val,
					 f->unit_h ? m_str (f->unit_h) : "");
		}
	default:
		return s_printf (0, 0, "%s", str);
	}
}

static void pad_print (const char *vbuf, int align, int w)
{
	switch (align) {
	case ALIGN_RIGHT:
		printf ("%*s", w, vbuf);
		break;
	case ALIGN_CENTER: {
		int pad = (w - (int)strlen (vbuf)) / 2;
		printf ("%*s%s%*s", pad, "", vbuf, w - pad - (int)strlen (vbuf),
			"");
		break;
	}
	default:
		printf ("%-*s", w, vbuf);
		break;
	}
}

/* like pad_print but hard-caps content at w bytes; backs off to avoid
   splitting a UTF-8 sequence, still pads to w so columns stay aligned */
static void pad_fit (const char *vbuf, int align, int w)
{
	int len = (int)strlen (vbuf);
	if (len <= w) {
		pad_print (vbuf, align, w);
		return;
	}
	int cut = w;
	while (cut > 0 && ((unsigned char)vbuf[cut] & 0xC0) == 0x80)
		cut--;
	int t = s_printf (0, 0, "%.*s", cut, vbuf);
	pad_print (m_str (t), align, w);
	m_free (t);
}

static void draw_bar (const field_t *f, void *cfg)
{
	cfg_t c = cfg ? *(cfg_t *)cfg : 0;
	int bw = cfg_int (c, "bar", "width", 20);
	const char *full = cfg_str (c, "bar", "full", "▓");
	const char *empty = cfg_str (c, "bar", "empty", "░");
	int filled = (int)round (f->frac * bw);
	if (filled < 0)
		filled = 0;
	if (filled > bw)
		filled = bw;
	printf (" ");
	if (color_on (cfg)) {
		fputs (bar_color (f->frac), stdout);
		for (int i = 0; i < filled; i++)
			fputs (full, stdout);
		fputs (CS_RESET CS_DIM, stdout);
		for (int i = filled; i < bw; i++)
			fputs (empty, stdout);
		fputs (CS_RESET, stdout);
	} else {
		for (int i = 0; i < filled; i++)
			fputs (full, stdout);
		for (int i = filled; i < bw; i++)
			fputs (empty, stdout);
	}
}

void out_field (const field_t *f, void *cfg)
{
	int vh = render_field_value (f);
	const char *vbuf = m_str (vh);
	pad_print (vbuf, f->align, f->len ? f->len : (int)strlen (vbuf));
	if (f->is_bar)
		draw_bar (f, cfg);
	m_free (vh);
}

void out_table (const data_t *d, void *cfg)
{
	if (!d)
		return;

	if (d->title_h)
		printf ("  %s\n", m_str (d->title_h));

	int ncols = (int)m_len (d->header);
	if (!ncols)
		return;

	int *colw = (int *)calloc ((size_t)ncols, sizeof (int));
	field_t *h0 = (field_t *)mls (d->header, 0);
	field_t *h1 = (field_t *)mls (d->header, 1);
	int kv_header = (ncols == 2 && h0 && h1 && h0->str_h && h1->str_h &&
			 s_strcmp_c (h0->str_h, "Key") == 0 &&
			 s_strcmp_c (h1->str_h, "Value") == 0);

	/* ponytail: cells are rendered once into a flat cache shared by the
	   width pass and the print loop; was 2x render+alloc per cell */
	int nrows = (int)m_len (d->rows);
	int *cache = (int *)calloc ((size_t)nrows * ncols, sizeof (int));
	int c;
	field_t *hc;
	m_foreach (d->header, c, hc)
	{
		colw[c] = hc->len ? hc->len
				  : (hc->str_h ? (int)s_strlen (hc->str_h) : 0);
	}

	int r;
	int *rh;
	m_foreach (d->rows, r, rh)
	{
		int row_h = *rh;
		if (!row_h)
			continue;
		int c2;
		field_t *fc;
		m_foreach (row_h, c2, fc)
		{
			if (c2 >= ncols)
				break;
			cache[r * ncols + c2] = render_field_value (fc);
			int vw = (int)strlen (m_str (cache[r * ncols + c2]));
			if (vw > colw[c2])
				colw[c2] = vw;
		}
	}

	cfg_t cc = cfg ? *(cfg_t *)cfg : 0;
	int maxw = cfg_int (cc, "table", "max_col_width", 24);
	for (int i = 0; i < ncols; i++)
		if (colw[i] > maxw)
			colw[i] = maxw;

	int col = color_on (cfg);
	if (!kv_header) {
		if (col)
			fputs (CS_BOLD, stdout);
		printf ("  ");
		m_foreach (d->header, c, hc)
		{
			if (c)
				printf ("  ");
			pad_fit (hc->str_h ? m_str (hc->str_h) : "", ALIGN_LEFT,
				 colw[c]);
		}
		printf ("\n");
		if (col)
			fputs (CS_RESET, stdout);

		printf ("  ");
		for (int i = 0; i < ncols; i++) {
			if (i)
				printf ("  ");
			for (int j = 0; j < colw[i]; j++)
				putchar ('-');
		}
		printf ("\n");
	}

	m_foreach (d->rows, r, rh)
	{
		int row_h = *rh;
		if (!row_h)
			continue;
		printf ("  ");
		int c2;
		field_t *fc;
		m_foreach (row_h, c2, fc)
		{
			if (c2 >= ncols)
				break;
			if (c2)
				printf ("  ");
			if (kv_header && c2 == 0 && col)
				fputs (CS_BOLD, stdout);
			int vh = cache[r * ncols + c2];
			pad_fit (m_str (vh), fc->align, colw[c2]);
			if (fc->is_bar)
				draw_bar (fc, cfg);
			m_free (vh);
			if (kv_header && c2 == 0 && col)
				fputs (CS_RESET, stdout);
		}
		printf ("\n");
	}

	free (colw);
	free (cache);
}

void out_list (const data_t *d, void *cfg)
{
	if (!d)
		return;

	if (d->title_h)
		printf ("  %s\n", m_str (d->title_h));

	int i;
	field_t *f;
	m_foreach (d->items, i, f)
	{
		printf ("  ");
		out_field (f, cfg);
		printf ("\n");
	}
}

void out_text (const data_t *d, void *cfg)
{
	(void)cfg;
	if (!d)
		return;

	if (d->text_h) {
		const char *txt = m_str (d->text_h);
		printf ("  %s\n", txt);
	}
	if (d->footer_h)
		printf ("  %s\n", m_str (d->footer_h));
}
