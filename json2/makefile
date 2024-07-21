include ../rules.mk

ALL: json_read_test

json_parse.tab.c: json_lex.lex.h

json_read.c: json_parse.tab.c json_lex.lex.c json_lex.lex.h

json_read_test:  json_parse.tab.o mls.o json_read.o json_lex.lex.o

clean:
	${RM} json_read_test *.o *~ *.lex.* *.tab.* 

extra: clean ALL
