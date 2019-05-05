include ../rules.mk

ALL: ini_read_test

ini_lex.lex.o: ini_parse.tab.h

ini_read2.o: ini_lex.lex.o

ini_read_test: mls.o ini_read2.o ini_lex.lex.o ini_parse.tab.o

clean:
	${RM} ini_read_test *.o *~ *.lex.* *.tab.* 

extra: clean all
