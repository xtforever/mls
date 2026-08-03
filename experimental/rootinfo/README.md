# rootinfo

A system-overview tool for the terminal, built on the MLS library.

Prints host, kernel, CPU model + load, memory, disk usage, running
process count, uptime and the current user, assembling the whole report
into an MLS string handle (`s_printf`).

## Build and run

    make          # build rootinfo.exed
    ./rootinfo.exed
    make check    # run with output to /dev/null

## Layout

The report is accumulated in one MLS string handle (`out`); `kv()` appends
each `key  value` line. Data comes from `uname(2)`, `sysconf(3)`,
`getloadavg(3)`, `statvfs(3)`, `/proc/cpuinfo`, `/proc/meminfo`,
`/proc/uptime` and `getpwuid(3)`.

## Extending

Add a `xxx_section()` following the existing pattern and call it from
`main()`. The MLS lib's `m_table` is available if you want to switch the
plain `kv` lines to a table-backed layout.
