#!/bin/bash

MAKE=../njson_read_test
LOGF=state.log

read -r -d '' global_state <<- EOM 
(

nodes: (

(        OUT: ('prepare'),
	 REC: "run itworks"
)

 ) )
EOM




clean()
{
  rm TMP.checksum $LOGF file-*.deps test$.a
}


itworks()
{
    echo "it works"
}

store-result()
{
    P+=($!)     
    X+=($1)
}

run()
{
    p=$1
    shift
    $p $* &
}

make_all()
{
ec=()
loop_count=0

while (( loop_count < 10 )) 
do
    ((loop_count++))
    echo "LOOP: $loop_count"
    
    prog=$( echo "$global_state" | ${MAKE} "${ec[*]}" | tee -a $LOGF )
    X=(); P=()
    eval "$prog"

    if (( ${#P[@]} == 0 )); then break; fi

    i=0;  ec=()
    for pid in "${P[@]}"; do
	ec+=(${X[i++]})
	wait "$pid"; n=$?
	ec+=($n)
    done

done
}

clean
make_all
echo "READY"


bcompile()
{
    echo "compiling ${IN[*]}"
    echo "generate ${OUT[*]}"
    touch  ${OUT[*]}
}

flexcompile()
{
    echo "generate .c and .h with flex"
}


read -r -d '' global_state <<- EOM 
(
import: "*.deps",
nodes: ( 
(        OUT: ('prepare'),
	 REC: "run itworks"
)

 ) )
EOM

for a in $(seq 1 10); do
    echo "test$a.a: test$a.b" >file-$a.deps
done

echo "scan.c scan.h: scan.flex" >scan.deps


make_all
echo "READY"
