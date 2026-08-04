#ifndef GATHER_H
#define GATHER_H

#include "cfg.h"
#include "m_types.h"

#define STRTAB_EMPTY(h) ((h) == 0 || m_len(h) == 0)
#define FIELD_ADD(container, str, a) do { \
	field_t f_ = { .str_h = s_dup(str), .fmt = FMT_NONE, .align = (a) }; \
	m_put((container), &f_); \
} while(0)

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
