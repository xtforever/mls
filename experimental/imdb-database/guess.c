/* Entwurf "intelligente Suche": errate den Filmtitel aus Dateiname/Pfad.
 *
 * Vorgehen pro Datei:
 *   1. Die letzten bis zu 3 Pfad-Elemente (Basisname + 2 Elternverzeichnisse)
 *      werden als Kandidaten genommen - der echte Titel steckt
 * erfahrungsgemaess entweder im Verzeichnis oder im Dateinamen.
 *   2. Pro Kandidat: Tokens normalisieren (Kleinschreibung, Umlaute,
 *      Trennzeichen -> Whitespace) und Rauschen entfernen (Jahre, SxxExx,
 *      Aufloesung, Codecs, Quality-Tag, Release-Gruppen-Vokabular).
 *   3. Kandidat in der IMDb-DB suchen (imdb_search), bester Treffer >= 0.4
 *      wird ausgegeben.
 *
 * Aufruf: guess [imdb_index.bin] [movies-db] [topk] [--tsv]
 *   --tsv: Tabellenausgabe (TSV) fuer alle Zeilen:
 *          path<TAB>title<TAB>year<TAB>id<TAB>score (leere Felder ohne Treffer)
 */

#include "imdb_db.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Rauschen, das in Filmtiteln praktisch nie vorkommt */
static const char *NOISE[] = {
	"x264",	    "x265",    "h264",	  "h265",     "xvid",	    "divx",
	"hevc",	    "av1",     "aac",	  "ac3",      "dts",	    "dts2",
	"dts5",	    "flac",    "mp3",	  "opus",     "480p",	    "576p",
	"720p",	    "1080p",   "1440p",	  "2160p",    "4k",	    "fhd",
	"uhd",	    "hd",      "dvdrip",  "dvd",      "bluray",	    "blu",
	"bdrip",    "brrip",   "webrip",  "web",      "hdtv",	    "cam",
	"remux",    "10bit",   "8bit",	  "hdr",      "ddp5",	    "dd5",
	"german",   "deutsch", "dt",	  "eng",      "english",    "satrip",
	"vhsrip",   "serien",  "filme",	  "film",     "movie",	    "movies",
	"serie",    "series",  "tv",	  "show",     "collection", "complete",
	"season",   "staffel", "episode", "eac3",     "eac",	    "ddp",
	"truehd",   "atmos",   "dtshd",	  "webdl",    "hdrip",	    "subbed",
	"subs",	    "multi",   "uncut",	  "extended", "proper",	    "repack",
	"internal", "amzn",    "dsnp",	  "hmax",     "itunes",	    "atvp",
	"dl",	    "ws",      "sd",	  "fs",	      "hq",	    "dd",
	"3d",	    "de",      "2ch",	  "5ch",      "6ch",	    "480",
	"576",	    "720",     "1080",	  "1440",     "2160",	    "nfo",
	NULL};

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
		if (strlen (t) < 3)
			continue;
		if (is_year (t)) {
			*year = atoi (t);
			continue;
		}
		if (is_se (t) || is_season_or_episode (t) || is_noise (t))
			continue;
		if (n++) {
			if (o + 1 >= cap)
				return 0;
			out[o++] = ' ';
		}
		for (const char *p = t; *p; p++) {
			if (o + 1 >= cap)
				return 0;
			out[o++] = *p;
		}
	}
	if (!cap)
		return 0;
	out[o] = 0;
	return (int)o;
}

/* Letzte bis zu 3 Pfad-Elemente: comp[0]=Basisname, comp[1]=davor, ... */
static void comps (const char *path, char comp[3][512], int *n)
{
	char cur[8192];
	strncpy (cur, path, sizeof cur - 1);
	cur[sizeof cur - 1] = 0;
	*n = 0;
	while (*n < 3) {
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

static int guess_line (const char *path, struct imdb_db *db, int topk,
		       struct imdb_hit *hits)
{
	char comp[3][512];
	int ncomp;
	comps (path, comp, &ncomp);
	strip_ext (comp[0]);

	char *seen[3] = {0, 0, 0};
	int nseen = 0;
	struct imdb_hit best = {0, -1, 0, 0};
	float best_key = -1;

	/* Kandidaten sammeln; das Jahr wird dateiweit aus dem ersten
	 * gefundenen Jahrestoken bestimmt (Basisname zuerst), damit auch
	 * jahrlose Kandidaten (z. B. der Serien-Ordner "shogun") gegen das
	 * richtige Jahr aufgeloest werden. */
	char cands[3][1024];
	int ncan = 0, file_year = 0;
	for (int i = 0; i < ncomp; i++) {
		char norm[1024];
		char *tok[256];
		int year = 0;

		imdb_norm (comp[i], norm, sizeof norm);
		int nt = imdb_split (norm, tok, 256);
		int nc = denoise (tok, nt, &year, cands[ncan], sizeof cands[0]);
		if (nc <= 0)
			continue;
		if (!file_year && year > 0)
			file_year = year;
		ncan++;
	}

	for (int i = 0; i < ncan; i++) {
		int dup = 0;
		for (int j = 0; j < nseen; j++)
			if (!strcmp (seen[j], cands[i])) {
				dup = 1;
				break;
			}
		if (dup)
			continue;
		char *c = strdup (cands[i]);
		if (!c)
			continue;
		seen[nseen++] = c;

		int cnt = imdb_search (db, c, file_year, topk, hits, topk);
		for (int k = 0; k < cnt; k++) {
			if (hits[k].score < IMDB_SCORE_MIN)
				continue; /* zu schwach -> Jahr-Bonus greift
					     hier nicht */
			float key = hits[k].score +
				    IMDB_YEAR_BONUS * (float)hits[k].yhit;
			if (key > best_key ||
			    (key == best_key && hits[k].weight > best.weight)) {
				best_key = key;
				best = hits[k];
			}
		}
	}
	for (int j = 0; j < nseen; j++)
		free (seen[j]);

	if (best.score < IMDB_SCORE_MIN) {
		if (tsv_mode)
			printf ("%s\t\t\t\t\n", path);
		return 0;
	}
	int year = imdb_year (db, best.rec);
	char id[16];
	imdb_id (db, best.rec, id, sizeof id);
	if (tsv_mode) {
		char ybuf[16] = "";
		if (year > 0)
			snprintf (ybuf, sizeof ybuf, "%d", year);
		printf ("%s\t%s\t%s\t%s\t%.2f\n", path,
			imdb_title (db, best.rec), ybuf, id,
			(double)best.score);
		return 1;
	}
	char ybuf[16] = "";
	if (year > 0)
		snprintf (ybuf, sizeof ybuf, " (%d)", year);
	printf ("%.2f  %s%s %s  <=  %s\n", (double)best.score,
		imdb_title (db, best.rec), ybuf, id, path);
	return 1;
}

int main (int argc, char **argv)
{
	const char *dbp = NULL, *mvp = NULL;
	int topk = 3, pos = 0;
	for (int i = 1; i < argc; i++) {
		if (!strcmp (argv[i], "--tsv")) {
			tsv_mode = 1;
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

	long total = 0, matched = 0;
	if (tsv_mode)
		printf ("path\ttitle\tyear\tid\tscore\n");
	char *line = NULL;
	size_t linecap = 0;
	ssize_t got;
	while ((got = getline (&line, &linecap, mv)) >= 0) {
		while (got > 0 &&
		       (line[got - 1] == '\n' || line[got - 1] == '\r'))
			line[--got] = 0;
		if (!*line)
			continue;
		total++;
		if (guess_line (line, db, topk, hits))
			matched++;
	}
	free (line);
	fclose (mv);
	fprintf (stderr, "matched %ld/%ld Dateien\n", matched, total);

	free (hits);
	imdb_close (db);
	return 0;
}
