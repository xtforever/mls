#!/bin/bash


CFLAGS="-I. -I../lib -g -Wall -DMLS_DEBUG -D_GNU_SOURCE -Wall"
YFLAGS=-d
YACC=bison
LEX=flex
CC=gcc


ccompile_build()
{
    inf=${IN[0]}
    outf=${OUT[0]}
    echo ccompile_build $inf
    $CC -c $CFLAGS  $inf -o$outf
}


bison_build()
{
    echo bison_build  ${IN[0]}   
    $YACC $YFLAGS ${IN[0]}   
}


flex_build()
{
    
    n=${IN[0]%.l}
    echo flex_build $n.l
    $LEX -o${n}.lex.c --header-file=${n}.lex.h $n.l
}


link_build()
{
	inf=${IN[*]}
	outf=${OUT[0]}
	echo link_build $outf
	$CC $CFLAGS $inf -o$outf
}

store-result()
{
    P+=($!)     
    X+=($1)
}

run()
{
  $1 &
}

rm log
ec=()
global_state=$( cat  nodes.json )
loop_count=0

while (( loop_count < 10 )) 
do
    ((loop_count++))
    
    prog=$( echo "$global_state" | ./bmake "${ec[*]}" | tee -a log )
    X=(); P=()
    eval "$prog"

    if (( ${#P[@]} == 0 )); then break; fi

    i=0;  ec=()
    for pid in "${P[@]}"; do
	ec+=(${X[i++]})
	wait "$pid"; ec+=($?)
    done
    echo >>log "exit codes ${ec[@]}"
done
