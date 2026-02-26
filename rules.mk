YFLAGS  = -d
YACC    = bison
LEX     = flex
CC = gcc

VPATH=../lib

DEBUGFLAGS=-g -Wall -DMLS_DEBUG  -fstack-protector-all -fsanitize=address
PRODUCTIONFLAGS=-O3 -fdata-sections -ffunction-sections -Wl,--gc-sections

ifeq ($(production),1)
TAG="\"PROD_$(shell date)\""
CFLAGS+=$(PRODUCTIONFLAGS)
OBJ=
else
TAG="\"DEBUG_$(shell date)\""
CFLAGS+=$(DEBUGFLAGS)
OBJ=d
endif

CFLAGS+=-D_GNU_SOURCE -I../lib -DCOMP_TAG=$(TAG)

%.tab.c %.tab.h: %.y
	$(YACC) $(YFLAGS) $<

%.lex.c %.lex.h: %.l
	$(LEX) -o$*.lex.c --header-file=$*.lex.h $<

%.od: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.exe: %.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.exed: %.od 
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)
