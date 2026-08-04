# rootinfo — presentation improvement plan

Analysis of stdout from `rootinfo.exed` (commit f3f5b14-ish). Each item
below is an observed issue → fix, ordered by impact/effort.

---

## 1. Memory values are raw kB — unreadable

**Observed:**
```
MemTotal      15975148 kB
MemAvailable  8964912 kB
SwapTotal     4194300 kB
SwapFree      3037104 kB
```

**Problem:** 8-digit numbers with kB suffix. User must mentally divide by 1M
to get GB.

**Fix:** In `sec_system.c` memory gathering, parse the numeric value, store via
`field_t` with `human=1`. The renderer's `human_suffix()` already does the
K/M/G/T dance.

**Affects:** `gather/sec_system.c:129-161` — the meminfo key-value builder.

**Risk:** Low. `human_suffix` is already used by the DISK bar path and tested
there.

---

## 2. Uptime shows hours only, no days

**Observed:**
```
UPTIME
1410 h 27 min
```

**Problem:** 1400+ hours = ~58 days but days aren't shown. After a week of
uptime, "17d 3h" is more meaningful than "411h".

**Fix:** In `gather/sec_system.c:217`, add day extraction:
```c
int dd = hh / 24; hh %= 24;
if (dd) snprintf(buf, sizeof(buf), "%dd %dh %dm", dd, hh, mm);
else    snprintf(buf, sizeof(buf), "%dh %dm", hh, mm);
```

**Affects:** `gather/sec_system.c:209-231`.

---

## 3. Empty LVM / ZFS sections waste space

**Observed:**
```
  LVM
  ------------------------------------------
  Device  VG  LV  Size  Free  Mount
  ------  --  --  ----  ----  -----
```

**Root cause confirmed:** Both `lvs` and `zfs` are NOT installed on this
system (exit code 127). `subproc_lines` treats exit code 127 as success
(`rc >= 0`), captures empty stdout as a valid handle, and `s_msplit("")`
produces a list with one empty-string element → `m_len` > 0 → the
`if (!n)` guard in the gather function doesn't fire. An empty table with
header row is built and rendered.

**Fix (two-layer):**

Layer 1 — `lib/m_subproc.c:subproc_lines`: Return 0 when exit code != 0.
```c
if (rc != 0 || !out_h) { ... return 0; }
```
This also fixes issue #5 (PHP-FPM "not found") at the same time, since
`subproc_read` has the same pattern.

Layer 2 — gather functions (belt-and-suspenders): After parsing rows, if
no rows were added, free the table handle and return 0.

**Affects:** `lib/m_subproc.c:137-147` (`subproc_lines`), `lib/m_subproc.c:131-134`
(`subproc_read`), `gather/sec_lvm.c` + `gather/sec_zfs.c` (defensive check).

---

## 4. Open ports — Process column always empty for non-root

**Observed:**
```
  Address  Port   Process
  -------  -----  -------
  0.0.0.0  52255
  0.0.0.0  3308
  0.0.0.0  2049
```

**Problem:** `ss -tlp4n` requires root to show process names. The Process
column uses 3 columns wide but is always blank, wasting space and confusing
the user (are these really unowned?).

**Fix:** Two-stage:
  1. Check if we're root; if not, drop the Process column and use only
     Address/Port.
  2. Add a footer hint: "(run as root for process names)".

**Affects:** `gather/sec_ports.c:30-36` (header), `:81-87` (row building).

**Risk:** Low. `ss -tlpn` always returns well-structured output; the
address:port parsing is column-independent.

---

## 5. PHP-FPM "not found" error leaks into output

**Observed:**
```
  PHP-FPM: sh: 1: php-fpm: not found
```

**Root cause:** `subproc_read` also suffers from issue #3's bug — non-zero
exit code isn't treated as failure. `php-fpm` doesn't exist (exit 127), but
`subproc_read` returns the stdout handle anyway. Shell writes "not found" to
stderr, but the pipes capture it and the shell's stderr ends up in stdout
because `subproc_fork` connects stderr to a separate pipe, not to stdout.
The `2>&1` suffix in the command string is irrelevant here — `subproc_run`
handles stderr capture internally.

Wait — actually the issue is different. The `subproc_fork` function redirects
STDOUT_FILENO and STDERR_FILENO to separate pipes. But `close_range(3, ~0U, 0)`
closes ALL file descriptors >= 3. Then `execl("/bin/sh", "sh", "-c", cmd)`.
But the pipe fds were dup'd to 0 and 1... no, only stdout.

Re-reading: `dup2(out_pipe[1], STDOUT_FILENO)` — stdout becomes the write end
of the out pipe. `dup2(err_pipe[1], STDERR_FILENO)` — stderr becomes the write
end of the err pipe. Then `close_range(3, ~0U, 0)` closes all fds >= 3.

Now `execl("/bin/sh", "sh", "-c", cmd)` — sh inherits fd 0 (stdin), 1 (stdout
→ out pipe), 2 (stderr → err pipe). sh runs the command. If `php-fpm` isn't
found, sh writes the error to its stderr (fd 2 → err pipe). The `2>&1` in the
command string would redirect the COMMAND's stderr to stdout, but sh's own
error ("not found") still goes to stderr (fd 2).

So the "sh: 1: php-fpm: not found" should go to the `err` handle, not stdout.
But `subproc_read` discards stderr (it passes NULL for `stderr_h` in
`subproc_run`). So the error should NOT appear in the output...

Unless the current `rootinfo.exed` was built against an older version of
`subproc_read` that worked differently. Let's not chase this further — the fix
is the same as #3: `subproc_read` should return 0 when exit code != 0.

**Fix:** Apply the same exit-code check from issue #3 to `subproc_read`.
Remove the `2>&1` from PHP/PHP-FPM calls (not needed since `subproc_run`
captures stderr separately). The `2>&1` on `python3 --version` is needed
because Python 3.12 writes version info to stderr — but with `subproc_run`'s
separate stderr capture, the python version would end up in the err handle,
not stdout, so `subproc_read` would get empty output. Either:
  - Keep `2>&1` for python3, or
  - Change `add_version` to use `subproc_run` directly and check both stdout
    and stderr.

**Affects:** `gather/sec_stack.c:41-43`, `lib/m_subproc.c:131-134`.

---

## 6. Empty crontab shows header with no rows

**Observed:**
```
  Crontab
  Schedule  Command
  --------  -------
```

**Problem:** `crontab -l` returned nothing (no user crontab), but the
function still creates a table entry with header and separator.

**Fix:** In `add_cron_table()` (`gather/sec_cron.c:11-68`), check if any
rows were added after parsing. If count==0, skip adding the entry.

**Affects:** `gather/sec_cron.c:62-66` — add `if (count == 0) { m_free(th); return; }`
before `add_entry`.

---

## 7. Column width over-padding visible

**Observed:**
```
  Command
  -------------
  opencode
  chrome
```

The `COMMAND` column header has 13 dashes but "opencode" is only 8 chars.
The extra padding is from `max_col_width` (24) being applied symmetrically;
when no actual cell reaches that width, the header separator stays at the
configured max.

**Fix:** The `max_col_width` cap is already correct (truncates at 24).
The separator dash line is computed from `colw[c]` which is `max(header_w,
max_cell_w, max_col_width)` — wait, no. Looking at `out_term.c:185`,
`colw[c]` starts at header width, then grows to max cell width (up to
`max_col_width` cap at line 204). The header field's `len` is set to
`colw[c]` at line 213 during rendering. The issue is that the header
text itself isn't padding-dependent, it's the `colw` computation that
might be picking up the `max_col_width` as a fallback.

Actually, re-reading the code: `int vw = hdr[c].len ? hdr[c].len : strlen(...)`
— if `hdr[c].len` is 0, it uses `strlen`. Then `colw[c]` starts at `vw`,
then grows to max actual cell value width. Then capped at `max_col_width`.
The separator line uses `colw[c]`. So if no cell is wider than the header,
the separator matches the header width. This looks correct.

The observed output isn't actually padded wrong — `COMMAND` is 7 chars and
the separator is 13 dashes because the max_col_width is 24 and...  Actually
wait, the output shows:
```
  COMMAND      
  -------------
```
So "COMMAND" is left-aligned in a 13-char column, but "opencode" is only
8 chars. The issue is that `COMMAND` column width is 13 because one or more
commands in the top-5 have a name that is 13 chars wide. Looking again at
the data:
```
  opencode     
  chrome       
  chrome       
  rootinfo.exed
  chrome       
```
`rootinfo.exed` is 13 chars. So the column IS being driven by actual data.
Not an issue — this is correct behavior.

However, `%MEM` shows `2.9  ` vs `7.6  ` — right-aligned would look better
for numeric columns. Currently all columns are `ALIGN_LEFT`. The PID column
also could be right-aligned.

**Fix:** Set `.align = ALIGN_RIGHT` on `%CPU`, `%MEM`, `PID` column fields.

**Affects:** `gather/sec_proc.c:34` and `:61`.

---

## 8. No ANSI styling — all plain text

**Observed:** Section titles, headers, and bars all use plain text. A
terminal tool benefits from even minimal styling.

**Fix:** Add ANSI escape helper in `out_term.c`:
  - Section titles: bold (`\033[1m`)
  - Table headers: underline or bold
  - Bar fill: could stay monochrome or use green/yellow/red gradient

Keep it minimal — section titles bold adds the most value for the least
complexity.

**Affects:** `out/out_term.c:77-78` (section title print), `:207-217`
(header row).

**Risk:** Low. All modern terminals support bold. Escape codes don't affect
column width calculations since they're zero-width.

---

## 9. Disk bar shows free%, not used% — counter-intuitive

**Observed:**
```
DISK  Free: 265.9 GB / 467.3 GB (57%) ▓▓▓▓▓▓▓▓▓▓▓░░░░░░░░░
```

**Problem:** The bar fills proportionally to free space (57% free = 11/20
filled). Convention is that bars show usage, not availability — a 57% full
bar looks like the disk is 57% full when it's actually 57% free.

**Fix:** In `gather/sec_system.c:169`, invert the fraction:
`double frac = total > 0 ? 1.0 - (avail / total) : 0.0` or change the
bar to show usage. Or keep the bar but rename the label to more clearly
indicate "Free".

**Simpler fix:** Change label to "DISK  Used: X GB / Y GB (Z%)" and use
`used / total` for the bar.

**Affects:** `gather/sec_system.c:164-182`.

---

## 10. Section ordering — minor reorder for scanability

**Observed order:** SYSTEM → LVM → ZFS → PORTS → PROCS → CRON → FW → STACKS

**Suggested:** SYSTEM (always first) → PROCESSES → DISK → MEMORY (within
SYSTEM) → PORTS → CRON → FW → STACKS → LVM → ZFS (move storage sections
to end; they're often empty).

Or better: leave SYSTEM as-is (it already contains DISK/MEMORY/PROCESSES
internally) and just reorder the top-level sections. Storage sections
(LVM, ZFS) that are commonly empty belong at the end.

**Fix:** Reorder calls in `gather_all()` (`gather/sec_system.c:254-277`).

---

## Quick wins (low effort, high impact)

| # | Fix | Effort | Lines changed |
|---|-----|--------|---------------|
| 3+5 | **`subproc_lines`/`subproc_read` return 0 on non-zero exit** — fixes empty LVM, ZFS, crontab, AND PHP-FPM "not found" in one place | small | 2 lines in `lib/m_subproc.c` |
| 2 | Uptime with days | tiny | 3 |
| 4 | Drop empty Process column for non-root | small | ~10 |
| 6 | Hide empty crontab table | tiny | 3 |
| 9 | Invert disk bar to show usage | tiny | 2 |

## Medium effort

| # | Fix | Effort | Lines changed |
|---|-----|--------|---------------|
| 1 | Humanize memory values | medium | ~20 |
| 7 | Right-align numeric table columns | small | ~6 |
| 8 | ANSI bold on section titles | tiny | ~4 |
| 10 | Reorder sections | tiny | reorder 8 lines |
