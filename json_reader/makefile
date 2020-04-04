include ../rules.mk

ALL: json_read_test

json_lex.lex.o: json_parse.tab.c

json_read.c: json_parse.tab.c json_lex.lex.c

json_read_test: mls.o json_read.o json_lex.lex.o json_parse.tab.o

clean:
	${RM} json_read_test *.o *~ *.lex.* *.tab.* 

extra: clean ALL
