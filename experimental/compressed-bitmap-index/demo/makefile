CC ?= gcc
LIBDIR = ../../../lib
CFLAGS = -g -O2 -Wall -Wextra -D_GNU_SOURCE -I.. -I$(LIBDIR)
LDLIBS = -lpthread -ldl -lm
LIBSRC = ../bm.c $(LIBDIR)/mls_base.c $(LIBDIR)/mls_ext.c

all: kwdemo test_kwdb bigindex

bigindex: bigindex.c $(LIBSRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

kwdemo: kwdb.c kwdemo.c $(LIBSRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test_kwdb: kwdb.c test_kwdb.c $(LIBSRC)
	$(CC) $(CFLAGS) -fsanitize=address,undefined -o $@ $^ $(LDLIBS)

run: test_kwdb
	./test_kwdb $(wildcard ../pg_catalog.csv pg_catalog.csv)

# Compression benchmark. DATA points at an unzipped MovieLens ml-25m dir:
#   curl -LO https://files.grouplens.org/datasets/movielens/ml-25m.zip && unzip ml-25m.zip
DATA ?= /tmp/opencode/ml/ml-25m
bench: bigindex
	./bigindex $(DATA)/ratings.csv --col 0 --verify --label "userId, file order"
	./bigindex $(DATA)/ratings.csv --col 1 --sort --verify --label "movieId, sorted"
	./bigindex $(DATA)/ratings.csv --col 1 --limit 2000000 --verify --label "movieId, file order (2M)"

clean:
	-$(RM) kwdemo test_kwdb bigindex

.PHONY: all run bench clean
