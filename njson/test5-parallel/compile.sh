#!/bin/bash
rm log

create-file()
{
    touch ${OUT[*]}
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

ec=()
loop_count=0
read -r -d '' global_state <<- EOM 
(

nodes: (

(   OUT: ( 'a.test' ), 
    REC: "run create-file"
),

(   IN:  ( 'a.test' ),
    OUT: ( 'b.test' ), 
    REC: "run create-file"
),

(   IN:  ( 'b.test' ),
    OUT: ( 'c.test' ), 
    REC: "run create-file"
),


(   IN:  ( 'b.test' ),
    OUT: ( 'd.test' ), 
    REC: "run create-file"
)



) )
EOM

while (( loop_count < 10 )) 
do
    ((loop_count++))
    
    prog=$( echo "${ec[*]} + $global_state" | ../njson_read_test | tee -a log )
    X=(); P=()
    echo $prog | grep "run "
    eval "$prog"

    if (( ${#P[@]} == 0 )); then break; fi

    i=0;  ec=()
    for pid in "${P[@]}"; do
	ec+=(${X[i++]})
	wait "$pid"; ec+=($?)
    done
    echo >>log "exit codes ${ec[@]}"
done
