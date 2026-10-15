# mls_base examples: HDF reader, table, and the universal list structure.
#
#   make                 # every app in APPS, debug + release
#   make hdf_dump.exed   # debug   (objects .od, -DMLS_DEBUG + ASan)
#   make hdf_dump.exe    # release (objects .o,  -O3)
#   make minimal         # size-optimized, stripped hdf_dump; prints size
#   make clean
#
# To add an example: write foo.c, append "foo" to APPS. That is the whole edit.

CC    = gcc
vpath %.c ../lib

COMMON         = -D_GNU_SOURCE -I../lib
CFLAGS_DEBUG   = $(COMMON) -g -Wall -DMLS_DEBUG -fstack-protector-all -fsanitize=address
CFLAGS_RELEASE = $(COMMON) -O3 -fdata-sections -ffunction-sections -Wl,--gc-sections
LDLIBS         = -lm -lpthread

# Examples link the whole core. Release drops unused code via --gc-sections;
# the ASan build pays a little dead weight, which is fine for demos.
LIBS = mls_base.c mls_ext.c m_tool.c m_var.c m_sql.c list.c table.c hdf_parse.c hdf_base.c

od = $(patsubst %.c,%.od,$(1))
o  = $(patsubst %.c,%.o,$(1))

%.od: %.c
	$(CC) $(CFLAGS_DEBUG) -c -o $@ $<
%.exed: %.od $(call od,$(LIBS))
	$(CC) $(CFLAGS_DEBUG) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS_RELEASE) -c -o $@ $<
%.exe: %.o $(call o,$(LIBS))
	$(CC) $(CFLAGS_RELEASE) -o $@ $^ $(LDLIBS)

APPS = hdf_dump list_demo table_demo csv2insert sql_demo world

all: $(addsuffix .exe,$(APPS)) $(addsuffix .exed,$(APPS))

# Render the world example's generic tree as a graph (opt-in, not in `all`).
world.svg: world.exe
	./world.exe dot | dot -Tsvg -o $@

world.png: world.exe
	./world.exe dot | dot -Tpng -o $@

# Smallest possible reader: -Os + per-function sections so the linker drops
# unused code, then strip. One compile+link step.
minimal: hdf_dump.min
	@printf 'minimal reader size: ' && wc -c < hdf_dump.min

hdf_dump.min: hdf_dump.c $(LIBS)
	$(CC) -Os -ffunction-sections -fdata-sections $(COMMON) -Wl,--gc-sections -s -o $@ $^ -lm -lpthread

clean:
	-$(RM) *.exe *.exed *.o *.od hdf_dump.min world.svg world.png

.SECONDARY:          # keep object files; don't delete them as intermediates
.DEFAULT_GOAL := all
.PHONY: all minimal clean
