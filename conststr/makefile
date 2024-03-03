include ../rules.mk

CFLAGS+=-Werror -DMLS_DEBUG -g -DWITH_CONSTR_MAIN
SRC=../lib/mls.c conststr.c 

ALL: conststr

conststr: conststr.o mls.o


clean:
	${RM} conststr *.o *~ *.lex.* *.tab.* 

extra: clean ALL
