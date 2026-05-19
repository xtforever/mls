# MLS: Improvements for Everyday Use

This document outlines remaining work to make MLS a foundational C
library that every C program includes by default to prevent buffer
overflow bugs.

---

## 3. C99 Standard Compliance — Remove GNU Extensions (DONE)

**Completed:**
- Binary literals (`0b00011111` etc.) replaced with hex equivalents
  in `UTF8GET` macro.
- VLA in `vas_printf` replaced with `malloc`/`free`.
- `__thread` replaced with `_Thread_local` via `MLS_THREAD_LOCAL`
  portability macro in `mls_internal.h`.
- CMake C standard bumped from 99 to 11.

## 6. Type-Safe Accessor Macros (DONE)

**Completed:** `_SAFE` and `_UNCHECKED` variants added for all 10
typed macros (`INT`, `UINT`, `FLOAT`, `DOUBLE`, `PTR`, `U32`, `U64`,
`CHAR`, `UCHAR`, `STR`).

- `_UNCHECKED` variants use `m_peek` (no bounds check, fast).
- `_SAFE` variants use `mls_safe` (return 0/NULL on error instead
  of aborting). Uses GNU statement expression `({...})` to avoid
  double-calling `mls_safe`.

---

## 10. Memory Usage / Footprint

**Current:** Every handle allocates a `pthread_rwlock_t` even in
single-threaded mode (lazy allocation via `init_handle_lock`).

**Required:**
- Lazily allocate locks only when first accessed from multiple threads
- Add `m_alloc_nolock(max, w, h)` for handles that will never be shared
  across threads (skip lock allocation entirely)
- Consider packed metadata: embed small structures (≤128 bytes)
  inline to avoid an extra allocation

---

## 11. Documentation

**Current:** Wiki, README, learn.md, thread-safe-porting.md, and
source comments exist but are inconsistent and spread across files.

**Completed:**
- Doxygen HTML API docs (zero warnings)
- Quick reference for LLMs (`QUICKREF.md`)
- Updated README with CMake, safe API, typed macros

**Remaining:**
- Single `mls.3` man page for core API
- `mls_faq.md` for common pitfalls (UAF, handle reuse, const strings)

---

## 12. Platform Support

**Current:** Only tested on Linux with GCC.

**Required:**
- CI matrix: GCC + Clang on Linux, Clang on macOS, MSVC + MinGW on Windows
- Abstract platform differences: `mls_platform.h` for atomics, TLS, alignment
- WASM support: no-threads mode with emscripten compatibility
- Android NDK + iOS compatibility

---

## 13. Performance

**Current:** Every `mls()` call acquires a per-handle rwlock (even
read-only). For tight loops this is measurable overhead.

**Required:**
- Add `mls_no_lock(h,i)` — resolve handle without lock (caller guarantees
  exclusive access or single-threaded context)
- Add batch operations: `m_get_range(h, start, n, dst)` — copy n
  elements in one locked operation
- Profile and optimize the handle-to-pointer path (avoid redundant
  master lock acquisition)

---

## 14. Const Correctness

**Current:** `mls()` returns `void*` even for read paths. Callers
cast away `const`.

**Required:**
```c
void        *mls(int h, size_t i);   // mutable access
const void  *cmls(int h, size_t i);  // read-only access
```

`m_write` should accept `const void* data` (already done).
`m_read` should have a `const` overload for source.

---

## 16. Atomicity for Compound Operations

**Current:** Operations like `m_binsert` (search + insert), `m_slice`
(read + create + write), and `vas_printf` are not atomic — locks are
released between sub-operations.

**Required:**
- Add `m_begin(h)` / `m_end(h)` for explicit lock scoping
- Or add compound functions that hold the write lock across entire
  operation sequences
- Document clearly which operations are atomic and which are not

---

## Completed Work

The following chapters from the original plan have been implemented:

| Chapter | Summary |
|---|---|
| **1. Error Handling** | `mls_errno`, `mls_errmsg()`, `_safe` API variants |
| **2. Thread Safety** | `MLS_THREAD_SAFE` auto-default on Unix |
| **4. Public Header** | Internal types split into `mls_internal.h` |
| **5. Build System** | CMakeLists.txt, pkg-config, install target |
| **7. String Functions** | Verified existing, added `s_from_double`/`s_from_int` |
| **8. Table/Dict** | Locking fix, `mt_get_str`, `mt_foreach`, `mt_remove` |
| **9. Error API** | Error code enum, `mls_errmsg()` |
| **15. Statistics** | `m_count_allocated`, `m_total_bytes`, `m_peak_handles`, `m_debug_print` |

---

## Priority Summary

| Priority | Category | Impact |
|---|---|---|
| **P1** | C99 compliance | Blocks non-GCC compilers |
| **P2** | Type-safe accessor variants | API ergonomics |
| **P2** | Error information API | Debugging UX |
| **P2** | Compound operation atomicity | Correctness |
| **P2** | Documentation (man page, FAQ) | Developer experience |
| **P3** | Memory usage / footprint | Embedded / resource-constrained |
| **P3** | Performance tuning | Hot-path optimization |
| **P3** | Platform support | Portability |
| **P3** | Const correctness | API quality |
