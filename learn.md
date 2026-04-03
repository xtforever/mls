# Learning: Technical Findings & Continuity

## Session: March 25, 2026 - Documentation and Advanced Testing

### `m_slice` and `s_slice` Behavior
- `m_slice(dest, offs, m, a, b)` with `m=0` should return `dest` (after setting length to `offs` if `dest > 0`) or `0`.
- String lists created by `s_cstr`, `s_strdup_c`, or `s_printf` often include a null terminator in their length. For example, `s_cstr("Hello World")` has a length of 12. This affects slicing indices, especially negative ones.
- When slicing strings with `s_slice`, remember that the last index (e.g., length - 1) is the null terminator.

### `s_cstr` Initialization
- Using `s_cstr` requires calling `conststr_init()` at the beginning and `conststr_free()` at the end of the program/test. Without this, the library will report "Not initialized".

### Handle Validation and `m_is_freed`
- `m_is_freed(h)` returns `1` only if the handle's slot index is currently in the free list `FR`.
- Once a slot is reused for a new allocation, `m_is_freed` for the old handle will return `0` because it's no longer in the free list, but subsequent use of the old handle will trigger a UAF (Use-After-Free) protection pattern mismatch in `_get_list`.
- Handle validation in `_get_list` catches invalid handles (m < 1) or protection pattern mismatches.

### Memory Management & Recursive Freeing
- `MFREE_EACH` allows for recursive freeing of nested structures.
- Circular references in custom free functions (like `m_reg_freefn`) should be handled by checking `m_is_freed()` before recursing to prevent infinite loops.

## Session: March 28, 2026 - Debugging & Error Handling Improvements

### MLS Debug Architecture
- **Debug Wrappers**: When `-DMLS_DEBUG` is defined, macros in `mls.h` redirect standard calls (e.g., `m_alloc`, `m_free`, `mls`) to their debug counterparts (`_m_alloc`, `_m_free`, `_mls`).
- **Caller Tracking**: These debug wrappers use `__LINE__`, `__FILE__`, and `__FUNCTION__` to record exactly where an operation was called from. This information is stored in the `debi` (debug info) structure before the actual library function is executed.
- **Post-Mortem Analysis**: An `atexit` handler (`exit_error`) is registered. If the program terminates due to a library error (via `ERR` -> `deb_err` -> `exit(1)`), the handler performs a "post-mortem" analysis using the last recorded caller info and the state of the handles.

### Call Tree & Error Flow
1. User calls a macro like `mls(h, i)`.
2. In debug mode, this expands to `_mls(__LINE__, ..., h, i)`.
3. `_mls` calls `_mlsdb_caller(...)` to save context, then calls the real `mls(h, i)`.
4. `mls(h, i)` calls `_get_list(h)`.
5. If `h` is invalid (e.g., freed, out of bounds, or UAF mismatch), `_get_list` calls `ERR(...)`.
6. `ERR` (macro for `deb_err`) prints the immediate error and calls `exit(1)`.
7. The `exit_error` handler runs, printing the detailed post-mortem analysis (who called the failing function, metadata of the handle, etc.).

### Makefile and Build Configuration
- **Debug Flag**: The project uses `-DMLS_DEBUG` to enable the debug version of the library. This must be consistent across the library and the application code.
- **Rules.mk**: A central `rules.mk` file is used to maintain consistency in compiler flags (`CFLAGS`) and library dependencies across different test directories.
- **Incremental Testing**: The `tests/makefile` uses a pattern for `.exe` targets and a `run` target to automate the execution of all tests, ensuring that changes to the core library don't introduce regressions.

### Improved Debug Formatting
- **Standardized Output**: Error messages now use a more professional `[mls error] file:line function(): message` format.
- **Structured Analysis**: The post-mortem report is now clearly labeled and structured (Operation, Context, Handle, Status, Created by, Metadata, Buffer, Index), making it much easier for developers to pinpoint the root cause of memory errors.
- **Format Attributes**: Using `__attribute__((format(printf, ...)))` on debug functions ensures the compiler can catch format string mismatches during the build process.

## Session: March 28, 2026 (Continued) - HDF Parser & Configurable Server

### HDF (Human Data Forms) Parser
- **Implementation**: The parser is implemented in `m_hdf.c` using `mls` handles. Each node is represented as a 2-element list: `[type, value/children]`.
- **Comment Filtering**: The parser automatically identifies and skips `rem` forms. This is handled during the list parsing phase by checking the first element of any newly parsed list.
- **Raw Strings**: Support for `[[...]]` and `[=[...]=]` was implemented by matching opening and closing delimiters exactly, allowing for nested brackets in configuration files.
- **Literal Support**: Keywords `true`, `false`, and `null` are recognized and converted to their respective HDF types. Numbers are also automatically detected.

### Configurable HTTP Server
- **Architecture**: A new `m_http_server.c` API allows loading entire server configurations from HDF.
- **Route Mapping**: Routing is managed via `m_table`, where paths are keys and `mls` handles (containing type and target) are values.
- **Memory Management**: By using `MFREE_EACH` for route handles, the entire route table (including target strings) is cleaned up automatically when `m_table_free` is called.
- **Flexibility**: The server supports various route types: `file` (serves local files), `json` (serves static JSON), `text` (serves plain text), and `echo` (echoes the request body).

### Library Robustness
- **Recursive Freeing**: `free_list_wrap` was improved to check `m_is_freed()` and handle `NULL` handles before recursing. This prevents crashes when nested structures contain handles that have already been manually freed.

## Session: March 28, 2026 (Continued) - Flask-like Web Framework

### Flask-inspired C API (`m_flask`)
- **Philosophy**: Bring the developer experience of Python's Flask to C by using HDF for routing and `mls` handles for request/response lifecycle.
- **HDF Bindings**: Routes are defined in HDF using the `(bind (path "...") (call "function_name"))` syntax. This allows decoupling the network topology from the implementation.
- **Handler Registry**: A global `m_table` stores mappings from HDF function names to actual C function pointers (`flask_handler_t`).
- **Context Objects**: Request and Response objects are managed as `mls` handles containing structured data (metadata, headers, body).
- **Convenience Functions**: 
    - `flask_arg()`: Automatically parses query strings into an `m_table` for easy key-based lookup.
    - `flask_printf()`: Allows formatted writing directly into the response body buffer.
    - `flask_json()`: Handles status codes, Content-Type headers, and body population in one call.
- **Lifecycle**: The framework automates the setup of the `http_parser_t`, handles the I/O loop, performs route matching via HDF traversal, and ensures all temporary buffers and tables are cleaned up after the handler returns.

### String Manipulation with `s_strlen` and `s_printf`

- **`s_strlen(m)`**: Returns the length of the string stored in MLS handle `m`, excluding the trailing null terminator if one is present. This is essential for protocol implementations (like HTTP) where you need to know the exact data size to send over a socket without including the C-style terminator.
- **`s_printf(m, p, format, ...)`**: Formats a string and places it at position `p` in handle `m`.
  - **New String**: If `m=0`, a new handle is allocated.
  - **Positional Write**: If `p >= 0`, the formatted string is written at that index.
  - **Appending**: If `p < 0`, the string is appended to the end of the handle.
- **Use Case: Clean Appending**: A common pattern for building complex strings (like HTTP response headers) is to use `s_printf(h, -1, format, ...)`. This internally uses `s_strlen` to find the last non-null character and overwrites the existing null terminator with the new formatted content. This prevents "null terminator pollution" where a buffer becomes a sequence of null-terminated strings instead of one continuous string, which would otherwise break network protocols or standard string functions.

## Session: March 29, 2026 - M_TABLE Improvements

### Encoded Memory Management in `m_table`
- **Encoded Type Flags**: Instead of separate bitfields, `m_table` now uses the highest bit (`0x80`) of the `type` and `key_type` fields in `m_table_entry_t` to signal ownership.
- **`MLS_TABLE_FREE`**: This flag is bit-ORed into the `mls_table_type_t` enum values for types that the table should automatically free (e.g., dynamic strings, lists, nested tables).
- **Helper Macros**: New macros `m_table_is_free(t)` and `m_table_type(t)` provide a clean way to check for the ownership flag and extract the base type, respectively. `m_table_is_str_key(t)` identifies both dynamic and constant string key types.
- **Binary Search Lookup**: To improve performance for large tables, `m_table` now maintains its entries in a sorted order (by key). Lookups and insertions use `m_bsearch` and `m_binsert` respectively, reducing lookup complexity from $O(N)$ to $O(\log N)$.
- **Robust Key Ownership**: The `m_table_set_entry` function now correctly handles key handle replacement. When an entry is replaced, it intelligently decides whether to keep the old key handle or use the new one based on ownership (`MLS_TABLE_FREE` flag), avoiding memory leaks and use-after-free errors.
- **Unified Logic**: The `m_table_free_handler` and `m_table_set_entry` functions now use these macros, making the memory management logic more readable and decoupled from specific enum values.
- **Efficiency**: Using the 8th bit of the existing `unsigned char` type fields avoids increasing the memory footprint of the `m_table_entry_t` structure.

### New Simplified API (`mt_` prefix)
- To reduce verbosity and improve developer experience, a new set of functions was added for the most common table operations:
    - `mt_seti(t, key, int_val)`: Set an integer value.
    - `mt_sets(t, key, str_val)`: Set a dynamic string (duplicates the C-string).
    - `mt_setc(t, key, str_val)`: Set a constant string (uses `s_cstr`).
    - `mt_seth(t, key, handle, type)`: Set a generic MLS handle (list, table, etc.).
    - `mt_get(t, key)`: Get a value (raw int or handle) by C-string key.
- These functions use C-string keys by default, as they are the most common use case in the project.

## Session: March 31, 2026 - QA Session Findings
- **Alignment:** 'lst_t' in 'mls.h' was forced to 1-byte alignment, causing UBSan warnings on 64-bit systems. Removed 'aligned(1)' and added 'aligned(8)' to 'char d[0]' to ensure payload alignment.
- **Null-Termination:** 
  - 'm_slice' does not null-terminate. Replaced with 's_slice' in 'lib/m_http.c' for HTTP parsing.
  - 'm_hdf.c' parser ('parse_quoted_string' and 'parse_raw_string') was not null-terminating strings, causing 'strlen' crashes in ASan builds.
## Session: April 2, 2026 - Documentation & Server Testing

### Makefile Build Constraints
- **Debug Extensions**: The build system in `rules.mk` uses a conditional `OBJ` variable to distinguish between debug and production builds. 
  - In **Debug mode** (default), `OBJ=d`, resulting in `.od` for object files and `.exed` for executables.
  - In **Production mode** (`production=1`), `OBJ` is empty, resulting in `.o` and `.exe`.
- **Consistency**: All targets in `tests/makefile` must use the `${OBJ}` suffix (e.g., `memcached_server.exe${OBJ}`) to ensure they work in both modes.
- **Library Dependencies**: Executables in the `tests/` directory rely on `DEPS` defined in the makefile, which points to the library object files in `../lib/`. These must also use the correct `${OBJ}` suffix.

### Testing and Debugging
- **Mandatory Initialization**: Every test or server must include `m_init()`, `conststr_init()`, and set `trace_level = 1` for effective debugging and handle tracking.
- **ASAN Integration**: The debug flags include `-fsanitize=address`, which is critical for catching the heap-buffer-overflows encountered during `m_table` key comparisons and Base64 decoding.

### LRU & Memory Management
- **Trace Noise Bottleneck**: High `trace_level` settings (e.g., 1 or higher) generate massive amounts of I/O during repetitive operations like HTTP parsing or large table evictions. This can slow the application enough to trigger client-side timeouts. Always use `trace_level = 0` for high-volume tests.
- **Handle Ownership in Lists**: When implementing LRU with `mls` handles, it's safer to have the list own its own `s_dup` copy of keys. Relying on handles owned by other structures (like an `m_table`) is dangerous because they may be freed during updates/overwrites.
- **Robust Removal**: `m_table_remove_by_str` should ideally support searching by content if searching by handle fails, especially if multiple duplicates of the same string content exist as different handles.
- **UAF Protection**: The `mls` library's handle-reusing mechanism with UAF protection bits is excellent for catching bugs but will call `exit(1)` if a stale handle is accessed via `mls()` or `m_buf()`. Use `m_is_freed()` to safely check handle validity without crashing.
