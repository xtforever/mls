#include "gather.h"
#include "m_types.h"
#include "m_subproc.h"
#include "m_tool.h"
#include <string.h>

#include <stdlib.h>

static void add_version(int items, const char *label, const char *cmd)
{
	int out = subproc_read(cmd);
	if (STRTAB_EMPTY(out)) { m_free(out); return; }

	int outbuf = str_line(out);
	m_free(out);
	s_trim(outbuf);
	if (!m_len(outbuf)) { m_free(outbuf); return; }

	int line = s_printf(0, 0, "%s: %s", label, m_str(outbuf));
	m_free(outbuf);
	FIELD_ADD(items, m_str(line), ALIGN_LEFT);
	m_free(line);
}

static void add_service(int items, const char *label, const char *name)
{
	char cmd[64];
	snprintf(cmd, sizeof(cmd), "pgrep -c %s 2>/dev/null", name);
	int out = subproc_read(cmd);
	if (STRTAB_EMPTY(out)) { m_free(out); return; }
	int line = str_line(out);
	m_free(out);
	long count = s_to_long(line);
	m_free(line);

	if (count > 0) {
		int l = s_printf(0, 0, "%s: running (%ld)", label, count);
		FIELD_ADD(items, m_str(l), ALIGN_LEFT);
		m_free(l);
	}
}

int gather_stack(cfg_t cfg)
{
	(void)cfg;
	int sec_h = m_alloc(1, sizeof(section_t), 0);
	section_t *sec = (section_t *)m_buf(sec_h);
	*sec = (section_t){0};
	sec->title = s_dup("SOFTWARE STACKS");

	int lh = m_alloc(1, sizeof(list_t), 0);
	list_t *l = (list_t *)m_buf(lh);
	*l = (list_t){0};
	l->items = m_create(10, sizeof(field_t));

	add_version(l->items, "Python", "python3 --version 2>&1");
	add_version(l->items, "PHP", "php --version 2>&1");
	add_version(l->items, "PHP-FPM", "php-fpm --version 2>&1");

	add_service(l->items, "Apache", "apache2");
	add_service(l->items, "Apache", "httpd");
	add_service(l->items, "Nginx", "nginx");
	add_service(l->items, "HAProxy", "haproxy");

	entry_t e = { .type_h = s_dup("list"), .data_h = lh };
	sec->entries = m_create(2, sizeof(entry_t));
	m_put(sec->entries, &e);

	return sec_h;
}
