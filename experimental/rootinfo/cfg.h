#ifndef CFG_H
#define CFG_H

typedef int cfg_t;

cfg_t cfg_load(const char *override);
void  cfg_free(cfg_t cfg);

int         cfg_int(cfg_t root, const char *sect, const char *key, int dflt);
int         cfg_bool(cfg_t root, const char *sect, const char *key, int dflt);
const char *cfg_str(cfg_t root, const char *sect, const char *key, const char *dflt);

#endif
