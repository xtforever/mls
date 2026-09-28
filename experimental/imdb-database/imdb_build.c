/* imdb_build: kompiliert eine IMDb-Index-TSV (titleId<TAB>year<TAB>Titel)
 * in einen mmap-faehigen Binaerindex (Format siehe imdb_bin.h).
 *
 *   imdb_build <index.tsv> <out.bin>
 *   imdb_build --selftest
 *
 * Aufbau:
 *   1. Stream-Pass (getline): Titel/ID/Jahr puffern, Token-Vorkommen
 *      sammeln. Innerhalb eines Records werden Token DEDUPLIZIERT
 *      (Set-Semantik); ntoks = Anzahl distinkter Tokens.
 *   2. qsort der Vorkommen: primaer Tokenstring, sekundaer rec.
 *   3. CSR-Pass: Tokenwechsel -> neue Token-ID, Postings.
 *   4. Sektionen 8-aligned schreiben.
 *
 * Deterministisch: totaler Sortierschluessel + deterministische TSV =>
 * byte-identische Ausgabe (Idempotenz).
 */

#include "imdb_bin.h"
#include "imdb_db.h"

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define U32_LIMIT (1ULL << 32)

struct buf {
	unsigned char *p;
	size_t len, cap;
};

static int buf_reserve (struct buf *b, size_t extra)
{
	if (b->len + extra <= b->cap)
		return 1;
	size_t cap = b->cap ? b->cap : 256;
	while (cap < b->len + extra)
		cap <<= 1;
	unsigned char *p = realloc (b->p, cap);
	if (!p)
		return 0;
	b->p = p;
	b->cap = cap;
	return 1;
}

static int buf_put (struct buf *b, const void *src, size_t n)
{
	if (!buf_reserve (b, n))
		return 0;
	memcpy (b->p + b->len, src, n);
	b->len += n;
	return 1;
}

static int push_u32 (struct buf *b, uint32_t v) { return buf_put (b, &v, 4); }
static int push_u16 (struct buf *b, uint16_t v) { return buf_put (b, &v, 2); }
static int push_u8 (struct buf *b, uint8_t v) { return buf_put (b, &v, 1); }

static void buf_free (struct buf *b)
{
	free (b->p);
	b->p = NULL;
	b->len = b->cap = 0;
}

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
	if (x->tok_off != y->tok_off)
		return x->tok_off < y->tok_off ? -1 : 1;
	return 0;
}

static uint64_t parse_tt (const char *id)
{
	if (id[0] != 't' || id[1] != 't' || !id[2])
		return UINT64_MAX;
	uint64_t v = 0;
	for (const char *p = id + 2; *p; p++) {
		if (*p < '0' || *p > '9')
			return UINT64_MAX;
		v = v * 10 + (uint64_t)(*p - '0');
		if (v >= U32_LIMIT)
			return UINT64_MAX;
	}
	return v;
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

static int build (const char *tsv, const char *out)
{
	FILE *fp = fopen (tsv, "r");
	if (!fp) {
		fprintf (stderr, "imdb_build: %s: %s\n", tsv, strerror (errno));
		return 1;
	}

	struct buf titles = {0}, title_off = {0}, ids = {0}, years = {0},
		   types = {0}, tokblob = {0}, occ = {0}, dict = {0},
		   dict_off = {0}, postings = {0}, post_off = {0};
	char *line = NULL, *norm = NULL;
	char **toks = NULL;
	size_t linecap = 0, normcap = 0, tokscap = 0;
	uint32_t n = 0;
	float *idf_sum = NULL;
	int rc = 1;

	ssize_t got;
	while ((got = getline (&line, &linecap, fp)) >= 0) {
		while (got > 0 &&
		       (line[got - 1] == '\n' || line[got - 1] == '\r'))
			line[--got] = 0;

		char *t1 = strchr (line, '\t');
		if (!t1)
			continue;
		*t1 = 0;
		char *t2 = strchr (t1 + 1, '\t');
		if (!t2)
			continue;
		*t2 = 0;
		char *t3 = strchr (t2 + 1, '\t');
		const char *id = line, *ys = t1 + 1, *title;
		uint8_t type = 0;
		if (t3) { /* 4 Spalten: titleId year type title */
			*t3 = 0;
			type = (uint8_t)atoi (t2 + 1);
			title = t3 + 1;
		} else { /* 3 Spalten (Alt-TSV): type = 0 */
			title = t2 + 1;
		}
		if (!*id || !*title)
			continue;

		uint64_t idv = parse_tt (id);
		if (idv == UINT64_MAX) {
			fprintf (stderr,
				 "imdb_build: ungültige/zu große ID "
				 "'%s'\n",
				 id);
			goto out;
		}
		if (n >= UINT32_MAX) {
			fprintf (stderr, "imdb_build: N >= UINT32_MAX\n");
			goto out;
		}
		int year = atoi (ys);
		uint16_t yv = (year > 0 && year <= 65535) ? (uint16_t)year : 0;

		if (titles.len >= U32_LIMIT) {
			fprintf (stderr, "imdb_build: titles_len >= 2^32\n");
			goto out;
		}
		if (!push_u32 (&title_off, (uint32_t)titles.len))
			goto nomem;
		if (!buf_put (&titles, title, strlen (title) + 1))
			goto nomem;
		if (!push_u32 (&ids, (uint32_t)idv))
			goto nomem;
		if (!push_u16 (&years, yv))
			goto nomem;
		if (!push_u8 (&types, type))
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
		if (nt > 65535) {
			fprintf (stderr, "imdb_build: ntoks > 65535\n");
			goto out;
		}
		/* Set-Semantik: jedes Token je Record nur einmal aufnehmen. */
		for (int i = 0; i < nt; i++) {
			int dup = 0;
			for (int j = 0; j < i; j++)
				if (toks[j] && !strcmp (toks[j], toks[i])) {
					dup = 1;
					break;
				}
			if (dup)
				continue;
			struct occ o;
			if (tokblob.len >= U32_LIMIT) {
				fprintf (stderr,
					 "imdb_build: Tokenblob >= 2^32\n");
				goto out;
			}
			if (occ.len / sizeof o >= U32_LIMIT) {
				fprintf (stderr, "imdb_build: P >= 2^32\n");
				goto out;
			}
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
		fprintf (stderr, "imdb_build: keine Records gelesen\n");
		goto out;
	}

	/* Titelblob 8-aligned halten (damit off[2]-off[1] exakt titles_len
	 * ergibt) und finalen title_off[N] setzen. */
	while (titles.len % 8)
		if (!buf_put (&titles, "\0", 1))
			goto nomem;
	if (titles.len >= U32_LIMIT) {
		fprintf (stderr, "imdb_build: titles_len >= 2^32\n");
		goto out;
	}
	if (!push_u32 (&title_off, (uint32_t)titles.len))
		goto nomem;

	/* Vorkommen sortieren */
	g_blob = tokblob.p;
	qsort (occ.p, occ.len / sizeof (struct occ), sizeof (struct occ),
	       occ_cmp);

	/* CSR-Pass: dict_off = String-Offsets, post_off = Posting-Zeilen */
	uint32_t T = 0;
	if (!push_u32 (&dict_off, 0))
		goto nomem;
	if (!push_u32 (&post_off, 0))
		goto nomem;
	struct occ *oa = (struct occ *)occ.p;
	size_t nocc = occ.len / sizeof *oa;
	for (size_t i = 0; i < nocc; i++) {
		if (i == 0 ||
		    strcmp ((const char *)tokblob.p + oa[i].tok_off,
			    (const char *)tokblob.p + oa[i - 1].tok_off) != 0) {
			if (T >= UINT32_MAX) {
				fprintf (stderr,
					 "imdb_build: T >= UINT32_MAX\n");
				goto out;
			}
			const char *s = (const char *)tokblob.p + oa[i].tok_off;
			size_t sl = strlen (s) + 1;
			if (dict.len + sl > U32_LIMIT) {
				fprintf (stderr,
					 "imdb_build: dict_len >= 2^32\n");
				goto out;
			}
			if (T > 0) {
				if (!push_u32 (&dict_off, (uint32_t)dict.len))
					goto nomem;
				if (!push_u32 (&post_off,
					       (uint32_t)(postings.len / 4)))
					goto nomem;
			}
			if (!buf_put (&dict, s, sl))
				goto nomem;
			T++;
		}
		if (!push_u32 (&postings, oa[i].rec))
			goto nomem;
	}
	while (dict.len % 8)
		if (!buf_put (&dict, "\0", 1))
			goto nomem;
	if (dict.len >= U32_LIMIT) {
		fprintf (stderr, "imdb_build: dict_len >= 2^32\n");
		goto out;
	}
	if (!push_u32 (&dict_off, (uint32_t)dict.len))
		goto nomem;
	if (!push_u32 (&post_off, (uint32_t)(postings.len / 4)))
		goto nomem;

	/* IDF-Summen je Record: idf(t) = log(1 + N/df(t)); Summe ueber die
	 * distinkten Tokens eines Records. Grundlage der IDF-Jaccard-
	 * Bewertung im Reader (Artikel/Function-Words fallen kaum ins
	 * Gewicht, seltene Tokens dominieren). */
	idf_sum = calloc (n, sizeof *idf_sum);
	if (!idf_sum)
		goto nomem;
	{
		const uint32_t *po = (const uint32_t *)post_off.p;
		const uint32_t *post = (const uint32_t *)postings.p;
		/* Stop-Woerter (df > N/20) zaehlen nicht in idf_sum - sonst
		 * bestraft ein Artikel im Titel ("The Thing") den richtigen
		 * Treffer, obwohl er auf der Query-Seite herausgefiltert
		 * wird. Beidseitig neutral. */
		uint32_t cutoff = n / 20;
		if (!cutoff)
			cutoff = 1;
		for (uint32_t ti = 0; ti < T; ti++) {
			uint32_t df = po[ti + 1] - po[ti];
			if (df > cutoff)
				continue;
			float idf = (float)log (
				1.0 + (double)n / (double)(df ? df : 1));
			for (uint32_t k = po[ti]; k < po[ti + 1]; k++) {
				uint32_t r = post[k];
				if (r < n)
					idf_sum[r] += idf;
			}
		}
	}

	/* Offsets berechnen */
	uint64_t off[10];
	off[0] = sizeof (struct imdb_bin_header);
	off[1] = align8 (off[0] + (uint64_t)(n + 1) * 4);
	off[2] = align8 (off[1] + titles.len);
	off[3] = align8 (off[2] + (uint64_t)n * 4);
	off[4] = align8 (off[3] + (uint64_t)n * 2);
	off[5] = align8 (off[4] + (uint64_t)n * 4);
	off[6] = align8 (off[5] + (uint64_t)(T + 1) * 4);
	off[7] = align8 (off[6] + dict.len);
	off[8] = align8 (off[7] + (uint64_t)nocc * 4);
	off[9] = align8 (off[8] + (uint64_t)(T + 1) * 4);

	/* Schreiben */
	FILE *of = fopen (out, "wb");
	if (!of) {
		fprintf (stderr, "imdb_build: %s: %s\n", out, strerror (errno));
		goto out;
	}
	struct imdb_bin_header h;
	memset (&h, 0, sizeof h);
	memcpy (h.magic, IMDB_BIN_MAGIC, 8);
	h.version = IMDB_BIN_VERSION;
	h.endian = IMDB_BIN_ENDIAN;
	h.n_records = n;
	h.n_tokens = T;
	h.n_postings = (uint32_t)nocc;
	h.reserved = 0;
	memcpy (h.off, off, sizeof off);

	int ok = fwrite (&h, sizeof h, 1, of) == 1 &&
		 write_section (of, title_off.p, (size_t)(n + 1) * 4) &&
		 write_section (of, titles.p, titles.len) &&
		 write_section (of, ids.p, (size_t)n * 4) &&
		 write_section (of, years.p, (size_t)n * 2) &&
		 write_section (of, idf_sum, (size_t)n * 4) &&
		 write_section (of, dict_off.p, (size_t)(T + 1) * 4) &&
		 write_section (of, dict.p, dict.len) &&
		 write_section (of, postings.p, nocc * 4) &&
		 write_section (of, post_off.p, (size_t)(T + 1) * 4) &&
		 write_section (of, types.p, (size_t)n);
	if (fclose (of) != 0)
		ok = 0;
	if (!ok) {
		fprintf (stderr, "imdb_build: Schreiben fehlgeschlagen: %s\n",
			 strerror (errno));
		goto out;
	}

	fprintf (stderr, "imdb_build: %s: N=%u T=%u P=%zu\n", out, n, T, nocc);
	rc = 0;
	goto out;

nomem:
	fprintf (stderr, "imdb_build: Speicher erschoepft\n");
out:
	if (fp)
		fclose (fp);
	free (line);
	free (norm);
	free (toks);
	free (idf_sum);
	buf_free (&titles);
	buf_free (&title_off);
	buf_free (&ids);
	buf_free (&years);
	buf_free (&types);
	buf_free (&tokblob);
	buf_free (&occ);
	buf_free (&dict);
	buf_free (&dict_off);
	buf_free (&postings);
	buf_free (&post_off);
	return rc;
}

static void selftest (void)
{
	char b[64];

	/* Umlaute -> ae/oe/ue, ß -> ss */
	assert (imdb_norm ("äöüß", b, sizeof b) == 8);
	assert (!strcmp (b, "aeoeuess"));

	/* ungemapptes 0xC3-Folgepaar (U+00D7) -> EIN Space, beide Bytes weg */
	{
		char in[3] = {(char)0xC3, (char)0x97, 0};
		assert (imdb_norm (in, b, sizeof b) == 1);
		assert (!strcmp (b, " "));
	}

	/* Satzzeichen -> Space, alnum -> tolower */
	assert (imdb_norm ("Ab3-c.d!", b, sizeof b) == 8);
	assert (!strcmp (b, "ab3 c d "));

	/* reine Laengenmessung ohne Puffer */
	assert (imdb_norm ("ä", NULL, 0) == 2);
	assert (imdb_norm ("abc", NULL, 0) == 3);

	/* Split: Space-Laeufe, keine leeren Tokens */
	char s[] = "foo  bar baz";
	char *tok[8];
	int nt = imdb_split (s, tok, 8);
	assert (nt == 3);
	assert (!strcmp (tok[0], "foo"));
	assert (!strcmp (tok[1], "bar"));
	assert (!strcmp (tok[2], "baz"));

	char s2[] = "   ";
	assert (imdb_split (s2, tok, 8) == 0);
}

int main (int argc, char **argv)
{
	if (argc == 2 && !strcmp (argv[1], "--selftest")) {
		selftest ();
		puts ("selftest ok");
		return 0;
	}
	if (argc != 3) {
		fprintf (stderr, "usage: %s <index.tsv> <out.bin>\n", argv[0]);
		return 2;
	}
	return build (argv[1], argv[2]);
}
