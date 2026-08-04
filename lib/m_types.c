#include "m_types.h"
#include "m_tool.h"
#include <string.h>
#include <stdlib.h>

static int reg = 0;

void dt_register(const datatype_t *dt)
{
	if (!reg) reg = m_create(4, sizeof(datatype_t));
	m_put(reg, dt);
}

const datatype_t *dt_lookup(const char *name)
{
	if (!reg) return NULL;
	for (int i = 0; i < (int)m_len(reg); i++) {
		datatype_t *p = (datatype_t *)m_peek(reg, (size_t)i);
		if (!strcmp(p->name, name))
			return p;
	}
	return NULL;
}

void dt_render(const entry_t *e, void *cfg)
{
	const datatype_t *dt = dt_lookup(m_str(e->type_h));
	if (dt && dt->render)
		dt->render(e->data_h, cfg);
}

void dt_free(const entry_t *e)
{
	const datatype_t *dt = dt_lookup(m_str(e->type_h));
	if (dt && dt->free)
		dt->free(e->data_h);
}

int field_new(void)
{
	field_t f = {0};
	int h = m_alloc(1, sizeof(field_t), 0);
	m_put(h, &f);
	return h;
}

void field_set(field_t *f, const char *str, field_fmt_t fmt, align_t align)
{
	f->str_h = s_dup(str);
	f->fmt = fmt;
	f->align = align;
}
