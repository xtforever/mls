#!/bin/bash
d=bm.nodes

INCDIR=(../lib)
CFLAGS="-g -Wall -DMLS_DEBUG -D_GNU_SOURCE -Wall -I../lib -I."
LDFLAGS=""
YACC=bison
LEX=flex
CC=gcc
PREFIX=build

debug()
{
    echo "$*" >> debug.log
}

sbison()
{
    local n=${IN[0]%.y}
    echo  "[$tid] bison $n.y"
    debug "$YACC -o$n.c -H$n.h $n.y"
    $YACC -o$n.c -H$n.h $n.y
}


sflex()
{    
    local n=${IN[0]%.l}
    echo "[$tid] flex $n.l"
    $LEX -o$n.c --header-file=$n.h $n.l
}

slink()
{
    local inf=${IN[*]}
    local outf=${OUT[0]}
    echo "[$tid] link $outf"
    $CC $CFLAGS $LDFLAGS $inf -o$outf
}


ccompile()
{
    local inf=${IN[*]}
    local outf=${OUT[0]}
    echo "[$tid] ccompile $inf"
    $CC -c $CFLAGS $inf -o $outf 
}

store-result()
{
    ## tid=$1
    tec=$2
    echo "completed task:$tid exit:$tec"
}


# wraptask <taskid>, global $pipefd 
wraptask()
{
    tid=$1
    node$1
    eval "echo '-- $BASHPID $tec $tid ---' >&$pipefd"
    sleep 2    # do not leave the proc until fifo message (above) is received 
}

nproc=0
maxproc=1
maxerror=3
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
	wraptask $id &
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
	    ## echo return $a $b $exit_code $task_id $e
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



rm debug.log
#rm TMP.checksum
pipe=$(mktemp -u)
mkfifo "$pipe" &>/dev/null
pipefd=3
exec 3<>$pipe

prog="$( cat $d | ./bash-make 2>>debug.log)"
debug "$prog"
eval "$prog"
topen=( ${nodes_ready[@]} )
error_cnt=0
loop=0
#solange noch tasks laufen oder auftrage in topen[] liegen weitermachen.
while (( loop < 50 && error_cnt < maxerror && (nproc > 0 || ${#topen[@]} > 0) ))
do
    ((loop++))
    make-parallel
    echo "LOOP:$loop RUNNING:$nproc WAITING:${#topen[@]}"
    #every unfinished task gets the exit code 9
    ec=()
    for a in ${trun[@]} ${topen[@]}
    do
	ec+=( $a 9 ) 
    done
    ec+=(  $task_id  $exit_code )
    # append failed $task_id to topen[] and retry ($max_errors times).
    if (( exit_code != 0 ))
    then	
	((error_cnt++))
	topen+=( $task_id )
	echo "ERROR IN TASK $task_id, error counter: $error_cnt (max=$maxerror)" 
    fi
    prog=$(echo ${ec[@]} - $global_state | ./bash-make 2>>debug.log)
    debug "$prog"
    eval "$prog"
    topen+=(  ${nodes_ready[*]} )
    
done
((nproc > 0)) && wait
exec 3>&-
rm "$pipe"
