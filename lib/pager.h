#ifndef PAGER_H
#define PAGER_H

#include <stdint.h>

#define PAGER_PAGE_SIZE 4096

/* Fixed-size page I/O with a small write-back LRU cache.
   Buffers passed to read/write MUST be PAGER_PAGE_SIZE bytes.
   Page numbers are 0-based; write may append page == pager_pages(). */
typedef struct pager pager_t;

pager_t *pager_create (const char *path); /* create/truncate */
pager_t *pager_open (const char *path);	  /* existing file, validates size */
int pager_close (pager_t *p);		  /* flush + close, 0 ok */
int pager_flush (pager_t *p);
int pager_read (pager_t *p, uint32_t page, void *buf);
int pager_write (pager_t *p, uint32_t page, const void *buf);
uint32_t pager_pages (const pager_t *p);
uint64_t pager_reads (const pager_t *p);  /* physical pread count */
uint64_t pager_writes (const pager_t *p); /* physical pwrite count */

#endif
