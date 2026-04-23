#ifndef SD_STRUCT_H
#define SD_STRUCT_H

#include "sd.h"
#include "sd_strings.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	SD_INT32,
	SD_INT64,
	SD_UINT32,
	SD_UINT64,
	SD_FLOAT,
	SD_DOUBLE,
	SD_STRING,
	SD_BOOL,
	SD_SUBLIST,
	SD_SUBMSG,
	SD_SUBLIST_STRUCT
} sd_field_type_t;

typedef struct sd_field_info {
	const char *name;
	sd_field_type_t type;
	size_t offset;
	int max_size;
	const struct sd_field_info *sub_fields;
	int sub_count;
} sd_field_info_t;

typedef struct {
	const char *name;
	int elem_size;
	const sd_field_info_t *fields;
	int field_count;
} sd_struct_type_t;

int sd_struct_register (const sd_struct_type_t *type);
const sd_struct_type_t *sd_struct_lookup (const char *name);

int sd_encode_struct (const void *msg, const char *type_name, int hdf_node);
int sd_decode_struct (void *msg, const char *type_name, int hdf_node,
		      int skip_errors);
void sd_free_struct (void *msg, const char *type_name);

int sd_encode_array_struct (const void *array, int len, size_t elem_size,
			    const char *type_name, int hdf_parent,
			    const char *name);
int sd_decode_array_struct (int hdf_node, const char *name,
			    const char *type_name, size_t elem_size,
			    int *out_list);

void sd_struct_free_array (int list, const char *type_name);

#ifdef __cplusplus
}
#endif

#endif
