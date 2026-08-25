# Bulletproofing HTTP Parsing with `mls`

HTTP parsing is notoriously difficult in Standard C. Because the protocol is text-based and relies on variable-length headers and bodies (like chunked encoding), it is a frequent source of security vulnerabilities. This document explores how the `mls` (Memory List System) architecture fundamentally mitigates the risks that have plagued major web servers for decades.

---

## 1. The Vulnerability Landscape: Lessons from History

Traditional C parsers often fail because they rely on manual pointer arithmetic and fixed-size buffers. When a parser encounters unexpected input—such as an extremely large chunk size or a malformed header—it can easily slip into undefined behavior.

### Key CVEs in HTTP Parsing

| CVE | Affected System | Type | Root Cause |
|---|---|---|---|
| **CVE-2013-2028** | **Nginx** | Stack Buffer Overflow | A signed comparison error when parsing a large `Transfer-Encoding: chunked` size allowed a buffer overflow. |
| **CVE-2002-0392** | **Apache** | Integer Overflow | Lack of sanity checks on chunk sizes led to an integer wrap-around, resulting in a heap buffer overflow. |
| **CVE-2021-32714** | **Hyper (Rust)** | Integer Overflow | Even in memory-safe languages like Rust, an integer overflow when decoding excessively large chunks could lead to request smuggling. |

### The Common Theme: Pointer & Length Fragility
In all these cases, the parser made assumptions about the input length and the available buffer space. In Standard C, once you have a `char *ptr`, you have lost the context of how much memory is actually behind it.

---

## 2. How `mls` Mitigates These Risks

`mls` replaces raw pointer management with an **Integer Handle System**, providing several layers of defense-in-depth for HTTP parsing.

### A. Automatic & Safe Resizing (`m_write`, `m_put`)
In a standard parser, if a header is longer than the `char buffer[4096]`, you must manually `realloc`. If you forget, or if your `realloc` logic has an integer overflow, you have a vulnerability.
- **MLS Benefit:** `m_write` and `m_put` automatically check the capacity of the list. If the input exceeds the current size, the library handles the resize internally. The developer doesn't need to write a single line of allocation logic.

### B. Handle-Based Bounds Checking (`_get_list`)
In Standard C, a pointer doesn't know its own bounds.
- **MLS Benefit:** Every time you access a handle via `mls(h, index)`, the library calls `_get_list`. This internal function validates:
    1. That the handle is actually allocated.
    2. That the index is within the valid range of that specific list.
    3. That the UAF (Use-After-Free) protection pattern matches.
If any of these fail, the library exits with an error before memory corruption can occur.

### C. Integer Overflow Protection
CVE-2002-0392 occurred because `size + 1` wrapped around to a small number.
- **MLS Benefit:** Because `mls` manages the "length" and "capacity" inside a structured `lst_t` header, it can perform consistent, centralized checks. While `mls` itself uses integers, the abstraction away from raw `malloc(n * size)` calls reduces the surface area where a developer can make a manual wrap-around mistake.

### D. UAF Protection (Use-After-Free)
HTTP parsing often involves complex state machines where headers are parsed, stored, and eventually freed. A logic error can lead to accessing a header after it was freed.
- **MLS Benefit:** `mls` handles include an 8-bit protection pattern. If a handle is freed and its slot is reused, the old handle becomes instantly invalid. Any attempt to use it will be caught by the pattern mismatch check, preventing UAF-based exploits.

---

## 3. Comparative Example: Parsing a Chunked Header

### Standard C (The "Dangerous" Way)
```c
char *buf = malloc(chunk_size); // What if chunk_size is negative or wrapped?
memcpy(buf, socket_data, chunk_size); // Out-of-bounds read/write risk!
```

### MLS (The "Safe" Way)
```c
int h = m_alloc(0, 1, MFREE);
// m_write(handle, offset, data, length)
m_write(h, 0, socket_data, chunk_size); 
// Internal mls logic validates that 'h' is active and handles 
// all resizing/bounds checks for 'chunk_size'.
```

---

---

## 5. Protocol-Level Caveats & Robust Parsing Practices

While `mls` solves the memory safety problem, robust HTTP parsing also requires careful protocol logic to defend against higher-level attacks.

### A. HTTP Request Smuggling (The CL.TE / TE.CL Trap)
If a request contains both `Content-Length` (CL) and `Transfer-Encoding: chunked` (TE), servers must decide which to trust. If a front-end proxy trusts CL and a back-end server trusts TE, an attacker can "smuggle" a hidden second request.
- **Caveat:** RFC 7230 states that if both headers are present, **TE must take precedence** and **CL must be ignored**.
- **MLS Practice:** Use `m_table` to store headers. If both appear, your logic should explicitly check for their presence and apply the RFC priority.

### B. Line Terminator Inconsistencies (Bare LFs)
The HTTP standard requires `\r\n` (CRLF) for line endings. However, many servers historically accepted a bare `\n` (LF). This inconsistency is a vector for smuggling attacks (e.g., CVE-2025-22871).
- **Caveat:** Be strict. If your parser expects CRLF, ensure it doesn't fallback to a bare LF without understanding the security implications of that choice in your specific architecture.
- **MLS Practice:** `s_split` and `s_tok` can be used to break headers by `\r\n`. Ensure your parser validates that the `\r` is present and doesn't silently ignore its absence.

### C. Resource Exhaustion (DoS Protection)
Even with memory-safe `mls` handles, an attacker can send a request with 10 million headers or a 4GB single header line. This can lead to Denial of Service (DoS) via CPU or memory exhaustion.
- **Caveat:** Always enforce limits on header count, header line length, and total body size.
- **MLS Practice:** Use `m_len()` to check the size of a header being accumulated. If it exceeds a pre-defined limit (e.g., 8KB for a single header), drop the connection immediately.

### D. Header Injection & Whitespace
Whitespace in header names (e.g., `Host : example.com`) is forbidden but often handled inconsistently. Attackers use this to bypass security filters.
- **Caveat:** Header names must not contain whitespace. Header values should have leading/trailing whitespace trimmed.
- **MLS Practice:** Use `s_trim` (or similar slice operations) to normalize header values after parsing. Use `m_slice` to extract the key and value efficiently without copying.

### E. Chunked Encoding: The "Tiny Chunk" Attack
An attacker can send 1-byte chunks to consume server resources while keeping the connection open for a long time.
- **Caveat:** Monitor the number of chunks and the rate of data transfer.
- **MLS Practice:** Accumulate chunk data into an `mls` handle, but track the chunk count. If you see thousands of tiny chunks, it may be a DoS attempt.

## 6. Conclusion: Moving from "Manual" to "Managed" C

The history of HTTP vulnerabilities shows that even the most experienced developers make mistakes when managing raw memory. `mls` provides a **Managed Memory Layer for C**. By abstracting buffers into handles, it turns catastrophic "Remote Code Execution" bugs into safe, controlled program exits.

---

## 7. Outline: Building a Bulletproof HTTP Parser with `mls`

To implement a robust parser, we follow a strict state-machine approach combined with the safety features of `mls`.

### Phase 1: Resource Limits & State
- **Limits:** Max headers (e.g., 100), max header line (8KB), max body size (10MB).
- **State:** `HTTP_STATE_REQUEST_LINE`, `HTTP_STATE_HEADERS`, `HTTP_STATE_BODY`, `HTTP_STATE_DONE`.

### Phase 2: Request Line Parsing
- Use `s_readln` or `s_tok` to extract the first line.
- Validate the method (GET, POST, etc.) using `s_cstr` comparisons.
- Extract the URI and Version using `m_slice` to avoid copies.

### Phase 3: Header Management with `m_table`
- Use an `m_table` to store headers.
- **Normalization:** Convert all header keys to lowercase using `s_lower` before inserting into the table to handle case-insensitivity securely.
- **Strict CRLF:** Only accept `\r\n`. Reject or log warnings for bare `\n`.
- **Precedence:** Explicitly check for both `Content-Length` and `Transfer-Encoding`. If both exist, delete `Content-Length` from the table and proceed with chunked parsing.

### Phase 4: Body Parsing (Chunked vs. Fixed)
- **Fixed:** If `Content-Length` is set, use `m_write` to accumulate the exact number of bytes.
- **Chunked:** Implement a sub-state machine for chunks. Use `mls` to store the accumulated body. Validate chunk sizes against remaining memory limits.

### Phase 5: Verification & Cleanup
- A single `m_table_free()` on the header table and `m_free()` on the body handle ensures zero memory leaks, even if the parsing was aborted due to an error.

---

## Implementation and Tests

See `lib/m_http.c` and `tests/test_http.c` for the reference implementation of this architecture.
