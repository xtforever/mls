#ifndef IMDB_DB_H
#define IMDB_DB_H

/* Minimale IMDb-Titelsuche ueber einen vorkompilierten mmap-Binaerindex
 * (siehe imdb_bin.h). Keine externen Abhaengigkeiten, nur POSIX.
 *
 * Suchprinzip: Inverted Token Index (Token -> CSR-Liste von Record-Indizes).
 * Ein Query-Token wird per O(log T)-Binaersuche im Token-Dictionary
 * gefunden, Kandidaten per Epoch-Merge gesammelt. Bewertung: IDF-gewichtete
 * Jaccard-Aehnlichkeit (Set-Semantik, jedes Token zaehlt je Titel einmal):
 *     idf(t) = log(1 + N/df(t))
 *     score  = Summe idf(gematcht) /
 *              (Summe idf(Query) + Summe idf(Titel) - Summe idf(gematcht))
 * exakter Titel = 1.0. Unbekannte Query-Tokens gehen mit idf_max in den
 * Query-Term ein und senken den Score.
 *
 * THREAD-SAFETY: imdb_search() nutzt mutablen Scratch-Speicher (mark/pos/
 * hits) und ist daher NICHT thread-safe. Mehrere Threads brauchen je ein
 * eigenes struct imdb_db. Alle read-only Zugriffe (imdb_title/year/id)
 * sind nach imdb_open() unveraenderlich.
 */

#include <stddef.h>
#include <stdint.h>

struct imdb_db;

struct imdb_hit {
	uint32_t rec; /* Record-Index in der DB */
	float score;  /* IDF-Jaccard, <= 1.0 */
	float weight; /* Summe IDF der gematchten Query-Tokens (Tiebreak) */
	uint8_t yhit; /* 1 = Record-Jahr passt zu year_hint */
};

/* Bonus fuer einen Jahres-Treffer bei der Kandidatenauswahl: gross genug,
 * um gleich gute Treffer verschiedener Jahre zu trennen, klein genug, um
 * einen klar besseren Titel ohne Jahres-Treffer nicht zu verdraengen.
 * Wirkt nur auf Treffer mit score >= IMDB_SCORE_MIN. */
#define IMDB_YEAR_BONUS 0.15f
#define IMDB_SCORE_MIN 0.4f

/* Mappt die Binaerindex-Datei read-only. NULL bei Fehler, errno gesetzt. */
struct imdb_db *imdb_open (const char *path);
void imdb_close (struct imdb_db *db);

/* Sucht query in db. Schreibt hoechstens min(topk,outcap) Treffer nach out
 * (nach score absteigend, Tiebreak rec aufsteigend) und gibt die Zahl der
 * geschriebenen Treffer zurueck. year_hint > 0 gibt Recordern mit diesem
 * Jahr einen Epsilon-Bonus. */
int imdb_search (const struct imdb_db *db, const char *query, int year_hint,
		 int topk, struct imdb_hit *out, int outcap);

/* Read-only Zugriffe. Bei rec ausserhalb [0,N): "" / 0 / "tt0000000". */
const char *imdb_title (const struct imdb_db *db, uint32_t rec);
int imdb_year (const struct imdb_db *db, uint32_t rec);
void imdb_id (const struct imdb_db *db, uint32_t rec, char *buf, size_t cap);

/* Normalisiert src byteweise (identisch zum frueheren mls-Imdb-Normalizer):
 * Umlaute/Akzente -> ASCII, alnum -> tolower, sonst Space. Ein ungemapptes
 * 0xC3-Folgepaar verbraucht BEIDE Bytes und emittiert EINEN Space.
 * Schreibt hoechstens cap-1 Zeichen + NUL nach dst und gibt die benoetigte
 * Laenge ohne NUL zurueck. dst darf NULL/cap 0 sein (reine Laengenmessung). */
size_t imdb_norm (const char *src, char *dst, size_t cap);

/* Splittet norm in-place an Space-Laeufen zu NUL-terminierten Tokens.
 * Leere Tokens werden uebersprungen. Schreibt hoechstens max Zeiger nach tok
 * und gibt die Tokenzahl zurueck. */
int imdb_split (char *norm, char **tok, int max);

#endif
