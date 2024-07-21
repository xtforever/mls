#!/bin/bash

echo "bash-make utility"
echo "needed: compile.lib and bash-make"

CFLAGS="-I. -I../lib -g -Wall -DMLS_DEBUG -D_GNU_SOURCE -Wall"
YFLAGS=-d
YACC=bison
LEX=flex
CC=gcc

n=njson_lex
$LEX -o${n}.lex.c --header-file=${n}.lex.h $n.l

n=njson_parse.y
$YACC $YFLAGS $n

target="bash-make"

depsc=(conststr.c  ../lib/mls.c  njson_lex.lex.c  njson_parse.tab.c  njson_read.c  njson_read_test.c)
depso=(conststr.o  mls.o  njson_lex.lex.o  njson_parse.tab.o  njson_read.o  njson_read_test.o)

for fn in ${depsc[@]}
do
	$CC $CFLAGS -c $fn
done

$CC $CFLAGS -o$target ${depso[*]} && echo ready || echo error


