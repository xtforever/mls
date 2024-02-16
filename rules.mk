YFLAGS  = -d
YACC    = bison
LEX     = flex
CC = gcc

VPATH=../lib

DEBUGFLAGS=-g -Wall -DMLS_DEBUG
PRODUCTIONFLAGS=-O3

ifeq ($(production),1)
TAG="\"PROD_$(shell date)\""
CFLAGS+=$(PRODUCTIONFLAGS)
else
TAG="\"DEBUG_$(shell date)\""
CFLAGS+=$(DEBUGFLAGS)
endif

CFLAGS+=-D_GNU_SOURCE -Wall -I../lib -DNO_XPM
CFLAGS+=-DCOMP_TAG=$(TAG)

%.tab.c %.tab.h: %.y
	$(YACC) $(YFLAGS) $<

%.lex.c %.lex.h: %.l
	$(LEX) -o$*.lex.c --header-file=$*.lex.h $<
