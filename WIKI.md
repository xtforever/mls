# MLS Library Wiki for LLMs

This wiki provides a concise guide for LLMs to use the `mls` (Master List System) library effectively.

## Core Philosophy
- **Handle-based Management:** Never use raw pointers for dynamic arrays or strings. Use `int` handles.
- **Memory Safety:** Every allocated handle MUST be freed.
- **Robustness:** Handles are strictly positive (`> 0`). Use `if (h > 0)` to check if a handle is allocated.
- **Width-Aware:** Arrays are created with a specific element width (e.g., `sizeof(int)`).

## Library Lifecycle
```c
m_init();       // Start of application (Initializes all systems)
conststr_init();// Initialize constant string system
// ... your code ...
m_destruct();   // End of application (Frees all remaining handles and constant strings)
```

## Essential API Reference

### 1. Basic Handle Management
- `int m_alloc(int max, int w, uint8_t free_hdl)`: Create a list.
  - `free_hdl`: `MFREE` (data), `MFREE_STR` (list of strings), `MFREE_EACH` (nested handles).
- `void m_free(int h)`: Free a handle. Safely ignores `h <= 0`.
- `void* mls(int h, int i)`: Get pointer to element at index `i`.
- `m_put(int h, const void* data)`: Append data. Returns index.
- `m_del(int h, int i)`: Delete element at index `i`.
- `m_len(int h)`: Get number of elements.

### 2. String Handling (`m_tool.h`)
MLS strings are also handles (width 1).
- `int s_new()`: Create empty string.
- `void s_free(int h)`: Free string.
- `int s_dup(const char *s)`: Create from C-string.
- `int s_printf(int h, int p, char *fmt, ...)`: Formatted write. `p < 0` to append.
- `int s_app(int h, ...)`: Append multiple C-strings. Terminate with `NULL`.
- `char* v_get(int vl, const char *name, int pos)`: Get value from variable list.
- `char* se_string(int vl, const char *frm)`: String expansion/interpolation.

### 3. Constant Strings (`s_cstr`)
Optimized for literal strings. Does not require manual freeing.
- `int s_cstr(const char *s)`: Get handle to constant string.
- `int cs_printf(const char *fmt, ...)`: Create constant string from format.

### 4. Tables / Dictionaries (`m_table.h`)
- `int m_table_create()`: Create a table.
- `void mt_seti(int h, const char *key, int64_t val)`: Set integer.
- `void mt_sets(int h, const char *key, const char *val)`: Set string (copies).
- `void mt_seth(int h, const char *key, uint64_t handle, mls_table_type_t type)`: Set MLS handle.
- `uint64_t mt_get(int h, const char *key)`: Get value (cast to `int` or `int64_t`).

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

## Best Practices
1. **Initialize handles to 0.**
2. **Check handles** with `if (h > 0)` before use if allocation is conditional.
3. **Use `s_cstr()`** for literals to avoid memory management overhead.
4. **Prefer `m_table`** for structured data over manual nested lists when possible.
5. **Use Structs for Fixed Data:** Do NOT replace a C struct with an MLS list just because all its fields are handles. The overhead and maintenance of tracking indices (e.g., `handle_at_index_3`) is not worth it. Use a struct for clear, named field access.
6. **Always call `m_init()`** at the start.
7. **return index instead of ptr `mls(h,i)`** avoid use of pointers, use indices instead.
