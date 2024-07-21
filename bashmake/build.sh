#!/bin/bash
INCDIR=(../lib)
CFLAGS="-g -Wall -DMLS_DEBUG -D_GNU_SOURCE -Wall"
YACC=bison
LEX=flex
CC=gcc
PREFIX=build


# convert rel-path for gcc include path to absolute path 
cdir=$PWD
for a in ${INCDIR[*]} . $PREFIX
do
    CFLAGS+=" -I$cdir/$a"
done


depmake()
{
	$CC -c $CFLAGS -MMD ${IN[*]}
}

f_bison()
{
    local n=${IN[0]%.y}
    echo bison $n.y   
    $YACC -o$PREFIX/$n.tab.c -H$PREFIX/$n.tab.h $n.y
}


f_flex()
{    
    local n=${IN[0]%.l}
    echo flex $n.l
    $LEX -o$PREFIX/$n.c --header-file=$PREFIX/$n.h $n.l
}


f_link()
{
    inf=${IN[*]}
    outf=${OUT[0]}
    echo link $outf
    $CC $CFLAGS $inf -o$PREFIX/$outf
}

f_ccall()
{
    cdir=$PWD
    pushd $PREFIX >/dev/null
    for a in ${IN[*]}
    do
	echo cc $a
	$CC -c $CFLAGS $cdir/$a 
    done
    popd >/dev/null
}


mkdir build  &>/dev/null || rm build/*


IN=(njson_parse.y)
f_bison

IN=(njson_lex.l)
f_flex

rm *.o &>/dev/null
IN=(../lib/mls.c *.c $PREFIX/*.c)
f_ccall

IN=($PREFIX/*.o)
OUT=(bash-make)
f_link
