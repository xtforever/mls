# Pflichtenheft: Forth als SQL-Ersatz auf ISAM (mls)

Status: Entwurf v8 — **Single Source of Truth**. Phase 0–4 (Schritte 0.1–0.20,
Monolith + Daemon) sind implementiert; Skalierungs-/Multi-Core-Erkenntnisse in
**§14**. M1.1-Persistenz ist in **§11a.7** ausgearbeitet: **Schritte 1–2**
(persistente Schema-Registry `meta`/`.dat`-Header + persistenter String-Pool
`strings.str`) sind umgesetzt; Record-Paging und Laufzeitmessung folgen.
Schema-DDL +
ALTER-per-Copy (FR-42/FR-43, §8.7) sind auf dem In-Memory-Kern umgesetzt.
Kontext: `../forth-db-c` (Referenz-Forth-VM), `mls` (Handle-Bibliothek).

> Dieses Dokument ist die alleinige Quelle der Wahrheit für Anforderungen,
> Architektur, Protokoll und Phasenplan. Bei Widerspruch gilt dieses Dokument;
> Änderungen werden hier eingetragen.

> **v2-Korrektur (Intention präzisiert):** Forth ist die **native API und der
> Multiplexer** (nicht nur das Wire-Protokoll). Der Server bindet API-Wörter
> **pro Tabelle** an Stored Procedures/Views und kann transparent auf gesplittete
> Tabellen oder andere DB-Server auslagern — der Client-Code bleibt unverändert.
> Fokus: Applikationsebene, millionenstarke Time-Series-Tabellen.
> Crash-Safety/Write-Logs/Transaktionen sind **Backlog (später)**.

> **v3-Korrektur (Vollständigkeit, Baseline & Cursor/Anti-Join):** Primärschlüssel-Typ `i`/`s` und
> **Composite-PK** erlaubt; Datumstyp `d`; Identität = **Unique-ID** (recno
> intern, monoton/nie wiederverwendet); **Storage-Engines pluggable** (RRD,
> Engine als Forth-Code = Backlog); **Joins = verschachtelte Cursor-Loops ab M1**
> (`DB-JOIN` nur Convenience); **Baseline §6.7 (Artikel/Bestellung/Kunde)** als
> Abnahmebasis; **Anti-Join ohne persistente Temp-Tables** (§8.5) mit
> **unabhängigen Cursorn + Scan/Range/Mengen** (FR-40).

> **v4-Ergänzung (Implementierungsplan):** Inkrementeller, **Monolith-first**-Pfad
> mit sauberer API-Grenze (`query_exec`: Forth-Skript rein/raus). Zuerst DB-Kern +
> App in *einem* Prozess, dann Daemon-Ausgliederung als reiner Backend-Tausch.
> Kleine, wiederverwendbare Module; grüne Module werden eingefroren. Details in
> **§11a**.

> **v5-Ergänzung (Forth-Engine):** Recherche zu etablierten Forth-Engines
> (ANS/Forth-2012, Threading-Modelle, Code-Wörter). Entscheidung: **DTC-artiger
> Interpreter in portablem C**, keine Native-Compilation; Hybrid (C-Primitives)
> ist der Normalfall. Verbindliche Wordset-Ergänzungen für `m_forth`:
> Execution Tokens, **`DEFER`/`IS`**, **Wordlists/Search-Order**, `ABORT`/`CATCH`/
> `THROW`, `LITERAL`/`POSTPONE`, Underflow-Erkennung. Details in **§5.5**.

> **v6-Ergänzung (Skalierung, Stand Phase 0–4):** Die Schritte 0.1–0.20 sind
> umgesetzt (generische Primitive, Pool/VM, DB-Kern, Monolith, Daemon) und je
> Modul durch einen assert-Selbsttest abgesichert. Für „eine Million Nutzer“ gilt
> das Lastprofil **viele kurzlebige Verbindungen, read-heavy, eine Maschine
> zuerst**. Die daraus abgeleiteten Engpässe, das Multi-Core-Konzept und die
> Architektur-Invarianten stehen in **§14**. Es werden **jetzt keine** Skalierungs-
> Umbauten vorgenommen; Ziel ist, die spätere Anpassbarkeit nicht zu verbauen.

> **v7-Ergänzung (M1.1 Persistenz + Laufzeitmessung):** Der nächste Schritt hebt
> `m_isam` auf **seitenbasierte Persistenz** (Pager) und bindet den persistenten
> String-Pool (`m_strpool`) an; der B⁺-Index bleibt in-memory und wird beim Open
> neu aufgebaut. Zusätzlich können Tests die **Laufzeit** von Forth-Skripten
> messen (gesamt + pro Wort, C-API, kein Wire-Change). Details + Schritte in
> **§11a.7**.

> **v8-Ergänzung (Schema-DDL + ALTER-per-Copy):** Schema-Erstellung ist jetzt
> auch per Forth möglich (`DB-CREATE-TABLE`/`DB-COL`/`DB-PRIMARY`/
> `DB-TABLE-CREATE`), nicht nur über die C-API. `ALTER TABLE` läuft als
> **Copy-and-Swap** (`DB-COPY-TABLE`/`DB-DROP-TABLE`/`DB-RENAME-TABLE`);
> hinzugefügte Spalten werden mit `0` (kein NULL) aufgefüllt. Persistente
> Schema-Ablage (`meta`) bleibt Teil von **§11a.7** Schritt 1. Details **§8.7**.

---

## 1. Zielbestimmung

### 1.1 Zweck
Eine Datenbank, die **nativ auf Forth** arbeitet (statt SQL). Forth-Wörter sind
die **stabile API**: der Client ruft sie auf, der Server bindet sie **pro Tabelle**
an Implementierungen (Stored Procedures, Views) und kann sie transparent auf
gesplittete Tabellen oder andere DB-Server auslagern — **ohne Client-Änderung**.
Komfort liefern dünne Helper-API-Funktionen (C) obendrauf. Ziel ist die
**Applikationsebene**: präzise, deterministische Steuerung des Zugriffs auf
komplexe, millionenstarke Tabellen (Time-Series) statt eines undurchsichtigen
SQL-Optimizers.

**Forth als Multiplexer**: ein Input (API-Aufruf) → mehrere mögliche Outputs
(welche Tabelle/welcher Server ihn bedient), server-seitig aufgelöst.

### 1.2 Ziele (MoSCoW für die erste Auslieferung)
- **Must**: zwei Prozesse (DB-Daemon + App), Unix-Socket, Forth-API als
  Wire-Protokoll, namensbasierter Spaltenzugriff, drei Standard-Queries
  (SELECT/INSERT/DELETE), Resultset mit Spaltenmetadaten, Demo, Ketten-Showcase,
  **Stored Procedures, Views, Per-Table-Override, Capability-Query (pro Tabelle)**.
- **Should**: Paginierung (COUNT+Seite in einem Round-Trip), Sekundärindizes,
  Aggregation, UPDATE/Projektion, `DB-STATS`, Multi-Client,
  **Tabellen-Splitting, Offloading, Record-by-ID (Routing)**.
- **Could**: Resultset-Cache, Join-Convenience (`DB-JOIN`), Streaming,
  B⁺-Baum-Count/Rank, SELECT→Forth-Übersetzer.
- **Später (Backlog, nicht Teil der ersten Auslieferung)**: Crash-Safety
  (Stromausfall), Write-Logs (WAL), Transaktionen, pluggable Storage-Engines
  (RRD-Ring, Engine als Forth-Code).

### 1.3 Nicht-Ziele (Out of scope)
- Vollständige SQL-Syntax-Kompatibilität (wir sind **kein** relationales System).
- Replikation/verteilte Konsistenz (Offloading ist zunächst funktional, nicht
  transaktional abgesichert).
- Client in einer anderen Sprache als C (Phase 1); ein Forth-nativer Client ist optional.
- NULL/3-wertige Logik (bewusst ausgeschlossen, siehe §3.2).
- Crash-Safety/WAL/Transaktionen (siehe Backlog).

### 1.4 Nutzer
- Anwendungsentwickler, die präzise DB-Zugriffe brauchen (Batch, Navigation,
  früh abbrechende Scans).
- Betreiber, die I/O-Kosten vorhersagbar machen wollen.

---

## 2. Ausgangslage

- **`../forth-db-c`**: funktionsfähige C11-Forth-VM (~812 Zeilen `forth.c` +
  kleine Helfer arena/carray/darray/strbuf/htable/token). Eigenes Speichermodell,
  Strings als `addr+len`, globales `g_vm`, Dictionary leakt. **Nur Referenz.**
- **`mls`**: C11-Handle-Bibliothek. Strings als `int`-Handles; `s_cstr` (zero-copy,
  stabil/dedup, global) und `s_cstrdup` (Kopie). Sortierte Sets, `list_*`,
  `tbl_*`, `m_bsearch_int`. **Heimat der Implementierung.**
- **Fehlt**: ISAM-Engine, persistierbarer String-Pool, Forth-Integration,
  Wire-Protokoll, Client, Demo, Showcase.

---

## 3. Design-Rationale & Abgrenzung (Recherche)

### 3.1 Was SQL gegenüber ISAM verloren hat
SQL bot zwei Vorteile gegenüber ISAM/VSAM: (a) viele Records mit einem Befehl,
(b) man muss nicht angeben, **wie** ein Record erreicht wird. Genau (b) ist der
Verlust — es folgt eine Liste der zurückzuholenden ISAM-Vorteile:

1. **Explizite Zugriffskontrolle („HOW“)**: ISAM/VSAM = Entwickler nutzt direkt
   eine API, um Indizes zu durchsuchen; relational = Query-Optimizer wählt
   Indizes automatisch. Verloren: deterministische, zählbare, vorhersagbare I/O.
2. **Gezielter Zugriff**: kleine Indizes (oft im RAM), nur die benötigten Records
   werden gelesen; kein Materialisieren ganzer Relationen.
3. **Physische Stabilität**: Records werden nicht verschoben (Insert →
   Overflow/B⁺-Baum), stabile `recno`/RBA → billiger Direktzugriff.
4. **Navigationale Abfragen**: Ketten/Zeiger folgen; SQL braucht dafür
   rekursive CTEs (erst ab SQL:1999), ohne frühen Abbruch, O(n²) bei Zyklen.
5. **Batch-Verarbeitung**: große sequentielle Dateien mit bekannter I/O-Folge.

**Bewusster Trade-off**: Codd's Rule 8 (*physical data independence*) ist der
SQL-Gewinn; sein Preis ist die Aufgabe der Kontrolle. Wir wählen **Kontrolle
statt Abstraktion** und benennen den Verlust offen (§13 Risiko R-1).

### 3.2 Größte SQL-Kritikpunkte und unsere Gegenmaßnahmen

| SQL-Kritik | Unsere Antwort |
|---|---|
| **NULL + 3-wertige Logik** (UNKNOWN, `NOT IN`-Fallstricke, `NULL = NULL` nicht true; Date/Darwen, Libkin) | **Kein NULL, keine 3VL.** Fehlen = expliziter Sentinel/Handle (0 = „kein Nachfolger“) oder Optional-Typ. (NFR-03) |
| **Duplikate** (SQL-Tabellen sind Listen, keine Mengen) | **Eindeutiger Primärschlüssel** (auch composite); client-seitige Identität = stabile **Unique-ID**, `recno` intern. (FR-12/FR-32) |
| **Impedance Mismatch** (deklarativ vs. prozedural) | **Forth ist prozedural**; Query = Forth-Skript, Resultset = Forth-Wert. Cursor/`NEXT` nativ. (NFR-09) |
| **Abweichung vom relationalen Modell** (Codd selbst; Third Manifesto) | Wir **behaupten gar nicht relational zu sein** — offen navigational/ISAM. |
| **Orthogonalität/Vollständigkeit** (PK/Subqueries erst 1992, JSON 2016) | Kleine, **komponierbare Forth-Wörter**. |
| **Vendor-Lock-in / Dialekte** | **Ein** Dialekt; die VM ist die Referenz. |
| **Undurchsichtiger Optimizer** (Plan-Katastrophen) | **Kein Optimizer.** Der Zugriffsplan ist das Forth-Skript; `DB-STATS` zeigt die I/O. (NFR-02) |

### 3.3 COBOL/VSAM: Performance ohne SQL
COBOL spricht VSAM/ISAM **direkt** (`OPEN/READ/WRITE/REWRITE/START`; START =
Record per Key suchen) — exakt unsere `DB-*`-Wörter. Kein SQL-Layer, kein
Optimizer. Größenordnung: 1997 „80 % der Geschäftswelt“ auf COBOL (>200 Mrd.
Zeilen); 2020 liefen ~95 % der Kartenzahlungen über COBOL-Backends.
Performance-Gründe: (a) vorhersehbarer Direktzugriff (VSAM KSDS = B⁺-Baum,
Control Intervals), (b) Batch großer sequentieller Dateien, (c) feste
Datenstrukturen (keine Laufzeit-Interpretation), (d) prozedural ohne
Impedance-Mismatch.

**Positionierung**: Die Leistung kommt von VSAM/ISAM + Batch, **nicht von
COBOL**. Wir holen die Leistungsmerkmale zurück (Direktzugriff, feste Records,
Batch, prozedural) und vermeiden COBOLs Mängel (Verbosity, Monolithen,
~300 Dialekte) durch knappe, komponierbare Forth-Syntax.

### 3.4 Forth als Interface/Multiplexer
SQL fixiert das **relationale Modell**: die Semantik der Abfrage ist vorgegeben,
der Optimizer wählt den Plan. Forth fixiert nur die **Wort-Schnittstelle**: der
Name eines API-Worts (z. B. `RECORD-BY-NUM`, `HISTORY-BY-DATE`) ist der Vertrag;
die **Implementierung** liegt server-seitig und ist **pro Tabelle** austauschbar
(Stored Procedure, View, Split-Tabelle, anderer Server). Ein Input, mehrere
mögliche Outputs — der Multiplexer ist das Forth-Wörterbuch des Servers.

Das ermöglicht die eigentliche Anforderung: **Speicherung nachträglich auf
mehrere Tabellen und Server auslagern, ohne den Client-Code zu ändern**.

### 3.5 Quellen
- Wikipedia: *ISAM*, *Virtual Storage Access Method (VSAM)*, *SQL* (Abschnitt
  Criticisms), *COBOL*, *Codd's 12 rules*.
- Codd, E. F.: *A Relational Model of Data for Large Shared Data Banks* (1970);
  *Codd's 12 rules* (1985).
- Date, C. J. & Darwen, H.: *The Third Manifesto*.
- Libkin et al.: *SQL's Three-Valued Logic and Certain Answers* (TODS 2016).
- Gray, J. & Reuter, A.: *Transaction Processing: Concepts and Techniques*
  (Slotted-Page-Heap, TID, Tombstone).
- Rosenblum, M. & Ousterhout, J.: *The Design and Implementation of a
  Log-Structured File System* (1991); O'Neil et al.: *LSM-Tree* (1996).
- Comer, D.: *The Ubiquitous B-Tree* (ACM Computing Surveys, 1979) — B/B⁺/B\*.
- Oetiker, T.: *RRDtool* — Round-Robin Database.
- Forth: Wikipedia *Forth (programming language)* und *Threaded code*;
  Forth-2012-Standard (forth-standard.org); Ertl, M. A.: *Speed of various
  interpreter dispatch techniques*; Moore, C. H. (ITC); Bell, J. R.: *Threaded
  Code* (CACM 1973).

---

## 4. Anforderungen

### 4.1 Funktionale Anforderungen (FR)

| ID | Anforderung | Prio | Meilenstein |
|---|---|---|---|
| FR-01 | DB läuft als **separater Prozess** (Daemon); Kommunikation über **Unix-Domain-Socket** | M | M0 |
| FR-02 | **Forth-Skript-Protokoll**: Query = Forth-Skript rein, Antwort = Forth-Skript raus | M | M0/M1 |
| FR-03 | **Framing** (length-prefixed), streaming-fähig, Trunkierungs-/Größenprüfung | M | M0 |
| FR-04 | **Client-C-Bibliothek** `m_fclient` (`fdb_open/close/select/insert/delete`) | M | M1 |
| FR-05 | **Handle-Invariante**: String-Gleichheit = Handle-Vergleich; Handles prozess-lokal; Wire trägt nur Content | M | M0/M1 |
| FR-06 | **Namensbasierter Spaltenzugriff** (`DB-GET`/`DB-PUT`/`DB-FIND` per Name) | M | M1 |
| FR-07 | **Drei Standard-Queries**: SELECT / INSERT / DELETE | M | M1 |
| FR-08 | **Resultset mit Spaltenmetadaten** (Name+Typ, MariaDB-Eigenschaft) | M | M1 |
| FR-09 | **Demo-Programm** mit `m_init/open_database/select/print_result/close_database` | M | M1 |
| FR-10 | **Showcase**: Ketten-Verfolgung mit Zykluserkennung (Handle-Set) | M | M1 |
| FR-11 | **Mehrere Tabellen pro Database**, benannte Spalten | M | M1 |
| FR-12 | **B⁺-Baum-Primärindex**, feste Records, stabile `recno` (monoton, nie wiederverwendet), Tombstone-Flag, eindeutiger PK (Typ `i`\|`s`, auch composite) | M | M1 |
| FR-13 | **String-Pool** (`m_strpool`) mit stabilen, persistierten `uint32`-IDs | M | M0 |
| FR-14 | `DB-STATS` (I/O-Zähler pro Query) | S | M1 |
| FR-15 | **Paginierung**: COUNT + Seite in **einem** Round-Trip (Server-Resultset) | S | M2 |
| FR-16 | **Sekundärindizes** (`DB-INDEX`/`DB-SEEK-IX`), Range-Scans | S | M2 |
| FR-17 | **Aggregation**: Standard-Aggregate `SUM/COUNT/AVG/MIN/MAX` + `GROUP-SUM`/`GROUP-AVG`/`GROUP-COUNT` (§8.6), `SORT`, `FILTER` | S | M2 |
| FR-18 | **UPDATE**, Projektion per Feldliste | S | M2 |
| FR-19 | **Resultset-Cache** (LRU, Key = Query-Hash + data-version) | C | M3 |
| FR-20 | **Join-Convenience** (`DB-JOIN`); Joins als verschachtelte Cursor-Loops sind bereits ab M1 möglich | C | M3 |
| FR-21 | *(entfallen — Transaktionen/WAL → Backlog §4.4)* | — | — |
| FR-22 | **Streaming-Antworten** (`DB-STREAM`) | C | M3 |
| FR-23 | **Multi-Client-Concurrency** (Reader/Writer, Writer-Lock pro DB-Datei) | S | M2/M3 |
| FR-24 | **B⁺-Baum-Count/Rank** (`DB-COUNT-RANGE`, `DB-NTH`), Keyset-Paging | C | M3+ |
| FR-25 | Optionaler **SELECT→Forth-Übersetzer** | C | M4 |
| FR-26 | **Stored Procedures**: server-seitige benannte Forth-Wörter, an eine Tabelle gebunden | M | M2 |
| FR-27 | **Views**: Forth-Wörter, die eine abgeleitete Tabelle präsentieren (z. B. Union über Split-Tabellen) | M | M2 |
| FR-28 | **Per-Table-Override**: Wort-Lookup = Tabellen-Wörterbuch → globales → Fehler | M | M2 |
| FR-29 | **Capability-Query pro Tabelle** (`HAS-WORD`/`CAPS`); Fallback oder Fehler bei fehlender Erweiterung | M | M2 |
| FR-30 | **Tabellen-Splitting on demand** (z. B. nach Jahr), transparent für den Client | S | M3 |
| FR-31 | **Offloading** auf andere Tabellen / andere DB-Server, transparent für den Client | S | M3 |
| FR-32 | **Record-by-ID**: globale Unique-ID (uint64); Direktzugriff ODER Tabellen-/Server-Override | S | M3 |
| FR-33 | **Forward-Kompatibilität**: alte Clients funktionieren, wenn der Server Wörter/Overrides ergänzt | M | M2 |
| FR-34 | **Time-Series-Skala**: Millionen Zeilen/Tabelle handhabbar | S | M2/M3 |
| FR-35 | **Storage-Engine-Abstraktion**: Tabelle wählt eine Engine; Engine = Storage-Primitive + Hooks | S | M3 |
| FR-36 | **RRD-artige Ring-Engine** (Slot-Berechnung + Slot-Reuse, konstante Größe) | C | Backlog |
| FR-37 | **Engine-Hooks als Forth-Wörter** (`PRE-INSERT` …), pro Tabelle überschreibbar | S | M3 |
| FR-38 | **Engine vollständig als Forth-Code ladbar** | C | Backlog |
| FR-39 | **Referenz-Datenmodell** (Artikel/Bestellung/Kunde, §6.7) als Abnahmebasis abfragbar | M | M2 |
| FR-40 | **Unabhängige Cursor + Scan/Range + Mengen-Primitive** (`DB-SCAN`/`DB-RANGE`/`CUR-*`, `DB-LOOKUP`, `SET?`/`SET-ADD`) | M | M1 |
| FR-41 | **Laufzeitmessung**: Tests messen die Forth-Skript-Laufzeit **gesamt + pro Wort** über die C-API (`m_engine_elapsed_ns`, `m_forth_prof*`); kein neues Forth-Wort, kein Wire-Change. Dient der Identifikation von Optimierungsquellen | S | M1.1 |
| FR-42 | **Schema-DDL**: Tabellen/Spalten/PK per Forth anlegen (`DB-CREATE-TABLE`/`DB-COL`/`DB-PRIMARY`/`DB-TABLE-CREATE`); Schema-Ablage (`meta`) persistent (§11a.7) | M | M1.1 |
| FR-43 | **ALTER per Copy**: Spalte add/drop/rename + Typänderung via create→copy→swap (`DB-COPY-TABLE`/`DB-DROP-TABLE`/`DB-RENAME-TABLE`); Introspektion (`DB-TABLES`/`DB-COLS`) | M | M1.1 |

### 4.2 Nicht-funktionale Anforderungen (NFR)

| ID | Anforderung |
|---|---|
| NFR-01 | C11; keine neuen Abhängigkeiten außer POSIX + bestehende mls-Module |
| NFR-02 | Deterministische, zählbare I/O; **kein Query-Optimizer** |
| NFR-03 | **Kein NULL / keine 3-wertige Logik** |
| NFR-04 | Speichersicher (ASan/valgrind sauber; intentional-globale Stores dokumentiert) |
| NFR-05 | VM re-entrant; Server serialisiert pro Verbindung; single-writer pro DB-Datei (bis M3) |
| NFR-06 | Portabilität: Linux/POSIX; Little-Endian/x86-64 dokumentiert |
| NFR-07 | Ein lauffähiger, assert-basierter Selbsttest pro nicht-trivialer Logik (kein Framework) |
| NFR-08 | Kein Impedance Mismatch: Query/Resultset sind Forth-nativ |
| NFR-09 | Doku: QUICKREF/wiki + `docs/guide/forth-sql.md`; pre-commit regeneriert `docs/api` |
| NFR-10 | Vorhersagbare Latenz: `DB-SEEK` = O(log n) Index-Reads + 1 Datenseiten-Read |
| NFR-11 | Client-Code-Stabilität: Speicher-/Sharding-Änderungen erfordern keine Client-Änderung |

### 4.3 Randbedingungen
- Umsetzung in `mls`, Module unter `lib/`.
- `../forth-db-c` ist Referenz, wird nicht verändert.
- AGENTS.md: lazy aber edge-case-korrekt, `ponytail:`-Kommentare für Decken,
  Vertrauensgrenzen validieren.
- Debug-Builds mit ASan/`-DMLS_DEBUG`.

### 4.4 Backlog (später, nicht erste Auslieferung)
- **Crash-Safety** (Stromausfall): dauerhafte Konsistenz nach Prozess-/Stromausfall.
- **Write-Logs (WAL)**: Logging + Recovery.
- **Transaktionen** (`DB-BEGIN/COMMIT/ROLLBACK`).
- **Pluggable Storage-Engines**: RRD-artiger Ring (Slot-Reuse) und Engine
  vollständig als Forth-Code (FR-36/FR-38).

> Bewusst zurückgestellt: Die erste Auslieferung ist funktional, **nicht**
> transaktional abgesichert.

---

## 5. Systemarchitektur

### 5.1 Zwei-Prozess-Modell
```
App-Prozess                              DB-Prozess (Daemon)
 m_fclient (C) + m_ftransport            m_forth (VM) + m_isam (DB-*)
   ── Forth-Skript-Stream ──► Unix ──►   m_strpool (DB-Pool, persistiert)
   ◄── Forth-Skript-Stream ── Socket     m_fserver (accept-Loop)
```
Die App linkt **niemals** `m_isam` — sie kennt die DB nur über das Protokoll
(Server-Modell, nicht SQLite-Modell).

### 5.2 Module & Verantwortlichkeiten

| Modul | Client | Server | Zweck |
|---|---|---|---|
| `m_forth` | ❌ (Phase 1) | ✅ | Forth-VM, nur Daemon-seitig |
| `m_strpool` | ❌ (Phase 1) | ✅ | persistierbarer String-Pool (stabile, append-only IDs) |
| `m_isam` | ❌ | ✅ | Seiten-I/O, B⁺-Baum, `DB-*`-Wörter |
| `m_fclient` | ✅ | ❌ | C-API: connect, Queries, Resultset |
| `m_fserver` | ❌ | ✅ | Daemon, pro Verbindung VM+DB |
| `m_ftransport` | ✅ | ✅ | Framing (`uint32 len` + Payload), Streaming |

Abhängigkeiten: `m_forth` → `m_strpool` (Interning-Callback); `m_isam` →
`m_forth` + `m_strpool`; `m_fserver` → `m_isam`; `m_fclient` → nur
`m_ftransport` (+ mls-Basis). `m_forth` kennt `m_isam` nicht.

> **Monolith-first (v4):** Der inkrementelle Pfad startet **in-process** mit
> `m_engine` (`query_exec`) statt `m_fclient`; die Daemon-Module ersetzen später
> nur das Backend. Die vollständige Modul-Liste (inkl. generischer Primitive
> `arena`/`bptree`/`pager`/`hashmap`/… sowie `m_dbwords`/`m_fapi`/`m_fproto`)
> steht in **§11a.3**.

### 5.3 Handle-Invariante
Ein String-Handle (Pool-ID oder conststr-Handle) gilt nur im eigenen Prozess.
**Über die Leitung laufen nur Inhalte** (`S" text"` / `Z" text"`, escaped).
Jeder Prozess interniert lokal; `=` vergleicht lokale Integers. Das
Antwort-Skript ist deshalb selbsttragend (Content statt Handle).

### 5.4 String-Räume
1. **Konstanten** (Wortnamen, Vokabular, Pfade, Formatstrings) → `s_cstr`/conststr:
   global, dedup, zero-copy, stabil.
2. **Daten** (Query-Literale, DB-Werte) → `m_strpool`: pro DB, persistierbar,
   unbeschränkt, stabile `uint32`-IDs.

Forth: `S"` → Pool-ID des aktuellen Kontexts; `Z"` → `s_cstr`-Handle;
`S.`/`Z.` → auflösen+ausgeben; `=` vergleicht IDs desselben Raums. `S"`/`Z"`
kompilieren den Literaltext **inline** + Laufzeit-Wort `(S")`/`(Z")` →
Definitionen sind DB-unabhängig.

Ein drittes Literal (z. B. Backtick) wird **nicht** eingeführt — `S"`/`Z"`
decken beide Räume ab.

### 5.5 Forth-Engine-Entscheidungen

Grundlage: Recherche zu etablierten Forth-Engines (Wikipedia *Forth*,
*Threaded code*; Ertl's Dispatch-Studie).

**Threading**: Die VM nutzt ein **DTC-artiges Modell** in portablem C — eine
Definition ist ein Array aus `word_t*` (plus getaggten Literalen), dispatcht in
einer C-Schleife. Das ist für uns richtig: der heiße Pfad sind die **C-DB-
Primitive**, nicht der Interpreter. **Kein** Native-Code/JIT (YAGNI), keine
Locals, kein Files-Wordset (Host-C erledigt I/O); **keine** volle
ANS-Konformität — wir bauen ein Hybrid/Domain-Specific-System.

**Hybrid („Zwitter") ist der Normalfall**: Ein Forth-Wort besteht aus Head
(Name+Link) und Body (Code-Field+Parameter-Field); der **Code-Field zeigt auf
Maschinencode** (C/Asm-Primitive) oder Threaded Code. `forth_define(name,
C-Funktion, immediate)` ist genau dieser Code-Word-Mechanismus; die
`DB-*`-Wörter sind daher **C-Primitives** (kein „reines Forth-OS").

**Wordset-Anforderungen an `m_forth`** (aus der Recherche, für unser Design
verbindlich):

| Priorität | Wörter | Grund |
|---|---|---|
| **Must** | Execution Tokens (`'`, `:NONAME`, `EXECUTE`), **`DEFER`/`IS`**, **Wordlists/Search-Order** (Wörterbuch pro Tabelle), `ABORT`/`CATCH`/`THROW`, `LITERAL`/`POSTPONE`, Underflow-/Fehler-Erkennung | Per-Table-Override (FR-28), Stored Procedures (FR-26), Views (FR-27), Engine-Hooks (FR-37) sind ohne `DEFER`/Wordlists nicht umsetzbar |
| **Should** | `CASE/OF/ENDOF`, `RECURSE` | Dispatch, rekursive Procedures |
| **Could** | `VALUES`/`TO`, `MOVE`/`FILL`/`ERASE` | Komfort |
| **Nicht nötig** | Native-Code, Locals, Files-Wordset, volle ANS-Konformität | hot path = C-Primitive; Host-C macht I/O |

---

## 6. Datenmodell

### 6.1 Database / Table / Column
- **Database** = Verzeichnis mit mehreren **Tabellen**.
- **Tabelle** = benannte Spalten `(name, type)`; `i`=int64, `d`=Unix-Zeitstempel
  (int64), `s`=String(Pool), `z`=konstant(conststr). Zugriff **per Name**, nie
  per Position.
- **Primärschlüssel — zwei Modi**:
  - **KSDS-artig**: benannter Key = Spalten-Präfix (Spalte 0, optional
    **composite**), Typ `i` oder `s`, eindeutig (Business-Key für den B⁺-Zugriff).
  - **RRDS-artig**: impliziter Key = `recno`/Unique-ID (kein separater
    Key-Ausdruck; für Surrogat-/Log-/Append-only-Tabellen).
- Spaltennamen sind internierte Strings; `DB-FIELD` mappt Name→Index.

### 6.2 Record-Layout
`{uint8 flags; uint8 pad[7]; cell_t cells[ncols]}`; `flags bit0` = Tombstone
(nicht `key==0`, damit leere Strings kollisionsfrei bleiben). Feste Recordgröße
ist möglich, weil `s`-Spalten nur Pool-IDs (uint32) speichern — ein String-
Update ändert nur die ID, nie die Recordgröße.
- **Primärschlüssel** (§6.1): **KSDS-artig** = Spalten-Präfix (Spalte 0, optional
  composite, Typ `i`/`s`, eindeutig); **RRDS-artig** = implizit `recno`/Unique-ID.
  Duplikat-PK wird abgelehnt.
- **`recno` = monotoner, nie wiederverwendeter physischer Slot** (Default-Engine):
  Delete = Tombstone, kein Renumbering, **kein Slot-Reuse**; Reorganisation nur
  offline mit Remap. (Andere Engines dürfen Slots wiederverwenden, §6.6.)
- **Persistenz (M1.1, §11a.7):** Records liegen in festen 4096-B-Seiten
  (`pager`); `recno` N → Offset `4096 + (N-1)·recsize`; Page 0 je `.dat` ist
  Header (Magic, recsize, nslots, live). Delete = Tombstone-Bit, **kein**
  Slot-Reuse (keine Freiliste).
- **Design-Referenz**: VSAM **RRDS/RRN** (relative record number), **BDAM**,
  **Slotted-Page-Heap mit Tombstone** (Gray & Reuter), **Append-only/LFS**.

### 6.3 String-Pool (`m_strpool`)
- On-disk `strings.str` (ein Pool **pro DB**, §11a.7): Header (magic `MLSSTR1`,
  version, count, next_id) + Einträge in ID-Reihenfolge
  `{uint32 len; bytes[len]}` (length-prefixed). (Frühere Formulierung `<t>.str`
  ist damit auf einen Pool pro Datenbank vereinheitlicht.)
- In-memory: `id2str` (mls-Array `char*`) + `str2id` (Open-Addressing-Hash,
  FNV-1a, case-sensitive).
- IDs: `uint32`, monoton, append-only, ab 1; `0` = ungültig.
- **Ordnung:** IDs sind in *Einfüge*-Reihenfolge monoton, **nicht** in Text-
  Reihenfolge. Handles taugen daher nur für **Gleichheit** (FR-05), nicht für
  Ordnung/Prefix/Range — die Text-Ordnung liefert der B⁺-Baum (§6.4), nicht der
  Pool. Eine „sortierte String-Liste“ ersetzt den Hash-Pool deshalb **nicht**.
- API: `m_strpool_create/open/close`, `lookup`, `intern`, `resolve`.

### 6.4 Index & Seiten
- Seitengröße 4096 B; I/O = ganze Seiten via `pread`/`pwrite` (zählbar).
- **Primärindex**: **KSDS-artig** = **B⁺-Baum** (leaf-linked) über den PK; Key =
  int64 **oder** Pool-ID (Ordnung über Wert bzw. aufgelösten Text); Blätter mit
  Geschwister-Verzeigern (Range-Scan); Composite-PK = Präfix-Vergleich.
  **RRDS-artig** = kein B⁺-Baum, Zugriff per Direkt-Offset `recno·recsize`.
- **Sekundärindizes** (M2): eigenständige B⁺-Bäume über `(Wert, PK)`
  (FK-Reverse-Lookup, Range-Scans).
- **B\*-Verfeinerung** (optional, M3+): Mindestfüllung 2/3 + Geschwister-
  Umverteilung vor dem Split → weniger Splits bei append-lastigen Time-Series.
- **Tombstones** über `flags`; `slot_count` ≠ `live_count`; Deletes renummerieren
  nicht; Duplikat-PK abgelehnt.
- **Prefix-Matching (Design-Notiz):** `LIKE 'pre%'` ist als **B⁺-Range-Scan
  `[pre, pre+1)`** über den text-geordneten Komparator realisierbar (Blätter
  leaf-linked, `DB-RANGE`/`DB-SEEK-IX`). Kein neuer Speicher, kein neues
  Literal. Case-Sensitivity = Wort-Parameter (CS = Roh-Bytes, CI =
  `htable`-artige Faltung). Kein eigenes FR-Wort; bei Bedarf ein dünner
  C-Primitive über `DB-RANGE`.

### 6.5 Unique Record-ID (uint64)
- Jeder Record hat eine **globale, stabile Unique-ID** (`uint64`), für den Client
  **opak** (nur per `=` vergleichbar). `recno` ist die interne physische Position
  (monoton, nie wiederverwendet, §6.2).
- **Kein `AUTO_INCREMENT` nötig**: Die Unique-ID ist bereits monoton und eindeutig
  — sie *ist* der Auto-Increment (Tabellen sind Listen, die Position ist die
  Identität). Business-Sequenzen (z. B. lückenlose Rechnungsnummern) sind
  app-seitige Zähler oder der Helper `DB-NEXT-ID`; die routing-kodierte Unique-ID
  ist bewusst *keine* lückenlose Menschen-Nummer.
- Der Server darf die **oberen 8 Bit als physisches Routing** nutzen
  (Shard-/Split-Tabellen-/Server-Selector), die unteren 56 Bit als lokalen
  `recno`. Damit ist die ID zugleich **Routing-Key des Multiplexers**:
  `RECORD-BY-NUM` entscheidet ohne zentrale Lookup-Tabelle, welche Tabelle/
  welcher Server bedient (32-bit-Variante analog mit weniger Routing-Bits).
- Dokumentierter Trade-off: Nutzt der Server die oberen Bits für den Ort, ist
  ein **Shard-Move** eine server-seitige Umwidmung (Remap); die ID bleibt für
  den Client unverändert opak. Alternative (monotone ID + server-seitige
  ID→Ort-Tabelle) bleibt möglich.

### 6.6 Storage Engines (pluggable, spätere Erweiterung)
Eine Tabelle wählt eine **Engine**. Die Engine implementiert die Storage-
Primitive: `insert`, `seek/read`, `next/prev`, `delete`, **`slot-assign`** und
stellt **Hooks als Forth-Wörter** bereit (pro Tabelle überschreibbar, §8.1a).

- **Default-Engine**: ISAM/RRDS (feste Records, monotone `recno`, Tombstones, §6.2).
- **Engine-Hooks**: z. B. `PRE-INSERT ( key -- slot )` berechnet den Ziel-Slot.
- **RRD-artige Ring-Engine** (Backlog): fester Ring, `slot = f(key)` (z. B.
  `(zeit/schritt) mod ringgröße`), **Slot-Wiederverwendung** (zirkulär),
  konstante Größe — der Time-Series-Klassiker.
- **Engine vollständig als Forth-Code** (Backlog): eine ganze Engine ist ein Satz
  von Forth-Wörtern/Hooks → nutzererweiterbar ohne C-Änderung.

Die Engine-Abstraktion generalisiert **über das Record-Datei-Modell hinaus**: eine
RRD-Ring-Engine hat keine klassischen Records/`recno`, sondern Zeit-Buckets. Genau
deshalb ist der Motor als Hook-Satz (Forth-Wörter) definiert — der Multiplexer
(„ein Input, mehrere Outputs“) passt auch auf Nicht-Record-Speicher.

### 6.7 Referenz-Datenmodell: Artikel / Bestellung / Kunde (Abnahmebasis)
Bekanntes Northwind-artiges Schema; **jede FR muss gegen dieses Modell
implementierbar sein**. Beziehungen: Kunde 1—N Bestellung, Bestellung 1—N
Position, Artikel 1—N Position (N:M über Position). FKs sind **Unique-IDs**,
aufgelöst via `RECORD-BY-NUM` (navigational; keine deklarativen Constraints).

```
DB-OPEN Z" shop.db"

DB-TABLE Z" kunde"        \ PK: kunde_id (i)
  Z" kunde_id"  Z" i"     Z" name"  Z" s"     Z" ort"  Z" s"

DB-TABLE Z" artikel"      \ PK: artikel_id (i)
  Z" artikel_id" Z" i"    Z" bez"  Z" s"      Z" preis" Z" i"    \ Cent

DB-TABLE Z" bestellung"   \ PK: bestellung_id (i); FK kunde_id; Zeitreihe
  Z" bestellung_id" Z" i" Z" kunde_id" Z" i"  Z" datum" Z" d"     Z" status" Z" s"

DB-TABLE Z" position"     \ Composite-PK: (bestellung_id, artikel_id)
  Z" bestellung_id" Z" i" Z" artikel_id" Z" i" Z" menge" Z" i"
```

> Die konkrete Schema-Erstellung nutzt die DDL-Wörter aus **§8.7**
> (`DB-CREATE-TABLE`/`DB-COL`/`DB-PRIMARY`/`DB-TABLE-CREATE`); die Skizze zeigt
> das Ziel-Schema.

| Operation | Forth | Struktur | FR |
|---|---|---|---|
| Kunde/Artikel anlegen | `DB-PUT` + `DB-INSERT` | B⁺-Unique-Check, Unique-ID | FR-07/12 |
| Kunde per ID | `RECORD-BY-NUM` / `DB-SEEK` | O(1) Offset / O(log n) B⁺ | FR-32 |
| Bestellungen eines Kunden | `DB-SEEK-IX` + `DB-NEXT` | Sekundärindex (B⁺) | FR-16 |
| Bestellungen nach Datum | `DB-SEEK-IX` + `DB-NEXT` (Range) | Sekundärindex (B⁺-Leaf) | FR-16/34 |
| Bestellung anzeigen (Positionen + Artikel) | 2 Cursor-Loops + `RECORD-BY-NUM` | Nested-Loop (navigational) | ab M1 |
| Bestellsumme | Loop + `+` | Aggregation im Skript | FR-17 |
| Umsatz pro Kunde/Monat | Group-by + Sum, Zeit-Buckets | Sort/Filter + Downsampling | FR-17/34 |
| Seite N | `RS-COUNT` + `RS-PAGE` | Server-`rs` | FR-15 |
| Anti-Join (B ohne A) | `DB-SCAN`+`DB-LOOKUP` / `SET?` / Merge | Punkt-Lookup / Set / 2 Cursor | FR-40 (§8.5) |
| Parallele Erfassung | — | Writer-Lock/DB-Datei | FR-23 |

**Datenstrukturen**: B⁺-Baum (leaf-linked) für Primär-/Sekundärindex; B\*-
Umverteilung optional; Unique-ID→Ort routing-kodiert (sonst Hash/Array);
String-Pool (Hash+Array); Tombstone-Bitmap (kein Slot-Reuse); `rs`-Array; RRD-Ring
(Backlog).

---

## 7. Protokoll (Wire)

### 7.1 Framing
`uint32 len` (little-endian) + Payload; max Frame 64 MiB; Prüfung auf
Trunkierung/Größe.

### 7.2 Grammatik
```
Request  := <forth script (UTF-8)>
Response := RESULT COLS <n> (COL Z" name" Z" type")* [COUNT <n>] (ROW <cell>*) END
          | ERROR <code> Z" message"
          | OK <int>
cell     := <int64> | S" text" | Z" text"
type     := "i" (int64) | "s" (pool string) | "z" (conststr)
```

### 7.3 Escaping
Emitter spiegelt Tokenizer-Escapes (`\"` `\\` `\n` `\t`).

### 7.4 Fehler
Fehler als `ERROR <code> Z" message"`; fail-fast: erster Fehler bricht die Query
ab und verwirft Teil-Antworten.

### 7.5 Streaming
Query wird als Skript-Stream gesendet; Antwort kann zeilenweise fließen
(`DB-STREAM`, M3).

---

## 8. Server-Vokabular & Standard-Queries

### 8.1 Wörter
| Wort | Stack-Effekt | Bedeutung |
|---|---|---|
| `DB-OPEN` | `Z"db" -- flag` | Datenbank öffnen |
| `DB-TABLE` | `Z"table" -- flag` | Tabelle selektieren |
| `DB-FIELD` | `Z"name" -- colidx\|-1` | Spaltenname → Index |
| `DB-FIND` | `Z"name" value -- flag` | Resultset: alle Records mit Feld==value (indexiert→Index, sonst Scan; Client steuert) |
| `DB-NEXT` | `-- flag` | Cursor durchs Resultset |
| `DB-SEEK` | `key -- flag` | Punkt-Seek auf Primärschlüssel |
| `DB-SEEK-IX` | `Z"name" value -- flag` | Punkt-/Range-Zugriff über Sekundärindex |
| `DB-LOOKUP` | `key Z"table" -- uid\|-1` | Punkt-Lookup, stört keinen Cursor |
| `DB-SCAN` | `Z"table" -- cur` | Scan-Cursor in PK-Reihenfolge |
| `DB-RANGE` | `Z"table" klo khi -- cur` | Cursor über Key-Bereich `[klo,khi)` |
| `DB-COUNT` | `Z"table" -- n` | Zeilenanzahl (live) |
| `DB-GET` | `Z"name" -- value` | Feld per Name lesen |
| `DB-PUT` | `Z"name" value --` | Feld per Name im Insert/Update-Puffer |
| `VISITED?` | `id -- flag` | Zyklus-Check (Pool-ID im sortierten int-Set) |
| `MARK-VISITED` | `id --` | Pool-ID ins besuchte Set |
| `SET?` | `id -- flag` | generischer Mengen-Test (in-memory) |
| `SET-ADD` | `id --` | Menge erweitern |
| `DB-INSERT` | `-- uid` | gepufferten Record schreiben, liefert Unique-ID |
| `DB-NEXT-ID` | `Z"table" -- id` | nächste Business-Sequenznummer (app-Helper) |
| `DB-UPDATE` | `-- flag` | aktuellen Record überschreiben |
| `DB-DELETE`/`DB-DELETE-ALL` | `-- flag` / `-- count` | Record / Resultset löschen |
| `DB-EMIT-TABLE` | `--` | Resultset als benannte Tabelle |
| `CUR-NEXT` | `cur -- flag` | Cursor weiter (`-1`=gültig, `0`=Ende) |
| `CUR-KEY` | `cur -- key` | Key des aktuellen Satzes |
| `CUR-GET` | `cur Z"name" -- value` | Feld des aktuellen Satzes |
| `CUR-UID` | `cur -- uid` | Unique-ID des aktuellen Satzes |
| `CUR-VALID` | `cur -- flag` | Cursor auf gültigem Satz? |
| `CUR-EMIT` | `cur --` | aktuellen Satz als Antwortzeile |
| `CUR-CLOSE` | `cur --` | Cursor schließen |
| `DB-STATS` | `-- reads writes` | I/O-Zähler der Query |
| `DB-ENGINE` | `Z"engine" -- flag` | Engine der Tabelle wählen |
| `PRE-INSERT` | `key -- slot` | Hook: Ziel-Slot berechnen (override-fähig) |
| `RECORD-BY-NUM` | `uid -- ...cells` | Record per Unique-ID (override-fähig, Routing) |
| `DB-PROC` / `;PROC` | `Z"name" --` / `--` | Stored Procedure definieren |
| `DB-VIEW` / `;VIEW` | `Z"name" --` / `--` | View definieren |
| `DB-BIND` | `Z"word" Z"proc" --` | Wort für die aktuelle Tabelle binden/überschreiben |
| `HAS-WORD` | `Z"table" Z"word" -- flag` | Capability-Query (pro Tabelle) |
| `CAPS` | `Z"table" -- list` | verfügbare Erweiterungen der Tabelle |
| `DB-CREATE-TABLE` | `Z"name" -- flag` | Schema-Definition starten (DDL, §8.7) |
| `DB-COL` | `Z"name" Z"type" --` | Spalte (`i`/`d`/`s`/`z`) anhängen |
| `DB-PRIMARY` | `Z"name" --` | PK-Spalte markieren (mehrfach = composite, Präfix) |
| `DB-TABLE-CREATE` | `-- flag` | Schema validieren + anlegen |
| `DB-COPY-TABLE` | `Z"src" Z"dst" -- flag` | Live-Records per Spaltenname in leeres `dst` kopieren |
| `DB-DROP-TABLE` | `Z"name" -- flag` | Tabelle löschen |
| `DB-RENAME-TABLE` | `Z"name" Z"new" -- flag` | Tabelle umbenennen |
| `DB-TABLES` | `-- n` | Anzahl Tabellen (Introspektion) |
| `DB-TABLE-NAME` | `i -- Z"name"` | Tabellenname per Index |
| `DB-COLS` | `Z"table" -- n` | Spaltenanzahl |

### 8.1a Per-Table-Override (Mechanik)
- Jede Tabelle besitzt ein **eigenes Wörterbuch**. Lookup-Reihenfolge:
  Tabellen-Wörterbuch → globales Wörterbuch → Fehler (`ERROR unknown-word`).
- `DB-BIND` legt eine Implementierung (Stored Procedure/View) für ein Wort an
  die aktuelle Tabelle. Standard-Wörter bleiben als globaler Fallback erhalten.
- **Beispiel**: `RECORD-BY-NUM` ist global = direkter B⁺-Zugriff; für Tabelle
  `history` überschreibt eine Procedure das Wort und routet über die Unique-ID
  auf `history_2023`/`history_2024` oder einen anderen DB-Server.
- **Beispiel View**: `HISTORY-BY-DATE` ist eine View, die die Suche über die
  Jahres-Splits legt; der Client ruft stets `HISTORY-BY-DATE` auf.

### 8.2 Drei Standard-Queries
**SELECT** `select(db, table, field, value)`:
```
DB-OPEN Z" users.db"  DB-TABLE Z" people"
Z" name" S" John" DB-FIND
DB-EMIT-TABLE
```
Antwort: `RESULT COLS 3 COL Z" id" Z" i" COL Z" name" Z" s" COL Z" age" Z" i" ROW 7 S" John" 30 END`

**INSERT** `insert(db, table, fields...)`:
```
DB-OPEN Z" users.db"  DB-TABLE Z" people"
Z" name" S" John" DB-PUT  Z" age" 30 DB-PUT
DB-INSERT                      →  OK 13
```

**DELETE** `delete(db, table, field, value)`:
```
DB-OPEN Z" users.db"  DB-TABLE Z" people"
Z" name" S" John" DB-FIND
DB-DELETE-ALL                  →  OK 2
```

### 8.3 Showcase: Ketten-Verfolgung (SQL-Killer)
Tabelle `nodes( id s, name s, next s )`; `next` = Pool-ID, `0` = Ende.
SQL braucht rekursiven CTE (Tiefenlimit, kein früher Abbruch, O(n²)-Zyklus).
Forth: ein Seek pro Hop, früher Abbruch, Zyklus-Set als Handle-Vergleich.
```
DB-OPEN Z" chain.db" DB-TABLE Z" nodes"
S" A"
BEGIN
  DUP VISITED?  IF ." cycle: " S. DROP EXIT THEN
  DUP MARK-VISITED
  DUP DB-SEEK DROP
  Z" name" DB-GET S.
  Z" next" DB-GET  NIP
  DUP 0= IF DROP ." END" EXIT THEN
AGAIN
```
IOPS: N Knoten = N Punkt-Seeks + N Datenseiten-Reads; Zyklus-Check = N
Binärsuchen im Handle-Set.

### 8.4 Paginierung/Resultset (M2)
Komplexes SELECT materialisiert **einmal** ein Server-`rs` (Temp-Liste von
recnos); `COUNT` = O(1), `PAGE` = Slice, **ein Round-Trip**.

| Wort | Effekt | Meilenstein |
|---|---|---|
| `DB-ADD-CURRENT` | `--` | aktuelles Match ins `rs` |
| `RS-COUNT` | `rs -- n` | O(1) Länge |
| `RS-PAGE` | `rs off limit --` | Slice-Cursor |
| `RS-EMIT` | `rs --` | `COUNT n` + Zeilen des Slice |
| `RS-FREE` | `rs --` | freigeben |
| `RS-CACHE-GET/PUT` | `key -- rs\|0` / `rs key --` | LRU (M3) |
| `DB-COUNT-RANGE` | `klo khi -- n` | B⁺-Baum-Zähler (M3+) |
| `DB-NTH` | `k -- recno` | Rank (M3+) |

Beispiel:
```
<komplexe Prädikat-Schleife> DB-ADD-CURRENT ...
RS-COUNT .
20 10 RS-PAGE RS-EMIT
```

### 8.5 Worked Example: Anti-Join — „B ohne A" (Key nicht in A)

**Brauchen wir Temp-Tables?** **Nein — keine persistenten Temp-Tables.** Der Fall
ist ein Anti-Join (`B.key NOT IN A`). In SQL ist eine „Temp-Tabelle" nur ein
Implementierungsdetail des Optimizers (Hash-Anti-Join). Hier wählt das
Forth-Skript die Strategie; die einzige „temp"-artige Struktur ist ein
**query-lokales Set** (in-memory, kein DB-Table).

Annahmen: `A(key, …)`, `B(key, …)`; `key` ist PK/indexiert.

**(1) Nested-Loop mit Punkt-Lookup — kein Temp, O(1) Speicher**
```
DB-OPEN Z"db"
DB-SCAN Z"B" -> curB
BEGIN CUR-NEXT curB WHILE
  CUR-GET curB Z"key" Z"A" DB-LOOKUP    \ ( uid|-1 )
  -1 = IF CUR-EMIT curB THEN
REPEAT
CUR-CLOSE curB
```
Kosten: |B| × O(log |A|) Index-Lookups. Gut, wenn B klein / A indexiert.

**(2) Query-lokales Set (materialisierte A-Keys) — das „Temp-Table"-Äquivalent**
```
DB-SCAN Z"A" -> curA
BEGIN CUR-NEXT curA WHILE
  CUR-GET curA Z"key" SET-ADD
REPEAT CUR-CLOSE curA

DB-SCAN Z"B" -> curB
BEGIN CUR-NEXT curB WHILE
  CUR-GET curB Z"key" SET?
  0= IF CUR-EMIT curB THEN
REPEAT CUR-CLOSE curB
```
Kosten: |A|+|B| I/O, O(|A|) Speicher. Gut, wenn A klein / B groß.

**(3) Merge-Anti-Join — zwei geordnete Cursor, O(1) Speicher**
Beide B⁺-Bäume leaf-linked, also in Key-Ordnung durchlaufen. Pro Schritt:
`a<b` → A vor; `a==b` → beide vor; `a>b` → B-Zeile emittieren, B vor.
Kosten: |A|+|B| **sequenzielle** I/O, O(1) Speicher. Best, wenn beide groß und
PK-sortiert.

**Entscheidungsregel** (Skript/Stored Procedure wählt; `DB-COUNT` liefert die
Kardinalität):

| Situation | Strategie |
|---|---|
| B klein, A indexiert | (1) Punkt-Lookup |
| A klein / passt in RAM, B groß | (2) Set |
| beide groß, PK-sortiert | (3) Merge |

Ein Helper `ANTI-JOIN` kann per `DB-COUNT` wählen; die drei Strategien bleiben
explizit wählbar. **Fazit: keine persistente Temp-Tabelle nötig.**

### 8.6 Aggregation (M2)

Aggregation ist ein **normaler Forth-Loop** über ein materialisiertes Resultset
(§8.4) — keine Engine-Magie, deterministische I/O (NFR-08). Die Standard-Aggregate
sind dünne C-Primitives über `rs` (materialisiertes Resultset, §8.4) und eine
Spalte; `COUNT` ist der O(1)-Alias von `RS-COUNT`.

| Wort | Stack-Effekt | Bedeutung |
|---|---|---|
| `SUM` | `rs Z"col" -- n` | Summe aller Zeilen; leere Menge = `0` |
| `COUNT` | `rs -- n` | Zeilenzahl; Alias für `RS-COUNT`; leer = `0` |
| `AVG` | `rs Z"col" -- n` | arithmetisches Mittel, ganzzahlig abgerundet; leer = Fehler |
| `MIN` | `rs Z"col" -- n` | kleinstes Element (`i`/`d` numerisch, `s`/`z` lexikografisch); leer = Fehler |
| `MAX` | `rs Z"col" -- n` | größtes Element (wie `MIN`); leer = Fehler |
| `GROUP-SUM` | `rs Z"grp" Z"col" -- rs'` | gruppiert nach `grp`, liefert `rs'` aus `(grp, sum)` |
| `GROUP-AVG` | `rs Z"grp" Z"col" -- rs'` | wie `GROUP-SUM`, Mittel pro Gruppe |
| `GROUP-COUNT` | `rs Z"grp" -- rs'` | wie `GROUP-SUM`, Zeilenzahl pro Gruppe |

**Kein NULL / keine 3VL (§3.2, NFR-03):** Aggregat-Identitäten sind explizit.
`SUM` und `COUNT` über eine leere Menge liefern `0`; `AVG`/`MIN`/`MAX` haben kein
neutrales Element und liefern `ERROR -1 "empty aggregate"`.

**Beispiel (Bestellsumme, §6.7):**
```
DB-OPEN Z" shop.db"  DB-TABLE Z" position"
... DB-ADD-CURRENT ...          \ Positionen der Bestellung materialisieren → rs
rs SUM Z"menge" .
rs COUNT .
```

`GROUP-SUM`/`GROUP-AVG`/`GROUP-COUNT` ersetzen die Group-by-Schleife aus §6.7 für
den häufigen Fall; die allgemeine Form bleibt ein expliziter Forth-Loop.

### 8.7 Schema-DDL & ALTER (per Copy)

Schema-Erstellung ist ein Forth-Programm (DDL), symmetrisch zum Insert-Muster
`DB-PUT` → `DB-INSERT`:

```
Z" kunde" DB-CREATE-TABLE      \ Schema-Puffer starten (flag)
  Z" kunde_id" Z" i" DB-COL    \ Spalte name+typ (i/d/s/z)
  Z" name"     Z" s" DB-COL
  Z" ort"      Z" s" DB-COL
Z" kunde_id" DB-PRIMARY        \ PK = Spalten-Präfix (mehrfach = composite)
DB-TABLE-CREATE                \ validieren + anlegen (+ meta, §11a.7)
```

`DB-CREATE-TABLE` setzt die aktuelle Tabelle auf „keine“; `DB-TABLE-CREATE`
wählt die neue Tabelle aus. Ohne `DB-PRIMARY` entsteht eine RRDS-Tabelle
(kein PK); PK-Spalten müssen ein **Präfix** der Spaltenliste sein. Ungültige
Typen, Doppelspalten und ein Nicht-Präfix-PK werden abgelehnt.

**ALTER TABLE = Copy-and-Swap** (kein In-Place-Umbau, keine Transaktion):

```
\ 1) Ziel mit neuem Schema anlegen (DDL wie oben)
Z" kunde" Z" kunde_v2" DB-COPY-TABLE   \ Live-Records per Spaltenname kopieren
Z" kunde" DB-DROP-TABLE                 \ alte Tabelle löschen
Z" kunde_v2" Z" kunde" DB-RENAME-TABLE  \ umbenennen
```

- Spalten werden **per Name** gemappt: gleiche Spalte → Wert; neue Zielspalte →
  `0` (kein NULL, §3.2); Quellspalte ohne Ziel → ignoriert.
- Typ-Mismatch, nicht-leeres Ziel und Duplikat-PK brechen ab (Ziel wird
  zurückgesetzt).
- Dieselbe DB/Pool ⇒ String-IDs werden direkt übernommen.
- `DB-TABLES`/`DB-TABLE-NAME`/`DB-COLS` liefern die Schema-Introspektion.
- Persistenz (`.dat`/`meta`) folgt §11a.7; der In-Memory-Kern ist umgesetzt.

---

## 9. Client-API & Resultset

### 9.1 C-API (`fdb.h`, stabile Fläche)

> Die `fdb_*`-Funktionen sind die **stabile Client-Fläche**. Implementiert werden
> sie von `m_fapi` (Monolith, in-process über `query_exec`) bzw. `m_fclient`
> (Daemon, über Socket) — der Aufrufer-Code ist identisch (§11a).
```c
int  fdb_open(const char *socket_path);                 // → conn
void fdb_close(int conn);
int  fdb_select(int conn, const char *db, const char *table,
                const char *field, const char *value);  // → resultset
int  fdb_insert(int conn, const char *db, const char *table,
                const char *field, const char *value);  // → uid
int  fdb_delete(int conn, const char *db, const char *table,
                const char *field, const char *value);  // → count
int         fdb_result_cols(int rs);
const char *fdb_result_colname(int rs, int i);
const char *fdb_result_coltype(int rs, int i);
int         fdb_result_rows(int rs);
long long   fdb_result_int(int rs, int row, int col);
const char *fdb_result_str(int rs, int row, int col);
void        fdb_result_free(int rs);
```

### 9.2 Resultset-Struktur
Kleine dedizierte Struktur (Schema als Namen+Typen, Zeilen als Zellen-Arrays
über mls-Handles), **kein** `tbl_*` (sortiertes Dict ungeeignet). Client-seitig
`fdb_result` (geparste Antwort) ≠ server-seitig `rs` (Temp-recnos).

### 9.3 Parser
`fdb_select` baut das Forth-Skript (escaped), sendet es, parst die Antwort
(`RESULT/COLS/COL/ROW/END`, Escapes) in ein Resultset. Kein Forth-VM nötig
client-seitig (Phase 1).

---

## 10. Demos

### 10.1 Demo 1 — SELECT (Pflicht-Demo)
```c
int main(void){
  m_init();
  int db = open_database("./mls-db.sock");                 // connect
  int rs = select(db, "users", "people", "name", "John");  // eine Zeile
  print_result(rs);                                        // Tabelle + Spaltennamen
  close_database(db);
  m_destruct();
}
```
`print_result` liest die Spaltennamen aus `fdb_result_colname`:
```
id      name    age
7       John    30
```
Server seeded `users/people` (Zeile „John“). `open_database/select/
print_result/close_database` sind dünne Wrapper um die `fdb_*`-API.

### 10.2 Demo 2 — Showcase Ketten-Verfolgung
`demos/forth_chain_demo.c` folgt der Kette (§8.3); Seed `A→B→C→A`; Ausgabe:
```
Alpha -> Beta -> Gamma -> (Zyklus: Alpha)
```

---

## 11. Phasenplan & Abnahmekriterien

### M0 — Fundament + Socket
| # | Schritt | Dateien | Abnahme |
|---|---|---|---|
| 0.1 | VM-Port: Kontext statt `g_vm`, silent `m_forth_new`, output→`m_forth_output`, Status-Rückgabe, kein stdout | `lib/m_forth.{c,h}` | `: SQ DUP * ; 5 SQ .` → `"25"`; `eval==0`; Underflow/Div0 → Status≠0 |
| 0.2 | Compiler: inline String-Literale + `(S")`/`(Z")` Laufzeit; `ABORT`/`CATCH`/`THROW`; Fehler-Reset; **Execution Tokens**, **`DEFER`/`IS`**, **Wordlists/Search-Order** (pro Tabelle), **`LITERAL`/`POSTPONE`**, Underflow-Erkennung (§5.5) | `m_forth.c` | `: HI S" hi" ; HI S.` → "hi"; `DEFER`/`IS`-Dispatch; Wordlist-Override; Fehler bricht ab |
| 0.3 | Leak-frei: Friedhof-Redefinition, ein `tok.scratch` | `m_forth.c` | ASan-sauber |
| 0.4 | `m_strpool` (lookup/intern/resolve, Persistenz, stabile IDs) | `lib/m_strpool.{c,h}` | ID-Stabilität über reopen |
| 0.5 | `m_ftransport` (Unix-Socket, Framing, Prüfungen) | `lib/m_ftransport.{c,h}` | Roundtrip; Kurz-Frame → Fehler |
| 0.6 | `m_fserver`/`m_fclient` Minimal (Ping-Pong) | `lib/m_fserver.*`, `lib/m_fclient.*` | `S" ok"` rauf, Antwort runter |

### M1 — ISAM-Kern + Demo end-to-end (Akzeptanz = Demo 1)
| # | Schritt | Dateien | Abnahme |
|---|---|---|---|
| 1.1 | Seiten-I/O + Datenseiten (feste Records, Tombstone, **kein Slot-Reuse**) | `m_isam.c` | create/write/read roundtrip; Details §11a.7 |
| 1.2 | B⁺-Baum-Primärindex (insert/delete/Range-Scan) | `m_isam.c` | seek/next/prev lexikalisch korrekt |
| 1.3 | Open-Validierung (Magic, ncols/recsize, Pool-ID, Seiten) | `m_isam.c` | korrupte Datei → sauberer Fehler |
| 1.4 | Benannte Spalten + mehrere Tabellen + `DB-*` | `m_isam.c` | `DB-FIELD` Name→Index; multi-table |
| 1.5 | `fdb_select/insert/delete` + Resultset-Parser | `m_fclient.c` | Antwort → Resultset, Namenszugriff |
| 1.6 | Demo 1 (Seed, select→print_result) + `DB-STATS` + Selbsttest | `demos/forth_db_demo.c` | Demo druckt Tabelle; Selbsttest grün |
| 1.7 | Showcase Ketten-Verfolgung (`DB-SEEK`, `VISITED?`/`MARK-VISITED`) | `demos/forth_chain_demo.c` | Zyklus erkannt, End-Kette terminiert |

> **Joins** sind ab M1 möglich: Forth-Skripte verketten Cursor-Loops
> (`DB-SCAN`/`CUR-NEXT` + `DB-LOOKUP`/`RECORD-BY-NUM`); `DB-JOIN` (FR-20) ist
> nur Convenience. Unabhängige Cursor (`CUR-*`) sind Teil von M1.

### M2 — API-Schicht (Stored Procedures, Views, Override, Capability) + Sekundärindizes
- **Stored Procedures / Views / Per-Table-Override**: `DB-PROC`/`;PROC`,
  `DB-VIEW`/`;VIEW`, `DB-BIND`, tabellen-skopiertes Wörterbuch.
- **Capability-Query pro Tabelle**: `HAS-WORD`/`CAPS` + Fallback/Fehler.
- **Forward-Kompatibilität**: alte Clients laufen bei neuen Server-Wörtern weiter.
- `DB-INDEX`/`DB-SEEK-IX`, Range-Scans.
- Standard-Aggregate `SUM/COUNT/AVG/MIN/MAX` + Group-By (`GROUP-*`, §8.6), `SORT`, `FILTER`.
- UPDATE, Feldlisten-Projektion.
- Resultset + Paginierung (`DB-ADD-CURRENT`/`RS-*`, §8.4).
- Mehrere Clients, Writer-Lock pro DB-Datei.
- **Baseline §6.7** (Artikel/Bestellung/Kunde) muss mit M1+M2 vollständig abfragbar sein.

### M3 — Multiplexer (Splitting, Offloading, Record-by-ID) + Engine-Abstraktion
- **Tabellen-Splitting on demand** (z. B. nach Jahr) über Procedures/Views.
- **Offloading** auf andere Tabellen / andere DB-Server, transparent für den Client.
- **Record-by-ID-Routing** (`RECORD-BY-NUM`, Unique-ID mit oberen Routing-Bits).
- **Storage-Engine-Abstraktion**: `DB-ENGINE`, `PRE-INSERT`, Hooks (FR-35/FR-37).
- Join-Convenience (`DB-JOIN`); `DB-STREAM`; Reader/Writer-Locks;
  LRU-Resultset-Cache.

### M3+ — ISAM-native Count/Rank
- B⁺-Baum-Subtree-Zähler, `DB-COUNT-RANGE`, `DB-NTH`, Keyset-Pagination.

### M4 — Abrundung
- Optionaler SELECT→Forth-Übersetzer.
- Benchmarks der IOPS-Steuerung über zwei Prozesse.
- `docs/guide/forth-sql.md`.

### Backlog (später)
- Crash-Safety (Stromausfall), Write-Logs (WAL), Transaktionen.
- Pluggable Storage-Engines: RRD-Ring (FR-36), Engine als Forth-Code (FR-38).

### 11a. Implementierungsplan (inkrementell, Monolith-first)

Bewusste Umordnung von M0/M1: erst ein **Monolith** (DB-Kern + App in *einem*
Prozess) hinter einer sauberen API-Grenze, dann die Daemon-Ausgliederung als
reiner Backend-Tausch.

#### 11a.1 Prinzipien
1. **Monolith zuerst**: ein Programm mit DB-Kern *und* App, aber nur über die
   API-Grenze (§11a.2). Der Kern wird später 1:1 als Daemon ausgegliedert.
2. **Kleine Module**: je `*.c/h` + ein Selbsttest; klar definierte Aufgabe und
   Abhängigkeiten.
3. **Wiederverwendbarkeit**: alles Generische ist ein eigenes Modul ohne
   DB-/Forth-Wissen.
4. **Einfrieren statt Re-Testen**: grüner Selbsttest ⇒ Modul **eingefroren**
   (API-stabil); Integrationstests prüfen nur Grenzen, nie Interna erneut.

#### 11a.2 Die API-Grenze
Die App spricht **immer** in Forth-Skripten; Antwort ist ein Forth-Skript:
```
answer = query_exec(engine, script)   // Skript rein, Antwort-Skript raus
```
- **Monolith**: `query_exec` = `m_engine` (in-process: m_forth + DB-Wörter).
- **Daemon**: `query_exec` = `m_fclient` (Socket).

`fdb_open/select/insert/delete/print_result` rufen nur `query_exec` → der
App-Code bleibt beim Übergang Monolith→Daemon **unverändert**.

#### 11a.3 Modul-Inventar (alle direkt in `lib/`)
| Modul | Zweck | Deps | Wiederverwendbar | Neu/Port |
|---|---|---|---|---|
| `arena` | nicht-verschiebbarer Chunk-Allocator | — | ✅ | Port |
| `carray` | `cell_t`-Stack | — | ✅ | Port |
| `darray` | `void*`-Stack | — | ✅ | Port |
| `strbuf` | String-Builder | — | ✅ | Port |
| `htable` | case-insensitive Hash (Dictionary) | — | ✅ | Port |
| `hashmap` | case-sensitive String→u32 | — | ✅ | Neu |
| `token` | Tokenizer **mit Escapes** | strbuf | ✅ | Port+ |
| `bptree` | generischer B⁺-Baum (int64/u32-Key) | pager (opt.) | ✅ | Neu |
| `pager` | Seiten-I/O + Cache, Prüfung | — | ✅ | Neu |
| `m_strpool` | String-Pool, persistiert | hashmap | ✅ | Neu |
| `m_forth` | Forth-VM (Compiler, Fehler, Strings) | arena/carray/darray/strbuf/htable/token | ✅ | Port |
| `m_isam` | Tabellen/Schema/Records/Cursor/PK | pager/bptree/m_strpool | ❌ | Neu |
| `m_dbwords` | registriert `DB-*`-Wörter | m_forth/m_isam | ❌ | Neu |
| `m_fproto` | `RESULT/COLS/ROW/END` Emitter+Parser | strbuf/token | ✅ | Neu |
| `m_engine` | `query_exec` (in-process) — **die Grenze** | m_forth/m_dbwords/m_fproto | ❌ | Neu |
| `m_fapi` | `fdb_*`-Client-API | m_engine/m_fproto | ❌ | Neu |
| `m_ftransport` | Framing (`uint32 len`+Payload) | — | ✅ | Neu (Phase 4) |
| `m_fserver` | Daemon-Loop | m_engine/m_ftransport | ❌ | Neu (Phase 4) |
| `m_fclient` | Socket-`query_exec` | m_ftransport/m_fproto | ❌ | Neu (Phase 4) |
| `demo` | Monolith-`main()` | m_fapi/m_isam | ❌ | Neu |

Keine Zyklen; Abhängigkeiten strikt von generisch (unten) nach spezifisch (oben).
Die ✅-Module sind projektunabhängig wiederverwendbar.

#### 11a.4 Phasen (je Schritt: Modul + Selbsttest + einfrieren)

**Phase 0 — Generische Primitive (Port + Selbsttest)**
| # | Modul | Selbsttest (Done) |
|---|---|---|
| 0.1 | `arena` | Alloc über Chunk-Grenzen, Adressen bleiben stabil |
| 0.2 | `carray`, `darray` | push/pop/peek, Bounds |
| 0.3 | `strbuf` | append/clear, Wachstum |
| 0.4 | `htable` | set/get, Redefinition |
| 0.5 | `hashmap` | insert/lookup/miss, Kollisionen |
| 0.6 | `token` | Escapes-Roundtrip (`\" \\ \n \t`) |
| 0.7 | `bptree` | insert/delete/seek/Range, Split/Merge |
| 0.8 | `pager` | Seiten-Roundtrip, gekürzte Datei → Fehler |

**Phase 1 — Pool + VM**
| # | Modul | Selbsttest (Done) |
|---|---|---|
| 0.9 | `m_strpool` | intern/resolve, ID-Stabilität über reopen, leere Strings |
| 0.10 | `m_forth` | `: SQ DUP * ; 5 SQ .`→`"25"`; `S"`/`Z"`; `DEFER`/`IS`-Dispatch; Wordlist-Override; ASan-sauber (§5.5) |

**Phase 2 — DB-Kern (in-process)**
| # | Modul | Selbsttest (Done) |
|---|---|---|
| 0.11 | `m_isam` | create/insert/seek/fetch/delete; composite-PK; Tombstone |
| 0.12 | `m_dbwords` | Skript INSERT→SEEK→FETCH→`S" John" =` → true |

**Phase 3 — Monolith (DB + App, über die Grenze)**
| # | Modul | Selbsttest (Done) |
|---|---|---|
| 0.13 | `m_fproto` | Emitter/Parser-Roundtrip inkl. Escapes + `COUNT` |
| 0.14 | `m_engine` | Skript → Antwort → Re-Eval reproduziert Handles |
| 0.15 | `m_fapi` | `fdb_select` → Resultset mit Feldnamen |
| 0.16 | `demo` | `m_init/open_database/select/print_result/close_database` |

**Abnahme Monolith**: Demo 1 (§10.1) läuft in **einem** Prozess; der App-Code
geht bereits über `query_exec`/`fdb_*`.

**Phase 4 — Daemon-Ausgliederung (später)**
| # | Modul | Selbsttest (Done) |
|---|---|---|
| 0.17 | `m_ftransport` | Framing-Roundtrip, Kurz-Frame → Fehler |
| 0.18 | `m_fserver` | Daemon-Loop, ein Query pro Verbindung |
| 0.19 | `m_fclient` | Socket-`query_exec` (Drop-in) |
| 0.20 | `demo` gesplittet | **identischer App-Code**, jetzt über Socket |

**Abnahme Daemon**: Demo 1 läuft in **zwei** Prozessen, ohne eine Zeile
App-Code zu ändern (nur das `query_exec`-Backend wird getauscht).

#### 11a.5 Test-Disziplin („funktionierende Module nicht erneut testen“)
- Je Modul ein assert-basierter Selbsttest (kein Framework).
- **Grün ⇒ eingefroren**: Modul wird nicht mehr angefasst; Änderungen laufen als
  neue Version.
- Integrationstests prüfen ausschließlich die **Grenzen** (`query_exec`, `fdb_*`,
  Wire-Roundtrip), nicht die Modul-Interna.
- Bei einem Fehler: zuerst der Selbsttest des betroffenen Moduls, dann die Grenze
  — nie das ganze System.

#### 11a.6 Bezug zu §11
Bewusste Umordnung von M0/M1: erst In-Process-Monolith (≈ M0.1–0.4 + M1.1–1.7),
dann Socket (≈ M0.5–0.6 + M1.5). Gleicher Endzustand, risikoärmerer Pfad; die
Prozess-Trennung ist ein reiner Backend-Tausch.

#### 11a.7 M1.1 Persistenz (`m_isam` v2) + Laufzeitmessung

Ziel: `m_isam` wird dauerhaft. Records liegen in festen Seiten (`pager`), Strings
im persistenten `m_strpool`; der B⁺-Index bleibt in-memory und wird beim Open
neu aufgebaut. Zusätzlich messen Tests die Laufzeit von Forth-Skripten (gesamt +
pro Wort) über die C-API. Abnahme = `create/write/read roundtrip` (M1.1) plus
Open-Validierung (M1.3-Vorarbeit). Transaktionen/WAL bleiben Backlog (§4.4).

**Layout (DB = Verzeichnis, §6.1):**
```
<db>/
  meta          # Schema-Registry (Magic MLSDB1, version, ntables, Schemas)
  strings.str   # ein m_strpool pro DB (Magic MLSSTR1)
  <table>.dat   # Datenseiten: Page 0 = Header, danach feste Records
```

**Festlegungen**
- **Record/Persistenz (§6.2):** `{uint8 flags; uint8 pad[7]; cell_t cells[ncols]}`,
  `recsize = 8 + 8*ncols`; `recno` N → Offset `4096 + (N-1)*recsize`. `.dat`-
  Page-0-Header: Magic `MLSISAM1`, version, recsize, nslots, live.
- **Keine Freiliste / kein Slot-Reuse** (§6.2): Delete = Tombstone-Bit; die
  frühere M1.1-Formulierung „Freiliste“ ist damit aufgehoben.
- **Index:** in-memory `bptree`, beim Open durch Scan der Live-Records neu
  aufgebaut. `ponytail:` O(n log n)-Rebuild; Upgrade = persistente B⁺-Seiten
  (M1.2/M3).
- **Ein Codepfad:** `m_isam_db_new()` = anonyme DB über `mkdtemp` (Temp-Dir,
  bei `free` gelöscht); `m_isam_db_create(dir)` / `m_isam_db_open(dir)` /
  `m_isam_db_close(db)`; `m_isam_db_free` ist NULL-sicherer Wrapper (kein
  Doppel-Call). Open rekonstruiert Tabellen über einen internen
  alloc-from-meta-Pfad (nicht `create_table`, sonst Rewrite).
- **Schreib-Reihenfolge:** Duplikat-Check → Record-Seite(n) schreiben → Header
  (`nslots`/`live`) → B⁺-Insert; bei Fehler Rollback/Tombstone, Rückgabe 0.
  Delete hält Tombstone + Index konsistent; `live` wird beim Open per Scan
  **neu berechnet**, nicht dem Header blind geglaubt.
- **M1.3-Validierung:** beim Open/Rebuild jede `s`/`z`-Zelle gegen den Pool
  prüfen (ID 0/out-of-range = korrupt → Fehler); ungültige String-Zellen schon
  beim Insert ablehnen. Geprüft werden ferner Header-vs-`meta`
  (`ncols`/`recsize`/`sizeof(cell_t)`), Seitenkapazität, `live`-vs-Tombstones,
  fehlende Dateien und Page-Grenzen-Trunkierung. `m_isam_resolve` bleibt
  non-NULL (`""`).
- **Tabellennamen** sind Pfadkomponenten: leer, `.`, `..`, `/`, Überlänge vor
  dem Mapping auf `<table>.dat` ablehnen.
- **`m_strpool`-Decke:** FILE/`fflush`-basiert; `intern==0` und Pool-Open-Fehler
  = fatal; partial-append als `ponytail:` (Crash-Safety = Backlog §4.4).
- **DB-STATS:** Per-Query-Delta aus logischem Page-Write-Zähler +
  `pager_reads`; Pool-I/O ist explizit exkludiert.
- **Laufzeit (FR-41, nur C-API/Test-Harness):** `m_engine_elapsed_ns` (gesamt,
  `clock_gettime(CLOCK_MONOTONIC)` um `m_forth_eval`) + flag-gated
  Per-Wort-Profiling (`m_forth_profile`/Report) an den **beiden**
  Dispatch-Stellen (`m_forth_execute`-Einstieg + innere Threaded-Loop); off =
  Nullkosten; keine neuen Forth-Wörter, kein Protokoll-Change.
- **Schema-DDL/ALTER (FR-42/FR-43, §8.7):** `DB-CREATE-TABLE`/`DB-COL`/
  `DB-PRIMARY`/`DB-TABLE-CREATE`; `ALTER` als Copy-and-Swap
  (`DB-COPY-TABLE`/`DB-DROP-TABLE`/`DB-RENAME-TABLE`) + Introspektion
  (`DB-TABLES`/`DB-TABLE-NAME`/`DB-COLS`). In-Memory-Kern umgesetzt;
  Persistenz schreibt `meta` (Schritt 1) + `.dat`.

**Schritte (je Modul + assert-Selbsttest; grün ⇒ eingefroren)**
| # | Schritt | Done |
|---|---|---|
| 1 | Dateiformate + Schema-Registry (`meta`) + DB-Lifecycle; `.dat`-Header (**umgesetzt**) | create→2 Tabellen→close→open→Schema+leere `.dat` intakt; korrupt/gekürzt/inkonsistent → sauberer Fehler; open mutiert nichts |
| 2 | Persistenter String-Pool (`strings.str`) (**umgesetzt**) | intern→close→open→gleiche ID; leerer String; `0`/out-of-range→`""`; korrupter Pool → Fehler |
| 3 | Seitenbasierte Record-CRUD (Cross-Page-RMW, Tombstone) | insert/fetch/delete/count für RRDS, Composite-PK, String-PK; Tombstone überlebt Flush+Reopen per recno |
| 4 | Reopen + M1.3-Validierung + B⁺-Rebuild | alle PK-Modi nach Reopen korrekt; ungültige Pool-ID/Duplikat-PK/Schema-Mismatch/Trunkierung → Fehler; Reopen verändert nichts |
| 5 | Stats, Build-Wiring, Demo, Docs | `make run` grün; `DB-STATS` real (Per-Query-Delta); Demo übersteht Neustart; Docs aktuell |
| 6 | Laufzeitmessung gesamt + pro Wort (C-API) | Self-Test: Wort-count>0/total_ns>0, off-Modus leer; Harness druckt elapsed + Hotlist + `DB-STATS` |
| 7 | Schema-DDL-Wörter (In-Memory-Kern, umgesetzt) | DDL-Skript legt Tabelle an; danach SELECT/INSERT; ungültiger Typ/Doppelspalte/Nicht-Präfix-PK → Fehler |
| 8 | ALTER-per-Copy + Introspektion (In-Memory-Kern, umgesetzt) | add/drop/rename-Spalte + Typänderung per Copy; Datenerhalt; Duplikat-PK → Fehler; `DB-TABLES`/`DB-COLS`/Drop/Rename |

**Build-Wiring:** `pager.o` + `m_strpool.o` zu allen `m_isam`-linkenden Targets
(`test_m_isam`, `test_m_dbwords`, `test_m_engine`, `test_m_fapi`,
`test_m_fserver`, `test_m_fclient`); `../lib/pager.c` + `../lib/m_strpool.c`
in `FORTH_SRCS` der Demos. CMake baut diese Module nicht.

**Demo/DB-OPEN:** M1.1-Persistenz ist **Library-Ebene** (create/open/close am
`m_isam`-API). Das Forth-Wort `DB-OPEN` und der `db`-Parameter von `fdb_select`
bleiben vorerst No-op bzw. ungenutzt; die Demo nutzt create-if-absent/open/
seed-once, um Persistenz über einen Neustart zu zeigen.

**Grenzen/Invarianten (§14.5):** geteilter Zustand bleibt in `m_isam_*`;
`query_exec`/`fdb_*` unverändert; keine neuen Globals; `m_isam` wird als
**v2 (persistent)** deklariert, `m_strpool` bleibt sonst unverändert.

---

## 12. Teststrategie
- **Selbsttests** pro Modul (assert-basiert, kein Framework).
- **Integration**: Client-Prozess → Socket → Daemon → Antwort → Re-Eval
  (Handle-Gleichheit end-to-end).
- **Fehlerpfade**: korrupte/gekürzte DB, ungültige Pool-IDs, fehlender Key,
  keine DB offen, Division durch 0, Frame-Trunkierung, Escaping-Roundtrip.
- **Speicher**: ASan/valgrind; intentional-globale Stores (conststr/Pool) dokumentiert.
- **IOPS**: `DB-STATS`-Zähler gegen erwartete Seitenzugriffe prüfen.
- **Laufzeit** (FR-41): Forth-Skript-Laufzeit gesamt (`m_engine_elapsed_ns`) +
  pro Wort (`m_forth_profile`/Report, flag-gated) im Test-Harness; I/O separat
  über `DB-STATS`. Nur C-API, kein Wire-Change.
- **Schema**: DDL-Validierung (Typ, Doppelspalte, PK-Präfix) und ALTER-per-Copy
  (Namens-Mapping, Default `0`, Duplikat-PK) prüfen.

---

## 13. Risiken & offene Entscheidungen

| ID | Risiko | Gegenmaßnahme |
|---|---|---|
| R-1 | Verlust logischer/physischer Datenunabhängigkeit (bewusster Trade-off) | Explizit dokumentiert; Schema-Migration als eigenes Werkzeug (später) |
| R-2 | `s_cstr`-Zero-Copy-Lebensdauer | Nicht-verschiebbarer, prozess-globaler Chunk-Store |
| R-3 | Single-Writer begrenzt Concurrency | M3 Reader/Writer-Locks |
| R-4 | B⁺-Baum-Komplexität (Splits/Merges) | Schrittweise, Selbsttests pro Operation |
| R-5 | Wire-Protokoll-Escaping-Fehler | Emitter/Tokenizer gemeinsame Escape-Menge + Roundtrip-Test |
| R-6 | Forth-Skripte als Angriffsfläche (`@ !`) | Nicht für untrusted Skripte; später Sandbox/Gate |
| R-7 | Shard-Move bei routing-kodierter Unique-ID | ID bleibt opak; server-seitiges Remap; Alternative = monotone ID + ID→Ort-Tabelle (§6.5) |
| R-8 | Per-Table-Override-Fehler (falsche/fehlende Bindung) | `HAS-WORD`/`CAPS`-Negotiation + Fallback auf globales Wort |
| R-9 | Engine-Abstraktion verwässert O(1)/IOPS-Kontrakt | Engine-Interface = dieselben zählbaren Primitive + `DB-STATS` |
| R-10 | Toter Platz durch „recno nie wiederverwendet“ | Monitoring + geplante Offline-Reorg mit Remap (§6.2) |
| R-11 | Single-threaded Server + Backlog 8 → Verbindungsabbrüche/Reconnects | Worker-Pool + `SO_REUSEPORT` + Backlog `SOMAXCONN` (§14.4) |
| R-12 | Geteilte `m_isam_db` ohne Locks (parallele Reads/Writes unsicher) | `pthread_rwlock` pro DB um die `m_isam_*`-Grenze (§14.4) |
| R-13 | Insert-Puffer `m_isam_table.buf` ist per-Connection-Zustand in der geteilten Tabelle | Vor Threading in `m_dbwords`/`m_engine` verschieben (§14.4) |

**Entschiedene Punkte**: SELECT/INSERT/DELETE (UPDATE in M2); M1 volles Schema +
Namenszugriff, Projektion M2; Socket-Pfad als Parameter; Resultset als dedizierte
Struktur; Server-`rs` materialisieren, COUNT+Seite in einem Round-Trip; Cache ab
M3; B⁺-Baum-Count/Rank ab M3+; Showcase = Ketten-Verfolgung; **Unique-ID = uint64,
obere 8 Bit optional als Routing, untere 56 lokal (§6.5)**; **Capability-Query pro
Tabelle**; **Crash-Safety/WAL/Transaktionen = Backlog**; **Composite-PK erlaubt
(Spalten-Präfix)**; **Joins = verschachtelte Cursor-Loops ab M1, `DB-JOIN` nur
Convenience**; **Baseline §6.7 (Artikel/Bestellung/Kunde) ist Abnahmebasis**;
**Storage-Engines sind pluggable (RRD/Forth-Engine = Backlog)**; **Anti-Join ohne
persistente Temp-Tables** (Punkt-Lookup / Set / Merge, §8.5); **unabhängige
Cursor + Scan/Range** (FR-40); **PK-Modi KSDS (benannter Key) / RRDS (impliziter
recno)**; **kein `AUTO_INCREMENT` — die Unique-ID ist der Auto-Increment,
Business-Sequenzen app-seitig (`DB-NEXT-ID`)**; **kein neues String-Literal;
Pool bleibt Hash-basiert; Prefix = B⁺-Range-Scan (Design-Notiz §6.4)**.

**Noch offen**: exakte Performance-Ziele (Latenz/Durchsatz) für Abnahme;
Zeitpunkt und Format des Offloadings über mehrere Server (M3-Detail).

---

## 14. Skalierung: eine Million kurzlebige Clients (Erkenntnisse Phase 0–4)

### 14.1 Lastprofil (entschieden)
„Eine Million Nutzer“ heißt hier: **viele kurzlebige Verbindungen**
(connect → genau eine Query → close), schneller Verbindungs-Reset,
**read-heavy**, zufälliges Abfragemuster, **eine Maschine zuerst**. Es ist *kein*
C10M-Szenario mit dauerhaft offenen Verbindungen; ein epoll-Event-Loop ist für
dieses Profil **nicht** erforderlich (und wird bewusst zurückgestellt).

### 14.2 Ist-Stand
Monolith (`m_engine`) und Daemon (`m_fserver`/`m_fclient`) sind implementiert;
der App-Code ist über beide Backends identisch (`fdb_*` → `query_exec`). Jede
Verbindung erhält eigene `m_forth`/`m_engine`/`m_dbwords`. Geteilt ist
ausschließlich `m_isam_db` (Tabellen + String-Pool + B⁺-Index).

### 14.3 Engpässe (nach Wirkung, Stand jetzt)

| # | Engpass | Ort | Wirkung |
|---|---|---|---|
| B-1 | Backlog = `8`, ein Accept-Loop | `m_ftransport`, `m_fserver` | Bei Churn ist die Queue sofort voll → Kernel verwirft Connections → Clients reconnecten („kein freier Slot“) |
| B-2 | Server strikt single-threaded | `m_fserver_serve_one` | Nutzt genau **einen** Kern; keine Parallelität |
| B-3 | Geteilte DB ohne Locks; Insert-Puffer in der Tabelle | `m_isam_db`, `m_isam_table.buf` | Parallele Zugriffe unsicher; `buf` ist per-Connection-Zustand an falscher Stelle → Korrektheitsfehler, nicht nur Race |
| B-4 | VM/Engine pro Query neu | `m_engine_new` | Alloc/Init dominiert bei hohem Verbindungs-Churn |
| B-5 | `DB-FIND` linear | `m_dbwords` | O(n) pro Lookup; Sekundärindex fehlt (FR-16/M2) |
| B-6 | ISAM rein in-memory | `m_isam` | Persistenz/Paging = M1.1 |
| B-7 | String-Vergleich im PK-Comparator; B⁺-Grad 4 | `m_isam`, `bptree` | Text-Auflösung pro Vergleich; viele Baum-Ebenen |
| B-8 | Nur aktuelle Zeile; keine Pagination/Streaming | `m_engine`, `m_fproto` | FR-15/FR-22 offen; große Ergebnisse unhandlich |

### 14.4 Multi-Core-Konzept (später, additiv — **nicht jetzt umsetzen**)
1. **Accept entkoppeln:** Backlog auf `SOMAXCONN`; `SO_REUSEPORT` mit einem
   Listener pro Worker (Linux/AF_UNIX unterstützt das) *oder* N Threads auf einem
   Listener. Worker-Pool = `nproc`; optional begrenzte Queue als Back-Pressure
   statt Reconnect-Sturm.
2. **Worker-Schleife:** `accept` → Engine (ggf. aus VM-Pool) → ein Skript →
   Antwort senden → close. Das entspricht dem heutigen `m_fserver_serve_one`.
3. **Shared State schützen:** `pthread_rwlock` pro DB (erst grob). Reads
   (`bpt_get`/`fetch`/`resolve`) parallel, Writes (`insert`/`delete`/`intern`)
   exklusiv. Die Lock-Grenze ist die `m_isam_*`-Fläche.
4. **Vor** Threading zwingend: Insert-Puffer aus `m_isam_table` in den
   per-Connection-Zustand (`m_dbwords`/`m_engine`) verschieben.
5. **VM-Pool (optional, messen):** unveränderliches Wörterbuch teilen, pro Query
   nur `dstack`/`rstack`/`output`/String-Tabelle neu.
6. **Client:** `m_fapi`-Statics (`conns[]`, `results[]`, `default_engine`) mutexen,
   falls die App selbst multithreaded ist.
7. **Decke/Upgrade:** ein rwlock pro DB → Locks pro Tabelle → lock-freies Lesen
   (Seqlock/MVCC), falls Profiling Contention zeigt. epoll erst bei
   Dauerverbindungen.

### 14.5 Architektur-Invarianten (beim späteren Umbau nicht verletzen)
- Geteilter mutabler Zustand ausschließlich in `m_isam_db`; alles
  Per-Connection bleibt in `m_dbwords`/`m_engine`.
- DB-Mutationen nur über die `m_isam_*`-Einstiegspunkte (dort sitzt später das
  Lock, nicht in `bptree`/`darray`-Interna).
- Keine Globals in der generischen Schicht (`arena`, `carray`, `darray`,
  `strbuf`, `htable`, `hashmap`, `token`, `bptree`, `pager`); die VM bleibt
  re-entrant.
- Über die Leitung gehen **Inhalte, keine Handles** (§5.3).
- `query_exec`/`fdb_*` bleibt die Backend-Grenze; Monolith, Socket und späterer
  Multi-Core-Server sind reine Backend-Varianten.
- **Konsequenz:** Multi-Core ist ein Backend-Tausch + ein Lock + die lokale
  Puffer-Verschiebung — keine Umstrukturierung.

### 14.6 Abnahme (später)
- N Worker (= Kerne), kurzlebige Clients: kein Reconnect-Sturm, Backlog wird
  ausgenutzt.
- Lesen skaliert über Kerne (Throughput/`DB-STATS`), Writer bleiben korrekt
  serialisiert; TSan ohne Datenrennen, paralleles Insert ohne Korruption.

---

## 15. Glossar
- **ISAM**: Indexed Sequential Access Method — feste Records + Index, direkter
  Zugriff, navigational.
- **VSAM/KSDS**: IBM-Nachfolger; KSDS = B⁺-Baum mit Data-/Index-Component.
- **Handle**: prozess-lokaler Integer-Verweis auf einen String (Gleichheit =
  Integer-Vergleich).
- **Resultset (`rs`)**: server-seitige Temp-Liste von recnos (materialisiert).
- **Tombstone**: gelöscht-markierter Record, `recno` bleibt stabil.
- **Round-Trip**: ein Request/Antwort-Zyklus über den Socket.
- **RRN / RRDS**: Relative Record Number / Relative Record Data Set (VSAM);
  Zugriff über die Ordinalposition des Records (Direkt-Offset, kein Index).
- **Slotted-Page-Heap**: Record-Identität = (Seite, Slot); Delete = Tombstone,
  andere Slots bleiben unverändert.
- **Unique-ID**: globale, client-opake `uint64`-Identität eines Records;
  `recno` ist die interne physische Position.
- **Composite-PK**: Primärschlüssel aus mehreren führenden Spalten (Präfix).
- **Storage-Engine**: austauschbarer Speicher-Motor einer Tabelle (Default: ISAM);
  implementiert Storage-Primitive + Hooks.
- **Pre-Insert-Hook**: Forth-Wort, das vor dem Insert den Ziel-Slot berechnet
  (z. B. `(zeit/schritt) mod ringgröße`).
- **RRD**: Round-Robin Database — fester Ring aus Zeit-Buckets, zirkuläre
  Überschreibung, konstante Größe.
- **Navigational**: Beziehungen als explizite Zeiger/IDs + Auflösung
  (`RECORD-BY-NUM`), keine deklarativen Joins/Constraints (IMS/CODASYL-Stil).
