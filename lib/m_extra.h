#ifndef M_EXTRA_H
#define M_EXTRA_H

#include "mls.h"
#include "m_tool.h"

/* Case-insensitive comparison */
int s_casecmp (int a, int b);
int s_ncasecmp (int a, int b, int n);

/* Numeric conversion */
long s_to_long (int h);

/* Advanced Trimming (returns new handle) */
int s_trim_left_c (int h, const char *chars);
int s_trim_right_c (int h, const char *chars);

/* String Manipulation */
int s_reverse (int h); /* In-place */
int s_pad_left (int h, int width, char pad); /* Returns new handle */
int s_pad_right (int h, int width, char pad); /* Returns new handle */

/* Classification */
int s_is_numeric (int h);
int s_is_alpha (int h);

/* Security */
int s_secure_cmp (int a, int b); /* Constant-time comparison */

#endif
