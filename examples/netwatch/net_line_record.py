#!/usr/bin/env python3
"""Python baseline for netwatch.c: net_line_record() from /proc/net/tcp.

This is the reference the C version is judged against — the same shape, the
same output strings. Run it directly for a self-test, or import it:

    python3 net_line_record.py
    ./netwatch selftest            # C side asserts the same vectors
"""


def ip4(hexip):
    """'0100007F' -> '127.0.0.1' (little-endian hex dword, as the kernel stores it)."""
    ip = int(hexip, 16)
    return f"{ip & 0xff}.{ip >> 8 & 0xff}.{ip >> 16 & 0xff}.{ip >> 24 & 0xff}"


def net_line_record(line):
    """One /proc/net/tcp line -> 'listen ip:port' / 'connect ip:port', or None."""
    f = line.split()
    if len(f) < 4 or not f[0].endswith(":"):
        return None                      # header or malformed
    local, remote, st = f[1], f[2], f[3]
    if st not in ("0A", "01"):
        return None                      # only LISTEN / ESTABLISHED
    lip, lport = local.rsplit(":", 1)
    rip, rport = remote.rsplit(":", 1)
    if st == "0A":
        return f"listen {ip4(lip)}:{int(lport, 16)}"
    return f"connect {ip4(rip)}:{int(rport, 16)}"


if __name__ == "__main__":
    hdr = "  sl  local_address rem_address   st tx_queue rx_queue tr"
    listener = "   0: 00000000:CC1F 00000000:0000 0A 00000000:00000000 00"
    estab = "   1: 0100007F:1FBB 0100007F:0016 01 00000000:00000000 00"
    timewait = "   2: 0100007F:1FBB 0100007F:0016 06 00000000:00000000 00"

    assert net_line_record(hdr) is None
    assert net_line_record(timewait) is None
    assert net_line_record(listener) == "listen 0.0.0.0:52255"
    assert net_line_record(estab) == "connect 127.0.0.1:22"
    print("selftest OK")
