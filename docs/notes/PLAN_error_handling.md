# Plan: Error Handling & Error Information API

Implements chapters 1 and 9 from IMPROVEMENTS.md.

---

## Scope

### A. Error codes + `mls_errno` (Chapter 9)
Add global error state that programs can query instead of parsing stderr.

### B. `_safe` API variants (Chapter 1)
Non-aborting versions of core functions that return error codes.

### C. `MLS_NO_EXIT` compile mode (Chapter 1)
When defined, `ERR()` macro sets `mls_errno` and does NOT call `exit(1)`.
Requires all ERR call-sites to handle the "soft error" path cleanly.

---

## Implementation Phases

### Phase 1 — Error code infrastructure
**Files:** `mls.h`, `mls.c`

- Add `enum mls_error` with: `MLS_OK, MLS_EINVAL, MLS_EBOUNDS, MLS_ENOMEM, MLS_EUAF, MLS_EOVERFLOW`
- Add global `mls_errno`, `mls_errfunc`, `mls_errfile`, `mls_errline`
- Add `mls_errmsg(int code)` returning descriptive string
- Add `_mls_set_error(code, func)` internal helper
- Make `deb_err` set `mls_errno` before `exit(1)` (always useful)

**Test:** Verify `mls_errmsg` returns non-NULL for every code. Verify `mls_errno` is 0 after `m_init`.

### Phase 2 — `_safe` API variants
**Files:** `mls.h`, `mls.c`

Functions to duplicate with `_safe` suffix:

| Function | On error returns | Sets mls_errno to |
|---|---|---|
| `mls_safe(h,i)` | NULL | MLS_EINVAL / MLS_EBOUNDS / MLS_EUAF |
| `m_put_safe(h,d)` | -1 | MLS_EINVAL / MLS_ENOMEM |
| `m_free_safe(h)` | -1 | MLS_EINVAL / MLS_EUAF |
| `m_write_safe(m,p,data,n)` | -1 | MLS_EINVAL / MLS_EOVERFLOW / MLS_ENOMEM |
| `m_read_safe(h,p,data,n)` | -1 | MLS_EINVAL / MLS_EOVERFLOW |
| `m_setlen_safe(m,len)` | -1 | MLS_EINVAL / MLS_ENOMEM |
| `m_del_safe(m,p)` | void (sets errno) | MLS_EINVAL |

Each safe function replaces every `ERR(...)` call inside its body with:
```c
do { _mls_set_error(code, __func__); return sentinel; } while(0)
```

**Test:** Create `experimental/ex_fuzzy/test_error_api.c` that provokes each error path and verifies return values + `mls_errno`.

### Phase 3 — `MLS_NO_EXIT` mode (SKIPPED)

Skipped as too risky. MLS_NO_EXIT would require every `ERR()` call site
(150+ locations) to have an error return path. The `_safe` function
variants already provide error-recoverable access. Developers who need
error recovery should use the `_safe` API.

---

## Testing Plan

### Per-Phase Tests

**Phase 1 (infrastructure):**
1. `mls_errmsg(MLS_OK)` returns `"Success"` (or non-NULL)
2. `mls_errmsg(MLS_EINVAL)` returns non-NULL
3. `mls_errmsg(-1)` returns `"Unknown error"`
4. `mls_errno == MLS_OK` after `m_init()`
5. Calling any fatal `ERR()` path (unavoidable in normal API) still calls `exit(1)` in default mode

**Phase 2 (safe variants):**

Create `experimental/ex_fuzzy/test_error_api.c`:

| Test | What it does | Expected |
|---|---|---|
| `test_safe_invalid_handle` | `mls_safe(-1, 0)` | returns NULL, `mls_errno == MLS_EINVAL` |
| `test_safe_oob` | create handle with len=3, `mls_safe(h, 999)` | returns NULL, `mls_errno == MLS_EBOUNDS` |
| `test_safe_uaf` | create handle, free it, then `mls_safe(h, 0)` | returns NULL, `mls_errno == MLS_EUAF` |
| `test_put_safe_oom` | (hard to provoke OOM, just verify success path) | returns index >= 0, `mls_errno == MLS_OK` |
| `test_put_safe_invalid` | `m_put_safe(-1, &val)` | returns -1, `mls_errno == MLS_EINVAL` |
| `test_free_safe_double` | create + free + free_safe | returns -1, `mls_errno == MLS_EUAF` |
| `test_write_safe_overflow` | `m_write_safe(h, 1000, data, SIZE_MAX)` | returns -1, `mls_errno == MLS_EOVERFLOW` |
| `test_read_safe_invalid` | `m_read_safe(-1, 0, &dst, 1)` | returns -1, `mls_errno == MLS_EINVAL` |
| `test_del_safe_invalid` | `m_del_safe(-1, 0)` | returns -1, `mls_errno == MLS_EINVAL` |
| `test_setlen_safe_invalid` | `m_setlen_safe(-1, 100)` | returns -1, `mls_errno == MLS_EINVAL` |
| `test_reset_errno` | after error, set `mls_errno = 0`, verify reset | `mls_errno == MLS_OK` |

**Phase 3 (MLS_NO_EXIT):**
1. Run all existing tests (core, table, extra, nested, slice, fuzzy) with `-DMLS_NO_EXIT`
2. Verify no test aborts with exit(1)
3. Add `test_no_exit_mode.c` that deliberately triggers errors and verifies program continues:
   - OOB access → mls_errno set, program continues
   - Double free → mls_errno set, program continues
   - Invalid handle → mls_errno set, program continues

---

## Documentation Plan

**WIKI.md updates:**
- New section: "Error Handling" after "Error Handling & Debugging"
  - `mls_errno`, `mls_errmsg()` reference
  - `_safe` API variants table
  - Example: recovering from OOB vs fatal abort
- Update "Core Philosophy" to mention error codes

**IMPROVEMENTS.md updates:**
- Mark chapters 1 and 9 as completed after all phases

**learn.md updates:**
- Note the `_safe` API availability for robust code

---

## Execution Order

```
Phase 1a — enum + globals + mls_errmsg  → test + commit
Phase 1b — _mls_set_error + deb_err update → test + commit
Phase 2a — mls_safe                     → test + commit
Phase 2b — m_put_safe + m_free_safe     → test + commit
Phase 2c — m_write_safe + m_read_safe   → test + commit
Phase 2d — m_setlen_safe + m_del_safe   → test + commit
Phase 2e — test_error_api.c test program → test + commit
Phase 3a — MLS_NO_EXIT infra            → test + commit
Phase 3b — MLS_NO_EXIT full suite test  → test + commit
```
