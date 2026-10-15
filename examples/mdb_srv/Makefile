# mdb_srv — fork-per-request MariaDB demo service (PHP-FPM style) on m_flask.
#
#   make          build ./mdb_srv (links libmariadb when pkg-config finds it)
#   make check    build + run the built-in selftest (needs no database)
#   make clean
#
# Uses the generated single-header amalgam ../../mls_full.h (core + HDF +
# m_http + m_flask).  Without libmariadb the SQL demo falls back to an echo
# stub; mdb_query.c is compiled only when the client library is present.
# mdb_query.c is a separate translation unit because MariaDB's ma_list.h
# collides with MLS's list_* symbols.

CC       ?= gcc
CPPFLAGS ?= -I../..
CFLAGS   ?= -std=c11 -O2 -Wall -Wextra
LDLIBS   ?= -lpthread -lm -ldl

MARIADB_CFLAGS := $(shell pkg-config --cflags libmariadb 2>/dev/null)
MARIADB_LIBS   := $(shell pkg-config --libs libmariadb 2>/dev/null)
ifneq ($(MARIADB_LIBS),)
MARIADB_CFLAGS += -DHAVE_MARIADB
MDB_MARIADB_SRC = mdb_query.c
endif

mdb_srv: mdb_srv.c $(MDB_MARIADB_SRC) mdb_query.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(MARIADB_CFLAGS) -o $@ \
		mdb_srv.c $(MDB_MARIADB_SRC) $(LDLIBS) $(MARIADB_LIBS)

check: mdb_srv
	./mdb_srv selftest

clean:
	rm -f mdb_srv

.PHONY: check clean
