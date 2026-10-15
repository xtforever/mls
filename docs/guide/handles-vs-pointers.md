# Handles instead of pointers — the `world.c` walkthrough

This document contrasts the traditional C style — raw `malloc`/`realloc`/`free`
with a pointer and a size at every access — with the handle style used by
`mls`, using `examples/world.c` as the running example. See also
[memory-management.md](memory-management.md) and
[advantages.md](advantages.md) for the broader comparison.

---

## 1. The C canon: inline `malloc` / `realloc` / `free`

Most C code bases — OpenSSL, libtiff, and the majority of C programs — allocate
and release memory ad hoc, right where a buffer is needed. That is the canon:
`malloc`, `realloc`, `free` in the middle of the logic.

It also means every one of those sites is responsible for a small checklist:

- **overflow check** — did `count + 1` or `size * elem` wrap?
- **bounds check** — is the index still inside the buffer?
- **resize check** — did `realloc` move the block and invalidate helpers?
- **free check** — was this pointer already freed, or is it still needed?

Missing any one is a memory error. That single pattern is responsible for a
large share of the bugs and CVEs in C code.

---

## 2. The `ptr + size` pair is the failure point

In C a buffer is not one value, it is a pair: a pointer **and** a length, both
kept in sync by hand on every access. The pointer can outlive the storage, the
length can drift from the allocation, and a `realloc` silently invalidates
every copy of the pointer.

```c
/* appending to a dynamic array, the traditional way */
int *a = malloc (n * sizeof *a);
size_t cap = n, len = 0;
/* ... */
if (len == cap) {
        int *t = realloc (a, (cap *= 2) * sizeof *a);
        if (!t) { /* error path */ }
        a = t;
}
a[len++] = 42; /* and the index check is on you */
free (a);
```

An `mls` handle is a single stable value; the pointer and the capacity never
leave the library:

```c
int a = m_alloc (n, sizeof (int), MFREE);
m_put (a, &(int){42}); /* bounds-checked, auto-resizing */
m_free (a);
```

---

## 3. Dangling pointers become deterministic errors

A pointer to freed memory — a dangling pointer — is always a problem: reading
it is undefined behaviour, writing it corrupts the heap. C has no built-in way
to tell whether an address is still valid.

An `mls` handle carries a generation pattern. When a slot is freed and reused,
the pattern changes, so an old handle no longer matches. Accessing it is a
**deterministic, loud error** instead of silent memory corruption. The same
check covers double free.

---

## 4. `world.c`: every object is a handle

`world.c` builds a small scene graph — groups, integer positions, double
masses, strings — out of one kind of node:

```c
typedef struct { int kind; int data; } node_t;
```

The `data` field is itself an `int` handle: a child-list handle for a group, an
int/double array for a leaf, a string handle for a name. Because every list in
`mls` is an `int` handle, one generic container can hold lists of **any**
element type; only when a leaf is resolved does the `kind` field select the
cast (`INT`, `DOUBLE`, `m_str`).

---

## 5. `world.c`: teardown with one `m_free`

Traditional recursive teardown is hand-written and easy to get wrong:

```c
void free_tree (Node *n) {
        if (!n) return;
        for (int i = 0; i < n->nchild; i++) free_tree (n->child[i]);
        free (n->child);
        free (n->name);
        free (n);
}
/* one missed branch = leak; a cycle = infinite recursion */
```

In `world.c` each node registers a free handler once, and a group's child list
uses `MFREE_EACH`:

```c
static void free_world_node (int h) { m_free (((node_t *)mls (h, 0))->data); }

static int ngroup (void)
{
        return node (NODE_GROUP, m_alloc (0, sizeof (int), MFREE_EACH));
}
```

The whole graph — nested groups, mixed leaves, and even a back-reference that
turns it into a cycle — is released with a single call:

```c
m_free (root);
```

`m_free` marks a handle as being freed **before** it runs the free handler, and
`MFREE_EACH` skips children that are already freed. A back-reference to an
ancestor is therefore seen as gone and skipped: no infinite recursion, no
double free.

---

## 6. Leaks are measurable: `m_count_allocated()`

You do not need ASan or the debug dump to notice a leak. The library exposes
the number of live handles, so a growing handle count is a leak that shows up
in a normal release build. `world.c`'s selftest brackets its work:

```c
size_t base = m_count_allocated ();

int seen = m_alloc (0, sizeof (int), MFREE);
CHECK (m_count_allocated () == base + 1);          /* the visited set */

/* ... build a 7-node world: each node = node handle + payload handle ... */
CHECK (m_count_allocated () == base + 1 + 14);

/* ... use it, then release everything ... */
m_free (seen);
m_free (root);
CHECK (m_count_allocated () == base);              /* nothing leaked */
```

A rising handle count over time is the cheap, always-on early warning that a
program's memory demand is drifting.

---

## 7. Beyond memory: other resources

The same idea generalises. If all resources a program holds — memory, but also
`FILE *` and file descriptors — are registered behind handles, then the same
introspection that reports leaked memory also surfaces an unfreed file handle.
Instead of leaking an `fd` silently, it shows up as a handle that is still
live. Resource accounting falls out of the handle table for free.

---

## 8. Debug mode: where was the handle allocated?

When a leak does happen, the interesting question is *where*. With
`-DMLS_DEBUG` every handle records the file, line, and function of its
allocation. A still-live handle at teardown is reported with that origin, so a
leak points at its source instead of leaving you to guess. The first
`[mls error]` line is the real failure site.

---

## 9. Net result

`world.c` is short **and** hard to break: no manual recursion, no pointer/size
bookkeeping, no hand-written bounds or free checks. The handle model trades a
tiny lookup overhead for code that is measurably leak-free in the release
build, survives cycles, and tells you where an allocation came from in debug
mode. Less code, more stable — and leaks you can see before they hurt.
