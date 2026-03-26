# Advanced: Global Constant Strings with `s_cstr`

For more advanced use cases, `mls` provides a special interning system for **Global Constant Strings**. 

## What is `s_cstr`?
`s_cstr(const char *s)` creates a handle to a string that is intended to exist throughout the **entire lifetime of your program**. These strings are interned, meaning if you call `s_cstr("FOO")` multiple times, you will always get the same handle.

## Key Rules:
1. **Never `m_free(s)` a constant handle.** Individual constant handles should not be freed.
2. **Read-Only:** These strings are intended for internal constants, keywords, or dictionary keys—not for general user input or modifiable buffers.
3. **Global Cleanup:** All memory used by constant strings is released at once using `conststr_free()`.

## Example: Internal Constants
```c
// At program start
conststr_init();

// Use internally as constants
int KEY_ID = s_cstr("ID");
int KEY_NAME = s_cstr("NAME");

// ... do some work ...

// At program exit
conststr_free();
```

## Why use this?
- **Speed:** Comparing handles is much faster than `strcmp`.
- **Memory Efficiency:** Identical strings are only stored once in memory.
- **Cognitive Ease:** No need to manage the lifecycle of these handles in your main logic.

---

[Back to Home](README.md)
