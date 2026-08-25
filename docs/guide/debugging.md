# Debugging with MLS

The `mls` library provides a powerful built-in debugging system that tracks every allocation, access, and deallocation. By enabling debug mode, you can catch memory leaks, double-frees, and invalid handle accesses before they reach production.

## Enabling Debug Mode

To enable debugging, define the `MLS_DEBUG` macro during compilation. This is typically done via the `-DMLS_DEBUG` compiler flag.

```bash
# Example compilation with debugging enabled
gcc -DMLS_DEBUG -I./lib your_program.c lib/mls.c -o your_program
```

When `MLS_DEBUG` is defined, the library replaces standard calls like `m_alloc` and `m_free` with their debugging counterparts (`_m_alloc`, `_m_free`), which automatically record the source file, line number, and function name of each call.

---

## Key Debugging Features

### 1. Leak Detection
When your program calls `m_destruct()`, the debug version (`_m_destruct`) automatically scans its internal tracking table. If any handles are still allocated, it prints a detailed warning for each one.

**Example output:**
```text
WARNING: List 3 still allocated. Source: my_function() in main.c:42
```

### 2. Double-Free Protection
If you attempt to free a handle that has already been deallocated, `mls` will detect it and issue a warning instead of crashing. This is significantly safer than standard C, where a double-free often leads to a heap corruption.

**Example output:**
```text
WARNING: Attempt to free already freed list 3. Called by cleanup() in utils.c:89
```

### 3. Handle Validation
Every time you access data using `mls(h, i)` or `m_put(h, d)`, the library validates the handle:
- **Initialization:** Ensures `m_init()` was called.
- **Validity:** Checks if the handle index is within the global range.
- **UAF Check:** Verifies that the 8-bit protection pattern matches the current generation for that handle index.

### 4. Real-time Tracing
By setting the global `trace_level` variable to `1`, you can see a live log of every allocation and deallocation.

```c
trace_level = 1; // Enable verbose tracing
```

**Example trace output:**
```text
[1]_m_create: NEW LIST 3 allocated by load_data:15 in database.c
[1]m_free_simple: Free List 3
```

---

## Best Practices for Debugging

1.  **Always Develop with `MLS_DEBUG`:** The runtime overhead is minimal during development, but the safety gain is massive.
2.  **Use `m_destruct()` at Exit:** Ensure your program calls `m_destruct()` before exiting to get a complete report of any leaked handles.
3.  **Check the Console:** `mls` debug warnings are printed to `stderr`. Pay close attention to these during your test runs.
4.  **Use ASAN:** For even more protection, compile with `-fsanitize=address`. `mls` is designed to be fully compatible with AddressSanitizer.

## Conclusion

The `mls` debugging system turns "silent" memory errors into "loud" warnings. By providing the exact file and line number for every leaked or double-freed list, it reduces the time spent in a debugger from hours to seconds.
