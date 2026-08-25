# rootinfo

A system-overview tool for the terminal, built on the MLS library.

Prints host, kernel, CPU model + load, memory, disk usage, running
process count, uptime and the current user, assembling the whole report
into an MLS string handle (`s_printf`).

## Build and run

    make          # build rootinfo.exed
    ./rootinfo.exed
    make check    # run with output to /dev/null

## Colors and config

Output is colorized when stdout is a terminal (section titles bold cyan,
table headers bold, key/value keys bold, usage bars green/yellow/red with
dim empty cells). Piped output stays plain. Override in `rootinfo.hdf`:

    (style (color on))    # always colorize
    (style (color off))   # never colorize
    (style (color auto))  # only on a terminal (default)

On first run a default `rootinfo.hdf` is written next to the binary.

## Layout

The report is accumulated in one MLS string handle (`out`); `kv()` appends
each `key  value` line. Data comes from `uname(2)`, `sysconf(3)`,
`getloadavg(3)`, `statvfs(3)`, `/proc/cpuinfo`, `/proc/meminfo`,
`/proc/uptime` and `getpwuid(3)`.

## Extending

Add a `xxx_section()` following the existing pattern and call it from
`main()`. The MLS lib's `m_table` is available if you want to switch the
plain `kv` lines to a table-backed layout.
