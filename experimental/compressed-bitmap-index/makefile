CC ?= gcc
LIBDIR = ../../lib
CFLAGS = -g -O1 -Wall -Wextra -D_GNU_SOURCE -I$(LIBDIR)
LDLIBS = -lpthread -ldl -lm
LIBSRC = $(LIBDIR)/mls_base.c $(LIBDIR)/mls_ext.c

all: test_bm

test_bm: bm.c test_bm.c $(LIBSRC)
	$(CC) $(CFLAGS) -fsanitize=address,undefined -o $@ $^ $(LDLIBS)

run: test_bm
	./test_bm

clean:
	-$(RM) test_bm

.PHONY: all run clean
