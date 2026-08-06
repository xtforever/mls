#ifndef M_TYPES_H
#define M_TYPES_H

#include "mls.h"
#include <stdint.h>

typedef enum { FMT_NONE=0, FMT_INT, FMT_FLOAT, FMT_HEX } field_fmt_t;
typedef enum { ALIGN_LEFT, ALIGN_RIGHT, ALIGN_CENTER } align_t;

typedef struct {
	int  str_h;
	field_fmt_t fmt;
	align_t align;
	int  len;
	int  prec;
	int  human;
	int  unit_h;
	int  is_bar;
	double frac;
} field_t;

typedef struct {
	int  title_h;
	int  items;
} list_t;

typedef struct {
	int  title_h;
	int  header;
	int  rows;
} table_t;

typedef struct {
	int  text_h;
	int  footer_h;
} text_t;

typedef struct {
	int  title;
	int  footer;
	int  type_h;
	int  data_h;
} entry_t;

typedef struct {
	int  title;
	int  footer;
	int  entries;
} section_t;

typedef void (*dt_render_fn)(int data_h, void *cfg);
typedef void (*dt_free_fn)(int data_h);

typedef struct {
	const char *name;
	dt_render_fn render;
	dt_free_fn   free;
} datatype_t;

void dt_register(const datatype_t *dt);
void dt_free_all(void);
const datatype_t *dt_lookup(const char *name);
void dt_render(const entry_t *e, void *cfg);
void dt_free(const entry_t *e);

int  field_new(void);
void field_set(field_t *f, const char *str, field_fmt_t fmt, align_t align);

#endif
