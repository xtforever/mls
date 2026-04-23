#ifndef SD_HDF_H
#define SD_HDF_H

#include "sd.h"
#include "sd_strings.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	HDF_TYPE_LIST,
	HDF_TYPE_STRING,
	HDF_TYPE_NUMBER,
	HDF_TYPE_BOOLEAN,
	HDF_TYPE_NULL
} hdf_node_type_t;

#define HDF_OK 0
#define HDF_ERR_PARSE -1
#define HDF_ERR_ALLOC -2
#define HDF_ERR_NOTFOUND -3
#define HDF_ERR_TYPE -4
#define HDF_ERR_SER -5
#define HDF_ERR_DESER -6
#define HDF_ERR_OVERFLOW -7

typedef struct {
	int code;
	char message[256];
	char node_path[128];
	int line;
	const char *field_name;
} sd_hdf_error_t;

void sd_hdf_clear_error (void);
sd_hdf_error_t *sd_hdf_get_error (void);
const char *sd_hdf_error_string (void);

int hdf_parse_string (const char *data);
int hdf_parse_file (const char *path);

void hdf_free (int h);

hdf_node_type_t hdf_get_type (int h);
const char *hdf_get_value (int h);
int hdf_get_children (int h);
int hdf_find_node (int h, const char *key);
const char *hdf_get_property (int h, const char *key);
int hdf_get_int (int h, const char *key, int default_val);
int hdf_get_bool (int h, const char *key, int default_val);

int hdf_set_property (int h, const char *key, const char *value);
int hdf_set_int (int h, const char *key, int value);
int hdf_set_bool (int h, const char *key, int value);
int hdf_add_child (int h, const char *name);
int hdf_add_property (int h, const char *key, const char *value);

#ifdef __cplusplus
}
#endif

#endif
