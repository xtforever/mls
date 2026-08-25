# Errors and Bug Fixes

## 2026-03-25: `m_slice` crash on null handle

**Problem:**
Calling `m_slice(dest, offs, 0, a, b)` caused a crash. The function attempted to call `m_width(m)` with `m=0`. `m_width` calls `_get_list(0)`, which triggers an `ERR("Not initialized")` if `ML` is initialized but the handle is invalid.

**Solution:**
Added a check at the beginning of `m_slice` to handle `m == 0` gracefully. If `m` is zero, it now returns `dest` (with length updated to `offs` if `dest > 0`) or `0`.

---

## 2026-03-25: `m_is_freed` logic in tests

**Problem:**
Tests were asserting `m_is_freed(old_handle)` after many new allocations.

**Root Cause:**
`m_is_freed` only checks if the handle index is in the `FR` (free) list. When a slot is reused for a new allocation, it is removed from the `FR` list, so `m_is_freed` returns `0` for the old handle. The actual protection comes from the 8-bit UAF pattern check in `_get_list`, not from `m_is_freed`.

**Solution:**
Updated tests to assert `m_is_freed` immediately after free, and clarified that it checks the free list status, not necessarily handle validity in a global sense.

---

## 2026-03-28: Flask Pointer Truncation (64-bit)

**Problem:**
When registering handlers in `m_flask`, function pointers (64-bit) were cast directly to `int` (32-bit) to be stored in `m_table`. This truncated the addresses, causing SEGVs when the handlers were called.

**Solution:**
Allocated a small memory block for each function pointer using `m_alloc` and stored the handle (int) in `m_table`. The handler is then retrieved by dereferencing the pointer stored in the handle's buffer.

---

## 2026-04-02: `m_table_get_cstr` lookup failure

**Problem:**
Looking up a C-string key in a table would fail if the table already contained keys stored as `mls` string slices. The comparison logic in `m_table_find_cstr_key_entry` used `strcmp` on raw buffer pointers which were not necessarily null-terminated.

**Solution:**
Updated `m_table_get_cstr` to duplicate the search key into a temporary null-terminated `mls` string handle (`s_dup`) and perform the search using `m_table_find_str_key_entry`. This ensures that all comparisons are performed against null-terminated strings.

---

## 2026-04-02: `s_base64_decode` empty result

**Problem:**
The Base64 decoder was returning empty strings or truncated data. The logic used `m_setlen(out, m_len(out) - 1)` after appending a null terminator, but the initial buffer management and bit-shifting logic didn't correctly track the number of bytes written to the `mls` buffer.

**Solution:**
Rewrote `s_base64_decode` to use a more robust bit-shifter and `m_putc` to append bytes to the destination handle. The null terminator is now added at the very end, and the length is correctly adjusted to represent the decoded binary data.

---

## 2026-04-02: `s_sub` logic error with negative length

**Problem:**
`s_sub(h, pos, -1)` (intended to mean "from pos to end") was returning an empty string. The condition `if (len < 0 || pos >= h_len) return s_new();` was too aggressive and didn't allow for the "rest of string" idiom.

**Solution:**
Refined the logic in `s_sub` to correctly calculate the remaining length if `len < 0`. It now checks if `pos` is within bounds and sets `len = h_len - pos` if a negative length is provided.

---

## 2026-04-02: LRU Eviction Hang / Timeout

**Problem:**
The Memcached-style server was hanging during LRU eviction when built in debug mode with `trace_level = 1`. The sheer volume of `TRACE` output from the HTTP parser and table operations saturated the I/O and caused the client to time out. Additionally, inconsistent handle management in the LRU list led to potential double-frees and program termination via `get_list`'s UAF protection.

**Solution:**
1. Implemented a robust `m_table_remove_by_str` that searches by content if the exact handle match fails, ensuring table entries are correctly removed even if handles were duplicated.
2. Simplified handle ownership: the table owns one `s_dup` copy, and the LRU list owns another.
3. Reduced `trace_level` to 0 for high-volume operations to prevent I/O bottlenecks.
4. Added `m_is_freed` checks before accessing handles in the LRU list to safely skip entries that were already freed by the table during overwrites.

---

## 2026-04-02: String Null-Termination Safeguards

**Problem:**
Several string-handling functions that return `mls` handles were not consistently ensuring that the returned string was null-terminated. This was particularly dangerous when cloning non-terminated string slices (`s_clone`) or during decoding/reading operations (`s_base64_decode`, `s_readln`). Many functions used a "hidden null" trick (`m_putc(h, 0); m_setlen(h, len);`) which, while sometimes effective, was inconsistent and potentially fragile.

**Solution:**
Audited all `s_` string functions and applied a consistent safeguard:
`if (m_len(ret) == 0 || CHAR(ret, m_len(ret) - 1) != 0) m_putc(ret, 0);`
This ensures that any handle returned as a string has at least one trailing zero, making it safe for use with `m_str()`, `strcmp()`, and other standard C-string functions.

Fixed functions:
- `s_clone`: Now ensures the clone is null-terminated even if the source was not. Also handles `h=0`.
- `s_base64_decode`: Replaced "hidden null" trick with explicit safeguard.
- `s_readln`: Replaced "hidden null" trick with explicit safeguard.
- `s_strlen`: Fixed to correctly handle multiple trailing null bytes by skipping all of them, ensuring it returns the length up to the last non-null character. Also added check for `m <= 0`.
- `s_slice`: Added check for `ret <= 0` to ensure it never returns 0 and correctly terminates.
- `s_sub`, `s_right`: Optimized and streamlined logic, ensuring both handle `h <= 0` gracefully.
- `s_strstr`, `s_find`: Now use `memmem` for safety with non-null-terminated strings.
- `s_has_suffix`: Now uses `memcmp` for safety.
- `s_chr`: Now uses `memchr` for safety.
- `s_spn`, `s_cspn`: Manual loops with `strchr` to avoid overflows on non-terminated strings.
- `s_casecmp`, `s_ncasecmp`: Implemented robust comparison logic with `tolower` loops to safely handle non-terminated strings and handle 0.
- `s_is_numeric`, `s_is_alpha`: Switched to counted loops using `s_strlen` to prevent buffer over-reads on non-terminated strings.
- `m_str`: Added a mandatory check for a trailing zero byte when `MLS_DEBUG` is enabled. This catches bugs where non-terminated buffers are accessed as strings.
- Verified `s_dup`, `s_printf`, `s_sub`, and `s_replace` already had adequate termination logic.

- `s_copy`: Refactored to use `s_slice` for consistency and to leverage `m_slice` indexing (e.g., `-1` for end).
- `s_replace`: Added explicit safeguard to ensure result is null-terminated even if `src` is a non-terminated slice.
- `s_msplit`: Updated to use `MFREE_EACH` for the resulting list so that parts are automatically freed. Also switched to `s_slice` to ensure every part is null-terminated.
- `v_find_key`, `http_server_config_load`: Added NULL pointer guards before `strcmp` calls where arguments originate from parameters or external data.
- `v_init`: Updated to use `MFREE_EACH` to leverage automatic recursive cleanup of variable handles.

---

## 2026-04-03: `m_table` Value Type Upgrade (uint64_t)

**Problem:**
The `m_table` entry structure used `int` for values, which is only 32-bit on many systems. This prevented safe storage of 64-bit pointers directly in the table, requiring awkward workarounds like allocating extra memory to hold the pointer.

**Solution:**
Upgraded `m_table_entry_t` to use `uint64_t` for the `value` field. This allows storing handles, raw integers, and 64-bit pointers in the same field safely.
- Added `MLS_TABLE_TYPE_PTR` for raw pointer identification.
- Implemented `mt_setp(t, key, ptr)` and `mt_getp(t, key)` for easy pointer management.
- Updated `m_table_free_handler` and `m_table_set_entry` to correctly handle the larger type while still respecting handle ownership for `mls` handles (by ignoring values outside the valid handle range or tagged as raw pointers).
- Verified that existing integer and handle-based functions work correctly with the new 64-bit backend.
