# MLS Quick Reference for LLMs

Concise guide combining the API reference (WIKI.md) and technical
findings (learn.md) for the Master List System library.

---

## Core Philosophy

- **Handle-based management:** Never use raw pointers for dynamic
  arrays or strings. Use `int` handles.
- **Memory safety:** Every allocated handle MUST be freed.
- **Wrapping (no ownership):** Handles can wrap existing C memory
  via `m_wrapstrings`/`m_wrapints`/`m_wrapcstr` — MLS never frees
  or reallocs memory owned by the caller (`MFREE_NOALLOC`).
- **Handles are strictly positive** (`> 0`). Use `if (h > 0)` to
  check if a handle is allocated.
- **Width-aware:** Arrays are created with a specific element width
  (e.g., `sizeof(int)`).

## Library Lifecycle

```c
m_init();       // Start (initializes all systems, including conststr)
// ... your code ...
m_destruct();   // End (frees all remaining handles)
```

**Note:** `conststr_init()` and `conststr_free()` are **obsolete**
— constant string interning is auto-initialized by `m_init()` and
cleaned up by `m_destruct()`.

## Essential API Reference

### Basic Handle Management (`mls.h`)

| Function | Description |
|---|---|
| `m_alloc(max, w, free_hdl)` | Create a list. `free_hdl`: `MFREE`, `MFREE_STR`, `MFREE_EACH` |
| `m_free(h)` | Free a handle (safe on `h <= 0`) |
| `m_is_freed(h)` | Check if slot is freed (safe, no crash) |
| `m_is_valid(h)` | Inverse of `m_is_freed` |
| `m_dub(m)` | Duplicate handle (writable copy of NOALLOC wraps) |
| `mls(h,i)` | Pointer to element at index `i` (aborts on error) |
| `mls_safe(h,i)` | Like `mls` but returns NULL on error, sets `mls_errno` |
| `m_put(h, data)` | Append data, returns index |
| `m_put_safe(h, data)` | Non-aborting append, returns -1 on error |
| `m_free_safe(h)` | Non-aborting free, returns -1 on error |
| `m_write(m, p, data, n)` | Write n elements at position p |
| `m_read(h, p, &dst, n)` | Read n elements into dst |
| `m_del(h, i)` | Delete element at index `i` |
| `m_setlen(m, len)` | Set logical length |
| `m_len(h)` | Number of elements |
| `m_width(h)` | Element width in bytes |
| `m_bufsize(m)` | Allocated capacity |
| `m_clear(m)` | Set length to 0 |
| `m_peek(m, i)` | Element pointer (no bounds check) |

### Typed Accessors (macros)

| Macro | Expands to |
|---|---|
| `INT(h,i)` | `*(int *)mls(h,i)` |
| `UINT(h,i)` | `*(unsigned int *)mls(h,i)` |
| `FLOAT(h,i)` | `*(float *)mls(h,i)` |
| `DOUBLE(h,i)` | `*(double *)mls(h,i)` |
| `PTR(h,i)` | `*(void **)mls(h,i)` |
| `U32(h,i)` | `*(uint32_t *)mls(h,i)` |
| `U64(h,i)` | `*(uint64_t *)mls(h,i)` |
| `CHAR(h,i)` | `*(char *)mls(h,i)` |
| `UCHAR(h,i)` | `*(unsigned char *)mls(h,i)` |
| `STR(h,i)` | `*(char **)mls(h,i)` |
| `CHARP(m)` | `(char *)m_buf(m)` |
| `MSTR(x)` | `(char *)mls(x, 0)` |
| `m_cat(h, s)` | `m_write(h, m_len(h), s, strlen(s))` |
| `m_foreach(lst, i, ptr)` | Iteration: `for(i=-1; m_next(lst,&i,&ptr);)` |

### Tables / Dictionaries (`m_table.h`)

Entries are kept in sorted order for O(log N) lookup via binary search.

| Function | Description |
|---|---|
| `m_table_create()` | Create a table |
| `mt_seti(t, key, val)` | Set integer |
| `mt_sets(t, key, val)` | Set dynamic string (copies C-string) |
| `mt_setc(t, key, val)` | Set constant string (uses `s_cstr`) |
| `mt_seth(t, key, h, type)` | Set MLS handle |
| `mt_get(t, key)` | Get raw value (int or handle) |
| `mt_get_str(t, key)` | Get `const char*` (string entries only) |
| `mt_get_handle(t, key)` | Get raw `int` handle |
| `mt_remove(t, key)` | Remove entry by C-string key |
| `mt_foreach(t, iter_fn, ctx)` | Callback iteration |
| `m_table_free(t)` | Free table and contents |

### Error Information (`mls_errno`)

```c
enum mls_error {
    MLS_OK = 0,
    MLS_EINVAL,    // invalid handle
    MLS_EBOUNDS,   // index out of bounds
    MLS_ENOMEM,    // out of memory
    MLS_EUAF,      // use-after-free detected
    MLS_EOVERFLOW, // integer overflow
};
extern int mls_errno;
const char *mls_errmsg(int code);
```

### Statistics / Introspection

```c
size_t m_count_allocated(void);   // active handles
size_t m_total_bytes(void);       // total allocated memory
size_t m_peak_handles(void);      // peak slot count
void   m_debug_print(FILE *fp);   // dump all handles
```

## String Handling (`m_tool.h` + `m_extra.h`)

MLS strings are handles with width 1.

### Core String Functions

| Function | Description |
|---|---|
| `s_new()` | Create empty string |
| `s_free(h)` | Free string |
| `s_dup(s)` | Create from C-string |
| `s_clone(h)` | Clone a string handle |
| `s_from_long(val)` | long → string handle |
| `s_from_double(val)` | double → string handle |
| `s_from_int(val)` | int → string handle |
| `s_printf(h, p, fmt, ...)` | Formatted write. `p<0` appends. `h=0` allocates new |
| `s_cat(h, src)` | Concatenate C-string |
| `s_app(h, ...)` | Append multiple C-strings (NULL-terminated) |
| `s_strlen(h)` | Length **excluding** trailing null |
| `s_slice(dest, offs, m, a, b)` | Sliced copy, **does** null-terminate |
| `s_cmp(a, b)` | Compare two string handles |
| `s_find(h, sub)` | Find substring (returns index, -1 if not found) |
| `s_has_prefix(h, prefix)` | Prefix check |
| `s_has_suffix(h, suffix)` | Suffix check |
| `s_replace_c(h, old, rep)` | Replace all C-string occurrences |
| `s_trim(m)` | Trim whitespace in-place |
| `s_split(m, s, c, rw)` | Split into `char*` list |
| `s_lower(m)` / `s_upper(m)` | Case conversion (in-place) |
| `m_str_from_file(name)` | Read file into string handle |
| `s_strncpy(dst, src, max)` | Copy up to max chars via `s_slice` |

### Constant Strings

Optimized for literal strings. No manual freeing required.

```c
int s_cstr(const char *s);       // zero-copy interning
int s_cstrdup(const char *s);    // copies the string
int s_ccstr(const char *s);      // alias for zero-copy interning
int cs_printf(const char *fmt, ...); // formatted constant string
```

**Important:** Constant strings include a null terminator in their
length. `s_cstr("Hello World")` has a length of 12. When slicing,
the last index (length - 1) is the null terminator.

## Wrapping External Memory (Zero-Copy)

| Function | Wraps | Width |
|---|---|---|
| `m_wrapstrings(list, n)` | Existing `char*[]` | `sizeof(char*)` |
| `m_wrapints(list, n)` | Existing `int[]` | `sizeof(int)` |
| `m_wrapcstr(s)` | C string | `sizeof(char)` |

All create handles with `MFREE_NOALLOC` — `m_free()` does **not**
free the wrapped buffer. Use `m_dub(h)` for a writable copy.

## Debug Architecture (`-DMLS_DEBUG`)

When `-DMLS_DEBUG` is defined:

| Normal call | Debug wrapper |
|---|---|
| `m_alloc(n,w,h)` | `_m_alloc(line,file,fun,n,w,h)` |
| `m_free(m)` | `_m_free(line,file,fun,m)` |
| `mls(m,i)` | `_mls(line,file,fun,m,i)` |

Debug wrappers record `__LINE__`, `__FILE__`, `__FUNCTION__` in the
`debi` structure before executing the real function. On error, an
`atexit` handler prints a post-mortem report.

**Post-mortem limitation:** The report shows the **last debug wrapper
state** (`debi`), not the actual crash site. Functions inside `mls.c`
(which defines `MLS_DEBUG_DISABLE`) call `ERR()` directly without
updating `debi`. Always read the **first error line**
`[mls error] file:line function(): message` — that is the real
crash site.

### Call Tree & Error Flow

```
User code: mls(h, i)
  → _mls(__LINE__, __FILE__, __FUNCTION__, h, i)  [debug wrapper]
    → _mlsdb_caller(...)  [saves context to debi]
    → mls(h, i)           [real function]
      → _get_list(h)      [validates handle, UAF pattern]
        → ERR(...)        [prints error, exit(1)]
          → exit_error()  [atexit handler, post-mortem]
```

## Thread-Safety Notes

- `MLS_THREAD_SAFE` is auto-defined on Unix platforms. Users can
  override with `-DMLS_THREAD_SAFE=0` or Makefile `thread_safe=0`.
- Per-handle `pthread_rwlock_t` (write lock for mutations, read
  lock for reads).
- `s_printf()` on shared handles is safe (uses temp buffer + `m_write`).
- `conststr_lookup_c` has a 64-bit pointer-truncation race (don't
  call `s_cstr`/`s_ccstr` concurrently from multiple threads).
- Ring buffer functions (`ring_put`/`ring_get`) bypass per-handle
  locks — use an external mutex for concurrent access.
- `m_foreach` iteration releases the read lock between elements.
  A concurrent writer can delete elements during iteration.

## Common Pitfalls

1. **Post-mortem shows wrong operation** — the post-mortem displays
   the last debug wrapper state, not the actual crash. Read the first
   `[mls error]` line.
2. **s_cstr length includes null** — slicing with negative indices
   can hit the null terminator. Remember: `s_cstr("Hi")` has length 3.
3. **m_slice vs s_slice** — `m_slice` does NOT null-terminate.
   Use `s_slice` for strings.
4. **UAF protection is a slot check** — `m_is_freed(h)` returns 1
   only if the slot is in the free list. Once the slot is reused,
   `m_is_freed` returns 0 for the old handle, but using it will
   trigger a UAF pattern mismatch.
5. **Trace noise** — `trace_level >= 1` in high-volume loops
   (HTTP parsing, large evictions) causes massive I/O and can
   trigger client timeouts. Use `trace_level = 0`.

## Build Configuration

### CMake (recommended for new projects)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
cmake --install build --prefix /usr/local
```

Options: `-DMLS_THREAD_SAFE=ON` (default on Unix).

### Makefile (existing project pattern)

```makefile
include path/to/rules.mk
VPATH=path/to/lib
CFLAGS+=-Ipath/to/lib

DEPS=mls.od m_tool.od m_table.od
TARGET=myprog.exed

$(TARGET): $(DEPS)
ALL: $(TARGET)
```

- Debug: `OBJ=d` (`.od`/`.exed`). Production: `OBJ=` (`.o`/`.exe`).
- `thread_safe=1` is now default (adds `-DMLS_THREAD_SAFE -lpthread`).

## Best Practices

1. **Initialize handles to 0.**
2. **Check handles** with `if (h > 0)` before use.
3. **Use `s_cstr()`** for literals (no memory management).
4. **Prefer `m_table`** over manual nested lists for structured data.
5. **Use structs for fixed data** — don't replace C structs with
   MLS lists just because fields are handles.
6. **Always call `m_init()`** at the start.
7. **Return indices, not pointers** — avoid using `mls(h,i)` return
   values across function calls.
8. **Check `m_is_freed()`** before recursing in custom free functions
   (prevents infinite loops on circular refs).
9. **Use `s_strlen(h)`** instead of `m_len(h)` for string data length
   (excludes null terminator).
10. **Use `s_printf(h, -1, ...)`** to append cleanly — overwrites
    the existing null, preventing "null terminator pollution".
11. **Own your keys** in LRU implementations — use `s_dup` copies
    instead of relying on handles owned by `m_table`.
12. **Wrap existing C memory** with `m_wrap*` to pass data through
    MLS APIs without copying.
