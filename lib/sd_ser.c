#include "sd_ser.h"
#include "sd_thread.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int serialize_element (int list, int idx, int hdf_node)
{
	if (hdf_node <= 0)
		return -1;

	char idx_buf[32];
	snprintf (idx_buf, sizeof (idx_buf), "%d", idx);
	int elem_node = hdf_add_child (hdf_node, idx_buf);
	if (elem_node <= 0)
		return -1;

	void *elem = sd_get (list, idx);
	int elem_width = sd_width (list);

	switch (elem_width) {
	case 1: {
		char *val = (char *)elem;
		hdf_set_property (elem_node, "value", val);
		break;
	}
	case 4: {
		int *val = (int *)elem;
		char buf[32];
		snprintf (buf, sizeof (buf), "%d", *val);
		hdf_set_property (elem_node, "value", buf);
		break;
	}
	default: {
		char *data = (char *)elem;
		int hex = sd_new ();
		for (int i = 0; i < elem_width; i++) {
			char byte[4];
			snprintf (byte, sizeof (byte), "%02x",
				  (unsigned char)data[i]);
			sd_cat (hex, byte);
		}
		hdf_set_property (elem_node, "hex", sd_buf (hex));
		sd_sfree (hex);
		break;
	}
	}

	return 0;
}

int sd_serialize (int list, int hdf_parent, const char *name)
{
	if (list <= 0 || hdf_parent <= 0)
		return -1;

	int parent = hdf_parent;
	int root_node = hdf_find_node (hdf_parent, "root");
	if (root_node > 0)
		parent = root_node;

	int array_node = hdf_add_child (parent, name);
	if (array_node <= 0)
		return -1;

	hdf_set_property (array_node, "type", "sd_array");
	hdf_set_int (array_node, "len", sd_len (list));
	hdf_set_int (array_node, "width", sd_width (list));

	int free_hdl = sd_get_free_hdl (list);
	const char *type_name = sd_get_type_name (list);
	if (type_name)
		hdf_set_property (array_node, "element_type", type_name);
	else
		hdf_set_property (array_node, "free_hdl", "raw");

	int len = sd_len (list);
	for (int i = 0; i < len; i++) {
		if (serialize_element (list, i, array_node) < 0)
			return -1;
	}

	return 0;
}

int sd_deserialize (int hdf_root, const char *name, int *out_list)
{
	if (hdf_root <= 0 || !out_list)
		return -1;

	int array_node = hdf_find_node (hdf_root, name);
	if (array_node <= 0)
		return -1;

	int len = hdf_get_int (array_node, "len", 0);
	int width = hdf_get_int (array_node, "width", 1);
	const char *type_name = hdf_get_property (array_node, "element_type");

	int free_hdl = SD_FREE;
	if (type_name) {
		free_hdl = SD_FREE;
	} else {
		const char *fh = hdf_get_property (array_node, "free_hdl");
		if (fh && strcmp (fh, "strings") == 0)
			free_hdl = SD_FREE_STRINGS;
		else if (fh && strcmp (fh, "list") == 0)
			free_hdl = SD_FREE_LIST;
	}

	int list_h;
	if (type_name)
		list_h = sd_alloc_typed (len > 0 ? len : 4, width, free_hdl,
					 type_name);
	else
		list_h = sd_alloc (len > 0 ? len : 4, width, free_hdl);

	if (list_h <= 0)
		return -1;

	int children = hdf_get_children (array_node);
	if (children <= 0) {
		*out_list = list_h;
		return 0;
	}

	int *child_buf = sd_buf (children);
	int child_count = sd_len (children);

	for (int i = 0; i < child_count; i++) {
		int elem_node = child_buf[i];
		const char *value = hdf_get_property (elem_node, "value");

		if (value) {
			if (width == 1) {
				char c = value[0];
				sd_put (list_h, &c);
			} else if (width == 4) {
				int v = atoi (value);
				sd_put (list_h, &v);
			} else {
				int v = atoi (value);
				sd_put (list_h, &v);
			}
		} else {
			const char *hex = hdf_get_property (elem_node, "hex");
			if (hex) {
				for (int j = 0; j < width && hex[j * 2]; j++) {
					char byte[3] = {hex[j * 2],
							hex[j * 2 + 1], 0};
					char b = (char)strtol (byte, NULL, 16);
					sd_put (list_h, &b);
				}
			}
		}
	}

	sd_free (children);
	*out_list = list_h;
	return len;
}

static int hdf_to_string_recursive (int node, int indent, int *out)
{
	hdf_node_type_t type = hdf_get_type (node);

	if (type == HDF_TYPE_LIST) {
		int children = hdf_get_children (node);
		if (children <= 0)
			return 0;

		int *buf = sd_buf (children);
		int num_children = sd_len (children);

		for (int i = 0; i < num_children; i++) {
			int child = buf[i];
			int child_type = hdf_get_type (child);

			for (int j = 0; j < indent; j++)
				sd_cat (*out, "  ");

			if (child_type == HDF_TYPE_LIST) {
				int grandchildren = hdf_get_children (child);
				if (grandchildren > 0) {
					int *gbuf = sd_buf (grandchildren);
					int gc_len = sd_len (grandchildren);

					if (gc_len >= 2 &&
					    hdf_get_type (gbuf[0]) ==
						    HDF_TYPE_STRING &&
					    hdf_get_type (gbuf[1]) ==
						    HDF_TYPE_STRING) {
						const char *key =
							hdf_get_value (gbuf[0]);
						const char *val =
							hdf_get_value (gbuf[1]);
						sd_cat (*out, "(");
						sd_cat (*out, key ? key : "");
						sd_cat (*out, ") ");
						if (val)
							sd_cat (*out, val);
						sd_cat (*out, "\n");
						continue;
					}

					const char *name =
						hdf_get_value (child);
					sd_cat (*out, "(");
					sd_cat (*out, name ? name : "");
					sd_cat (*out, "\n");

					for (int k = 0; k < gc_len; k++) {
						hdf_to_string_recursive (
							gbuf[k], indent + 1,
							out);
					}

					for (int j = 0; j < indent; j++)
						sd_cat (*out, "  ");
					sd_cat (*out, ")\n");
					continue;
				}

				const char *name = hdf_get_value (child);
				sd_cat (*out, "(");
				sd_cat (*out, name ? name : "");
				sd_cat (*out, ")\n");
			} else {
				sd_cat (*out, "(");
				const char *v = hdf_get_value (child);
				sd_cat (*out, v ? v : "");
				sd_cat (*out, ")\n");
			}
		}
	} else if (type == HDF_TYPE_STRING || type == HDF_TYPE_NUMBER ||
		   type == HDF_TYPE_BOOLEAN) {
		const char *value = hdf_get_value (node);
		if (value)
			sd_cat (*out, value);
	}

	return 0;
}

int sd_serialize_to_string (int list)
{
	if (list <= 0)
		return 0;

	int hdf = hdf_parse_string ("(root)");
	if (hdf <= 0)
		return 0;

	if (sd_serialize (list, hdf, "data") < 0) {
		hdf_free (hdf);
		return 0;
	}

	int out = sd_new ();
	hdf_to_string_recursive (hdf, 0, &out);
	hdf_free (hdf);

	return out;
}

int sd_serialize_file (int list, const char *path)
{
	if (list <= 0 || !path)
		return -1;

	int hdf = hdf_parse_string ("(root)");
	if (hdf <= 0)
		return -1;

	if (sd_serialize (list, hdf, "data") < 0) {
		hdf_free (hdf);
		return -1;
	}

	FILE *fp = fopen (path, "w");
	if (!fp) {
		hdf_free (hdf);
		return -1;
	}

	int out = sd_new ();
	hdf_to_string_recursive (hdf, 0, &out);
	fprintf (fp, "%s", sd_buf (out));
	sd_sfree (out);
	hdf_free (hdf);
	fclose (fp);

	return 0;
}

int sd_deserialize_from_string (const char *hdf_string)
{
	if (!hdf_string)
		return 0;

	int hdf = hdf_parse_string (hdf_string);
	if (hdf <= 0)
		return 0;

	int list;
	int res = sd_deserialize (hdf, "data", &list);
	hdf_free (hdf);

	if (res < 0)
		return 0;
	return list;
}

int sd_deserialize_file (const char *path)
{
	if (!path)
		return 0;

	FILE *fp = fopen (path, "r");
	if (!fp)
		return 0;

	fseek (fp, 0, SEEK_END);
	long len = ftell (fp);
	fseek (fp, 0, SEEK_SET);

	char *buf = malloc (len + 1);
	fread (buf, 1, len, fp);
	buf[len] = 0;
	fclose (fp);

	int list = sd_deserialize_from_string (buf);
	free (buf);

	return list;
}
