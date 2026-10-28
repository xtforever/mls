# Sprecherskript — mls: csv2insert

**Zielpublikum:** erfahrene C-Programmierer. Kein C-Grundlagen-Tutorial.
Dynamische Arrays, `realloc` und Längen-Buchhaltung sind bekannt — es geht
darum, wie `mls` diese Reibung entfernt.

**Video:** `mls-csv2insert.mp4` (1920×1080, 30 fps, ~4:00).
**Referenzcode:** `csv2insert.c` im selben Verzeichnis (mls_full-Amalgam,
kein Threading).

**Bauen (zum Mitverfolgen):**
```bash
cp ../../mls_full.h .
gcc -std=c11 -DMLS_THREAD_SAFE=0 csv2insert.c -o csv2insert -lm -ldl
```

Legende: `▶` = Bildschirm-Cue (Highlight/Übergang). Zeiten sind mm:ss.

---

## Szene 0 — Ziel (0:00–0:18)

**Zu sehen:** Zwei Textzeilen: „CSV von stdin → SQL-INSERT“, „ein Header, kein Make, kein Threading“.

> Wir bauen `csv2insert`: CSV von der Standardeingabe rein, fertige
> SQL-INSERT-Zeilen raus. Alles gegen den `mls`-Amalgam — eine Header-Datei,
> kein Build-System, bewusst ohne Threading. Es geht nicht um C-Grundlagen,
> sondern darum, wie `mls` die Handarbeit bei dynamischen Arrays und Strings
> übernimmt.

▶ 0:00 Szenenwechsel.

---

## Szene 1 — Handle-Modell (0:18–0:50)

**Zu sehen:** Code `m_init` / `m_alloc` / `m_put` / `INT` / `m_free` / `m_destruct`.

> Ihr kennt das Problem: dynamische Arrays in C heißen `realloc`,
> Längen-Buchhaltung und Sonderfälle. `mls` ersetzt den rohen Puffer durch
> einen `int`-Handle. Der Puffer lebt in einer internen Tabelle; `m_put` hängt
> an und wächst bei Bedarf, `INT` liest bounds-checked. Ein freigegebener
> Handle ist ein lauter Fehler, kein Dangling Pointer. Wichtig: `m_init`
> einmal am Anfang, `m_destruct` einmal am Ende — dazwischen lebt der
> Handle-Zustand. Und: Zeiger aus `mls()` nie über ein Resize hinweg halten,
> sondern den Handle neu auflösen.

▶ 0:25 Highlight auf `INT`-Zeile. ▶ 0:28 Highlight aus.

---

## Szene 2 — Einbau & Bauen (0:50–1:14)

**Zu sehen:** `#define MLS_FULL_IMPLEMENTATION`, `#include "mls_full.h"`, gcc-Zeile.

> Einbau ist eine Zeile: In genau einer C-Datei definiert man
> `MLS_FULL_IMPLEMENTATION` und inkludiert `mls_full.h` als ersten Header.
> Alle anderen Dateien inkludieren ihn nur. Gebaut wird mit einem einzigen
> gcc-Aufruf. `MLS_THREAD_SAFE=0` schaltet die Locks ab — deshalb kein
> `-lpthread`, nur `-lm` und `-ldl`.

▶ 0:50 Szenenwechsel.

---

## Szene 3 — Strings + tmls (1:14–1:44)

**Zu sehen:** String-Handle (`s_cstrdup`, `s_printf`, `m_str`, `m_free`), dann `tmls buf`.

> Strings sind ebenfalls Handles. `s_cstrdup` kopiert, `s_printf` hängt
> formatiert an, `m_str` liefert den `char*` für `printf`. Und dann `tmls`:
> derselbe Handle-Typ, aber mit `cleanup`-Attribut — er wird beim Verlassen
> des Blocks automatisch freigegeben. Das ist RAII in portablem C. Für
> temporäre Puffer wie unseren SQL-String ist das genau richtig; dauerhafte
> Handles bleiben `int` und werden explizit mit `m_free` freigegeben.

▶ 1:20 Highlight auf `s_printf`. ▶ 1:23 Highlight auf `tmls`-Zeile. ▶ 1:26 aus.

---

## Szene 4 — Variable-System (1:44–2:19)

**Zu sehen:** `cols[]` + `ALEN`, `vs_add`, `vs_string_builder` mit `$col_names[*]`.

> Jetzt der Kern von `csv2insert`: das Variable-System. Eine Variable ist ein
> Name plus eine Werteliste. `vs_add` legt aus dem C-Array `cols` die Variable
> `col_names` an — das ist unsere single source of truth. `vs_string_builder`
> expandiert dann ein Format: `$col_names[*]` wird zur komma-separierten
> Liste, `$'name'` würde SQL-quoten, `$name[N]` greift ein einzelnes Element.
> So entsteht der INSERT-Header aus derselben Spaltenliste, die später die
> Feldzahl prüft.

▶ 1:53 Highlight auf `vs_add`. ▶ 1:55 Highlight auf `vs_string_builder`. ▶ 1:58 aus.

---

## Szene 5 — Leseschleife (2:19–2:54)

**Zu sehen:** Checkpoint `len = s_strlen(buf)`, `vs_read_fields`, `vs_klen`, Feldzahl-Check.

> Die Schleife: `len` ist ein Checkpoint — die Länge des reinen Headers.
> `vs_read_fields` liest eine Zeile von `stdin`, splittet sie am Trennzeichen
> und legt die getrimmten Felder als Variable `fields` ab. Null Werte bedeuten
> EOF. Danach die Feldzahl prüfen: passt sie nicht zu `NCOLS`, überspringen
> wir den Datensatz still. Damit fallen auch leere Zeilen weg, ohne
> Sonderfall. Das ist der klassische Leser-Schleifen-Aufbau — nur ohne
> manuelles Pufferwachstum.

▶ 2:29 Highlight auf `vs_read_fields`. ▶ 2:32 Highlight auf `vs_klen`. ▶ 2:34 aus.

---

## Szene 6 — INSERT-Zeile (2:54–3:22)

**Zu sehen:** `m_setlen(buf, len)`, `vs_sql_string_builder`, `s_puts`.

> Für jeden gültigen Datensatz spulen wir den String mit `m_setlen` auf den
> Header zurück — kein Neuaufbau. `vs_sql_string_builder` hängt dann
> `$'fields[*]'` an, also die Werte SQL-quotiert: einfache Anführungszeichen
> werden verdoppelt, aus `O'Brien` wird `O''Brien`. Das Quoting lebt an genau
> einer Stelle. `s_puts` gibt die fertige Zeile aus.

▶ 2:59 Highlight auf `m_setlen`. ▶ 3:01 Highlight auf `vs_sql_string_builder`. ▶ 3:04 aus.

---

## Szene 7 — Komplett & Test (3:22–4:00)

**Zu sehen:** Die vollständige `csv2insert()`-Funktion, dann der Testlauf und die Ausgabe.

> Das ist der komplette Kern: eine Spaltenliste, ein Header, ein Checkpoint,
> eine Leseschleife. Alles, was Zwischenzustand ist, liegt in `tmls`-Handles
> und räumt sich selbst auf; nichts wird manuell freigegeben. Am Ende steht
> der Test: eine CSV-Zeile rein, ein korrektes INSERT raus — inklusive
> Quote-Doubling. Bewusste Vereinfachung: naives Split am Komma, keine
> Quoted-Felder. Das ist die Grenze dieses Beispiels, nicht die der
> Bibliothek.

▶ 3:22 Szenenwechsel, vollständige Funktion. ▶ 3:38 Testbefehl und Ausgabe.

---

## Regie-Hinweise

- Sprech tempo ruhig; die Haltezeiten am Szenenende sind für genau diesen Text
  dimensioniert.
- Bei den Highlights nicht hetzen: der Rahmen bleibt stehen, bis der jeweilige
  Satz zu Ende ist.
- Die vier Wiederholungen in Szene 4 (`` `$name` ``, `` `$name[*]` ``,
  `` `$name[N]` ``, `` `$'name'` ``) bewusst als Aufzählung sprechen.
- Syntax betonen, nicht buchstabieren: `` `vs_read_fields` `` als ein Name,
  nicht „v-s-unterstrich-read“. Kein C-Grundlagen-Exkurs.
