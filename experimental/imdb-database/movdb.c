/* movdb: kleine indexierte Datenbank ueber die gefundenen Zuordnungen
 * (Titel, Jahr, IMDb-ID, Dateipfad) aus movies-guess.tsv.
 *
 * Eigenes mmap-Binaerformat mit Inverted-Token-Index ueber die
 * normalisierten Titel-Tokens (CSR, wie imdb_bin.h).
 *
 *   movdb build <movies-guess.tsv> <out.bin>
 *   movdb query <out.bin> <term>...     # UND-Suche ueber Titel-Tokens
 *   movdb --selftest
 *
 * Header (8-aligned):
 *   off[0] rec_off  u32[N+1]   Offset je Record in rec_blob
 *   off[1] rec_blob            "title\tyear\tid\tpath\0" je Record
 *   off[2] dict_off u32[T+1]   Offset je Token in dict
 *   off[3] dict                Token-Strings (sortiert)
 *   off[4] postings u32[P]     Record-Indizes je Token
 *   off[5] post_off u32[T+1]   CSR-Zeilen
 */
#include "imdb_db.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define MOVDB_MAGIC "MOVDB001"
#define MOVDB_VERSION 1
#define U32_LIMIT (1ULL << 32)

struct mov_header {
	char magic[8];
	uint32_t version;
	uint32_t n_rec;
	uint32_t n_tok;
	uint32_t n_post;
	uint64_t off[6];
};

_Static_assert(sizeof (struct mov_header) == 72,
	       "mov_header muss 72 Byte sein");

struct buf {
	unsigned char *p;
	size_t len, cap;
};

static int buf_put (struct buf *b, const void *src, size_t n)
{
	if (b->len + n > b->cap) {
		size_t cap = b->cap ? b->cap : 256;
		while (cap < b->len + n)
			cap <<= 1;
		unsigned char *p = realloc (b->p, cap);
		if (!p)
			return 0;
		b->p = p;
		b->cap = cap;
	}
	memcpy (b->p + b->len, src, n);
	b->len += n;
	return 1;
}

static int push_u32 (struct buf *b, uint32_t v) { return buf_put (b, &v, 4); }

struct occ {
	uint32_t tok_off;
	uint32_t rec;
};

static const unsigned char *g_blob;

static int occ_cmp (const void *a, const void *b)
{
	const struct occ *x = a, *y = b;
	int c = strcmp ((const char *)g_blob + x->tok_off,
			(const char *)g_blob + y->tok_off);
	if (c)
		return c;
	if (x->rec != y->rec)
		return x->rec < y->rec ? -1 : 1;
	return 0;
}

static uint64_t align8 (uint64_t v) { return (v + 7) & ~(uint64_t)7; }

static int write_section (FILE *fp, const void *p, size_t len)
{
	static const char zero[8] = {0};
	if (len && fwrite (p, 1, len, fp) != len)
		return 0;
	size_t pad = (size_t)((8 - (len % 8)) % 8);
	if (pad && fwrite (zero, 1, pad, fp) != pad)
		return 0;
	return 1;
}

/* ---- build ---- */
static int build (const char *tsv, const char *out)
{
	FILE *fp = fopen (tsv, "r");
	if (!fp) {
		fprintf (stderr, "movdb: %s: %s\n", tsv, strerror (errno));
		return 1;
	}

	struct buf rec_off = {0}, rec_blob = {0}, tokblob = {0}, occ = {0},
		   dict = {0}, dict_off = {0}, postings = {0}, post_off = {0};
	char *line = NULL, *norm = NULL, **toks = NULL;
	size_t linecap = 0, normcap = 0, tokscap = 0;
	uint32_t n = 0;
	int rc = 1;

	ssize_t got;
	while ((got = getline (&line, &linecap, fp)) >= 0) {
		while (got > 0 &&
		       (line[got - 1] == '\n' || line[got - 1] == '\r'))
			line[--got] = 0;
		if (!*line || !strncmp (line, "path\t", 5))
			continue;

		/* path \t title \t year \t id \t score */
		char *f[5] = {line, NULL, NULL, NULL, NULL};
		int nf = 1;
		for (char *p = line; *p && nf < 5; p++)
			if (*p == '\t') {
				*p = 0;
				f[nf++] = p + 1;
			}
		const char *path = f[0];
		const char *title = nf > 1 ? f[1] : "";
		const char *year = nf > 2 ? f[2] : "";
		const char *id = nf > 3 ? f[3] : "";
		if (!*title)
			continue; /* Zeile ohne Treffer */

		char rec[4096];
		int rl = snprintf (rec, sizeof rec, "%s\t%s\t%s\t%s", title,
				   year, id, path);
		if (rl < 0 || (size_t)rl >= sizeof rec)
			continue;

		if (!push_u32 (&rec_off, (uint32_t)rec_blob.len))
			goto nomem;
		if (rec_blob.len + (size_t)rl + 1 > U32_LIMIT)
			goto nomem;
		if (!buf_put (&rec_blob, rec, (size_t)rl + 1))
			goto nomem;

		size_t need = imdb_norm (title, NULL, 0);
		if (need + 1 > normcap) {
			char *np = realloc (norm, need + 1);
			if (!np)
				goto nomem;
			norm = np;
			normcap = need + 1;
		}
		imdb_norm (title, norm, normcap);
		size_t maxt = need + 1;
		if (maxt > tokscap) {
			char **tp = realloc (toks, maxt * sizeof *tp);
			if (!tp)
				goto nomem;
			toks = tp;
			tokscap = maxt;
		}
		int nt = imdb_split (norm, toks, (int)maxt);
		for (int i = 0; i < nt; i++) {
			int dup = 0;
			for (int j = 0; j < i; j++)
				if (!strcmp (toks[j], toks[i])) {
					dup = 1;
					break;
				}
			if (dup)
				continue;
			struct occ o;
			if (tokblob.len >= U32_LIMIT)
				goto nomem;
			o.tok_off = (uint32_t)tokblob.len;
			o.rec = n;
			if (!buf_put (&tokblob, toks[i], strlen (toks[i]) + 1))
				goto nomem;
			if (!buf_put (&occ, &o, sizeof o))
				goto nomem;
		}
		n++;
	}
	free (line);
	line = NULL;
	fclose (fp);
	fp = NULL;

	if (n == 0) {
		fprintf (stderr, "movdb: keine Records gelesen\n");
		goto out;
	}
	if (!push_u32 (&rec_off, (uint32_t)rec_blob.len))
		goto nomem;

	g_blob = tokblob.p;
	qsort (occ.p, occ.len / sizeof (struct occ), sizeof (struct occ),
	       occ_cmp);

	uint32_t T = 0;
	if (!push_u32 (&dict_off, 0) || !push_u32 (&post_off, 0))
		goto nomem;
	struct occ *oa = (struct occ *)occ.p;
	size_t nocc = occ.len / sizeof *oa;
	for (size_t i = 0; i < nocc; i++) {
		if (i == 0 ||
		    strcmp ((const char *)tokblob.p + oa[i].tok_off,
			    (const char *)tokblob.p + oa[i - 1].tok_off)) {
			const char *s = (const char *)tokblob.p + oa[i].tok_off;
			if (T > 0) {
				if (!push_u32 (&dict_off, (uint32_t)dict.len) ||
				    !push_u32 (&post_off,
					       (uint32_t)(postings.len / 4)))
					goto nomem;
			}
			if (!buf_put (&dict, s, strlen (s) + 1))
				goto nomem;
			T++;
		}
		if (!push_u32 (&postings, oa[i].rec))
			goto nomem;
	}
	if (!push_u32 (&dict_off, (uint32_t)dict.len) ||
	    !push_u32 (&post_off, (uint32_t)(postings.len / 4)))
		goto nomem;

	uint64_t off[6];
	off[0] = sizeof (struct mov_header);
	off[1] = align8 (off[0] + (uint64_t)(n + 1) * 4);
	off[2] = align8 (off[1] + rec_blob.len);
	off[3] = align8 (off[2] + (uint64_t)(T + 1) * 4);
	off[4] = align8 (off[3] + dict.len);
	off[5] = align8 (off[4] + (uint64_t)(postings.len / 4) * 4);

	FILE *of = fopen (out, "wb");
	if (!of) {
		fprintf (stderr, "movdb: %s: %s\n", out, strerror (errno));
		goto out;
	}
	struct mov_header h;
	memset (&h, 0, sizeof h);
	memcpy (h.magic, MOVDB_MAGIC, 8);
	h.version = MOVDB_VERSION;
	h.n_rec = n;
	h.n_tok = T;
	h.n_post = (uint32_t)(postings.len / 4);
	memcpy (h.off, off, sizeof off);

	int ok = fwrite (&h, sizeof h, 1, of) == 1 &&
		 write_section (of, rec_off.p, (size_t)(n + 1) * 4) &&
		 write_section (of, rec_blob.p, rec_blob.len) &&
		 write_section (of, dict_off.p, (size_t)(T + 1) * 4) &&
		 write_section (of, dict.p, dict.len) &&
		 write_section (of, postings.p, postings.len) &&
		 write_section (of, post_off.p, (size_t)(T + 1) * 4);
	if (fclose (of) != 0)
		ok = 0;
	if (!ok) {
		fprintf (stderr, "movdb: Schreiben fehlgeschlagen: %s\n",
			 strerror (errno));
		goto out;
	}
	fprintf (stderr, "movdb: %s: N=%u T=%u P=%u\n", out, n, T, h.n_post);
	rc = 0;
	goto out;

nomem:
	fprintf (stderr, "movdb: Speicher erschoepft\n");
out:
	if (fp)
		fclose (fp);
	free (line);
	free (norm);
	free (toks);
	free (rec_off.p);
	free (rec_blob.p);
	free (tokblob.p);
	free (occ.p);
	free (dict.p);
	free (dict_off.p);
	free (postings.p);
	free (post_off.p);
	return rc;
}

/* ---- query ---- */
static int dict_find (const char *dict, const uint32_t *doff, uint32_t T,
		      const char *tok)
{
	int lo = 0, hi = (int)T - 1;
	while (lo <= hi) {
		int mid = lo + (hi - lo) / 2;
		int c = strcmp (tok, dict + doff[mid]);
		if (c == 0)
			return mid;
		if (c < 0)
			hi = mid - 1;
		else
			lo = mid + 1;
	}
	return -1;
}

static int query (const char *bin, int nterm, char **terms)
{
	int fd = open (bin, O_RDONLY);
	if (fd < 0) {
		fprintf (stderr, "movdb: %s: %s\n", bin, strerror (errno));
		return 1;
	}
	struct stat st;
	if (fstat (fd, &st) != 0 ||
	    (size_t)st.st_size < sizeof (struct mov_header)) {
		fprintf (stderr, "movdb: %s: zu klein\n", bin);
		close (fd);
		return 1;
	}
	void *m =
		mmap (NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	close (fd);
	if (m == MAP_FAILED) {
		fprintf (stderr, "movdb: mmap: %s\n", strerror (errno));
		return 1;
	}
	const struct mov_header *h = m;
	if (memcmp (h->magic, MOVDB_MAGIC, 8) || h->version != MOVDB_VERSION) {
		fprintf (stderr, "movdb: %s: kein movdb-Index\n", bin);
		munmap (m, (size_t)st.st_size);
		return 1;
	}
	const uint32_t *rec_off =
		(const uint32_t *)((const char *)m + h->off[0]);
	const char *rec_blob = (const char *)m + h->off[1];
	const uint32_t *dict_off =
		(const uint32_t *)((const char *)m + h->off[2]);
	const char *dict = (const char *)m + h->off[3];
	const uint32_t *post = (const uint32_t *)((const char *)m + h->off[4]);
	const uint32_t *post_off =
		(const uint32_t *)((const char *)m + h->off[5]);

	/* Ergebnis-Set als Bitset ueber Records. */
	unsigned char *hit = calloc ((h->n_rec + 7) / 8, 1);
	if (!hit) {
		munmap (m, (size_t)st.st_size);
		return 1;
	}
	int first = 1;
	int matched = 0;
	for (int t = 0; t < nterm; t++) {
		char norm[512];
		imdb_norm (terms[t], norm, sizeof norm);
		char *tok[64];
		int nt = imdb_split (norm, tok, 64);
		for (int q = 0; q < nt; q++) {
			int m2 = dict_find (dict, dict_off, h->n_tok, tok[q]);
			if (m2 < 0) {
				matched = 0;
				goto done;
			}
			unsigned char *cur = calloc ((h->n_rec + 7) / 8, 1);
			if (!cur) {
				matched = 0;
				goto done;
			}
			for (uint32_t k = post_off[m2]; k < post_off[m2 + 1];
			     k++) {
				uint32_t r = post[k];
				if (r < h->n_rec)
					cur[r >> 3] |=
						(unsigned char)(1u << (r & 7));
			}
			if (first) {
				memcpy (hit, cur, (h->n_rec + 7) / 8);
				first = 0;
			} else {
				for (uint32_t i = 0; i < (h->n_rec + 7) / 8;
				     i++)
					hit[i] &= cur[i];
			}
			free (cur);
		}
	}
	for (uint32_t r = 0; r < h->n_rec; r++)
		if (hit[r >> 3] & (1u << (r & 7))) {
			printf ("%s\n", rec_blob + rec_off[r]);
			matched++;
		}
done:
	free (hit);
	munmap (m, (size_t)st.st_size);
	fprintf (stderr, "%d Treffer\n", matched);
	return matched ? 0 : 1;
}

int main (int argc, char **argv)
{
	if (argc >= 2 && !strcmp (argv[1], "--selftest")) {
		char b[64];
		imdb_norm ("Mord im Mittsommer", b, sizeof b);
		printf ("norm: '%s'\n", b);
		return 0;
	}
	if (argc >= 4 && !strcmp (argv[1], "build"))
		return build (argv[2], argv[3]);
	if (argc >= 4 && !strcmp (argv[1], "query"))
		return query (argv[2], argc - 3, argv + 3);
	fprintf (stderr,
		 "usage: %s build <movies-guess.tsv> <out.bin>\n"
		 "       %s query <out.bin> <term>...\n",
		 argv[0], argv[0]);
	return 2;
}
