# Anleitungen (How-To)

Typische Muster für den Alltag mit MLS.

## Integer-Liste

```c
int h = m_alloc(8, sizeof(int), MFREE);

for (int i = 0; i < 100; i++)
    m_put(h, &i);               // wächst automatisch

int summe = 0;
for (int i = 0; i < m_len(h); i++)
    summe += INT(h, i);

m_free(h);
```

## String-Liste mit Auto-Free

```c
int h = m_alloc(4, sizeof(char *), MFREE_STR);

char *s = strdup("hallo");
m_put(h, &s);                   // m_free(h) ruft später free() auf jedes Element

m_free(h);                      // kein manuelles Aufräumen nötig
```

## Verschachtelte Listen (Baum/Graph)

```c
int wurzel = m_alloc(0, sizeof(int), MFREE_EACH);

int kind1 = m_alloc(0, sizeof(int), MFREE_EACH);
int kind2 = m_alloc(0, sizeof(int), MFREE_EACH);

m_put(wurzel, &kind1);
m_put(wurzel, &kind2);

m_free(wurzel);                 // gibt rekursiv alle Kinder frei
```

Zyklen sind sicher: `m_free` markiert freigegebene Handles und überspringt
sie beim nächsten Kontakt.

## Pre-Allocation für Random Access

```c
int h = m_alloc(1000, sizeof(double), MFREE);
m_setlen(h, 1000);              // direkt 1000 Elemente reservieren

DOUBLE(h, 42) = 3.14;           // schreiben ohne m_put
```

## Strings bauen

```c
int s = s_printf(0, 0, "Temperatur: %.1f°C", 21.5);
printf("%s\n", m_str(s));
m_free(s);
```

`s_printf(h, pos, fmt, ...)` hängt formatiert an Handle `h` an (ab Position
`pos`) und vergrößert bei Bedarf. `h == 0` legt einen neuen String an.

## Zero-Copy: fremden Speicher einbinden

```c
char *rohdaten = mmap(...);              // irgendein fremder Puffer
int  h = m_wrapcstr(rohdaten);           // MLS verwaltet, besitzt aber nicht
printf("%s\n", m_str(h));
m_free(h);                               // Puffer bleibt unangetastet
```

## Fehler behandeln

**Standard (Fail-Fast):** Programmierfehler beenden das Programm sofort —
gut zum Entwickeln, nichts wird verschleppt.

**Robust (`_safe`):** Behandelbare Fehler abfangen:

```c
int *p = (int *)mls_safe(h, index);
if (!p) {
    if (mls_errno == MLS_EBOUNDS)
        return -1;                       // Index zu groß: Aufrufer entscheidet
    return -2;
}
```

**Opt-in `mls_must`:** Eine `_safe`-Operation mit sofortigem Abbruch bei Fehler:

```c
mls_must(m_write_safe(h, 0, daten, n));  // bei OOM/Overflow: Meldung + exit(1)
```

Details: [error-handling.md](error-handling.md)

## Tabellen (Key-Value)

```c
#include "m_table.h"

int tab = mt_create();
mt_set_int(tab, "antwort", 42);
int antwort = mt_get_int(tab, "antwort");
mt_free(tab);
```

Details: [tables.md](tables.md)

## Eigener Free-Handler

```c
void meine_cleanup(int h) {
    int i; MeinTyp *e;
    m_foreach(h, i, e) free(e->name);
}

int handler_id = m_reg_freefn(meine_cleanup);
int h = m_alloc(4, sizeof(MeinTyp), (uint8_t)handler_id);
```

`m_free(h)` ruft dann zuerst `meine_cleanup(h)` auf.

## Iteration

```c
int i; int *wert;
m_foreach(h, i, wert) {
    printf("%d\n", *wert);
}
```

`m_foreach` startet bei `-1` und läuft bis `m_len(h)`.
