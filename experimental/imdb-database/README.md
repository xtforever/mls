# imdb-database

Filmtitel aus Dateipfaden erraten und die gefundenen Zuordnungen
(Titel, Jahr, IMDb-ID, Pfad) in eine kleine, per `mmap` durchsuchbare
Datenbank wandeln. Keine externen Abhaengigkeiten ausser POSIX und den
Shell-Werkzeugen von `download.sh`.

## Pipeline

```
movies-db                 Pfadliste (1 Datei/Zeile)
   │
   │  download.sh          IMDb-Datasets laden + TSV-Index bauen
   ▼
imdb_index.tsv            titleId \t year \t type \t title
   │  imdb_build.exed
   ▼
imdb_index.bin            mmap-Binaerindex (Token -> CSR-Postings)
   │
   │  guess.exed           Titel aus dem Pfad raten
   ▼
movies-guess.tsv          path \t title \t year \t id \t score
   │  movdb.exed build
   ▼
movies.bin                kleine Such-DB (Titel-Tokens, AND-Suche)
   │  movdb.exed meta        (title.ratings + title.basics per tt-ID)
   ▼
movies-meta.bin           Zusatzinfo je tt-ID (Rating, Stimmen, Genres,
                          Laufzeit, Typ, Jahr) — separates mmap-File
```

## Voraussetzungen

- Linux, C11, `make`
- `awk`, `sort`, `join`, `zcat`, `curl` (fuer `download.sh`)
- Build-Regeln aus `../../rules.mk` (mls-Repo); `thread_safe=1`,
  `production=0` (Debug-Build mit ASan)
- Platz: ca. 1,2 GB fuer Datasets + Index (beide per `.gitignore`
  ausgeschlossen)

## Bauen

```bash
make            # guess.exed, imdb_build.exed, movdb.exed
make check      # Selfcheck gegen testdata/ (guess + movdb-Roundtrip)
make clean      # *.exed *.od entfernen
```

## Kompletter Durchlauf

```bash
./start.sh                 # download + index + guess, sequenziell
./start.sh -j 3            # parallel (globaler Prefix-Schnitt + Chunks)
./start.sh --force         # Datasets neu laden
./start.sh --no-download   # nur bauen + raten (Index vorhanden)
./start.sh datei.txt       # andere Eingabeliste
```

`start.sh` ruft `make`, dann `download.sh`, dann `guess.exed` auf und
schreibt `movies-guess.tsv`. Sind die Datasets vorhanden, baut es danach
per `movdb.exed meta` noch `movies-meta.bin` (Zusatzinfo je tt-ID).

Fuer die Such-DB anschliessend:

```bash
./movdb.exed build movies-guess.tsv movies.bin
./movdb.exed query movies.bin mord mittsommer

# Zusatzinfo je Film (Rating/Stimmen/Genres/Laufzeit) per tt-ID:
./movdb.exed meta movies-guess.tsv movies-meta.bin \
    --ratings title.ratings.tsv.gz --basics title.basics.tsv.gz
./movdb.exed get movies-meta.bin tt0118480
./movdb.exed query --meta movies-meta.bin movies.bin stargate sg 1
```

> **Pfade:** `guess.exed` matcht intern auf den um Sammlungs-Prefixe
> gekuerzten Pfaden, gibt im `--tsv`-Modus aber den **urspruenglichen**
> Pfad aus. Das gilt fuer den direkten/sequenziellen Aufruf. Der
> Parallel-Zweig von `start.sh` schneidet den Prefix dagegen schon vor
> dem Aufruf ab und liefert daher gekuerzte Pfade. Fuer eine DB mit
> vollstaendigen Pfaden entweder sequenziell laufen lassen oder die
> Pfadspalte wie im vorherigen Lauf per `join` mit `movies-db`
> zurueckersetzen.

## Werkzeuge

### `download.sh`

Laedt `title.basics.tsv.gz`, `title.akas.tsv.gz` und
`title.ratings.tsv.gz` von `https://datasets.imdbws.com` ins
Skriptverzeichnis und baut daraus `imdb_index.tsv` (basics-Titel +
deutschsprachige aka-Titel, angereichert um Jahr und Titeltyp). Danach
`imdb_index.bin`, falls `imdb_build.exed` vorhanden ist.

Idempotent: vorhandene Dateien werden uebersprungen, der Index nur neu
gebaut, wenn er aelter als die Quelldaten ist.

```bash
./download.sh            # laden (falls noetig) + Index bauen
./download.sh --force    # neu laden
IMDB_BASE_URL=... ./download.sh   # Mirror/lokale Basis-URL (Tests)
```

### `imdb_build.exed`

```bash
./imdb_build.exed <index.tsv> <out.bin>
./imdb_build.exed --selftest
```

Kompiliert eine Index-TSV in den mmap-Binaerindex (Format: `imdb_bin.h`).
Akzeptiert 3 Spalten (`id \t year \t title`) oder 4 Spalten
(`id \t year \t type \t title`). Deterministisch, byte-identische Ausgabe
bei gleicher Eingabe.

### `guess.exed`

```bash
./guess.exed [index.bin] [liste] [topk] [--algo NAME] [--tsv]
             [--strip-prefix PCT] [--strip-only]
```

- `index.bin` — Default `imdb_index.bin`
- `liste` — Default `movies-db` (Pfade, 1 pro Zeile)
- `topk` — Default 10
- `--algo current|year|year-deep|union|both|all` — Default `union`
- `--tsv` — maschinenlesbare Ausgabe
  (`path \t title \t year \t id \t score`, leere Spalten ohne Treffer)
- `--strip-prefix PCT` — Sammlungs-Prefixe ab `PCT`% Haeufigkeit vor dem
  Matching entfernen (Default 2); `0` deaktiviert
- `--strip-only` — nur die gekuerzten Pfade ausgeben (fuer parallele Laeufe)

Ohne `--tsv`:

```
<score>  <Titel> (<Jahr>) <id>  <=  <Pfad>
```

Statistik (`matched N/M Dateien`) geht nach stderr.

### `movdb.exed`

Kleine indexierte Datenbank ueber `movies-guess.tsv` (eigenes
`MOVDB001`-mmap-Format, Inverted-Token-Index ueber die normalisierten
Titel-Tokens).

```bash
./movdb.exed build <movies-guess.tsv> <out.bin>
./movdb.exed query [--meta M] [--genre G] [--min-rating R] \
                   [--min-votes N] [--year Y] [--year-range A-B] \
                   [--type T] <out.bin> [term...]
./movdb.exed meta <movies-guess.tsv> <meta.bin> [--ratings F] [--basics F]
./movdb.exed get <meta.bin> <tt-id>...
./movdb.exed --selftest
```

`query` gibt Treffer als TSV nach stdout und `N Treffer` nach stderr;
Exitcode 1, wenn nichts passt. Zeilen ohne Titel (nicht erraten) werden
beim `build` uebersprungen. Ohne `--meta`: `title year id path`. Mit
`--meta`: `title year id rating votes runtime genres path`.

Sind Titelbegriffe angegeben, werden sie als UND-Suche ueber die
Titel-Tokens mit den Meta-Filtern kombiniert; ohne Titelbegriffe liefert
der Filter alle passenden Records. Meta-Filter brauchen `--meta`:

| Filter | Bedeutung |
|---|---|
| `--genre G` | Genre muss enthalten sein (Gross-/Kleinschreibung egal); mehrfach = alle |
| `--min-rating R` | Rating >= R (z. B. `8.0`) |
| `--min-votes N` | Stimmen >= N |
| `--year Y` | startYear == Y |
| `--year-range A-B` | startYear in [A,B] (inklusiv); offen: `A-` bzw. `-B` |
| `--type T` | `movie`, `tvSeries`/`series`, `tvMovie`, `miniSeries`, `short`, `video`, `tvSpecial`, `tvShort`, `special`, `other` |

```bash
# Alle Action-Titel mit Rating >= 8.0, inkl. Metadaten
./movdb.exed query --meta movies-meta.bin --genre action --min-rating 8.0 movies.bin
# Titel-Suche und Genre kombinieren
./movdb.exed query --meta movies-meta.bin --genre action movies.bin goldfinger
# Jahreszeitraum (inklusiv) / offene Grenzen
./movdb.exed query --meta movies-meta.bin --year-range 1930-1933 movies.bin
./movdb.exed query --meta movies-meta.bin --year-range 2020- movies.bin
```

`meta` baut ein **separates mmap-File** mit Zusatzinfo je IMDb-titel-id.
`--ratings` (title.ratings: `averageRating`, `numVotes`) und `--basics`
(title.basics: Typ, Jahr, Laufzeit, Genres, Adult) sind optional und
werden per tt-ID mit den IDs aus `movies-guess.tsv` verknuepft; `.gz`
wird per `zcat` gelesen. `get` schlaegt einzelne IDs nach
(`id rating votes runtime genres type year adult`).

```bash
$ ./movdb.exed get movies-meta.bin tt0118480
tt0118480	8.4	110850	44	Action,Adventure,Drama	3	1997	0
```

### `start.sh`

Kompletter Durchlauf (siehe oben). Optionen: `-j N`, `--force`,
`--no-download`, `-h`, oder eine abweichende Eingabeliste. Env
`IMDB_INDEX=<pfad>` fuer einen abweichenden Binaerindex.

### `checker.sh`

Dedupliziert `movies-guess.tsv` nach Titel und zeigt alphabetisch (max.
10) pro Titel den ersten Treffer mit Zeilennummer:

```bash
./checker.sh
```

### `testdata/`

- `mini-imdb.tsv` — Mini-Index (18 Titel)
- `mini-movies.txt` — 8 Pfade dazu
- werden von `make check` genutzt

## Dateiformate

| Datei | Format |
|---|---|
| `movies-db` | eine Datei pro Zeile (Pfad) |
| `imdb_index.tsv` | `titleId \t year \t type \t title` |
| `imdb_index.bin` | `IMDBBIN1` (siehe `imdb_bin.h`) |
| `movies-guess.tsv` | `path \t title \t year \t id \t score` |
| `movies.bin` | `MOVDB001` (siehe Kommentar in `movdb.c`) |
| `movies-meta.bin` | `MOVMETA1` (Zusatzinfo je tt-ID, siehe `movdb.c`) |

## Docker-Frontend

`Dockerfile` + `docker-compose.yml` bauen `movdb.exed` und starten ein
kleines Web-Frontend (`web/server.py`, nur Python-Stdlib) mit
Titel-Suche und Meta-Filtern (Genre, Rating, Jahresbereich, Typ).

```bash
docker compose build
docker compose up -d          # http://localhost:8000
docker compose down
```

- Die DB-Dateien `movies.bin` und `movies-meta.bin` muessen im
  Projektverzeichnis liegen (gitignored) und werden **read-only** nach
  `/data` gemountet (`MOVDB_BIN`/`MOVDB_META`).
- Der Build-Kontext wird per `.dockerignore` klein gehalten (Datasets/
  Indizes gehen nicht an den Docker-Daemon).
- Port/Verhalten per Umgebung: `PORT` (8000), `BIND` (0.0.0.0).
- Endpunkte: `/` (UI), `/api/search?q=...&genre=...&min_rating=...&year_range=A-B&type=...`, `/healthz`.
- Genres sind Checkboxen; mehrere ausgewaehlte Genres werden UND-verknuepft
  (jedes als eigenes `--genre`), passend zu `movdb query`.

## Bekannte Einschränkungen

- `movdb`-Query liefert Treffer in Record-Reihenfolge (kein Ranking),
  UND-Semantik; ein unbekanntes Token ergibt 0 Treffer.
- Der `build` legt je Token-Vorkommen eine Kopie im Zwischenpuffer ab;
  bei ~16k Records unkritisch (Max RSS ~13 MB), skaliert aber linear mit
  den Vorkommen.
- Normalisierung ist byteweise und deckt nur den 2-Byte-UTF-8-Bereich
  (Umlaute/Akzente im 0xC3-Block) ab.
