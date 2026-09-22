#ifndef KWDB_H
#define KWDB_H

#include <stddef.h>
#include <stdint.h>

/* Keyword-index demo over the Project Gutenberg catalog.
   One compressed bitmap per keyword; bit position == book id.
   Requires the mls/bm library; kwdb_build() initialises both. */

typedef struct kwdb kwdb;

/* cap = max books to load (0 = all). Returns NULL on error. */
kwdb *kwdb_build (const char *csv_path, const char *kw_path, size_t cap);
void kwdb_free (kwdb *db);

size_t kwdb_books (const kwdb *db);
size_t kwdb_keywords (const kwdb *db);
const char *kwdb_title (const kwdb *db, size_t id);
const char *kwdb_author (const kwdb *db, size_t id);
const char *kwdb_subjects (const kwdb *db, size_t id);

const char *kwdb_keyword_name (const kwdb *db, size_t i);
const char *kwdb_keyword_patterns (const kwdb *db, size_t i);
int kwdb_keyword_index (const kwdb *db, const char *name);
int kwdb_keyword_bitmap (const kwdb *db, const char *name); /* handle, or -1 */
size_t kwdb_keyword_count (const kwdb *db, const char *name);

/* Evaluate a boolean keyword query. Returns a bitmap handle owned by the
   caller (bm_destroy) or -1 with a message in err. */
int kwdb_query (const kwdb *db, const char *query, char *err, size_t errlen);
size_t kwdb_bitmap_count (int bs);

/* case-insensitive substring match against a ';'-separated pattern list */
int kwdb_match (const char *haystack, const char *patterns);

#endif
