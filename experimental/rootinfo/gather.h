#ifndef GATHER_H
#define GATHER_H

#include "cfg.h"
#include "m_types.h"
#include "m_tool.h"
#include "m_extra.h"
#include <ctype.h>

#define STRTAB_EMPTY(h) ((h) == 0 || m_len(h) == 0)
#define FIELD_ADD(container, str, a) do { \
	field_t f_ = { .str_h = s_dup(str), .fmt = FMT_NONE, .align = (a) }; \
	m_put((container), &f_); \
} while(0)

#define FIELD_ADD_H(container, handle, a) do { \
	field_t f_ = { .str_h = (handle), .fmt = FMT_NONE, .align = (a) }; \
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
	sec->entries = m_create((size_t)cap, sizeof(entry_t));
	return sec_h;
}

static inline void add_entry(int entries, const char *type, int data_h)
{
	entry_t e = {0};
	e.type_h = s_dup(type);
	e.data_h = data_h;
	m_put(entries, &e);
}

static inline int table_new_a(int ncols, const char **cols, const align_t *aligns)
{
	int th = m_alloc(1, sizeof(table_t), 0);
	table_t *t = (table_t *)m_buf(th);
	*t = (table_t){0};
	t->header = m_create((size_t)ncols, sizeof(field_t));
	for (int i = 0; i < ncols; i++)
		FIELD_ADD(t->header, cols[i], aligns ? aligns[i] : ALIGN_LEFT);
	return th;
}

static inline int table_new(int ncols, const char **cols)
{
	return table_new_a(ncols, cols, 0);
}

static inline int text_new(int text_h)
{
	int th = m_alloc(1, sizeof(text_t), 0);
	text_t *t = (text_t *)m_buf(th);
	*t = (text_t){0};
	t->text_h = text_h;
	return th;
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
