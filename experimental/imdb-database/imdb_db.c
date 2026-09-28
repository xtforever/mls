#include "imdb_db.h"
#include "imdb_bin.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

struct imdb_db {
	int fd;
	const uint8_t *base;
	size_t size;

	uint32_t n; /* Records */
	uint32_t t; /* Tokens */
	uint32_t p; /* Postings */

	const uint32_t *title_off; /* [n+1] */
	const char *titles;
	const uint32_t *ids;	  /* [n] */
	const uint16_t *years;	  /* [n] */
	const float *idf_sum;	  /* [n] Summe IDF der Titel-Tokens */
	const uint8_t *types;	  /* [n] Titeltyp (imdb_type) */
	const uint32_t *dict_off; /* [t+1] String-Offsets */
	const char *dict;
	const uint32_t *postings; /* [p] */
	const uint32_t *post_off; /* [t+1] CSR-Zeilenoffsets */

	/* Scratch fuer imdb_search (lazy, mutabel => nicht thread-safe) */
	uint32_t *mark;
	uint32_t *pos;
	uint32_t epoch;
	struct imdb_hit *hits;
	size_t hits_cap;
};

/* ---------------------------------------------------------------- normal */

/* ponytail: Normalisierung ist byteweise und deckt nur den laeufigen
 * 2-Byte-UTF-8-Bereich (0xC3-Block) ab; fremde Mehrbyte-Zeichen fallen
 * raus. Umlaute werden zu ae/oe/ue expandiert, damit "Koenig" und "Koenig"
 * zusammenfallen. Byteweise identisch zum frueheren mls-Normalizer.
 * Upgrade-Pfad: echte UTF-8-Decodierung + Locale-Foldings. */
size_t imdb_norm (const char *src, char *dst, size_t cap)
{
	const unsigned char *p = (const unsigned char *)src;
	size_t out = 0;

#define PUT(ch)                                                                \
	do {                                                                   \
		char _c = (char)(ch);                                          \
		if (dst && out + 1 < cap)                                      \
			dst[out] = _c;                                         \
		out++;                                                         \
	} while (0)
#define PUTS(s)                                                                \
	do {                                                                   \
		const char *_s = (s);                                          \
		while (*_s) {                                                  \
			PUT (*_s);                                             \
			_s++;                                                  \
		}                                                              \
	} while (0)

	while (*p) {
		int c = *p++;
		if (c == 0xC3 && *p) { /* 2-Byte-UTF-8, Block U+00C0..U+00FF */
			int c2 = *p++;
			switch (c2) {
			case 0x84: /* Ae */
			case 0xA4: /* ae */
			case 0x86: /* Ae */
			case 0xA6: /* ae */
				PUTS ("ae");
				continue;
			case 0x80: /* A` */
			case 0x81: /* A' */
			case 0x82: /* A^ */
			case 0x83: /* A~ */
			case 0x85: /* Aa */
			case 0xA0: /* a` */
			case 0xA1: /* a' */
			case 0xA2: /* a^ */
			case 0xA3: /* a~ */
			case 0xA5: /* aa */
				PUTS ("a");
				continue;
			case 0x87: /* C, */
			case 0xA7: /* c, */
				PUTS ("c");
				continue;
			case 0x88: /* E` */
			case 0x89: /* E' */
			case 0x8A: /* E^ */
			case 0x8B: /* E" */
			case 0xA8: /* e` */
			case 0xA9: /* e' */
			case 0xAA: /* e^ */
			case 0xAB: /* e" */
				PUTS ("e");
				continue;
			case 0x8C: /* I` */
			case 0x8D: /* I' */
			case 0x8E: /* I^ */
			case 0x8F: /* I" */
			case 0xAC: /* i` */
			case 0xAD: /* i' */
			case 0xAE: /* i^ */
			case 0xAF: /* i" */
				PUTS ("i");
				continue;
			case 0x90: /* D */
			case 0xB0: /* d */
				PUTS ("d");
				continue;
			case 0x91: /* N~ */
			case 0xB1: /* n~ */
				PUTS ("n");
				continue;
			case 0x92: /* O` */
			case 0x93: /* O' */
			case 0x94: /* O^ */
			case 0x95: /* O~ */
			case 0xB2: /* o` */
			case 0xB3: /* o' */
			case 0xB4: /* o^ */
			case 0xB5: /* o~ */
				PUTS ("o");
				continue;
			case 0x96: /* Oe */
			case 0x98: /* Oe */
			case 0xB6: /* oe */
			case 0xB8: /* oe */
				PUTS ("oe");
				continue;
			case 0x9C: /* Ue */
			case 0xBC: /* ue */
				PUTS ("ue");
				continue;
			case 0x99: /* U` */
			case 0x9A: /* U' */
			case 0x9B: /* U^ */
			case 0xB9: /* u` */
			case 0xBA: /* u' */
			case 0xBB: /* u^ */
				PUTS ("u");
				continue;
			case 0x9D: /* Y' */
			case 0xBD: /* y' */
			case 0xBF: /* y" */
				PUTS ("y");
				continue;
			case 0x9F: /* ss */
				PUTS ("ss");
				continue;
			}
			/* ungemapptes Paar: beide Bytes verbraucht, ein Space
			 */
		}
		if (isalnum (c))
			PUT (tolower (c));
		else
			PUT (' ');
	}

#undef PUT
#undef PUTS

	if (dst && cap)
		dst[out < cap ? out : cap - 1] = 0;
	return out;
}

int imdb_split (char *norm, char **tok, int max)
{
	int n = 0;
	char *p = norm;
	while (*p) {
		while (*p == ' ')
			p++;
		if (!*p)
			break;
		if (n >= max)
			break;
		tok[n++] = p;
		while (*p && *p != ' ')
			p++;
		if (*p)
			*p++ = 0;
	}
	return n;
}

/* ------------------------------------------------------------------ open */

static int sec_fits (const struct imdb_bin_header *h, size_t fsize, int idx,
		     uint64_t len)
{
	uint64_t o = h->off[idx];
	if (o % 8)
		return 0;
	if (o > fsize)
		return 0;
	if (len > (uint64_t)fsize - o)
		return 0;
	return 1;
}

struct imdb_db *imdb_open (const char *path)
{
	struct imdb_bin_header h;
	struct imdb_db *db = NULL;
	int fd = -1;
	void *map = MAP_FAILED;
	struct stat st;

	fd = open (path, O_RDONLY);
	if (fd < 0)
		return NULL;
	if (fstat (fd, &st) < 0 || st.st_size < 0)
		goto invalid;
	if ((uint64_t)st.st_size < sizeof h)
		goto invalid;
	map = mmap (NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (map == MAP_FAILED)
		goto fail;

	memcpy (&h, map, sizeof h);
	if (memcmp (h.magic, IMDB_BIN_MAGIC, 8) != 0)
		goto invalid;
	if (h.version != IMDB_BIN_VERSION)
		goto invalid;
	if (h.endian != IMDB_BIN_ENDIAN)
		goto invalid;
	if (h.n_records == 0 || h.n_records >= UINT32_MAX)
		goto invalid;
	if (h.n_tokens >= UINT32_MAX || h.n_postings >= UINT32_MAX)
		goto invalid;

	for (int i = 0; i < 9; i++)
		if (h.off[i] % 8)
			goto invalid;

	{
		uint64_t N = h.n_records, T = h.n_tokens, P = h.n_postings;
		uint64_t titles_len, dict_len;

		if (h.off[IMDB_SEC_TITLES] > h.off[IMDB_SEC_IDS])
			goto invalid; /* titles_len = off[2]-off[1] */
		if (h.off[IMDB_SEC_DICT] > h.off[IMDB_SEC_POSTINGS])
			goto invalid; /* dict_len = off[7]-off[6] */
		if (!sec_fits (&h, (size_t)st.st_size, IMDB_SEC_TITLE_OFF,
			       (N + 1) * 4))
			goto invalid;
		if (!sec_fits (&h, (size_t)st.st_size, IMDB_SEC_IDS, N * 4))
			goto invalid;
		if (!sec_fits (&h, (size_t)st.st_size, IMDB_SEC_YEARS, N * 2))
			goto invalid;
		if (!sec_fits (&h, (size_t)st.st_size, IMDB_SEC_IDF_SUM, N * 4))
			goto invalid;
		if (!sec_fits (&h, (size_t)st.st_size, IMDB_SEC_TYPES, N))
			goto invalid;
		if (!sec_fits (&h, (size_t)st.st_size, IMDB_SEC_DICT_OFF,
			       (T + 1) * 4))
			goto invalid;
		if (!sec_fits (&h, (size_t)st.st_size, IMDB_SEC_POSTINGS,
			       P * 4))
			goto invalid;
		if (!sec_fits (&h, (size_t)st.st_size, IMDB_SEC_POST_OFF,
			       (T + 1) * 4))
			goto invalid;

		titles_len = h.off[IMDB_SEC_IDS] - h.off[IMDB_SEC_TITLES];
		dict_len = h.off[IMDB_SEC_POSTINGS] - h.off[IMDB_SEC_DICT];
		if (!sec_fits (&h, (size_t)st.st_size, IMDB_SEC_TITLES,
			       titles_len))
			goto invalid;
		if (!sec_fits (&h, (size_t)st.st_size, IMDB_SEC_DICT, dict_len))
			goto invalid;

		db = calloc (1, sizeof *db);
		if (!db)
			goto fail;
		db->fd = fd;
		db->base = map;
		db->size = (size_t)st.st_size;
		db->n = h.n_records;
		db->t = h.n_tokens;
		db->p = h.n_postings;
		db->title_off = (const uint32_t *)(db->base +
						   h.off[IMDB_SEC_TITLE_OFF]);
		db->titles = (const char *)(db->base + h.off[IMDB_SEC_TITLES]);
		db->ids = (const uint32_t *)(db->base + h.off[IMDB_SEC_IDS]);
		db->years =
			(const uint16_t *)(db->base + h.off[IMDB_SEC_YEARS]);
		db->idf_sum =
			(const float *)(db->base + h.off[IMDB_SEC_IDF_SUM]);
		db->types = (const uint8_t *)(db->base + h.off[IMDB_SEC_TYPES]);
		db->dict_off =
			(const uint32_t *)(db->base + h.off[IMDB_SEC_DICT_OFF]);
		db->dict = (const char *)(db->base + h.off[IMDB_SEC_DICT]);
		db->postings =
			(const uint32_t *)(db->base + h.off[IMDB_SEC_POSTINGS]);
		db->post_off =
			(const uint32_t *)(db->base + h.off[IMDB_SEC_POST_OFF]);

		if ((uint64_t)db->title_off[db->n] != titles_len)
			goto invalid;
		for (uint32_t i = 0; i < db->n; i++)
			if (db->title_off[i] > db->title_off[i + 1])
				goto invalid;

		if (db->dict_off[0] != 0)
			goto invalid;
		if ((uint64_t)db->dict_off[db->t] != dict_len)
			goto invalid;
		for (uint32_t i = 0; i < db->t; i++)
			if (db->dict_off[i] > db->dict_off[i + 1])
				goto invalid;

		if (db->post_off[0] != 0)
			goto invalid;
		if ((uint64_t)db->post_off[db->t] != P)
			goto invalid;
		for (uint32_t i = 0; i < db->t; i++)
			if (db->post_off[i] > db->post_off[i + 1])
				goto invalid;
	}
	return db;

invalid:
	errno = EINVAL;
fail:
	if (map != MAP_FAILED)
		munmap (map, (size_t)st.st_size);
	if (fd >= 0)
		close (fd);
	free (db);
	return NULL;
}

void imdb_close (struct imdb_db *db)
{
	if (!db)
		return;
	free (db->mark);
	free (db->pos);
	free (db->hits);
	if (db->base)
		munmap ((void *)db->base, db->size);
	if (db->fd >= 0)
		close (db->fd);
	free (db);
}

/* ---------------------------------------------------------------- search */

static int dict_find (const struct imdb_db *db, const char *key)
{
	uint32_t lo = 0, hi = db->t;
	while (lo < hi) {
		uint32_t mid = lo + (hi - lo) / 2;
		int c = strcmp (key, db->dict + db->dict_off[mid]);
		if (c == 0)
			return (int)mid;
		if (c < 0)
			hi = mid;
		else
			lo = mid + 1;
	}
	return -1;
}

static int hits_reserve (struct imdb_db *db, size_t need)
{
	if (need <= db->hits_cap)
		return 1;
	size_t cap = db->hits_cap ? db->hits_cap : 64;
	while (cap < need)
		cap <<= 1;
	struct imdb_hit *p = realloc (db->hits, cap * sizeof *p);
	if (!p)
		return 0;
	db->hits = p;
	db->hits_cap = cap;
	return 1;
}

static float hit_key (const struct imdb_hit *h)
{
	if (h->score < IMDB_SCORE_MIN)
		return h->score;
	return h->score + IMDB_YEAR_BONUS * (float)h->yhit;
}

static int hit_cmp (const void *a, const void *b)
{
	const struct imdb_hit *x = a, *y = b;
	float kx = hit_key (x), ky = hit_key (y);
	if (kx < ky)
		return 1;
	if (kx > ky)
		return -1;
	/* Typ (Serie) nur als Tiebreak, nicht additiv - sonst verdraengt
	 * ein generischer Serientitel ("Star Trek") den spezifischeren. */
	if (x->thit != y->thit)
		return x->thit ? -1 : 1;
	/* Mehr gematchte Tokens = spezifischer (z. B. "Alice im Wunderland"
	 * mit 2 statt "Moria" mit 1); schlaegt das IDF-Gewicht, weil ein
	 * einzelnes seltenes Token sonst einen Mehrwort-Titel ueberstimmt. */
	if (x->nmatch != y->nmatch)
		return x->nmatch > y->nmatch ? -1 : 1;
	if (x->weight < y->weight)
		return 1;
	if (x->weight > y->weight)
		return -1;
	if (x->rec < y->rec)
		return -1;
	if (x->rec > y->rec)
		return 1;
	return 0;
}

int imdb_is_stop (const struct imdb_db *db, const char *tok)
{
	int m = dict_find (db, tok);
	if (m < 0)
		return 1; /* unbekannt: traegt nichts zum Score bei */
	uint32_t df = db->post_off[m + 1] - db->post_off[m];
	uint32_t cutoff = db->n / 20;
	if (!cutoff)
		cutoff = 1;
	return df > cutoff;
}

int imdb_search (const struct imdb_db *cdb, const char *query, int year_hint,
		 int want_series, int topk, struct imdb_hit *out, int outcap)
{
	if (topk <= 0 || outcap <= 0)
		return 0;

	struct imdb_db *db = (struct imdb_db *)cdb; /* Scratch ist mutabel */
	size_t qlen = strlen (query);
	char *norm = malloc (qlen + 1);
	char **tok = NULL;
	const char **uq = NULL;
	int *qmid = NULL;    /* dict-Index je nicht-stoppendem Query-Token */
	double *qidf = NULL; /* IDF-Gewicht je nicht-stoppendem Query-Token */
	int res = 0;
	int nq = 0, nqe = 0;

	if (!norm)
		return 0;
	{
		size_t need = imdb_norm (query, norm, qlen + 1);
		if (need >= qlen + 1) {
			char *n2 = realloc (norm, need + 1);
			if (!n2) {
				free (norm);
				return 0;
			}
			norm = n2;
			imdb_norm (query, norm, need + 1);
		}
	}

	tok = malloc ((qlen + 2) * sizeof *tok);
	uq = malloc ((qlen + 2) * sizeof *uq);
	qmid = malloc ((qlen + 2) * sizeof *qmid);
	qidf = malloc ((qlen + 2) * sizeof *qidf);
	if (!tok || !uq || !qmid || !qidf)
		goto done;

	int ntok = imdb_split (norm, tok, (int)(qlen + 1));
	for (int i = 0; i < ntok; i++) {
		const char *t = tok[i];
		if (!*t)
			continue;
		int dup = 0;
		for (int j = 0; j < nq; j++)
			if (!strcmp (uq[j], t)) {
				dup = 1;
				break;
			}
		if (!dup)
			uq[nq++] = t;
	}

	/* Stop-Filter + IDF. nqe zaehlt die nicht-stoppenden Query-Tokens
	 * fuer den Nenner - auch solche, die nicht im Dict stehen
	 * (unbekannt -> idf_max, senkt qcov). idf_Q = Summe der Gewichte. */
	double idf_max = log (1.0 + (double)db->n);
	double idf_Q = 0.0;
	{
		uint32_t cutoff = db->n / 20;
		if (!cutoff)
			cutoff = 1; /* Mini-DBs: sonst waere alles stoppend */
		for (int i = 0; i < nq; i++) {
			int m = dict_find (db, uq[i]);
			if (m >= 0) {
				uint32_t df =
					db->post_off[m + 1] - db->post_off[m];
				if (df > cutoff)
					continue; /* stoppend */
				qmid[nqe] = m;
				qidf[nqe] =
					log (1.0 + (double)db->n / (double)df);
			} else {
				qmid[nqe] = -1;
				qidf[nqe] = idf_max;
			}
			idf_Q += qidf[nqe];
			nqe++;
		}
	}

	if (nqe > 0 && idf_Q > 0.0) {
		if (!db->mark)
			db->mark = calloc (db->n, sizeof *db->mark);
		if (!db->pos)
			db->pos = calloc (db->n, sizeof *db->pos);
		if (!db->mark || !db->pos)
			goto done;

		if (db->epoch > 0x3fffffff) { /* Epoch ueberlauf: Marke neu */
			free (db->mark);
			free (db->pos);
			db->mark = calloc (db->n, sizeof *db->mark);
			db->pos = calloc (db->n, sizeof *db->pos);
			db->epoch = 1;
			if (!db->mark || !db->pos)
				goto done;
		}
		uint32_t epoch = ++db->epoch;
		size_t ncand = 0;

		for (int q = 0; q < nqe; q++) {
			if (qmid[q] < 0)
				continue; /* unbekanntes Token: keine Postings
					   */
			uint32_t m = (uint32_t)qmid[q];
			float wq = (float)qidf[q];
			uint32_t start = db->post_off[m];
			uint32_t end = db->post_off[m + 1];
			for (uint32_t k = start; k < end; k++) {
				uint32_t r = db->postings[k];
				if (r >= db->n)
					continue; /* Guard: korrupte Postings */
				if (db->mark[r] != epoch) {
					if (!hits_reserve (db, ncand + 1))
						goto done;
					db->mark[r] = epoch;
					db->pos[r] = (uint32_t)ncand;
					db->hits[ncand].rec = r;
					db->hits[ncand].score = 0.0f;
					db->hits[ncand].weight = wq;
					db->hits[ncand].nmatch = 1;
					ncand++;
				} else {
					struct imdb_hit *h =
						&db->hits[db->pos[r]];
					h->weight += wq;
					h->nmatch++;
				}
			}
		}

		for (size_t i = 0; i < ncand; i++) {
			uint32_t r = db->hits[i].rec;
			/* IDF-gewichtete Jaccard-Aehnlichkeit:
			 *   num   = Summe IDF der gematchten Query-Tokens
			 *   idf_T = Summe IDF aller Titel-Tokens (idf_sum)
			 *   score = num / (idf_Q + idf_T - num)
			 * Ungematchte Tokens strafen nach ihrer IDF: seltene
			 * (diskriminierende) stark, haeufige Artikel/Function-
			 * Words kaum. exakter Titel = 1.0. */
			double num = (double)db->hits[i].weight;
			double idf_T = (double)db->idf_sum[r];
			double denom = idf_Q + idf_T - num;
			double sc = denom > 0.0 ? num / denom : 0.0;
			if (sc > 1.0)
				sc = 1.0;
			db->hits[i].score = (float)sc;
			db->hits[i].yhit =
				(year_hint > 0 && db->years[r] > 0 &&
				 abs ((int)db->years[r] - year_hint) <= 1)
					? 1
					: 0;
			db->hits[i].thit = (want_series > 0 &&
					    IMDB_TYPE_IS_SERIES (db->types[r]))
						   ? 1
						   : 0;
		}

		if (ncand) {
			qsort (db->hits, ncand, sizeof *db->hits, hit_cmp);
			int nout = (int)ncand;
			if (nout > topk)
				nout = topk;
			if (nout > outcap)
				nout = outcap;
			memcpy (out, db->hits, (size_t)nout * sizeof *out);
			res = nout;
		}
	}

done:
	free (norm);
	free (tok);
	free (uq);
	free (qmid);
	free (qidf);
	return res;
}

/* ---------------------------------------------------------------- access */

const char *imdb_title (const struct imdb_db *db, uint32_t rec)
{
	if (rec >= db->n)
		return "";
	return db->titles + db->title_off[rec];
}

int imdb_year (const struct imdb_db *db, uint32_t rec)
{
	if (rec >= db->n)
		return 0;
	return db->years[rec];
}

uint8_t imdb_type (const struct imdb_db *db, uint32_t rec)
{
	if (rec >= db->n)
		return IMDB_T_OTHER;
	return db->types[rec];
}

void imdb_id (const struct imdb_db *db, uint32_t rec, char *buf, size_t cap)
{
	if (!buf || !cap)
		return;
	uint32_t v = rec < db->n ? db->ids[rec] : 0;
	if (v < 10000000u)
		snprintf (buf, cap, "tt%07u", v);
	else
		snprintf (buf, cap, "tt%u", v);
}
