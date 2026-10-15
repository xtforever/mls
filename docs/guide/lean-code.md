# Weniger Code schreiben: die `procwatch`-Fallstudie

Diese Anleitung beschreibt die Methode, mit der das Beispiel
`examples/procwatch` von ~270 auf 198 Zeilen (176 nicht-leer) reduziert wurde —
**ohne** App-Wissen in die Bibliothek zu tragen. Sie ist als Checkliste für das
nächste Beispiel gedacht.

## Leitsatz

Python ist die Referenz, nicht der Gegner. C darf pro verkettetem
Python-Ausdruck ruhig 2–3 Zeilen brauchen — aber **nicht 20**. Wenn eine
triviale Python-Zeile viele C-Zeilen kostet, fehlt ein kleines, allgemeines
Primitiv in der lib. Die lib wächst dann um dieses Primitiv und wird für alle
folgenden Beispiele komfortabler.

> Beispiel: Python `open(f"/proc/{pid}/comm").read().strip()` ist in C mit
> POSIX-Boilerplate ~18 Zeilen — mit `m_str_from_file` + `s_trim` sind es 2.

## Die Methode (5 Schritte)

1. **Erst naiv schreiben.** Die direkteste C-Version (POSIX-Boilerplate). Sie
   zeigt genau, wo der Schmerz sitzt.
2. **Python-Vorlage danebenlegen.** Jede Komfort-Funktion auflisten:
   `os.listdir`, `open().read().strip()`, `set`, `sorted`, `splitlines`, ...
3. **Vorhandene Primitive suchen** — zuerst in `wiki.md` §6, nicht in den
   Headern. Oft gibt es sie schon (`s_trim`, `m_str_from_file`,
   `m_lookup_str`, `m_strlist_*`, `m_qsort`, `s_isempty`, ...).
4. **Fehlendes Primitiv ergänzen — klein und allgemein.** Name wie die
   Stdlib/Python-Entsprechung. Keine App-Spezifika in die lib.
5. **Lokalen Kleber löschen.** Wrapper, Helfer und Datenstruktur-Zeremonie
   entfernen, sobald die Primitiven komponieren.

## Die konkreten Einsparungen

| Aufgabe | Vorher (C-Boilerplate) | Nachher (Primitiv) | ca. Δ |
|---|---|---|---|
| Datei lesen + trimmen | `fopen`/`fgets`/`fclose`/`strchr` (~19 Z.) | `m_str_from_file` + `s_trim` (2 Z.) | −17 |
| Verzeichnis listen | `opendir`/`readdir`/`closedir` + `<dirent.h>` (~10 Z.) | `m_dir_list` (1 Z.) | −9 |
| Set (Dedup + Membership) | `tbl_create/set/get/len/free` + `tbl_keys` + `save/load`-Wrapper (~40 Z.) | `m_lookup_mstr`/`m_lookup_str` (+ `m_strlist_save/load`) | −35 |
| „Nur Ziffern?“ | `is_pid`-Schleife (9 Z.) | `strspn(pid,"0123456789")==strlen(pid)` (1 Z.) | −8 |
| Persistenz | HDF-Baum bauen + `hdf_write_file` + Parser (~47 Z.) | `m_strlist_save`/`m_strlist_load` (2 Z.) | −45 |
| Handle → C-String → Handle | `m_str(comm)` + lokale `name`-Variable | `m_lookup_mstr(list, comm, ...)` | −1 + Klarheit |
| String-Liste sortieren | eigener `qsort`-Comparator | `m_qsort(list, cmp_mstr)` | −3 |

> Die Δ-Werte sind Richtwerte **je Code-Stelle** (teils gegen den ursprünglichen
> HDF-Entwurf). Sie summieren sich nicht exakt auf die Gesamtdifferenz, weil
> zwischen den Ständen auch Header-Kommentar und Selftest gewachsen sind.

Die konkreten Vorher/Nachher-Ausschnitte:

**Datei lesen + trimmen**

```c
/* vorher: 18 Zeilen POSIX-Boilerplate */
static int read_comm (const char *pid, char *name, size_t n) {
    char path[64];
    snprintf (path, sizeof path, "/proc/%s/comm", pid);
    FILE *f = fopen (path, "r");
    if (!f) return 0;
    if (!fgets (name, (int)n, f)) { fclose (f); return 0; }
    fclose (f);
    char *nl = strchr (name, '\n');
    if (nl) *nl = 0;
    return name[0] != 0;
}

/* nachher: 2 Zeilen (+ Fehlerprüfung) */
tmls path = s_printf (0, 0, "/proc/%s/comm", pid);
tmls comm = m_str_from_file (m_str (path));
if (comm < 0) continue;
s_trim (comm);
```

**Set (Dedup + Membership) + Persistenz**

```c
/* vorher: Table-Zeremonie + Snapshot-Helfer + Datei-Wrapper (~40 Zeilen) */
int tbl = tbl_create ();
tbl_set (tbl, name, "1");
if (tbl_get (tbl, name) != NULL) ...
int keys = tbl_keys (tbl);
m_strlist_save (keys, path);          /* + m_free(keys) */
...
int names = m_strlist_load (path);
m_foreach (names, i, p) tbl_set (tbl, *p, "1");

/* nachher: eine String-Liste ist das Set und die Datei in einem */
int list = m_alloc (0, sizeof (char *), MFREE_STR);
if (m_lookup_mstr (list, comm, 1) < 0) {   /* unbekannt? */
    ...
    m_lookup_mstr (list, comm, 0);          /* merken  */
}
m_strlist_save (list, path);                 /* fertig  */
```

## Die 4 neuen Primitive — und warum sie allgemein sind

| Primitiv | Entspricht | Zweck |
|---|---|---|
| `m_strlist_save` / `m_strlist_load` | `"\n".join` / `splitlines` | Liste ⇄ Textdatei, eine Zeile je Eintrag |
| `m_dir_list` | `os.listdir` | Verzeichnis-Einträge als `char*`-Liste |
| `cmp_mstr` | `sorted()` | `qsort`-Comparator für `char*`-Listen |
| `m_lookup_mstr` | `set` / `in` | sortiertes Set (O(log n)) mit **Handle**-Key (Pendant zu `m_lookup_str`) |

Jedes Primitiv ist ~10–20 Zeilen, nach der Python/Stdlib-Entsprechung benannt
und enthält **kein** App-Wissen. Genau deshalb ist es im nächsten Beispiel
wiederverwendbar.

## Checkliste für das nächste Beispiel

- [ ] Python-Vorlage grob danebenlegen; jede Zeile einer C-Entsprechung
      zuordnen.
- [ ] Wenn eine Python-Zeile >3 C-Zeilen kostet: **fehlt ein Primitiv?**
- [ ] Datei-/Verzeichnis-/String-Schleifen nie von Hand — erst `wiki.md` §6.
- [ ] Handles bevorzugen; `m_str()` / C-Strings nur an der I/O-Grenze.
- [ ] Bestehende Primitive zuerst verwenden, dann ergänzen.
- [ ] Fehlendes Primitiv in `lib/` ergänzen, nicht lokal als `static`
      verstecken — sonst wiederholt sich der Boilerplate im nächsten Beispiel.
- [ ] Klein und orthogonal halten; keine Frameworks. Erst wenn zwei Beispiele
      es brauchen, verallgemeinern.
- [ ] Nach lib-Änderungen Amalgam + Docs regenerieren:
      `python3 generate_amalgam.py` und `python3 generate_docs.py`.
- [ ] Ein runnbarer Check bleibt (Selftest / Häkchen), siehe AGENTS.md.

## Anti-Patterns, die den Code lang gemacht haben

- `opendir`/`fopen`-Boilerplate direkt im Programm.
- Datenstruktur-Zeremonie (Table anlegen/füllen/freigeben), wo ein
  Set-Primitiv reicht.
- Handle → C-String → Handle hin- und herkonvertieren.
- Ein Spezialformat (HDF-Baum) für eine simple „eine Zeile je Eintrag“-Datei.
- Lokale `static`-Helfer, die allgemein wären.

## Ergebnis

- `examples/procwatch/procwatch.c`: **~270 → 198 Zeilen** (176 nicht-leer);
  die Kernlogik (`scan_procs` + `learn` + `exec`) ist deutlich geschrumpft.
- Die lib wuchs um 4 kleine, wiederverwendbare Primitive.
- Das nächste Beispiel startet auf diesem Komfort-Niveau.

## Wichtig: nicht Zeilen um der Zeilen willen

Die lib bleibt schlank — nur kleine, orthogonale Primitive. „Lazy“ heißt
weniger Code, **nicht** der flimsier Algorithmus (siehe AGENTS.md). Beispiel:
`m_lookup_mstr` hält die Liste sortiert und sucht binär (O(log n)); man muss
also nur `m_qsort(list, cmp_mstr)` aufrufen, wenn die Liste von außen kommt
(z. B. eine geladene Datei). Das richtige Primitiv wählen, nicht das kürzeste.

## Fallstudie: `net_line_record()` — Python-Baseline vs. C

Python-Baseline: `examples/netwatch/net_line_record.py` (C: `netwatch.c`).
Beide liefern dieselben Records; die Python-Version dient als Maßstab.

```python
def ip4(hexip):
    ip = int(hexip, 16)
    return f"{ip & 0xff}.{ip >> 8 & 0xff}.{ip >> 16 & 0xff}.{ip >> 24 & 0xff}"

def net_line_record(line):
    f = line.split()
    if len(f) < 4 or not f[0].endswith(":"):
        return None
    local, remote, st = f[1], f[2], f[3]
    if st not in ("0A", "01"):
        return None
    lip, lport = local.rsplit(":", 1)
    rip, rport = remote.rsplit(":", 1)
    if st == "0A":
        return f"listen {ip4(lip)}:{int(lport, 16)}"
    return f"connect {ip4(rip)}:{int(rport, 16)}"
```

| Aspekt | Python | C |
|---|---|---|
| Logik-Zeilen | ~13 | ~30 |
| Tokenisieren | `line.split()` | `sscanf(" %*d: %63s %63s %7s", …)` |
| Header überspringen | implizit (`f[0]` endet nicht auf `:`) | implizit (`sscanf` schlägt fehl) |
| Hex → int | `int(x, 16)` | `sscanf("%x", &n)` |
| Adresse teilen | `rsplit(":", 1)` | `strchr` + Pointerarithmetik |
| Formatieren | f-string | `snprintf` + Puffer + Größe |
| Rückgabe | `str` / `None` | `int` (1/0) + Out-Puffer |
| Speicher | automatisch | Stack-Puffer, keine Allokation |

**Fazit:** Die Logik ist 1:1 — der Mehr-Code ist reines Puffer-/Rückgabe-
Plumbing (Lifetime, Größen, Fehlercode). Python ist ~2,3× kompakter.
**Kein neues lib-Primitiv gerechtfertigt:** das ist Domänen-Parsing, `sscanf`/
`snprintf` ist das richtige Werkzeug. Die C-Version ist so schlank, wie C hier
vernünftig wird.

*mls-idiomatische Alternative:* statt Out-Puffer `s_printf(0,0,…)` einen
String-Handle zurückgeben und im Aufrufer `m_lookup_mstr` + `tmls` nutzen —
kürzere Signatur, aber eine Allokation je Zeile. Die Puffer-Variante ist zur
Laufzeit schlanker; die richtige Wahl hängt vom Hot-Path ab.

*Cross-Check:* beide Self-Tests nutzen dieselben Vektoren, und auf der Live-
`/proc/net/tcp` liefern beide exakt dieselben 19 Listener-Records (diff leer).
