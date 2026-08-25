# MLS Guide

Konzepte, Handlungsanweisungen und tiefere Erläuterungen.

## Einstieg

| Seite | Inhalt |
|---|---|
| [concepts.md](concepts.md) | Kernkonzepte: Handles, UAF-Schutz, Zero-Copy, Strings, Tabellen |
| [howto.md](howto.md) | Anleitungen: typische Muster für Listen, Strings, Tabellen, Fehlerbehandlung |
| [advantages.md](advantages.md) | Warum MLS? Vorteile gegenüber rohen Pointern *(englisch)* |

## Deep Dives

| Seite | Inhalt |
|---|---|
| [error-handling.md](error-handling.md) | `ERR()` vs. `_safe`, `mls_errno`, `mls_try`/`mls_must` |
| [memory-management.md](memory-management.md) | Speicherverwaltung, Free-Handler, Rekursion *(englisch)* |
| [thread-safety.md](thread-safety.md) | Locking-Architektur, TSAN, Fallstricke *(englisch)* |
| [security.md](security.md) | Sicherheitsbetrachtungen *(englisch)* |
| [debugging.md](debugging.md) | `MLS_DEBUG`, Tracing, Post-Mortem-Analyse *(englisch)* |
| [philosophy.md](philosophy.md) | Design-Philosophie *(englisch)* |

## Module & Anwendungen

| Seite | Inhalt |
|---|---|
| [strings.md](strings.md) · [string-functions.md](string-functions.md) · [string-expansion.md](string-expansion.md) · [advanced-strings.md](advanced-strings.md) | String-Handling in allen Facetten *(englisch)* |
| [tables.md](tables.md) | `m_table` — Dictionary/Key-Value-Store *(englisch)* |
| [wrappers.md](wrappers.md) | Externen Speicher per Zero-Copy einbinden *(englisch)* |
| [slice.md](slice.md) | `m_slice` — Teilbereiche kopieren *(englisch)* |
| [real-world-examples.md](real-world-examples.md) · [case-study-curl.md](case-study-curl.md) · [curl-integration.md](curl-integration.md) | Beispiele aus der Praxis *(englisch)* |
| [httpparser.md](httpparser.md) | HTTP-Parsing mit MLS *(englisch)* |
| [beginners-guide.md](beginners-guide.md) | Einstieg für C-Umsteiger *(englisch)* |

## API

Vollständige, aus den Quellen generierte Referenz:
[docs/api/API.md](../api/API.md)
