# mdb_srv memory report — fork-per-request vs spawned Python workers

**Date:** 2026-10-05
**Host:** Ubuntu 24.04, 8 cores, 15.6 GB RAM
**Topology:** clients → **mdb_srv** (fork-per-request HTTP) → **ProxySQL** (frontend 10 000,
backend pool 300) → **MariaDB**

---

## Executive summary

For the target workload (300 concurrent requests, each holding a MariaDB connection
≈1 s):

| Backend | Memory for 300 live workers (PSS) | Per worker (private) |
|---|---|---|
| **mdb_srv (this project)** | **86 MB** | **0.27 MB** |
| gunicorn/Flask, `--preload` (best-case Python) | ≈ 675 MB | 2.15 MB |
| Python/Flask, 300 independently spawned processes | ≈ 4.2 GB | 13.4 MB |

- Our backend is **~8× smaller than a preloaded gunicorn pool** and **~49× smaller
  than 300 naively spawned Python processes**.
- The naive `ps`-style sum of RSS (1 527 MB for our 300 workers) is misleading:
  **1 281 MB of that is copy-on-write-shared** with the parent. The true physical
  footprint is the **PSS sum, 86 MB**.
- Holding the MySQL client connection costs ≈ **0.2 MB per worker** (300 connections
  ≈ 60 MB); the bare worker is ≈ **0.08 MB private**.

---

## Why PSS, not RSS

`fork()` shares the parent's pages copy-on-write. Every child's `RSS` counts those
shared pages, so summing RSS over 300 children multiplies the shared image 300×.
**PSS (proportional set size)** splits each shared page among the processes that map
it, so `Σ PSS` is the correct estimate of physical RAM attributable to the service.
`private` = `Private_Clean + Private_Dirty` is what each process uniquely owns.

---

## Our backend — measured

Method: start mdb_srv (pool of forked workers, `max_children=1200`), fire 300
concurrent requests that sleep 6 s (so the 300 workers are alive simultaneously),
then read `/proc/<pid>/smaps_rollup` for the master and all 300 children.

### 300 workers, each holding a MySQL connection (`?op=dbsleep&ms=6000`)

| Scope | RSS | PSS | private |
|---|---|---|---|
| master | 3.8 MB | 0.2 MB | 0.1 MB |
| 300 children (sum) | **1 523.5 MB** | **86.1 MB** | **81.7 MB** |
| **total** | 1 527.4 MB | **86.4 MB** | 81.8 MB |
| per worker | 5.08 MB | 0.29 MB | 0.27 MB |

### 300 workers, no DB connection (`?op=dbdelay&ms=6000`)

| Scope | RSS | PSS | private |
|---|---|---|---|
| 300 children (sum) | 484.0 MB | 25.5 MB | 24.3 MB |
| **total** | 487.8 MB | **25.9 MB** | 24.4 MB |
| per worker | 1.61 MB | 0.09 MB | 0.08 MB |

**Interpretation.** A bare forked worker costs ~0.08 MB private. A worker with an
open MariaDB connection (libmariadb client state + buffers) costs ~0.27 MB private,
i.e. **each live MySQL connection ≈ 0.2 MB**. For 300 connections that is ~60 MB —
the difference between 26 MB and 86 MB.

These workers are **transient**: they serve one request and exit. The 86 MB is held
only while 300 requests are simultaneously in flight.

---

## Python baseline — measured on the same host

| Configuration | RSS | PSS | private |
|---|---|---|---|
| bare `python3` (sleep) | 9.2 MB | – | – |
| `python3 -c "import flask"` | 30.0 MB | – | – |
| gunicorn, 1 worker | 29.3 MB | 18.7 MB | 14.0 MB |
| gunicorn, 20 workers, no preload (per worker) | 29.4 MB | 14.1 MB | 13.4 MB |
| gunicorn, 20 workers, `--preload` (per worker) | 26.0 MB | 3.21 MB | 2.15 MB |

**Extrapolated to 300 workers:**

| Python model | Total PSS | naive ΣRSS |
|---|---|---|
| 300 independent spawned processes | **≈ 4.2 GB** | ≈ 8.8 GB |
| gunicorn `--preload` (fork after import) | **≈ 675 MB** | ≈ 7.8 GB |

(`--preload` shares the imported Flask interpreter via fork, which is the most
memory-favourable Python deployment; it is the fairest comparison since our model
also forks.)

---

## Comparison

| | mdb_srv | gunicorn `--preload` | 300 spawned Python |
|---|---|---|---|
| Total PSS for 300 workers | **86 MB** | ≈ 675 MB | ≈ 4 200 MB |
| Per-worker private | **0.27 MB** | 2.15 MB | 13.4 MB |
| Worker lifetime | per request (transient) | permanent | permanent |
| Shared base (counted once) | ~4.3 MB | ~30 MB interpreter | none (each its own) |
| Relative to mdb_srv | 1× | **~8×** | **~49×** |

The advantage comes from two things:
1. The shared base is tiny — a ~140 KB C binary versus a Python interpreter plus
   Flask/Werkzeug/Jinja2 modules (~9 MB bare, ~30 MB with Flask).
2. Per-request allocation is small (~0.08 MB private, dominated by the HTTP buffers
   and libmariadb state) and fully reclaimed when the worker exits.

---

## Supporting infrastructure (context, not part of "our backend")

| Process | RSS | Notes |
|---|---|---|
| ProxySQL | 159 MB baseline → 173 MB with 10 000 idle client connections | +14 MB for 10k conns (~1.4 KB/conn); drops to 107 MB after close |
| MariaDB | 136 MB | holds the 300 backend pool connections |

Whole data path (mdb_srv 300 workers + ProxySQL + MariaDB) ≈ **390–400 MB**.
The equivalent with 300 preloaded Python workers ≈ **~1.0 GB**; with 300 spawned
Python workers ≈ **~4.5 GB**.

---

## Caveats

- PSS/private were read from `smaps_rollup` at a single instant while all 300 workers
  were alive; numbers are stable across repeats but are point-in-time.
- "300 spawned Python processes" is extrapolated from the measured per-worker cost
  (14–19 MB PSS); running 300 to confirm would itself consume ~4 GB.
- Python numbers include only Flask + gunicorn; a real app with a DB driver and
  request processing would use strictly more.
- Our per-request workers are transient, so steady-state memory depends on
  concurrency, not on total requests served. 50 000 sequential requests leave the
  master at a flat 3.9 MB.
- The 0.2 MB/worker DB cost is the **client** connection to ProxySQL. MariaDB's own
  300 backend connections are ProxySQL's memory, reported above.
