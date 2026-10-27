---
name: forth-db
description: Use when implementing, reviewing, or testing the Forth-ISAM database modules in lib/ (arena, bptree, pager, m_forth, m_isam, m_fproto, …). Loads the project conventions and the Pflichtenheft pointer.
---

# forth-db — Konventionen

Single Source of Truth: `docs/notes/PFLICHTENHEFT_forth_sql.md` (zuerst §5.5, §6, §11a).

## Grundregeln
- C11, keine neuen Abhängigkeiten; Module unter `lib/`.
- **Ein Modul = ein `*.c/h`-Paar + ein assert-basierter Selbsttest** (kein Framework).
- **Grüner Selbsttest ⇒ Modul eingefroren**: nicht erneut testen, nicht „mal eben
  patchen“; Änderungen laufen als neue Version.
- `ponytail:`-Kommentar für bewusste Decken — Decke **und** Upgrade-Pfad benennen.
- Handles sind `> 0`; `m_init()`/`m_destruct()`; die AGENTS.md-Regeln gelten.
- Build: `gcc -std=c11 -I./lib …`; Debug mit ASan + `-DMLS_DEBUG`.

## Reihenfolge (Pflichtenheft §11a.4)
Phase 0: `arena` → `carray`/`darray` → `strbuf` → `htable` → `hashmap` → `token`
→ `bptree` → `pager`
Phase 1: `m_strpool` → `m_forth`
Phase 2: `m_isam` → `m_dbwords`
Phase 3: `m_fproto` → `m_engine` → `m_fapi` → `demo`
Phase 4 (später): `m_ftransport` → `m_fserver` → `m_fclient`

## Forth-Engine (§5.5)
`m_forth` braucht: Execution Tokens, `DEFER`/`IS`, Wordlists/Search-Order,
`ABORT`/`CATCH`/`THROW`, `LITERAL`/`POSTPONE`, Underflow-Erkennung. Threading =
DTC-artig in portablem C; keine Native-Compilation, keine Locals. C-Primitives
(`forth_define`) sind der Normalfall (Hybrid).
