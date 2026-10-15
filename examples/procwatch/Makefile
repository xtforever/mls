# procwatch — MLS showcase: learn/exec process allowlist, one name per line.
#
#   make          build ./procwatch
#   make check    build + run the built-in selftest
#   make clean
#
# Uses the generated single-header amalgam ../../mls_core.h (core release; no
# HDF/HTTP). To make this directory standalone, copy mls_core.h next to
# procwatch.c and set CPPFLAGS=-I.

CC       ?= gcc
CPPFLAGS ?= -I../..
CFLAGS   ?= -std=c11 -O3
LDLIBS   ?= -lpthread -lm -ldl

procwatch: procwatch.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< $(LDLIBS)

check: procwatch
	./procwatch selftest

clean:
	rm -f procwatch

.PHONY: check clean
