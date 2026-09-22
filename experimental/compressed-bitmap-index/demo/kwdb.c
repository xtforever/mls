/* Keyword-index demo: Gutenberg catalog -> one bitmap per keyword. */
#include "kwdb.h"

#include "bm.h"
#include "mls.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

struct kw_entry {
	char *name;
	char *patterns;
	int bitmap;
};

struct kwdb {
	struct kw_entry *kws;
	size_t nkw;
	char **titles;
	char **authors;
	char **subjects; /* haystack: Subjects + ';' + Bookshelves */
	size_t nbooks, cap;
	int universe;
};

/* ---- small helpers ------------------------------------------------------ */

static void *xrealloc (void *p, size_t n)
{
	void *r = realloc (p, n);
	if (!r)
		ERR ("Out of Memory");
	return r;
}

static char *xstrdup (const char *s)
{
	char *r = strdup (s ? s : "");
	if (!r)
		ERR ("Out of Memory");
	return r;
}

static const char *ci_strstr (const char *h, const char *n)
{
	size_t nl = strlen (n);
	if (!nl)
		return h;
	for (; *h; h++)
		if (strncasecmp (h, n, nl) == 0)
			return h;
	return NULL;
}

int kwdb_match (const char *haystack, const char *patterns)
{
	if (!haystack || !patterns)
		return 0;
	const char *p = patterns;
	while (*p) {
		const char *semi = strchr (p, ';');
		size_t len = semi ? (size_t)(semi - p) : strlen (p);
		if (len) {
			char *tmp = malloc (len + 1);
			if (!tmp)
				ERR ("Out of Memory");
			memcpy (tmp, p, len);
			tmp[len] = 0;
			int m = ci_strstr (haystack, tmp) != NULL;
			free (tmp);
			if (m)
				return 1;
		}
		if (!semi)
			break;
		p = semi + 1;
	}
	return 0;
}

/* ---- CSV reader (RFC4180 subset: quotes, "" escapes, embedded newlines) -- */

static void field_push (char **buf, size_t *len, size_t *cap, char ch)
{
	if (*len + 1 > *cap) {
		*cap = *cap ? *cap * 2 : 16;
		*buf = xrealloc (*buf, *cap);
	}
	(*buf)[(*len)++] = ch;
}

/* read one record; 1 = ok, 0 = EOF, -1 = error. Caller frees fields. */
static int csv_next (FILE *fp, char ***fields, size_t *nfields, size_t *fcap)
{
	int c = getc (fp);
	if (c == EOF)
		return 0;
	size_t n = 0;
	for (;;) {
		char *buf = NULL;
		size_t len = 0, bcap = 0;
		if (c == '"') {
			c = getc (fp);
			while (c != EOF) {
				if (c == '"') {
					int d = getc (fp);
					if (d == '"') {
						field_push (&buf, &len, &bcap,
							    '"');
						c = getc (fp);
					} else {
						c = d;
						break;
					}
				} else {
					field_push (&buf, &len, &bcap, (char)c);
					c = getc (fp);
				}
			}
		} else {
			while (c != EOF && c != ',' && c != '\n' && c != '\r') {
				field_push (&buf, &len, &bcap, (char)c);
				c = getc (fp);
			}
		}
		field_push (&buf, &len, &bcap, '\0');
		if (n == *fcap) {
			*fcap = *fcap ? *fcap * 2 : 16;
			*fields = xrealloc (*fields, *fcap * sizeof (char *));
		}
		(*fields)[n++] = buf;
		if (c == '\r') {
			int d = getc (fp);
			if (d != '\n')
				ungetc (d, fp);
			c = '\n';
		}
		if (c == ',') {
			c = getc (fp);
			continue;
		}
		break;
	}
	*nfields = n;
	return 1;
}

static void fields_free (char **f, size_t n)
{
	for (size_t i = 0; i < n; i++)
		free (f[i]);
}

static int col_of (char **f, size_t n, const char *name)
{
	for (size_t i = 0; i < n; i++)
		if (strcmp (f[i], name) == 0)
			return (int)i;
	return -1;
}

/* ---- build -------------------------------------------------------------- */

static int load_keywords (kwdb *db, const char *path)
{
	FILE *fp = fopen (path, "r");
	if (!fp)
		return -1;
	char *line = NULL;
	size_t cap = 0;
	ssize_t n;
	while ((n = getline (&line, &cap, fp)) >= 0) {
		char *tab = strchr (line, '\t');
		if (!tab)
			continue; /* require <name>\t<patterns> */
		*tab = 0;
		char *name = line;
		char *pats = tab + 1;
		size_t pl = strlen (pats);
		while (pl && (pats[pl - 1] == '\n' || pats[pl - 1] == '\r'))
			pats[--pl] = 0;
		if (!*name || name[0] == '#')
			continue;
		db->kws = xrealloc (db->kws,
				    (db->nkw + 1) * sizeof (struct kw_entry));
		db->kws[db->nkw].name = xstrdup (name);
		db->kws[db->nkw].patterns = xstrdup (pats);
		db->kws[db->nkw].bitmap = bm_create ();
		if (db->kws[db->nkw].bitmap < 0)
			return -1;
		db->nkw++;
	}
	free (line);
	fclose (fp);
	return 0;
}

kwdb *kwdb_build (const char *csv_path, const char *kw_path, size_t cap)
{
	m_init ();
	if (bm_init () != 0)
		return NULL;

	FILE *fp = fopen (csv_path, "r");
	if (!fp)
		return NULL;

	kwdb *db = calloc (1, sizeof *db);
	if (!db) {
		fclose (fp);
		return NULL;
	}
	if (load_keywords (db, kw_path) != 0) {
		fclose (fp);
		kwdb_free (db);
		return NULL;
	}
	db->universe = bm_create ();

	char **f = NULL;
	size_t nf = 0, fcap = 0;
	if (csv_next (fp, &f, &nf, &fcap) != 1) {
		fclose (fp);
		kwdb_free (db);
		return NULL;
	}
	int ci_type = col_of (f, nf, "Type");
	int ci_title = col_of (f, nf, "Title");
	int ci_auth = col_of (f, nf, "Authors");
	int ci_subj = col_of (f, nf, "Subjects");
	int ci_shelf = col_of (f, nf, "Bookshelves");
	fields_free (f, nf);

	while (csv_next (fp, &f, &nf, &fcap) == 1) {
		if (cap && db->nbooks >= cap) {
			fields_free (f, nf);
			break;
		}
		if (ci_type < 0 || (size_t)ci_type >= nf ||
		    strcmp (f[ci_type], "Text") != 0) {
			fields_free (f, nf);
			continue;
		}
		if (db->nbooks == db->cap) {
			db->cap = db->cap ? db->cap * 2 : 1024;
			db->titles = xrealloc (db->titles,
					       db->cap * sizeof (char *));
			db->authors = xrealloc (db->authors,
						db->cap * sizeof (char *));
			db->subjects = xrealloc (db->subjects,
						 db->cap * sizeof (char *));
		}
		size_t id = db->nbooks;
		db->titles[id] = xstrdup (ci_title >= 0 && (size_t)ci_title < nf
						  ? f[ci_title]
						  : "");
		db->authors[id] = xstrdup (
			ci_auth >= 0 && (size_t)ci_auth < nf ? f[ci_auth] : "");
		const char *subj =
			ci_subj >= 0 && (size_t)ci_subj < nf ? f[ci_subj] : "";
		const char *shelf = ci_shelf >= 0 && (size_t)ci_shelf < nf
					    ? f[ci_shelf]
					    : "";
		size_t hl = strlen (subj) + strlen (shelf) + 2;
		char *hay = malloc (hl);
		if (!hay)
			ERR ("Out of Memory");
		snprintf (hay, hl, "%s;%s", subj, shelf);
		db->subjects[id] = hay;

		for (size_t k = 0; k < db->nkw; k++)
			if (kwdb_match (hay, db->kws[k].patterns))
				bm_set (db->kws[k].bitmap, (bm_bit_t)id);
		bm_set (db->universe, (bm_bit_t)id);
		db->nbooks++;

		fields_free (f, nf);
	}
	free (f);
	fclose (fp);
	return db;
}

void kwdb_free (kwdb *db)
{
	if (!db)
		return;
	for (size_t k = 0; k < db->nkw; k++) {
		free (db->kws[k].name);
		free (db->kws[k].patterns);
		bm_destroy (db->kws[k].bitmap);
	}
	free (db->kws);
	for (size_t i = 0; i < db->nbooks; i++) {
		free (db->titles[i]);
		free (db->authors[i]);
		free (db->subjects[i]);
	}
	free (db->titles);
	free (db->authors);
	free (db->subjects);
	bm_destroy (db->universe);
	free (db);
}

/* ---- accessors ---------------------------------------------------------- */

size_t kwdb_books (const kwdb *db) { return db->nbooks; }
size_t kwdb_keywords (const kwdb *db) { return db->nkw; }
const char *kwdb_title (const kwdb *db, size_t id)
{
	return id < db->nbooks ? db->titles[id] : "";
}
const char *kwdb_author (const kwdb *db, size_t id)
{
	return id < db->nbooks ? db->authors[id] : "";
}
const char *kwdb_subjects (const kwdb *db, size_t id)
{
	return id < db->nbooks ? db->subjects[id] : "";
}
const char *kwdb_keyword_name (const kwdb *db, size_t i)
{
	return i < db->nkw ? db->kws[i].name : "";
}
const char *kwdb_keyword_patterns (const kwdb *db, size_t i)
{
	return i < db->nkw ? db->kws[i].patterns : "";
}
int kwdb_keyword_index (const kwdb *db, const char *name)
{
	for (size_t k = 0; k < db->nkw; k++)
		if (strcasecmp (db->kws[k].name, name) == 0)
			return (int)k;
	return -1;
}
int kwdb_keyword_bitmap (const kwdb *db, const char *name)
{
	int i = kwdb_keyword_index (db, name);
	return i < 0 ? -1 : db->kws[i].bitmap;
}
size_t kwdb_keyword_count (const kwdb *db, const char *name)
{
	int i = kwdb_keyword_index (db, name);
	return i < 0 ? (size_t)-1 : kwdb_bitmap_count (db->kws[i].bitmap);
}

size_t kwdb_bitmap_count (int bs)
{
	size_t c = 0;
	int64_t b = bm_next1 (bs, 0);
	while (b >= 0) {
		c++;
		if (b == (int64_t)UINT32_MAX)
			break;
		b = bm_next1 (bs, (bm_bit_t)(b + 1));
	}
	return c;
}

/* ---- query parser / evaluator ------------------------------------------- */

enum { Q_END, Q_LP, Q_RP, Q_AND, Q_OR, Q_NOT, Q_WORD, Q_BAD };

typedef struct {
	const char *p;
	int type;
	char word[128];
	const kwdb *db;
	char *err;
	size_t errlen;
} qparser;

static void q_next (qparser *q)
{
	while (*q->p == ' ' || *q->p == '\t' || *q->p == '\n' || *q->p == '\r')
		q->p++;
	char c = *q->p;
	if (!c) {
		q->type = Q_END;
		return;
	}
	if (c == '(') {
		q->p++;
		q->type = Q_LP;
		return;
	}
	if (c == ')') {
		q->p++;
		q->type = Q_RP;
		return;
	}
	if (isalnum ((unsigned char)c) || c == '_' || c == '-') {
		size_t n = 0;
		while ((isalnum ((unsigned char)*q->p) || *q->p == '_' ||
			*q->p == '-') &&
		       n < sizeof (q->word) - 1)
			q->word[n++] = *q->p++;
		q->word[n] = 0;
		if (!strcasecmp (q->word, "and"))
			q->type = Q_AND;
		else if (!strcasecmp (q->word, "or"))
			q->type = Q_OR;
		else if (!strcasecmp (q->word, "not"))
			q->type = Q_NOT;
		else
			q->type = Q_WORD;
		return;
	}
	q->type = Q_BAD;
	snprintf (q->err, q->errlen, "unexpected character '%c'", c);
}

static int q_or (qparser *q);

static int q_bin (int op, int a, int b, const kwdb *db)
{
	if (a < 0 || b < 0) {
		if (a >= 0)
			bm_destroy (a);
		if (b >= 0)
			bm_destroy (b);
		return -1;
	}
	int r = op == Q_AND ? bm_and (0, a, b, 0, (bm_bit_t)db->nbooks)
			    : bm_or (0, a, b, 0, (bm_bit_t)db->nbooks);
	bm_destroy (a);
	bm_destroy (b);
	return r;
}

static int q_primary (qparser *q)
{
	if (q->type == Q_LP) {
		q_next (q);
		int r = q_or (q);
		if (r < 0)
			return -1;
		if (q->type != Q_RP) {
			bm_destroy (r);
			snprintf (q->err, q->errlen, "expected ')'");
			return -1;
		}
		q_next (q);
		return r;
	}
	if (q->type == Q_WORD) {
		int idx = kwdb_keyword_index (q->db, q->word);
		if (idx < 0) {
			snprintf (q->err, q->errlen, "unknown keyword '%s'",
				  q->word);
			return -1;
		}
		int r = bm_or (0, q->db->kws[idx].bitmap,
			       q->db->kws[idx].bitmap, 0,
			       (bm_bit_t)q->db->nbooks);
		q_next (q);
		return r;
	}
	snprintf (q->err, q->errlen, "expected keyword or '('");
	return -1;
}

static int q_not (qparser *q)
{
	if (q->type == Q_NOT) {
		q_next (q);
		int a = q_not (q);
		if (a < 0)
			return -1;
		int r = bm_xor (0, q->db->universe, a, 0,
				(bm_bit_t)q->db->nbooks);
		bm_destroy (a);
		return r;
	}
	return q_primary (q);
}

static int q_and (qparser *q)
{
	int r = q_not (q);
	while (q->type == Q_AND) {
		q_next (q);
		int b = q_not (q);
		r = q_bin (Q_AND, r, b, q->db);
		if (r < 0)
			return -1;
	}
	return r;
}

static int q_or (qparser *q)
{
	int r = q_and (q);
	while (q->type == Q_OR) {
		q_next (q);
		int b = q_and (q);
		r = q_bin (Q_OR, r, b, q->db);
		if (r < 0)
			return -1;
	}
	return r;
}

int kwdb_query (const kwdb *db, const char *query, char *err, size_t errlen)
{
	if (err && errlen)
		err[0] = 0;
	qparser q = {.p = query, .db = db, .err = err, .errlen = errlen};
	q_next (&q);
	if (q.type == Q_END) {
		snprintf (err, errlen, "empty query");
		return -1;
	}
	int r = q_or (&q);
	if (r < 0)
		return -1;
	if (q.type != Q_END) {
		bm_destroy (r);
		snprintf (err, errlen, "unexpected trailing input");
		return -1;
	}
	return r;
}
