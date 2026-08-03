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

A separate, reusable module (alongside `m_tool`, `m_hdf`, ...). Three
composable primitives built on one basic value type. No separate `bar_t` —
a bar is just a `field_t` with `is_bar=1`.

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
renderer just calls `snprintf` with the conventional flags.

### List — ordered array of fields

```c
typedef struct {
    int  title_h;       /* MLS string handle — list title, or 0           */
    field_t *items;
    int  n;
} list_t;
```

### Table — list of lists

A table is a list of lists: a header row (list of `field_t`) plus `n` body
rows, each a list with exactly the same number of columns.

```c
typedef struct {
    int  title_h;       /* MLS string handle — table caption, or 0        */
    list_t header;      /* column labels (fields, one per col)             */
    list_t *rows;
    int  n_rows;
} table_t;
```

### Text — free-form block

```c
typedef struct {
    int text_h;         /* MLS string handle for text content              */
    int footer_h;       /* optional footer (MLS string) or 0               */
} text_t;
```

### Entry and section (unchanged contract with registry)

```c
typedef struct {
    int  title;         /* MLS string handle (section header)              */
    int  footer;        /* MLS string handle or 0                          */
    const char *type;   /* "table" | "list" | "text" | "field"             */
    void *data;         /* table_t* / list_t* / text_t* / field_t*         */
} entry_t;

typedef struct { int title; int footer; entry_t *e; int n; } section_t;
```

### How gather sections use these

| Gather output    | Uses              |
|------------------|-------------------|
| LVM table        | `table_t` header = `{Device, VG, LV, Size, Free, Mount}`, rows = list per PV |
| ZFS datasets     | `table_t` with an `is_bar` field for Used% column |
| open ports       | `table_t` — ip:port + service                                   |
| cron             | `table_t` — schedule + command, truncated per cfg               |
| system info      | `list_t` of fields (hostname, kernel, …) or `text_t`            |
| memory           | `list_t` with `human=1` + `unit_h="kB"` fields                  |
| installed stacks | `list_t` with `fmt=FT_NONE` comma-separated                     |
| firewall status  | `text_t` (`ENABLED` / `DISABLED`)                               |
| bargraph         | just a `field_t` with `is_bar=1` + `frac` inside any list/table |

Nothing special about bars — the renderer sees `is_bar`, draws `cfg->bar_width`
characters, no separate datatype registration needed.

### Example: LVM table as data

```c
field_t header[] = {
    { .str_h = s_cstrc("Device"), .fmt = FMT_NONE },
    { .str_h = s_cstrc("VG"),     .fmt = FMT_NONE },
    { .str_h = s_cstrc("LV"),     .fmt = FMT_NONE },
    { .str_h = s_cstrc("Size"),   .fmt = FMT_INT,  .human = 1, .unit_h = s_cstrc("GB") },
    { .str_h = s_cstrc("Free"),   .fmt = FMT_INT,  .human = 1, .unit_h = s_cstrc("GB"),
      .is_bar = 1, .frac = .08 },                                    /* human + bar */
    { .str_h = s_cstrc("Mount"),  .fmt = FMT_NONE },
};
/* ... populate rows[] from `lvs` output ... */
```

### Datatype registry

A section entry is `{ type-name, void *data }`. The **registry** maps a
datatype name to its render + free functions, so adding a datatype never
touches a `switch` or an enum.

```c
/* m_types.h */
typedef void (*dt_render_fn)(const void *data, void *cfg);
typedef void (*dt_free_fn)(void *data);

typedef struct {
    const char *name;       /* "table", "list", "text", ...               */
    dt_render_fn render;
    dt_free_fn   free;
} datatype_t;

void dt_register(const datatype_t *dt);          /* one call to add a datatype */
const datatype_t *dt_lookup(const char *name);
void dt_render(const entry_t *e, cfg_t cfg);     /* registry dispatch */
```

```c
/* m_types.c — small name→datatype array */
static datatype_t *reg[32];               /* ponytail: fixed cap, linear scan */
void dt_register(const datatype_t *dt) { /* append to reg[] */ }
const datatype_t *dt_lookup(const char *name) { /* strcmp scan */ }
void dt_render(const entry_t *e, void *cfg)
{
    const datatype_t *dt = dt_lookup(e->type);
    if (dt) dt->render(e->data, cfg);
}
```

Built-ins (`table`, `list`, `text`) are registered by `out_init()`.
Adding e.g. a `tree` datatype = write `render`+`free` for a `tree_t`,
`dt_register`, done — gather and renderer dispatch untouched.

Contract (per goal 3): payloads, titles and every string are MLS handles —
`m_alloc` for structs/arrays, `s_dup`/`s_printf`/`s_cstr` for strings.
Renderers read via `m_str`/`m_len`; each datatype's `free` fn releases
handles with `m_free`; `free_sections()` calls `dt_free_fn` per entry.
No bare `malloc`/`free`/`strdup` in gather or render code (only inside
`m_subproc.c`, where `popen` reads need a raw buffer for `s_printf`).

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
- `gather.h` declares all eight + `section_t **gather_all(cfg_t)` (NULL-terminated).

`rootinfo.c` shrinks to:

```c
m_init();
cfg_t cfg = cfg_load(NULL);            /* or -c <path> */
section_t **s = gather_all(cfg);
out_render(s, cfg);
free_sections(s);   /* also in out.h or rootinfo.c */
cfg_free(cfg);
m_destruct();
```

## Output layer — all renderers in one file per backend

`out.h` is the porting seam:

```c
void out_init(void *cfg);                     /* register this backend's renderers */
int  out_render(section_t **sections, void *cfg);  /* out_section for all      */
void out_section(section_t *s, void *cfg);         /* dt_render per entry      */

/* one render function per built-in datatype (registered via dt_register)
   — each casts void *cfg back to cfg_t internally */
void out_table(const table_t *t, void *cfg);       /* list-of-list + header     */
void out_list(const list_t *l, void *cfg);         /* fields, comma-sep or vert */
void out_text(const text_t *t, void *cfg);
void out_field(const field_t *f, void *cfg);       /* helper: string+modifiers+bar */
```

`out_section` is a thin loop: for each entry it calls `dt_render(e, cfg)`,
which looks the datatype up in the registry and invokes the backend's
registered function — no `switch` to extend.

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
DEPS   = mls.od m_tool.od m_table.od m_hdf.od m_types.od m_subproc.od cfg.od \
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

1. **`lib/m_types.c` + `m_types.h`** — reusable type system: `field_t`/`list_t`/`table_t`/`text_t`, registry (`dt_register`/`dt_lookup`/`dt_render`), `section_t`/`entry_t`; built-in entry types (`table`, `list`, `text`). No gather yet.

2. **`cfg.h` + `cfg.c`** — `cfg_load` (search `./rootinfo.hdf` → `~/.config/rootinfo/rootinfo.hdf` → embedded default written via `hdf_write_file`), `cfg_int`/`cfg_bool`/`cfg_str` — one-liners over `hdf_find_node` + `hdf_get_*`. `make check` runs (no-op, just verifies no crash on cfg load/free).

3. **`out/out_term.c`** + **`out/out.h`** — `out_init` registers terminal renderers; `out_section` dispatches via `dt_render`. Hardcode one entry (`text` type, "scaffold works") → compile and run → verifies registry + render pipeline.

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

12. **Final** — remove the hardcoded scaffold entry from step 3; all three
    built-in types (`table`, `list`, `text`) render correctly end-to-end;
    config `(section ...)` block honored (`gather_all` skips disabled
    sections). `make check` green with all eight sections.
