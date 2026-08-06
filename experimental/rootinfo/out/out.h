#ifndef OUT_H
#define OUT_H

#include "m_types.h"

void out_init(void *cfg);
int  out_render(int sections_h, void *cfg);
void out_section(section_t *s, void *cfg);
void free_sections(int sections_h);

void out_table(int data_h, void *cfg);
void out_list(int data_h, void *cfg);
void out_text(int data_h, void *cfg);
void out_bar(int data_h, void *cfg);
void out_field(const field_t *f, void *cfg);

#endif
