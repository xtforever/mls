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

## 2026-03-28: HTTP Response Null Terminators

**Problem:**
Using `s_printf` with `m_len(h)` as the offset caused multiple null terminators to be embedded in the HTTP response headers and body. `curl` saw these as binary data and failed to display the response.

**Solution:**
Use `-1` as the offset for `s_printf` to correctly append to the string by overwriting the existing null terminator. Also use `s_strlen()` to get the correct length (excluding the final null) when writing to the socket.
