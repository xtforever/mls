# rootinfo — universal storage section plan

Replace `sec_lvm.c` + `sec_zfs.c` (which only work when those tools are
installed) with a single `sec_storage.c` that works on any Linux system
using `df` + `lsblk`.

## What it replaces

| Remove | Reason |
|--------|--------|
| `gather/sec_lvm.c` | Only works when `lvs` is installed; empty on non-LVM systems |
| `gather/sec_zfs.c` | Only works when `zfs` is installed; empty on non-ZFS systems |
| `gather/sec_system.c` disk bar | Redundant with the new storage table (keep as quick-glance summary in SYSTEM, or move it out — TBD) |

The disk bar at the top of SYSTEM stays — it's a useful one-liner. The
new storage table goes deeper.

## New file: `gather/sec_storage.c`

### Data source

Two commands, both universally available on Linux:

1. **`LC_ALL=C df --output=source,size,used,avail,pcent,target`**
   — Gives: filesystem source, 1K-blocks total, used, available, use%, mountpoint.
   Parsed line-by-line, skip header.

2. **`LC_ALL=C lsblk -P -o NAME,SIZE,TYPE,MOUNTPOINT,FSTYPE`**
   — Gives `KEY="VALUE"` pair per line. Used to look up device TYPE and FSTYPE
   for each mountpoint (merges into the `df` table).

### Output format

```
  STORAGE
  ------------------------------------------
  FILESYSTEM               SIZE   USED  FREE  USE%  TYPE  MOUNT
  ------------------------ ------ ----- ----- ----- ----- -----------
  /dev/nvme0n1p2           467G   178G  266G   41%  ext4  /
  /dev/nvme0n1p1           1.1G   6.2M  1.1G    1%  vfat  /boot/efi
  otto:/8tbmv2             1.8T   1.6T  118G   94%  nfs   /mnt/8tbmv2
  otto:/back2019a          4.6T   2.9T  1.4T   68%  nfs   /mnt/back2019a
  ...
```

- SIZE/USED/FREE: humanized (GB/TB, one decimal, auto-scale).
- USE% bar: inline using the existing `field_t` bar mechanism (`.is_bar=1`,
  `.frac` = percentage/100).
- Skip virtual filesystems: `tmpfs`, `devtmpfs`, `efivarfs`, `squashfs`,
  `overlay`, `cgroup`, `debugfs`, `tracefs`, `securityfs`, `configfs`,
  `pstore`, `hugetlbfs`, `mqueue`, `binfmt_misc`.  Configurable via
  cfg key `storage.skip_fstypes`.
- FSTYPE column comes from `lsblk`. If `lsblk` lookup misses (e.g. NFS
  mount from a remote server that isn't a block device), show `-` or
  the raw filesystem column from `df`.

### Config keys (new in `cfg.c` default)

```
(storage
  (skip_fstypes "tmpfs,devtmpfs,efivarfs,squashfs,overlay,cgroup,
                 debugfs,tracefs,securityfs,configfs,pstore,
                 hugetlbfs,mqueue,binfmt_misc")
  (max_rows 20)
)
```

### Implementation outline

```c
// gather/sec_storage.c Pseudocode
int gather_storage(cfg_t cfg)
{
    // 1. df output
    int df_lines = subproc_lines("LC_ALL=C df --output=source,size,used,avail,pcent,target 2>/dev/null");
    if (!df_lines) return 0;

    // 2. lsblk output (optional — if it fails, just skip type column)
    int lsblk_lines = subproc_lines("LC_ALL=C lsblk -P -o NAME,SIZE,TYPE,MOUNTPOINT,FSTYPE 2>/dev/null");
    // build a mountpoint → {TYPE, FSTYPE} lookup map from lsblk

    // 3. Parse df lines, build a `table_new(...)` table
    //    - Parse 6 columns: source, size_kb, used_kb, avail_kb, pcent, mount
    //    - Humanize size/used/avail (÷1024² for GB, ÷1024³ for TB)
    //    - Add USE% field with is_bar=1 + frac
    //    - Look up TYPE/FSTYPE from lsblk map
    //    - Filter out skip_fstypes

    // 4. Build section_t (section_new + add_entry) → return
}
```

### build changes

- `makefile`: replace `gather/sec_lvm.od gather/sec_zfs.od` with `gather/sec_storage.od`
- `gather.h`: drop `gather_lvm`/`gather_zfs`, add `gather_storage`
- `gather/sec_system.c:gather_all`: call `gather_storage` (keep after `proc`,
  before `cron`)
- `cfg.c`: add `(storage ...)` defaults

### Edge cases

- **df fails** (e.g. `statvfs` issues): return 0, section omitted.
- **lsblk fails**: table renders without TYPE column; FSTYPE comes from
  the first field of df if available, otherwise `-`.
- **NFS/CIFS mounts**: `df` shows them; `lsblk` won't have them → TYPE= `nfs`,
  FSTYPE= `-` (or try to detect from df source field prefix like `server:/path`).
- **Very long mountpaths**: truncated per `cfg.table.max_col_width`.
- **No mounts at all**: return 0 (shouldn't happen — `/` always exists).
