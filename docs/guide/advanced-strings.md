# Advanced: Global Constant Strings and Interning

The MLS library provides a specialized interning system for **Global Constant Strings**. This system ensures that identical strings share the same memory and handle, improving performance and reducing memory usage.

## 1. What is Constant String Interning?

Interning is the process of storing only one copy of each distinct string value. In MLS, when you "intern" a string, you receive a handle. Multiple calls with the same string content will return the same handle.

## 2. Key Functions

### `s_ccstr` - Intern Constant C Strings
```c
int s_ccstr(const char *s);
```
Use this for **string literals** or strings that are guaranteed to exist for the entire lifetime of the program. 
- **Zero-Copy:** It wraps the provided pointer directly.
- **Fast:** Ideal for internal keys, keywords, or fixed identifiers.

### `s_cstrdup` - Intern Dynamic Strings
```c
int s_cstrdup(const char *s);
```
Use this when you want to intern a string that might be temporary (e.g., read from a file or user input).
- **Copies Data:** It duplicates the string into internal storage managed by MLS.
- **Persistent:** The handle remains valid until `m_destruct()` is called.

## 3. Lifecycle Management

The constant string system is automatically managed by the main MLS lifecycle:
- **Initialization:** Handled automatically by `m_init()`. You no longer need to call `conststr_init()`.
- **Cleanup:** All interned strings (including those duplicated by `s_cstrdup`) are released when you call `m_destruct()`.

## 4. Key Rules

1. **Immutable:** Interned strings are read-only. Do not attempt to modify the buffer returned by `m_buf()` for these handles.
2. **Do Not Free Individually:** Constant handles are managed by the global interning map. Calling `m_free()` on them will be refused or have no effect (protected by `MFREE_NOALLOC` and `MFREE_NODESTRUCT`).
3. **Handle Comparison:** Since strings are interned, you can compare two handles using `==` instead of `strcmp()` to check if the strings are identical.

## 5. Example: Internal Keywords

```c
// MLS initialization handles everything
m_init();

// Interning literals (Zero-Copy)
int KEY_ID = s_ccstr("ID");
int KEY_NAME = s_ccstr("NAME");

// Interning dynamic data (Copies)
char buffer[100];
snprintf(buffer, sizeof(buffer), "USER_%d", 123);
int USER_KEY = s_cstrdup(buffer);

// Handle comparison is fast!
if (some_handle == KEY_ID) {
    // ...
}

// Cleanup everything at once
m_destruct();
```

## 6. Wrapper Functions

For zero-copy integration with existing C arrays, see the [Wrappers Documentation](wrappers.md).

---

[Back to Home](README.md)
