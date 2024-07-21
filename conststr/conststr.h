#ifndef CONSTSTR_H
#define CONSTSTR_H

/*
  s_cstr - returns a handle to a constant string equal to the given c-style string 
  s_mstr - returns a handle to a constant string equal to the given array 
  cs_printf - creates a string and returns a handle to a constant string equal to the created one
*/

void conststr_init(void);
void conststr_free(void);
int conststr_lookup_c(const char *s);
int conststr_lookup(int s);
int cs_printf( int m, int p, const char *format, ... );

inline static int s_cstr(const char *s) { return conststr_lookup_c(s); }
inline static int s_mstr(int m) { return conststr_lookup(m); }

#endif // CONSTSTR_H
