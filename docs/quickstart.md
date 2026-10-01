# Quickstart

In fünf Minuten von null zum ersten laufenden Programm.

## Eine Datei kopieren, fertig

`mls` gibt es auch als einzelnen Header (stb-Stil). Für ein neues Projekt
kopierst du einfach **eine** Datei aus diesem Repo — kein Build-System,
keine weiteren Quellen nötig:

- `mls_core.h` — Kern (Arrays, Strings, Listen, Tabellen, Tools)
- `mls_full.h` — zusätzlich `m_http`, `m_hdf`, `m_http_server`, `m_flask`

```bash
cp mls_core.h dein_projekt/
```

In **genau einer** `.c`-Datei das Implementierungs-Makro setzen und den Header
als Erstes einbinden; alle anderen Dateien binden ihn nur ein:

```c
// main.c
#define MLS_CORE_IMPLEMENTATION
#include "mls_core.h"   // muss vor <stdio.h> etc. stehen
#include <stdio.h>

int main(void)
{
    m_init();
    int h = m_alloc(10, sizeof(int), MFREE);
    int v = 42;
    m_put(h, &v);
    printf("%d\n", INT(h, 0));   // 42
    m_free(h);
    m_destruct();
    return 0;
}
```

```bash
gcc -std=c11 main.c -o prog -lpthread -lm -ldl
```

Pthreads sind auf Unix standardmäßig aktiv (`-DMLS_THREAD_SAFE=0` schaltet sie
ab). Beide Header werden von `python3 generate_amalgam.py` aus `lib/` erzeugt;
`python3 generate_amalgam.py --check` prüft sie.

## Voraussetzungen

- C99-Compiler (GCC/Clang), `make` oder CMake
- Optional: pthreads (auf Unix standardmäßig aktiv)

## Bauen

**CMake (empfohlen):**

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

**Ohne Build-System:**

```bash
gcc -I./lib prog.c lib/mls_base.c lib/mls_ext.c -o prog -lpthread -lm -ldl
```

## Erstes Programm

```c
#include "mls.h"
#include <stdio.h>

int main(void)
{
    m_init();                                  // 1. Bibliothek initialisieren

    int h = m_alloc(10, sizeof(int), MFREE);   // 2. int-Array anlegen

    for (int i = 0; i < 5; i++)
        m_put(h, &i);                          // 3. Elemente anhängen

    printf("%d\n", INT(h, 2));                 // 4. typisiert lesen → 2

    m_free(h);                                 // 5. Handle freigeben
    m_destruct();                              // 6. Bibliothek aufräumen
    return 0;
}
```

Ausgabe: `2`

## Lebenszyklus

| Funktion | Wann |
|---|---|
| `m_init()` | einmal am Programmanfang |
| `m_destruct()` | einmal am Programmende (gibt alles frei) |
| `m_alloc(max, w, hfree)` | neues Array: Kapazität `max`, Elementbreite `w`, Free-Handler `hfree` |
| `m_free(h)` | Array freigeben (bei `MFREE_STR`/`MFREE_EACH` inklusive Elemente) |

## Essenzielle API

```c
int  h   = m_alloc(4, sizeof(int), MFREE);  // neues Array
int  idx = m_put(h, &wert);                 // anhängen → Index
int *p   = (int *)mls(h, idx);              // Zeiger auf Element (bounds-checked)
int  n   = m_len(h);                        // logische Länge
m_setlen(h, n + 1);                         // Länge setzen (resized bei Bedarf)
m_write(h, 0, daten, 2);                    // n Elemente ab Index p schreiben
void *dst = NULL;
m_read(h, 0, &dst, 2);                      // n Elemente ab Index p lesen
int  weg  = m_is_freed(h);                  // 1 = Handle ist frei/ungültig
m_free(h);                                  // freigeben
```

## Typisierte Zugriffe (Makros)

Statt `*(int*)mls(h,i)` einfach:

```c
INT(h,i)   CHAR(h,i)   FLOAT(h,i)   DOUBLE(h,i)
U32(h,i)   U64(h,i)    PTR(h,i)     STR(h,i)
```

Für Strings: `m_str(h)` liefert den `char*`-Puffer eines Byte-Arrays,
`CHARP(h)` ist das Gleiche als Makro.

## Temporäre Handles automatisch freigeben: `tmls`

`tmls` ist ein Handle, das am Ende seines Blocks automatisch freigegeben wird —
ideal für Zwischenergebnisse, die man sonst leicht vergisst:

```c
m_init();
{
    tmls h = m_alloc(4, sizeof(int), MFREE);   // tmls statt int
    int v = 42;
    m_put(h, &v);
    printf("%d\n", INT(h, 0));                 // 42
}                                              // h wird hier automatisch freigegeben
m_destruct();
```

- `m_free(h)` entfällt: die Freigabe passiert beim Verlassen des Blocks.
- Nur für **temporäre** Handles gedacht. Dauerhafte Handles bleiben `int` und
  werden weiterhin explizit mit `m_free` freigegeben.
- **Immer initialisieren:** ein `tmls h;` ohne Zuweisung enthält Müll und wird
  beim Blockende an `m_free` übergeben.
- Basiert auf `__attribute__((cleanup))` (GCC/Clang). Ohne diese Unterstützung
  (z. B. MSVC) ist `tmls` ein normales `int` — dann manuell freigeben.
- Die automatische Freigabe ruft das echte `m_free` auf (nicht den
  `MLS_DEBUG`-Wrapper), liefert also keine `__LINE__/__FILE__`-Diagnose.

## Strings

```c
int s = s_cstrdup("hallo");      // String-Handle (kopiert)
printf("%s\n", m_str(s));        // "hallo"

s_printf(s, m_len(s), " welt");  // anfügen mit Format
printf("%s\n", m_str(s));        // "hallo welt"

m_free(s);
```

`m_tool.h`/`m_extra.h` liefern `s_printf`, `s_app`, `s_split`, `s_join`,
`se_string` (Interpolation) und mehr — siehe
[docs/guide/string-functions.md](guide/string-functions.md).

## Fehlerbehandlung in einem Satz

Die normale API (`m_put`, `mls`, …) beendet das Programm bei
Programmierfehlern (UAF, ungültiger Handle) mit `exit(1)`. Die `_safe`-Varianten
melden behandelbare Fehler (Index out of bounds, OOM, Overflow) über Rückgabewert
und `mls_errno`:

```c
int *p = (int *)mls_safe(h, 999);
if (!p)
    printf("Fehler: %s\n", mls_errmsg(mls_errno));  // "Index out of bounds"
```

Details: **[error-handling.md](guide/error-handling.md)**

## Häufige Fallstricke

1. **`m_init()` vergessen** → erster Aufruf stirbt mit „Not init."
2. **Handle nach `m_free` benutzen** → UAF-Protection beendet das Programm.
   Vorher mit `m_is_freed(h)` prüfen, wenn der Zustand unsicher ist.
3. **`mls()`-Zeiger nicht über Resize hinweg halten** — `realloc` kann den
   Puffer verschieben. Handle erneut auflösen statt Zeiger zu cachen.
4. **Breite falsch wählen** — `m_alloc(n, sizeof(int), MFREE)` für `int`,
   `sizeof(char*)` für String-Listen.

## Weiter

- Konzepte: [docs/guide/concepts.md](guide/concepts.md)
- Anleitungen: [docs/guide/howto.md](guide/howto.md)
- Alle Funktionen: [docs/api/API.md](api/API.md)
