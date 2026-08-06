#ifndef GATHER_H
#define GATHER_H

#include "cfg.h"
#include "m_types.h"
#include "m_tool.h"
#include "m_extra.h"
#include <ctype.h>

#define STRTAB_EMPTY(h) ((h) == 0 || m_len(h) == 0)
#define FIELD_ADD(container, str) do { \
	field_t f_ = { .str_h = s_dup(str), .align = ALIGN_LEFT }; \
	m_put((container), &f_); \
} while(0)

#define FIELD_ADD_R(container, str) do { \
	field_t f_ = { .str_h = s_dup(str), .align = ALIGN_RIGHT }; \
	m_put((container), &f_); \
} while(0)

#define FIELD_ADD_H(container, handle) do { \
	field_t f_ = { .str_h = (handle), .align = ALIGN_LEFT }; \
	m_put((container), &f_); \
} while(0)

#define FIELD_ADD_H_R(container, handle) do { \
	field_t f_ = { .str_h = (handle), .align = ALIGN_RIGHT }; \
	m_put((container), &f_); \
} while(0)

#define STR_COPY(dst, dstsz, h) do { \
	int _i = 0; \
	for (; _i < (int)(dstsz) - 1; _i++) { \
		char _c = CHAR((h), _i); \
		if (_c == 0 || _c == '\n') break; \
		(dst)[_i] = _c; \
	} \
	(dst)[_i] = 0; \
} while(0)

static inline int mstr_empty(int a)
{
	return (a == 0 || m_len(a) == 0 || CHAR(a, 0) == 0);
}

static inline int section_new(const char *title, int cap)
{
	int sec_h = m_alloc(1, sizeof(section_t), 0);
	section_t *sec = (section_t *)m_buf(sec_h);
	*sec = (section_t){0};
	sec->title = s_dup(title);
	sec->entries = m_create((size_t)cap, sizeof(int));
	return sec_h;
}

static inline int data_new(dt_type_t type)
{
	int dh = m_alloc(1, sizeof(data_t), 0);
	data_t *d = (data_t *)m_buf(dh);
	*d = (data_t){ .type = type };
	return dh;
}

static inline void add_entry(int entries, int data_h)
{
	m_put(entries, &data_h);
}

static inline int table_new_a(int ncols, const char **cols, const align_t *aligns)
{
	int dh = data_new(DT_TABLE);
	data_t *d = (data_t *)m_buf(dh);
	d->header = m_create((size_t)ncols, sizeof(field_t));
	for (int i = 0; i < ncols; i++) {
		field_t f_ = { .str_h = s_dup(cols[i]), .align = aligns ? aligns[i] : ALIGN_LEFT };
		m_put(d->header, &f_);
	}
	d->rows = m_create((size_t)ncols, sizeof(int));
	return dh;
}

static inline int table_new(int ncols, const char **cols)
{
	return table_new_a(ncols, cols, 0);
}

static inline int text_new(int text_h)
{
	int dh = data_new(DT_TEXT);
	((data_t *)m_buf(dh))->text_h = text_h;
	return dh;
}

static inline int bar_new(int str_h, double frac)
{
	int dh = data_new(DT_BAR);
	data_t *d = (data_t *)m_buf(dh);
	d->bar_h = m_alloc(1, sizeof(field_t), 0);
	field_t *f = (field_t *)m_buf(d->bar_h);
	*f = (field_t){ .str_h = str_h, .is_bar = 1, .frac = frac };
	return dh;
}

static inline int _cmp_mstr(const void *va, const void *vb)
{
	int a = *(int *)va;
	int b = *(int *)vb;
	if (mstr_empty(a) || mstr_empty(b)) return 0;
	return m_cmp(a, b);
}

static inline int copy_word(int buf, int str)
{
	if (!buf) buf = m_create(10, 1); else m_clear(buf);
	int p; char *d;
	m_foreach(str, p, d) {
		if (isspace(*d)) break;
		m_putc(buf, *d);
	}
	m_putc(buf, 0);
	return buf;
}

static inline int str_dup_h(int h)
{
	int out = s_new();
	for (int i = 0; ; i++) {
		char c = CHAR(h, i);
		if (!c) break;
		m_putc(out, c);
	}
	m_putc(out, 0);
	return out;
}

static inline int str_line(int h)
{
	int out = m_create(256, 1);
	for (int i = 0; ; i++) {
		char c = CHAR(h, i);
		if (c == 0 || c == '\n') break;
		m_putc(out, c);
	}
	m_putc(out, 0);
	return out;
}

int gather_all(cfg_t cfg);

int gather_system(cfg_t cfg);
int gather_lvm(cfg_t cfg);
int gather_zfs(cfg_t cfg);
int gather_ports(cfg_t cfg);
int gather_proc(cfg_t cfg);
int gather_cron(cfg_t cfg);
int gather_firewall(cfg_t cfg);
int gather_stack(cfg_t cfg);

#endif
