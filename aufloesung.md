# The Tripwire, Corrected

### A resolution of the `mls` criticism in *Safety Is Purchased, Not Declared*

**Status:** Resolution
**Date:** 2026

---

## 1. Purpose and scope

This document resolves the five criticisms of the `mls` library that appear in
`synthesis-v1.md` (*Safety Is Purchased, Not Declared*), which in turn summarises
the hostile reading in `contra-you-do-not-know-c.md` (*The Carriage
Manifesto*, §5 and §9).

It is written as an **honest resolution**, not a defence brief. The synthesis
was right about the one thing that matters most: `mls` is valuable as
*ergonomics*, and its *memory-safety claim* was oversold by the thesis. That
split stands and is not re-litigated here. What this document does is narrower:
it checks the synthesis's *technical* claims against the source, corrects the
two or three that do not survive that check, and proposes concrete fixes for
the limitations that are real.

This is also, explicitly, a **correction of the synthesis itself**. The
synthesis did not merely report the antithesis's dissection of `mls`; it
accepted it without qualification ("The antithesis's technical dissection of
`mls` is correct and checkable against the source", §4.3), and then reused it
in §6.1 item 3 and §8. A dissection that is *checkable* should have been
checked. It was not. This document does that.

### 1.1 The criticisms, with their exact sources

| # | Criticism | Where the synthesis states it |
|---|---|---|
| 1 | The use-after-free "protection" is a 7-bit tag that wraps after 128 frees → ~1/128 chance a stale handle matches a reused slot | §4.3, §6.1 item 3, §8 |
| 2 | On the default path a stale handle calls `exit(1)` → a remote denial-of-service primitive, "a different CVE class, not the absence of one" | §4.3, §6.1 item 3 |
| 3 | Bounds checking is per-access at runtime through a table lookup; Rust's is compile-time and free at runtime | §4.3 |
| 4 | It does not attempt data-race freedom at all — one of Rust's two headline guarantees | §4.3 |
| 5 | "Build your own" is the disease: `mls` is one more unstandardised private abstraction (`strlcpy`, `sds`, `GString`, `bstrlib`, `mls`) | §4.5, Appendix A #9 |

There is also the framing criticism, which the synthesis treats separately and
which this document accepts without argument:

| 6 | `mls`'s real value is ergonomics, which the antithesis ignores; its safety claim is the weak part | §3.4, §8, Appendix A #5 |

### 1.2 How to read the refutations

Each remaining section ends with a **Residual still true** line. That line is
binding. It names what remains correct even after the refutation. No section
claims that `mls` is memory-safe, and the conclusion does not withdraw the
synthesis's ergonomics/safety split. The claims are checkable against the
source; every factual assertion carries a `file:line`.

The code in sections 4, 5 and 10 is **sketch-level only**. It is written to show
the shape of a fix, not to run. Nothing in `lib/` is changed by this document.
Sketch labels say "proposal" throughout.

---

## 2. Fact-check table

| # | Claim | Correct as stated | Incorrect / incomplete | Tunable? |
|---|---|---|---|---|
| 1 | 7-bit tag, 128-free wrap | The mechanism exists: `REAL_UAF(m) = (m >> 24) & 0x7f` (`lib/mls_internal.h:42`); the counter advances on every free, `UAF_PROTECTION = (UAF_PROTECTION + 1) & 0x7f` (`lib/mls_base.c:739`). | The tag is **not the only stale-handle defence**, and the 1/128 is **not the detection rate for the common UAF**. See §4. | Yes — the tag width is a packing choice, not a law. |
| 2 | Default path calls `exit(1)` | True: `deb_err()` ends in `exit(1)` (`lib/mls_base.c:274`). | The antithesis's specific claim that the `_SAFE` accessors "return an error instead" is **false for UAF**: they exit too. Only bounds/OOM/overflow are returnable. See §5. | Yes — the *policy* around the exit is pluggable. |
| 3 | Bounds checking is runtime | True: `mls_safe` checks `i >= lp->l` at runtime (`lib/mls_base.c:804`). | The implied comparison is incomplete: Rust's `vec[i]` with a runtime index is **also** a runtime check plus `panic!`. "Compile-time and free" applies only where the optimiser proves the bound. See §6. | Partly — the check can be made once per loop, not once per access. |
| 4 | No data-race freedom | True as a scope statement. | "One of Rust's two headline guarantees" is a category slip: data-race freedom is **not** memory safety. In thread-safe builds `mls` does synchronise its own state. See §7. | Not within C's means; TSan is the answer. |
| 5 | Fragmentation | True as an ecosystem fact. | The characterisation of `mls` as just one more private string type is incomplete: it is a single-header, zero-dependency, integer-handle ABI. See §8. | Yes, by adoption, not by code. |

Two source facts underpin section 5 and are worth stating here because the
synthesis's phrasing obscures them:

- The public header itself documents the policy: **"UAF and invalid handles
  (m > 0) are programmer errors and still exit()"** while bounds, OOM and
  overflow "report externally handleable errors ... via 0/NULL and
  `mls_errno`" (`lib/mls_base.h:174-176`).
- The non-dying probe is `m_is_freed()` / `m_is_valid()`, which route through
  `lock_handle_safe()` → `lock_handle_core(m, w, die=0)` and set `mls_errno`
  instead of dying (`lib/mls_base.c:148-151`; `lib/mls_ext.c:457-475`).

---

## 3. What remains true

Before any rebuttal, the concessions, in one place. They are not retracted
later.

1. **The ergonomics/safety split is correct.** `mls` is genuinely useful as a
   strings / sorted-dict / list / HDF / HTTP / subprocess toolkit. It is not a
   memory-safety mechanism in the Rust sense. The thesis did oversell it by
   attaching a safety claim to an ergonomics library.
2. **It is a tripwire, not a guarantee.** Nothing in `mls` makes a class of
   error *unrepresentable*. It makes a class of error *louder*. Dijkstra's
   lesson — make the error class impossible — is not satisfied by an assertion,
   and the synthesis is right to say so.
3. **The runtime, memory and CPU cost is real.** A handle indirection through a
   table is not free; Rust's iterator-based bounds elimination is genuinely
   cheaper at runtime than a per-access table lookup.
4. **Data races on application data are untouched.** C's concurrency story is
   ThreadSanitizer plus discipline, not the language.
5. **Fragmentation is real.** `mls` is not a standard. The fact that C has
   produced a hundred length-carrying abstractions and standardised none is a
   genuine failure, and no single library fixes it.
6. **It is the author's own library, with no third-party audit.** The
   disclosure in the thesis is honoured, and the absence of external review is
   a real assurance deficit.

The rest of this document argues that the synthesis's strong technical verdicts
(§4.3, §6.1 item 3) overstate these true limitations into false ones.

---

## 4. Refutation A — the 7-bit tag and the 128-free wrap

The synthesis says:

> the use-after-free "protection" is a 7-bit tag ... whose global counter
> increments on every `m_free` and **wraps after 128 frees**, at which point a
> stale handle has roughly a one-in-128 chance of matching a reused slot
> (§4.3)

The mechanism is described correctly. The conclusion drawn from it — "a
tripwire that stops working with predictable frequency" (contra §5) — is wrong
on three counts.

### 4.1 There are two lines of defence, and the deterministic one comes first

When `m_free` releases a handle it sets the slot to a state that cannot be
mistaken for a live one:

```c
lp->data = 0;        /* lib/mls_base.c:734 */
lp->free_hdl = 255;  /* lib/mls_base.c:735 */
```

`lock_handle_core` tests exactly those fields **before** returning a slot:

```c
if (lp->data == NULL ||
    (lp->free_hdl == 255 && (!die || ...))) {
        /* ... */
        ERR ("List %d is being freed", REAL_HDL (m));
}
```

(`lib/mls_base.c:122-135`). So a stale handle to a slot that has been freed
**and not yet recycled** dies deterministically, regardless of the tag. The tag
only matters *after* the slot has been reallocated by a new handle. The
synthesis describes the tag as if it were the whole mechanism; it is the second
line, not the first.

### 4.2 The 1/128 is an expectation, not a bound

The counter is a single process-global variable (`static int UAF_PROTECTION`,
`lib/mls_base.c:21`). The tag is stamped at allocation (`lp->uaf_protection =
UAF_PROTECTION`, `lib/mls_base.c:601`) and the counter advances only on free.
Therefore:

> A stale handle silently matches a recycled slot **iff the number of `m_free`
> calls between its allocation and the slot's re-use is a multiple of 128.**

That makes 1/128 an *average* over a hypothetical uniform distribution, not a
worst case. The worst case is *deterministic*: exactly 128 intervening frees
always produce a match. Stating the average without the deterministic
worst-case is a rhetorical choice, not an analysis.

### 4.3 The temporal-locality argument: 1/128 is not correlated with real bugs

This is the part the synthesis misses entirely, and it is decisive for the
common case.

A use-after-free is a **logic error**: the code frees an object and then,
mistakenly, touches it again. That second touch is overwhelmingly likely to
happen *near* the free in program-execution terms, not a thousand frees later.
The reasoning follows directly from the mechanism in §4.2:

| Intervening `m_free` calls | Detection |
|---|---|
| **0** (touch immediately after free) | **Deterministic.** The slot still has `data = NULL` / `free_hdl = 255`; `lock_handle_core` dies before the tag is even consulted. |
| **1 … 127** (slot already recycled, but bug is still temporally close) | **Deterministic.** Tag mismatch: the counter has not yet completed a full cycle. |
| **exactly 128, 256, …** (many frees between free and touch) | **Miss** (the tag has wrapped onto the same value). This is the only failing case. |

So the collision does not happen at an arbitrary "1 in 128" rate. It happens
only when the number of frees since allocation lands on a multiple of 128. For
a bug where the stale handle is used within a handful of allocations — the
overwhelmingly common shape of a real UAF — the count is nowhere near 128, and
detection is effectively **100 %**.

Rephrased: the detection rate is not `127/128`. It is `~1` for the
temporally-near bug, degrading toward `127/128` only for the *other*, rarer bug
shape: a dangling handle that is retained for a long time and dereferenced much
later, after the slot has been recycled and the counter has wrapped. That long-
lived dangling reference is exactly the case that motivates §4.4.

(A note on arithmetic: the tag space is 128 values, 0…127, because the counter
is masked with `& 0x7f`. The collision probability is **1/128**, sometimes
misquoted as 1/127. And the counter advances on `m_free`, not on allocation,
which is the detail §4.2 turns into the useful condition.)

### 4.4 The fix: widen the tag

The 128-free wrap is a **packing decision**, not a property of handle-based
allocation. The handle is a signed 32-bit `int` — a 24-bit slot index
(`m & 0xffffff`, `lib/mls_internal.h:41`) plus a 7-bit tag. The fix is to spend
more bits on the generation counter.

**Proposal A1 — widen the handle to 64 bits (recommended).**

```c
/* PROPOSAL — not in the current tree. */
typedef int64_t mls_h;

/* 31-bit slot index (keeps the handle positive: bit 63 stays clear),
   32-bit generation counter. */
static inline int64_t  REAL_HDL (int64_t m)
{
    return m & 0x7fffffffLL;                     /* 2^31 slots */
}
static inline uint32_t REAL_UAF (int64_t m)
{
    return (uint32_t)((m >> 31) & 0xffffffffULL); /* 2^32 generations */
}
```

That is 2^32 frees before a wrap and a collision probability of **1/2^32** —
and it *raises* the concurrent-handle ceiling from 2^24 to 2^31 while it does
so. The cost is honest and not small: the handle is the public type across the
entire API (`lib/mls_base.h:124-148`), `struct ls_st.uaf_protection` is a
`char` (`lib/mls_internal.h:34`), and `UAF_PROTECTION` is a `static int`
(`lib/mls_base.c:21`). This is an ABI break with mechanical but non-trivial
propagation. It is not "five lines".

**Proposal A2 — repack within 32 bits (minimal disruption).**

```c
/* PROPOSAL — keep int handles, reallocate the bit budget. */
static inline int REAL_HDL (int m) { return m & 0xffff; }        /* 16-bit index */
static inline int REAL_UAF (int m) { return (m >> 16) & 0x7fff; } /* 15-bit tag */
```

This gives 1/32768 collisions and no ABI break, but caps concurrent handles at
65 536 (down from 16.7 M). It is a bounded improvement with a real ceiling of
its own, not a solution.

The point is not which sketch is chosen. The point is that
**"wraps after 128 frees" is a parameter that can be moved**, and the
synthesis treated it as an immutable property of the approach.

> **Residual still true:** a *long-lived* dangling handle — held far longer
> than 128 frees and then dereferenced — can still slip through until the tag
> is widened. The tag remains probabilistic, never a guarantee; it is a
> tripwire, and only a memory-safe language makes the error unrepresentable.

---

## 5. Refutation B — `exit(1)` and the "remote DoS primitive"

The synthesis says a stale handle "does not return an error — it calls
`exit(1)`, which in a network daemon is a **remote denial-of-service
primitive**, a different CVE class, not the absence of one" (§4.3). Three
things are wrong with this.

### 5.1 The `_SAFE` correction: the synthesis repeats a factual error

The antithesis, which the synthesis adopts, claims that "the `_SAFE` accessors
return an error instead, which merely reintroduces the 'did you remember to
check?' discipline" (contra §5). That is **false for use-after-free**.

`mls_safe` calls the dying lock:

```c
void *mls_safe (int m, size_t i)
{
    if (m <= 0) { mls_errno = MLS_EINVAL; return NULL; }
    lst_t lp = lock_handle (m, 0);   /* die = 1 -> ERR() -> exit(1) on UAF */
    if (i >= lp->l) { mls_errno = MLS_EBOUNDS; unlock_handle (lp); return NULL; }
    ...
}
```

(`lib/mls_base.c:797-812`). `lock_handle` is `lock_handle_core(m, write, 1)`
(`lib/mls_base.c:139`), so a UAF mismatch still dies. The `_safe` variants
handle **bounds, OOM and overflow**; they do **not** handle UAF. And the public
header says so in plain text: *"UAF and invalid handles (m > 0) are programmer
errors and still exit()"* (`lib/mls_base.h:174-176`).

This is a deliberate design choice, and a defensible one: an out-of-bounds
*index* is an externally handleable condition ("you asked for element 10 of 5"),
whereas a use-after-free means the program has already lost track of its own
lifetimes and continuing is arbitrary. The synthesis presents the UAF exit as
the *default* path versus a safe path; there is no safe path for UAF, by
design.

(For completeness: the truly non-dying probe is `m_is_freed()` /
`m_is_valid()`, which use `lock_handle_safe` → `die = 0` and set `mls_errno`
(`lib/mls_base.c:148-151`, `lib/mls_ext.c:457-475`). That is the check to call
before touching a handle whose state is genuinely in doubt. And `h <= 0` is
returned as an error rather than dying; only `h > 0` invalid handles are fatal.)

### 5.2 The comparison is against the wrong baseline

`exit(1)` is not a regression relative to the alternative. It is the
**downgrade** of the same bug. Consider the same use-after-free without any
tripwire — which is what raw C gives you:

| | Without `mls` | With `mls` |
|---|---|---|
| Behaviour | reads freed memory | prints a diagnostic, `exit(1)` |
| Bug class | silent corruption; potential remote code execution | deterministic crash |
| CIA impact | confidentiality + integrity + availability | availability |
| Detectability | may be silent for years | immediate, loud, restartable |

The synthesis calls `exit(1)` "a different CVE class, not the absence of one".
A different class, yes — a strictly *less severe* one. Trading a potential RCE
for a clean crash is the entire point of fail-fast. Calling the downgrade "not
the absence of [a CVE]" is true and irrelevant: the alternative was not "no
CVE", it was "a worse CVE".

### 5.3 Rust has the same runtime failure mode

Rust's indexed access does not offer a different behaviour here. `vec[i]` with
a runtime `i` performs a runtime bounds check and `panic!`s on failure; an
uncaught panic aborts the process. A network daemon written in Rust that
indexes a vector out of range has the same availability outcome as one that
trips an `mls` UAF check. The asymmetry the synthesis implies — Rust safe, C a
DoS primitive — is not there for indexed access.

### 5.4 The fix: a configurable fatal policy

The legitimate kernel of the criticism is not *that* `mls` fails fast; it is
that **the failure policy is hard-wired**. That is easy to fix without giving
up fail-fast by default.

```c
/* PROPOSAL — one hook, routed through BOTH exit sites. */
typedef void (*mls_fatal_fn) (int err, const char *file, int line,
                              const char *func, const char *msg);

extern mls_fatal_fn mls_fatal_handler;  /* NULL = current behaviour */

/* lib/mls_base.c:274 site */
void deb_err (int line, const char *file, const char *function,
              const char *format, ...)
{
    /* ... format buf ... */
    if (mls_fatal_handler) {
        mls_fatal_handler (mls_errno, file, line, function, buf);
        abort ();          /* the hook must not return */
    }
    fprintf (stderr, "\n[mls error] %s:%d %s(): %s\n", file, line, function, buf);
    exit (1);
}

/* lib/mls_base.c:345 site, used by mls_must() */
void _mls_die (int line, const char *file, const char *function)
{
    if (mls_fatal_handler) {
        mls_fatal_handler (mls_errno, file, line, function, mls_errmsg (mls_errno));
        abort ();
    }
    /* ... existing print ... */
    exit (1);
}
```

Both exit sites must be routed, or `mls_must()` would bypass a daemon's policy.
The `noreturn` contract matters: if the hook returns, `deb_err`/`_mls_die`
must still terminate (hence the `abort()` backstop), or the fail-fast guarantee
is lost. A daemon can then choose its own policy:

```c
/* PROPOSAL — daemon-side policy: log and crash the worker, keep the supervisor. */
static void daemon_fatal (int err, const char *file, int line,
                          const char *func, const char *msg)
{
    syslog (LOG_CRIT, "mls fatal: %s (%s:%d %s) err=%d",
            msg, file, line, func, err);
    abort ();   /* core dump for forensics; systemd restarts the unit */
}
...
mls_fatal_handler = daemon_fatal;
```

For a worker-isolated daemon, the hook can instead `siglongjmp()` to the worker
boundary, so one request dies and the process lives. Either way, the *decision*
moves to the application, while the library default stays fail-fast for tools.

> **Residual still true:** even with a hook, `mls` ships no per-thread recovery
> out of the box; the default is still process termination, and a program that
> ignores `m_is_freed()` / `_safe` errors will still die where it could have
> recovered. Fail-fast protects the memory-safety boundary; it does not keep
> the service up.

---

## 6. Refutation C — runtime bounds checking versus Rust

The synthesis says bounds checking "is per-access and at runtime, through a
table lookup, whereas Rust's is compile-time and free at runtime" (§4.3).
The first half is true (`lib/mls_base.c:804`); the second half is only half
true, and the comparison is drawn asymmetrically.

### 6.1 Rust's indexed access is also a runtime check

Rust's compile-time bound elimination applies where the optimiser (or the
borrow checker, for iterators) can *prove* the index in range — `for x in
&v`, slice patterns, and the like. For `v[i]` where `i` is a runtime value,
Rust emits a bounds check and calls `panic!` (index out of bounds) when it
fails. The check is not free, and the failure is not compile-time. So the
honest comparison is:

| Access | C + `mls` | Rust |
|---|---|---|
| Indexed with runtime `i` | runtime table lookup + check, abort/error | runtime check, `panic!` |
| Proven-in-bounds loop | one check per access; loop forms avoid it | iterator elides the check |

The synthesis's "compile-time and free" describes the *iterator* column for
Rust while comparing it to the *indexed* column for C. That is not a
like-for-like comparison.

### 6.2 The C analogue is approximate, not equivalent

The plan for this section originally claimed `mls` "already provides the C
analogue" of checked iterator access. Checking the source, that is too strong
and is corrected here.

- `m_peek()` — the engine behind `_UNCHECKED` — calls the **dying**
  `lock_handle` (`lib/mls_base.c:956-964`), and `lst_peek()` still `ERR()`s
  when `i >= l->max` (`lib/mls_base.c:439-443`). So `_UNCHECKED` removes the
  *logical-length* check but keeps a *capacity* check, and can still abort.
- The compiler cannot prove `i < m_len(h)` for you; nothing in C can.

So the correct statement is: both languages pay a runtime check on indexed
access, and Rust additionally elides the check for iterator/slice loops. In C
you can approximate the ergonomics with a single validated loop:

```c
/* PROPOSAL — check once, then walk the known-good buffer. */
size_t n = m_len (h);
for (size_t i = 0; i < n; i++) {
    int *p = (int *)m_buf (h);   /* one lookup for the loop */
    /* use p[i] — bounded by the loop condition, no per-element call */
}
```

That is a pattern, not a guarantee: it relies on the loop condition staying
honest. The synthesis is right that Rust gives a *stronger* form. It is wrong
to imply `mls` gives *nothing* and that Rust's check is universally free.

> **Residual still true:** C cannot turn a runtime-proven bound into a
> compile-time one. `mls` gives you a checked dynamic buffer; it does not give
> you Rust's iterator-level proof, and the per-access table lookup is a real
> runtime cost that Rust's proven loops avoid.

---

## 7. Refutation D — data-race freedom

The synthesis says `mls` "does not attempt data-race freedom at all — one of
Rust's two headline guarantees" (§4.3), and that the thesis's admission that it
"does not, by itself, prevent every possible concurrency race" actually means it
"prevents **none** of them" (§4.3).

Two corrections.

### 7.1 Data-race freedom is not memory safety

Rust's two headline guarantees are memory safety *and* data-race freedom. They
are related but distinct. `mls` is not a memory-safety mechanism, and it is not
a data-race-freedom mechanism either — but calling the latter "one of Rust's
two headline guarantees" and then measuring `mls` against it conflates the
library's stated purpose. `mls` is an ergonomics library with a development-time
tripwire. Judging it for not implementing a compile-time concurrency type
system is judging the wrong artifact.

### 7.2 In thread-safe builds, `mls` does synchronise its own state

The library's actual concurrency behaviour should be stated precisely, because
the synthesis's "none" is not accurate.

- The master handle table is guarded by `MLS_MASTER_LOCK()` and each handle
  carries its own `pthread_rwlock_t` (`lib/mls_internal.h:10-18`); readers and
  writers on the *same* handle are serialised, and operations on *different*
  handles do not corrupt the table.
- **Caveat:** this holds only when built with `MLS_THREAD_SAFE=1`. When it is
  off, the macros are the empty statement (`lib/mls_internal.h:20-27`). The
  CMake default is ON (`CMakeLists.txt:12`), but the claim must be "internally
  synchronised when built thread-safe", not unconditional.
- **Caveat 2:** `mls_lock_handle` (`lib/mls_base.c:588`) forwards to the dying
  `lock_handle`, so a foreground lock converts a lifetime bug into an exit.
  Iteration paths (for example `m_foreach`, which releases the read lock between
  elements) must be checked before claiming internal race-freedom for a given
  access pattern.

What `mls` does **not** do is prevent application-level races: two handles
referencing the same external data, or a handle shared across threads with a
user-maintained invariant that spans operations. That is the same gap every C
library has, and the answer is ThreadSanitizer plus design, not a library
change.

> **Residual still true:** C has no language-level race freedom. `mls`
> serialises its own internal state but cannot prevent races in the data the
> program builds around it. Greenfield concurrent code where races are the
> dominant risk is still a case for Rust; existing C code is a case for TSan.

---

## 8. Refutation E — fragmentation and "build your own"

This is the strongest criticism, and it is not fully refutable. The honest
position is a partial concession followed by a narrower claim.

### 8.1 The concession

C has spent fifty years producing length-carrying abstractions without
standardising one. `strlcpy`/`strlcat`, `sds`, GLib's `GString`, `bstrlib`,
`mls`, and one private buffer per serious project share no common API. Every
library boundary becomes a conversion boundary, and none of them is a
substrate. That is a real failure and `mls` does not fix it by existing.

### 8.2 The narrower refutation

The criticism's list flattens four different things into one:

- `strlcpy`/`strlcat` are **two functions**, not an abstraction. They do not
  know a buffer's capacity and cannot carry a length.
- `sds` and `GString` are **allocator- and type-bound**; they are not drop-in
  across a C project with a different allocation strategy.
- `bstrlib` is a **library with its own conventions**, headers, and boundary
  types.
- `mls` is a **single-header** (via `generate_amalgam.py`) **zero-dependency**
  library whose entire interface is an **`int` handle** — the most portable
  shape a C abstraction can take, and one that crosses an ABI boundary as a
  plain integer.

The fragmentation argument is strongest when abstractions are *incompatible*.
An integer handle and a single self-contained header lower the cost of
adoption; they do not add a dialect that must be converted. That is a real, if
local, mitigation. It is not a standard, and this document does not pretend it
is.

The synthesis's own conclusion is therefore accepted with one adjustment. Its
§8 says `mls` is valuable as ergonomics, not as safety. Agreed. The adjustment
is that the *ergonomic* value is not merely "another private type"; its
distribution shape is specifically aimed at the standardisation problem, and
that is worth stating even while conceding the problem is unsolved.

> **Residual still true:** `mls` is not a standard and never will be by
> declaration. "Build your own" remains an ergonomic and local answer, not a
> safety or global one. The remedy for a standardisation failure is
> standardisation — which is what Rust, Go and Zig did and C did not.

---

## 9. Adoption is not the measure

There is one objection the synthesis implies but never states outright, and it
is the one a working engineer actually raises: **no widely-deployed system uses
`mls`.** Linux, SQLite, Redis, PostgreSQL, nginx — none of them links it. Each
built its own private buffer or handle layer, in its own dialect, on its own
clock. Read uncharitably, that looks like evidence against the library. Read
carefully, it is evidence against the *framing*, not the library.

**The fact cuts the other way.** That every serious, long-lived C project built
its own length-carrying layer is evidence that the abstraction is *necessary* —
the thesis's point — not that `mls` is worthless. The antithesis reads the same
fact as fragmentation; the thesis reads it as "the industry already knows
`strcpy` is not the answer." Both are true, and both can be true at once:
fragmentation is a *collective* failure (accepted in §8.1), while necessity is
an *individual* fact. A collective failure does not cancel an individual
cost/benefit.

**The adoption cost is small, and this one is measurable.** The unmeasured
magnitude in this debate — the thing the synthesis rightly refuses to accept on
assertion — is the cost of *changing languages*: retraining, dual toolchains,
FFI, ecosystem churn. None of it applies to `mls`. Adopting it inside an
existing C project is one amalgam header (`mls_core.h` or `mls_full.h`), one
`#define`, one `#include`, and the link line the project already carries
(`-lpthread -lm -ldl`). No runtime, no garbage collector, no build-system
change, no retraining — it is C, with a handful of primitives. That cost is
concrete and checkable, which is more than the migration cost ever gets.

**The benefit is intrinsic, not networked.** A library's value to *you* does
not depend on how many strangers already adopted it. The bounds check, the
use-after-free tripwire, and the deterministic teardown catch *your* bugs in
*your* process, and the saving is paid to the adopter directly and immediately.
That is the opposite of a *standard*, whose value only materialises through
network effects and interoperability. For an ergonomics library — which is what
§8 establishes `mls` is — the right question is "what does it cost me, and what
does it save me," not "who else is already using it."

Adoption is a lagging indicator and a network-effect metric. It is the correct
metric for a platform, where value scales with the number of other users. It is
the wrong metric for a drop-in library whose benefit is intrinsic and whose
cost is one header. To dismiss `mls` because Redis did not use it is to judge a
safety device by how many people already own one.

> **Residual still true:** none of this makes `mls` a standard, earns it
> third-party review, or repairs the ecosystem fragmentation §8 names. The
> *adoption* cost is negligible; the *standardisation* cost is not, and remains
> unsolved.

---

## 10. How C is sensibly used

The synthesis's decision procedure (§7) classifies components by threat model
and says "if background: C with an ergonomics library". This section makes that
concrete. It is the constructive half of the document: not what `mls` fails to
be, but what disciplined C + `mls` looks like when the threat model is
background.

### 10.1 Checklist

1. **State the threat model first.** Hostile input across a trust boundary →
   memory-safe language; local/trusted/closed network → C is defensible. This
   classification is a property of the deployment, not the file.
2. **Route every buffer through `s_*` / `m_*`.** No `strcpy`, no `strncpy` at
   any boundary. The length lives in the handle.
3. **Use `_safe` + `mls_errno` at trust boundaries.** Bounds, OOM and overflow
   are recoverable; check them where input is untrusted.
4. **Use `m_is_freed()` before touching a handle whose state is in doubt**
   (ownership hand-offs, caches, callbacks). This is the non-dying probe.
5. **Never hold an `mls()`/`m_buf()` pointer across a resize.** Re-resolve the
   handle; `realloc` can move the buffer.
6. **Fail-fast is a feature for tools; make it a policy for daemons.** Tools
   want `exit(1)` with a diagnostic. Daemons install `mls_fatal_handler`
   (Proposal B) and isolate the worker.
7. **Run the tools that catch what the tripwire cannot.** ASan/UBSan in debug,
   TSan for shared state, fuzzing for parsers. These compose with `mls`; they
   are not an alternative to it.
8. **Bracket work with allocation asserts.** `m_count_allocated() == 0` at
   shutdown is a leak check that works in a plain release build, without a
   sanitizer.
9. **Reduce surface.** Fewer features, fewer dependencies, smaller boundary.
   This is language-independent and was as important to `sudo-rs` as the
   language change.

### 10.2 A worked example

A small process-watcher in the style of `examples/procwatch`, showing the
points above in one place. It is illustrative, not a complete program.

```c
#define MLS_FULL_IMPLEMENTATION   /* exactly one TU; before the first include */
#include "mls_full.h"
#include <assert.h>
#include <signal.h>

static volatile sig_atomic_t running = 1;
static void on_term (int sig) { (void)sig; running = 0; }

/* Fail-fast is right for a tool: a loud crash beats silent corruption. */
/* A daemon would instead install mls_fatal_handler here (Proposal B). */

int main (void)
{
    m_init ();
    signal (SIGINT, on_term);
    signal (SIGTERM, on_term);

    int seen = m_alloc (0, sizeof (char *), MFREE_STR);  /* sorted name set */

    while (running) {
        int entries = m_dir_list ("/proc");         /* no raw opendir */
        if (entries <= 0)
            break;

        for (size_t i = 0; i < (size_t)m_len (entries); i++) {
            const char *name = STR (entries, i);
            if (!isdigit ((unsigned char)name[0]))
                continue;

            /* dedup + membership in one O(log n) call */
            int key_h = s_dup (name);
            if (m_lookup_mstr (seen, key_h, 1 /* NOT_INSERT */) >= 0) {
                m_free (key_h);
                continue;   /* already seen */
            }
            m_lookup_mstr (seen, key_h, 0);         /* insert, kept sorted */
            m_free (key_h);

            /* read the status file with the comfort primitive, not fgets */
            int path = s_printf (0, -1, "/proc/%s/status", name);
            int text = m_str_from_file (m_str (path));
            m_free (path);
            if (text > 0) {
                /* parse, log, ... */
                m_free (text);
            }
        }
        m_free (entries);
        usleep (200000);
    }

    m_free (seen);
    assert (m_count_allocated () == 0);   /* leak check, release build */
    m_destruct ();
    return 0;
}
```

What this demonstrates, point by point:

- **No raw pointers to library memory** are held across calls; only handles.
- **Bounds-safe access** via `STR(entries, i)` inside the loop condition.
- **Sorted membership** via `m_lookup_mstr`, O(log n), no hand-rolled
  `qsort` + search.
- **Deterministic teardown** — `m_free` on each handle, then a leak assert.
- **Fail-fast** is the default and correct here; a daemon would swap in the
  Proposal B hook without touching the rest.
- **Threat model**: reads `/proc` on a local host, no hostile parser, no
  privilege boundary — the background case where the synthesis agrees C is
  defensible.

That is the sensible use of C that the synthesis's §8 gestures at but does not
show: not "C plus a safety claim", but C plus *ergonomic primitives*,
*tripwires*, and *sanitizers*, applied to a component whose risk is background.

---

## 11. Conclusion and solutions summary

### 11.1 What was checked, and what changed

| Synthesis claim | Verdict | Basis |
|---|---|---|
| 7-bit tag wraps at 128; ~1/128 collision | **Incomplete.** Mechanism correct; the deterministic first line, the expectation-vs-bound distinction, and temporal locality are omitted. | §4; `mls_base.c:122-135, 601, 734-735, 739` |
| `exit(1)` is a remote DoS primitive, "a different CVE class" | **Reframed.** It is the downgrade of a worse bug class, and Rust's indexed access panics too. | §5; `mls_base.c:274, 797-812` |
| `_SAFE` returns an error instead | **False for UAF.** They exit; only bounds/OOM/overflow are returnable. | §5.1; `mls_base.h:174-176` |
| Bounds check runtime, Rust compile-time/free | **Half true.** Indexed access is a runtime check in both; only proven loops are compile-time. | §6 |
| No data-race freedom, "prevents none" | **Imprecise.** Internal state is synchronised in thread-safe builds; application races are untouched. | §7; `mls_internal.h:10-27`, `CMakeLists.txt:12` |
| Fragmentation / "build your own" | **Accepted, narrowed.** Real failure; `mls`'s shape lowers but does not remove the cost. | §8 |

### 11.2 The fixes, as proposals

Nothing below is implemented. Each is a sketch for a future change, with its
honest cost.

| Fix | Effect | Cost | Status |
|---|---|---|---|
| **A1** 64-bit handles | collision 1/2³², handle cap 2³¹ | ABI/type break across the public API | proposal |
| **A2** repack within 32 bits | collision 1/32768 | caps concurrent handles at 65 536 | proposal |
| **B** `mls_fatal_handler` through both exit sites | configurable fatal policy for daemons | small public API addition | proposal |
| **C** checked-once loop pattern | matches iterator ergonomics approximately | documentation, not a guarantee | pattern exists |
| **D** `_safe` vs dying documentation | removes the misconception the synthesis still carries | none | proposed |

### 11.3 Final position

The synthesis got the big thing right. `mls` is an ergonomics library with a
development-time tripwire; it is not memory safety, and the thesis was wrong to
lean on it for a safety claim. That conclusion is not withdrawn.

What *is* withdrawn is the specific technical verdict that made the conclusion
look stronger than it is. The tag is a tunable parameter, not a mechanism that
"stops working". `exit(1)` is a downgrade of a bug class, not an escalation.
The `_SAFE` accessors were misdescribed. Runtime indexed bounds checks are not
a C-vs-Rust asymmetry. And in thread-safe builds the library *does* manage its
own concurrency, even though it cannot manage the program's.

The corrected position is the synthesis's own, stated more precisely:

> **C can be made *productive* — strings, dictionaries, configuration, HTTP,
> and subprocesses behind an integer-handle ABI, with development-time
> tripwires and fail-fast degradation — but it cannot be made *memory-safe*.
> Choose it for background risk, with the checklist of §10, and measure the
> rest.**

That is a smaller claim than the thesis made and a fairer one than the
antithesis allowed. It is also falsifiable: every technical assertion above has
a `file:line`, and a reader who distrusts the prose can build and run the
library and check each one.
