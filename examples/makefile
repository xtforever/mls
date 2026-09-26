# mls_base examples: HDF reader (unsorted), table (sorted), and the universal
# list structure they share.
#
#   make              # debug build   -> *.exed (objects .od)
#   make production=1 # release build -> *.exe  (objects .o)
#   make run          # build (debug) and run all examples
#   make minimal      # size-optimized, stripped hdf_dump; prints its size
#   make clean

include ../rules.mk

production ?= 0

ifeq ($(production),1)
OBJ_EXT = o
BIN_EXT = exe
else
OBJ_EXT = od
BIN_EXT = exed
endif

PROGS = hdf_dump list_demo table_demo

# shared library sources (found in ../lib via rules.mk VPATH)
MLS   = mls_base.$(OBJ_EXT) list.$(OBJ_EXT)
HDF   = hdf_parse.$(OBJ_EXT) hdf_base.$(OBJ_EXT)
TABLE = table.$(OBJ_EXT)

BINS = $(addsuffix .$(BIN_EXT), $(PROGS))

all: $(BINS)

hdf_dump.$(BIN_EXT): hdf_dump.$(OBJ_EXT) $(MLS) $(HDF)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

list_demo.$(BIN_EXT): list_demo.$(OBJ_EXT) $(MLS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

table_demo.$(BIN_EXT): table_demo.$(OBJ_EXT) $(MLS) $(TABLE)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

run: $(BINS)
	./hdf_dump.$(BIN_EXT) sample.hdf
	./list_demo.$(BIN_EXT)
	./table_demo.$(BIN_EXT)

# Smallest possible reader: -Os + per-function sections so the linker drops
# every unused function, then strip symbols. One compile+link step.
MIN_CFLAGS  = -Os -ffunction-sections -fdata-sections -D_GNU_SOURCE -I../lib
MIN_LDFLAGS = -Wl,--gc-sections -s

minimal: hdf_dump.min
	@printf 'minimal reader size: ' && wc -c < hdf_dump.min

hdf_dump.min: hdf_dump.c mls_base.c list.c hdf_parse.c hdf_base.c
	$(CC) $(MIN_CFLAGS) $(MIN_LDFLAGS) -o $@ $^ -lm -lpthread

clean:
	-${RM} $(addsuffix .exe,$(PROGS)) $(addsuffix .exed,$(PROGS)) hdf_dump.min *.o *.od
