#!/bin/bash

echo "bash-make utility"

CFLAGS="-I. -I../lib -g -Wall -DMLS_DEBUG -D_GNU_SOURCE -Wall"
YFLAGS=-d
YACC=bison
LEX=flex
CC=gcc

n=njson_lex
$LEX -o${n}.c --header-file=${n}.h $n.l

n=njson_parse
$YACC -o$n.c -H$n.h $n.y

target="bash-make"

depsc=(conststr.c  ../lib/mls.c  njson_lex.c  njson_parse.c  njson_read.c  bash-make.c)
depso=(conststr.o  mls.o  njson_lex.o  njson_parse.o  njson_read.o bash-make.o)

for fn in ${depsc[@]}
do
	$CC $CFLAGS -c $fn
done

$CC $CFLAGS -o$target ${depso[*]} && echo ready || echo error


