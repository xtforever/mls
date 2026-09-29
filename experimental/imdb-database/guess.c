/* Errate den Filmtitel aus Dateiname/Pfad.
 *
 * Die Kandidaten-Extraktion ist pluggable: jeder Algorithmus liefert nur
 * Suchkandidaten (+ Jahr-Hint + Serien-Hint), die Suche/Bewertung ist
 * gemeinsam. Auswahl mit --algo current|year|both.
 *
 *   current: Pfad-Komponenten normalisieren, Rauschen (Jahre, SxxExx,
 *            Aufloesung, Codecs, Release-Tags) entfernen.
 *   year:    nur Pfad-Chunks mit 4-stelliger Jahreszahl; Separatoren zu
 *            Spaces, ab Wort "german"/"ger" abschneiden (Anti-Pollution).
 *   both:    beide laufen lassen und pro Datei vergleichen.
 *
 * Aufruf: guess [imdb_index.bin] [movies-db] [topk] [--algo NAME] [--tsv]
 */
#include "imdb_db.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Rauschen, das in Filmtiteln praktisch nie vorkommt */
static const char *NOISE[] = {
	"x264",	      "x265",	    "h264",	"h265",	      "xvid",
	"divx",	      "hevc",	    "av1",	"aac",	      "ac3",
	"dts",	      "dts2",	    "dts5",	"flac",	      "mp3",
	"opus",	      "480p",	    "576p",	"720p",	      "1080p",
	"1440p",      "2160p",	    "4k",	"fhd",	      "uhd",
	"hd",	      "dvdrip",	    "dvd",	"bluray",     "blu",
	"bdrip",      "brrip",	    "webrip",	"web",	      "hdtv",
	"cam",	      "remux",	    "10bit",	"8bit",	      "hdr",
	"ddp5",	      "dd5",	    "german",	"deutsch",    "dt",
	"eng",	      "english",    "satrip",	"vhsrip",     "serien",
	"filme",      "film",	    "movie",	"movies",     "serie",
	"series",     "tv",	    "show",	"collection", "complete",
	"season",     "staffel",    "episode",	"eac3",	      "eac",
	"ddp",	      "truehd",	    "atmos",	"dtshd",      "webdl",
	"hdrip",      "subbed",	    "subs",	"multi",      "uncut",
	"extended",   "proper",	    "repack",	"internal",   "amzn",
	"dsnp",	      "hmax",	    "itunes",	"atvp",	      "dl",
	"dubbed",     "remastered", "remaster", "dub",	      "idtv",
	"omu",	      "rip",	    "vostfr",	"multisub",   "sub",
	"ws",	      "sd",	    "fs",	"hq",	      "dd",
	"3d",	      "de",	    "2ch",	"5ch",	      "6ch",
	"480",	      "576",	    "720",	"1080",	      "1440",
	"2160",	      "nfo",	    "bonus",	"extras",     "extra",
	"featurette", "trailer",    "sample",	"interview",  "deleted",
	"scenes",     "behind",	    NULL};

static int tsv_mode = 0;

static int is_year (const char *t)
{
	if (strlen (t) != 4)
		return 0;
	for (int i = 0; i < 4; i++)
		if (!isdigit ((unsigned char)t[i]))
			return 0;
	int y = atoi (t);
	return y >= 1900 && y <= 2099;
}

static int is_se (const char *t) /* s01e07, s1e7 */
{
	if (t[0] != 's')
		return 0;
	int i = 1, seen_e = 0, digits = 0;
	for (; t[i]; i++) {
		if (t[i] == 'e') {
			if (seen_e)
				return 0;
			seen_e = 1;
			continue;
		}
		if (!isdigit ((unsigned char)t[i]))
			return 0;
		digits++;
	}
	return seen_e && digits >= 2 && digits <= 4;
}

static int is_season_or_episode (const char *t) /* s01 / e07 */
{
	if (t[0] != 's' && t[0] != 'e')
		return 0;
	int i = 1, n = 0;
	for (; t[i]; i++) {
		if (!isdigit ((unsigned char)t[i]))
			return 0;
		n++;
	}
	return n >= 1 && n <= 3;
}

static int is_noise (const char *t)
{
	for (const char **p = NOISE; *p; p++)
		if (!strcmp (t, *p))
			return 1;
	return 0;
}
/* ---- Englische Zahlwoerter (0-999): "13" <-> "thirteen" ---- */
static const char *const NUM_WORDS[] = {
	"zero",	    "one",	"two",	    "three",   "four",	  "five",
	"six",	    "seven",	"eight",    "nine",    "ten",	  "eleven",
	"twelve",   "thirteen", "fourteen", "fifteen", "sixteen", "seventeen",
	"eighteen", "nineteen", "twenty",   NULL};
static const char *const TENS_WORDS[] = {"",	   "",	    "twenty", "thirty",
					 "forty",  "fifty", "sixty",  "seventy",
					 "eighty", "ninety"};

static int num_to_word (int n, char *out, size_t cap)
{
	if (n < 0 || n > 999)
		return 0;
	if (n <= 20) {
		snprintf (out, cap, "%s", NUM_WORDS[n]);
		return 1;
	}
	if (n < 100) {
		int t = n / 10, r = n % 10;
		if (r)
			snprintf (out, cap, "%s %s", TENS_WORDS[t],
				  NUM_WORDS[r]);
		else
			snprintf (out, cap, "%s", TENS_WORDS[t]);
		return 1;
	}
	int h = n / 100, r = n % 100;
	if (r) {
		char sub[64];
		num_to_word (r, sub, sizeof sub);
		snprintf (out, cap, "%s hundred %s", NUM_WORDS[h], sub);
	} else
		snprintf (out, cap, "%s hundred", NUM_WORDS[h]);
	return 1;
}

static int word_to_num (const char *w)
{
	for (int i = 0; i <= 20; i++)
		if (!strcmp (w, NUM_WORDS[i]))
			return i;
	for (int t = 2; t <= 9; t++)
		if (!strcmp (w, TENS_WORDS[t]))
			return t * 10;
	if (!strcmp (w, "hundred"))
		return 100;
	if (!strcmp (w, "thousand"))
		return 1000;
	return -1;
}

/* Ersetzt in cand Zahl-Tokens durch Zahlwoerter bzw. umgekehrt.
 * Rueckgabe 1, wenn sich etwas geaendert hat. */
static int convert_numbers (const char *in, char *out, size_t cap)
{
	int changed = 0;
	size_t o = 0;
	for (const char *p = in; *p;) {
		while (*p == ' ')
			p++;
		if (!*p)
			break;
		const char *e = p;
		while (*e && *e != ' ')
			e++;
		size_t len = (size_t)(e - p);
		char tok[64], rep[64];
		rep[0] = 0;
		if (len && len < sizeof tok) {
			memcpy (tok, p, len);
			tok[len] = 0;
			int alldig = 1;
			for (size_t i = 0; i < len; i++)
				if (!isdigit ((unsigned char)tok[i])) {
					alldig = 0;
					break;
				}
			if (alldig) {
				if (num_to_word (atoi (tok), rep, sizeof rep))
					changed = 1;
			} else {
				int v = word_to_num (tok);
				if (v >= 0) {
					snprintf (rep, sizeof rep, "%d", v);
					changed = 1;
				}
			}
		}
		if (!rep[0] && len < sizeof rep) {
			memcpy (rep, p, len);
			rep[len] = 0;
		}
		if (o && o + 1 < cap)
			out[o++] = ' ';
		for (const char *q = rep; *q && o + 1 < cap; q++)
			out[o++] = *q;
		p = e;
	}
	out[o] = 0;
	return changed;
}

/* Haengt an cands[ncan-1] eine Zahlwort-Variante an (falls vorhanden). */
static void add_num_variant (char cands[][1024], int *ncan, int max)
{
	if (*ncan < 1 || *ncan >= max)
		return;
	char v[1024];
	if (convert_numbers (cands[*ncan - 1], v, sizeof v)) {
		strncpy (cands[*ncan], v, sizeof cands[0] - 1);
		cands[*ncan][sizeof cands[0] - 1] = 0;
		(*ncan)++;
	}
}

/* Datums-Kategorieordner wie "19-12", "2024-01", "2024-01-15"
 * (nur 2-4-stellige Zahlengruppen mit Trennern) tragen keinen Titel. */
static int is_date_dir (const char *s)
{
	int groups = 0, d = 0;
	for (const char *p = s;; p++) {
		if (isdigit ((unsigned char)*p)) {
			d++;
			continue;
		}
		if (*p == 0) {
			if (d >= 2 && d <= 4)
				groups++;
			break;
		}
		if ((*p == '-' || *p == '_' || *p == '.') && d >= 2 && d <= 4) {
			groups++;
			d = 0;
			continue;
		}
		return 0; /* anderes Zeichen -> kein reiner Datumsordner */
	}
	return groups >= 2;
}

/* Sammelordner benennen eine Sammlung, nicht den einzelnen Film: Marker
 * wie "Collection"/"Sammlung". Ein Titel-Kandidat daraus ist oft der
 * Sammlungs-/Darstellername (z. B. "Belmondo") und darf den Dateinamen
 * nicht ueberstimmen. Bewusst NICHT enthalten: "Komplett"/"Complete"
 * (Staffel-Packs sind eine gueltige Titelquelle) und Jahresspannen
 * ("1979-1985", trifft sonst Titelzahlen wie "Blade Runner 2049"). */
static int is_collection_dir (const char *s)
{
	static const char *mark[] = {"collection", "sammlung",	"boxset",
				     "anthology",  "filmreihe", "werkschau",
				     NULL};
	char low[512];
	size_t n = strlen (s);
	if (n >= sizeof low)
		n = sizeof low - 1;
	for (size_t i = 0; i < n; i++)
		low[i] = (char)tolower ((unsigned char)s[i]);
	low[n] = 0;
	for (int i = 0; mark[i]; i++)
		if (strstr (low, mark[i]))
			return 1;
	return 0;
}

/* Rauschen aus Tokens filtern; *year = letztes gefundenes Jahrestoken.
 * Schreibt die verbleibenden Tokens space-getrennt nach out und gibt die
 * Laenge zurueck (0 = nur Rauschen bzw. Puffer zu klein). */
static int denoise (char **tok, int ntok, int *year, char *out, size_t cap)
{
	size_t o = 0;
	int n = 0;
	*year = 0;
	for (int i = 0; i < ntok; i++) {
		const char *t = tok[i];
		/* 1-Zeichen-Junk raus, aber kurze Titelwoerter wie "im",
		 * "it", "up" behalten (2-Zeichen-Noise steht in NOISE). */
		if (strlen (t) < 2)
			continue;
		if (is_year (t)) {
			*year = atoi (t);
			continue;
		}
		/* 2-stellige Zahl mit führender Null ("02", "07") ist ein
		 * Datums-/Episodenfragment, kein Titelwort. */
		if (strlen (t) == 2 && t[0] == '0' &&
		    isdigit ((unsigned char)t[1]))
			continue;
		if (is_se (t) || is_season_or_episode (t) || is_noise (t))
			continue;
		/* An Buchstaben/Ziffern-Grenzen zerlegen ("sg1" -> "sg",
		 * "1"), damit z. B. "Stargate SG1" den DB-Titel
		 * "Stargate SG-1" trifft. */
		char parts[8][64];
		int np = 0;
		size_t pl = 0;
		for (const char *p = t;; p++) {
			if (p > t && ((isdigit ((unsigned char)p[-1]) &&
				       isalpha ((unsigned char)*p)) ||
				      (isalpha ((unsigned char)p[-1]) &&
				       isdigit ((unsigned char)*p)))) {
				if (pl) {
					parts[np][pl] = 0;
					np++;
					pl = 0;
				}
			}
			if (!*p || np >= 8)
				break;
			if (pl + 1 < sizeof parts[0])
				parts[np][pl++] = *p;
		}
		if (pl && np < 8) {
			parts[np][pl] = 0;
			np++;
		}
		for (int j = 0; j < np; j++) {
			const char *s = parts[j];
			if (strlen (s) < 2 || is_noise (s))
				continue;
			if (n++) {
				if (o + 1 >= cap)
					return 0;
				out[o++] = ' ';
			}
			for (const char *p = s; *p; p++) {
				if (o + 1 >= cap)
					return 0;
				out[o++] = *p;
			}
		}
	}
	if (!cap)
		return 0;
	out[o] = 0;
	return (int)o;
}

/* Letzte bis zu 5 Pfad-Elemente: comp[0]=Basisname, comp[1]=davor, ... */
static void comps (const char *path, char comp[5][512], int *n)
{
	char cur[8192];
	strncpy (cur, path, sizeof cur - 1);
	cur[sizeof cur - 1] = 0;
	*n = 0;
	while (*n < 5) {
		char *slash = strrchr (cur, '/');
		size_t len = slash ? strlen (slash + 1) : strlen (cur);
		if (len == 0 || len >= 512)
			break;
		memcpy (comp[*n], slash ? slash + 1 : cur, len);
		comp[*n][len] = 0;
		(*n)++;
		if (!slash)
			break;
		*slash = 0;
		if (!*cur)
			break;
	}
}

static void strip_ext (char *b)
{
	char *dot = strrchr (b, '.');
	if (dot && strlen (dot) <= 5)
		*dot = 0;
}

/* Erkennt Serien-Marker im Pfad (SxxExx, s01, e07, Staffel/Season), um
 * bei gleichem Titelnamen die Serie statt z. B. eines gleichnamigen Films
 * zu bevorzugen. */
static int looks_series (const char *path)
{
	char norm[4096];
	char *tok[256];
	imdb_norm (path, norm, sizeof norm);
	int nt = imdb_split (norm, tok, 256);
	for (int i = 0; i < nt; i++) {
		if (is_se (tok[i]) || is_season_or_episode (tok[i]))
			return 1;
		if (!strcmp (tok[i], "staffel") || !strcmp (tok[i], "season"))
			return 1;
	}
	return 0;
}

/* Baut aus cand einen Kandidaten ohne die letzten `drop` Tokens. */
static int drop_tail (const char *cand, int drop, char *out, size_t cap)
{
	char buf[1024];
	size_t n = strlen (cand);
	if (n >= sizeof buf)
		n = sizeof buf - 1;
	memcpy (buf, cand, n);
	buf[n] = 0;
	char *t[64];
	int nt = 0;
	for (char *p = buf; *p && nt < 64;) {
		while (*p == ' ')
			p++;
		if (!*p)
			break;
		t[nt++] = p;
		while (*p && *p != ' ')
			p++;
		if (*p)
			*p++ = 0;
	}
	int use = nt - drop;
	if (use < 1)
		return 0;
	size_t o = 0;
	for (int j = 0; j < use; j++) {
		if (j && o + 1 < cap)
			out[o++] = ' ';
		for (const char *q = t[j]; *q && o + 1 < cap; q++)
			out[o++] = *q;
	}
	out[o] = 0;
	return 1;
}

/* Anzahl Tokens im DB-Titel; laengere Titel gewinnen bei Gleichstand. */
static int title_ntok (struct imdb_db *db, int rec)
{
	const char *t = imdb_title (db, rec);
	int n = 0;
	for (const char *p = t; *p;) {
		while (*p == ' ')
			p++;
		if (!*p)
			break;
		n++;
		while (*p && *p != ' ')
			p++;
	}
	return n;
}

/* Ist h (Key hk) besser als b (Key bk)? Prioritaet: Key, laengerer Titel,
 * Typ, Anzahl gematchter Tokens, IDF-Gewicht, dann bei Namenszwilling das
 * zum year_hint naechste (sonst neuere) Jahr. */
static int hit_better (struct imdb_db *db, int year_hint,
		       const struct imdb_hit *h, float hk,
		       const struct imdb_hit *b, float bk)
{
	/* Praktisch gleiche Keys (Float-Rauschen, exakter Prefix-Treffer vs.
	 * fast-exakter langer Titel) als Gleichstand behandeln, sonst
	 * entscheidet der Bruchteil vor den Tiebreaks. */
	float d = hk - bk;
	if (d > 2e-2f)
		return 1;
	if (d < -2e-2f)
		return 0;
	/* Serien-Marker im Pfad: Serien-Typ vor Titel-Laenge, damit z. B.
	 * "Mord im Mittsommer" (Serie) nicht einer gleichnamigen
	 * Episode-/Filmproduktion mit laengerem Titel unterliegt. */
	if (h->thit != b->thit)
		return h->thit > b->thit;
	if (bk >= 0.0f) {
		int ht = title_ntok (db, h->rec), bt = title_ntok (db, b->rec);
		if (ht != bt)
			return ht > bt;
	}
	if (h->thit != b->thit)
		return h->thit > b->thit;
	if (h->nmatch != b->nmatch)
		return h->nmatch > b->nmatch;
	if (h->weight != b->weight)
		return h->weight > b->weight;
	if (bk >= 0.0f &&
	    !strcmp (imdb_title (db, h->rec), imdb_title (db, b->rec))) {
		int hy = imdb_year (db, h->rec), by = imdb_year (db, b->rec);
		if (year_hint > 0) {
			int dh = abs (hy - year_hint);
			int db2 = abs (by - year_hint);
			if (dh != db2)
				return dh < db2;
		}
		return hy > by;
	}
	return 0;
}

/* ---- Algorithmus-Schnittstelle: liefert nur Suchkandidaten ---- */
typedef int (*cand_fn) (const char *path, char cands[][1024], int max,
			int *file_year, int *want_series);

struct algo {
	const char *name;
	cand_fn cand;
};

/* ---- Algorithmus 1: Pfad-Komponenten + Rauschfilter ---- */
static int cand_current (const char *path, char cands[][1024], int max,
			 int *file_year, int *want_series)
{
	char comp[5][512];
	int ncomp;
	comps (path, comp, &ncomp);
	strip_ext (comp[0]);
	*want_series = looks_series (path);
	*file_year = 0;
	int ncan = 0;
	for (int i = 0; i < ncomp && ncan < max; i++) {
		char norm[1024];
		char *tok[256];
		int year = 0;

		if (is_date_dir (comp[i]))
			continue;
		if (i >= 1 && is_collection_dir (comp[i]))
			continue;
		imdb_norm (comp[i], norm, sizeof norm);
		int nt = imdb_split (norm, tok, 256);
		int nc = denoise (tok, nt, &year, cands[ncan], sizeof cands[0]);
		if (nc <= 0)
			continue;
		int ntok2 = 1;
		for (const char *p = cands[ncan]; *p; p++)
			if (*p == ' ')
				ntok2++;
		/* Sammel-/Kategorieordner: Ein-Token-Kandidat aus einem
		 * VERZEICHNIS, dessen erstes Token ein Noise-Wort ist. */
		if (i >= 2 && ntok2 == 1 && nt > 0 && is_noise (tok[0]))
			continue;
		if (!*file_year && year > 0)
			*file_year = year;
		ncan++;
		add_num_variant (cands, &ncan, max);
		/* Fuehrendes Jahr kann der Titel selbst sein
		 * ("1923.YELLOSTONE", "2012.German..."), obwohl es sonst als
		 * Jahr gilt. */
		if (i >= 1 && nt > 0 && is_year (tok[0]) && ncan < max) {
			strncpy (cands[ncan], tok[0], sizeof cands[0] - 1);
			cands[ncan][sizeof cands[0] - 1] = 0;
			ncan++;
		}
	}
	return ncan;
}
/* ---- Algorithmus 2: Jahr-Chunk ----
 * Pfad an '/' zerlegen, nur Chunks mit 4-stelliger Jahreszahl verwenden.
 * In so einem Chunk '.', '_', '-' zu Spaces machen, alles lowercase und
 * ab dem Wort "german" bzw. "ger" den Rest verwerfen. Das Jahres-Token
 * selbst wird nicht in die Suchquery aufgenommen. */
static int find_year (const char *s)
{
	for (const char *p = s; *p; p++) {
		if (!isdigit ((unsigned char)*p))
			continue;
		if (p > s && isdigit ((unsigned char)p[-1]))
			continue;
		int n = 0;
		const char *q = p;
		while (isdigit ((unsigned char)*q)) {
			n++;
			q++;
		}
		if (n == 4) {
			int y = atoi (p);
			if (y >= 1900 && y <= 2099)
				return y;
		}
		p = q - 1;
	}
	return 0;
}

static int cand_year_impl (const char *path, char cands[][1024], int max,
			   int *file_year, int *want_series, int maxdepth)
{
	*want_series = looks_series (path);
	*file_year = 0;
	char cur[8192];
	strncpy (cur, path, sizeof cur - 1);
	cur[sizeof cur - 1] = 0;
	char *chunks[64];
	int nch = 0;
	for (char *p = cur; p && *p && nch < 64;) {
		char *slash = strchr (p, '/');
		chunks[nch++] = p;
		if (slash) {
			*slash = 0;
			p = slash + 1;
		} else
			break;
	}
	/* maxdepth > 0: nur die maxdepth tiefsten Chunks (nahe der Datei). */
	int start = (maxdepth > 0 && nch > maxdepth) ? nch - maxdepth : 0;
	int ncan = 0;
	for (int ci = start; ci < nch && ncan < max; ci++) {
		char *chunk = chunks[ci];
		int year = find_year (chunk);
		if (!year)
			continue;
		if (is_date_dir (chunk))
			continue; /* "2025-09": Jahr ist kein Filmjahr */
		if (ci != nch - 1 && is_collection_dir (chunk))
			continue; /* Sammelordner, kein Filmtitel */
		if (!*file_year)
			*file_year = year;
		char buf[1024];
		size_t o = 0;
		for (const char *q = chunk; *q && o + 1 < sizeof buf; q++) {
			char c = *q;
			if (c == '.' || c == '_' || c == '-')
				c = ' ';
			buf[o++] = (char)tolower ((unsigned char)c);
		}
		buf[o] = 0;
		char *tok[128];
		int nt = 0;
		for (char *r = buf; *r && nt < 128;) {
			while (*r == ' ')
				r++;
			if (!*r)
				break;
			tok[nt++] = r;
			while (*r && *r != ' ')
				r++;
			if (*r)
				*r++ = 0;
		}
		/* ab german/ger abschneiden, dann denselben Rauschfilter wie
		 * current (sonst matchen Tags wie "1080p" als Titel). */
		int nkeep = 0;
		for (int i = 0; i < nt; i++) {
			if (!strcmp (tok[i], "german") ||
			    !strcmp (tok[i], "ger"))
				break;
			tok[nkeep++] = tok[i];
		}
		int y2 = 0;
		int nc =
			denoise (tok, nkeep, &y2, cands[ncan], sizeof cands[0]);
		if (nc > 0) {
			ncan++;
			add_num_variant (cands, &ncan, max);
		}
	}
	return ncan;
}

static int cand_year (const char *path, char cands[][1024], int max,
		      int *file_year, int *want_series)
{
	return cand_year_impl (path, cands, max, file_year, want_series, -1);
}

/* Variante: nur Basisname + Elternordner (verhindert Sammel-Root-Jahre). */
static int cand_year_deep (const char *path, char cands[][1024], int max,
			   int *file_year, int *want_series)
{
	return cand_year_impl (path, cands, max, file_year, want_series, 2);
}

/* Kombination: Kandidaten beider Verfahren; der gemeinsame Vergleicher
 * nimmt den besten, daher mindestens so gut wie "current". */
static int cand_union (const char *path, char cands[][1024], int max,
		       int *file_year, int *want_series)
{
	int n = cand_current (path, cands, max, file_year, want_series);
	if (n < max) {
		int fy = 0, ws = 0;
		n += cand_year_deep (path, cands + n, max - n, &fy, &ws);
		if (!*file_year && fy > 0)
			*file_year = fy;
	}
	return n;
}

static const struct algo ALGOS[] = {
	{"current", cand_current},
	{"year", cand_year},
	{"year-deep", cand_year_deep},
	{"union", cand_union},
};
#define NALGO ((int)(sizeof ALGOS / sizeof ALGOS[0]))

struct guess_result {
	int hit;
	float score;
	int year;
	char title[256];
	char id[16];
};

/* Kandidaten eines Algorithmus suchen und besten Treffer liefern. */
static int run_algo (const char *path, struct imdb_db *db, int topk,
		     struct imdb_hit *hits, const struct algo *algo,
		     struct guess_result *res)
{
	char cands[8][1024];
	int file_year = 0, want_series = 0;
	int ncan = algo->cand (path, cands, 8, &file_year, &want_series);
	struct imdb_hit best = {0, -1, 0, 0, 0, 0};
	float best_key = -1;
	char *seen[64] = {0};
	int nseen = 0;
	int dbg = getenv ("GUESS_DEBUG") != NULL;
	if (dbg) {
		fprintf (stderr, "  ncan=%d want_series=%d year=%d:", ncan,
			 want_series, file_year);
		for (int i = 0; i < ncan; i++)
			fprintf (stderr, " [%s]", cands[i]);
		fprintf (stderr, "\n");
	}

	for (int i = 0; i < ncan; i++) {
		/* Vollen Kandidaten und Prefixe (min. 2 Tokens) suchen;
		 * Release-Tags/Gruppen am Ende wuerden sonst seltene
		 * Query-Tokens beisteuern. Bei Gleichstand gewinnt der
		 * laengere Titel (hit_better). */
		for (int drop = 0; drop <= 5; drop++) {
			char cc[1024];
			if (!drop_tail (cands[i], drop, cc, sizeof cc))
				break;
			if (drop > 0) {
				int nt2 = 1;
				for (const char *p = cc; *p; p++)
					if (*p == ' ')
						nt2++;
				if (nt2 < 2)
					break;
				/* Endet der Prefix auf einem Stoppwort,
				 * degeneriert die Query ("zwei vom" ->
				 * "zwei") und trifft Kurztitel exakt. */
				const char *last = strrchr (cc, ' ');
				if (last && imdb_is_stop (db, last + 1))
					continue;
			}
			int dup = 0;
			for (int j = 0; j < nseen; j++)
				if (!strcmp (seen[j], cc)) {
					dup = 1;
					break;
				}
			if (dup)
				continue;
			char *c = strdup (cc);
			if (c) {
				if (nseen < 64)
					seen[nseen++] = c;
				else
					free (c);
			}
			int cnt = imdb_search (db, cc, file_year, want_series,
					       topk, hits, topk);
			for (int k = 0; k < cnt; k++) {
				if (hits[k].score < IMDB_SCORE_MIN)
					continue;
				/* Mehrtoken-Kandidat: ein einzelnes Treffer-
				 * token (oft ein haeufiges Wort wie "zwei")
				 * reicht nicht; sonst matcht "zwei vom affen
				 * gebissen" den Kurztitel "Zwei". */
				int ctok = 1;
				for (const char *p = cc; *p; p++)
					if (*p == ' ')
						ctok++;
				if (ctok >= 3 && hits[k].nmatch < 2)
					continue;
				float key =
					hits[k].score +
					IMDB_YEAR_BONUS * (float)hits[k].yhit;
				if (dbg)
					fprintf (
						stderr,
						"  hit '%s' (%d) sc=%.3f nt=%d "
						"nm=%d th=%d key=%.3f\n",
						imdb_title (db, hits[k].rec),
						imdb_year (db, hits[k].rec),
						(double)hits[k].score,
						title_ntok (db, hits[k].rec),
						hits[k].nmatch, hits[k].thit,
						(double)key);
				if (hit_better (db, file_year, &hits[k], key,
						&best, best_key)) {
					best_key = key;
					best = hits[k];
				}
			}
		}
	}
	for (int j = 0; j < nseen; j++)
		free (seen[j]);

	memset (res, 0, sizeof *res);
	res->hit = best.score >= IMDB_SCORE_MIN;
	if (res->hit) {
		res->score = best.score;
		res->year = imdb_year (db, best.rec);
		snprintf (res->title, sizeof res->title, "%s",
			  imdb_title (db, best.rec));
		imdb_id (db, best.rec, res->id, sizeof res->id);
	}
	return res->hit;
}

static void year_str (int y, char *buf, size_t cap)
{
	buf[0] = 0;
	if (y > 0)
		snprintf (buf, cap, "%d", y);
}

static void out_plain (const char *tag, const struct guess_result *r,
		       const char *path)
{
	if (tag)
		printf ("[%s] ", tag);
	if (!r->hit) {
		printf ("-\n");
		return;
	}
	char ybuf[16];
	year_str (r->year, ybuf, sizeof ybuf);
	char full[300];
	if (r->year > 0)
		snprintf (full, sizeof full, "%s (%s)", r->title, ybuf);
	else
		snprintf (full, sizeof full, "%s", r->title);
	printf ("%.2f  %s %s  <=  %s\n", (double)r->score, full, r->id, path);
}

/* ---- Sammlungs-Prefixe entfernen ----
 * Verzeichnis-Ketten (Wurzel/Kategorieordner), die in einem grossen Teil
 * der Pfade vorkommen, sind kein Titel und verduennen die Suche. Ein
 * Vorab-Durchlauf zaehlt alle Ancestor-Verzeichnisse und schneidet je
 * Pfad die laengste Kette ab, die in >= mincnt Pfaden vorkommt. */
struct pkey {
	const char *s;
	int len;
};

static int pkey_cmp (const void *a, const void *b)
{
	const struct pkey *x = a, *y = b;
	int m = x->len < y->len ? x->len : y->len;
	int c = memcmp (x->s, y->s, (size_t)m);
	if (c)
		return c;
	return x->len - y->len;
}

static int uniq_find (const struct pkey *u, int nu, const char *s, int len)
{
	int lo = 0, hi = nu - 1;
	while (lo <= hi) {
		int mid = lo + (hi - lo) / 2;
		struct pkey k = {s, len};
		int c = pkey_cmp (&k, &u[mid]);
		if (c == 0)
			return mid;
		if (c < 0)
			hi = mid - 1;
		else
			lo = mid + 1;
	}
	return -1;
}

static void compute_prefix_cuts (char **paths, int n, int mincnt, int *cut)
{
	size_t cap = (size_t)n * 8 + 16;
	struct pkey *all = malloc (cap * sizeof *all);
	struct pkey *uniq = NULL;
	int *cnt = NULL;
	if (!all)
		goto out;
	size_t na = 0;
	for (int i = 0; i < n; i++) {
		const char *p = paths[i];
		for (const char *s = strchr (p, '/'); s;
		     s = strchr (s + 1, '/')) {
			if (na < cap) {
				all[na].s = p;
				all[na].len = (int)(s - p);
				na++;
			}
		}
	}
	qsort (all, na, sizeof *all, pkey_cmp);
	uniq = malloc ((na + 1) * sizeof *uniq);
	cnt = malloc ((na + 1) * sizeof *cnt);
	if (!uniq || !cnt)
		goto out;
	int nu = 0;
	for (size_t i = 0; i < na;) {
		size_t j = i + 1;
		while (j < na && all[j].len == all[i].len &&
		       !memcmp (all[j].s, all[i].s, (size_t)all[i].len))
			j++;
		uniq[nu] = all[i];
		cnt[nu] = (int)(j - i);
		nu++;
		i = j;
	}
	for (int i = 0; i < n; i++) {
		const char *p = paths[i];
		int best = 0;
		for (const char *s = strchr (p, '/'); s;
		     s = strchr (s + 1, '/')) {
			int len = (int)(s - p);
			int idx = uniq_find (uniq, nu, p, len);
			if (idx >= 0 && cnt[idx] >= mincnt && len > best)
				best = len;
		}
		cut[i] = best > 0 ? best + 1 : 0;
	}
out:
	free (all);
	free (uniq);
	free (cnt);
}

int main (int argc, char **argv)
{
	const char *dbp = NULL, *mvp = NULL;
	int topk = 10, pos = 0, run_all = 0;
	int algo_idx = NALGO - 1; /* Default: "union" (current + year-deep) */
	int strip_pct = 2;  /* Sammlungs-Prefixe ab 2% Haeufigkeit entfernen */
	int strip_only = 0; /* nur die gekuerzten Pfade ausgeben */
	for (int i = 1; i < argc; i++) {
		if (!strcmp (argv[i], "--tsv")) {
			tsv_mode = 1;
			continue;
		}
		if (!strcmp (argv[i], "--strip-prefix")) {
			if (i + 1 < argc)
				strip_pct = atoi (argv[++i]);
			continue;
		}
		if (!strcmp (argv[i], "--strip-only")) {
			strip_only = 1;
			continue;
		}
		if (!strcmp (argv[i], "--algo")) {
			if (i + 1 >= argc) {
				fprintf (stderr,
					 "--algo braucht einen Namen\n");
				return 1;
			}
			const char *nm = argv[++i];
			if (!strcmp (nm, "both") || !strcmp (nm, "all"))
				run_all = 1;
			else {
				algo_idx = -1;
				for (int a = 0; a < NALGO; a++)
					if (!strcmp (ALGOS[a].name, nm))
						algo_idx = a;
				if (algo_idx < 0) {
					fprintf (
						stderr,
						"unbekannter Algorithmus: %s\n",
						nm);
					return 1;
				}
			}
			continue;
		}
		if (pos == 0)
			dbp = argv[i];
		else if (pos == 1)
			mvp = argv[i];
		else if (pos == 2)
			topk = atoi (argv[i]);
		pos++;
	}
	if (!dbp)
		dbp = "imdb_index.bin";
	if (!mvp)
		mvp = "movies-db";
	if (topk < 1)
		topk = 1;

	struct imdb_db *db = imdb_open (dbp);
	if (!db) {
		fprintf (stderr, "imdb_open(%s): %s\n", dbp, strerror (errno));
		return 1;
	}
	FILE *mv = fopen (mvp, "r");
	if (!mv) {
		fprintf (stderr, "%s: nicht lesbar\n", mvp);
		imdb_close (db);
		return 1;
	}
	struct imdb_hit *hits = malloc ((size_t)topk * sizeof *hits);
	if (!hits) {
		fclose (mv);
		imdb_close (db);
		return 1;
	}

	long total = 0, matched = 0, magree = 0;
	long malgo[NALGO];
	for (int a = 0; a < NALGO; a++)
		malgo[a] = 0;

	if (tsv_mode) {
		if (run_all) {
			printf ("path");
			for (int a = 0; a < NALGO; a++)
				printf ("\t%s_title\t%s_year\t%s_id\t%s_score",
					ALGOS[a].name, ALGOS[a].name,
					ALGOS[a].name, ALGOS[a].name);
			printf ("\tagree\n");
		} else
			printf ("path\ttitle\tyear\tid\tscore\n");
	}

	/* Alle Pfade einlesen (Vorab-Durchlauf fuer die Prefix-Erkennung). */
	char **paths = NULL;
	int npath = 0, pcap = 0;
	char *line = NULL;
	size_t linecap = 0;
	ssize_t got;
	while ((got = getline (&line, &linecap, mv)) >= 0) {
		while (got > 0 &&
		       (line[got - 1] == '\n' || line[got - 1] == '\r'))
			line[--got] = 0;
		if (!*line)
			continue;
		if (npath == pcap) {
			int nc = pcap ? pcap * 2 : 256;
			char **np = realloc (paths, (size_t)nc * sizeof *np);
			if (!np)
				break;
			paths = np;
			pcap = nc;
		}
		paths[npath++] = strdup (line);
	}
	free (line);
	fclose (mv);

	int *cut = calloc ((size_t)npath + 1, sizeof *cut);
	if (cut && strip_pct > 0 && npath > 0) {
		int mincnt = npath * strip_pct / 100;
		if (mincnt < 20) /* kleine Eingaben: nur echte Sammlungen */
			mincnt = 20;
		compute_prefix_cuts (paths, npath, mincnt, cut);
	}

	if (strip_only) { /* nur gekuerzte Pfade (fuer parallele Laeufe) */
		for (int pi = 0; pi < npath; pi++)
			printf ("%s\n", paths[pi] + (cut ? cut[pi] : 0));
		for (int pi = 0; pi < npath; pi++)
			free (paths[pi]);
		free (paths);
		free (cut);
		free (hits);
		imdb_close (db);
		return 0;
	}

	for (int pi = 0; pi < npath; pi++) {
		const char *orig = paths[pi];
		const char *line = orig + (cut ? cut[pi] : 0);
		total++;

		if (run_all) {
			struct guess_result res[NALGO];
			for (int a = 0; a < NALGO; a++)
				run_algo (line, db, topk, hits, &ALGOS[a],
					  &res[a]);
			int agree = 1;
			for (int a = 0; a < NALGO; a++) {
				if (res[a].hit)
					malgo[a]++;
				if (res[a].hit != res[0].hit ||
				    (res[0].hit &&
				     strcmp (res[a].title, res[0].title)))
					agree = 0;
			}
			if (agree)
				magree++;
			if (tsv_mode) {
				printf ("%s", orig);
				for (int a = 0; a < NALGO; a++) {
					if (res[a].hit) {
						char yb[16];
						year_str (res[a].year, yb,
							  sizeof yb);
						printf ("\t%s\t%s\t%s\t%.2f",
							res[a].title, yb,
							res[a].id,
							(double)res[a].score);
					} else
						printf ("\t\t\t\t");
				}
				printf ("\t%d\n", agree);
			} else {
				for (int a = 0; a < NALGO; a++)
					out_plain (ALGOS[a].name, &res[a],
						   orig);
			}
		} else {
			struct guess_result r;
			if (run_algo (line, db, topk, hits, &ALGOS[algo_idx],
				      &r))
				matched++;
			if (tsv_mode) {
				if (r.hit) {
					char yb[16];
					year_str (r.year, yb, sizeof yb);
					printf ("%s\t%s\t%s\t%s\t%.2f\n", orig,
						r.title, yb, r.id,
						(double)r.score);
				} else
					printf ("%s\t\t\t\t\n", orig);
			} else
				out_plain (NULL, &r, orig);
		}
	}
	for (int pi = 0; pi < npath; pi++)
		free (paths[pi]);
	free (paths);
	free (cut);

	if (run_all) {
		fprintf (stderr, "matched %ld/%ld Dateien (", total, total);
		for (int a = 0; a < NALGO; a++)
			fprintf (stderr, "%s%s=%ld", a ? " " : "",
				 ALGOS[a].name, malgo[a]);
		fprintf (stderr, ", agree=%ld)\n", magree);
	} else
		fprintf (stderr, "matched %ld/%ld Dateien\n", matched, total);

	free (hits);
	imdb_close (db);
	return 0;
}
