# You Do Not Know C

### Pointers Considered Harmful — the case for handles

**Status:** Manifesto  
**Date:** 2026  
**Repo:** <https://github.com/xtforever/mls> · v0.2.0

---

> If you still write `malloc`, `realloc` and `free` by hand, and pass a pointer
> and a size around as a pair, you are doing it wrong.

---

## 1. The canon is the problem

Open a random C file. Skip the headers. Find the first `malloc`. Chances are it
is sitting in the middle of the logic, three lines below the loop it belongs to,
with a `free` forty lines later and a `realloc` somewhere in between.

That is not a mistake. That is the canon. It is how most C is written — in
OpenSSL, in libtiff, in the majority of the code you will read today. Allocate
where you need it, resize when it fills, free when you are done. It reads as
direct and honest: you are managing your own memory, the way C intends.

It is also the single pattern behind a large share of the memory errors and CVEs
in C [1][2]. Not because the programmers are careless, but because the pattern
hands every one of those sites a checklist it must get right, every time:

- **overflow check** — did `count + 1` or `size * elem` wrap?
- **bounds check** — is the index still inside the buffer?
- **resize check** — did `realloc` move the block and invalidate the helpers?
- **free check** — was this pointer already freed, or is it still needed?

Four checks, per site, forever. Miss one and you have a memory error. The
industry has spent fifty years telling itself this is a discipline problem, to
be solved by being more careful, by code review, by fuzzing, by sanitizers. It
is not a discipline problem. It is a representation problem. The buffer is
represented as something that makes the error easy and the check manual.

Change the representation and the checklist shrinks to nothing.

---

## 2. The failure point is the pair

Here is the uncomfortable fact: in C, a dynamic buffer is not one value. It is
**two** — a pointer and a length — and you keep them in sync by hand at every
access.

```c
/* appending to a dynamic array, the traditional way — every check, by hand */
int *a = NULL;
size_t cap = 0, len = 0;
/* ... */
if (len == cap) {                                    /* resize check */
        size_t ncap = cap ? cap * 2 : 8;
        if (ncap < cap || ncap > SIZE_MAX / sizeof *a)   /* overflow check */
                goto err;
        int *t = realloc (a, ncap * sizeof *a);
        if (!t)                                      /* failure check */
                goto err;
        a = t;
        cap = ncap;
}
if (len >= cap)                                      /* bounds check */
        goto err;
a[len++] = 42;
/* the free check is still yours: who else holds `a`? */
err:
free (a);
```

Look at how much of that is ceremony, and notice that none of it is optional:
the overflow check guards the growth and the element-size product; the bounds
check is still yours — the language will not do it, and this `len >= cap` guard
is only defensive; the resize check catches the block moving out from under every
other copy of the pointer; and the free check has no code form at all — it is
pure discipline.

The pointer and the length are a matched pair, and C gives you no way to keep
them matched. You can enforce the invariant with discipline, review and prayer,
or you can remove the pair from your program.

The pair is the bug. Not the programmer's attention span.

---

## 3. One value instead of two

An `mls` handle is a single stable `int`. You do not hold the pointer. You do
not hold the capacity. The library holds them, and the handle keeps working
across every resize.

```c
int a = m_alloc (n, sizeof (int), MFREE);
m_put (a, &(int){42}); /* bounds-checked, auto-resizing */
m_free (a);
```

`m_alloc` treats allocation failure as fatal — it prints a diagnostic and exits;
`m_alloc_safe` reports the same failure as an error you can handle via
`mls_errno`. The bookkeeping is still centralized; you choose the failure policy.

The contrast is not cosmetic. It is the difference between **two values that
must agree** and **one value that cannot disagree with itself**:

| | Traditional C | Handles (`mls`) |
|---|---|---|
| What you hold | `T *` **and** `size_t` | one `int` |
| On resize | every pointer copy is invalidated | the handle is unchanged |
| On append | check capacity, `realloc`, re-check | `m_put` — resizes and bounds-checks |
| On access | index check is on you | `INT(h, i)` is checked |
| On free | remember every path | one `m_free(h)` |

The handle is not a pointer with extra steps. It is a number that names a slot
in a table the library owns. The pointer and the capacity never leave the
library, so they cannot drift, cannot dangle, and cannot be left stale by a
`realloc`. The bookkeeping that used to be spread across your file now lives in
one place, and it is not in your diff.

That is the whole trade: a table lookup on access, paid once per operation, in
exchange for never doing the four-point checklist again.

> The ten names you need: `m_alloc(n, w, f)` allocates `w`-byte elements with
> free-handler `f`; `m_put(h, &v)` appends one element, growing the buffer as
> needed; `mls(h, i)`/`m_buf(h)` return a pointer into the buffer, `INT(h, i)` a
> checked element; `m_free(h)` releases, recursing through `MFREE_EACH` children.
> Free handlers: `MFREE` (free the buffer), `MFREE_EACH` (free each element
> handle). `m_count_allocated()` is the live-handle count. `m_alloc_safe`/
> `mls_safe` report the handleable failures — bounds, OOM, size overflow — through
> `mls_errno`; use-after-free and invalid positive handles remain fatal, matching
> §7. `CHECK`, `node()` and `node_t` in §5–§6 are from the example, not the
> library.

---

## 4. Dangling pointers become deterministic errors

A raw pointer after `free` is untellable. It is an address; C has no way to ask
whether the storage behind it is still there. Reading it is undefined behaviour,
writing it corrupts the heap, and the failure may surface a thousand instructions
later in a function that never touched the allocation.

A handle can be checked, because it is not an address. When `m_free` releases a
slot it marks that slot in a way that cannot be mistaken for a live one, and the
access path tests it **before** returning anything. So a stale handle to a slot
that has been freed and not yet recycled dies right there, with a diagnostic —
not with silent corruption.

When the slot *has* been recycled by a new allocation, a generation tag in the
handle is the second line of defence: the old handle's tag no longer matches,
and the access is refused. Double free is caught by the same check.

Be precise about what this buys, because the honest version is more useful than
the marketing version:

- the **common** use-after-free — the stale touch that happens near the free,
  the shape real bugs actually take — is caught **deterministically**, before
  the tag is even consulted;
- the tag is a **backstop** for the rarer long-lived dangling handle, and it is
  probabilistic: the counter is finite and wraps after 128 frees;
- the failure is **loud**. On the default path it terminates the program with
  the file, the line and the function, instead of continuing in an undefined
  state.

Loud and deterministic beats silent and undefined. That is the entire point of a
tripwire, and it is why you want one.

---

## 5. Teardown is one call

Freeing a nested structure by hand is where the careful programmer finally runs
out of care. The recursive free function is always the same and always a little
wrong:

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

One missed branch leaks. A back-reference into an ancestor recurses forever. And
the function has to know the shape of the object, so every type gets its own
near-identical version to get subtly wrong.

In `mls` the object declares its own release rule **once**, and the whole graph
follows it. A node registers a free handler that releases its payload, and a
group's child list is allocated with `MFREE_EACH`, so `m_free` recurses into the
children:

```c
static void free_world_node (int h) { m_free (((node_t *)mls (h, 0))->data); }

static int ngroup (void)
{
        /* Child array of node handles; MFREE_EACH m_free()s each child. */
        return node (NODE_GROUP, m_alloc (0, sizeof (int), MFREE_EACH));
}
```

The entire scene — nested groups, mixed leaf types, shared nodes and even a
back-reference that turns it into a cycle — is released with a single call:

```c
m_free (root);
```

`m_free` marks a handle as being freed **before** it runs the free handler, and
`MFREE_EACH` skips children that are already gone. A cycle is therefore seen as
already-freed and skipped: no infinite recursion, no double free. The teardown
logic is written once, next to the data it describes, instead of once per caller
that happens to own a reference.

`examples/world.c` in this repository builds exactly such a graph — including a
`world → actors → earth → world` cycle — and tears it down with one call in its
selftest. It is not a sketch; it runs.

---

## 6. Leaks you can see

You do not need a sanitizer to notice a leak. `m_count_allocated()` reports the
number of live handles, so a rising handle count is a leak you can observe in a
plain, optimized release build.

`examples/world.c`'s selftest brackets its work with it:

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

That check is not a debugging ritual you switch on and forget to switch off. It
is arithmetic on an integer, and it works in the build you ship.

The same introspection generalises to other resources. If the things a program
holds — memory, but also `FILE *` and file descriptors — sit behind handles, the
count that reports leaked memory also surfaces an unclosed file. Resource
accounting falls out of the handle table for free.

---

## 7. The honest boundary

Handles do not make C memory-safe. Nothing in this paper claims they do.

What they do is narrower, and worth saying precisely so it cannot be oversold:

- they make a class of error **louder** — a dangling handle dies with a
  location, where a dangling pointer corrupts silently;
- they make the code **shorter** — the pointer/size bookkeeping is gone, and a
  whole graph frees with one call;
- they make leaks **visible in release builds**, through an integer count;
- they do **not** make a bad memory access impossible.

The UAF check is a tripwire, not a guarantee. The generation tag is finite and
wraps; a long-lived stale handle that is dereferenced after the counter comes
full circle can slip through, and the library is explicit that on its default
path a use-after-free is a *fatal* error — it exits, it does not politely return.
That is the right default for a tool, where a loud crash beats silent corruption;
a long-running daemon whose availability matters should decide its own fatal
policy and isolate the failure, not pretend the tripwire is a wall.

There is one boundary where the old hazard walks back in, and it deserves to be
named: `mls(h, i)` and `m_buf(h)` hand back a plain pointer into the library's
buffer, because `write(2)`, `qsort`, `memcpy` and every existing C API take
pointers. That pointer is valid only until the next call on the same handle that
can move the buffer — `m_put`, `m_write`, `m_setlen` or `m_resize`, any of which
may `realloc` it — or until `m_free` releases it. Cache that pointer across a
resize and you have recreated, by hand, the exact dangling pointer the handle was
meant to remove. The rule is: resolve the handle at the point of use and never
hold the pointer across a call that can grow the buffer. And because the pointer
is returned without the lock held, another thread mutating the same handle can
move the buffer out from under you mid-access: the handle serialises the list it
names, not the buffer it hands back. One more consequence worth stating: a handle
is not a reference count — two parts of a program that both call `m_free` on a
shared node race for a single owner slot, and the second dies the same loud
use-after-free death as any other. The handle model assumes single, explicit
ownership; it does not invent shared ownership for you.

So choose handles for what they are: **ergonomics and development-time
tripwires**, the equivalent of seatbelts and a dashboard warning light. Where a
component's threat model genuinely demands a memory-safe language, that is a
different decision, made on different evidence, and this paper does not relitigate
it. The claim here is smaller and, unlike the grand version, true: *if you are
writing C, stop doing the bookkeeping by hand.*

---

## 8. Conclusion: the check is the confession

Dijkstra's famous letter [3] was not titled *Programmers Who Use Go To Considered
Harmful*. He argued that the go to statement should be **abolished** — the
unbridled jump of the ALGOL era, a transfer that could land in the middle of a
loop or out of a block, leaving no meaningful answer to "where is the program" —
because a construct that is the recurring source of a class of errors should be
made unrepresentable. The letter targets that unbridled ALGOL-era form, not C's
function-local `goto`, whose idiomatic use — `goto err;` — is one of the
disciplined forms the argument left standing (see the appendix on Dijkstra's
*go to*). The industry did not respond by being more careful. It abolished the
unbridled use, or replaced the go to with structured forms, and the class of bugs
went with it.

You cannot remove `malloc` from C. But you can stop putting it in the middle of
your logic, stop passing pointer-plus-size pairs across function boundaries, and
stop pretending that the four-point checklist is a test of character. Every raw
`malloc` in your diff is a small confession: *I am still doing, by hand, the
bookkeeping a table could do for me.* Every `free_tree` you write is a recursive
function that exists only because the data structure could not free itself.

The handle does not make you careless. It makes you finished. The pointer and
the length live in one place, the resize cannot strand them, the dangling
reference is loud, the leak is a number, and the teardown is one call.

If you are still doing it the other way, you are doing it wrong.

---

### Appendix: what Dijkstra meant by *go to* — and why it is not C's `goto`

Section 8 leans on Dijkstra's 1968 letter [3], so it owes the reader the precise
target. Dijkstra's word was *unbridled*: "The unbridled use of the **go to**
statement has as an immediate consequence that it becomes terribly hard to find a
meaningful set of coordinates in which to describe the process progress." His
argument was not moral but mechanical: the progress of a running program can be
located by a *textual index* — which statement in the text — that procedures
extend into a sequence (one per call) and repetition extends with a *dynamic
index* (which iteration). These are coordinates the programmer does not control,
and that therefore stay meaningful. The go to, used without restraint, destroys
them; the only coordinate left is a counter of actions performed since program
start, which is "utterly unhelpful."

That is the go to of ALGOL-era languages: a label declared in a block or an
enclosing block, and a jump to it from inside — out of a loop, out of a block,
out of a procedure body. The one thing ALGOL 60 did forbid was a jump from
outside *into* a block. It is *not* the `goto` of C, which did not exist in 1968.
C's `goto` is the same primitive,
restrained only by function locality and the variably modified scope rule
(C11 6.8.6.1); it cannot cross a function boundary, and it may not jump from
outside the scope of a variably modified identifier into that scope. Its idiomatic
use — `goto err;` and friends — is exactly the restricted "alarm exit" that
Dijkstra's letter recalled, tentatively attributing to Hoare the recommendation to
"restrict the use of the **go to** statement to alarm exits."

So when Section 8 says the industry abolished the unbridled use and replaced the
go to with structured forms, C's `goto` is one of the kept, bridled forms, not one
of the removed ones. The memory analogy holds the same way: the unstructured,
hand-rolled handling of memory — `malloc` here, `free` there, a length drifting
alongside — is the unbridled form, and the handle is the structured form. The point
is not to ban `goto`, and not to claim the handle makes memory errors
unrepresentable. It is that the disciplined remnant beats the unbridled original.

The famous title is itself a retitling: Dijkstra's manuscript was titled *A Case
against the GO TO Statement*; the *Considered Harmful* wording was the editor's.

---

### References

[1] The Chromium Project. "Memory Safety" (2020).
<https://www.chromium.org/Home/chromium-security/memory-safety/> — "Around 70% of
our high severity security bugs are memory unsafety problems … Half of those are
use-after-free bugs."

[2] Miller, Matt. "Trends, challenge, and shifts in software vulnerability
mitigation." Microsoft Security Response Center, BlueHat IL, 2019.
<https://github.com/microsoft/MSRC-Security-Research/tree/master/presentations/2019_02_BlueHatIL>
— roughly 70% of Microsoft's patched CVEs are memory-safety issues.

[3] Dijkstra, E. W. (1968). "Go To Statement Considered Harmful." *Communications
of the ACM*, 11(3), 147–148. DOI: 10.1145/362929.362947.

---

### Appendix: the two-minute test — one header, one `cc`

Do not take the word for it. Copy the single-header amalgam and the demo file
(`examples/handles_demo.c`) into an empty folder and build them with one command:

    python3 generate_amalgam.py            # repo root; writes mls_full.h
    cc -std=c11 handles_demo.c -o demo -lpthread -lm -ldl

`mls_full.h` is the whole library as one header — arrays, strings, tables, HTTP,
HDF — with no build system and no `-D` flags. Then:

    $ ./demo
    0 1 2 3 4 5 6 7
    live handles after teardown: 0

The first line is an array that starts with room for two and is appended to eight
times: the handle never changes, there is no `realloc` in the code, and
`INT(a, i)` is checked. The second line is two nodes that point at each other — a
cycle — released with a single `m_free(g1)`, with `m_count_allocated()` as proof
that nothing leaked.

The tripwire is one argument away:

    $ ./demo crash
    0 1 2 3 4 5 6 7
    live handles after teardown: 0

    [mls error] mls_full.h:1415 lock_handle_core(): List 5 not allocated

The process exits with status 1 at the point of detection, naming the handle,
instead of carrying on with a corrupted heap. Build with `-DMLS_DEBUG` and the
same crash prints a post-mortem that names both the line that freed the handle
and the line that used it.

```c
#define MLS_FULL_IMPLEMENTATION
#include "mls_full.h"
#include <stdio.h>
#include <string.h>

/* one node = one handle; its payload is another handle */
typedef struct {
	int kind;
	int data;
} node_t;

static int node_hdl; /* id of the registered free handler */
static void free_node (int h) { m_free (((node_t *)mls (h, 0))->data); }
static int payload (int h) { return ((node_t *)mls (h, 0))->data; }

static int node (int kind, int data)
{
	node_t n = {kind, data};
	int h = m_alloc (0, sizeof (node_t), node_hdl);
	m_put (h, &n);
	return h;
}

int main (int argc, char **argv)
{
	m_init ();
	node_hdl = m_reg_freefn (free_node);
	size_t base = m_count_allocated ();

	/* 1. one value, not two: the handle survives every resize */
	int a = m_alloc (2, sizeof (int), MFREE); /* capacity 2 */
	for (int i = 0; i < 8; i++)
		m_put (a, &i); /* grows past 2 */
	for (int i = 0; i < m_len (a); i++)
		printf ("%d%s", INT (a, i), i + 1 < m_len (a) ? " " : "\n");
	m_free (a);

	/* 2. teardown is one call: two nodes that point at each other */
	int g1 = node (0, m_alloc (0, sizeof (int), MFREE_EACH));
	int g2 = node (0, m_alloc (0, sizeof (int), MFREE_EACH));
	m_put (payload (g1), &g2); /* g1 -> g2 */
	m_put (payload (g2), &g1); /* g2 -> g1 (a cycle) */
	m_free (g1);

	/* 3. leaks you can see, in a release build */
	printf ("live handles after teardown: %zu\n",
		m_count_allocated () - base);

	/* 4. use-after-free is loud, not silent: run `./demo crash` */
	if (argc > 1 && strcmp (argv[1], "crash") == 0) {
		int h = m_alloc (1, sizeof (int), MFREE);
		m_free (h);
		printf ("%d\n", INT (h, 0)); /* exits, with a diagnostic */
	}

	m_destruct ();
	return 0;
}
```

---

### Appendix: the claim, in one line

> **C stays C. But the buffer should be a handle, not a pointer and a size.**
