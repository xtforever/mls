#include "gather.h"
#include "m_types.h"
#include "m_subproc.h"
#include "m_tool.h"
#include <string.h>

int gather_firewall(cfg_t cfg)
{
	(void)cfg;
	int out = 0, rc = subproc_run("iptables -L 2>/dev/null", &out, NULL, 0);
	m_free(out);

	const char *status;
	if (rc == 0)
		status = "ENABLED";
	else if (rc == 127)
		status = "DISABLED (iptables not found)";
	else
		status = "ENABLED (requires root to list rules)";

	int sec_h = m_alloc(1, sizeof(section_t), 0);
	section_t *sec = (section_t *)m_buf(sec_h);
	*sec = (section_t){0};
	sec->title = s_dup("IPTABLES");

	int th = m_alloc(1, sizeof(text_t), 0);
	text_t *t = (text_t *)m_buf(th);
	*t = (text_t){0};
	t->text_h = s_dup(status);

	entry_t e = { .type_h = s_dup("text"), .data_h = th };
	sec->entries = m_create(2, sizeof(entry_t));
	m_put(sec->entries, &e);

	return sec_h;
}
