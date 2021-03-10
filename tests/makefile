include ../rules.mk

TARGETS=binsert

ALL: $(TARGETS)

binsert: mls.o

clean:
	-${RM} $(TARGETS) *.o *~ *.lex.* *.tab.* 

extra: clean ALL
