#ifndef GATHER_H
#define GATHER_H

#include "cfg.h"

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
