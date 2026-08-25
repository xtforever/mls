# Konzepte

Die zentralen Ideen hinter MLS — kurz erklärt.

## 1. Integer-Handles statt Pointer

Jedes Array und jeder String wird über eine **positive Ganzzahl** (`int`)
angesprochen. Die eigentliche Speicheradresse steht in einer internen
Master-Liste.

```c
int h = m_alloc(10, sizeof(int), MFREE);  // h ist der "Name" des Arrays
```

- `h <= 0` gilt als „kein Handle" (Null-Sentinel).
- Der Handle ändert sich nie — auch wenn `realloc` den Puffer verschiebt.

## 2. Aufbau eines Handles

Ein Handle ist ein 32-Bit-Wert:

- **Untere 24 Bit:** Index des Slots in der Master-Liste.
- **Obere 8 Bit (7 genutzt):** UAF-Schutzmuster.

Beim Freigeben wandert der Slot in die Free-Liste und das Schutzmuster wird
weitergedreht. Wird der Slot später neu vergeben, trägt er ein anderes
Muster — ein Zugriff über den **alten** Handle scheitert an der
Muster-Prüfung. Use-after-free wird damit ein deterministischer Fehler
statt undefiniertem Verhalten.

## 3. Breiten-bewusste Arrays

`m_alloc(max, w, hfree)` bekommt die **Elementbreite** `w`
(z. B. `sizeof(int)`, `sizeof(char*)`). Alle Operationen rechnen intern in
Bytes; der Anwender denkt in Elementen:

```c
int  h = m_alloc(0, sizeof(int), MFREE);
m_put(h, &(int){42});
```

## 4. Free-Handler

Der dritte Parameter von `m_alloc` entscheidet, was `m_free` mit den
**Elementen** tut:

| Handler | Verhalten |
|---|---|
| `MFREE` | nur den Puffer freigeben |
| `MFREE_STR` | jedes Element als `char*` mit `free()` freigeben |
| `MFREE_EACH` | jedes Element als MLS-Handle mit `m_free()` freigeben (rekursiv) |
| `MFREE_NODESTRUCT` | gar nichts anfassen (Konstanten) |
| `MFREE_NOALLOC` | Bitmap: Puffer gehört dem Anwender (Zero-Copy) |

Mit `m_reg_freefn()` lassen sich eigene Handler registrieren.

## 5. Zero-Copy / Wrapping

Fremder Speicher kann ohne Kopie als MLS-Array eingebunden werden:

```c
int  werte[] = {1, 2, 3};
int  h = m_wrapints(werte, 3);      // MFREE_NOALLOC: mls verwaltet, besitzt nicht
```

Der Puffer darf dann nicht von MLS vergrößert oder freigegeben werden —
der Versuch endet mit `exit(1)`.

## 6. Fehlerklassen

MLS trennt Fehler nach Behandelbarkeit:

| Fehler | Beispiele | Verhalten |
|---|---|---|
| Programmierfehler | UAF, ungültiger Handle, `NULL`-Data-Pointer | `exit(1)` — immer |
| Behandelbar | Index out of bounds, OOM, Overflow | `_safe`-API liefert Code + `mls_errno` |

Details: [error-handling.md](error-handling.md)

## 7. Thread-Sicherheit

Jeder Handle hat einen eigenen Read-Write-Lock. Lesende Zugriffe
(`mls`, `m_len`, …) laufen parallel, schreibende (`m_put`, `m_free`, …)
exklusiv. Aktiv per `-DMLS_THREAD_SAFE` (Unix-Default).

Details: [thread-safety.md](thread-safety.md)

## 8. Debug-Modus

Mit `-DMLS_DEBUG` bekommt jeder Aufruf Kontext (Datei, Zeile, Funktion),
Allokationen werden protokolliert, und bei einem Fehler läuft eine
Post-Mortem-Analyse (`exit_error`), die Handle-Zustand, Erzeuger und
Pufferdaten ausgibt.

Details: [debugging.md](debugging.md)
