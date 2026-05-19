# MLS: Improvements for Everyday Use

This document outlines what needs to change for MLS to become a
foundational C library — like libc — that every C program includes
by default to prevent buffer overflow bugs.

---

## 1. Error Handling — Return Error Codes (DONE)

**Completed:** `_safe` API variants added in `mls.c`.

| Function | Returns | Sets `mls_errno` |
|---|---|---|
| `mls_safe(h,i)` | NULL | `MLS_EINVAL` / `MLS_EBOUNDS` / `MLS_EUAF` |
| `m_put_safe(h,d)` | -1 | `MLS_EINVAL` / `MLS_ENOMEM` |
| `m_free_safe(h)` | -1 | `MLS_EINVAL` / `MLS_EUAF` |
| `m_write_safe(m,p,data,n)` | -1 | `MLS_EINVAL` / `MLS_EOVERFLOW` / `MLS_ENOMEM` |
| `m_read_safe(h,p,data,n)` | -1 | `MLS_EINVAL` / `MLS_EBOUNDS` / `MLS_EOVERFLOW` / `MLS_ENOMEM` |
| `m_setlen_safe(m,len)` | -1 | `MLS_EINVAL` / `MLS_ENOMEM` |
| `m_del_safe(m,p)` | -1 | `MLS_EINVAL` / `MLS_EBOUNDS` |

The `MLS_NO_EXIT` compile flag was considered but **skipped** — it
would require error-labels at every `ERR()` call site (150+). The
`_safe` variants are the recommended approach for error recovery.

---

## 2. Thread Safety — Make It Default, Not Optional

**Current:** Thread safety requires `-DMLS_THREAD_SAFE` and linking
`-lpthread`. Without it, behavior in multi-threaded programs is
undefined.

**Required:** Thread safety should be the default. If the platform
has no pthreads, fall back to no-ops at compile time.

**Specific issues found:**
- `trace_level` is an unprotected global int (data race)
- `conststr_lookup_c` has a 64-bit pointer-truncation race in `m_binsert`
- `m_table_create()` has a double-checked locking init bug
- `MFREE_TABLE_ENTRIES_HDLR` initialization is racy
- Ring buffer functions (`ring_put`/`ring_get`) bypass all per-handle locks
- `m_slice` uses source pointer after releasing source lock
- `vas_printf` (fixed) had a dangling-pointer race
- `m_binsert` is not atomic (read/write across multiple function calls)

**Proposed fix:**
```c
// Autodetect threading support at compile time
#if defined(__STDC_NO_THREADS__)
  #define MLS_THREAD_SAFE 0
#else
  #define MLS_THREAD_SAFE 1
#endif
```

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

## 4. Public Header Cleanup

**Current:** `mls.h` exposes internal implementation details
(`struct ls_st`, `lst_t`, macros like `REAL_HDL`) to every includer.

**Required:** Split into two headers:
- `mls.h` — public API only (type-safe accessors, clean prototypes)
- `mls_internal.h` — for library implementation files

---

## 5. Build System — Standardize for Distribution

**Current:** Hand-written makefiles with project-specific patterns.
No `pkg-config` support, no CMake integration, no install target.

**Required:**
- Remove `rules.mk` dependency from project makefiles
- Add `make install` that puts headers in `$(PREFIX)/include/mls/`
  and library in `$(PREFIX)/lib/libmls.a`
- Provide `mls.pc` for `pkg-config`
- Optional: CMakeLists.txt for wider IDE support
- Single-header distribution option (`mls_amalgamated.h`)

---

## 6. Type-Safe Accessor Macros

**Current:** `INT(h,i)`, `CHAR(h,i)`, `STR(h,i)` macros perform
unchecked access through `mls()` and `exit()` on errors.

**Required:** Provide checked and unchecked variants:
```c
#define INT(h,i)     (*(int*)mls(h,i))            // checked, exits on error
#define INT_SAFE(h,i) (*(int*)mls_safe(h,i))       // checked, returns NULL
#define INT_UNCHECKED(h,i) (*(int*)m_peek(h,i))    // no bounds check
```

Add typed macros for common types: `FLOAT(h,i)`, `DOUBLE(h,i)`,
`PTR(h,i)`, `U32(h,i)`, `U64(h,i)`.

---

## 7. String Functions — Interface Consistency

**Current:** The string API is rich but spread across modules with
inconsistent naming conventions (`s_replace_c` vs `s_replace`,
`m_str_from_file` vs `s_read_file`, etc.). There is no general
numeric-to-string converter.

**Already available:**

| Function | Location | Purpose |
|---|---|---|
| `s_cmp(a,b)` | `m_tool.h:24` | Compare two string handles |
| `s_has_prefix(h, prefix)` | `m_tool.h:19` | Prefix check |
| `s_has_suffix(h, suffix)` | `m_tool.h:20` | Suffix check |
| `s_find(h, sub)` | `m_tool.h:28` | Find substring (returns index, -1 if not found) |
| `s_replace_c(h, old, rep)` | `m_tool.h:36` | Replace all C-string occurrences |
| `s_replace(d, src, p, r, cnt)` | `m_tool.h:70` | Replace using handles |
| `s_from_long(val)` | `m_tool.h:21` | Convert long to string handle |
| `m_str_from_file(name)` | `m_tool.h:53` | Read file into MLS string |
| `s_trim(m)` | `m_tool.h:79` | Trim whitespace in-place |
| `s_trim_c(h, chars)` | `m_tool.h:37` | Trim specific characters |
| `s_split(m, s, c, rw)` | `m_tool.h:128` | Split into `char*` list |
| `s_msplit(dest, src, pat)` | `m_tool.h:82` | Split into handle list |
| `s_trim_left_c / s_trim_right_c` | `m_extra.h:15-16` | Directional trim |

**Still missing:**
- Generic numeric-to-string (double, int → handle) beyond `s_from_long`

---

## 8. Table/Dictionary Improvements

**Current:** `m_table` has thread-safety bugs (init race) and
requires manual type tracking.

**Required:**
- Fix `MFREE_TABLE_ENTRIES_HDLR` double-checked locking init
- Add `mt_get_str(t, key)` that returns `const char*` directly
- Add `mt_get_handle(t, key)` that returns the raw handle
- Add `mt_foreach(t, iter_fn)` for iteration without raw access
- Add `mt_remove(t, key)` as first-class function
- Support string-table interop: `mt_from_json(h)`, `mt_to_json(h)`

---

## 9. Error Information API (DONE)

**Completed:** `mls_errno`, `mls_errmsg()`, and error code enum added.

```c
enum mls_error {
    MLS_OK = 0,
    MLS_EINVAL,    // invalid handle
    MLS_EBOUNDS,   // index out of bounds
    MLS_ENOMEM,    // out of memory
    MLS_EUAF,      // use-after-free detected
    MLS_EOVERFLOW, // integer overflow
};
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

**Required:**
- Single `mls.3` man page for core API
- Single `mls_guide.md` covering all modules end-to-end
- `mls_faq.md` for common pitfalls (UAF, handle reuse, const strings)
- Remove documentation from WIKI.md (it's an LLM reference, not user docs)
- Every public function must have a `@return` doc describing error conditions

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

## 15. Statistics / Introspection

Add runtime visibility for debugging and monitoring:
```c
size_t m_count_allocated(void);      // total active handles
size_t m_total_bytes(void);          // total allocated memory
size_t m_peak_handles(void);         // peak handle count
void   m_debug_print(FILE *fp);      // dump all handles to stream
```

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

## Priority Summary

| Priority | Category | Impact |
|---|---|---|
| **P0** | Error handling (no exit) | Blocks library adoption |
| **P0** | Thread safety defaults | Blocks multi-threaded use |
| **P1** | C99 compliance | Blocks non-GCC compilers |
| **P1** | Public header cleanup | Blocks clean integration |
| **P1** | Build system / install | Blocks packaging |
| **P1** | String function completion | API gap closure |
| **P2** | Type-safe accessors | API ergonomics |
| **P2** | Error information API | Debugging UX |
| **P2** | Compound operation atomicity | Correctness |
| **P3** | Statistics / introspection | Operational visibility |
| **P3** | Performance tuning | Hot-path optimization |
| **P3** | Platform support | Portability |
