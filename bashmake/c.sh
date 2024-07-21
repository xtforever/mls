#!/bin/bash


CFLAGS="-I. -I../lib -g -Wall -DMLS_DEBUG -D_GNU_SOURCE -Wall"
YFLAGS=-d
YACC=bison
LEX=flex
CC=gcc
MAKE=./bmake

depmake()
{
	$CC -c $CFLAGS -MMD ${IN[*]}
}


ccompile()
{
    inf=${IN[0]}
    outf=${OUT[0]}
    echo ccompile_build $inf
    $CC -c $CFLAGS  $inf -o$outf
}


sbison()
{
    echo bison_build  ${IN[0]}   
    $YACC $YFLAGS ${IN[0]}   
}


sflex()
{
    
    n=${IN[0]%.l}
    echo flex_build $n.l
    $LEX -o${n}.lex.c --header-file=${n}.lex.h $n.l
}


slink()
{
	inf=${IN[*]}
	outf=${OUT[0]}
	echo link_build $outf
	$CC $CFLAGS $inf -o$outf
}

store-result()
{
    ec+=($1)
    ec+=($2)
}

rm log in 2>/dev/null

ec=()
global_state=$( cat  bm.nodes )
loop_count=0
nr_jobs=1

while (( loop_count < 10 && nr_jobs > 0 )) 
do
    ((loop_count++))

    prog=$( echo "${ec[*]} + $global_state" | tee -a log | ${MAKE} ; echo $? >xx.out )
    ec=()
    echo $loop_count
    echo "$prog" >>log
    [ -z "$prog" ] && exit 
    eval "$prog"

done
