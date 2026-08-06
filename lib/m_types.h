#ifndef M_TYPES_H
#define M_TYPES_H

#include "mls.h"

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

typedef enum { DT_TABLE, DT_LIST, DT_TEXT, DT_BAR } dt_type_t;

typedef struct {
	dt_type_t type;   /* discriminator, select the live slots below */
	int  title_h;
	int  footer_h;
	int  header;      /* DT_TABLE: field list  */
	int  rows;        /* DT_TABLE: list of row handles */
	int  items;       /* DT_LIST:  field list  */
	int  text_h;      /* DT_TEXT:  string      */
	int  bar_h;       /* DT_BAR:   field       */
} data_t;

typedef struct {
	int  title;
	int  entries;     /* list of data_t handles */
} section_t;

#endif
