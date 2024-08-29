#!/bin/bash
d=$PWD/bm.nodes

INCDIR=(../lib)
CFLAGS="-g -Wall -DMLS_DEBUG -D_GNU_SOURCE -Wall -I../lib -I."
LDFLAGS=""
YACC=bison
LEX=flex
CC=gcc
PREFIX=build


sbison()
{
    local n=${IN[0]%.y}
    echo bison $n.y   
    $YACC -o$PREFIX/$n.tab.c -H$PREFIX/$n.tab.h $n.y
}


sflex()
{    
    local n=${IN[0]%.l}
    echo flex $n.l
    $LEX -o$PREFIX/$n.c --header-file=$PREFIX/$n.h $n.l
}

slink()
{
    local inf=${IN[*]}
    local outf=${OUT[0]}
    echo link $outf
    $CC $CFLAGS $LDFLAGS $inf -o$PREFIX/$outf
}


ccompile()
{
    local inf=${IN[*]}
    local outf=${OUT[0]}
    echo ccompile $outf
    $CC -c $CFLAGS $inf -o $outf 
}

store-result()
{
    echo "completed task:$1 exitcode:$2"
}


wraptask()
{
    $1
    eval "echo '-- $BASHPID $? $id ---' >&$pipefd"
    sleep 2    # do not leave the proc until fifo message (above) is received 
}

nproc=0
maxproc=8
topen=()
trun=()


# run until task is complete
# returns: $exit_code $task_id
make-parallel()
{
    # start up to 'nproc' tasks from 'topen[]', move id from 'topen[]' to 'trun[]'
    for cp in ${!topen[@]}
    do
	if (( nproc >= maxproc )) ; then break; fi
	id=${topen[cp]}
	wraptask node$id &
	echo "new task id=$id pid=$!"
	((nproc++))
	unset topen[cp]
	trun+=($id)
    done
    topen=(${topen[@]})

    # if there are running tasks, wait for one task to finish
    # remove id from 'trun[]'
    while (( nproc > 0 ))
    do
	read -t 1 -u $pipefd a b exit_code task_id e
	if (( $? != 0 ))
	then
	    echo timeout running:$nproc left:$cnt
	else
	    ((nproc--))
	    echo return $a $b $exit_code $task_id $e
	    kill $b ; wait $b
	    for a in ${!trun[@]}
	    do
		if (( trun[a] == $task_id ))
		then
		    unset trun[a]
		    break
		fi
	    done
	    trun=(${trun[@]})
	    break
	fi
    done
}


cd ..
rm TMP.checksum
pipe=$(mktemp -u)
mkfifo "$pipe" &>/dev/null
pipefd=3
exec 3<>$pipe

prog="$(cat $d | ./bash-make 2>/dev/null)"
eval "$prog"
topen=( ${nodes_ready[@]} )

#solange noch tasks laufen oder auftrage in topen[] liegen weitermachen.
while (( nproc > 0 || ${#topen[@]} > 0 ))
do

    make-parallel
    echo "RUNNING:$nproc WAITING:${#topen[@]}"
    # if $exit_code is not zero append $task_id to topen
    # and try again
    ec=()
    for a in ${trun[@]} ${topen[@]}
    do
	ec+=( $a 9 ) 
    done
    ec+=(  $task_id  $exit_code )
    if (( exit_code != 0 ))
    then
	topen+=( $task_id )
    else
	echo TASK $task_id COMPLETED
    fi

    prog=$(echo ${ec[@]} - $global_state | ./bash-make 2>/dev/null)
    eval "$prog"
    for a in  ${nodes_ready[*]}
    do
	id=${a##node}
	topen+=( $id )
    done
    
done

    
exec 3>&-

