include ../rules.mk

CFLAGS+=-Werror -DMLS_DEBUG
SRC=../lib/mls.c njson_read.c njson_lex.lex.c njson_parse.tab.c njson_read_test.c conststr.c

ALL: njson_read_test deps

njson_lex.lex.o: njson_parse.tab.c

njson_read.c: njson_parse.tab.c njson_lex.lex.c

njson_read_test: mls.o njson_read.o njson_lex.lex.o njson_parse.tab.o conststr.o


deps: njson_read_test
	${CC} ${CFLAGS} -c -MMD ${SRC}

clean:
	${RM} njson_read_test *.o *~ *.lex.* *.tab.* 

extra: clean ALL
