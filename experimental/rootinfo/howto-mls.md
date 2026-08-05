# howto-mls: string list creation and iteration

The pattern in `gather_users()` shows how to process MLS string handles
without raw pointer access, ad-hoc arrays, or hand-rolled loops.  The same
building blocks apply wherever you need to extract substrings, build
unique lists, or iterate handle contents.  The lib's `s_*`/`m_*` API is a
full libc-string replacement — the reference table is below in
"String API — libc replacement".

## Core ideas

1. **`m_foreach` instead of `m_peek`/`m_len` loops**
2. **`copy_word` — reusable buffer, extracts first whitespace-delimited token**
3. **`mstr_empty` — safe empty-handle check (replaces `!m_str(h)` or `!*v`)**
4. **`_cmp_mstr` — compares two MLS string handles, empty-handle safe**
5. **`m_binsert` — sorted unique list, drops duplicates automatically**
6. **`s_split` — delimiter parsing without `strtok`/`strdup` (LVM/ports style)**
7. **`str_dup_h` / `str_line` / `STR_COPY` — safe CHAR-based copies**
8. **`s_*` string API — libc replacement** (reference below)
9. **supervised access — `mls_safe`/`*_SAFE`/`m_write_safe` instead of raw
   pointer deref or `memcpy` into the buffer** (below)

## String API — libc replacement

The `s_*` family (`lib/m_tool.h`) plus the `m_*` core (`lib/mls.h`) is
designed to replace the libc string functions entirely. Everything works on
integer handles, never raw `char *`; the lib tracks length, reallocation, and
freeing. The only `char *` that ever appears is a *read-only* view via
`m_str(h)` / `m_buf(h)` — never write through it.

| libc | MLS replacement | notes |
|------|-----------------|-------|
| `strlen` | `s_strlen(h)` | logical length, trailing nulls excluded |
| `strdup` | `s_dup(c)` | empty handle for NULL/`""` |
| `strcpy` | `s_strcpy_c(out, c)` | clears `out`, copies in |
| `strcat` | `s_cat(h, c)` | allocates if `h == 0` |
| `strncat` | `s_ncat(h, c, n)` | up to `n` chars |
| `sprintf` | `s_printf(h, p, fmt, ...)` | appends when `p` is out of range |
| `strcmp` | `s_cmp(a, b)` | both handles |
| `strncmp` | `s_ncmp(a, b, n)` | |
| `strcasecmp` | `s_casecmp(a, b)` | `m_extra.h` |
| `strchr` | `s_chr(h, c, off)` | index or `-1` |
| `strrchr` | `s_rchr(h, c)` | index or `-1` |
| `strstr` | `s_find(h, sub_c)` / `s_strstr(h, off, sub_h)` | index or `-1` |
| `strspn` / `strcspn` | `s_spn(h, accept)` / `s_cspn(h, reject)` | |
| `strncpy` | `m_strncpy(dst, src, max)` | handle-to-handle |
| `atol` | `s_to_long(h)` | `m_extra.h` |
| `tolower`/`toupper` | `s_lower(h)` / `s_upper(h)` | in-place |
| `strtrim` (hand-rolled) | `s_trim(h)` in-place; `s_trim_c(h, chars)` new handle | |
| `strstr`+replace | `s_replace_c(h, old, new)` | all occurrences, new handle |
| `fgets` | `s_readln(buf, fp)` | length or `EOF` |
| `fgetc` loop | `m_fscan(m, delim, fp)` / `m_fscan2` | |
| whole file | `m_str_from_file(path)` | handle or `-1` |
| `printf("%s", m_str(h))` | `printf("%M", h)` | after one `m_register_printf()` |

**Split / join:**
- `s_split(m, cstr, c, trimws)` — m-array of `char *` (`MFREE_STR`; free with
  `m_free_strings(m, 1)` then `m_free(m)`).
- `s_msplit(dest, src_h, pat_h)` — m-array of string **handles** (`MFREE_EACH`).
- `m_str_split(ms, cstr, delim, trimws)` — m-array of `char *`, multi-char delimiter.
- `s_join(sep, ...)` — joins C strings (NULL-terminated varargs).
- `s_implode(dest, srcs_h, sep_h)` — joins a handle list into one handle.

**Substring extraction:** `s_sub(h, pos, len)`, `s_left(h, n)`, `s_right(h, n)`,
`s_slice(dest, offs, src, a, b)` — all return/new handles, source untouched.

**Wrap without copying** (zero-copy / constant): `s_ccstr(c)` / `s_cstrdup(c)`
(constant string handles from the conststr pool), `m_wrapcstr(s)`,
`m_wrapstrings(arr, n)`, `m_wrapints(arr, n)` — wrap existing memory
(`MFREE_NOALLOC`, do not `m_free` the data).

**Gotchas:**
- `m_putc(m, c)` does **not** null-terminate — you must `m_putc(m, 0)`
  yourself before `m_str()` or length logic works. `s_printf`/`s_app`/
  `s_slice` do it for you.
- `s_strlen` excludes trailing nulls; `m_len` counts raw bytes (may include
  the terminator). For strings, use `s_strlen`.
- `s_msplit` (handles) vs `s_split` (`char *`) differ in element type. Use
  `s_split` for fixed-width column parsing (see the LVM pattern), `s_msplit`
  when the parts feed other handle APIs.
- `cmp_mstr`, `cmp_mstr_fast`, `cmp_mstr_cstr_fast`, and `compare_int` are
  declared in `m_tool.h` but **not implemented** — using them is a link error.
  Use `_cmp_mstr` from `gather.h`, or `cmp_int` (defined in `mls.c`).
- `s_has_prefix(h, c)` / `s_has_suffix(h, c)` replace hand-rolled
  `strncmp` prefix/suffix checks.
- `m_str_split(ms, s, " ", 1)` only returns the **first** token on
  space-separated input — `skip_delim` advances its own `delim` pointer, so a
  single-char delimiter consumes just one char and a second `cut_word` hits
  `a == b` and stops. It is safe only for single-delimiter text (HTTP lines,
  query strings). For variable whitespace, use `s_split(m, s, ' ', 1)` and
  skip the empty tokens (repeated delimiters yield empty parts), or split on a
  char that never repeats (LVM/ports `'|'`/`':'` style).

## m_extra.c — extra string utilities

`lib/m_extra.h` adds string helpers beyond the m_tool core. Most return a
**new** handle and leave the input untouched; in-place ones are marked.

**Case-insensitive compare:**
- `s_casecmp(a, b)` — like `s_cmp` but case-insensitive.
- `s_ncasecmp(a, b, n)` — up to `n` chars.

**Numeric conversion:**
- `s_to_long(h)` — `atol` replacement, 0 if empty/invalid.
- `s_from_int(val)` / `s_from_double(val)` — number → new handle
  (`%d` / `%g`).

**Trimming** (all return new handles):
- `s_trim_left_c(h, chars)` — strip leading chars (NULL/empty = whitespace).
- `s_trim_right_c(h, chars)` — strip trailing chars.
- (m_tool core already has `s_trim` in-place and `s_trim_c` both sides.)

**Manipulation:**
- `s_reverse(h)` — in-place.
- `s_pad_left(h, width, pad)` / `s_pad_right(h, width, pad)` — pad to a
  width, return new handles (clone if already wide enough).

**Classification:**
- `s_is_numeric(h)` — digits only.
- `s_is_alpha(h)` — letters only.

**Security / encoding:**
- `s_secure_cmp(a, b)` — constant-time compare (timing-attack safe); returns
  0 equal / 1 not equal.
- `s_base64_decode(h)` — decode to a new handle.

## Supervised memory access — the `*_SAFE` family

The lib's whole point is to replace libc's **unsafe direct memory access** —
`memcpy` into the buffer, `*(int*)m_buf(h)` casts, `strcpy`/`strcat` into
fixed arrays — with **supervised** functions that check bounds, handle
use-after-free, and never overflow. Prefer these over touching `m_buf(h)`
or `m_str(h)` yourself.

### Element access — three tiers

Every typed accessor comes in three flavors. Pick checked or safe; the raw
pointer forms are the ones you are replacing:

| tier | macro | does |
|------|-------|------|
| checked | `INT(h,i)`, `CHAR(h,i)`, `STR(h,i)` … | `mls(h,i)`, aborts on bad handle/OOB |
| safe | `INT_SAFE(h,i)`, `CHAR_SAFE(h,i)`, `STR_SAFE(h,i)` … | `mls_safe(h,i)`, returns 0/NULL + sets `mls_errno` |
| unchecked | `INT_UNCHECKED(h,i)` … | `m_peek(h,i)`, no bounds check |

- `mls_safe(h, i)` — the non-aborting element pointer itself. Returns NULL
  on `h <= 0` (`MLS_EINVAL`), out-of-bounds (`MLS_EBOUNDS`), or
  use-after-free (`MLS_EUAF`). Check `mls_errno` to distinguish.
- `INT_SAFE(h, i)` — typed read that returns `0`/`NULL` instead of
  crashing. **This replaces `*(int *)m_buf(h) + i` and pointer arithmetic.**
- Use `_UNCHECKED` only in a proven-bound hot loop; it skips the check
  entirely.

### Bulk copy — replace `memcpy`

- `m_write(m, p, data, n)` — supervised `memcpy` **into** the array.
- `m_write_safe(m, p, data, n)` — same, returns `-1` + `mls_errno` instead
  of aborting.
- `m_read_safe(h, p, &data, n)` — supervised read **out** of the array.
- `m_cat(h, s)` — `m_write(h, m_len(h), s, strlen(s))`: append a C string,
  the `strcat` replacement. `s_cat(h, c)`/`s_printf` are the handle
  versions.
- `m_slice(dest, offs, m, a, b)` — copy a range between handles; the
  `memcpy(dst + off, src + a, n)` replacement.

### Non-aborting mutations

`m_put_safe`, `m_free_safe`, `m_setlen_safe`, `m_del_safe` — the normal
`m_put`/`m_free`/`m_setlen`/`m_del` but return `-1` and set `mls_errno`
instead of calling `ERR()`. Use at trust boundaries (parsing external
input) where a malformed handle should degrade, not kill the program.

The safe API is validated by `experimental/ex_fuzzy/test_error_api.c`
(build `make -C experimental/ex_fuzzy` and run `test_error_api.exed`).

## copy_word — reusable word buffer

```
static int copy_word(int buf, int str)
{
    if (!buf) buf = m_create(10, 1); else m_clear(buf);
    int p; char *d;
    m_foreach(str, p, d) {
        if (isspace(*d)) break;
        m_putc(buf, *d);
    }
    m_putc(buf, 0);
    return buf;
}
```

Allocates once on first call, reuses across iterations (`m_clear` then
refill).  Returns an MLS handle you can compare, insert, or read via
`m_str`.

**Usage pattern:**
```
int buf = 0;
for (...) {
    buf = copy_word(buf, some_handle);
    // buf now contains the first word of some_handle
}
m_free(buf);
```

## s_split — delimiter parsing without strtok/strdup

For fixed-field command output (LVM `--separator '|'`, etc.) use
`s_split()` from `lib/m_tool.c` instead of `strtok_r` + `strdup` + manual
trim loops:

```
int toks = m_alloc(10, sizeof(char *), MFREE_STR);   // token array, reused

m_foreach(lines, p, d) {
    s_split(toks, m_buf(*d), '|', 1);   // 1 = trim whitespace from fields
    char **t = (char **)m_buf(toks);
    if (m_len(toks) >= 4) {             // guard short/malformed lines
        // t[0] t[1] t[2] t[3] = columns, in position
    }
}
m_free(toks);
```

How it works:
- Splits a C string on a delimiter char into an m-array of `char*`.
- `remove_wspace = 1` trims each field, so LVM's column padding is handled
  for free.
- Passing a non-zero handle makes `s_split` clear and reuse it — one
  allocation for the whole loop, `m_free` once at the end.
- Empty fields become empty strings, so column positions stay stable.
- The tokens are owned by the lib (`MFREE_STR`); there is no `strdup` or
  `free` in your code, and no raw token pointer escapes the lib.
- Read the line via `m_buf(*d)` directly — no 512-byte `STR_COPY`
  intermediate, so no truncation.

## str_dup_h / str_line / STR_COPY — CHAR-based copies

Three ways to copy a handle, all reading through `CHAR()` and writing
through `m_putc` (no `m_str` pointer chasing):

- `str_dup_h(h)` — exact copy of a handle into a fresh handle.
- `str_line(h)` — copy up to the first `\n` or end (a handle that holds
  several lines), returns a fresh handle.
- `STR_COPY(dst, dstsz, h)` — copy into a fixed C buffer, stopping at `\0`
  or `\n`, always null-terminated.

Prefer a handle-returning helper (`str_dup_h`/`str_line`) whenever the
result feeds another MLS API; use `STR_COPY` only when a raw buffer is
genuinely required.

## mstr_empty — safe empty check

```
static inline int mstr_empty(int a)
{
    return (a == 0 || m_len(a) == 0 || CHAR(a, 0) == 0);
}
```

Replaces the common anti-patterns:
- `!handle` — misses zero-length handles
- `!*m_str(h)` — raw pointer access, unsafe
- `STRTAB_EMPTY(h)` — frees the handle; use `mstr_empty` when you only
  want to test, not free

## _cmp_mstr — safe string comparison callback

```
static int _cmp_mstr(const void *va, const void *vb)
{
    int a = *(int *)va;
    int b = *(int *)vb;
    if (mstr_empty(a) || mstr_empty(b)) return 0;
    return s_cmp(a, b);
}
```

Drop-in for `m_binsert` or any API that takes a `int (*cmpf)(const void *,
const void *)`.  Protects against empty handles (returns equal) then
delegates to `s_cmp`.

## m_binsert — sorted unique list

```
int unames = m_create(8, sizeof(int));  // list of MLS string handles
int buf = 0;

int p, *d;
m_foreach(who_lines, p, d) {
    buf = copy_word(buf, *d);
    if (m_binsert(unames, &buf, _cmp_mstr, 0) >= 0)
        buf = 0;       // ownership transferred to unames, alloc fresh next loop
}
m_free(buf);
```

How it works:
- `m_binsert` binary-searches the sorted list.  If the key already exists
  it returns a negative index.  If not, it inserts and returns the
  position.
- `buf = 0` after a successful insert tells `copy_word` to allocate a new
  buffer next iteration (since the old one now belongs to `unames`).
- `&buf` is passed so `m_binsert` can `m_put` the handle value into the
  list.
- `_cmp_mstr` with `with_duplicates = 0` ensures uniqueness.

## Full gather_users example

```
static void gather_users(int entries)
{
    int who_lines = subproc_lines("who 2>/dev/null");
    if (STRTAB_EMPTY(who_lines)) { m_free(who_lines); return; }

    int unames = m_create(8, sizeof(int));
    int buf = 0, p, *d;
    m_foreach(who_lines, p, d) {
        buf = copy_word(buf, *d);
        if (m_binsert(unames, &buf, _cmp_mstr, 0) >= 0)
            buf = 0;
    }
    m_free(buf);
    m_free(who_lines);

    int ucount = (int)m_len(unames);
    if (!ucount) { m_free(unames); return; }

    char line[256];
    int off = snprintf(line, sizeof(line), "Users (%d): ", ucount);
    for (int i = 0; i < ucount; i++) {
        if (i) off += snprintf(line + off, sizeof(line) - (size_t)off, ", ");
        char user[64] = "";
        sscanf(m_str(INT(unames, i)), "%63s", user);
        off += snprintf(line + off, sizeof(line) - (size_t)off, "%s", user);
    }

    int h = m_alloc(1, sizeof(text_t), 0);
    text_t *t = (text_t *)m_buf(h);
    *t = (text_t){0};
    t->text_h = s_dup(line);
    add_entry(entries, "text", h);
    m_free(unames);
}
```

Zero ad-hoc arrays.  No `m_peek`/`m_len` index loops.  No raw `m_str`
pointer chasing in the hot path.  No `O(n^2)` manual dedup.

## What this replaces

The pattern is not just about deduplication — `m_binsert` with
`with_duplicates=1` works the same way for non-unique lists.  The real
win is that MLS **handles stored as list entries** behave like values:
iterate with `m_foreach`, compare with `_cmp_mstr`, extract substrings
with `copy_word` — all without index arithmetic, pointer arithmetic, or
fixed-size arrays.

| Before | After |
|--------|-------|
| `for (i=0; i<m_len(h); i++) { m_peek(...) }` | `m_foreach(h, p, d)` |
| `char names[64][64]; int n=0;` + linear dedup | `m_binsert(list, &key, _cmp_mstr, 0)` |
| `!handle` or `!*m_str(h)` | `mstr_empty(h)` |
| `strcmp(m_str(a), m_str(b))` | `_cmp_mstr(&a, &b)` |
| `const char *v = m_str(h); sscanf(v, "%s", buf)` | `buf = copy_word(buf, h)` |
| `strdup(line)` + `strtok_r` + trim loop + `free` | `s_split(toks, m_buf(h), '|', 1)` |
| `char line[512]; memcpy(line, m_str(h), ...)` | `str_dup_h(h)` / `str_line(h)` / `STR_COPY(buf, sz, h)` |
| `*(int *)m_buf(h) + i` / pointer arithmetic | `INT(h,i)` (checked) / `INT_SAFE(h,i)` (safe) |
| `memcpy(dst, src, n)` into/out of the array | `m_write(m,p,data,n)` / `m_read_safe(h,p,&data,n)` |
| `strcat(buf, s)` into a fixed buffer | `m_cat(h, s)` / `s_cat(h, c)` / `s_printf(h, -1, ...)` |
| unchecked `memcpy`/`memmove` | `m_slice(dest, offs, m, a, b)` |
| abort-prone `m_put`/`m_free` on untrusted data | `m_put_safe` / `m_free_safe` (+ `mls_errno` check) |

## Where else this applies

The pattern has nothing to do with dedup specifically — it's the general
model for working with handle lists:

- Parsing command output line-by-line (any gather section that shells out)
- Sorting or grouping data by a key field
- Extracting the first whitespace-delimited field from each entry
- Any place where `m_peek` + index variable + `m_len` can become
  `m_foreach`
- Anywhere you reach into the buffer with `m_buf`/`m_str` + `memcpy` or a
  cast — replace with `*_SAFE` accessors / `m_write` / `m_slice` instead
  (write through `m_buf(h)` is undefined; always go through the API)
