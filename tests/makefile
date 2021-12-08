include ../rules.mk

TARGETS=binsert bsearch osview

ALL: $(TARGETS)

binsert: mls.o
bsearch: mls.o
osview: mls.o
clean:
	-${RM} $(TARGETS) *.o *~ *.lex.* *.tab.* 

extra: clean ALL
