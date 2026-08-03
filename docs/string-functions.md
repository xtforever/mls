# String Functions Overview

A **string handle** is an `int` returned by `s_new()` or `m_alloc(..., 1, ...)` — a dynamic, null-terminated byte buffer (width=1).

## Lifecycle

| Function | Description |
|---|---|
| `s_new()` | Create empty string (init cap 16) |
| `s_dup(const char *s)` | Create from C-string copy |
| `s_clone(int h)` | Deep-copy an existing handle |
| `s_free(int h)` | Free the handle |
| `s_clear(int h)` | Empty content (keep handle) |
| `s_resize(int h, int len)` | Resize, ensure null-termination |

## Query

| Function | Description |
|---|---|
| `s_strlen(int h)` | Logical length (excluding trailing nulls) |
| `s_isempty(int h)` | 1 if empty or invalid |
| `s_lastchar(int h)` | Last non-null char, or 0 |
| `s_hash(int h)` | djb2 hash of content |

## Comparison

| Function | Description |
|---|---|
| `s_cmp(int a, int b)` | Lexicographic compare |
| `s_ncmp(int a, int b, int n)` | Compare up to n chars |
| `s_strcmp_c(int h, const char *s)` | Compare with C-string |
| `s_casecmp(int a, int b)` | Case-insensitive compare |
| `s_ncasecmp(int a, int b, int n)` | Case-insensitive up to n |
| `s_subcmp(s0, a, b, s1, a, b)` | Compare substrings by index ranges |
| `s_strncmpr(int str, int suffix)` | Compare suffix (end of string) |
| `s_secure_cmp(int a, int b)` | Constant-time comparison |

## Search

| Function | Description |
|---|---|
| `s_chr(int h, int c, int off)` | First `c` at or after `off` |
| `s_rchr(int h, int c)` | Last `c` |
| `s_find(int h, const char *sub)` | First C-string substring |
| `s_strstr(int h, int off, int pat)` | First handle `pat` from `off` |
| `s_has_prefix(int h, const char *p)` | 1 if starts with `p` |
| `s_has_suffix(int h, const char *s)` | 1 if ends with `s` |
| `s_spn(int h, const char *accept)` | Span of chars in `accept` |
| `s_cspn(int h, const char *reject)` | Span of chars not in `reject` |

## Concatenation & Appending

| Function | Description |
|---|---|
| `s_app(int h, ...)` | Append C-strings (NULL-terminated list) |
| `s_app1(int h, char *s)` | Append single C-string |
| `s_cat(int h, const char *s)` | Concatenate C-string (auto-alloc if h==0) |
| `s_ncat(int h, const char *s, int n)` | Concatenate up to n chars |
| `s_join(const char *sep, ...)` | Join C-strings with separator, return new handle |
| `s_printf(int h, int pos, const char *fmt, ...)` | Formatted append |
| `vas_printf(int h, int pos, const char *fmt, va_list)` | Formatted append with va_list |

## Substring & Slicing

| Function | Description |
|---|---|
| `s_sub(int h, int pos, int len)` | Extract substring (new handle) |
| `s_left(int h, int n)` | Leftmost n chars (new handle) |
| `s_right(int h, int n)` | Rightmost n chars (new handle) |
| `s_slice(int dest, int offs, int src, int a, int b)` | Copy range `[a,b]` into `dest` at `offs` |
| `s_copy(int h, int first, int last)` | Copy char range (new handle) |
| `s_strncpy(int dst, int src, int max)` | Copy handle `src` to `dst` with max |

## Replace, Trim & Case

| Function | Description |
|---|---|
| `s_replace_c(int h, const char *old, const char *new)` | Replace all C-string occurrences (new handle) |
| `s_replace(int dst, int src, int pat, int rep, int cnt)` | Replace handle pattern up to cnt times |
| `s_trim_c(int h, const char *chars)` | Trim chars from both ends (new handle; whitespace if NULL) |
| `s_trim(int h)` | In-place trim whitespace |
| `s_trim_left_c(int h, const char *chars)` | Trim left only |
| `s_trim_right_c(int h, const char *chars)` | Trim right only |
| `s_lower(int h)` | In-place lowercase |
| `s_upper(int h)` | In-place uppercase |
| `s_reverse(int h)` | In-place reverse |
| `s_pad_left(int h, int width, char pad)` | Left-pad to width (new handle) |
| `s_pad_right(int h, int width, char pad)` | Right-pad to width (new handle) |

## Numeric Conversion

| Function | Description |
|---|---|
| `s_from_int(int val)` | Create handle from int |
| `s_from_long(long val)` | Create handle from long |
| `s_from_double(double val)` | Create handle from double |
| `s_to_long(int h)` | Parse handle content as long |
| `s_is_numeric(int h)` | 1 if all digits |
| `s_is_alpha(int h)` | 1 if all alpha |

## Split & Join (Handle Lists)

| Function | Description |
|---|---|
| `s_split(int h, const char *s, int delim, int trimws)` | Split C-string by delimiter char into m-array of strings |
| `s_msplit(int dest, int src, int pattern)` | Split handle `src` by handle `pattern` into m-array |
| `s_implode(int dest, int srcs, int sep)` | Join m-array of string handles with separator |

## File I/O

| Function | Description |
|---|---|
| `s_readln(int h, FILE *fp)` | Read one line into handle |
| `m_str_from_file(const char *path)` | Read entire file into new string handle |
| `ioread_all(int fd, int h)` | Read all data from fd into handle |

## Output

| Function | Description |
|---|---|
| `s_puts(int h)` | Print content + newline to stdout |
| `s_write(int h, int n)` | Print up to n chars to stdout |
| `m_register_printf(void)` | Enable `%M` printf specifier for m-array handles |

## Wrapping C-strings (Zero-Copy)

| Function | Description |
|---|---|
| `m_wrapcstr(char *s)` | Wrap existing `char*` into handle (NOALLOC) |
| `m_wrapstrings(char **list, int n)` | Wrap `char**` array into handle |
| `s_ccstr(const char *s)` | Constant string from compile-time C-string |
| `s_cstrdup(const char *s)` | Constant string from heap copy |
| `conststr_lookup_c(const char *s)` | Interned constant string (deduplicated) |
| `conststr_lookup(int h)` | Interned constant string from handle |

## Header Locations

| Header | Functions |
|---|---|
| `lib/m_tool.h` / `m_tool.c` | Core `s_*` API (~50 functions) |
| `lib/m_extra.h` / `m_extra.c` | Extended: casecmp, to_long, trim_*, reverse, pad, is_*, secure_cmp, from_double/int, base64 |
| `lib/mls.h` / `mls.c` | Wrappers: `s_ccstr`, `s_cstrdup`, `m_wrapcstr`, `m_wrapstrings` |
