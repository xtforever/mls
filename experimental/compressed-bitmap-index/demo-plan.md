# Keyword-index demo — plan & status

A book keyword index built on the chunked compressed bitmaps in `bm.c`:
one bitmap per keyword, bit position == book id. Boolean queries
(`and` / `or` / `not`, parentheses) evaluate directly on the bitmaps.

Status: **implemented and tested** (`demo/`).

## Dataset

**Project Gutenberg catalog** — `https://www.gutenberg.org/cache/epub/feeds/pg_catalog.csv`
(21 MB, public-domain metadata).

- 79,433 rows, **78,182 `Text` records** (verified 2026-09-21).
- Columns: `Text#,Type,Issued,Title,Language,Authors,Subjects,LoCC,Bookshelves`.
- `Subjects` and `Bookshelves` are `;`-separated; matching is a
  case-insensitive substring test against `Subjects + ";" + Bookshelves`.
- 42,342 distinct subjects, so the demo uses a curated 100-keyword
  dictionary (`demo/keywords.txt`) instead of the raw phrases.

Download (once):

```
curl -L -o pg_catalog.csv https://www.gutenberg.org/cache/epub/feeds/pg_catalog.csv
```

`pg_catalog.csv` and the built binaries are git-ignored.

## Data model

- `id = 0..N-1` in catalog order; bit position == id, so query results are
  book ids directly.
- 100 keyword bitmaps + a `universe` bitmap (all ids) used for `not`.
- Side table `id -> title, author` for rendering.

## Build

`kwdemo stats` parses the CSV (RFC4180 subset: quotes, `""`, embedded
newlines/commas), assigns ids, and for each book/keyword does
`bm_set(kw_bitmap, id)`. Rebuilds in-process on every invocation; no
serialization needed at this size.

## Query engine

Recursive-descent parser, precedence `not` > `and` > `or`:

```
expr := or_expr
or   := and ('or' and)*
and  := not ('and' not)*
not  := 'not' not | primary
primary := keyword | '(' expr ')'
```

Evaluation (range `[0,N)`):
- `and` → `bm_and(0,A,B,0,N)`, `or` → `bm_or(...)`
- `not X` → `bm_xor(0,universe,X,0,N)`
- a keyword operand is copied so stored bitmaps are never mutated
- results enumerated with `bm_next1`

## CLI

```
kwdemo stats    <csv> <kw> [cap]
kwdemo keywords <csv> <kw> [cap]
kwdemo match    <csv> <kw> <keyword> [cap]
kwdemo query    <csv> <kw> "<expr>" [cap]
```

## Files

```
demo/kwdb.c kwdb.h   CSV reader, keyword matching, build, parser/evaluator
demo/kwdemo.c        CLI
demo/keywords.txt    100 keyword -> pattern dictionary
demo/test_kwdb.c     synthetic + brute-force tests
demo/bigindex.c      compression-ratio benchmark
demo/makefile
```

## Tests run

`demo/test_kwdb` (ASan/UBSan, leak-check on):

- **synthetic** CSV: 6 books, exact counts, CSV quoting (embedded comma,
  escaped quote), query semantics, `and`> `or` precedence, `not`, case-insensitive
  keywords/operators, and error cases (unknown keyword, dangling `and`,
  trailing input, unbalanced `(`, bad char, empty query).
- **brute-force cross-check** on the first 3,000 real books: five queries
  compared bit-for-bit against a scalar scan (`bm_test` vs predicate) — all match.

Full-data spot check (independent Python reference over all 78,182 books):

```
love-story and roman and not vampire   -> 2 hits  (ids 2745 "Undine", 71877 "Love")
```

## Observed numbers (full catalog)

```
books:            78182
keywords:         100 (100 non-empty)
total memberships:210147 (avg 2101.5/keyword)
largest keyword:  30768 books
build time:       6.4 s
query eval:       ~3-5 ms
```

## Compression benchmark (bigindex)

Gutenberg (78K bits) is far too small to say anything about compression. For
that we use **MovieLens 25M** — `https://files.grouplens.org/datasets/movielens/ml-25m.zip`
(262 MB, unzip `ratings.csv`): **25,000,095 ratings**, 162,541 users,
59,047 movies, `userId` non-decreasing in file order.

`demo/bigindex <csv> --col N [--sort] [--limit N] [--verify]` builds one bitmap
per distinct integer key and reports the index size against the dense
(`K x N` bits) equivalent. `--sort` assigns ids by a stable counting sort on
the key, so each key becomes one contiguous id run. `--verify` samples 200 keys
and checks bitmap counts (and, when sorted, the exact id range).

Measured (RSS of the index; dense = K bitmaps x N bits). One-word chunks are
stored **inline** in the chunk (no MLS handle), so a scattered isolated bit
costs ~16-26 bytes instead of a whole container:

| dataset / ordering | rows | keys | dense | index (RSS) | chunks | ratio |
|---|---:|---:|---:|---:|---:|---:|
| MovieLens `userId`, file order | 25,000,095 | 162,541 | 473.06 GiB | 44.02 MiB | 162,541 | **11,006x** |
| MovieLens `movieId`, sorted | 25,000,095 | 59,047 | 171.85 GiB | 15.54 MiB | 59,053 | **11,328x** |
| MovieLens `movieId`, file order | 25,000,095 | 59,047 | 171.85 GiB | 597.43 MiB | 23,770,752 | 294.6x |
| MovieLens `movieId`, file order | 2,000,000 | 27,321 | 6.36 GiB | 52.41 MiB | 1,899,838 | 124.3x |
| MovieLens `userId`, file order | 2,000,000 | 13,322 | 3.10 GiB | 3.75 MiB | 13,322 | 848x |

`--verify` passed on every run. Build times: 3.9 s (userId 25M), 6.3 s
(movieId sorted 25M), 26.5 s (movieId file order 25M).

The point: the chunked format stores **runs**. An inline one-word chunk costs
~16 bytes of chunk plus array slack (~26 B/chunk measured), while a multi-word
handle chunk costs ~275-300 B/chunk (payload + per-chunk MLS handle). So:

- `userId` in file order is already clustered (each user's 154 ratings are
  contiguous) -> ~1 handle chunk/user, ~11,000x.
- `movieId` in file order is scattered -> one inline chunk per rating
  (23.8M chunks), ~295x. Before the inline optimization this needed ~4.5 GB;
  now 597 MiB.
- Sorting by `movieId` first restores one run per movie -> ~11,300x.

The inline representation is what makes the sparse case viable: the old design
paid a full MLS handle (~180 B) for every isolated bit, which is why unsorted
scattered keys were the worst case.

Run it with `make bench` (override `DATA=` to point at your unzipped dir).

## Limits / next steps

- 78K bits is tiny; the compressed structure really pays off at billions of
  rows. The demo proves the API and query layer, not the compression ratio.
- `not` is universe-relative.
- Rebuilds per process; a `--dump/--load` bitmap cache is the obvious next
  step if query latency without rebuild matters.
- Keyword matching is naive substring; LCSH-aware parsing or weights would
  improve precision.
