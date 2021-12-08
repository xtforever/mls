include ../rules.mk

ALL: njson_read_test

njson_lex.lex.o: njson_parse.tab.c

njson_read.c: njson_parse.tab.c njson_lex.lex.c

njson_read_test: mls.o njson_read.o njson_lex.lex.o njson_parse.tab.o

clean:
	${RM} njson_read_test *.o *~ *.lex.* *.tab.* 

extra: clean ALL
