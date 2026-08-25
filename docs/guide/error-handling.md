# Fehlerbehandlung

MLS kennt zwei Fehlerklassen — und zwei passende APIs.

## Die Regel

| Fehlerklasse | Beispiele | Verhalten |
|---|---|---|
| **Programmierfehler** | UAF, ungültiger Handle (`m > 0`), `NULL`-Data-Pointer, Resize auf Zero-Copy-Puffer | `ERR()` → Meldung + `exit(1)`, **immer** |
| **Behandelbare Fehler** | Index out of bounds, Out of Memory, Integer-Overflow | `_safe`-API liefert `-1`/`NULL` + `mls_errno` |

Der `m <= 0`-Fall ist **kein Fehler**, sondern der Null-Handle-Sentinel:
Funktionen liefern dafür still ihren Default (`NULL`, `-1`, `0`).

## Standard-API: Fail-Fast

```c
int *p = (int *)mls(h, 999);   // out of bounds → Meldung + exit(1)
```

Gut für die Entwicklung: Fehler werden sofort sichtbar und lassen sich nicht
verschleppen. UAF und ungültige Handles sind Bugs — die kann man nicht
„behandeln", nur beheben.

## `_safe`-API: behandelbare Fehler abfangen

```c
int *p = (int *)mls_safe(h, 999);
if (!p) {
    printf("Fehler: %s\n", mls_errmsg(mls_errno));  // "Index out of bounds"
    // Anwendung entscheidet: Default nehmen, abbrechen, ...
}
```

**Wichtig:** Auch die `_safe`-Varianten beenden das Programm bei
Programmierfehlern. Sie melden nur, was man sinnvoll behandeln kann.

## Fehlercodes

| Code | Bedeutung |
|---|---|
| `MLS_OK` | kein Fehler |
| `MLS_EINVAL` | ungültiger Handle / Null-Handle-Sentinel |
| `MLS_EBOUNDS` | Index außerhalb der Grenzen |
| `MLS_ENOMEM` | kein Speicher |
| `MLS_EUAF` | Use-after-free erkannt |
| `MLS_EOVERFLOW` | Integer-Overflow bei Größenberechnung |

`mls_errno` ist thread-lokal und wird von `_safe`-Aufrufen nur gesetzt, nie
zurückgesetzt. Vor einer neuen Prüfung also selbst auf `MLS_OK` setzen.

## Verfügbare `_safe`-Varianten

| Funktion | Rückgabe bei Fehler | Meldet |
|---|---|---|
| `mls_safe(m, i)` | `NULL` | `MLS_EBOUNDS` |
| `m_alloc_safe(max, w, hfree)` | `-1` | `MLS_ENOMEM` / `MLS_EOVERFLOW` |
| `m_create_safe(max, w)` | `-1` | `MLS_ENOMEM` / `MLS_EOVERFLOW` |
| `m_put_safe(m, data)` | `-1` | `MLS_ENOMEM` |
| `m_new_safe(m, n)` | `-1` | `MLS_ENOMEM` / `MLS_EOVERFLOW` |
| `m_add_safe(m)` | `NULL` | `MLS_ENOMEM` |
| `m_write_safe(m, p, data, n)` | `-1` | `MLS_ENOMEM` / `MLS_EOVERFLOW` |
| `m_read_safe(h, p, &data, n)` | `-1` | `MLS_EBOUNDS` / `MLS_ENOMEM` / `MLS_EOVERFLOW` |
| `m_setlen_safe(m, len)` | `-1` | `MLS_ENOMEM` / `MLS_EOVERFLOW` |
| `m_del_safe(m, p)` | `-1` | `MLS_EBOUNDS` |
| `m_ins_safe(m, p, n)` | `0` | `MLS_ENOMEM` / `MLS_EOVERFLOW` |
| `m_resize_safe(m, n)` | `-1` | `MLS_ENOMEM` / `MLS_EOVERFLOW` |

## `mls_try` und `mls_must`

```c
// Fehlercode holen, ohne selbst mls_errno zurückzusetzen:
int rc = mls_try(m_put_safe(h, &wert));
if (rc != MLS_OK) { /* ... */ }

// Oder: bei Fehler sofort Meldung + exit(1) — wie die Standard-API:
mls_must(m_write_safe(h, 0, daten, n));
```

`mls_try(ausdruck)` setzt `mls_errno` auf `MLS_OK`, führt den Ausdruck aus und
liefert danach den Code. `mls_must(ausdruck)` macht dasselbe und ruft bei
Fehler `_mls_die()` auf (mit Datei/Zeile der Aufrufstelle).

## Vor der Benutzung prüfen: `m_is_freed`

Wo ein Handle aus unsicherer Quelle kommt (Cache, Queue, fremder Thread),
erst prüfen — das ist der **einzige** Weg, UAF ohne `exit(1)` zu erkennen:

```c
if (m_is_freed(h))
    return;                  // Handle ist frei/ungültig
int *p = (int *)mls(h, 0);   // jetzt sicher
```

`m_is_freed(h)` liefert `1` für freie/ungültige Handles und `0` für aktive —
und beendet das Programm nie.

## Double-Free und UAF

Beides sind Programmierfehler und führen zu `exit(1)`:

```c
m_free(h);
m_free(h);       // → "List N is being freed" + exit(1)
int *p = mls(h, 0);  // → UAF-Prüfung + exit(1)
```

Wer „freigeben, falls noch nicht geschehen" braucht:

```c
if (!m_is_freed(h))
    m_free(h);
```

Details zur Post-Mortem-Analyse im Debug-Modus: [debugging.md](debugging.md)
