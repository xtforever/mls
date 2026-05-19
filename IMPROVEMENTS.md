# MLS: Improvements for Everyday Use

This document outlines remaining work to make MLS a foundational C
library that every C program includes by default to prevent buffer
overflow bugs.

---

## 3. C99 Standard Compliance — Remove GNU Extensions

**Current:** The code uses `__thread` (GNU extension), `alloca`, and
binary integer literals `0b00011111`. This prevents compilation on
compilers other than GCC/Clang.

**Required:** Replace with C99/C11 standard equivalents:
- `__thread int` → `_Thread_local int` (C11) or `thread_local int` (C23)
- `0b00011111` → `0x1F` (hex equivalent)
- Avoid `alloca` / VLAs in favor of `malloc`/`free` or fixed buffers

---

## 6. Type-Safe Accessor Macros

**Current:** `INT(h,i)`, `CHAR(h,i)`, `STR(h,i)` macros perform
unchecked access through `mls()` and `exit()` on errors. `FLOAT`,
`DOUBLE`, `PTR`, `U32`, `U64` macros have been added but no
`_SAFE`/`_UNCHECKED` variants exist.

**Required:** Provide checked and unchecked variants:
```c
#define INT_SAFE(h,i) (*(int*)mls_safe(h,i))       // returns NULL on error
#define INT_UNCHECKED(h,i) (*(int*)m_peek(h,i))    // no bounds check
```

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
