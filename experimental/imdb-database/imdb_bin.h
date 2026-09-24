#ifndef IMDB_BIN_H
#define IMDB_BIN_H

/* Gemeinsames Binaerformat fuer Build-Tool (imdb_build.c) und Reader
 * (imdb_db.c). Little-Endian, x86-64, alle Sektionen 8-Byte-aligned.
 *
 *   Header (104 B):
 *     char     magic[8]   "IMDBBIN1"
 *     uint32_t version    1
 *     uint32_t endian     0x01020304
 *     uint32_t n_records  N
 *     uint32_t n_tokens   T
 *     uint32_t n_postings P
 *     uint32_t reserved   0
 *     uint64_t off[9]     Sektionsoffsets (off[8] = Start post_off)
 *
 *   Sektionen (Index = off[]-Index):
 *     0 title_off u32[N+1]   Offset ins Titelblob; letzter = titles_len
 *     1 titles    Bytes      NUL-terminierte Titel
 *     2 ids       u32[N]     numerischer tt-Teil
 *     3 years     u16[N]     Jahr (0 = unbekannt)
 *     4 idf_sum   f32[N]     Summe IDF der distinkten Tokens des Titels
 *     5 dict_off  u32[T+1]   Byte-Offset der Tokenstrings in dict
 *     6 dict      Bytes      NUL-terminierte Token, aufsteigend sortiert
 *     7 postings  u32[P]     CSR, je Token rec aufsteigend, je (Token,rec)
 *                            genau EINMAL (Set-Semantik)
 *     8 post_off  u32[T+1]   CSR-Zeilenoffset je Token in postings
 *
 * HINWEIS zum Auftragsformat: Die Auftragsbeschreibung nennt dict_off
 * zugleich als CSR-Zeilenoffset ("df = dict_off[i+1]-dict_off[i]") und
 * verlangt in der Validierung "dict_off[T]==dict_len" sowie eine
 * Binaersuche ueber dict. Das ist widerspruechlich: Ein Array kann nicht
 * gleichzeitig Byte-Offsets in den Stringblob und Posting-Anzahlen sein.
 * Aufgeloest wird das, indem dict_off die String-Offsets haelt (erfuellt
 * die Validierung + Binaersuche) und die CSR-Zeilenoffsets in der 9.
 * Sektion post_off liegen. off[9] ist im Auftrag als "Sektionsoffsets"
 * beschrieben, es waren nur 8 Sektionen benannt.
 */

#include <stdint.h>

#define IMDB_BIN_MAGIC "IMDBBIN1"
#define IMDB_BIN_VERSION 4u /* v4: idf_sum ohne Stop-Woerter */
#define IMDB_BIN_ENDIAN 0x01020304u

enum imdb_section {
	IMDB_SEC_TITLE_OFF = 0,
	IMDB_SEC_TITLES = 1,
	IMDB_SEC_IDS = 2,
	IMDB_SEC_YEARS = 3,
	IMDB_SEC_IDF_SUM = 4,
	IMDB_SEC_DICT_OFF = 5,
	IMDB_SEC_DICT = 6,
	IMDB_SEC_POSTINGS = 7,
	IMDB_SEC_POST_OFF = 8,
	IMDB_SEC_COUNT = 9
};

struct imdb_bin_header {
	char magic[8];
	uint32_t version;
	uint32_t endian;
	uint32_t n_records;
	uint32_t n_tokens;
	uint32_t n_postings;
	uint32_t reserved;
	uint64_t off[9];
};

_Static_assert(sizeof (uint32_t) == 4, "uint32_t muss 4 Byte sein");
_Static_assert(sizeof (uint16_t) == 2, "uint16_t muss 2 Byte sein");
_Static_assert(sizeof (uint64_t) == 8, "uint64_t muss 8 Byte sein");
_Static_assert(sizeof (struct imdb_bin_header) == 104,
	       "Header muss 104 Byte sein");

#endif
