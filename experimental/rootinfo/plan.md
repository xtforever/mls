# rootinfo plan

`rootinfo` prints a system overview (what machine is this, what storage do I
have, what runs here). Source spec: `draft-rootinfo.txt`.

Design goal: **gathering is separate from rendering**. Every section gathers
its data into a generic `section_t`; every renderer reads only `section_t`.
The initial target is an **ANSI terminal** (`out/out_term.c`); curses, kitty
graphics and X11 backends are kept cheap by the separation but deferred until
the ANSI terminal version is production-ready.

Second goal: **every number is an example, not a rule**. The draft's `TOP 5`,
`max 5 ipv4`, `shortend if too long`, column widths, bar width — all are
configurable via an HDF config file (the lib ships a full HDF parser, so no
new format, no new dependency).

Third goal: **MLS owns every allocation**. All memory goes through MLS
handles (`m_alloc`, `m_create`, ...) and all strings are MLS string handles
(`s_dup`, `s_printf`, `s_cstr`); bare `malloc`/`free`/`strdup` are only used
where MLS has no cheap alternative (e.g. `popen` line buffers handed to a
`kv_value` copy). Renderers read payloads via `m_str`/`m_len`, and the
registry's per-datatype `free` fn releases handles with `m_free` — nothing to
leak, nothing manually freed by callers.

## File layout

```
experimental/rootinfo/
  rootinfo.c              main: load cfg, gather_all(), out_render(), cleanup
  cfg.h                   cfg load + typed getters (wraps m_hdf)
  cfg.c
  rootinfo.hdf            user config (searched: ./rootinfo.hdf, ~/.config/…)
  gather.h                one prototype per section + gather_all()
  gather/
    sec_system.c          OS/CPU/MEM, NICs          (draft 10-15)
    sec_zfs.c             pools, datasets            (draft 2-4)
    sec_lvm.c             PV/VG/LV table             (draft 6-9)
    sec_cron.c            weekly/daily + crontabs    (draft 17-27)
    sec_proc.c            TOP N processes            (draft 28-29)
    sec_ports.c           open ports, max N ipv4     (draft 30-36)
    sec_firewall.c        iptables status            (draft 37)
    sec_stack.c           python/php etc.            (draft 38-40)
  out/
    out.h                 renderer interface
    out_term.c            ANSI terminal backend (MVP — see goal 1)
    out_curses.c          (deferred) ncurses backend
    out_kitty.c           (deferred) kitty graphics backend
    out_x11.c             (deferred) X11 window backend
  makefile
  README.md

lib/
  m_types.h              reusable type system: field/list/table/text + registry
  m_types.c
  m_subproc.h            reusable subprocess runner (plain ANSI C)
  m_subproc.c
```

## Configuration (HDF)

`cfg_t` wraps the HDF tree root (`int` handle). Load order:
`./rootinfo.hdf`, then `~/.config/rootinfo/rootinfo.hdf`; if neither exists,
an embedded default config is parsed (`hdf_parse_string`) and written to
`./rootinfo.hdf` via `hdf_write_file`. `-c path` overrides the search.

Example `rootinfo.hdf` (S-expressions, as parsed by the lib's HDF):

```
(cfg
  (proc        (top 5))
  (ports       (max 5) (ipv4 true))
  (cron        (max_lines 10))
  (table       (max_col_width 24) (marker "..."))
  (bar         (width 20) (empty "░") (full "▓"))
  (section     (zfs true) (lvm true) (cron true) (proc true)
               (ports true) (firewall true) (stack true))
)
```

All defaults live in `cfg.c`; every lookup has a sane fallback. The numbered
limits in the draft (`TOP 5`, `max 5`, `shortend if too long`) become these
keys — the numbers are examples only.

`cfg.h` — **convenience wrappers so config access is a single call**. The
caller never calls `hdf_find_node`/`hdf_get_int` themselves; each getter is
one line built on exactly those two:

```c
typedef int cfg_t;                       /* HDF root handle */

cfg_t cfg_load(const char *override);    /* search + fallback + write default */
void  cfg_free(cfg_t);                   /* hdf_free */

int   cfg_int(cfg_t, const char *sect, const char *key, int dflt);
int   cfg_bool(cfg_t, const char *sect, const char *key, int dflt);
const char *cfg_str(cfg_t, const char *sect, const char *key, const char *dflt);
```

```c
/* the whole trick: one convenience call == hdf_find_node + hdf_get_int */
int cfg_int(cfg_t root, const char *sect, const char *key, int dflt)
{
    return hdf_get_int(hdf_find_node(root, sect), key, dflt);
}
```

`cfg_bool`/`cfg_str` are the same shape (`hdf_get_bool`/`hdf_get_property`,
falling back to `dflt` when the section node or key is missing). `cfg_load`
returns the parsed root handle, so call sites are just:

```c
int top = cfg_int(cfg, "proc", "top", 5);   /* one call, no hdf_* anywhere */
```

The gather and output code never touches `hdf_*` directly — only `cfg.h`'s
getters.

## Subprocess module (`lib/m_subproc.c` / `m_subproc.h`)

Every gather section shells out to system commands (`zfs list`, `lvs`,
`ss -tlp`, `crontab`, `ps`, `iptables`, `python --version`, ...).  Rather
than spreading `popen`/`pclose` + manual buffer management across eight
files, we build one thin, reusable **subprocess module** in plain ANSI C.

It lives in `lib/` (alongside `m_tool`, `m_hdf`, ...) so `dirwalker`,
`indexer` and future tools can use it — not just `rootinfo`.

### API

```c
/* m_subproc.h */

/* Run cmd, capture stdout+stderr into MLS string handles. Returns exit code
   (or -1 on popen failure).  0 handles mean "discard". */
int subproc_run(const char *cmd, int *stdout_h, int *stderr_h);

/* Run cmd, return stdout as one MLS string handle (stderr discarded). */
int subproc_read(const char *cmd);

/* Run cmd, return stdout split into an MLS list of MLS string handles
   (one per line, blank lines preserved).  stderr discarded. */
int subproc_lines(const char *cmd);
```

Implementation wraps `popen("cmd 2>/dev/null", "r")` (or `popen` both
streams with a pipe trick), reads into `s_printf` appends, `pclose`, returns
exit code.  `malloc` is only used for internal read buffers (handed straight
to `s_printf`); no bare `free` — the caller owns the output handles and
`m_free`s them.

### Build

```makefile
DEPS   = mls.od m_tool.od m_table.od m_hdf.od m_types.od m_subproc.od cfg.od \
         gather/*.od out/out_term.od
```

### Usage in gather sections

```c
/* sec_zfs.c */
int zfs_out = 0;
int rc = subproc_read("zfs list -H -o name,used,avail,refer,mountpoint");
if (rc != 0) return NULL;
int lines = subproc_lines(...);  /* or s_split(m_str(zfs_out), '\n', 0) */
...
m_free(zfs_out);
```

When a command is not installed on the system `subproc_run` returns -1,
the section returns `NULL`, and that section is silently omitted — exactly
the current `if (cpuinfo)` pattern.

## Type system (`lib/m_types.c` / `m_types.h`)

A separate, reusable module (alongside `m_tool`, `m_hdf`, ...). One value
type plus one block type. No separate `bar_t` — a bar is just a `field_t`
with `is_bar=1`.

### Field — the atomic type

Every value in rootinfo (a cell, a list item, a table header) is a `field_t`.
It carries the raw string plus formatting hints so the renderer can display
numbers, hex, units and bars without re-parsing.

```c
typedef enum { FMT_NONE=0, FMT_INT, FMT_FLOAT, FMT_HEX } field_fmt_t;
typedef enum { ALIGN_LEFT, ALIGN_RIGHT, ALIGN_CENTER } align_t;

typedef struct {
    int str_h;          /* MLS string handle — the raw text representation */
    field_fmt_t fmt;    /* render hint: int, float, hex, or none (raw)    */
    align_t align;      /* justification (all types; tables use col-align) */
    int len;            /* min display width (0 = auto)                   */
    int prec;           /* float precision / hex width (0 = auto)         */
    int human;          /* 1 → append K/M/G/T suffix (memory, disk)      */
    int unit_h;         /* MLS handle for unit symbol (kB, %, …) or 0    */
    int is_bar;         /* 1 → draw an inline bar for this field          */
    double frac;        /* bar fill proportion 0..1 (only if is_bar)      */
} field_t;
```

The renderer reuses printf-style formatting for numbers: `FMT_INT`/`FMT_FLOAT`/
`FMT_HEX` map to `%d`/`%f`/`%x`, `prec` controls the fraction-width, and
`align` sets `%-*s` vs `%*s` vs center-pad.  No new format system — the
renderer builds each value string with `s_printf` (`lib/m_tool.h`) and
prints it with `%*s`/`%-*s`.

### Block — one struct for every entry kind

All section entries (table, list, text, bar) are one `data_t` struct. A
`dt_type_t` tag selects which slots are live; `title_h`/`footer_h` work on
any kind. Blocks are MLS handles; `section_t.entries` is a list of data
handles.

```c
typedef enum { DT_TABLE, DT_LIST, DT_TEXT, DT_BAR } dt_type_t;

typedef struct {
    dt_type_t type;     /* discriminator, selects the live slots below   */
    int  title_h;       /* optional title for any block                  */
    int  footer_h;      /* optional footer for any block                 */
    int  header;        /* DT_TABLE: MLS list handle of field_t          */
    int  rows;          /* DT_TABLE: MLS list of row handles             */
    int  items;         /* DT_LIST:  MLS list handle of field_t          */
    int  text_h;        /* DT_TEXT:  MLS string handle                   */
    int  bar_h;         /* DT_BAR:   MLS handle to a field_t             */
} data_t;

typedef struct {
    int  title;         /* MLS string handle (section header)            */
    int  entries;       /* MLS list handle of data_t handles             */
} section_t;
```

A table is a list of lists: `header` (column labels) plus `rows`, each row an
MLS list of `field_t` with the same column count.  A list is `items` (a list
of `field_t`).  A text block is one string.  A bar block holds a single
`field_t` with `is_bar=1` + `frac`.

### How gather sections build these

`gather.h` exposes small helpers — no hand-built structs in the gather code:

```c
int  section_new(const char *title, int cap);   /* section + empty entries */
int  data_new(dt_type_t type);                  /* one data_t handle       */
void add_entry(int entries, int data_h);        /* append a data handle    */
int  table_new(int ncols, const char **cols);   /* table + header + rows   */
int  table_new_a(int ncols, const char **cols, const align_t *aligns);
int  text_new(int text_h);                      /* text block owns string  */
int  bar_new(int str_h, double frac);           /* bar block owns field    */
```

`FIELD_ADD(list, str)` / `FIELD_ADD_R(list, str)` append a left/right-aligned
field; `FIELD_ADD_H`/`FIELD_ADD_H_R` take ownership of an existing handle.

| Gather output    | Builds                               |
|------------------|--------------------------------------|
| LVM table        | `table_new(7, ...)` + one row per LV |
| ZFS datasets     | `table_new(5, ...)`                  |
| open ports       | `table_new(ncols, ...)`              |
| cron             | `table_new(2, ...)` + `text_new` for the `cron.*` dirs |
| system info      | `table_new(2, {"Key","Value"})` (hostname/os, cpu, mem) |
| process count    | `data_new(DT_LIST)` + one field      |
| stacks           | `data_new(DT_LIST)` + one field per runtime |
| users / network  | `text_new(s_printf(...))`            |
| firewall status  | `text_new(s_dup(status))`            |
| disk summary     | `bar_new(s_printf(...), frac)`       |

Nothing special about bars — inline `is_bar` fields work inside any table
row; standalone bars are `DT_BAR` blocks.

### Dispatch

Each backend provides one renderer per kind plus a generic `free_data`.
`out_data` dispatches on the tag:

```c
void out_data(const data_t *d, void *cfg)
{
    switch (d->type) {
    case DT_TABLE: out_table(d, cfg); break;
    case DT_LIST:  out_list(d, cfg);  break;
    case DT_TEXT:  out_text(d, cfg);  break;
    case DT_BAR:   out_bar(d, cfg);   break;
    }
}
```

Contract (per goal 3): all memory and strings are MLS handles.  Every
collection — `data_t.header`/`rows`/`items`, `section_t.entries` — is an MLS
list handle, so access is bounds-checked via `m_read`/`m_write` and the count
is `m_len(handle)` (no raw `.n` / `.n_rows` fields to drift).  `m_alloc` for
single structs, `m_create` for lists, `s_dup`/`s_printf`/`s_cstr` for
strings.  Renderers read strings via `m_str`, lists via `m_len` + `m_read`;
`free_data` releases whatever slots the tag selects and the caller frees the
block/section handles.  `free_sections()` walks sections → entries →
`free_data`, then frees each block and section handle.  No bare `malloc`/
`free`/`strdup` in gather or render code (only inside `m_subproc.c`, where
`popen` reads need a raw buffer for `s_printf`).

## Data gathering — one file per section

Each `sec_*.c` exposes exactly one function:

```c
section_t *gather_xxx(cfg_t cfg);   /* returns a fully-built section, or NULL */
```

- Gathers via the `subproc` module (`subproc_read`/`subproc_lines`) for shell
  commands (`zfs list`, `lvs`, `ss -tlp`, `crontab`, `ps aux`, `iptables`);
  system info via `uname`, `sysconf`, `getloadavg`, `statvfs`, `getpwuid`
  (as in the current `rootinfo.c`).
- A command not installed → `subproc_run` returns -1, `sec` returns `NULL`,
  section silently omitted (same pattern as current `if (cpuinfo)` guards).
- Limits come from `cfg`: `sec_proc` uses `cfg_int(cfg, "proc", "top", 5)`,
  `sec_ports` uses `ports.max`/`ports.ipv4`, `sec_cron` uses
  `cron.max_lines`. The draft numbers are just the defaults.
- A command fails → `sec` returns `NULL` and that section is simply not
  rendered (like the current `if (cpuinfo)` guards).
- Section enable/disable lives in the `(section ...)` block: `rootinfo.c`
  skips `gather_xxx` when `cfg_bool(cfg, "section", "xxx", 1)` is false.
- `gather.h` declares all eight + `int gather_all(cfg_t)` (returns MLS list of `int` — section handles; `m_buf(h)` gives the `section_t *`).

`rootinfo.c` shrinks to:

```c
m_init();
cfg_t cfg = cfg_load(NULL);            /* or -c <path> */
int s = gather_all(cfg);
out_render(s, cfg);

cfg_free(cfg);
m_destruct();
```

## Output layer — all renderers in one file per backend

`out.h` is the porting seam:

```c
void out_render(int sections_h, void *cfg);   /* out_section for each section */
void out_section(section_t *s, void *cfg);    /* out_data per entry           */
void out_data(const data_t *d, void *cfg);    /* dispatch on d->type          */

/* one render function per block kind — each casts void *cfg back to cfg_t */
void out_table(const data_t *d, void *cfg);   /* list-of-list + header     */
void out_list(const data_t *d, void *cfg);    /* fields, vertical          */
void out_text(const data_t *d, void *cfg);
void out_bar(const data_t *d, void *cfg);
void out_field(const field_t *f, void *cfg);  /* helper: string+mods+bar (field is a value, not a handle) */
void free_sections(int sections_h);
```

`out_section` iterates the entries list via `m_len`+`m_read` (bounds-checked),
calling `out_data` per entry.  Each renderer reads its payload from the
`data_t` pointer and iterates sub-lists the same way via MLS handles.

- `out_term.c` renders tables, lists and text. `out_field` draws one field
  — format (int/hex/float), human-readable suffix, unit, and an inline bar
  when `is_bar=1` (ANSI Unicode `░`/`▓` from config, width per
  `cfg.bar.width`). `out_table` truncates wide columns via `cfg.table.*`.
  No curses, no X11, no kitty — those are deferred, the MVP is ANSI-only.
- Other backends are a copy of `out_term.c` with the same four functions
  reimplemented for the target; `rootinfo.c` picks the backend via
  `-DOUT_BACKEND=term|curses|…` or `argv[1]`. No gather code changes.

## Build

`makefile` (extends the current one):

```
DEPS   = mls.od m_tool.od m_table.od m_hdf.od m_subproc.od cfg.od \
         gather/*.od out/out_term.od
```

`make check` stays: build + run with stdout to /dev/null.

## Build-out — one section at a time, verified each step

The current `rootinfo.c` is a working prototype with one implicit section
(system/cpu/memory/disk/processes/uptime/user). We pull it into the new
architecture, get one section rendering through the full pipeline, then add
sections one by one.  `make check` must be green after every step.

### Phase 0 — scaffold + one section

0. **`lib/m_subproc.c` + `m_subproc.h`** — reusable subprocess runner (plain ANSI C). `subproc_run`, `subproc_read`, `subproc_lines` — each wraps `popen`/`pclose`, captures output into MLS handles. No external deps beyond `mls`/`m_tool`. `make check` in `lib/` verifies a trivial `subproc_read("echo ok")`.

1. **`lib/m_types.h`** — reusable type system: `field_t`/`data_t`/`section_t` with the `dt_type_t` tag. Pure data — no registry, no render code. No gather yet.

2. **`cfg.h` + `cfg.c`** — `cfg_load` (search `./rootinfo.hdf` → `~/.config/rootinfo/rootinfo.hdf` → embedded default written via `hdf_write_file`), `cfg_int`/`cfg_bool`/`cfg_str` — one-liners over `hdf_find_node` + `hdf_get_*`. `make check` runs (no-op, just verifies no crash on cfg load/free).

3. **`out/out_term.c`** + **`out/out.h`** — four renderers (`table`/`list`/`text`/`bar`) + `free_data`; `out_data` dispatches on the tag. Hardcode one entry (`text` kind, "scaffold works") → compile and run → verifies dispatch + free pipeline.

4. **`gather/sec_system.c`** — port the existing `system/cpu/memory/disk/processes/uptime/user` code into a `gather_system(cfg)` that returns a `section_t`. Uses the `subproc` module for `/proc` reads. `gather.h` with `gather_all()` (only `gather_system` for now). **This is the first section that matches the draft.** `make check` runs the full pipeline: cfg → gather → render → free. Output matches the current `rootinfo.c`.

### Phase 1+ — add sections, one per step

Each step adds one `gather/sec_xxx.c`, registers it in `gather_all`, verifies with `make check`.

5. **`sec_lvm.c`** — `lvs` + `pvs` via `subproc_lines`, produces a `table` entry (PV/VG/LV/Size/Free/Mount, free column uses inline bar cells). Verify.

6. **`sec_zfs.c`** — `zfs list` via `subproc_lines`, produces `table` (pools) + `bar` (usage). Returns `NULL` on systems without ZFS. Verify.

7. **`sec_ports.c`** — `ss -tlp` via `subproc_lines`, produces `table` (ip:port, service), limited to `cfg_int(cfg, "ports", "max", 5)`, ipv4-only toggle `cfg_bool(cfg, "ports", "ipv4", 1)`. Verify.

8. **`sec_proc.c`** — `ps aux` via `subproc_lines`, produces `table` (top N processes), limited to `cfg_int(cfg, "proc", "top", 5)`. Verify.

9. **`sec_cron.c`** — `crontab -l` + `/etc/cron.daily`/`weekly` listings via `subproc_read`, produces `table` (schedule + command), truncated to `cfg_int(cfg, "cron", "max_lines", 10)`. Verify.

10. **`sec_firewall.c`** — `iptables -L` via `subproc_read`, produces `text` (`ENABLED`/`DISABLED`/rule count). Verify.

11. **`sec_stack.c`** — `python --version`, `php --version`, `php-fpm --version` via `subproc_read`, produces `list` (comma-separated versions). Installed runtimes only; missing ones are omitted. Verify.

12. **Final** — remove the hardcoded scaffold entry from step 3; all four
    block kinds (`table`, `list`, `text`, `bar`) render correctly
    end-to-end; config `(section ...)` block honored (`gather_all` skips
    disabled sections). `make check` green with all eight sections.
