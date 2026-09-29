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
#include "imdb_bin.h"
#include "imdb_db.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
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

/* ---- Zusatzinfo: separates mmap-File, Schluessel = IMDb-titel-id ----
 * Quellen (per tt-ID verknuepft): title.ratings (Rating/Stimmen) und
 * title.basics (Typ, Jahr, Laufzeit, Genres, Adult).
 *
 *   Header 24 B: magic "MOVMETA1", version, n, reserved
 *   n * movmeta_entry (24 B), nach id aufsteigend -> Binaersuche
 */
#define MOVMETA_MAGIC "MOVMETA1"
#define MOVMETA_VERSION 1u

struct movmeta_header {
	char magic[8];
	uint32_t version;
	uint32_t n;
	uint64_t reserved;
};

struct movmeta_entry {
	uint64_t id;	   /* numerischer tt-Teil */
	uint32_t votes;	   /* numVotes, 0 = unbekannt */
	uint32_t genres;   /* Bitmaske ueber GENRES */
	uint16_t year;	   /* startYear, 0 = unbekannt */
	uint16_t runtime;  /* Minuten, 0 = unbekannt */
	uint16_t rating10; /* averageRating*10, 0 = unbekannt */
	uint8_t type;	   /* enum imdb_type */
	uint8_t adult;
};

_Static_assert(sizeof (struct movmeta_header) == 24,
	       "movmeta_header muss 24 Byte sein");
_Static_assert(sizeof (struct movmeta_entry) == 24,
	       "movmeta_entry muss 24 Byte sein");

/* Bitindex = Position in GENRES; deckt alle IMDb-Genres ab. */
static const char *GENRES[] = {
	"Action",   "Adult",	 "Adventure",	"Animation", "Biography",
	"Comedy",   "Crime",	 "Documentary", "Drama",     "Family",
	"Fantasy",  "Film-Noir", "Game-Show",	"History",   "Horror",
	"Music",    "Musical",	 "Mystery",	"News",	     "Reality-TV",
	"Romance",  "Sci-Fi",	 "Short",	"Sport",     "Talk-Show",
	"Thriller", "War",	 "Western",	NULL};

static uint64_t parse_tt (const char *s)
{
	if (s[0] != 't' || s[1] != 't')
		return UINT64_MAX;
	uint64_t v = 0;
	for (const char *p = s + 2; *p; p++) {
		if (*p < '0' || *p > '9')
			return UINT64_MAX;
		v = v * 10 + (uint64_t)(*p - '0');
	}
	return v;
}

static int cmp_u64 (const void *a, const void *b)
{
	uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
	return x < y ? -1 : x > y ? 1 : 0;
}

static int ent_id_cmp (const void *k, const void *e)
{
	uint64_t a = *(const uint64_t *)k;
	uint64_t b = ((const struct movmeta_entry *)e)->id;
	return a < b ? -1 : a > b ? 1 : 0;
}

/* titleType-String -> enum imdb_type (wie download.sh tcode). */
static uint8_t type_code (const char *t)
{
	if (!strcmp (t, "movie"))
		return IMDB_T_MOVIE;
	if (!strcmp (t, "tvMovie"))
		return IMDB_T_TVMOVIE;
	if (!strcmp (t, "tvSeries"))
		return IMDB_T_TVSERIES;
	if (!strcmp (t, "miniSeries") || !strcmp (t, "tvMiniSeries"))
		return IMDB_T_MINISERIES;
	if (!strcmp (t, "tvShort"))
		return IMDB_T_TVSHORT;
	if (!strcmp (t, "tvSpecial"))
		return IMDB_T_TVSPECIAL;
	if (!strcmp (t, "short"))
		return IMDB_T_SHORT;
	if (!strcmp (t, "video"))
		return IMDB_T_VIDEO;
	if (!strcmp (t, "special"))
		return IMDB_T_SPECIAL;
	return IMDB_T_OTHER;
}

static uint32_t genre_mask (const char *s)
{
	uint32_t mask = 0;
	const char *p = s;
	while (*p) {
		const char *comma = strchr (p, ',');
		size_t len = comma ? (size_t)(comma - p) : strlen (p);
		for (int i = 0; GENRES[i]; i++)
			if (strlen (GENRES[i]) == len &&
			    !strncmp (GENRES[i], p, len)) {
				mask |= 1u << i;
				break;
			}
		if (!comma)
			break;
		p = comma + 1;
	}
	return mask;
}

static void genres_str (uint32_t mask, char *out, size_t cap)
{
	size_t o = 0;
	out[0] = 0;
	for (int i = 0; GENRES[i] && o + 1 < cap; i++) {
		if (!(mask & (1u << i)))
			continue;
		if (o) {
			if (o + 1 >= cap)
				break;
			out[o++] = ',';
		}
		size_t l = strlen (GENRES[i]);
		if (o + l >= cap)
			l = cap - 1 - o;
		memcpy (out + o, GENRES[i], l);
		o += l;
	}
	out[o] = 0;
}

/* .gz per zcat streamen, sonst direkt oeffnen. */
static FILE *open_stream (const char *path, int *piped)
{
	size_t n = strlen (path);
	if (n > 3 && !strcmp (path + n - 3, ".gz")) {
		char cmd[4200];
		snprintf (cmd, sizeof cmd, "zcat -- \"%s\"", path);
		*piped = 1;
		return popen (cmd, "r");
	}
	*piped = 0;
	return fopen (path, "r");
}

static int close_stream (FILE *fp, int piped)
{
	return piped ? pclose (fp) : fclose (fp);
}

/* Zerlegt eine Zeile in bis zu max Felder (in-place). Liefert Feldzahl. */
static int split_fields (char *line, char **f, int max)
{
	int nf = 1;
	f[0] = line;
	for (char *p = line; *p && nf < max; p++)
		if (*p == '\t') {
			*p = 0;
			f[nf++] = p + 1;
		}
	return nf;
}

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

/* ---- Zusatzinfo bauen ---- */
/* meta <movies-guess.tsv> <meta.bin> [ratings.tsv[.gz]] [basics.tsv[.gz]] */
static int meta_build (const char *tsv, const char *out, const char *ratings,
		       const char *basics)
{
	FILE *fp = fopen (tsv, "r");
	if (!fp) {
		fprintf (stderr, "movdb: %s: %s\n", tsv, strerror (errno));
		return 1;
	}
	struct buf ids = {0};
	char *line = NULL;
	size_t cap = 0;
	ssize_t got;
	int rc = 1;

	while ((got = getline (&line, &cap, fp)) >= 0) {
		while (got > 0 &&
		       (line[got - 1] == '\n' || line[got - 1] == '\r'))
			line[--got] = 0;
		if (!*line || !strncmp (line, "path\t", 5))
			continue;
		char *f[5];
		int nf = split_fields (line, f, 5);
		if (nf < 4)
			continue;
		uint64_t id = parse_tt (f[3]);
		if (id == UINT64_MAX)
			continue;
		if (!buf_put (&ids, &id, sizeof id))
			goto nomem;
	}
	free (line);
	line = NULL;
	fclose (fp);
	fp = NULL;

	size_t n = ids.len / sizeof (uint64_t);
	if (n == 0) {
		fprintf (stderr, "movdb: keine IDs in %s\n", tsv);
		goto out;
	}
	uint64_t *ia = (uint64_t *)ids.p;
	qsort (ia, n, sizeof *ia, cmp_u64);
	size_t m = 0;
	for (size_t i = 0; i < n; i++)
		if (i == 0 || ia[i] != ia[i - 1])
			ia[m++] = ia[i];
	n = m;

	struct movmeta_entry *ent = calloc (n, sizeof *ent);
	if (!ent)
		goto nomem;
	for (size_t i = 0; i < n; i++)
		ent[i].id = ia[i];

	if (ratings) {
		int piped = 0;
		FILE *rf = open_stream (ratings, &piped);
		if (!rf) {
			fprintf (stderr, "movdb: %s: %s\n", ratings,
				 strerror (errno));
			free (ent);
			goto out;
		}
		char *rl = NULL;
		size_t rcap = 0;
		ssize_t rgot;
		while ((rgot = getline (&rl, &rcap, rf)) >= 0) {
			while (rgot > 0 &&
			       (rl[rgot - 1] == '\n' || rl[rgot - 1] == '\r'))
				rl[--rgot] = 0;
			char *f[4];
			if (split_fields (rl, f, 4) < 3)
				continue;
			uint64_t id = parse_tt (f[0]);
			if (id == UINT64_MAX)
				continue;
			struct movmeta_entry *e =
				bsearch (&id, ent, n, sizeof *ent, ent_id_cmp);
			if (!e)
				continue;
			double r = atof (f[1]);
			e->rating10 = (uint16_t)(r * 10.0 + 0.5);
			e->votes = (uint32_t)strtoul (f[2], NULL, 10);
		}
		free (rl);
		if (close_stream (rf, piped) != 0) {
			fprintf (stderr, "movdb: %s: Lesefehler\n", ratings);
			free (ent);
			goto out;
		}
	}

	if (basics) {
		int piped = 0;
		FILE *bf = open_stream (basics, &piped);
		if (!bf) {
			fprintf (stderr, "movdb: %s: %s\n", basics,
				 strerror (errno));
			free (ent);
			goto out;
		}
		char *bl = NULL;
		size_t bcap = 0;
		ssize_t bgot;
		while ((bgot = getline (&bl, &bcap, bf)) >= 0) {
			while (bgot > 0 &&
			       (bl[bgot - 1] == '\n' || bl[bgot - 1] == '\r'))
				bl[--bgot] = 0;
			char *f[10];
			if (split_fields (bl, f, 10) < 9)
				continue;
			uint64_t id = parse_tt (f[0]);
			if (id == UINT64_MAX)
				continue;
			struct movmeta_entry *e =
				bsearch (&id, ent, n, sizeof *ent, ent_id_cmp);
			if (!e)
				continue;
			e->type = type_code (f[1]);
			e->adult = (uint8_t)(f[4][0] == '1');
			long y = atol (f[5]);
			e->year = (y > 0 && y <= 65535) ? (uint16_t)y : 0;
			long rt = atol (f[7]);
			e->runtime = (rt > 0 && rt <= 65535) ? (uint16_t)rt : 0;
			e->genres = genre_mask (f[8]);
		}
		free (bl);
		if (close_stream (bf, piped) != 0) {
			fprintf (stderr, "movdb: %s: Lesefehler\n", basics);
			free (ent);
			goto out;
		}
	}

	struct movmeta_header mh;
	memset (&mh, 0, sizeof mh);
	memcpy (mh.magic, MOVMETA_MAGIC, 8);
	mh.version = MOVMETA_VERSION;
	mh.n = (uint32_t)n;
	FILE *of = fopen (out, "wb");
	if (!of) {
		fprintf (stderr, "movdb: %s: %s\n", out, strerror (errno));
		free (ent);
		goto out;
	}
	int ok = fwrite (&mh, sizeof mh, 1, of) == 1 &&
		 fwrite (ent, sizeof *ent, n, of) == n;
	if (fclose (of) != 0)
		ok = 0;
	free (ent);
	if (!ok) {
		fprintf (stderr, "movdb: Schreiben fehlgeschlagen: %s\n",
			 strerror (errno));
		goto out;
	}
	fprintf (stderr, "movdb: %s: n=%zu\n", out, n);
	rc = 0;
	goto out;

nomem:
	fprintf (stderr, "movdb: Speicher erschoepft\n");
out:
	if (fp)
		fclose (fp);
	free (line);
	free (ids.p);
	return rc;
}

struct metamap {
	const struct movmeta_entry *e;
	uint32_t n;
	void *m;
	size_t len;
};

static int meta_open (const char *path, struct metamap *mm)
{
	int fd = open (path, O_RDONLY);
	if (fd < 0) {
		fprintf (stderr, "movdb: %s: %s\n", path, strerror (errno));
		return 0;
	}
	struct stat st;
	if (fstat (fd, &st) != 0 ||
	    (size_t)st.st_size < sizeof (struct movmeta_header)) {
		fprintf (stderr, "movdb: %s: zu klein\n", path);
		close (fd);
		return 0;
	}
	void *m =
		mmap (NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	close (fd);
	if (m == MAP_FAILED) {
		fprintf (stderr, "movdb: mmap: %s\n", strerror (errno));
		return 0;
	}
	const struct movmeta_header *h = m;
	if (memcmp (h->magic, MOVMETA_MAGIC, 8) ||
	    h->version != MOVMETA_VERSION) {
		fprintf (stderr, "movdb: %s: kein movmeta-File\n", path);
		munmap (m, (size_t)st.st_size);
		return 0;
	}
	mm->m = m;
	mm->len = (size_t)st.st_size;
	mm->e = (const struct movmeta_entry *)((const char *)m +
					       sizeof (struct movmeta_header));
	mm->n = h->n;
	return 1;
}

static void meta_close (struct metamap *mm) { munmap (mm->m, mm->len); }

static const struct movmeta_entry *meta_find (const struct metamap *mm,
					      uint64_t id)
{
	if (id == UINT64_MAX)
		return NULL;
	return bsearch (&id, mm->e, mm->n, sizeof *mm->e, ent_id_cmp);
}

static void meta_print (uint64_t id, const struct movmeta_entry *e)
{
	if (!e) {
		printf ("tt%07llu\t-\t-\t-\t-\t-\t-\t-\n",
			(unsigned long long)id);
		return;
	}
	char g[128];
	genres_str (e->genres, g, sizeof g);
	char rbuf[16] = "-";
	if (e->rating10)
		snprintf (rbuf, sizeof rbuf, "%.1f", e->rating10 / 10.0);
	printf ("tt%07llu\t%s\t%u\t%u\t%s\t%u\t%u\t%d\n",
		(unsigned long long)id, rbuf, e->votes, e->runtime,
		g[0] ? g : "-", e->type, e->year, e->adult);
}

/* get <meta.bin> <tt-id>... */
static int meta_get (const char *path, int nid, char **ids)
{
	struct metamap mm;
	if (!meta_open (path, &mm))
		return 1;
	for (int i = 0; i < nid; i++) {
		uint64_t id = parse_tt (ids[i]);
		meta_print (id, meta_find (&mm, id));
	}
	meta_close (&mm);
	return 0;
}

/* ---- Meta-Filter ---- */
struct qfilter {
	uint32_t genres; /* alle gesetzten Genre-Bits muessen passen */
	uint16_t min_rating10;
	uint32_t min_votes;
	uint16_t year;	   /* exakt, 0 = egal */
	uint16_t year_min; /* Jahresbereich, 0 = unbeschraenkt */
	uint16_t year_max;
	uint8_t type;
	int has_type;
	int has_range;
};

static int qfilter_active (const struct qfilter *q)
{
	return q->genres || q->min_rating10 || q->min_votes || q->year ||
	       q->has_type || q->has_range;
}

/* Genre-Name -> Bitindex, -1 = unbekannt. */
static int genre_bit (const char *name)
{
	for (int i = 0; GENRES[i]; i++)
		if (!strcasecmp (name, GENRES[i]))
			return i;
	return -1;
}

/* Typ-Name -> enum imdb_type, -1 = unbekannt. */
static int type_from_name (const char *s)
{
	static const struct {
		const char *n;
		uint8_t v;
	} T[] = {{"movie", IMDB_T_MOVIE},
		 {"tvmovie", IMDB_T_TVMOVIE},
		 {"tvseries", IMDB_T_TVSERIES},
		 {"series", IMDB_T_TVSERIES},
		 {"miniseries", IMDB_T_MINISERIES},
		 {"tvminiseries", IMDB_T_MINISERIES},
		 {"tvshort", IMDB_T_TVSHORT},
		 {"tvspecial", IMDB_T_TVSPECIAL},
		 {"short", IMDB_T_SHORT},
		 {"video", IMDB_T_VIDEO},
		 {"special", IMDB_T_SPECIAL},
		 {"other", IMDB_T_OTHER},
		 {NULL, 0}};
	for (int i = 0; T[i].n; i++)
		if (!strcasecmp (s, T[i].n))
			return T[i].v;
	return -1;
}

static int meta_match (const struct movmeta_entry *e, const struct qfilter *q)
{
	if (!e)
		return 0;
	if (q->genres && (e->genres & q->genres) != q->genres)
		return 0;
	if (q->min_rating10 && e->rating10 < q->min_rating10)
		return 0;
	if (q->min_votes && e->votes < q->min_votes)
		return 0;
	if (q->year && e->year != q->year)
		return 0;
	if (q->has_range) {
		if (e->year == 0)
			return 0; /* unbekanntes Jahr passt in keinen Bereich */
		if (q->year_min && e->year < q->year_min)
			return 0;
		if (q->year_max && e->year > q->year_max)
			return 0;
	}
	if (q->has_type && e->type != q->type)
		return 0;
	return 1;
}

/* Extrahiert das ID-Feld (tt...) aus einem Record in idbuf. */
static int rec_id (const char *rec, char *idbuf, size_t cap)
{
	const char *t1 = strchr (rec, '\t');
	const char *t2 = t1 ? strchr (t1 + 1, '\t') : NULL;
	const char *t3 = t2 ? strchr (t2 + 1, '\t') : NULL;
	if (!t1 || !t2 || !t3)
		return 0;
	const char *idp = t2 + 1;
	size_t l = (size_t)(t3 - idp);
	if (l >= cap)
		l = cap - 1;
	memcpy (idbuf, idp, l);
	idbuf[l] = 0;
	return 1;
}

/* ---- query ---- */
/* Gibt einen Record aus; mit Meta: title year id rating votes runtime
 * genres path, sonst den rohen Record. */
static void print_rec (const char *rec, const struct metamap *mm)
{
	if (!mm) {
		printf ("%s\n", rec);
		return;
	}
	const char *t1 = strchr (rec, '\t');
	const char *t2 = t1 ? strchr (t1 + 1, '\t') : NULL;
	const char *t3 = t2 ? strchr (t2 + 1, '\t') : NULL;
	if (!t1 || !t2 || !t3) {
		printf ("%s\n", rec);
		return;
	}
	const char *title = rec, *year = t1 + 1, *idp = t2 + 1;
	int ltitle = (int)(t1 - rec), lyear = (int)(t2 - year),
	    lid = (int)(t3 - idp);
	const char *path = t3 + 1;
	/* ID-Feld ist im Blob tab-terminiert, nicht NUL -> kopieren. */
	char idbuf[16];
	if (lid >= (int)sizeof idbuf)
		lid = (int)sizeof idbuf - 1;
	memcpy (idbuf, idp, (size_t)lid);
	idbuf[lid] = 0;
	const struct movmeta_entry *e = meta_find (mm, parse_tt (idbuf));
	char g[128] = "-", rbuf[16] = "-";
	unsigned votes = 0, runtime = 0;
	if (e) {
		genres_str (e->genres, g, sizeof g);
		if (!g[0])
			snprintf (g, sizeof g, "-");
		if (e->rating10)
			snprintf (rbuf, sizeof rbuf, "%.1f",
				  e->rating10 / 10.0);
		votes = e->votes;
		runtime = e->runtime;
	}
	printf ("%.*s\t%.*s\t%s\t%s\t%u\t%u\t%s\t%s\n", ltitle, title, lyear,
		year, idbuf, rbuf, votes, runtime, g, path);
}

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

static int query (const char *bin, const char *metapath, int nterm,
		  char **terms, const struct qfilter *q)
{
	if (qfilter_active (q) && !metapath) {
		fprintf (stderr,
			 "movdb: Meta-Filter brauchen --meta <meta.bin>\n");
		return 2;
	}
	if (nterm == 0 && !qfilter_active (q)) {
		fprintf (stderr, "movdb: kein Suchbegriff und kein Filter\n");
		return 2;
	}
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
	struct metamap mm;
	int have_meta = 0;
	if (metapath) {
		if (!meta_open (metapath, &mm)) {
			munmap (m, (size_t)st.st_size);
			return 1;
		}
		have_meta = 1;
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
	if (nterm == 0) { /* nur Filter: alle Records als Startmenge */
		memset (hit, 0xff, (h->n_rec + 7) / 8);
		first = 0;
	}
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
			const char *rec = rec_blob + rec_off[r];
			if (qfilter_active (q)) {
				char idb[16];
				const struct movmeta_entry *e = NULL;
				if (rec_id (rec, idb, sizeof idb))
					e = meta_find (&mm, parse_tt (idb));
				if (!meta_match (e, q))
					continue;
			}
			print_rec (rec, have_meta ? &mm : NULL);
			matched++;
		}
done:
	free (hit);
	if (have_meta)
		meta_close (&mm);
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
	if (argc >= 2 && !strcmp (argv[1], "meta")) {
		const char *tsv = NULL, *out = NULL, *ratings = NULL,
			   *basics = NULL;
		for (int i = 2; i < argc; i++) {
			if (!strcmp (argv[i], "--ratings") && i + 1 < argc)
				ratings = argv[++i];
			else if (!strcmp (argv[i], "--basics") && i + 1 < argc)
				basics = argv[++i];
			else if (!tsv)
				tsv = argv[i];
			else if (!out)
				out = argv[i];
		}
		if (!tsv || !out)
			goto usage;
		return meta_build (tsv, out, ratings, basics);
	}
	if (argc >= 4 && !strcmp (argv[1], "get"))
		return meta_get (argv[2], argc - 3, argv + 3);
	if (argc >= 2 && !strcmp (argv[1], "query")) {
		int i = 2;
		const char *meta = NULL;
		struct qfilter q;
		memset (&q, 0, sizeof q);
		while (i < argc) {
			if (!strcmp (argv[i], "--meta") && i + 1 < argc) {
				meta = argv[++i];
				i++;
			} else if (!strcmp (argv[i], "--genre") &&
				   i + 1 < argc) {
				int b = genre_bit (argv[++i]);
				if (b < 0) {
					fprintf (stderr,
						 "movdb: unbekanntes Genre "
						 "'%s'\n",
						 argv[i]);
					return 2;
				}
				q.genres |= 1u << b;
				i++;
			} else if (!strcmp (argv[i], "--min-rating") &&
				   i + 1 < argc) {
				q.min_rating10 =
					(uint16_t)(atof (argv[++i]) * 10.0 +
						   0.5);
				i++;
			} else if (!strcmp (argv[i], "--min-votes") &&
				   i + 1 < argc) {
				q.min_votes =
					(uint32_t)strtoul (argv[++i], NULL, 10);
				i++;
			} else if (!strcmp (argv[i], "--year") &&
				   i + 1 < argc) {
				q.year = (uint16_t)atoi (argv[++i]);
				i++;
			} else if (!strcmp (argv[i], "--year-range") &&
				   i + 1 < argc) {
				const char *s = argv[++i];
				const char *dash = strchr (s, '-');
				long a = (dash && dash != s) ? atol (s) : 0;
				long b =
					(dash && dash[1]) ? atol (dash + 1) : 0;
				if (!dash || (!a && !b) || a < 0 || b < 0 ||
				    a > 9999 || b > 9999 || (a && b && a > b)) {
					fprintf (stderr,
						 "movdb: --year-range A-B "
						 "erwartet, nicht '%s'\n",
						 s);
					return 2;
				}
				q.year_min = (uint16_t)a;
				q.year_max = (uint16_t)b;
				q.has_range = 1;
				i++;
			} else if (!strcmp (argv[i], "--type") &&
				   i + 1 < argc) {
				int t = type_from_name (argv[++i]);
				if (t < 0) {
					fprintf (
						stderr,
						"movdb: unbekannter Typ '%s'\n",
						argv[i]);
					return 2;
				}
				q.type = (uint8_t)t;
				q.has_type = 1;
				i++;
			} else {
				break;
			}
		}
		if (i >= argc)
			goto usage;
		return query (argv[i], meta, argc - i - 1, argv + i + 1, &q);
	}
usage:
	fprintf (stderr,
		 "usage: %s build <movies-guess.tsv> <out.bin>\n"
		 "       %s meta <movies-guess.tsv> <meta.bin> "
		 "[--ratings F] [--basics F]\n"
		 "       %s get <meta.bin> <tt-id>...\n"
		 "       %s query [--meta <meta.bin>] [--genre G] "
		 "[--min-rating R]\n"
		 "              [--min-votes N] [--year Y] "
		 "[--year-range A-B] [--type T]\n"
		 "              <out.bin> [term...]\n",
		 argv[0], argv[0], argv[0], argv[0]);
	return 2;
}
