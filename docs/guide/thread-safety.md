# MLS Thread-Safe Porting Notes

This document explains the thread-safety changes made to `mls.c`, why they
were needed, and which API boundaries are now protected.

## Goal

The port follows this locking model:

| Shared state | Lock |
|---|---|
| Master List (`ML`) | `pthread_mutex_t ml_lock` |
| Free List (handle slot `0`) | also under `ml_lock` |
| Each handle/list | own `pthread_rwlock_t` |
| Reads (`mls`, `m_read`, `m_len`, etc.) | shared/read lock |
| Writes (`m_put`, `m_write`, `m_resize`, etc.) | exclusive/write lock |

The main problem was that the global master handle table and the individual
list buffers could be changed by multiple threads with no synchronization.
That made these operations unsafe:

- Creating a handle while another thread also creates or frees one.
- Reusing a free handle slot while another thread reads the master list.
- Resizing a list while another thread reads from it.
- Appending or deleting elements while another thread observes `l`, `max`, or
  `data`.
- Freeing a list while another thread resolves the same handle.

## Build Modes

Thread safety is now opt-in at compile time:

```sh
make -C tests thread_safe=1
```

That define adds `-DMLS_THREAD_SAFE` and links `-lpthread`. If
`MLS_THREAD_SAFE` is not defined, the same MLS API builds with the locking
helpers compiled to no-ops. This keeps simple single-threaded applications from
needing to include or link POSIX threads.

## Header Change

File: `lib/mls.h`

`pthread.h` is included only when `MLS_THREAD_SAFE` is defined:

```c
#ifdef MLS_THREAD_SAFE
#include <pthread.h>
typedef pthread_rwlock_t mls_rwlock_t;
#else
typedef void mls_rwlock_t;
#endif
```

Each list/handle now stores its own lock:

```c
mls_rwlock_t *lock;
```

The lock is a pointer, not an embedded `pthread_rwlock_t`, because `ML.data` can
be resized with `realloc`. Embedding a pthread lock directly inside a struct
that may be moved or byte-copied is unsafe. Keeping the lock separately
allocated gives it a stable address for the lifetime of the handle slot.

## Global MLS Lock

File: `lib/mls.c`

Two new globals were added:

```c
static pthread_mutex_t ml_lock = PTHREAD_MUTEX_INITIALIZER;
static __thread int freeing_handle = 0;
```

`ml_lock` protects the master list metadata and slot `0`, which is the Free
List. All operations that allocate a handle, reuse a freed slot, return a slot
to the free list, or read the master list range now go through this mutex.

`freeing_handle` is thread-local state used during `m_free()`. It allows the
free handler for a handle to iterate that same handle after it has been marked
as "being freed", while other threads are still blocked from using it.

## Handle Lock Helpers

File: `lib/mls.c`

The following helpers were added:

- `init_handle_lock(lst_t lp)`
- `destroy_handle_lock(lst_t lp)`
- `get_list_locked(int m)`
- `lock_handle(int m, int write)`
- `unlock_handle(lst_t lp)`

`init_handle_lock()` lazily allocates and initializes the per-handle
`pthread_rwlock_t` when `MLS_THREAD_SAFE` is enabled. In the non-threaded build
it is a no-op.

`destroy_handle_lock()` destroys and frees a handle lock during `m_destruct()`
when locks are enabled. In the non-threaded build it is a no-op.

`get_list_locked()` is the old handle validation logic, but it must be called
while `ml_lock` is already held. It validates:

- the real handle index is inside `ML`,
- the list has live data,
- the UAF protection byte matches,
- the handle is not currently being freed by another thread.

`lock_handle()` is the main lock entry point. It takes `ml_lock`, resolves the
handle safely, then takes either the handle read lock or write lock. It releases
`ml_lock` after the handle lock is acquired.

That order matters:

1. `ml_lock`
2. handle `rwlock`
3. release `ml_lock`
4. operate on the handle
5. release handle `rwlock`

This prevents the master list from being resized or a handle slot from being
reused while a thread is resolving the handle.

## Master List and Free List Changes

File: `lib/mls.c`

`new_list()` now locks `ml_lock` before calling `get_free_hdl()`. This protects
both possible allocation paths:

- popping a reused slot from Free List slot `0`,
- growing `ML` with `lst_new(&ML, 1)`.

It also initializes the per-handle lock before publishing the new list metadata.

`m_free()` now returns the real handle number to Free List slot `0` under
`ml_lock`:

```c
pthread_mutex_lock (&ml_lock);
lst_put ((lst_t)ML.data, &realh);
UAF_PROTECTION = (UAF_PROTECTION + 1) & 0x7f;
pthread_mutex_unlock (&ml_lock);
```

That protects the free-list push and the global UAF protection counter update.

`m_init()` now initializes `ML` and slot `0` under `ml_lock`, including the lock
for the Free List handle.

`m_destruct()` now runs under `ml_lock`, frees live buffers, destroys per-handle
locks, releases `ML.data`, and resets `UAF_PROTECTION`.

## Read-Locked Operations

These functions now take a shared/read lock on the target handle:

- `m_free_hdl()`
- `m_len()`
- `mls()`
- `m_next()`
- `m_bufsize()`
- `m_peek()`
- `m_read()`
- `m_width()`

Why: these functions read handle metadata or buffer content. A shared lock lets
multiple readers run at the same time, but blocks concurrent writers that could
change `l`, `max`, `w`, `free_hdl`, or `data`.

Example:

```c
lst_t lp = lock_handle (m, 0);
int len = lp->l;
unlock_handle (lp);
```

`m_is_freed()` is slightly different. It only needs `ml_lock` because it checks
the master table slot and does not need to inspect or copy list content under a
handle lock.

## Write-Locked Operations

These functions now take an exclusive/write lock on the target handle:

- `set_free_protection()`
- `unset_free_protection()`
- `m_set_data()`
- `m_free()`
- `m_new()`
- `m_add()`
- `m_put()`
- `m_setlen()`
- `m_write()`
- `m_clear()`
- `m_del()`
- `m_pop()`
- `m_ins()`
- `m_resize()`
- `m_remove()`
- `m_bzero()`
- `m_putc()`
- `m_puti()`

Why: these functions mutate list metadata, mutate list memory, can trigger
`realloc`, or alter handle state. They must exclude both readers and other
writers while the mutation is in progress.

`m_putc()` and `m_puti()` were changed to avoid calling `m_add()` and then
writing through a returned pointer after the lock was released. They now append
and assign while the write lock is still held.

## `m_free()` Details

`m_free()` needed special handling because free handlers can inspect or recurse
through MLS handles.

The new flow is:

1. Take the target handle write lock.
2. Read `free_hdl` and decide whether a custom free handler is needed.
3. For custom handlers, release the target lock before looking up the handler in
   `FH`. This avoids holding one handle lock while calling into another MLS API.
4. Re-lock the target handle and verify `free_hdl` did not change.
5. Mark the handle as `255` (`NOHDL` / being freed).
6. Release the lock and call the free handler with `freeing_handle` set.
7. Re-lock the target handle.
8. Free the backing buffer if it is MLS-owned.
9. Set `data = 0` and `free_hdl = 255`.
10. Push the real handle id into Free List slot `0` under `ml_lock`.
11. Bump the UAF protection pattern under `ml_lock`.

The important behavior is that other threads cannot use a handle once it is
marked as being freed. The free handler for that same handle is allowed to
iterate it because `freeing_handle` is thread-local and identifies the current
free operation.

## `exported_get_list()` Caveat

`exported_get_list()` still returns a raw `lst_t`:

```c
lst_t exported_get_list (int r) { return get_list (r); }
```

This function is used by lower-level helpers such as ring-buffer code in
`m_tool.c`. It now resolves the handle under `ml_lock`, but it does not keep the
handle locked after returning.

That preserves the existing API, but it is not a complete lifetime guarantee for
raw `lst_t` users. Code that mutates a returned `lst_t` directly is still
outside the new read/write lock discipline unless it is ported to use public MLS
functions or a future explicit lock API.

## Raw Pointer Caveat

Functions like `mls()`, `m_peek()`, `m_add()`, and `m_pop()` return raw pointers.
The lookup itself is now protected, but the lock is released before the pointer
is returned to the caller.

That means the implementation is safer for handle resolution and internal
metadata consistency, but raw pointer lifetime is still a caller-side contract.
If one thread keeps a pointer returned by `mls()` while another thread resizes or
frees the same handle, the pointer can still become stale.

A fully strict thread-safe API would need explicit scoped access functions such
as:

```c
const void *m_read_lock(int h, int i, m_lock_guard_t *guard);
void m_unlock(m_lock_guard_t *guard);
```

or copy-based APIs only.

## Tests Added

File: `tests/test_mls_core.c`

Two pthread tests were added and are compiled when `MLS_THREAD_SAFE` is enabled.

`test_m_put_threaded()`:

- creates one integer handle,
- starts 8 threads,
- each thread calls `m_puti()` 1000 times,
- checks the final length is `8000`.

This verifies that concurrent appends to the same handle serialize correctly and
do not lose length updates or corrupt realloc state.

`test_m_alloc_free_threaded()`:

- starts 8 threads,
- each thread repeatedly allocates a small handle, appends one integer, and
  frees it,
- stresses `ML`, Free List slot `0`, handle reuse, and UAF counter updates.

This verifies the global `ml_lock` path and handle slot reuse path.

## Verification

The changes were built and tested with:

```sh
make -C tests test_mls_core.exed OBJ=d
ASAN_OPTIONS=detect_leaks=0 ./test_mls_core.exed
make -C tests test_mls_core.exed test_thread_safe_fuzzy.exed OBJ=d thread_safe=1
ASAN_OPTIONS=detect_leaks=0 ./test_mls_core.exed
ASAN_OPTIONS=detect_leaks=0 ./test_thread_safe_fuzzy.exed
gcc -D_GNU_SOURCE -Ilib -x c - \
  lib/mls.c lib/m_tool.c lib/m_table.c lib/m_http.c lib/m_hdf.c \
  lib/m_http_server.c lib/m_flask.c lib/m_extra.c -ldl -lm \
  -o /tmp/mls_no_pthread_check <<'EOF'
#include "mls.h"
int main(void) {
    int h;
    m_init();
    h = m_alloc(0, sizeof(int), MFREE);
    m_puti(h, 42);
    return m_len(h) == 1 && INT(h, 0) == 42 ? 0 : 1;
}
EOF
nm -u /tmp/mls_no_pthread_check | rg 'pthread|thrd_' || true
make -C tests ALL OBJ=d
git diff --check
```

`test_mls_core.exed` passed all 13 tests, including the 2 new threaded tests.
The full debug test build completed, and `git diff --check` reported no
whitespace errors.

## Summary

The port makes MLS internally synchronized at the two important levels:

- global handle table and Free List operations are serialized by `ml_lock`,
- per-handle reads and writes are coordinated by `pthread_rwlock_t` when
  `MLS_THREAD_SAFE` is enabled.

This protects the core MLS metadata and buffer mutation paths without changing
the public API. The remaining sharp edge is the existing raw-pointer API: it is
now protected during lookup, but it cannot guarantee pointer lifetime after the
function returns.
