#include "gather.h"
#include "m_types.h"
#include "m_subproc.h"
#include "m_tool.h"
#include <unistd.h>

static int section_with_text(const char *title, const char *text)
{
	int sec_h = section_new(title, 2);
	add_entry(((section_t *)m_buf(sec_h))->entries, "text", text_new(s_dup(text)));
	return sec_h;
}

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
	else if (geteuid() != 0)
		status = "ENABLED (requires root to list rules)";
	else
		status = "DISABLED (iptables returned error)";

	return section_with_text("IPTABLES", status);
}
