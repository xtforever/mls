#!/bin/bash

MAKE=./njson_read_test
LOGF=state.log

read -r -d '' global_state <<- EOM 
( nodes: (

(   IN:  ( "$MAKE" ),
    OUT: ( 'test1/b.test' ), 
    REC: "run make_b"
),

(   OUT: ( 'test1/c.test' ),
    IN: ( 'test1/b.test' ), 
    REC: "run make_c"
),

(   DEP: ( 'test1/b.test', 'test1/c.test' ),
    REC: "verify"
) ) )
EOM


clean()
{
  rm test1/*.test TMP.checksum
}


make_b()
{
    echo "create b"
	touch test1/b.test
}


make_c()
{
        echo  "create c"
	touch test1/c.test
}

verify()
{
    echo  "verify"
	[ -r test1/b.test -a  -r test1/c.test ] 
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

while (( loop_count < 4 )) 
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
	wait "$pid"; ec+=($?)
    done

done
}

print_verify()
{
    
    if verify; then echo ok $1; else echo fail $1; ERROR=1; fi
}


ERROR=0
clean
gs1="$global_state"
make_all
print_verify "phase1"

exit 1

global_state="$gs1"
make_all

print_verify "phase2"
if (( loop_count == 2 )) ; then echo "ok phase2"; else echo "fail phase2"; ERROR=1; fi
