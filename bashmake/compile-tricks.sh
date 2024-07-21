



run f_bison:njson_parse.y:%.tab.c %.tab.h
run f_flex:njson_lex.l:%.c %.h


gen-each:run f_cc:.c .o:../lib/mls.c *.c $PREFIX/*.c


IN
OUT
DEP
REC
CMD
