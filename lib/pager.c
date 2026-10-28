#define _POSIX_C_SOURCE 200809L

#include "pager.h"

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* ponytail: 16-page cache, LRU by timestamp, whole-page write-back.
   Upgrade: more slots / dirty-bit batching if the working set grows. */
#define PAGER_CACHE 16

typedef struct {
	uint32_t page;
	int valid;
	int dirty;
	uint64_t age;
	unsigned char data[PAGER_PAGE_SIZE];
} page_entry;

struct pager {
	int fd;
	uint32_t pages;
	uint64_t reads;
	uint64_t writes;
	uint64_t tick;
	page_entry cache[PAGER_CACHE];
};

static pager_t *pager_alloc (int fd, uint32_t pages)
{
	pager_t *p = calloc (1, sizeof *p);
	if (!p) {
		close (fd);
		return NULL;
	}
	p->fd = fd;
	p->pages = pages;
	return p;
}

pager_t *pager_create (const char *path)
{
	if (!path)
		return NULL;
	int fd = open (path, O_RDWR | O_CREAT | O_TRUNC, 0644);
	if (fd < 0)
		return NULL;
	return pager_alloc (fd, 0);
}

pager_t *pager_open (const char *path)
{
	if (!path)
		return NULL;
	int fd = open (path, O_RDWR);
	if (fd < 0)
		return NULL;

	struct stat st;
	if (fstat (fd, &st) != 0 || st.st_size < 0 ||
	    st.st_size % PAGER_PAGE_SIZE != 0 ||
	    (uint64_t)st.st_size / PAGER_PAGE_SIZE > UINT32_MAX) {
		close (fd);
		return NULL;
	}
	return pager_alloc (fd,
			    (uint32_t)((uint64_t)st.st_size / PAGER_PAGE_SIZE));
}

static int flush_entry (pager_t *p, page_entry *e)
{
	if (!e->dirty)
		return 0;
	ssize_t n = pwrite (p->fd, e->data, PAGER_PAGE_SIZE,
			    (off_t)e->page * PAGER_PAGE_SIZE);
	if (n != PAGER_PAGE_SIZE)
		return -1;
	e->dirty = 0;
	p->writes++;
	return 0;
}

int pager_flush (pager_t *p)
{
	if (!p)
		return -1;
	for (int i = 0; i < PAGER_CACHE; i++)
		if (p->cache[i].valid && flush_entry (p, &p->cache[i]) != 0)
			return -1;
	return fsync (p->fd) == 0 ? 0 : -1;
}

int pager_close (pager_t *p)
{
	if (!p)
		return 0;
	int rc = pager_flush (p);
	close (p->fd);
	free (p);
	return rc;
}

static page_entry *cache_acquire (pager_t *p, uint32_t page, int *hit)
{
	page_entry *free_slot = NULL, *lru = NULL;
	*hit = 0;
	for (int i = 0; i < PAGER_CACHE; i++) {
		page_entry *e = &p->cache[i];
		if (e->valid && e->page == page) {
			e->age = p->tick++;
			*hit = 1;
			return e;
		}
		if (!e->valid)
			free_slot = free_slot ? free_slot : e;
		else if (!lru || e->age < lru->age)
			lru = e;
	}
	page_entry *victim = free_slot ? free_slot : lru;
	if (!victim)
		return NULL;
	if (victim->valid && flush_entry (p, victim) != 0)
		return NULL;
	victim->valid = 1;
	victim->dirty = 0;
	victim->page = page;
	victim->age = p->tick++;
	return victim;
}

int pager_read (pager_t *p, uint32_t page, void *buf)
{
	if (!p || !buf || page >= p->pages)
		return -1;
	int hit;
	page_entry *e = cache_acquire (p, page, &hit);
	if (!e)
		return -1;
	if (!hit) {
		ssize_t n = pread (p->fd, e->data, PAGER_PAGE_SIZE,
				   (off_t)page * PAGER_PAGE_SIZE);
		if (n != PAGER_PAGE_SIZE) {
			e->valid = 0;
			return -1;
		}
		p->reads++;
	}
	memcpy (buf, e->data, PAGER_PAGE_SIZE);
	return 0;
}

int pager_write (pager_t *p, uint32_t page, const void *buf)
{
	if (!p || !buf || page > p->pages)
		return -1;
	int hit;
	page_entry *e = cache_acquire (p, page, &hit);
	(void)hit;
	if (!e)
		return -1;
	memcpy (e->data, buf, PAGER_PAGE_SIZE);
	e->dirty = 1;
	if (page == p->pages)
		p->pages++;
	return 0;
}

uint32_t pager_pages (const pager_t *p) { return p ? p->pages : 0; }

uint64_t pager_reads (const pager_t *p) { return p ? p->reads : 0; }

uint64_t pager_writes (const pager_t *p) { return p ? p->writes : 0; }
