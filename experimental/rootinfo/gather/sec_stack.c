#include "gather.h"
#include "m_types.h"
#include "m_subproc.h"
#include "m_tool.h"
#include <string.h>

#include <stdlib.h>

static void add_version(int items, const char *label, const char *cmd)
{
	int out = subproc_read(cmd);
	if (!out) return;

	int outbuf = s_new();
	int started = 0;
	for (int i = 0; ; i++) {
		char c = CHAR(out, i);
		if (c == 0 || c == '\n') break;
		if (!started && (c == ' ' || c == '\t')) continue;
		started = 1;
		m_putc(outbuf, c);
	}
	m_putc(outbuf, 0);
	m_free(out);

	if (!m_len(outbuf)) { m_free(outbuf); return; }

	char line[320];
	snprintf(line, sizeof(line), "%s: %s", label, m_str(outbuf));
	m_free(outbuf);
	FIELD_ADD(items, line, ALIGN_LEFT);
}

static void add_service(int items, const char *label, const char *name)
{
	char cmd[64];
	snprintf(cmd, sizeof(cmd), "pgrep -c %s 2>/dev/null", name);
	int out = subproc_read(cmd);
	if (!out) return;
	const char *v = m_str(out);
	int count = atoi(v);
	m_free(out);

	if (count > 0) {
		char line[64];
		snprintf(line, sizeof(line), "%s: running (%d)", label, count);
		FIELD_ADD(items, line, ALIGN_LEFT);
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
