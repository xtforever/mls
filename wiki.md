# mls — Wissensbasis (wiki.md)

Kompakter Gesamtüberblick über die `mls`-Bibliothek, gedacht als **erste und
einzige** Lektüre vor einer Aufgabe. Fakten sind direkt aus den Headern in
`lib/` und aus `generate_amalgam.py` abgeleitet (Stand: siehe Commit).
Tiefere Details: `docs/notes/QUICKREF.md`, `docs/api/API.md`, `docs/guide/`.

`mls` ist eine C11-Bibliothek: dynamische Arrays und Strings über `int`-Handles
statt roher Pointer. Ein falscher oder bereits freigegebener Handle ist ein
lauter Fehler (Slot-Check), kein Dangling Pointer.

---

## 1. Kernidee & Lebenszyklus

- Ein Handle ist ein **`int > 0`**. Prüfen mit `if (h > 0)`; ein freigegebener
  Handle ist `0` bzw. `m_is_freed(h) == 1`.
- `m_init()` **einmal am Anfang**, `m_destruct()` **einmal am Ende**.
  Vergessenes `m_init()` → erster Aufruf stirbt mit `Not init.`.
- `m_free(h)` gibt einen Handle frei; bei `MFREE_STR`/`MFREE_EACH` werden die
  Elemente rekursiv mit freigegeben.
- Zeiger aus `mls(h,i)`/`m_buf(h)` **nie über ein Resize hinweg halten** —
  `realloc` kann den Puffer verschieben. Handle neu auflösen.
- Zugriff nach `m_free` → UAF-Protection beendet das Programm (`exit(1)`).
  Bei unsicherem Zustand vorher `m_is_freed(h)` prüfen.

```c
m_init();
int h = m_alloc(0, sizeof(int), MFREE);   // Kapazität, Elementbreite, Free-Handler
int v = 42;
m_put(h, &v);                             // anhängen → Index
printf("%d\n", INT(h, 0));                // 42
m_free(h);
m_destruct();
```

---

## 2. Bauen & Linken

```bash
# Single-Header (Amalgam, empfohlen für Beispiele/kleine Projekte):
gcc -std=c11 main.c -o prog -lpthread -lm -ldl

# Multi-File gegen lib/:
gcc -std=c11 -I./lib prog.c lib/mls_base.c lib/mls_ext.c -o prog -lpthread -lm -ldl
```

- Pthreads sind auf Unix standardmäßig aktiv; `-DMLS_THREAD_SAFE=0` schaltet ab.
- `-DMLS_DEBUG` aktiviert Debug-Wrapper + Post-Mortem; CMake-Debug baut
  automatisch mit ASan. `-DMLS_WERROR=ON` macht Warnungen zu Fehlern.
- C11, kein C++-Syntax.
- Thread-Safety: per-Handle `pthread_rwlock_t`. `s_cstr`/`s_ccstr` nicht
  concurrent aufrufen (Pointer-Truncation-Race); Ring-Buffer braucht externen
  Mutex; `m_foreach` gibt die Read-Lock zwischen Elementen frei.

---

## 3. Kern-API (`mls_base.h`, `mls_ext.h`)

Array-Handles (`w` = Elementbreite, z. B. `sizeof(int)`):

| Funktion | Zweck |
|---|---|
| `m_alloc(max, w, hfree)` | neues Array (Kapazität, Breite, Free-Handler) |
| `m_create(n, w)` | wie `m_alloc`, ohne Free-Handler |
| `m_free(h)` | freigeben (safe für `h <= 0`) |
| `mls(h, i)` / `mls_safe(h, i)` | Zeiger auf Element (safe: NULL + `mls_errno`) |
| `m_put(h, &v)` | anhängen, liefert Index |
| `m_puti(h, i)` / `m_putc(h, c)` | int / char anhängen |
| `m_len(h)` / `m_width(h)` / `m_bufsize(h)` | Länge / Elementbreite / Kapazität |
| `m_buf(h)` / `m_str(h)` | roher Puffer / `char*` (leerer String wenn frei) |
| `m_setlen(h, n)` / `m_clear(h)` | logische Länge setzen / auf 0 |
| `m_write(h, p, data, n)` / `m_read(h, p, &dst, n)` | schreiben / lesen |
| `m_del(h, i)` / `m_ins` / `m_remove` | Element löschen / einfügen / Bereich |
| `m_peek(h, i)` | Elementzeiger ohne Bounds-Check |
| `m_is_freed(h)` / `m_is_valid(h)` | Slot-Status (safe) |
| `m_dub(h)` | beschreibbare Kopie (für NOALLOC-Wraps) |

Typisierte Makros (bounds-checked): `INT(h,i)`, `STR(h,i)`, `CHAR(h,i)`,
`UINT`, `U32`, `U64`, `FLOAT`, `DOUBLE`, `PTR`; `_UNCHECKED`-Varianten und
`_SAFE`-Varianten (`INT_SAFE` → 0 statt Abbruch). Weitere:
`CHARP(h)` = `(char*)m_buf(h)`, `MSTR(h)` = `(char*)mls(h,0)`,
`m_cat(h, s)` = `m_write(h, m_len(h), s, strlen(s))`.

Iteration:

```c
int i, *p;
m_foreach(list, i, p) {           // i startet bei -1, wird hochgezählt
    int child = *p;               // Element ist hier ein int-Handle
}
```

Free-Handler-Konstanten: `MFREE` (Puffer), `MFREE_STR` (`free()` je Element),
`MFREE_EACH` (`m_free()` je Element), `MFREE_NODESTRUCT`, `MFREE_NOALLOC`
(kein free/realloc), `NOHDL`.

Wrapping (Zero-Copy, **nie** vom Aufrufer-Speicher freigeben):
`m_wrapints(int*, n)`, `m_wrapstrings(char**, n)`, `m_wrapcstr(char*)`,
`m_wrapstrhandles(const char**, n)` — alle `MFREE_NOALLOC`.

Konstant-Strings: `s_cstr(s)`/`s_ccstr(s)` internieren (Zero-Copy),
`s_cstrdup(s)` kopiert. **Länge inklusive Null-Byte** → für Längen
`s_strlen()` nutzen. `cs_printf(fmt, ...)` formatierter Konstant-String.

Block-Scoped Auto-Free: `tmls h = m_alloc(...)` gibt am Blockende frei
(GCC/Clang `__attribute__((cleanup))`; sonst normales `int`). **Immer
initialisieren.**

Statistiken: `m_count_allocated()`, `m_total_bytes()`, `m_peak_handles()`,
`m_debug_print(fp)`.

---

## 4. Amalgam-System (Single-Header, stb-Stil)

`python3 generate_amalgam.py` erzeugt aus `lib/*` fertige Single-Header im
**Repo-Root**. Nutzung in **genau einer** `.c`-Datei:

```c
#define MLS_FULL_IMPLEMENTATION   // oder MLS_CORE_IMPLEMENTATION
#include "mls_full.h"             // MUSS vor <stdio.h> etc. stehen
```

Der Header setzt `_GNU_SOURCE` selbst — aber nur, wenn noch kein System-Header
eingebunden wurde. Alternativ `-D_GNU_SOURCE` auf der Kommandozeile. Link
immer `-lpthread -lm -ldl`. Andere `.c`-Dateien binden den Header nur ein
(ohne Impl-Makro).

Erzeugte Header:

| Header | Inhalt | Impl-Makro | Config |
|---|---|---|---|
| `mls_core.h` | Kern, release, single-threaded | `MLS_CORE_IMPLEMENTATION` | eingebrannt |
| `mls_coremt.h` | Kern, release, multithread | `MLS_CORE_IMPLEMENTATION` | eingebrannt |
| `mls_cored.h` | Kern, debug, single-threaded | `MLS_CORE_IMPLEMENTATION` | eingebrannt |
| `mls_coredmt.h` | Kern, debug, multithread | `MLS_CORE_IMPLEMENTATION` | eingebrannt |
| `mls_full.h` | Kern **+** `m_http`, `m_hdf`, `m_http_server`, `m_flask` | `MLS_FULL_IMPLEMENTATION` | keine (auto thread-safe) |

- "Kern" = `mls_base`, `mls_ext`, `list`, `table`, `m_tool`, `m_var`, `m_extra`.
- Nur `mls_full.h` enthält **HDF** — wer HDF parsen/schreiben will, braucht
  diese (große) Amalgam. Unbenutzter HTTP/Flask-Code lässt sich mit
  `-ffunction-sections -fdata-sections -Wl,--gc-sections` wegwerfen.
- **Nicht in irgendeinem Amalgam** (nur Multi-File): `hdf_base.c`,
  `hdf_parse.c`, `m_subproc.c`, `mlscoretest.c`.
- Prüfen: `python3 generate_amalgam.py --check` (kompiliert + raucht alle
  Header mit `-std=c11 -I. -lpthread -lm -ldl`).

---

## 5. Datei-Map `lib/`

| Datei | Inhalt |
|---|---|
| `mls_base.*` | Kern: Handle-Tabelle, `m_alloc`/`m_free`/`mls`/`m_put`/`m_buf`, `mls_errno` |
| `mls_ext.*` | `m_init`/`m_destruct`, `m_dub`, `m_slice`, Binärsuche, Wrapping, Konstant-Strings, `tmls` |
| `m_tool.*` | String-API `s_*`, `m_lookup`, UTF-8 |
| `m_extra.*` | `s_casecmp`, trim/pad, `s_to_long`, base64, `s_secure_cmp` |
| `m_var.*` | Variablen-/Interpolationssystem `vs_*` (`vs_get`, `vs_string`) |
| `list.*` | universelle Node/List-Struktur `list_*` (Basis für HDF und Tables) |
| `table.*` | sortierte Dict `tbl_*`, O(log n) |
| `m_http.*`, `m_http_server.*` | HTTP-Parser/Client + eingebetteter Server |
| `m_hdf.*` | HDF-Konfigformat (eigener Baum, Parser **und** Writer) |
| `hdf_base.*`, `hdf_parse.*` | alternativer HDF-Reader über `list.h` (**kein** Amalgam) |
| `m_flask.*` | HTTP-Routing über HDF-Konfig |
| `m_subproc.*` | `subproc_run`/`subproc_read`/`subproc_lines` (**kein** Amalgam) |
| `mlscoretest.c` | Selftest, keine öffentliche API |

`indexer/`, `experimental/`, `idea/`, `html/` sind KEIN Teil der Library.

---

## 6. Strings (`m_tool.h`, `m_extra.h`)

Strings sind Handles der Breite 1. Länge **ohne** Null-Byte: `s_strlen(h)`.

| Funktion | Zweck |
|---|---|
| `s_new()` / `s_free(h)` | leerer String / freigeben |
| `s_dup(const char*)` / `s_clone(h)` / `s_mdup(h)` | Kopie aus C-String / Handle |
| `s_printf(h, p, fmt, ...)` | formatiert; `p < 0` hängt an, `h == 0` allokiert neu |
| `s_cat(h, cstr)` / `s_ncat` / `s_app(h, ..., NULL)` | anhängen |
| `s_slice(dest, offs, m, a, b)` | Slice, **null-terminiert** |
| `s_split(m, s, c, remove_ws)` | in `char*`-Liste splitten |
| `s_join(sep, ..., NULL)` / `s_implode(dest, srcs, sep)` | verbinden |
| `s_implode_q(dest, srcs, sep, quote)` | mit Quoting verbinden |
| `s_find(h, sub)` / `s_has_prefix` / `s_has_suffix` | suchen / Präfix / Suffix |
| `s_replace_c(h, old, rep)` / `s_trim(h)` / `s_trim_c` | ersetzen / trimmen |
| `s_lower(h)` / `s_upper(h)` | Case (in-place) |
| `s_cmp(a,b)` / `s_ncmp` / `s_casecmp` / `s_ncasecmp` | Vergleiche |
| `s_secure_cmp(a,b)` | konstantzeit-Vergleich |
| `s_from_long` / `s_from_int` / `s_from_double` / `s_to_long` | Zahlen ↔ String |
| `s_is_numeric(h)` / `s_is_alpha(h)` / `s_base64_decode(h)` | Klassifikation/Decode |
| `m_str_from_file(name)` / `s_readln(buf, fp)` / `s_read_fields` | Datei-Einlesen |
| `m_dir_list(path)` | Verzeichnis-Einträge als `char*`-Liste (`os.listdir`, ohne `.`/`..`) |
| `m_strlist_save(h, path)` / `m_strlist_load(path)` | Namensliste ⇄ Textdatei (eine Zeile je Eintrag) |
| `m_lookup_str(h, key, NOT_INSERT)` | String-Set (**sortiert**, O(log n)): Lookup (`1`) oder Lookup + Sort-Insert (`0`), Key = C-String |
| `m_lookup_mstr(h, key_h, NOT_INSERT)` | wie `m_lookup_str`, Key als String-Handle (kein `m_str()` nötig) |
| `m_qsort(h, cmp_mstr)` | `char*`-Liste in `strcmp`-Ordnung sortieren |
| `cs_printf(fmt, ...)` | formatierter Konstant-String |

`m_lookup_str`/`m_lookup_mstr` halten die Liste sortiert (Binärsuche) und fügen
an der richtigen Stelle ein. Voraussetzung: die Liste ist sortiert — entweder
nur über diese Funktionen aufbauen oder vorher `m_qsort(h, cmp_mstr)` aufrufen
(wie `exec` es nach dem Laden tut). Leerer Key → `-1`, nie eingefügt.

**`conststr_*` vs. `m_lookup_*`:** `conststr_lookup_c` / `s_cstr` / `s_cstrdup`
sind ebenfalls ein sortierter Dedup-Lookup (O(log n)) — aber **global** (ein
`CS_MAP` pro Prozess, geteilt mit allen `s_cstr`-Literalen), **ohne Enumeration**
(internierte Strings lassen sich nicht auflisten) und **ohne „war es neu?“**.
conststr ist damit ein Interner, kein speicherbares, aufrufer-eigenes Set:
dafür braucht man `m_lookup_str`/`m_lookup_mstr` + `m_strlist_save`/`load`.
Beide nutzen intern dieselben `m_bsearch`/`m_binsert2`-Primitive.

`m_str(h)` liefert den `char*`-Puffer (leerer String, wenn frei/leer).
`s_printf(h, -1, ...)` sauber anhängen: überschreibt das vorhandene
Null-Byte ("Null-Terminator-Pollution").

---

## 7. Tabellen (`table.h`) — `tbl_*`, **nicht** `mt_*`

Sortiertes Dict, binäre Suche → **O(log n)**. Ein Eintrag ist
`(key value)`; Keys sind Strings, Values String **oder** owned Handle. Alle
Keys/Values werden von `tbl_free` freigegeben.

| Funktion | Zweck |
|---|---|
| `tbl_create()` / `tbl_free(t)` / `tbl_is_table(t)` | anlegen / freigeben / Magie prüfen |
| `tbl_set(t, key, value)` | String-Value (Key + Value kopiert) |
| `tbl_set_handle(t, key, value_h)` | owned mls-Handle als Value |
| `tbl_set_handle_str(t, key_h, value_h)` | Handle-Key + Handle-Value (Ownership geht über) |
| `tbl_get(t, key)` | String-Value oder `NULL` wenn nicht vorhanden |
| `tbl_get_handle(t, key)` | Handle-Value oder `0` |
| `tbl_del(t, key)` | `0` entfernt / `-1` nicht da |
| `tbl_len(t)` | Anzahl Einträge |
| `tbl_key(t, i)` / `tbl_value(t, i)` | i-ter Key (sortiert) / String-Value |

Mitgliedschaftstest: `tbl_get(t, key) != NULL`. Die alte `mt_*`-API aus
`docs/notes/QUICKREF.md`/`WIKI.md` existiert nicht mehr.

---

## 8. `list.h` — universelle Node/List-Struktur

Basis für den HDF-Reader (`hdf_base`) **und** die Table. Ein Node ist ein
Handle mit genau einem `list_node_t { int type; int data; }`.

Typen: `LIST_LIST` (data = Handle auf Kind-Int-Array) · `LIST_STRING` /
`LIST_NUMBER` / `LIST_BOOL` (data = Handle auf Text) · `LIST_NULL` ·
`LIST_HANDLE` (owned mls-Handle, wird von `list_free` freigegeben).

| Funktion | Zweck |
|---|---|
| `list_node(type, data)` | roher Node |
| `list_new()` | neuer leerer LIST-Node |
| `list_string(const char*)` | neuer STRING-Node (kopiert) |
| `list_str(const char*, len)` | Roh-Char-Array (NUL-terminiert) |
| `list_add(h, child)` | Kind anhängen → Index |
| `list_children(h)` / `list_len(h)` / `list_get(h, i)` | Kinder-Handle / Anzahl / i-tes Kind |
| `list_data(h)` / `list_text(h)` | roher data-Handle / Atom-Text (`NULL` bei LIST) |
| `list_type(h)` | Typ |
| `list_free(h)` | rekursiv freigeben |

---

## 9. HDF

HDF ist ein S-Expression-Konfigformat:

```lisp
(server
  (port 8080)
  (host "localhost")
  (debug true)
  (paths (static "/var/www")))
```

Syntax: `( ... )` Listen · `"..."` quoted Strings (Escapes `\n \r \t \\ \"`) ·
`[[...]]` Raw-Strings (`[=[ ... ]=]` mit `=`-Leveln) · `;`-Zeilenkommentare ·
`true`/`false`/`null` · Zahlen. `(rem ...)`-Formen werden übersprungen.

**Zwei unabhängige APIs mit unterschiedlicher Node-Darstellung:**

### 9a. `m_hdf.h` (nur in `mls_full.h`, eigener Baum + **Writer**)

Ein Node = mls-Array `[type, value]`; Typen `HDF_TYPE_LIST/STRING/NUMBER/
BOOLEAN/NULL`.

| Funktion | Zweck |
|---|---|
| `hdf_parse_string(data)` / `hdf_parse_file(path)` | parsen → Root-`LIST`-Handle (`-1` wenn Datei nicht lesbar) |
| `hdf_get_type(h)` | Knotentyp |
| `hdf_get_value(h)` | Atom-Text (`const char*`) oder `NULL` |
| `hdf_get_children(h)` | Kind-Array-Handle (`INT(arr,i)`, `m_len`) oder `-1` |
| `hdf_find_node(h, key)` | Kind finden, dessen 1. Element `key` ist |
| `hdf_get_property(h, key)` | Wert von `(key value)` |
| `hdf_get_int` / `hdf_get_bool` | mit Default |
| `hdf_put_property(h, key, value)` | Property setzen/ergänzen |
| `hdf_write_file(h, path)` | Baum als S-Expressions schreiben |
| `hdf_free(h)` | Baum rekursiv freigeben |

Lese-Muster: `hdf_parse_file` liefert einen Root-`LIST`-Node, dessen Kinder die
**Top-Level-Formen** sind. Für `( "a" "b" )` ist das: Root → 1 Kind (die innere
Liste) → deren Kinder sind die String-Nodes.

```c
int root = hdf_parse_file("cfg.hdf");
int top  = hdf_get_children(root);          // Top-Level-Formen
for (int i = 0; i < m_len(top); i++) {
    int node = INT(top, i);
    if (hdf_get_type(node) != HDF_TYPE_LIST) continue;
    int kids = hdf_get_children(node);
    for (int j = 0; j < m_len(kids); j++)
        printf("%s\n", hdf_get_value(INT(kids, j)));
}
hdf_free(root);
```

### 9b. `hdf_base.h` / `hdf_parse.h` (**kein** Amalgam, nur Reader)

Baut auf `list.h` (`hdf_base_type_t` = `list_type_t`, `HDF_BASE_*` = `LIST_*`).
Kein Writer. Wird z. B. von `examples/hdf_dump.c` genutzt.

`hdf_base_read(text)`, `hdf_base_read_file(path)`, `hdf_base_free(h)`,
`hdf_base_type(h)`, `hdf_base_children(h)`, `hdf_base_str(h)`,
`hdf_base_find(h, key)`, `hdf_base_prop(h, key)`.

**Achtung Namenskollision:** `hdf_parse_file` existiert **zweimal** — in
`m_hdf.c` (Rückgabe `-1` bei Fehler) und in `hdf_parse.c` (Rückgabe `0` bei
Fehler). Beide in einer Multi-File-Anwendung zu linken kollidiert. Im Amalgam
ist nur die `m_hdf`-Variante enthalten (`hdf_parse.c` ist kein Amalgam-Bestand).

---

## 10. `m_subproc.h` (**kein** Amalgam)

`subproc_run(...)`, `subproc_read(...)`, `subproc_lines(...)` zum Ausführen
externer Kommandos inkl. Ausgabe-Einlesen.

---

## 11. HTTP / Flask (nur in `mls_full.h`)

- `m_http.*` — HTTP-Parser/Client.
- `m_http_server.*` — eingebetteter Server.
- `m_flask.*` — Routing über HDF-Konfig.

---

## 12. Fehlerbehandlung & Debug

- Normale API (`m_put`, `mls`, …) beendet bei Programmierfehlern (UAF,
  ungültiger Handle) mit `exit(1)`. `_safe`-Varianten melden behandelbare
  Fehler über Rückgabewert + `mls_errno`:
  `MLS_OK`, `MLS_EINVAL`, `MLS_EBOUNDS`, `MLS_ENOMEM`, `MLS_EUAF`,
  `MLS_EOVERFLOW`. Text: `mls_errmsg(code)`.
- `mls_try(expr)` → Fehlercode; `mls_must(expr)` → meldet + exit; Helfer
  `m_put_try`, `m_setlen_try`, `m_write_try`, `m_read_try`, `m_del_try`.
- `-DMLS_DEBUG`: Wrapper `_m_alloc`/`_m_free`/`_mls`/… merken
  `__LINE__/__FILE__/__FUNCTION__`. **Die erste `[mls error]`-Zeile ist die
  echte Fehlerstelle**; der Post-Mortem-Dump zeigt nur den letzten Wrapper.
- `trace_level` ist eine Bitmaske (Modul-Tracing, je Modul ein Bit).

---

## 13. Fallstricke (Kurzliste)

1. `s_cstr`/`s_ccstr`: Länge **inklusive** Null-Byte → `s_strlen()` für Länge.
2. `m_slice` null-terminiert **nicht**, `s_slice` schon.
3. `mls()`-Zeiger nicht über Resize/`m_put` hinweg halten.
4. `m_init()` vergessen → `Not init.`.
5. Handle nach `m_free` benutzen → UAF-`exit`; vorher `m_is_freed()`.
6. Breite falsch (`sizeof(int)` vs `sizeof(char*)`) → Datenkorruption.
7. Hoher `trace_level` in Hot-Loops → massives I/O.
8. `s_cstr()`/`s_ccstr()` nicht concurrent aufrufen.
9. Handles mit `if (h > 0)` prüfen, nicht auf `NULL`.
10. `hdf_base`/`hdf_parse` und `m_subproc` sind **nicht** im Amalgam.

---

## 14. Beispiele & Makefile-Muster (`examples/`)

- `vpath %.c ../lib`; Debug `.od`/`.exed` (`-DMLS_DEBUG` + ASan), Release
  `.o`/`.exe` (`-O3 -ffunction-sections -Wl,--gc-sections`).
- `APPS = hdf_dump list_demo table_demo csv2insert`; neues Beispiel = `foo.c`
  schreiben + `foo` an `APPS` anhängen.
- `hdf_dump.c` nutzt `hdf_base`; weitere Beispiele: `list_demo`, `table_demo`,
  `csv2insert`.
- `procwatch/` — eigenständiges Amalgam-Beispiel mit **eigenem, minimalem
  Makefile**: `-I../..` bindet `mls_core.h` ein (kein `lib/`-Link nötig),
  Mods `learn`/`exec`/`selftest`. Zeigt die "Python-Komfort"-Primitive:
  `m_dir_list` (statt `opendir`), `m_str_from_file`+`s_trim` (statt `fopen`),
  `m_lookup_mstr` als sortiertes String-Set (Dedup + Membership, O(log n), Key
  als Handle statt C-String), `m_qsort`+`cmp_mstr` (normalisiert die geladene
  Datei) und `m_strlist_save`/`load` (Persistenz). `exec` meldet neue Prozesse
  mit Identität (`pid/uid/gid/ppid/comm`, gelesen aus `/proc/<pid>/status`);
  ein versteckter Name (leerer `comm`) wird einmalig mit `comm=-` gemeldet
  (PID-Dedup per `m_bsearch_int`/`m_binsert_int`). `-v`/`--verbose` für
  Status; default ist still (Unix-Filter). SIGINT/SIGTERM/SIGUSR1 stoppen
  sauber (`learn` schreibt einen finalen Checkpoint); SIGHUP lädt im `exec`
  die Allowlist neu. Ideal als Vorlage für neue Amalgam-Projekte.
- `netwatch/` — zweites Amalgam-Beispiel im selben Muster, Quelle
  `/proc/net/tcp`. Lernt **LISTEN**-Sockets (`listen ip:port`, z. B. eine
  bösartige systemd-Unit/Backdoor) und **ESTABLISHED**-Ziele
  (`connect remote:port`, C2/Exfil). `exec` meldet neue Endpunkte einmalig;
  neue Listener zusätzlich mit Besitzer (`pid/uid/gid/ppid/comm`), ermittelt
  über `/proc/<pid>/fd` → `socket:[inode]` und `/proc/<pid>/{status,comm}`.
  Nutzt `m_strlist_load` (Zeile je Socket), `sscanf`-Parsing (IPv4, lesbar
  dekodiert), `m_lookup_str` und `m_strlist_save`. **Keine neuen lib-Primitive
  nötig** — reiner Konsument. `-v`/`--verbose` schaltet Status/Progress dazu
  (default ist **still** wie ein Unix-Filter: nur Daten auf stdout, Fehler auf
  stderr). Modus `help` (`-h`/`--help`) zeigt eine integrierte Hilfe. SIGINT
  (Ctrl-C), SIGTERM (kill/systemd stop) und SIGUSR1 beenden sauber: `learn`
  schreibt einen letzten Checkpoint, dann `m_destruct`. SIGHUP lädt im
  `exec`-Modus die Allowlist neu (Daemon-Konvention).
- Wie Beispiele knapp bleiben: `docs/guide/lean-code.md` — Fallstudie
  procwatch, Python→C-Primitive und Checkliste fürs nächste Beispiel.
- `.gitignore` ignoriert `*.hdf`, `*.o`, `*.od`, `*.exe`, `*.exed`.
