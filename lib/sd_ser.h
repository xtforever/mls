#ifndef SD_SER_H
#define SD_SER_H

#include "sd.h"
#include "sd_hdf.h"
#include "sd_struct.h"

#ifdef __cplusplus
extern "C" {
#endif

int sd_serialize (int list, int hdf_parent, const char *name);
int sd_deserialize (int hdf_root, const char *name, int *out_list);

int sd_serialize_to_string (int list);
int sd_serialize_file (int list, const char *path);

int sd_deserialize_from_string (const char *hdf_string);
int sd_deserialize_file (const char *path);

#ifdef __cplusplus
}
#endif

#endif
