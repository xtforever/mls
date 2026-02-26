include ../rules.mk

TARGETS=binsert bsearch osview test_autofree

ALL: $(TARGETS)

binsert: mls.o
bsearch: mls.o
osview: mls.o
test_autofree: mls.o
clean:
	-${RM} $(TARGETS) *.o *~ *.lex.* *.tab.* 

extra: clean ALL
