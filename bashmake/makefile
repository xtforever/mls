include ../rules.mk
CFLAGS+=-Werror -DMLS_DEBUG
SRC=../lib/mls.c njson_read.c njson_lex.c njson_parse.tab.c bash-make.c conststr.c

ALL: bash-make deps

njson_lex.c njson_lex.h: njson_lex.l
	$(LEX) -o$*.c --header-file=$*.h $<

njson_lex.o: njson_parse.tab.c njson_lex.c

njson_read.c: njson_parse.tab.c njson_lex.c

bash-make: mls.o njson_read.o njson_lex.o njson_parse.tab.o conststr.o

deps: bash-make
	${CC} ${CFLAGS} -c -MMD ${SRC}

clean:
	${RM} njson_read_test *.o *~ *.lex.* *.tab.* 

extra: clean ALL
