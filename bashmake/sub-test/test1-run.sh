#!/bin/bash
#set -x

source ../compile.lib

MAKE="../bash-make"
LOGF=state.log
echo "" >$LOGF

LOG()
{
    echo >>$LOGF "$*"
}


read -r -d '' global_state <<- EOM 
(


nodes: (

(   OUT: ( 'test1/b.test' ), 
    REC: "run make_b"
),

(   OUT: ( 'test1/c.test' ),
    DEP: ( 'test1/b.test' ), 
    REC: "run make_c"
),

(   DEP: ( 'test1/b.test', 'test1/c.test' ),
    REC: "verify"
) ) )
EOM


clean()
{
  mkdir test1 &>/dev/null
  rm test1/*.test &>/dev/null
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
	[ -r test1/b.test -a  -r test1/c.test ] 
}




print_verify()
{
    
    if verify; then echo ok $1; else echo fail $1; ERROR=1; exit 1; fi
}


ERROR=0
clean
make_all
print_verify "phase1: b.test, c.test created"

echo "recompile. check if loop count is one."  
make_all
print_verify "phase2"
if (( loop_count == 1 )) ; then echo "ok phase3"; else echo "fail phase3"; ERROR=1; fi
echo "loop check OK"



echo -e "\n\nREDO CHECK"
make_exec2()
{
    fn=test1/xfail$1
    echo filename: $1
    if [ -r $fn ]
    then
	rm $fn 
	return 0
    fi
    touch $fn 
    return 1
}

read -r -d '' global_state <<- EOM 
( nodes: (

(
    REC: "run make_exec2 1"
)

) )
EOM

echo "testing if function is repeated 2 times"
make_all
if (( loop_count == 3 )) ; then echo "ok phase4"; else echo "fail phase4"; ERROR=1; exit 1; fi
echo "redo check OK"



echo -e "\n\nTEST-PARALLEL"
read -r -d '' global_state <<- EOM 
( nodes: (

(
    REC: "run make_exec2 1"
),

(
    REC: "run make_exec2 2"
)



) )
EOM

make_all
if (( loop_count == 3 )) ; then echo "ok phase5"; else echo "fail phase5"; ERROR=1; exit 1; fi

echo "two files generated in parallel OK"



showname()
{
    
    echo "Node: ${OUT[*]}  ( ${IN[*]}  )"
    
}

echo -e "\n\nTEST-DEPS and Non Filename Outputs"
read -r -d '' global_state <<- EOM 
( nodes: (

(
	OUT: ( phony )
    	REC: "run showname "
)

(
	OUT: ( prepare )
    	REC: "run showname "
)

(
	IN: ( phony )
	OUT: ( dep2 )
    	REC: "run  showname "
)

(
	IN: ( phony dep2 )
	OUT: ( dep3 )
    	REC: "run  showname "
)



) )
EOM

make_all
echo $loop_count



exit $ERROR




