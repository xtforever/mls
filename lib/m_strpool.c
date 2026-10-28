#include "m_strpool.h"

#include "darray.h"
#include "hashmap.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SP_MAGIC "MLSSTR1"
#define SP_VERSION 1u
#define SP_HEADER 20u
#define SP_MAX_STR (64u * 1024u * 1024u)

struct m_strpool {
	FILE *fp;
	darray_t *id2str;  /* char* per id, index id-1 */
	hashmap_t *str2id; /* borrowed keys -> uint32 id */
	uint32_t count;
};

static int write_u32 (FILE *fp, uint32_t v)
{
	return fwrite (&v, sizeof v, 1, fp) == 1 ? 0 : -1;
}

static int read_u32 (FILE *fp, uint32_t *v)
{
	return fread (v, sizeof *v, 1, fp) == 1 ? 0 : -1;
}

static int write_header (m_strpool_t *sp)
{
	if (fseek (sp->fp, 0, SEEK_SET) != 0)
		return -1;
	if (fwrite (SP_MAGIC, 1, 8, sp->fp) != 8)
		return -1;
	uint32_t next = sp->count + 1;
	if (write_u32 (sp->fp, SP_VERSION) != 0 ||
	    write_u32 (sp->fp, sp->count) != 0 || write_u32 (sp->fp, next) != 0)
		return -1;
	return fflush (sp->fp) == 0 ? 0 : -1;
}

static void pool_free (m_strpool_t *sp)
{
	if (!sp)
		return;
	if (sp->fp)
		fclose (sp->fp);
	if (sp->id2str)
		for (int i = 0; i < da_len (sp->id2str); i++)
			free (da_get (sp->id2str, i));
	da_free (sp->id2str);
	hm_free (sp->str2id);
	free (sp);
}

static m_strpool_t *pool_alloc (FILE *fp)
{
	m_strpool_t *sp = calloc (1, sizeof *sp);
	if (!sp) {
		fclose (fp);
		return NULL;
	}
	sp->fp = fp;
	sp->id2str = da_new ();
	sp->str2id = hm_new ();
	if (!sp->id2str || !sp->str2id) {
		pool_free (sp);
		return NULL;
	}
	return sp;
}

m_strpool_t *m_strpool_create (const char *path)
{
	if (!path)
		return NULL;
	FILE *fp = fopen (path, "w+b");
	if (!fp)
		return NULL;
	m_strpool_t *sp = pool_alloc (fp);
	if (!sp)
		return NULL;
	sp->count = 0;
	if (write_header (sp) != 0) {
		pool_free (sp);
		return NULL;
	}
	return sp;
}

m_strpool_t *m_strpool_open (const char *path)
{
	if (!path)
		return NULL;
	FILE *fp = fopen (path, "r+b");
	if (!fp)
		return NULL;

	char magic[8];
	uint32_t ver, count, next;
	if (fread (magic, 1, 8, fp) != 8 || memcmp (magic, SP_MAGIC, 8) != 0 ||
	    read_u32 (fp, &ver) != 0 || read_u32 (fp, &count) != 0 ||
	    read_u32 (fp, &next) != 0 || ver != SP_VERSION ||
	    count == UINT32_MAX || next != count + 1) {
		fclose (fp);
		return NULL;
	}

	if (fseek (fp, 0, SEEK_END) != 0) {
		fclose (fp);
		return NULL;
	}
	long sz = ftell (fp);
	if (sz < 0 || (uint64_t)sz < SP_HEADER ||
	    (uint64_t)count > ((uint64_t)sz - SP_HEADER) / 4) {
		fclose (fp);
		return NULL;
	}

	m_strpool_t *sp = pool_alloc (fp);
	if (!sp)
		return NULL;
	sp->count = 0;

	if (fseek (fp, (long)SP_HEADER, SEEK_SET) != 0)
		goto fail;
	for (uint32_t i = 0; i < count; i++) {
		uint32_t len;
		if (read_u32 (fp, &len) != 0 || len > SP_MAX_STR ||
		    (uint64_t)len > (uint64_t)(sz - SP_HEADER))
			goto fail;
		char *s = malloc ((size_t)len + 1);
		if (!s)
			goto fail;
		if (len && fread (s, 1, len, fp) != len) {
			free (s);
			goto fail;
		}
		s[len] = '\0';
		da_push (sp->id2str, s);
		if (da_len (sp->id2str) != (int)(i + 1) ||
		    hm_put (sp->str2id, s, i + 1) != 0) {
			/* roll back the just-pushed entry */
			free (da_pop (sp->id2str));
			goto fail;
		}
		sp->count++;
	}
	if (fgetc (fp) != EOF)
		goto fail;
	return sp;

fail:
	pool_free (sp);
	return NULL;
}

int m_strpool_close (m_strpool_t *sp)
{
	if (!sp)
		return 0;
	int rc = fflush (sp->fp) == 0 ? 0 : -1;
	pool_free (sp);
	return rc;
}

uint32_t m_strpool_intern (m_strpool_t *sp, const char *s)
{
	if (!sp || !s)
		return 0;
	uint32_t id;
	if (hm_get (sp->str2id, s, &id))
		return id;

	size_t n = strlen (s);
	if (n > UINT32_MAX || sp->count == UINT32_MAX)
		return 0;

	char *copy = malloc (n + 1);
	if (!copy)
		return 0;
	memcpy (copy, s, n + 1);

	da_push (sp->id2str, copy);
	if (da_len (sp->id2str) != (int)sp->count + 1) {
		da_pop (sp->id2str);
		free (copy);
		return 0;
	}

	uint32_t new_id = sp->count + 1;
	if (hm_put (sp->str2id, copy, new_id) != 0) {
		da_pop (sp->id2str);
		free (copy);
		return 0;
	}

	/* Append to disk and update the header. On an I/O error the in-memory
	   pool is ahead of the file; callers should treat that as fatal.
	   ponytail: no rollback because the hashmap has no delete
	   (append-only). */
	if (fseek (sp->fp, 0, SEEK_END) != 0) {
		sp->count++;
		return 0;
	}
	uint32_t len = (uint32_t)n;
	if (write_u32 (sp->fp, len) != 0 ||
	    (len && fwrite (s, 1, len, sp->fp) != len)) {
		sp->count++;
		return 0;
	}
	sp->count++;
	if (write_header (sp) != 0)
		return 0;
	return new_id;
}

uint32_t m_strpool_lookup (const m_strpool_t *sp, const char *s)
{
	if (!sp || !s)
		return 0;
	uint32_t id;
	return hm_get (sp->str2id, s, &id) ? id : 0;
}

const char *m_strpool_resolve (const m_strpool_t *sp, uint32_t id)
{
	if (!sp || id == 0 || id > sp->count)
		return NULL;
	return da_get (sp->id2str, (int)id - 1);
}

uint32_t m_strpool_count (const m_strpool_t *sp) { return sp ? sp->count : 0; }
