#include "sd_struct.h"
#include "sd_thread.h"
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TYPES 64

static const sd_struct_type_t *g_types[MAX_TYPES];
static int g_type_count = 0;
static pthread_mutex_t g_type_mtx = PTHREAD_MUTEX_INITIALIZER;

int sd_struct_register (const sd_struct_type_t *type)
{
	if (!type || !type->name || !type->fields)
		return -1;

	pthread_mutex_lock (&g_type_mtx);

	if (g_type_count >= MAX_TYPES) {
		pthread_mutex_unlock (&g_type_mtx);
		return -1;
	}

	for (int i = 0; i < g_type_count; i++) {
		if (strcmp (g_types[i]->name, type->name) == 0) {
			pthread_mutex_unlock (&g_type_mtx);
			return -1;
		}
	}

	g_types[g_type_count++] = type;
	pthread_mutex_unlock (&g_type_mtx);
	return 0;
}

const sd_struct_type_t *sd_struct_lookup (const char *name)
{
	if (!name)
		return NULL;

	pthread_mutex_lock (&g_type_mtx);
	for (int i = 0; i < g_type_count; i++) {
		if (strcmp (g_types[i]->name, name) == 0) {
			pthread_mutex_unlock (&g_type_mtx);
			return g_types[i];
		}
	}
	pthread_mutex_unlock (&g_type_mtx);
	return NULL;
}

void sd_free_struct_fields (void *msg, const sd_field_info_t *fields,
			    int count);

static int encode_fields (const void *elem, const sd_field_info_t *fields,
			  int count, int hdf_node);

static int encode_field (const void *elem, const sd_field_info_t *field,
			 int hdf_node)
{
	void *ptr = (char *)elem + field->offset;
	int child;
	extern int hdf_add_child (int parent, const char *name);
	extern void hdf_set_property (int node, const char *key,
				      const char *value);
	extern int sd_serialize (int list, int hdf_parent, const char *name);

	switch (field->type) {
	case SD_INT32: {
		int val = *(int *)ptr;
		char buf[32];
		snprintf (buf, sizeof (buf), "%d", val);
		hdf_set_property (hdf_node, field->name, buf);
		break;
	}
	case SD_INT64: {
		long long val = *(long long *)ptr;
		char buf[32];
		snprintf (buf, sizeof (buf), "%lld", val);
		hdf_set_property (hdf_node, field->name, buf);
		break;
	}
	case SD_UINT32: {
		unsigned int val = *(unsigned int *)ptr;
		char buf[32];
		snprintf (buf, sizeof (buf), "%u", val);
		hdf_set_property (hdf_node, field->name, buf);
		break;
	}
	case SD_UINT64: {
		unsigned long long val = *(unsigned long long *)ptr;
		char buf[32];
		snprintf (buf, sizeof (buf), "%llu", val);
		hdf_set_property (hdf_node, field->name, buf);
		break;
	}
	case SD_FLOAT: {
		float val = *(float *)ptr;
		char buf[32];
		snprintf (buf, sizeof (buf), "%g", (double)val);
		hdf_set_property (hdf_node, field->name, buf);
		break;
	}
	case SD_DOUBLE: {
		double val = *(double *)ptr;
		char buf[32];
		snprintf (buf, sizeof (buf), "%g", val);
		hdf_set_property (hdf_node, field->name, buf);
		break;
	}
	case SD_STRING: {
		char *val = *(char **)ptr;
		if (val)
			hdf_set_property (hdf_node, field->name, val);
		break;
	}
	case SD_BOOL: {
		int val = *(int *)ptr;
		hdf_set_property (hdf_node, field->name,
				  val ? "true" : "false");
		break;
	}
	case SD_SUBLIST: {
		int list_h = *(int *)ptr;
		if (list_h > 0) {
			child = hdf_add_child (hdf_node, field->name);
			sd_serialize (list_h, child, "items");
		}
		break;
	}
	case SD_SUBMSG: {
		if (field->sub_fields) {
			child = hdf_add_child (hdf_node, field->name);
			encode_fields (ptr, field->sub_fields, field->sub_count,
				       child);
		}
		break;
	}
	case SD_SUBLIST_STRUCT: {
		int list_h = *(int *)ptr;
		if (list_h > 0 && field->sub_fields) {
			child = hdf_add_child (hdf_node, field->name);
			int len = sd_len (list_h);
			for (int i = 0; i < len; i++) {
				char idx[16];
				snprintf (idx, sizeof (idx), "%d", i);
				int item_node = hdf_add_child (child, idx);
				void *item = sd_get (list_h, i);
				encode_fields (item, field->sub_fields,
					       field->sub_count, item_node);
			}
		}
		break;
	}
	}
	return 0;
}

static int encode_fields (const void *elem, const sd_field_info_t *fields,
			  int count, int hdf_node)
{
	for (int i = 0; i < count; i++) {
		encode_field (elem, &fields[i], hdf_node);
	}
	return 0;
}

int sd_encode_struct (const void *msg, const char *type_name, int hdf_node)
{
	const sd_struct_type_t *type = sd_struct_lookup (type_name);
	if (!type)
		return -1;
	return encode_fields (msg, type->fields, type->field_count, hdf_node);
}

static int decode_fields (void *msg, const sd_field_info_t *fields, int count,
			  int hdf_node, int skip_errors);

static int decode_field (void *elem, const sd_field_info_t *field, int hdf_node,
			 int skip_errors);

static int decode_sublist (void *ptr, const sd_field_info_t *field,
			   int hdf_node, int skip_errors)
{
	extern int hdf_find_node (int node, const char *name);
	extern int sd_deserialize (int hdf_root, const char *name,
				   int *out_list);
	extern int hdf_get_children (int h);

	int child = hdf_find_node (hdf_node, "items");
	if (child <= 0 && !skip_errors)
		return -1;

	int list_h = 0;
	if (child > 0)
		sd_deserialize (child, "items", &list_h);
	*(int *)ptr = list_h;
	return list_h > 0 ? 0 : -1;
}

static int decode_sublist_struct (void *ptr, const sd_field_info_t *field,
				  int hdf_node)
{
	const sd_struct_type_t *type = sd_struct_lookup (field->name);
	if (!type)
		return -1;

	extern int hdf_get_children (int h);
	extern int hdf_find_node (int node, const char *name);

	int list_h = sd_alloc_typed (10, type->elem_size, SD_FREE, field->name);
	if (list_h <= 0)
		return -1;

	int children = hdf_get_children (hdf_node);
	int *child_buf = sd_buf (children);
	int child_count = sd_len (children);

	for (int i = 0; i < child_count; i++) {
		void *item = sd_add (list_h);
		decode_fields (item, field->sub_fields, field->sub_count,
			       child_buf[i], 0);
	}
	sd_free (children);

	*(int *)ptr = list_h;
	return 0;
}

static int decode_field (void *elem, const sd_field_info_t *field, int hdf_node,
			 int skip_errors)
{
	extern const char *hdf_get_property (int node, const char *key);
	extern int hdf_find_node (int node, const char *name);
	extern int hdf_get_children (int h);

	void *ptr = (char *)elem + field->offset;

	switch (field->type) {
	case SD_INT32: {
		const char *s = hdf_get_property (hdf_node, field->name);
		if (s)
			*(int *)ptr = atoi (s);
		else if (!skip_errors)
			return -1;
		break;
	}
	case SD_INT64: {
		const char *s = hdf_get_property (hdf_node, field->name);
		if (s)
			*(long long *)ptr = atoll (s);
		else if (!skip_errors)
			return -1;
		break;
	}
	case SD_UINT32: {
		const char *s = hdf_get_property (hdf_node, field->name);
		if (s)
			*(unsigned int *)ptr =
				(unsigned int)strtoul (s, NULL, 10);
		else if (!skip_errors)
			return -1;
		break;
	}
	case SD_UINT64: {
		const char *s = hdf_get_property (hdf_node, field->name);
		if (s)
			*(unsigned long long *)ptr = strtoull (s, NULL, 10);
		else if (!skip_errors)
			return -1;
		break;
	}
	case SD_FLOAT: {
		const char *s = hdf_get_property (hdf_node, field->name);
		if (s)
			*(float *)ptr = (float)atof (s);
		else if (!skip_errors)
			return -1;
		break;
	}
	case SD_DOUBLE: {
		const char *s = hdf_get_property (hdf_node, field->name);
		if (s)
			*(double *)ptr = atof (s);
		else if (!skip_errors)
			return -1;
		break;
	}
	case SD_STRING: {
		const char *s = hdf_get_property (hdf_node, field->name);
		if (s)
			*(char **)ptr = strdup (s);
		else if (!skip_errors)
			return -1;
		break;
	}
	case SD_BOOL: {
		const char *s = hdf_get_property (hdf_node, field->name);
		if (s) {
			if (strcmp (s, "true") == 0 || strcmp (s, "1") == 0 ||
			    strcmp (s, "yes") == 0)
				*(int *)ptr = 1;
			else
				*(int *)ptr = 0;
		} else if (!skip_errors)
			return -1;
		break;
	}
	case SD_SUBLIST: {
		return decode_sublist (ptr, field, hdf_node, skip_errors);
	}
	case SD_SUBMSG: {
		if (field->sub_fields) {
			int child = hdf_find_node (hdf_node, field->name);
			if (child > 0) {
				decode_fields (ptr, field->sub_fields,
					       field->sub_count, child,
					       skip_errors);
			} else if (!skip_errors)
				return -1;
		}
		break;
	}
	case SD_SUBLIST_STRUCT: {
		int child = hdf_find_node (hdf_node, field->name);
		if (child > 0) {
			return decode_sublist_struct (ptr, field, child);
		} else if (!skip_errors)
			return -1;
		break;
	}
	}
	return 0;
}

static int decode_fields (void *msg, const sd_field_info_t *fields, int count,
			  int hdf_node, int skip_errors)
{
	int errors = 0;
	for (int i = 0; i < count; i++) {
		if (decode_field (msg, &fields[i], hdf_node, skip_errors) < 0)
			errors++;
	}
	return skip_errors ? 0 : errors;
}

int sd_decode_struct (void *msg, const char *type_name, int hdf_node,
		      int skip_errors)
{
	const sd_struct_type_t *type = sd_struct_lookup (type_name);
	if (!type)
		return -1;
	return decode_fields (msg, type->fields, type->field_count, hdf_node,
			      skip_errors);
}

void sd_free_struct_fields (void *msg, const sd_field_info_t *fields, int count)
{
	for (int i = 0; i < count; i++) {
		void *ptr = (char *)msg + fields[i].offset;
		switch (fields[i].type) {
		case SD_STRING: {
			char **s = *(char ***)ptr;
			if (s)
				free (s);
			break;
		}
		case SD_SUBLIST:
		case SD_SUBLIST_STRUCT: {
			int list_h = *(int *)ptr;
			if (list_h > 0)
				sd_free (list_h);
			break;
		}
		case SD_SUBMSG: {
			if (fields[i].sub_fields)
				sd_free_struct_fields (ptr,
						       fields[i].sub_fields,
						       fields[i].sub_count);
			break;
		}
		default:
			break;
		}
	}
}

void sd_free_struct (void *msg, const char *type_name)
{
	const sd_struct_type_t *type = sd_struct_lookup (type_name);
	if (!type)
		return;
	sd_free_struct_fields (msg, type->fields, type->field_count);
}

int sd_encode_array_struct (const void *array, int len, size_t elem_size,
			    const char *type_name, int hdf_parent,
			    const char *name)
{
	extern int hdf_add_child (int parent, const char *name);

	const sd_struct_type_t *type = sd_struct_lookup (type_name);
	if (!type)
		return -1;

	int array_node = hdf_add_child (hdf_parent, name);
	if (array_node <= 0)
		return -1;

	const char *byte_ptr = (const char *)array;
	for (int i = 0; i < len; i++) {
		char idx[32];
		snprintf (idx, sizeof (idx), "%d", i);
		int item_node = hdf_add_child (array_node, idx);
		encode_fields (byte_ptr + i * elem_size, type->fields,
			       type->field_count, item_node);
	}

	return 0;
}

int sd_decode_array_struct (int hdf_node, const char *name,
			    const char *type_name, size_t elem_size,
			    int *out_list)
{
	extern int hdf_find_node (int node, const char *name);
	extern int hdf_get_children (int h);

	const sd_struct_type_t *type = sd_struct_lookup (type_name);
	if (!type)
		return -1;

	int list_h = sd_alloc_typed (10, elem_size, SD_FREE, type_name);
	if (list_h <= 0)
		return -1;

	int array_node = hdf_find_node (hdf_node, name);
	if (array_node <= 0) {
		sd_free (list_h);
		return -1;
	}

	int children = hdf_get_children (array_node);
	int *child_buf = sd_buf (children);
	int child_count = sd_len (children);

	for (int i = 0; i < child_count; i++) {
		void *item = sd_add (list_h);
		decode_fields (item, type->fields, type->field_count,
			       child_buf[i], 0);
	}
	sd_free (children);

	*out_list = list_h;
	return child_count;
}

void sd_struct_free_array (int list, const char *type_name)
{
	if (list <= 0)
		return;

	const sd_struct_type_t *type = sd_struct_lookup (type_name);
	if (!type)
		return;

	for (int i = 0; i < sd_len (list); i++) {
		void *item = sd_get (list, i);
		sd_free_struct_fields (item, type->fields, type->field_count);
	}
	sd_free (list);
}
