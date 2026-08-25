# Wrappers: Zero-Copy Integration with C Arrays

The MLS library supports wrapping existing C arrays without copying data. This enables zero-copy integration with external data sources while still benefiting from MLS handle management.

---

## 1. The Problem: Data Copying Overhead

When working with external C libraries or passing data between systems, copying can be expensive:
- Large datasets become slower to process
- Memory usage doubles during the copy
- Performance-critical applications suffer

The MLS wrapper system solves this by letting you wrap existing C arrays directly.

---

## 2. The Solution: MFREE_NOALLOC Flag

The `MFREE_NOALLOC` flag (value 128) marks an MLS list as **immutable**:
- **No Allocation:** MLS never reallocates the underlying buffer
- **No Free:** MLS will refuse to free the memory
- **Data Ownership:** You retain full control of the memory lifecycle

This is ideal for wrapping external data while still using MLS handle access patterns.

### Flag Definition
```c
MFREE_NOALLOC = 128,    /* BITMAP: runtime protection against free/realloc */
```

---

## 3. Wrapper API Functions

### m_wrapstrings - Wrap a String Array
```c
int m_wrapstrings( char **list, int nelem );
```
Wraps an existing `char**` array (e.g., argv) into an MLS string list without copying.

**Example:**
```c
char *args[] = { "program", "-v", "--help", NULL };
int h = m_wrapstrings(args, 4);

// Access without copying:
char *first = STR(h, 0);  // Returns "program"
```

### m_wrapints - Wrap an Integer Array
```c
int m_wrapints( int *list, int nelem );
```
Wraps an existing `int*` array into an MLS int list without copying.

**Example:**
```c
int values[] = { 10, 20, 30, 40, 50 };
int h = m_wrapints(values, 5);

// Access without copying:
int val = INT(h, 2);  // Returns 30
```

### m_wrapcstr - Wrap a C String
```c
int m_wrapcstr( char *s );
```
Wraps an existing C string into an MLS list.

**Example:**
```c
char *ext_data = "wrapped string";
int h = m_wrapcstr(ext_data);
```

### s_ccstr - Constant C String Helper
```c
int s_ccstr(const char *s);
```
Creates a constant string handle from a constant C string (literals). This does not copy the string data.

### s_cstrdup - Constant String Helper
```c
int s_cstrdup(const char *s);
```
Creates a constant string handle from a C string using constant string lookup. This copies the string data into internal storage.

---

## 4. Behavior Differences

| Operation | Normal List | MFREE_NOALLOC List |
|-----------|-------------|-------------------|
| m_free()  | Frees memory | Refuses, returns error |
| m_realloc() | Resizes buffer | Refuses, returns error |
| m_set()   | Modifies element | Allowed (if not MFREE_NODESTRUCT) |
| mls()     | Reads element | Allowed |

### Protected Operations
When you attempt to free or reallocate an MFREE_NOALLOC list:
```c
if (hfree & MFREE_NOALLOC) {
    // Refuses to free
    return;
}
```

---

## 5. Use Cases

### CLI Argument Processing
```c
int main(int argc, char *argv[]) {
    // Wrap argc/argv directly without copying
    int args = m_wrapstrings(argv, argc);
    
    for (int i = 0; i < argc; i++) {
        printf("Arg %d: %s\n", i, STR(args, i));
    }
    // No free needed - original argv remains valid
}
```

### Interfacing with C Libraries
```c
void process_external_data(int *data, int count) {
    int wrapped = m_wrapints(data, count);
    
    // Use MLS functions for iteration
    int i; int *val;
    m_foreach(wrapped, i, val) {
        printf("Value: %d\n", *val);
    }
    // Original data array remains under caller's control
}
```

### Zero-Copy IPC
```c
// Receiving data from network/library
int received = m_wrapints(received_buffer, buffer_len);

// Process with MLS
for (int i=0; i < m_len(received); i++) {
    int val = INT(received, i);
    // ...
}

// No copy performed - direct buffer access
```

---

## 6. Combining with Other Flags

MFREE_NOALLOC can be combined with other protection flags:

```c
// Immutable AND non-destructible (like empty string)
int h = new_list(data, len, len, 1, MFREE_NOALLOC | MFREE_NODESTRUCT);
```

---

## 7. Best Practices

1. **Track Ownership:** Since MLS won't free NOALLOC data, you must manage lifetime yourself
2. **Validate Before Free:** Check for MFREE_NOALLOC before attempting operations
3. **Document Contracts:** Clearly document wrapper data ownership in your API
4. **Avoid Mixed Ownership:** Don't mix NOALLOC and regular data in the same application flow without clear boundaries

---

## 8. Example: Full Integration

```c
#include "mls.h"

int main() {
    m_init();
    
    // External data from somewhere (file, network, library)
    int scores[] = { 95, 87, 92, 78, 88 };
    int num_scores = 5;
    
    // Wrap without copying
    int scores_h = m_wrapints(scores, num_scores);
    
    // Use MLS capabilities
    int sum = 0;
    for (int i=0; i < m_len(scores_h); i++) {
        sum += INT(scores_h, i);
    }
    int avg = sum / m_len(scores_h);
    printf("Average: %d\n", avg);
    
    // Attempting free will fail (protected)
    // m_free(scores_h);  // Returns error, data unchanged
    
    // Original array still valid here
    printf("Original: %d\n", scores[0]);
    
    return 0;
}
```

---

## 9. Summary

| Feature | Benefit |
|---------|---------|
| m_wrapstrings() | Zero-copy string array wrapping |
| m_wrapints() | Zero-copy integer array wrapping |
| m_wrapcstr() | Zero-copy C string wrapping |
| s_ccstr() | Zero-copy constant string lookup |
| s_cstrdup() | Constant string lookup with copy |
| MFREE_NOALLOC | Prevent accidental free/realloc |
| MLS handle access | Use all MLS functions on wrapped data |

The wrapper system provides the best of both worlds: full MLS functionality with zero-copy performance.