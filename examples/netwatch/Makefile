# netwatch — MLS showcase: learn/exec socket allowlist from /proc/net/tcp.
#
#   make          build ./netwatch
#   make check    build + run the built-in selftest
#   make baseline run the Python baseline (net_line_record.py) selftest
#   make clean
#
# Uses the generated single-header amalgam ../../mls_core.h (core release; no
# HDF/HTTP). To make this directory standalone, copy mls_core.h next to
# netwatch.c and set CPPFLAGS=-I.

CC       ?= gcc
CPPFLAGS ?= -I../..
CFLAGS   ?= -std=c11 -O3
LDLIBS   ?= -lpthread -lm -ldl

netwatch: netwatch.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< $(LDLIBS)

check: netwatch
	./netwatch selftest

# The Python reference for net_line_record() (see docs/guide/lean-code.md).
baseline: net_line_record.py
	python3 net_line_record.py

clean:
	rm -f netwatch

.PHONY: check baseline clean
