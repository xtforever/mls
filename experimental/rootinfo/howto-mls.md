# howto-mls: string list creation and iteration

The pattern in `gather_users()` shows how to process MLS string handles
without raw pointer access, ad-hoc arrays, or hand-rolled loops.  The same
building blocks apply wherever you need to extract substrings, build
unique lists, or iterate handle contents.

## Core ideas

1. **`m_foreach` instead of `m_peek`/`m_len` loops**
2. **`copy_word` — reusable buffer, extracts first whitespace-delimited token**
3. **`mstr_empty` — safe empty-handle check (replaces `!m_str(h)` or `!*v`)**
4. **`_cmp_mstr` — compares two MLS string handles, empty-handle safe**
5. **`m_binsert` — sorted unique list, drops duplicates automatically**
6. **`s_split` — delimiter parsing without `strtok`/`strdup` (LVM/ports style)**
7. **`str_dup_h` / `str_line` / `STR_COPY` — safe CHAR-based copies**

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

## Where else this applies

The pattern has nothing to do with dedup specifically — it's the general
model for working with handle lists:

- Parsing command output line-by-line (any gather section that shells out)
- Sorting or grouping data by a key field
- Extracting the first whitespace-delimited field from each entry
- Any place where `m_peek` + index variable + `m_len` can become
  `m_foreach`
