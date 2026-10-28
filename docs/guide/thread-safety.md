# MLS Thread-Safety Model

`mls` has one optional build mode for use from multiple threads:
compile with `MLS_THREAD_SAFE=1` (the default on Unix) and link `-lpthread`.
With `MLS_THREAD_SAFE=0` every lock helper below compiles to a no-op and no
pthreads are needed.

The model is deliberately simple: **one global recursive mutex protects
everything; there are no per-handle locks.**

## Locking model

| Shared state | Protected by |
|---|---|
| Master list `ML` (handle slots) | `ml_lock` |
| Free list (slot `0`) + `UAF_PROTECTION` | `ml_lock` |
| Free-handler table `FH` | normal handle path (which takes `ml_lock`) |
| Constant-string map (`s_ccstr`) | `cs_map_lock` (separate) |
| Flask route registry | `flask_lock` (separate, `m_flask.c`) |

`ml_lock` is held for the **whole duration of every operation**, from handle
resolution until the caller releases it. That means:

- two threads calling `mls`/`m_put`/`m_len`/... are safe but **serialized**;
- `ML` can never be `realloc`'d (moved) while a thread still holds a resolved
  slot pointer; this removes the use-after-free that the old two-level design
  had when `ML` grew under a held `lst_t`;
- no lock-order inversion is possible (there is only one lock).

`ml_lock` is a **recursive** mutex because `mls` re-enters itself while an
operation is in progress (`m_alloc` → `m_len(FH)`, `m_free` → `mls(FH)`,
in-place slice, ...). A single thread may re-acquire it; other threads block.
It is initialised once via `pthread_once` from `ml_lock_init()`, which
`MLS_MASTER_LOCK()` calls before every acquisition (so an early lock is safe).

## Contract: handles are not shared between threads

The locks make internal state consistent; they do **not** make it safe to use
one handle from two threads at once. The supported model is: **a handle is
owned by one thread at a time.** Hand ownership between threads, don't let two
threads touch the same handle.

This is the same reality the raw-pointer APIs impose:

```c
char *s = m_str(h);          // lock released before this returns
// ... another thread must not resize/free h here ...
printf("%s\n", s);
```

`m_str`, `mls`, `m_buf`, `m_peek`, `m_add`, `m_pop` all return raw pointers
after releasing the lock. A pointer is only valid while no other thread can
resize or free the same handle. If you need concurrent access to one buffer,
hand it to a C library under your own synchronisation, or copy it out.

Because of that, locking individual handle data could never provide a real
guarantee anyway. Restricting the lock to the central table is the honest,
simpler design.

## How operations lock

`lock_handle_core()`:

1. `MLS_MASTER_LOCK()` (takes the recursive `ml_lock`),
2. validate the handle (range, UAF pattern, `data`, `free_hdl`),
3. return the `lst_t` **with the lock still held**.

`unlock_handle()` simply releases `ml_lock`. Every `lock_handle*()` call must
be paired with an `unlock_handle()`.

All the public operations (`m_put`, `m_write`, `m_setlen`, `m_free`, `mls`,
`m_len`, `m_next`, `m_read`, the `_safe` variants, ...) keep their existing
lock/unlock pairs, so they automatically run under the global lock. The `write`
argument of `lock_handle()` is now ignored (there is no read/write split).

`m_is_freed()` uses `lock_handle_safe()` and, like the others, answers under
`ml_lock`.

`m_free()` still releases the lock before calling a user free handler (so user
code does not run under the global lock) and re-acquires it afterwards. The
thread-local `freeing_handle` lets that handler iterate the handle currently
being freed (marked `255`) without being rejected as “being freed”.

## Build modes

```sh
make -C tests thread_safe=1     # -DMLS_THREAD_SAFE=1 -lpthread
make -C tests thread_safe=0     # -DMLS_THREAD_SAFE=0, no pthreads
```

When the macro is `0`, `MLS_MASTER_LOCK/UNLOCK`, `CS_MAP_LOCK/UNLOCK` and the
`lock_handle`/`unlock_handle` bodies become no-ops; the same sources build
single-threaded without linking pthreads.

## Caveats

- `s_cstr` / `s_ccstr` share a process-wide constant-string map with a
  64-bit pointer-truncation race; do not call them concurrently.
- Ring-buffer helpers need an external mutex for concurrent use.
- `m_foreach` calls `m_next`, which locks per element; no lock is held across
  the iteration, so a concurrent writer can still change the list.
- Debug builds (`MLS_DEBUG`) keep a post-mortem/debug list; multi-threaded
  debug builds are not a supported combination.

## Tests

Threaded builds are exercised by `tests/test_thread_safety.c`,
`tests/test_thread_safe_fuzzy.c` and (under ThreadSanitizer via
`make check-tsan`) `tests/test_tsan_races.c`. Because the model is
“safe but serialized”, the stress tests verify correctness, not parallel
throughput.
