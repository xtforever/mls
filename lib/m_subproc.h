#ifndef M_SUBPROC_H
#define M_SUBPROC_H

int subproc_run (const char *cmd, int *stdout_h, int *stderr_h, int timeout_ms);
int subproc_read (const char *cmd);
int subproc_lines (const char *cmd);

#endif
