# Binärformate und mmap-Datenbank (`imdb-database`)

Technische Beschreibung der drei Binärdateien des Projekts. Quellen sind
ausschließlich `imdb_bin.h`, `imdb_db.h`/`imdb_db.c`, `imdb_build.c`,
`movdb.c` und (für Pipeline/Betrieb) `README.md`. Die Pipeline selbst steht
im README und wird hier nicht wiederholt.

## 1. Überblick: die drei Dateien und ihr Zusammenspiel

| Datei | Format | Zweck |
|---|---|---|
| `imdb_index.bin` | `IMDBBIN1` | Volltext-Titelindex über alle IMDb-Titel (aus `imdb_index.tsv`, gebaut von `download.sh` + `imdb_build`); davon rät `guess.exed` über den mmap-Layer `imdb_db.c` |
| `movies.bin` | `MOVDB001` | kleine Such-DB über die *erratenen* Zuordnungen (`movies-guess.tsv`: Titel, Jahr, IMDb-ID, Pfad); gebaut und abgefragt von `movdb.exed` |
| `movies-meta.bin` | `MOVMETA1` | separates Zusatzinfo-File je tt-ID (Rating, Stimmen, Genres, Laufzeit, Typ, Jahr, Adult), verknüpft aus `title.ratings` + `title.basics`; bei `movdb query --meta` pro Record per Binärsuche über die tt-ID hinzugezogen |

Kette: `imdb_index.bin` liefert den Kandidatenraum fürs Raten, `movies.bin`
indexiert die dabei entstandenen Zuordnungen für Rückwärts-Suchen (Titel →
Datei), `movies-meta.bin` ergänzt rein additiv die
IMDb-Metadaten. Die Trennung hält `movies.bin` klein und verlustfrei
neu-baubar; Meta bleibt ein optionales Second-File.

## 2. Format `IMDBBIN1` — `imdb_index.bin`

Definiert in **`imdb_bin.h`** (Header-Struct, Sektions-Enum, Magics).
Writer: **`imdb_build.c`** (`build()`, `write_section()`).
Reader/Validierung: **`imdb_db.c`** (`imdb_open()`).
Little-Endian, x86-64, alle Sektionen 8-Byte-aligned (`imdb_bin.h`,
Kommentar).

### 2.1 Header (112 Byte)

`struct imdb_bin_header`; `_Static_assert` erzwingt 112 Byte. Magic
`"IMDBBIN1"`, Version **5** (`IMDB_BIN_VERSION`; v5 führte die
types-Sektion ein), endian-Marker `0x01020304` (`IMDB_BIN_ENDIAN`):

| Offset (B) | Größe (B) | Feld | Typ | Wert / Bedeutung |
|---|---|---|---|---|
| 0 | 8 | `magic` | `char[8]` | `"IMDBBIN1"` |
| 8 | 4 | `version` | `uint32_t` | `5` |
| 12 | 4 | `endian` | `uint32_t` | `0x01020304` — verrät bei abweichender Byte-Reihenfolge eine Fremddatei |
| 16 | 4 | `n_records` | `uint32_t` | N (Titel) |
| 20 | 4 | `n_tokens` | `uint32_t` | T (distinkte Tokens) |
| 24 | 4 | `n_postings` | `uint32_t` | P (Postings gesamt) |
| 28 | 4 | `reserved` | `uint32_t` | stets 0 |
| 32 | 80 | `off[10]` | `uint64_t[10]` | Sektionsoffsets, Index = `enum imdb_section` |

### 2.2 Die zehn Sektionen

Reihenfolge on-disk = `off[]`-Index (Schreibreihenfolge in `build()`):

| idx | enum (`imdb_bin.h`) | Typ | Elem. (B) | Anzahl | Bedeutung |
|---|---|---|---|---|---|
| 0 | `IMDB_SEC_TITLE_OFF` | `uint32_t` | 4 | N+1 | Offset ins Titelblob; `title_off[N]` = `titles_len` |
| 1 | `IMDB_SEC_TITLES` | Bytes | 1 | — | NUL-terminierte Titel (Originalschreibung, *nicht* normalisiert) |
| 2 | `IMDB_SEC_IDS` | `uint32_t` | 4 | N | numerischer tt-Teil der IMDb-ID |
| 3 | `IMDB_SEC_YEARS` | `uint16_t` | 2 | N | Jahr, 0 = unbekannt |
| 4 | `IMDB_SEC_IDF_SUM` | `float` | 4 | N | Summe IDF der distinkten Tokens des Titels (→ 2.5) |
| 5 | `IMDB_SEC_DICT_OFF` | `uint32_t` | 4 | T+1 | Byte-Offset der Tokenstrings in `dict` |
| 6 | `IMDB_SEC_DICT` | Bytes | 1 | — | NUL-terminierte Tokens, aufsteigend sortiert (`strcmp`) |
| 7 | `IMDB_SEC_POSTINGS` | `uint32_t` | 4 | P | Record-Indizes (CSR), je Token aufsteigend |
| 8 | `IMDB_SEC_POST_OFF` | `uint32_t` | 4 | T+1 | CSR-Zeilenoffset je Token in `postings` |
| 9 | `IMDB_SEC_TYPES` | `uint8_t` | 1 | N | Titeltyp (`enum imdb_type`) |

Abgeleitete Längen: `titles_len = off[2] − off[1]`,
`dict_len = off[7] − off[6]` (Reader `imdb_open()`). `dict_off` hält die
String-Offsets, `post_off` die CSR-Zeilen — „ein Array kann nicht beides
zugleich sein“ (`imdb_bin.h`, Kommentar).

`enum imdb_type` (Werte 0–9): `OTHER`, `MOVIE`, `TVMOVIE`, `TVSERIES`,
`MINISERIES`, `TVSHORT`, `TVSPECIAL`, `SHORT`, `VIDEO`, `SPECIAL`.
`miniSeries`/`tvMiniSeries` sind zu `MINISERIES` zusammengefasst;
`IMDB_TYPE_IS_SERIES(t)` = `TVSERIES ∥ MINISERIES`.

### 2.3 Alignment und Padding

Jede Sektion beginnt auf einem 8-Byte-offset (`align8()` im Builder,
`% 8`-Prüfung im Reader). `write_section()` füllt nach jeder Sektion mit
`0x00` auf ein Vielfaches von 8 auf. Zusätzlich polstert der Builder die
Blobs `titles` und `dict` *intern* mit `'\0'` auf ein Vielfaches von 8,
bevor `title_off[N]` bzw. `dict_off[T]` gesetzt werden — damit gilt
`off[2] − off[1]` bzw. `off[7] − off[6]` exakt als Blob-Länge (Padding
zählt mit hinein).

### 2.4 Invarianten und Validierung im Reader

Vom Builder garantierte Invarianten (`imdb_build.c`), die `imdb_open()`
bei jedem `imdb_open` prüft — Verstoß ⇒ `NULL` mit `errno = EINVAL`:

- Dateigröße ≥ 112; `magic`/`version`/`endian` stimmen exakt.
- `0 < n_records < UINT32_MAX`; `n_tokens < UINT32_MAX`;
  `n_postings < UINT32_MAX`.
- `off[0..8]` sind 8-justiert (`off[9]` über die eigene Fit-Prüfung).
- `off[1] ≤ off[2]`, `off[6] ≤ off[7]`; jede Sektion mit berechenbarer
  Länge liegt komplett in der Datei (`sec_fits()`).
- `title_off[N] == titles_len`; `title_off` monoton nicht fallend.
- `dict_off[0] == 0`; `dict_off[T] == dict_len`; monoton nicht fallend.
- `post_off[0] == 0`; `post_off[T] == n_postings`; monoton nicht fallend.

Zusätzlich überspringt die Suche Postings mit `rec ≥ n_records`
(„Guard: korrupte Postings“, `imdb_search()`).

### 2.5 Postings: CSR mit Set-Semantik; `idf_sum`

Zeile des Tokens `t` = `postings[post_off[t] .. post_off[t+1])`, Record-
Indizes aufsteigend. Je Paar (Token, Record) genau **ein** Eintrag
(Set-Semantik): der Builder dedupliziert Token-Wiederholungen innerhalb
eines Records vor dem CSR-Pass.

`idf_sum[rec] = Σ idf(t)` über die distinkten Titel-Tokens mit
`idf(t) = log(1 + N/df(t))` (natürliches `log`). Tokens mit
`df > N/20` (cutoff mindestens 1) fließen **nicht** ein — sonst würde ein
im Query ohnehin weggefilterter Artikel („The Thing“) den richtigen
Treffer bestrafen; die Regel ist beidseitig neutral
(`imdb_build.c`, IDF-Pass).

## 3. Suchalgorithmus über IMDBBIN1 (`imdb_db.c`)

1. **Normalisieren/Splitten:** `imdb_norm()` (byteweise: Umlaute →
   `ae/oe/ue`, `ß` → `ss`, alnum → `tolower`, sonst Space), dann
   `imdb_split()` an Space-Läufen; Query-Tokens werden duplikatfrei
   gemacht (Set-Semantik).
2. **Dict-Suche:** jedes Token per Binärsuche `dict_find()` in
   `dict`/`dict_off`, **O(log T)** über `strcmp`.
3. **Gewichtung/Stopwords:** Token mit `df > N/20` (cutoff ≥ 1) wird
   verworfen (`imdb_is_stop()` liefert dafür 1, ebenso für unbekannte
   Tokens). Bekanntes, nicht-stoppendes Token:
   `idf = log(1 + N/df)`. **Unbekanntes Token: `idf_max = log(1 + N)`** —
   es erhöht nur den Nenner (`idf_Q`) und senkt den Score, hat aber keine
   Postings. `idf_Q` = Summe dieser Gewichte.
4. **Kandidatensammlung per Epoch-Mark:** je Kandidat ein Slot in `hits`;
   `mark[rec] == epoch` (suche-eigener Zeitstempel) statt Clearing,
   `pos[rec]` → Slot. Überlauf bei `epoch > 0x3fffffff` ⇒ frische
   `calloc`-Puffer, `epoch = 1`. Pro Treffer: `weight += idf`,
   `nmatch++`.
5. **Score (IDF-gewichtete Jaccard-Ähnlichkeit):** mit `num` = Summe IDF
   der *gematchten* Query-Tokens, `idf_T = idf_sum[rec]`:

   ```
   score = num / (idf_Q + idf_T − num)      (≤ 1.0, Nenner ≤ 0 ⇒ 0)
   ```

   Exakter Titel ⇒ 1.0 (`imdb_db.h`). Ungematchte Tokens strafen nach
   ihrer IDF: seltene stark, häufige kaum.
6. **Nebenmerkmale:** `yhit` = 1 wenn `year_hint > 0` und
   `|Jahr − year_hint| ≤ 1`; `thit` = 1 bei `want_series` und
   Serientyp.
7. **Sortierung** (`hit_cmp`, `imdb_db.c`): Schlüssel
   `score + IMDB_YEAR_BONUS · yhit` absteigend,
   `IMDB_YEAR_BONUS = 0.15` wirkt **nur** für `score ≥ IMDB_SCORE_MIN =
   0.4`. Bei Gleichstand: erst `thit`, dann mehr `nmatch`, dann höheres
   `weight`, zuletzt `rec` aufsteigend. Der Titeltyp wirkt **nur als
   Tiebreak, nicht additiv** — sonst verdrängt ein generischer
   Serientitel den spezifischeren (Code-Kommentar).
   Auslieferung als Top-k: Einbausortierung, wenn `topk < ncand`, sonst
   `qsort`+`memcpy` (gleiche Ordnung, `hit_cmp` ist streng total).
8. **Thread-Safety** (`imdb_db.h`): `imdb_search()` nutzt mutablen
   Scratch (`mark`/`pos`/`hits`, lazy allokiert) und ist **nicht
   thread-safe**; pro Thread ein eigenes `struct imdb_db`. Die
   Read-only-Zugriffe `imdb_title/year/type/id` sind nach `imdb_open()`
   unveränderlich.

## 4. Format `MOVDB001` — `movies.bin`

Definiert in **`movdb.c`** (Dateikonstanten `MOVDB_MAGIC "MOVDB001"`,
`MOVDB_VERSION 1`, `struct mov_header`; `_Static_assert`: 72 Byte).
Writer: `movdb.c build()`, Reader: `movdb.c query()`. Gleiches
Prinzip (Inverted-Token-Index, CSR) wie `IMDBBIN1`, aber schlanker:
**kein endian-Marker, kein `reserved`**, und indexiert wird nur der Titel.

### 4.1 Header (72 Byte)

| Offset (B) | Größe (B) | Feld | Typ | Wert / Bedeutung |
|---|---|---|---|---|
| 0 | 8 | `magic` | `char[8]` | `"MOVDB001"` |
| 8 | 4 | `version` | `uint32_t` | `1` |
| 12 | 4 | `n_rec` | `uint32_t` | N Records |
| 16 | 4 | `n_tok` | `uint32_t` | T Tokens |
| 20 | 4 | `n_post` | `uint32_t` | P Postings |
| 24 | 48 | `off[6]` | `uint64_t[6]` | Sektionsoffsets; `off[0] = 72` |

### 4.2 Die sechs Sektionen

| idx | Name | Typ | Elem. (B) | Anzahl | Bedeutung |
|---|---|---|---|---|---|
| 0 | `rec_off` | `uint32_t` | 4 | N+1 | Offset je Record in `rec_blob` |
| 1 | `rec_blob` | Bytes | 1 | — | Record-Blobs |
| 2 | `dict_off` | `uint32_t` | 4 | T+1 | Offset je Token in `dict` |
| 3 | `dict` | Bytes | 1 | — | Tokenstrings, sortiert, NUL-terminiert |
| 4 | `postings` | `uint32_t` | 4 | P | Record-Indizes je Token (CSR) |
| 5 | `post_off` | `uint32_t` | 4 | T+1 | CSR-Zeilen je Token |

**Record-Blob:** genau `title\tyear\tid\tpath\0` (NUL-terminiert;
gebaut per `snprintf("%s\t%s\t%s\t%s", title, year, id, path)` aus
`movies-guess.tsv`). Zeilen ohne erratenen Titel fehlen; das `score`-Feld
der TSV wird nicht übernommen.

**Index:** invertierter Token-Index über die *normalisierten* Titel-Tokens
(`imdb_norm` + `imdb_split`, je Record duplikatfrei) — CSR wie in
`imdb_bin.h`; Postings je Token aufsteigend.

**Query-Semantik** (`query()`): jeder Suchbegriff wird normalisiert und
gesplittet; alle Tokens über alle Begriffe sind **UND**-verknüpft. Ein
einziges fehlendes Dict-Token beendet mit 0 Treffern. Je Token wird die
CSR-Zeile in ein Bitset über die Records gemalt, danach Bitweise-`AND`
aller Token-Mengen. Ausgabe in **Record-Reihenfolge** (Zeilenreihenfolge
der Quell-TSV) — **kein Ranking**. Mit `--meta` werden pro Record die
Metadaten via tt-ID angehängt und optionale Filter angewandt
(`meta_match()`: alle Genre-Bits müssen gesetzt sein, `min-rating`,
`min-votes`, exaktes Jahr oder Jahresbereich — unbekanntes Jahr (0)
passt in keinen Bereich —, Typ); die Filter-Tabelle zeigt das README.
Praktische Query-Grenze aus dem Code: Norm-Puffer 512 Byte, max. 64
Tokens pro Begriff (`norm[512]`, `tok[64]`).

Reader-Validierung: Dateigröße ≥ 72, `magic`, `version` — weitergehendes
wie bei `IMDBBIN1` findet hier nicht statt.

## 5. Format `MOVMETA1` — `movies-meta.bin`

Definiert in **`movdb.c`** (`MOVMETA_MAGIC "MOVMETA1"`,
`MOVMETA_VERSION 1`, `struct movmeta_header`, `struct movmeta_entry`;
jeweils `_Static_assert` auf **24** Byte). Writer: `meta_build()`,
Reader: `meta_open()`/`meta_find()`. Reines Festplatten-Array ohne
Zeiger-Sektionen.

### 5.1 Header (24 Byte)

| Offset (B) | Größe (B) | Feld | Typ | Wert / Bedeutung |
|---|---|---|---|---|
| 0 | 8 | `magic` | `char[8]` | `"MOVMETA1"` |
| 8 | 4 | `version` | `uint32_t` | `1` |
| 12 | 4 | `n` | `uint32_t` | Anzahl Entries |
| 16 | 8 | `reserved` | `uint64_t` | 0 |

Es folgt exakt ein Array `movmeta_entry[n]`, **aufsteigend nach `id`**
sortiert — Nachschlageweg ist `bsearch()` über die tt-ID (`ent_id_cmp`).

### 5.2 Entry (24 Byte)

| Offset (B) | Größe (B) | Feld | Typ | Bedeutung |
|---|---|---|---|---|
| 0 | 8 | `id` | `uint64_t` | numerischer tt-Teil |
| 8 | 4 | `votes` | `uint32_t` | `numVotes`, 0 = unbekannt |
| 12 | 4 | `genres` | `uint32_t` | Bitmaske über `GENRES[]` (→ 5.3) |
| 16 | 2 | `year` | `uint16_t` | `startYear`, 0 = unbekannt |
| 18 | 2 | `runtime` | `uint16_t` | Laufzeit Minuten, 0 = unbekannt |
| 20 | 2 | `rating10` | `uint16_t` | `averageRating × 10` (gerundet), 0 = unbekannt |
| 22 | 1 | `type` | `uint8_t` | `enum imdb_type` (→ 5.4) |
| 23 | 1 | `adult` | `uint8_t` | 0/1 |

Reader-Validierung: Dateigröße ≥ 24, `magic`, `version`; eine
Konsistenzprüfung `Dateigröße == 24 + n·24` findet in `meta_open()`
nicht statt.

### 5.3 Genre-Bitmaske

Bitindex = Position im Array `GENRES[]` (`movdb.c`), 28 Einträge in
dieser Reihenfolge: Action, Adult, Adventure, Animation, Biography,
Comedy, Crime, Documentary, Drama, Family, Fantasy, Film-Noir,
Game-Show, History, Horror, Music, Musical, Mystery, News, Reality-TV,
Romance, Sci-Fi, Short, Sport, Talk-Show, Thriller, War, Western.
Beim Bau setzt `genre_mask()` pro Komma-Feld der `title.basics`-Genre-
Liste das jeweilige Bit; beim Filtern (`--genre`, `genre_bit()`,
Groß-/Kleinschreibung egal) müssen **alle** gesetzten Filter-Bits im
Entry gesetzt sein.

### 5.4 Bezug zu `enum imdb_type`

Das Feld `type` nutzt denselben `enum imdb_type` aus `imdb_bin.h`;
`type_code()` mappt die `titleType`-Zeichenketten (`movie`, `tvMovie`,
`tvSeries`, `miniSeries`/`tvMiniSeries` → `MINISERIES`, …) wie der
`IMDBBIN1`-Index. Der Query-Filter `--type` akzeptiert die Namen aus der
README-Tabelle (`series` ist Alias für `tvSeries`).

## 6. mmap-Zugriffsmodell

Alle drei Dateien werden nach demselben Muster gelesen
(`imdb_open()`, `movdb.c query()`/`meta_open()`):

1. `open(path, O_RDONLY)` → `fstat()` (Mindestgröße = Header) →
   `mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0)`.
2. Validierung im Kopf/über die Sektionen (→ 2.4, 4.2, 5.2);
   `movdb` schließt das `fd` direkt nach dem `mmap`.
3. **Kein Kopieren:** Zeiger zeigen direkt in die Karte
   (`db->title_off = base + off[...]` usw.); nur der Header wird in
   `imdb_open()` für die Prüfungen auf den Stack gespiegelt. Daten-
   Zugriff kostet Page-Faults, kein `read()`.
4. Schließen: `munmap()` + `close()` (`imdb_close()`, `meta_close()`).

Betrieb: `movies.bin`/`movies-meta.bin` werden im Wartungsbetrieb
**atomar per `tmp`+`mv`** ersetzt, damit laufende mmap-Suchen am alten
Inode ungestört weiterlesen (README, Abschnitt Wartungs-Tab). Wegen
`PROT_READ`/`MAP_PRIVATE` würde ein Überschreiben *derselben* Datei die
bestehende Karte ohnehin nicht konsistent aktualisieren.

## 7. Gemeinsame Konventionen

- **Little-Endian, x86-64** (`imdb_bin.h`, Kopfkommentar). Nur
  `IMDBBIN1` trägt einen endian-Marker (`0x01020304`); `MOVDB001` und
  `MOVMETA1` haben kein endian-Feld.
- **`_Static_assert` auf Struct-Größen:** `imdb_bin_header` = 112,
  `mov_header` = 72, `movmeta_header` = 24, `movmeta_entry` = 24; dazu
  Größen-Asserts für `uint32_t`/`uint16_t`/`uint64_t` (`imdb_bin.h`).
- **`uint32_t`-Offsets/-Zähler ⇒ 2^32-Grenze:** `U32_LIMIT = 1<<32`;
  beide Builder brechen mit klarer Meldung ab (z. B.
  `titles_len >= 2^32`, `P >= 2^32`), der `IMDBBIN1`-Reader verwirft
  `N/T/P ≥ UINT32_MAX`.
- Strings stets NUL-terminiert; `imdb_build` ist deterministisch
  (byte-identische Ausgabe bei gleicher Eingabe, README).

## 8. Bekannte Grenzen

Nur was Code/README hergeben (README: „Bekannte Einschränkungen“):

- **Normalisierung ist byteweise** und deckt nur den 2-Byte-UTF-8-Bereich
  ab (0xC3-Block, Umlaute/Akzente); fremde Mehrbyte-Zeichen fallen raus,
  ein ungemapptes 0xC3-Folgepaar verbraucht beide Bytes und ergibt einen
  Space (`imdb_norm()`, ponytail-Kommentar mit Upgrade-Pfad: echte
  UTF-8-Decodierung).
- **`movdb`-Query ohne Ranking:** UND-Semantik, Treffer in
  Record-Reihenfolge, ein unbekanntes Token ergibt 0 Treffer.
- **`movdb`-Build** legt je Token-*Vorkommen* eine Kopie im Zwischenpuffer
  ab (tokblob + occ); skaliert linear mit den Vorkommen (bei ~16k Records
  unkritisch, max. RSS ~13 MB laut README).
- **Query-Puffer in `movdb`:** 512 Byte Norm-Puffer, 64 Tokens pro Begriff.
- **`MOVMETA1`-Reader** prüft nur Header (Größe/Magic/Version), nicht die
  Eintragsbereichslänge `24 + n·24`.

## 9. Definitionsdateien je Format

| Format | Datei | Definiert in | Writer | Reader |
|---|---|---|---|---|
| `IMDBBIN1` | `imdb_index.bin` | `imdb_bin.h` (`struct imdb_bin_header`, `enum imdb_section`, `enum imdb_type`) | `imdb_build.c` | `imdb_db.c` (`imdb_open`, `imdb_search`) |
| `MOVDB001` | `movies.bin` | `movdb.c` (`struct mov_header`) | `movdb.c` (`build`) | `movdb.c` (`query`) |
| `MOVMETA1` | `movies-meta.bin` | `movdb.c` (`struct movmeta_header`, `struct movmeta_entry`, `GENRES[]`) | `movdb.c` (`meta_build`) | `movdb.c` (`meta_open`, `meta_find`) |

Pipeline-Kontext, Werkzeug-Aufrufe und Betrieb: `README.md`.
