#ifndef GATHER_H
#define GATHER_H

#include "cfg.h"
#include "m_types.h"

#define STRTAB_EMPTY(h) ((h) == 0 || m_len(h) == 0)
#define FIELD_ADD(container, str, a) do { \
	field_t f_ = { .str_h = s_dup(str), .fmt = FMT_NONE, .align = (a) }; \
	m_put((container), &f_); \
} while(0)

#define STR_COPY(dst, dstsz, h) do { \
	int _i = 0; \
	for (; _i < (int)(dstsz) - 1; _i++) { \
		char _c = CHAR((h), _i); \
		if (_c == 0 || _c == '\n') break; \
		(dst)[_i] = _c; \
	} \
	(dst)[_i] = 0; \
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
