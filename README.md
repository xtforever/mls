# MLS - Interpreted Comfort for C

`mls` is a lightweight C library designed to provide the flexibility and ease of use found in interpreted languages while maintaining the performance and control of C. It features dynamic arrays (managed via handles), string manipulation, and "typeless" variables, all with built-in memory safety and optional automatic cleanup for nested structures.

## Features

- **Handle-based Memory Management:** Data structures are referenced by integer handles, reducing pointer errors and simplifying API calls.
- **Dynamic Arrays:** Automatically resizing arrays that can store any data type.
- **Nested List Support:** Built-in support for lists containing other lists, with safe recursive freeing (`MFREE_EACH`).
- **UAF Protection:** Basic Use-After-Free detection using protection patterns in handles.
- **Debug Toolset:** Integrated tracing and allocation tracking.
- **No Dependencies:** Standard C library only.

## Installation

Simply include `lib/mls.h` in your project and link against `lib/mls.c` or the compiled object file.

```bash
# Typical compilation
gcc -I./lib your_program.c lib/mls.c -o your_program
```

## Key Modules
- **`lib/`**: The core `mls` library (`mls.h`, `mls.c`).
- **`m_tool/`**: Utility tools for the `mls` ecosystem.
- **`m_table/`**: A dictionary-like data structure for storing MLS handles, accessible by name or integer, with introspection.

## Usage

### Core Concepts

#### MLS Library
The core of the library is the handle system. Instead of managing pointers to structures, you receive an `int` handle. All operations (adding, reading, deleting) are performed using this handle.

#### Handles
A handle is a 32-bit integer. The lower 24 bits represent the index in the global list of arrays, while the upper 8 bits are used for UAF (Use-After-Free) protection.

#### Data Types
`mls` arrays are "width-aware". When you allocate a list, you specify the width of its elements (e.g., `sizeof(int)`, `sizeof(char*)`). The library then handles the byte-level offsets for you.

### Why Integer Handles?

In traditional C, managing dynamic arrays often involves using pointers (`void*`) or pointers-to-pointers (`void**`). `mls` uses integer handles to solve several common architectural issues:

#### 1. Resizing without Stale Pointers
When a dynamic array is resized (e.g., via `realloc`), its memory address might change. If you have multiple pointers to that array throughout your code, they all become "stale" (invalid) after the resize. 
With **handles**, the actual memory address is managed internally in a central table. Your handle (the `int`) never changes, even if the underlying memory moves.

#### 2. Simpler Function Signatures
In standard C, if a function needs to resize a buffer passed by the caller, it must accept a pointer-to-pointer (`void**`) and often a pointer to the length. This makes code verbose and error-prone.

**Traditional C approach:**
```c
// Caller must manage both the pointer and the length
size_t len = 128;
char *buf = malloc(len);
ssize_t bytes = read_line(&buf, &len, fp); // Must pass address of pointer
// ... later ...
free(buf);
```

**MLS approach:**
```c
// Caller only manages a single integer handle
int h = m_alloc(128, 1, MFREE);
int bytes = m_read_line(h, fp); // Handle stays the same even if resized
// ... later ...
m_free(h);
```

By using handles, functions become much simpler to write and maintain because they don't need to "reach back" into the caller's memory to update pointers.

#### 3. Robustness over Strict Typing
Creating a universal library in C usually requires extensive use of `void*`, which already bypasses compile-time type safety. By choosing `int` handles, `mls` trades this manual type management for significant runtime robustness:

- **No Loss of Safety:** Since generic C libraries already rely on `void*` for "typeless" behavior, using an `int` handle doesn't lose any safety that wasn't already being managed manually by the programmer.
- **Superior Validation:** Unlike a raw pointer, which is just an address, an `int` handle can be rigorously validated. Every time you call an `mls` function, the library checks if the handle is initialized, if it has been freed, and if its internal protection bits match. This prevents the most common C errors like double-frees or null-pointer dereferences.
- **Avoiding Workarounds:** Strict typing for generic structures in C often leads to complex workarounds like "macro hell," nested unions, or repetitive boilerplate. Integer handles provide a clean, universal identifier that works identically across all data structures.

#### 4. Minimal Runtime Overhead
While handle-to-pointer resolution involves a small overhead compared to raw pointer access, this trade-off is negligible in most real-world scenarios:

- **Safety at a Discount:** A standard `if (ptr == NULL)` check only verifies the address isn't zero. The `mls()` function performs a handle lookup, checks for initialization, and validates the UAF protection pattern in just a few extra CPU cycles.
- **Efficient Lookup:** The internal handle-to-pointer mapping is implemented as a simple array index lookup. For performance-critical loops, you can always resolve the handle once and work with the resulting raw pointer directly.
- **Value-Added Security:** The minimal cost of handle resolution buys you features that would be much more expensive to implement manually, such as global Use-After-Free detection and cross-module memory tracking.

#### 5. Positive Handles (> 0)
In `mls`, all valid handles are strictly positive integers (`h > 0`). This convention provides several benefits:

- **Simple Initialization Checks:** You can initialize your handle variables to `0` and then use a simple `if (h > 0)` check to verify if a list has been allocated.
- **Error Ambiguity Removal:** Unlike pointers where only `NULL` (0) is traditionally invalid, `mls` explicitly treats all values `<= 0` as unallocated or invalid handles. 
- **Robust API:** Functions like `m_free(h)` gracefully ignore handles `<= 0`, preventing crashes if an uninitialized handle is passed. Core access functions like `mls(h, i)` strictly enforce the `h > 0` rule, ensuring that any attempt to access an invalid handle is caught immediately during development. (Note: While handles themselves must be positive, the data stored *within* an array—such as in a list of integers—can safely contain negative values.)

> **Fun Side Note:** In a 64-bit world, a standard pointer takes up 8 bytes of memory. An `mls` handle is a lean 4-byte integer. If you find yourself storing the same reference multiple times (like in a complex graph), using `mls` is essentially putting your data structure on a half-calorie diet. Your cache will thank you!

### Simplifying Complex Data Structures

The power of `mls` becomes even more apparent when building complex, nested data structures like trees, graphs, or JSON-like objects. By using integer handles for every child node instead of pointers, you can build deeply nested structures that are both robust and easy to manage.

#### Handle-based Memory Management
Building a complex structure involves these core operations:
- **`m_alloc(max, w, h)`**: Creates a new list node.
- **`m_free(m)`**: Safely deallocates a node and (if `MFREE_EACH` is used) its entire subtree.
- **`mls(m, i)`**: Returns a `void*` to the element at index `i` in handle `m`.

#### Data Access Macros
While `mls()` returns a generic `void*`, `mls` provides several macros for type-safe and concise data access:

| Macro | Type | Description |
|---|---|---|
| `STR(m, i)` | `char*` | Access a string pointer at index `i`. |
| `INT(m, i)` | `int` | Access an integer value at index `i`. |
| `CHAR(m, i)` | `char` | Access a single character at index `i`. |

#### Structure Examples

- **Lists:** Simply a handle where elements are data values (e.g., `sizeof(int)`).
- **Trees:** A handle where each element is another handle (e.g., `sizeof(int)` with `MFREE_EACH`).
- **Graphs:** Handles where elements represent edges (pointers to other handles). Because `m_free` uses the circular-reference marker (255), cycles in graphs are handled safely during recursion.

#### Example: Building a Tree
Building a tree node with multiple children is straightforward and avoids complex pointer arithmetic:

```c
// Create a node that will contain child handles
int root = m_alloc(0, sizeof(int), MFREE_EACH);

// Create child nodes
int child1 = m_alloc(0, sizeof(int), MFREE_EACH);
int child2 = m_alloc(0, sizeof(int), MFREE_EACH);

// Add children to the root
m_put(root, &child1);
m_put(root, &child2);

// Recursive Cleanup:
// Because root was created with MFREE_EACH, a single call 
// to m_free(root) will recursively free all children.
m_free(root);
```

#### Measurable Benefits
- **Unified API:** Use the same `m_put`, `mls`, and `m_free` functions regardless of structural complexity.
- **Robust Code:** No more dangling pointers or complex pointer arithmetic.
- **Reduced Error Rate:** Automated recursive freeing eliminates manual tracking of every child node, significantly reducing memory leak potential.

### API Reference

#### `int m_alloc(int max, int w, uint8_t free_hdl)`
Allocates a new dynamic array.
- `max`: Initial capacity (number of elements).
- `w`: Width of each element in bytes.
- `free_hdl`: A pre-registered free handler (e.g., `MFREE`, `MFREE_STR`, `MFREE_EACH`).

#### `int m_create(int max, int w)`
**Deprecated.** Use `m_alloc` instead. Equivalent to `m_alloc(max, w, MFREE)`.

#### `void* mls(int m, int i)`
Returns a pointer to the element at index `i` in handle `m`.
- **Note:** Always cast the result to your data type (e.g., `*(int*)mls(h, 0)`).

#### `int m_put(int m, const void* data)`
Appends `data` to the end of the array `m`. The library automatically resizes the buffer if needed.
- Returns the index of the newly added element.

#### `int m_write(int m, int pos, const void* data, int n)`
A safer, handle-aware alternative to `memcpy`. Copies `n` elements from `data` into list `m` starting at `pos`.
- **Note:** It does not resize the buffer automatically. If you write beyond the current length, you should call `m_setlen(m, pos + n)` to keep the list's size in sync.

#### `int m_free(int m)`
Frees the memory associated with handle `m`. If a specialized `free_hdl` was used during allocation (like `MFREE_STR`), it will also free the elements (e.g., the strings inside the list) before freeing the list itself. It handles circular references safely.

## Examples

### Basic Integer List
```c
#include "mls.h"

int main() {
    m_init();
    
    // Create a list for integers
    int h = m_alloc(10, sizeof(int), MFREE);
    
    for(int i = 0; i < 5; i++) {
        m_put(h, &i);
    }
    
    printf("Value at index 2: %d\n", *(int*)mls(h, 2));
    
    m_free(h);
    m_destruct();
    return 0;
}
```

### String List (Auto-freeing)
```c
int h = m_alloc(5, sizeof(char*), MFREE_STR);
char *s = strdup("Hello World");
m_put(h, &s);
// m_free(h) will now automatically call free() on the string
m_free(h);
```

## Building

The project uses a hierarchical Makefile system.

```bash
# Debug build (with MLS_DEBUG and trace support)
make

# Production build (optimized)
make production=1
```

## Testing

To run the test suite:

```bash
cd tests
make
```

This will execute various tests including `test_autofree`, `test_ssplit`, and `test_nested_lists`.

## Contributing

Contributions are welcome! To contribute:
1. **Report Bugs:** Use the issue tracker to report bugs or request features.
2. **Submit PRs:** Ensure your code follows existing patterns and includes tests for any new functionality.
3. **Run Tests:** Always run `make test` before submitting changes.

## License

This project is licensed under the **MIT License**. See the `LICENSE` file for details.
