#ifndef M_STRPOOL_H
#define M_STRPOOL_H

#include <stdint.h>

/* Persistent, append-only string pool. IDs start at 1 and never change
   (0 = invalid). On-disk layout: "MLSSTR1" + version + count + next_id,
   followed by { uint32 len; bytes[len] } entries in ID order. */
typedef struct m_strpool m_strpool_t;

m_strpool_t *m_strpool_create (const char *path);
m_strpool_t *m_strpool_open (const char *path);
int m_strpool_close (m_strpool_t *sp);

uint32_t m_strpool_intern (m_strpool_t *sp, const char *s); /* 0 on error */
uint32_t m_strpool_lookup (const m_strpool_t *sp, const char *s); /* 0 miss */
const char *m_strpool_resolve (const m_strpool_t *sp,
			       uint32_t id); /* NULL bad id */
uint32_t m_strpool_count (const m_strpool_t *sp);

#endif
