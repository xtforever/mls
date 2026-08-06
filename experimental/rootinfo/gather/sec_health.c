#include "gather.h"
#include "m_types.h"
#include "m_subproc.h"
#include "m_tool.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

int gather_health(cfg_t cfg)
{
	(void)cfg;
	int sec_h = section_new("HEALTH", 8);
	section_t *sec = (section_t *)m_buf(sec_h);

	/* SSH posture */
	int ssh = s_new();
	int s_cfg = m_str_from_file("/etc/ssh/sshd_config");
	if (s_cfg >= 0) {
		int p, *d;
		int toks = m_alloc(4, sizeof(int), MFREE_EACH);
		s_msplit(toks, s_cfg, s_cstr("\n"));
		m_foreach(toks, p, d) {
			s_trim(*d);
			if (s_isempty(*d) || CHAR(*d, 0) == '#') continue;
			if (s_has_prefix(*d, "PermitRootLogin") ||
			    s_has_prefix(*d, "PasswordAuthentication")) {
				if (m_len(ssh)) s_cat(ssh, ", ");
				s_cat(ssh, m_str(*d));
			}
		}
		m_free(toks);
		m_free(s_cfg);
	}
	int s_listen = subproc_read("ss -tln 2>/dev/null");
	int has_ssh = s_listen > 0 && s_find(s_listen, ":22") >= 0;
	m_free(s_listen);
	int ssh_line = s_printf(0, 0, "sshd: %s%s%s",
		has_ssh ? "listening" : "not listening",
		ssh && !s_isempty(ssh) ? ", " : "",
		ssh ? m_str(ssh) : "");
	add_entry(sec->entries, text_new(ssh_line));
	m_free(ssh);

	/* Security framework */
	int aa = subproc_read("apparmor_status 2>/dev/null");
	if (aa > 0 && !s_isempty(aa)) {
		int lc = 0, ec = 0;
		int p, *d;
		int toks = m_alloc(4, sizeof(int), MFREE_EACH);
		s_msplit(toks, aa, s_cstr("\n"));
		m_foreach(toks, p, d) {
			if (s_has_suffix(*d, "profiles are loaded."))
				lc = (int)s_to_long(*d);
			else if (s_has_suffix(*d, "profiles are in enforce mode."))
				ec = (int)s_to_long(*d);
		}
		m_free(toks);
		add_entry(sec->entries, text_new(s_printf(0, 0, "apparmor: %d loaded, %d enforced", lc, ec)));
		m_free(aa);
	}
	if (access("/sys/module/selinux", F_OK) == 0)
		add_entry(sec->entries, text_new(s_dup("selinux: enabled")));

	/* GPU */
	int lspci = subproc_read("lspci 2>/dev/null | grep -iE 'vga|3d|display'");
	if (lspci > 0 && !s_isempty(lspci)) {
		int p, *d;
		int toks = m_alloc(4, sizeof(int), MFREE_EACH);
		s_msplit(toks, lspci, s_cstr("\n"));
		m_foreach(toks, p, d) {
			s_trim(*d);
			if (s_isempty(*d)) continue;
			int cc = s_chr(*d, ' ', 0);
			int col = cc >= 0 ? s_chr(*d, ':', cc) : -1;
			int body = col >= 0 ? s_right(*d, (int)s_strlen(*d) - col - 2) : *d;
			add_entry(sec->entries, text_new(s_printf(0, 0, "GPU: %s", m_str(body))));
			if (col >= 0) m_free(body);
		}
		m_free(toks);
	}
	m_free(lspci);

	/* Systemd health */
	int failed = subproc_read("systemctl --failed --no-legend 2>/dev/null");
	if (failed > 0 && !s_isempty(failed)) {
		s_trim(failed);
		int nf = 0, p;
		for (p = 0; p < (int)s_strlen(failed); p++)
			if (CHAR(failed, p) == '\n') nf++;
		add_entry(sec->entries, text_new(s_printf(0, 0, "systemd: %d failed units", nf)));
	} else {
		add_entry(sec->entries, text_new(s_dup("systemd: no failed units")));
	}
	m_free(failed);

	/* Pending updates */
	int up = subproc_read("apt list --upgradable 2>/dev/null");
	if (up > 0 && !s_isempty(up)) {
		int nup = -1, p;
		for (p = 0; p < (int)s_strlen(up); p++)
			if (CHAR(up, p) == '\n') nup++;
		if (nup < 0) nup = 0;
		add_entry(sec->entries, text_new(s_printf(0, 0, "updates: %d pending", nup)));
	} else {
		add_entry(sec->entries, text_new(s_dup("updates: none pending")));
	}
	m_free(up);

	/* Last reboot (from who -b) */
	int last = subproc_read("who -b 2>/dev/null");
	if (last > 0 && !s_isempty(last)) {
		s_trim(last);
		int sp = s_chr(last, ' ', 0);
		if (sp >= 0) {
			int boot = s_right(last, (int)s_strlen(last) - sp - 1);
			add_entry(sec->entries, text_new(s_printf(0, 0, "last boot: %s", m_str(boot))));
			m_free(boot);
		}
		m_free(last);
	}

	return sec_h;
}
