#ifndef OUT_H
#define OUT_H

#include "m_types.h"

void out_render(int sections_h, void *cfg);
void out_section(section_t *s, void *cfg);
void out_data(const data_t *d, void *cfg);

void out_table(const data_t *d, void *cfg);
void out_list(const data_t *d, void *cfg);
void out_text(const data_t *d, void *cfg);
void out_bar(const data_t *d, void *cfg);
void out_field(const field_t *f, void *cfg);

void free_sections(int sections_h);

#endif
