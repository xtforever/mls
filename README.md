# MLS — Int-Handles statt C-Pointer

`mls` ist eine kleine C-Bibliothek, die dynamische Arrays und Strings über
ganzzahlige Handles verwaltet statt über rohe Pointer. Ein Handle ist eine
4-Byte-Ganzzahl; die eigentliche Adresse bleibt in einer internen Tabelle.

Damit werden die häufigsten C-Fehler abgefangen:

- **Use-after-free wird ein Bounds-Check** — ein freigegebener Handle fällt
  bei der nächsten Benutzung laut auf, statt still in fremden Speicher zu greifen.
- **Keine Leaks** — `m_free()` räumt auch verschachtelte Strukturen rekursiv ab.
- **Kein Pointer-Gejongle** — eine Funktion, die einen Puffer vergrößert,
  braucht kein `void**` und keine Längen-Rückgabe mehr.
- **Eine API für alles** — Liste, Baum, Graph oder String: immer `int` Handle,
  `m_put`/`mls`/`m_free`.

## Schnellstart

```c
#include "mls.h"

int main(void) {
    m_init();

    int h = m_alloc(10, sizeof(int), MFREE);   // dynamisches int-Array
    for (int i = 0; i < 5; i++)
        m_put(h, &i);

    printf("%d\n", INT(h, 2));                 // typisierter Zugriff

    m_free(h);
    m_destruct();
}
```

Bauen (Single-File, keine Abhängigkeiten außer der C-Standardbibliothek):

```bash
gcc -I./lib prog.c lib/mls.c -o prog -lpthread -lm -ldl
```

Mehr dazu: **[docs/quickstart.md](docs/quickstart.md)**

## Dokumentation

| Teil | Ort | Inhalt |
|---|---|---|
| **Quickstart** | [docs/quickstart.md](docs/quickstart.md) | Installation, erstes Programm, wichtigste API |
| **README** | diese Datei | Überblick und Einstieg |
| **API (auto-generiert)** | [docs/api/API.md](docs/api/API.md) · [docs/api/API.html](docs/api/API.html) | Aus den Doxygen-Kommentaren in `lib/*.c` generiert (`python3 generate_docs.py`) |
| **Guide** | [docs/guide/index.md](docs/guide/index.md) | Konzepte, Anleitungen, Deep Dives, Vorteile |
| **Notizen** | [docs/notes/](docs/notes/) | Interne Entwicklungsnotizen, Fehlerhistorie, Pläne |

## Bauen

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug   # oder: make
cmake --build build
```

Optionen: `-DMLS_THREAD_SAFE=OFF` (Threads aus), `-DMLS_WERROR=ON`
(Warnungen als Fehler), `make production=1` (optimiert).

## Tests

```bash
cd tests && make thread_safe=1
```

## Lizenz

MIT — siehe [LICENSE](LICENSE).
