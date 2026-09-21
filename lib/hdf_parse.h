#ifndef HDF_PARSE_H
#define HDF_PARSE_H

#include "list.h"

/* HDF s-expression parser: turns text into the universal list structure
 * (list.h). Pure parsing — no key/value interpretation. */

int hdf_parse (const char *text);      /* root LIST node */
int hdf_parse_file (const char *path); /* root LIST node, 0 on error */

#endif
