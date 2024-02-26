#!/bin/bash

MAKE=./njson_read_test
LOGF=state.log

read -r -d '' global_state <<- EOM 
( nodes: (

(        OUT: ('prepare'),
	 REC: "run two_times_fail"
),

(   DEP:  ( 'prepare' ),
    IN:  ( '$MAKE' ),
    OUT: ( 'test1/b.test' ), 
    REC: "run make_b"
),

(   OUT: ( 'test1/c.test' ),
    IN: ( 'test1/b.test' ), 
    REC: "run make_c"
),

(   DEP: ( 'test1/b.test', 'test1/c.test' ),
    REC: "run verify"
) ) )
EOM




clean()
{
  rm test1/*.test TMP.checksum lock1 lock2
}

fail=0
two_times_fail()
{
    echo "twotimesfail"
    
    if [ -r lock1 ]; then
	if [ -r lock2 ]; then
	    rm lock1 lock2
	    return 0
	else
	    touch lock2
	fi
    else
	touch lock1
    fi
    
    return 1
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
    echo Verify
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

print_verify()
{
    
    if verify; then echo ok $1; else echo fail $1; ERROR=1; fi
}


ERROR=0
clean
gs1="$global_state"
make_all
print_verify "phase1"

global_state="$gs1"
make_all

print_verify "phase2"
if (( loop_count == 4 )) ; then echo "ok phase2"; else echo "fail phase2"; ERROR=1; fi

exit $ERROR

# prepare:
#    bash...
#    ....
#    ....
# <empty line>
#
#
# link: 
#   bash...
#
# comp: 
#
#
# TREE:
#
# foreach c file create a list of depencencies
#



firstosm
tom3uat
