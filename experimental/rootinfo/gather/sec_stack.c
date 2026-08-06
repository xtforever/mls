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

	FIELD_ADD_H(items, s_printf(0, 0, "%s: %s", label, m_str(outbuf)));
	m_free(outbuf);
}

static void add_service(int items, const char *label, const char *name)
{
	int cmd = s_printf(0, 0, "pgrep -c %s 2>/dev/null", name);
	int out = subproc_read(m_str(cmd));
	m_free(cmd);
	if (STRTAB_EMPTY(out)) { m_free(out); return; }
	int line = str_line(out);
	m_free(out);
	long count = s_to_long(line);
	m_free(line);

	if (count > 0)
		FIELD_ADD_H(items, s_printf(0, 0, "%s: running (%ld)", label, count));
}

int gather_stack(cfg_t cfg)
{
	(void)cfg;
	int sec_h = section_new("SOFTWARE STACKS", 2);
	section_t *sec = (section_t *)m_buf(sec_h);

	int lh = data_new(DT_LIST);
	data_t *l = (data_t *)m_buf(lh);
	l->items = m_create(10, sizeof(field_t));

	add_version(l->items, "Python", "python3 --version 2>&1");
	add_version(l->items, "PHP", "php --version 2>&1");
	add_version(l->items, "PHP-FPM", "php-fpm --version 2>&1");

	add_service(l->items, "Apache", "apache2");
	add_service(l->items, "Apache", "httpd");
	add_service(l->items, "Nginx", "nginx");
	add_service(l->items, "HAProxy", "haproxy");

	add_entry(sec->entries, lh);

	return sec_h;
}
