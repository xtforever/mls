#!/bin/bash



a()
{
    echo a; sleep 1;
    echo a ready
    exit 1
}

b()
{
    echo b; sleep 2
    echo b ready
    exit 2
}

c()
{
    echo c; sleep 1
    echo c ready
    exit 3
}

st()
{
  P+=($!)     
  X+=($1)
}

run()
{
  $1 &
}



run a 
st 100 

run b
st 200

run c 
st 300

# Check the exit codes
for pid in "${P[@]}"; do
    wait "$pid"
    ec+=($?)
done

echo ${ec[@]}
echo ${X[@]}
