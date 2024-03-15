#ifndef CONSTSTR_H
#define CONSTSTR_H

void conststr_init(void);
void  conststr_free(void);
int conststr_lookup_c(const char *s);
int conststr_lookup(int s);
int cs_printf( int m, int p, const char *format, ... );
  
#endif // CONSTSTR_H
