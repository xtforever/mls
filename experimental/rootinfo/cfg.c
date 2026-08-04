#include "cfg.h"
#include "m_hdf.h"
#include "m_tool.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

static const char *default_cfg =
"(cfg\n"
"  (proc        (top 5))\n"
"  (ports       (max 5) (ipv4 true))\n"
"  (cron        (max_lines 10))\n"
"  (table       (max_col_width 24) (marker \"...\"))\n"
"  (bar         (width 20) (empty \"░\") (full \"▓\"))\n"
"  (section     (zfs true) (lvm true) (cron true) (proc true)\n"
"               (ports true) (firewall true) (stack true))\n"
")\n";

cfg_t cfg_load(const char *override)
{
	int h = 0;
	if (override && override[0])
		h = hdf_parse_file(override);
	if (!h)
		h = hdf_parse_file("./rootinfo.hdf");
	if (!h) {
		const char *home = getenv("HOME");
		if (home) {
			char buf[512];
			snprintf(buf, sizeof(buf), "%s/.config/rootinfo/rootinfo.hdf", home);
			h = hdf_parse_file(buf);
		}
	}
	if (!h) {
		h = hdf_parse_string(default_cfg);
		hdf_write_file(h, "./rootinfo.hdf");
	}
	return h;
}

void cfg_free(cfg_t cfg)
{
	if (cfg > 0) hdf_free(cfg);
}

int cfg_int(cfg_t root, const char *sect, const char *key, int dflt)
{
	int node = hdf_find_node(root, sect);
	if (!node) return dflt;
	return hdf_get_int(node, key, dflt);
}

int cfg_bool(cfg_t root, const char *sect, const char *key, int dflt)
{
	int node = hdf_find_node(root, sect);
	if (!node) return dflt;
	return hdf_get_bool(node, key, dflt);
}

const char *cfg_str(cfg_t root, const char *sect, const char *key, const char *dflt)
{
	int node = hdf_find_node(root, sect);
	if (!node) return dflt;
	const char *v = hdf_get_property(node, key);
	return v ? v : dflt;
}
