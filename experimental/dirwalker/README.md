# dirwalker — tree-backed directory traversal without pointers

`dirwalker.c` recursively walks a directory tree and builds an in-memory
representation using only integer indices. There is not a single `malloc`/`free`
call, no `->next` pointer chasing, and no recursive `free()` dance at cleanup
time. Instead, every node and every relationship lives in a handful of flat,
mls-managed arrays.

## Data model

```
dirnodelist[0] = root                           bm_dirnodeleaf = bitmap
       |                                         marking leaf nodes
       v
  struct dirnode {
      int name;      // handle to mls char[]  (string)
      int subs;      // handle to mls int[]   (child indices)
      int parent;    // index into dirnodelist
  };
```

A directory gets one `dirnode`. Its children are stored as integer indices in
`subs`, and those children reference their parent via the `parent` field.
Together, this forms a tree — without a single C pointer between nodes.

## The pointer-free promise

### 1. Ownership is unambiguous

In a pointer-based tree every node carries an array of `dirnode*` children and
possibly a `dirnode* parent`. Who owns those allocations? If a node is removed,
you must simultaneously update the parent's child list *and* free the node. If
the child list is `malloc`'d by the node but traversed by the parent, it's easy
to leak or double-free.

With mls handles (`int`), ownership reduces to one rule: whoever created the
handle calls `m_free()` on it. The flat `dirnodelist` owns every `struct
dirnode`. Each node owns its own `name` and `subs`. The bitmap is a separate,
independently managed resource. There is no ambiguity.

### 2. Cleanup is a single flat loop

```c
void dirnodelist_free(int m) {
    struct dirnode *dn; int p;
    m_foreach(m, p, dn) {
        m_free(dn->name);
        m_free(dn->subs);
    }
}
```

Compare this to a pointer-based tree where cleanup requires a post-order DFS
— recursively free children before freeing the current node. Get the recursion
wrong (a missing base case, a self-referential cycle, or a node shared between
two parents) and you have a dangling pointer, a double-free, or a leak. The mls
version is immune to all of those: it walks a flat array once and releases each
node's owned resources. The mls system handles the underlying memory.

### 3. No dangling pointers, ever

When a pointer-based tree is partially freed or mutated, the remaining pointers
can silently point to freed memory. A `dirnode*` becomes dangling. Index-based
references into an mls array remain valid until the array itself is freed.
During the lifetime of `dirnodelist`, every integer index is a safe handle. You
can pass indices around freely, store them in secondary structures, and never
worry about use-after-free.

### 4. Bitmap acceleration without pointer overhead

`bm_dirnodeleaf` is a parallel bitmap tracking leaf nodes (files) for fast lookups.
During tree construction, `bit_set()` marks each file node's bit as it is created.
`bit_set` and `bit_get` operate on a resizable `uint64_t[]` managed by mls — the
bitmap auto-grows via `m_setlen` when new bits fall beyond the current capacity.
In a pointer-based design this would be another linked structure or a hash set of
pointers — more allocations, more cleanup burden, more surface for bugs. Here
it's a flat `uint64_t[]` managed by mls, freed with a single `m_free()`.

### 5. Full-path reconstruction without recursion

`print_files_fullpath()` iterates the flat `dirnodelist` once, skips non-files via
`bit_get()`, and for each file walks the `parent` chain upward to reconstruct the
full path. No tree traversal, no recursion — just a flat loop over an array and a
`while` loop up the parent indices.

## Why this matters

Even experienced C developers routinely ship:

| Pointer-based pitfall            | mls approach                                      |
|----------------------------------|---------------------------------------------------|
| use-after-free / dangling ptr    | handles stay valid until array is freed           |
| double-free                      | each handle freed exactly once in flat loop       |
| recursive free stack overflow    | single flat loop, O(n)                            |
| cycle detection                  | impossible — a node can't contain itself          |
| ownership confusion              | owner = creator, simple to verify                 |
| pointer arithmetic off-by-one    | array indexing is bounds-checked by mls           |
| fragmentation from many mallocs  | mls allocates in pools, reducing fragmentation    |
| forgot to NULL out a pointer     | no raw pointers to NULL                           |

The flat-list + integer-handle pattern is not new (entity-component systems,
database IDs), but mls makes it practical for everyday C data structures with
zero boilerplate.

## Building and running

```sh
cc -I../../lib -std=c99 -o dirwalker dirwalker.c ../../lib/mls.c ../../lib/m_tool.c ../../lib/m_table.c && ./dirwalker
```

Walks the parent directory and prints the directory tree followed by every file with its full path.
