# C Programming Rules for MLS Project

This document outlines the mandatory programming standards and best practices derived from technical findings and bug fixes.

## 1. Memory and Handle Management
- **Never rely on handle 0:** A handle of `0` is invalid for most operations. Always check `if (h <= 0)` before calling library functions like `m_len`, `m_buf`, or `mls`, as they will trigger `exit(1)` in debug mode.
- **Validate handles with `m_is_freed()`:** Before accessing an untrusted or potentially stale handle, check its status with `m_is_freed(h)`. This avoids library-level termination for Use-After-Free (UAF) or invalid index errors.
- **Prefer `mls(h, index)` over direct memory access:** Use the `mls` macro to access elements. This ensures proper bounds checking and enables the debug tracker to record access context.
- Use `MFREE_EACH` for nested structures:** When creating a list or table that stores other MLS handles, use the `MFREE_EACH` flag during allocation to ensure recursive cleanup.
- **Store pointers safely in `m_table`:** Use `mt_setp(table, key, ptr)` and `mt_getp(table, key)` to store and retrieve raw 64-bit pointers. The `m_table` value field is now `uint64_t`, making direct storage safe without extra allocations or truncation.
- **Never cast 64-bit pointers to 32-bit `int`:** Always ensure pointer types are preserved or stored in 64-bit fields like the updated `m_table` value.


## 2. String Handling and Robustness
- **Never assume user-supplied buffers are null-terminated:** When working with raw buffers or string slices, always use counted loops based on `s_strlen(h)` or `m_len(h)`.
- **Returned strings MUST be null-terminated:** Any function returning a handle meant to be used as a string must ensure it has at least one trailing zero. Use the standard safeguard:
  `if (m_len(ret) == 0 || CHAR(ret, m_len(ret) - 1) != 0) m_putc(ret, 0);`
- **Use `s_slice` for string extraction:** Prefer `s_slice` over `m_slice` for string data, as it automatically applies the null-termination safeguard.
- **Avoid standard `str*` functions on raw buffers:** Prefer robust alternatives like `memmem`, `memchr`, or `memcmp` combined with `s_strlen` to avoid reading past the end of non-terminated slices.
- **Use `s_printf(h, -1, ...)` for clean appending:** When building complex strings, use the `-1` offset to overwrite the existing null terminator and prevent "null terminator pollution."

## 3. Code Style and Complexity
- **Strict Nesting Limits:** Never use nesting (loops, conditionals, or data structures) with more than **5 levels**. If complexity increases, refactor into smaller, focused functions.
- **Consistent Initialization:** Every standalone program or test must call `m_init()` and `conststr_init()` at startup, and their corresponding free functions at exit.
- **Doxygen Documentation:** All public functions must have Doxygen-style comments specifying `@param`, `@return`, and overall behavior.

## 4. Build and Debugging
- **Develop in Debug Mode:** Always compile with `-DMLS_DEBUG` during development. This enables the caller tracker, UAF protection patterns, and post-mortem analysis.
- **Manage Trace Levels:** Use `trace_level = 1` for general debugging. For high-volume operations (e.g., fuzzing or large evictions), set `trace_level = 0` to prevent I/O bottlenecks and timeouts.
- **Makefile Consistency:** Always use the `${OBJ}` variable for target extensions (e.g., `program.exe${OBJ}`) to maintain compatibility between debug (`.exed`) and production (`.exe`) builds.
