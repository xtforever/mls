#include "gather.h"
#include "m_types.h"
#include "m_subproc.h"
#include "m_tool.h"
#include <unistd.h>

int gather_firewall(cfg_t cfg)
{
	(void)cfg;
	int sec_h = section_new("FIREWALL", 2);
	section_t *sec = (section_t *)m_buf(sec_h);
	int is_root = (geteuid() == 0);

	int ufw_h = subproc_read("ufw status 2>/dev/null");
	if (ufw_h > 0 && s_find(ufw_h, "Status: active") >= 0) {
		m_free(ufw_h);
		add_entry(sec->entries, text_new(s_dup("ufw: active")));
		return sec_h;
	}
	m_free(ufw_h);

	int out = 0, rc = subproc_run("iptables -S 2>/dev/null", &out, NULL, 0);
	m_free(out);

	if (!is_root) {
		add_entry(sec->entries, text_new(s_dup("ENABLED (requires root to list rules)")));
		return sec_h;
	}

	if (rc != 0) {
		int nft_h = subproc_read("nft list ruleset 2>/dev/null");
		int has_nft = nft_h > 0 && !s_isempty(nft_h);
		m_free(nft_h);
		if (has_nft)
			add_entry(sec->entries, text_new(s_dup("nftables: ENABLED")));
		else
			add_entry(sec->entries, text_new(s_dup("DISABLED")));
		return sec_h;
	}

	int nrules = 0;
	int pol_h = s_new();
	int p, *d;
	int lines = subproc_lines("iptables -S 2>/dev/null");
	if (!STRTAB_EMPTY(lines)) {
		m_foreach(lines, p, d) {
			if (s_has_prefix(*d, "-P ")) {
				if (m_len(pol_h) > 0) s_cat(pol_h, ", ");
				s_cat(pol_h, m_str(*d));
			} else if (!s_has_prefix(*d, "-N ") && !s_has_prefix(*d, "-X ")) {
				nrules++;
			}
		}
	}
	m_free(lines);

	add_entry(sec->entries, text_new(s_printf(0, 0, "iptables: ENABLED, %d rules", nrules)));
	if (!s_isempty(pol_h))
		add_entry(sec->entries, text_new(pol_h));
	else
		m_free(pol_h);

	return sec_h;
}
