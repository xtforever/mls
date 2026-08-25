# MLS Library Wiki for LLMs

This wiki provides a concise guide for LLMs to use the `mls` (Master List System) library effectively.

## Core Philosophy
- **Handle-based Management:** Never use raw pointers for dynamic arrays or strings. Use `int` handles.
- **Memory Safety:** Every allocated handle MUST be freed.
- **Wrapping (No Ownership):** Handles can wrap existing C memory (pointers, arrays, strings) without taking ownership. Use `m_wrapstrings`, `m_wrapints`, `m_wrapcstr`, or `m_set_data` — MLS will never free or realloc memory owned by the caller (`MFREE_NOALLOC`).
- **Robustness:** Handles are strictly positive (`> 0`). Use `if (h > 0)` to check if a handle is allocated.
- **Width-Aware:** Arrays are created with a specific element width (e.g., `sizeof(int)`).

## Library Lifecycle
```c
m_init();       // Start of application (Initializes all systems, including conststr)
// ... your code ...
m_destruct();   // End of application (Frees all remaining handles and constant strings)
```
**Note:** `conststr_init()` and `conststr_free()` are **obsolete** — constant string interning is now automatically initialized by `m_init()` and cleaned up by `m_destruct()`.

## Essential API Reference

### 1. Basic Handle Management
- `int m_alloc(int max, int w, uint8_t free_hdl)`: Create a list.
  - `free_hdl`: `MFREE` (data), `MFREE_STR` (list of strings), `MFREE_EACH` (nested handles).
- `void m_free(int h)`: Free a handle. Safely ignores `h <= 0`.
- `void* mls(int h, int i)`: Get pointer to element at index `i`.
- `m_put(int h, const void* data)`: Append data. Returns index.
- `m_del(int h, int i)`: Delete element at index `i`.
- `m_len(int h)`: Get number of elements.
- `int m_is_freed(int h)`: Safely check if handle `h` has been freed. Returns `1` only if the handle's slot is currently in the free list. Does **not** crash on stale handles (unlike `mls()` or `m_buf()` which trigger `exit(1)` on UAF).

### 2. String Handling (`m_tool.h`)
MLS strings are also handles (width 1).
- `int s_new()`: Create empty string.
- `void s_free(int h)`: Free string.
- `int s_dup(const char *s)`: Create from C-string.
- `int s_printf(int h, int p, char *fmt, ...)`: Formatted write. `p < 0` to append. If `m=0`, allocates a new handle.
- `int s_app(int h, ...)`: Append multiple C-strings. Terminate with `NULL`.
- `int s_strlen(int h)`: Returns the string length **excluding** the trailing null terminator. Essential for protocol implementations (e.g., HTTP socket writes).
- `int s_slice(int h, int a, int b)`: Returns a new handle with the sliced string. Unlike `m_slice`, this **does** null-terminate.
- `char* v_get(int vl, const char *name, int pos)`: Get value from variable list.
- `char* se_string(int vl, const char *frm)`: String expansion/interpolation.

**Important:** Strings created by `s_cstr`, `s_strdup_c`, or `s_printf` often include a null terminator in their length. For example, `s_cstr("Hello World")` has a length of 12. When slicing with `s_slice`, remember that the last index (length - 1) is the null terminator.

### 3. Constant Strings (`s_cstr`)
Optimized for literal strings. Does not require manual freeing.
- `int s_cstr(const char *s)`: Get handle to constant string (zero-copy interning).
- `int s_cstrdup(const char *s)`: Get handle to a **copied** constant string.
- `int cs_printf(const char *fmt, ...)`: Create constant string from format.

### 4. Tables / Dictionaries (`m_table.h`)
Tables now maintain entries in **sorted order** (by key) for O(log N) lookup via binary search.
- `int m_table_create()`: Create a table.
- `void mt_seti(int h, const char *key, int64_t val)`: Set integer.
- `void mt_sets(int h, const char *key, const char *val)`: Set string (duplicates the C-string).
- `void mt_setc(int h, const char *key, const char *val)`: Set constant string (uses `s_cstr`).
- `void mt_seth(int h, const char *key, uint64_t handle, mls_table_type_t type)`: Set MLS handle.
- `uint64_t mt_get(int h, const char *key)`: Get value (cast to `int` or `int64_t`).

The `mt_` prefix API uses C-string keys by default. Ownership is tracked via the `MLS_TABLE_FREE` flag (bit 7 of `type`/`key_type`). Use `m_table_is_free(t)` and `m_table_type(t)` to check.

## Macros for Convenience
- `STR(h, i)`: Access `char*` at index `i`.
- `INT(h, i)`: Access `int` at index `i`.
- `CHAR(h, i)`: Access `char` at index `i`.

## Common Patterns

### Integer List
```c
int h = m_alloc(0, sizeof(int), MFREE);
int val = 42;
m_put(h, &val);
int retrieved = INT(h, 0);
m_free(h);
```

### String List (Auto-freeing elements)
```c
int h = m_alloc(0, sizeof(char*), MFREE_STR);
char *s = strdup("Hello");
m_put(h, &s);
m_free(h); // Automatically frees the strdup'd string
```

### Nested Lists (Recursive freeing)
```c
int parent = m_alloc(0, sizeof(int), MFREE_EACH);
int child = m_alloc(0, sizeof(int), MFREE);
m_put(parent, &child);
m_free(parent); // Recursively frees 'child'
```

### Pre-allocation for Random Access
If you need to access elements by index directly (random access) instead of appending them sequentially, use `m_setlen()` to grow the logical size of the array immediately.

```c
int h = m_alloc(0, sizeof(int), MFREE);

// Grow logical size to 100 elements. 
// Newly added elements are zero-initialized.
m_setlen(h, 100);

// Now you can safely access index 50 directly:
INT(h, 50) = 42; 

m_free(h);
```

## Error Handling & Debugging
- Define `-DMLS_DEBUG` during compilation for enhanced checks.
- `ERR("msg", ...)`: Exit with error.
- `WARN("msg", ...)`: Print warning.
- `TRACE(level, "msg", ...)`: Trace message (if `(level & trace_level) != 0`).

### Debug Architecture (`-DMLS_DEBUG`)
When `-DMLS_DEBUG` is defined:
- Macros in `mls.h` redirect calls (e.g., `m_alloc` → `_m_alloc`, `m_free` → `_m_free`, `mls` → `_mls`).
- Debug wrappers record `__LINE__`, `__FILE__`, and `__FUNCTION__` in a `debi` (debug info) structure before the real function executes.
- An `atexit` handler (`exit_error`) performs **post-mortem analysis** on error: prints the operation, context, handle metadata, status, creator info, buffer, and index in a structured report like:
  ```
  [mls error] file:line function(): message
  ```
- `__attribute__((format(printf, ...)))` on debug functions catches format string mismatches at compile time.

### Handle Validation
The `_get_list` function validates handles:
- Rejects handles with `m < 1`.
- Detects protection pattern mismatches (catch Use-After-Free when a slot is reused).

### Custom Tracing for your Libs (Bitmap Pattern)
The `trace_level` variable is used as a bitmask. This allows up to 32 independent modules (on 32-bit systems) to have their own tracing toggles. 

**Mandatory Requirement:** Each category MUST be a unique **power of two** (a single bit set) to ensure they don't overlap.

```c
// Library A - use bit 0
#define TR_LIBA_ERR   (1 << 0)
// Library B - use bit 1
#define TR_LIBB_DEBUG (1 << 1)
// App Core - use bit 2
#define TR_CORE_INFO  (1 << 2)

// Enable Lib A and Core, but keep Lib B silent:
trace_level = TR_LIBA_ERR | TR_CORE_INFO;

// In Library A code:
TRACE(TR_LIBA_ERR, "Critical error in Lib A: %d", err_code);
```
By using bitwise AND (`(level & trace_level) != 0`), the system can independently enable or disable any combination of these 32 modules.

## Creating Test Programs
When developing or testing a new module, follow this lifecycle for clean results:

1. **Initialization:** Call `m_init()` at the very start.
2. **Set Trace Level:**
   - **Normal Run:** Set `trace_level` to your specific module bit (e.g., `trace_level = TR_LIBA_ERR`).
   - **Deep Inspection:** Use `trace_level = 0xFFFFFFFF` to enable ALL trace categories across all modules.
3. **Your Logic:** Implement your tests/logic using MLS handles.
4. **Cleanup:** Call `m_destruct()` at the end. Use `-DMLS_DEBUG` to catch any leaked handles during this phase.

```c
#include "mls.h"

#define TR_MY_TEST (1 << 0)

int main() {
    m_init();
    
    // Normal testing:
    trace_level = TR_MY_TEST;
    
    // If something breaks and you need to see EVERYTHING:
    // trace_level = 0xFFFFFFFF; 

    TRACE(TR_MY_TEST, "Starting test...");
    
    int h = m_alloc(0, sizeof(int), MFREE);
    // ... test logic ...
    m_free(h);

    m_destruct();
    return 0;
}
```

## Replacing Standard Allocation
Replace `malloc`/`calloc` with `m_alloc` to gain handle safety and auto-resize capabilities.

| Standard C | MLS Equivalent |
|---|---|
| `T *p = calloc(1, sizeof(T));` | `int h = m_alloc(1, sizeof(T), MFREE);` |
| `T *p = malloc(N * sizeof(T));` | `int h = m_alloc(N, sizeof(T), MFREE);` |
| `p[i] = val;` | `m_write(h, i, &val, 1);` |
| `free(p);` | `m_free(h);` |

## Implicit Size Management
In standard C, you must always pass a pointer AND its size. In MLS, the handle knows its own length, logical size, and element width.

| Traditional C (Manual tracking) | MLS (Self-describing) |
|---|---|
| `void process(int *arr, int len)` | `void process(int h)` |
| `for(int i=0; i<len; i++)` | `for(int i=0; i<m_len(h); i++)` |
| `size_t bytes = len * sizeof(int);` | `size_t bytes = m_len(h) * m_width(h);` |

**Example: Passing Arrays**
```c
// Standard C: Error-prone (caller must provide correct length)
void standard_print(int *data, int count) {
    for(int i=0; i<count; i++) printf("%d ", data[i]);
}

// MLS: Robust (handle contains all metadata)
void mls_print(int h) {
    for(int i=0; i < m_len(h); i++) {
        printf("%d ", INT(h, i));
    }
}
```
This significantly simplifies function signatures and eliminates "length mismatch" bugs.

## Wrapping External Memory (Zero-Copy, No Ownership)

MLS can wrap existing C memory without allocating or taking ownership. The handle points directly to your buffer; MLS will never `free` or `realloc` it.

| Function | Wraps | Width |
|---|---|---|
| `m_wrapstrings(char **list, int nelem)` | Existing `char*[]` array | `sizeof(char*)` |
| `m_wrapints(int *list, int nelem)` | Existing `int[]` array | `sizeof(int)` |
| `m_wrapcstr(char *s)` | Existing null-terminated C string (width 1) | `sizeof(char)` |
| `m_set_data(int h, size_t len, size_t w, const void *data)` | Replace data on an existing `MFREE_NOALLOC` handle | user-specified |

All `m_wrap*` functions create handles with the `MFREE_NOALLOC` flag. Calling `m_free()` on them will **not** free the wrapped buffer.

**Example: Wrapping an existing C array**
```c
int my_c_array[] = {10, 20, 30, 40, 50};
int h = m_wrapints(my_c_array, 5);

// Read-only access — MLS points directly to my_c_array
for (int i = 0; i < m_len(h); i++)
    printf("%d\n", INT(h, i));

m_free(h);  // my_c_array is NOT freed — still owned by caller
```

**Example: Wrapping a C string without copying**
```c
char buf[] = "hello world";
int h = m_wrapcstr(buf);
printf("%s\n", m_str(h));  // reads directly from buf
m_free(h);  // buf is still valid
```

**Mutating wrapped data:** By default wrapped handles are read-only (resize will fail with `ERR`). To safely mutate, use `m_dub()` which creates a writable copy that owns its memory:

```c
int h = m_wrapcstr("immutable");
int h2 = m_dub(h);    // writable copy
m_strcpy(h2, "hello");
m_free(h2);
m_free(h);
```

## Type-Safe Wrapper Pattern
To make MLS easier to use for specific types and provide "interpreted-like" comfort, wrap the handle in a simple API:

```c
typedef struct { int x, y; } point_t;

// Create: replaces point_t *p = calloc(n, sizeof(point_t));
inline int point_list_new() { 
    return m_alloc(0, sizeof(point_t), MFREE); 
}

// Append: replaces manual resize + assignment
inline int point_list_add(int h, int x, int y) {
    TRACE(2, "Adding point (%d, %d) to handle %d", x, y, h);
    point_t p = {x, y};
    return m_put(h, &p);
}

// Lookup: find by attribute
int point_list_find_x(int h, int search_x) {
    TRACE(3, "Searching for x=%d in handle %d", search_x, h);
    for (int i = 0; i < m_len(h); i++) {
        point_t *p = mls(h, i);
        if (p->x == search_x) return i;
    }
    return -1;
}

// Delete: handles shifting automatically
inline void point_list_del(int h, int index) {
    TRACE(2, "Deleting point at index %d from handle %d", index, h);
    m_del(h, index);
}

// Usage Example
int my_points = point_list_new();
point_list_add(my_points, 10, 20);
int idx = point_list_find_x(my_points, 10);
if (idx != -1) point_list_del(my_points, idx);
m_free(my_points);
```

## Build Configuration

### Central Rules (`rules.mk`)
- **Debug/Production modes**: Controlled via `OBJ` variable.
  - **Debug** (default): `OBJ=d` → `.od` object files, `.exed` executables.
  - **Production** (`production=1`): `OBJ` empty → `.o` and `.exe`.
- Flags include `-DMLS_DEBUG` and `-fsanitize=address` for ASan.
- Set `thread_safe=1` before including `rules.mk` to enable threading support.

### Simple Project Makefile Pattern

For a project in `experimental/foo/` that uses the MLS library:

```makefile
production=0
thread_safe=0

include ../../rules.mk
VPATH=../../lib
CFLAGS+=-I../../lib

DEPS=mls.od m_tool.od m_table.od
TARGET=foo.exed

$(TARGET): $(DEPS)
ALL: $(TARGET)

clean:
	-${RM} -f $(TARGET) $(DEPS)
```

Key points:
- **`include ../../rules.mk`** imports the central build rules (compiler flags, suffix rules).
- **`VPATH=../../lib`** lets `make` find MLS source files in the shared library directory.
- **`CFLAGS+=-I../../lib`** adds the library headers to the include path.
- **`DEPS`** lists the required library objects using the `${OBJ}` suffix (`.od` for debug, `.o` for production).
- **`TARGET`** must also use the `${OBJ}` suffix (`.exed` or `.exe`).
- **`ALL:`** rule ensures the target is built by default.
- **`clean`** removes both the target and all dependency objects.

### Guidelines
- Every test or server must include `m_init()` and `trace_level = 1` for effective debugging.
- Use `trace_level = 0` for high-volume tests (e.g., HTTP parsing, large table evictions) to avoid I/O bottlenecks and client timeouts.

## Best Practices
1. **Initialize handles to 0.**
2. **Check handles** with `if (h > 0)` before use if allocation is conditional.
3. **Use `s_cstr()`** for literals to avoid memory management overhead.
4. **Prefer `m_table`** for structured data over manual nested lists when possible.
5. **Use Structs for Fixed Data:** Do NOT replace a C struct with an MLS list just because all its fields are handles. The overhead and maintenance of tracking indices (e.g., `handle_at_index_3`) is not worth it. Use a struct for clear, named field access.
6. **Always call `m_init()`** at the start.
7. **return index instead of ptr `mls(h,i)`** avoid use of pointers, use indices instead.
8. **Check `m_is_freed()`** before recursing in custom free functions to prevent infinite loops on circular references.
9. **Use `s_strlen(h)`** instead of `m_len(h)` for string data length (excludes null terminator) — critical for protocol implementations like HTTP socket writes.
10. **Use `s_printf(h, -1, ...)`** to append text cleanly: it overwrites the existing null terminator to prevent "null terminator pollution" in buffers.
11. **When implementing LRU**, have the list own `s_dup` copies of keys rather than relying on handles owned by other structures (like `m_table`) that may be freed during updates.
12. **Use `m_wrapstrings`/`m_wrapints`/`m_wrapcstr`** to pass existing C data through MLS APIs without copying or transferring ownership. MLS will never free the wrapped buffer.
